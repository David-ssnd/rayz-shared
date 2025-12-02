#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ble_config.h"
#include "ble_target.h"
#include "protocol_config.h"
#include <esp_log.h>
#include <os/os_mbuf.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <string.h>

static const char* TAG = "BLETarget";

BLETarget* BLETarget::instance = nullptr;

namespace
{
bool parse_uuid128(const char* uuid_str, ble_uuid128_t& uuid_out)
{
    ble_uuid_any_t uuid_any;
    int rc = ble_uuid_from_str(&uuid_any, uuid_str);
    if (rc != 0 || uuid_any.u.type != BLE_UUID_TYPE_128)
    {
        return false;
    }

    uuid_out = uuid_any.u128;
    return true;
}
} // namespace

BLETarget::BLETarget()
    : conn_handle(BLE_HS_CONN_HANDLE_NONE), message_char_handle(0), connected(false), lastReceivedMessage(0),
      hasNewMessage(false)
{
    instance = this;
    event_group = xEventGroupCreate();
    message_queue = xQueueCreate(10, sizeof(uint16_t));
    memset(&service_uuid, 0, sizeof(service_uuid));
    memset(&message_uuid, 0, sizeof(message_uuid));
}

void BLETarget::begin()
{
    ESP_LOGI(TAG, "Initializing BLE Target...");

    if (!parse_uuid128(BLE_SERVICE_UUID, service_uuid))
    {
        ESP_LOGE(TAG, "Failed to parse service UUID");
        return;
    }

    if (!parse_uuid128(BLE_MESSAGE_CHAR_UUID, message_uuid))
    {
        ESP_LOGE(TAG, "Failed to parse characteristic UUID");
        return;
    }

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize NimBLE
    ESP_ERROR_CHECK(nimble_port_init());

    ble_hs_cfg.reset_cb = [](int reason) { ESP_LOGI(TAG, "BLE reset, reason: %d", reason); };

    ble_hs_cfg.sync_cb = []()
    {
        ESP_LOGI(TAG, "BLE host synchronized");
        if (BLETarget::instance)
        {
            BLETarget::instance->start_scan();
        }
    };

    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_svc_gap_device_name_set(BLE_TARGET_NAME);

    nimble_port_freertos_init(
        [](void* param)
        {
            nimble_port_run();
            nimble_port_freertos_deinit();
        });

    ESP_LOGI(TAG, "BLE Target initialized");
}

void BLETarget::start_scan()
{
    ESP_LOGI(TAG, "Starting BLE scan...");

    struct ble_gap_disc_params disc_params = {};
    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &disc_params, gap_event_handler, nullptr);
}

int BLETarget::gap_event_handler(struct ble_gap_event* event, void* arg)
{
    if (!instance)
        return 0;

    switch (event->type)
    {
        case BLE_GAP_EVENT_DISC:
        {
            // Check if this is the weapon device
            if (event->disc.length_data > 0)
            {
                struct ble_hs_adv_fields fields;
                ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);

                if (fields.name != nullptr && fields.name_len > 0)
                {
                    char name[32] = {0};
                    memcpy(name, fields.name, fields.name_len < 31 ? fields.name_len : 31);

                    if (strcmp(name, BLE_WEAPON_NAME) == 0)
                    {
                        ESP_LOGI(TAG, "Found weapon device, connecting...");
                        ble_gap_disc_cancel();
                        instance->connect_to_device(&event->disc.addr);
                    }
                }
            }
            break;
        }

        case BLE_GAP_EVENT_CONNECT:
        {
            if (event->connect.status == 0)
            {
                ESP_LOGI(TAG, "Connected successfully");
                instance->conn_handle = event->connect.conn_handle;
                instance->connected = true;

                // Start service discovery
                int rc = ble_gattc_disc_all_svcs(event->connect.conn_handle, BLETarget::on_service_discovery, instance);
                if (rc != 0)
                {
                    ESP_LOGE(TAG, "Service discovery start failed (rc=%d)", rc);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Connection failed, status: %d", event->connect.status);
                instance->start_scan();
            }
            break;
        }

        case BLE_GAP_EVENT_DISCONNECT:
        {
            ESP_LOGI(TAG, "Disconnected, reason: %d", event->disconnect.reason);
            instance->connected = false;
            instance->conn_handle = BLE_HS_CONN_HANDLE_NONE;
            vTaskDelay(pdMS_TO_TICKS(BLE_RECONNECT_DELAY_MS));
            instance->start_scan();
            break;
        }

        case BLE_GAP_EVENT_DISC_COMPLETE:
        {
            ESP_LOGI(TAG, "Scan complete, restarting...");
            instance->start_scan();
            break;
        }
        case BLE_GAP_EVENT_NOTIFY_RX:
        {
            if (event->notify_rx.attr_handle == instance->message_char_handle && event->notify_rx.om &&
                OS_MBUF_PKTLEN(event->notify_rx.om) == 2)
            {
                uint8_t data[2];
                os_mbuf_copydata(event->notify_rx.om, 0, sizeof(data), data);
                uint16_t message = (static_cast<uint16_t>(data[0]) << 8) | data[1];
                instance->lastReceivedMessage = message;
                instance->hasNewMessage = true;
                if (xQueueSend(instance->message_queue, &message, 0) != pdTRUE)
                {
                    ESP_LOGW(TAG, "BLE queue full, dropping message");
                }
            }
            break;
        }
    }

    return 0;
}

void BLETarget::connect_to_device(const ble_addr_t* addr)
{
    ble_gap_connect(BLE_OWN_ADDR_PUBLIC, addr, 30000, nullptr, gap_event_handler, nullptr);
}

int BLETarget::on_service_discovery(uint16_t conn_handle, const struct ble_gatt_error* error,
                                    const struct ble_gatt_svc* service, void* arg)
{
    auto* self = static_cast<BLETarget*>(arg);
    if (!self)
        return 0;

    if (error->status != 0)
    {
        return (error->status == BLE_HS_EDONE) ? 0 : error->status;
    }

    if (service && ble_uuid_cmp(&service->uuid.u, &self->service_uuid.u) == 0)
    {
        ESP_LOGI(TAG, "Found target service, discovering characteristics...");
        return ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle,
                                       BLETarget::on_characteristic_discovery, self);
    }

    return 0;
}

int BLETarget::on_characteristic_discovery(uint16_t conn_handle, const struct ble_gatt_error* error,
                                           const struct ble_gatt_chr* chr, void* arg)
{
    auto* self = static_cast<BLETarget*>(arg);
    if (!self)
        return 0;

    if (error->status != 0)
    {
        return (error->status == BLE_HS_EDONE) ? 0 : error->status;
    }

    if (chr && ble_uuid_cmp(&chr->uuid.u, &self->message_uuid.u) == 0)
    {
        ESP_LOGI(TAG, "Found message characteristic, enabling notifications...");
        self->message_char_handle = chr->val_handle;
        int rc = self->enable_notifications(conn_handle);
        if (rc != 0)
        {
            ESP_LOGE(TAG, "Failed to enable notifications (rc=%d)", rc);
        }
    }

    return 0;
}

int BLETarget::enable_notifications(uint16_t conn_handle) const
{
    if (message_char_handle == 0)
    {
        return BLE_HS_EINVAL;
    }

    uint16_t ccc_handle = message_char_handle + 1;
    const uint8_t notify_enable[2] = {0x01, 0x00};
    return ble_gattc_write_flat(conn_handle, ccc_handle, notify_enable, sizeof(notify_enable), nullptr, nullptr);
}

void BLETarget::update()
{
    // NimBLE handles events in background task, nothing needed here
}

bool BLETarget::isConnected()
{
    return connected;
}

bool BLETarget::hasMessage()
{
    // Prefer queue to avoid losing messages; fall back to flag if needed
    return (uxQueueMessagesWaiting(message_queue) > 0) || hasNewMessage;
}

uint16_t BLETarget::getMessage()
{
    hasNewMessage = false;
    return lastReceivedMessage;
}

bool BLETarget::fetchMessage(uint16_t* message, TickType_t ticksToWait)
{
    if (xQueueReceive(message_queue, message, ticksToWait) == pdTRUE)
    {
        lastReceivedMessage = *message;
        hasNewMessage = false;
        return true;
    }
    return false;
}

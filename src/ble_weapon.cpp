#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ble_config.h"
#include "ble_weapon.h"
#include "protocol_config.h"
#include <esp_log.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <string.h>


static const char* TAG = "BLEWeapon";

BLEWeapon* BLEWeapon::instance = nullptr;

static uint8_t gatt_svr_chr_message[2] = {0, 0};

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

BLEWeapon::BLEWeapon()
    : conn_handle(BLE_HS_CONN_HANDLE_NONE), deviceConnected(false), oldDeviceConnected(false), message_char_handle(0)
{
    instance = this;
    event_group = xEventGroupCreate();
    memset(&service_uuid, 0, sizeof(service_uuid));
    memset(&message_uuid, 0, sizeof(message_uuid));
    memset(chr_defs, 0, sizeof(chr_defs));
    memset(svc_defs, 0, sizeof(svc_defs));
}

void BLEWeapon::begin()
{
    ESP_LOGI(TAG, "Initializing BLE Weapon...");

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

    chr_defs[0].uuid = &message_uuid.u;
    chr_defs[0].access_cb = BLEWeapon::gatt_svr_chr_access;
    chr_defs[0].flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY;
    chr_defs[0].val_handle = &message_char_handle;

    svc_defs[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    svc_defs[0].uuid = &service_uuid.u;
    svc_defs[0].characteristics = chr_defs;

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
        if (BLEWeapon::instance)
        {
            BLEWeapon::instance->start_advertising();
        }
    };

    // Register GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    // Set up custom GATT service
    ble_gatts_count_cfg(svc_defs);
    ble_gatts_add_svcs(svc_defs);

    ble_svc_gap_device_name_set(BLE_WEAPON_NAME);

    nimble_port_freertos_init(
        [](void* param)
        {
            nimble_port_run();
            nimble_port_freertos_deinit();
        });

    ESP_LOGI(TAG, "BLE Weapon ready. Waiting for a target to connect...");
}

void BLEWeapon::start_advertising()
{
    struct ble_gap_adv_params adv_params = {};
    struct ble_hs_adv_fields fields = {};

    // Set advertising data
    const char* name = BLE_WEAPON_NAME;
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t*)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    // Set advertising parameters
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ESP_LOGI(TAG, "Starting advertising...");
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER, &adv_params, gap_event_handler, nullptr);
}

int BLEWeapon::gap_event_handler(struct ble_gap_event* event, void* arg)
{
    if (!instance)
        return 0;

    switch (event->type)
    {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "Connection %s; status=%d", event->connect.status == 0 ? "established" : "failed",
                     event->connect.status);

            if (event->connect.status == 0)
            {
                instance->conn_handle = event->connect.conn_handle;
                instance->deviceConnected = true;
            }
            else
            {
                instance->start_advertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected; reason=%d", event->disconnect.reason);
            instance->deviceConnected = false;
            instance->conn_handle = BLE_HS_CONN_HANDLE_NONE;
            instance->start_advertising();
            break;

        case BLE_GAP_EVENT_CONN_UPDATE:
            ESP_LOGI(TAG, "Connection updated");
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising complete; restarting");
            instance->start_advertising();
            break;
    }

    return 0;
}

int BLEWeapon::gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt,
                                   void* arg)
{
    if (!instance)
        return BLE_ATT_ERR_UNLIKELY;

    switch (ctxt->op)
    {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            os_mbuf_append(ctxt->om, gatt_svr_chr_message, sizeof(gatt_svr_chr_message));
            return 0;

        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}

void BLEWeapon::sendMessage(uint16_t message)
{
    if (deviceConnected && conn_handle != BLE_HS_CONN_HANDLE_NONE)
    {
        gatt_svr_chr_message[0] = (message >> 8) & 0xFF;
        gatt_svr_chr_message[1] = message & 0xFF;

        struct os_mbuf* om = ble_hs_mbuf_from_flat(gatt_svr_chr_message, sizeof(gatt_svr_chr_message));
        if (om)
        {
            int rc = ble_gatts_notify_custom(conn_handle, message_char_handle, om);
            if (rc != 0)
            {
                ESP_LOGW(TAG, "Notify failed; rc=%d", rc);
                os_mbuf_free_chain(om);
            }
        }
    }
}

bool BLEWeapon::isConnected()
{
    return deviceConnected;
}

void BLEWeapon::handleConnection()
{
    if (!deviceConnected && oldDeviceConnected)
    {
        vTaskDelay(pdMS_TO_TICKS(BLE_RECONNECT_DELAY_MS));
        ESP_LOGI(TAG, "BLE disconnected. Restarting advertising...");
        oldDeviceConnected = deviceConnected;
    }

    if (deviceConnected && !oldDeviceConnected)
    {
        ESP_LOGI(TAG, "BLE Target connected!");
        oldDeviceConnected = deviceConnected;
    }
}

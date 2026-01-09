// Keep public API thin and move internals into separate units
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <string.h>


#include "nvs_store.h"
#include "wifi_internal.h"
#include "wifi_manager.h"

static const char* TAG = "WiFiMgr";

EventGroupHandle_t wifi_manager_event_group()
{
    return g_wifi_events;
}

bool wifi_manager_is_connected()
{
    if (!g_wifi_events)
    {
        return false;
    }
    EventBits_t bits = xEventGroupGetBits(g_wifi_events);
    return (bits & WIFI_EVENT_STA_CONNECTED_BIT) != 0;
}

const char* wifi_manager_get_ip()
{
    return g_wifi_ip;
}

int wifi_manager_get_rssi()
{
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
    {
        return ap.rssi;
    }
    return 0;
}

uint32_t wifi_manager_get_uptime_ms()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

uint8_t wifi_manager_get_channel()
{
    return g_wifi_channel;
}

const char* wifi_manager_get_peer_list()
{
    return g_peer_list;
}

bool wifi_manager_set_peer_list(const char* csv_peers)
{
    if (!csv_peers)
        return false;

    strncpy(g_peer_list, csv_peers, sizeof(g_peer_list) - 1);
    g_peer_list[sizeof(g_peer_list) - 1] = '\0';
    return nvs_store_write_str(NVS_NS_WIFI, NVS_KEY_PEERS, g_peer_list);
}

bool wifi_manager_load_peer_list(char* out, size_t max_len)
{
    if (!out || max_len == 0)
        return false;
    bool ok = nvs_store_read_str(NVS_NS_WIFI, NVS_KEY_PEERS, out, max_len);
    if (ok)
    {
        strncpy(g_peer_list, out, sizeof(g_peer_list) - 1);
        g_peer_list[sizeof(g_peer_list) - 1] = '\0';
    }
    return ok;
}

void wifi_manager_factory_reset()
{
    ESP_LOGW(TAG, "Factory reset requested");
    nvs_store_erase_namespace(NVS_NS_WIFI);
    g_peer_list[0] = '\0';
    esp_restart();
}

void wifi_manager_init(const char* device_name, const char* role)
{
    if (!g_wifi_events)
        g_wifi_events = xEventGroupCreate();
    if (device_name)
        strncpy(g_device_name, device_name, sizeof(g_device_name) - 1);
    if (role)
        strncpy(g_role, role, sizeof(g_role) - 1);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();

    // Load cached peer list from NVS (used by ESP-NOW after Wi-Fi is ready)
    nvs_store_read_str(NVS_NS_WIFI, NVS_KEY_PEERS, g_peer_list, sizeof(g_peer_list));
    if (strlen(g_peer_list) > 0)
    {
        ESP_LOGI(TAG, "Loaded %u bytes of peer list from NVS", (unsigned)strlen(g_peer_list));
    }

    wifi_evaluate_boot_mode();
}

// New Accessors for Display
wifi_boot_mode_t wifi_manager_get_boot_mode()
{
    return g_wifi_boot_mode;
}

const char* wifi_manager_get_ssid()
{
    static char ssid_buf[33];
    memset(ssid_buf, 0, sizeof(ssid_buf));

    if (g_wifi_boot_mode == WIFI_BOOT_PROVISIONING)
    {
        wifi_config_t conf;
        if (esp_wifi_get_config(WIFI_IF_AP, &conf) == ESP_OK)
        {
            strncpy(ssid_buf, (char*)conf.ap.ssid, 32);
            return ssid_buf;
        }
        return "AP Mode";
    }
    else
    {
        wifi_config_t conf;
        if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK)
        {
            strncpy(ssid_buf, (char*)conf.sta.ssid, 32);
            return ssid_buf;
        }
        return "?";
    }
}

const char* wifi_manager_get_status_string()
{
    if (g_wifi_boot_mode == WIFI_BOOT_PROVISIONING)
    {
        return "AP Active";
    }

    if (wifi_manager_is_connected())
    {
        return "Online";
    }

    return "Connecting...";
}

const char* wifi_manager_get_device_name()
{
    return g_device_name;
}

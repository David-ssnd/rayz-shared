// Keep public API thin and move internals into separate units
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
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

void wifi_manager_factory_reset()
{
    ESP_LOGW(TAG, "Factory reset requested");
    nvs_store_erase_namespace(NVS_NS_WIFI);
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

    wifi_evaluate_boot_mode();
}

#include "http_api.h"
#include "nvs_store.h"
#include "wifi_internal.h"
#include "ws_server.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <string.h>

static const char* TAG = "WiFiCore";

static void on_got_ip(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)data;
    snprintf(g_wifi_ip, sizeof(g_wifi_ip), IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Got IP: %s", g_wifi_ip);
    xEventGroupSetBits(g_wifi_events, WIFI_EVENT_STA_CONNECTED_BIT);
    wifi_start_http_server(false);
    http_api_start(g_httpd);
    ws_server_register(g_httpd);
}

void wifi_start_ap()
{
    ESP_LOGI(TAG, "Starting AP provisioning mode");
    g_wifi_boot_mode = WIFI_BOOT_PROVISIONING;

    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_AP);

    wifi_config_t ap_config = {};
    strcpy((char*)ap_config.ap.ssid, "RayZ-Setup");
    ap_config.ap.ssid_len = strlen("RayZ-Setup");
    ap_config.ap.channel = 1;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.max_connection = 4;
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();

    wifi_start_http_server(true);
}

void wifi_start_sta(const char* ssid, const char* pass)
{
    g_wifi_boot_mode = WIFI_BOOT_STA;
    ESP_LOGI(TAG, "Starting STA mode SSID=%s", ssid);
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL, NULL);
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t sta_config = {};
    strncpy((char*)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid));
    strncpy((char*)sta_config.sta.password, pass, sizeof(sta_config.sta.password));
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    esp_wifi_start();
    esp_wifi_connect();
}

void wifi_evaluate_boot_mode()
{
    char ssid[WIFI_MAX_SSID_LEN] = {0};
    char pass[WIFI_MAX_PASS_LEN] = {0};
    bool have = nvs_store_read_str(NVS_NS_WIFI, NVS_KEY_SSID, ssid, sizeof(ssid));
    if (have)
    {
        nvs_store_read_str(NVS_NS_WIFI, NVS_KEY_PASS, pass, sizeof(pass));
        wifi_start_sta(ssid, pass);
        xEventGroupSetBits(g_wifi_events, WIFI_EVENT_PROVISIONED_BIT);
    }
    else
    {
        wifi_start_ap();
    }
}

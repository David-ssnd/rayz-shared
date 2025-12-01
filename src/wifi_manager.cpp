#include "wifi_manager.h"
#include "http_api.h"
#include "nvs_store.h"
#include "ws_server.h"


#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <string.h>


static const char* TAG = "WiFiMgr";
static EventGroupHandle_t s_event_group = NULL;
static wifi_boot_mode_t s_boot_mode = WIFI_BOOT_PROVISIONING;
static char s_ip[16] = {0};
static httpd_handle_t s_httpd = NULL;
static char s_device_name[32] = {0};
static char s_role[12] = {0};

// Namespace & keys
#define NVS_NS_WIFI "wifi"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASS "pass"
#define NVS_KEY_NAME "name"
#define NVS_KEY_ROLE "role"

static void start_ap();
static void start_sta(const char* ssid, const char* pass);
static void start_http_server(bool provisioning_mode);
static void evaluate_boot_mode();

EventGroupHandle_t wifi_manager_event_group()
{
    return s_event_group;
}

bool wifi_manager_is_connected()
{
    EventBits_t bits = xEventGroupGetBits(s_event_group);
    return (bits & WIFI_EVENT_STA_CONNECTED_BIT) != 0;
}

const char* wifi_manager_get_ip()
{
    return s_ip;
}

static esp_err_t root_get_handler(httpd_req_t* req)
{
    if (s_boot_mode == WIFI_BOOT_PROVISIONING)
    {
        const char* page =
            "<html><body><h2>RayZ Provisioning</h2><form method='POST' action='/config'>"
            "SSID:<br><input name='ssid' maxlength='32'><br>"
            "Password:<br><input name='pass' type='password' maxlength='64'><br>"
            "Device Name:<br><input name='name' maxlength='32'><br>"
            "Role:<br><select name='role'><option>weapon</option><option>target</option></select><br><br>"
            "<input type='submit' value='Save & Connect'></form></body></html>";
        httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    }
    else
    {
        const char* page = "<html><body><h2>RayZ Online</h2><p>Device connected.</p></body></html>";
        httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

static esp_err_t config_post_handler(httpd_req_t* req)
{
    if (s_boot_mode != WIFI_BOOT_PROVISIONING)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Already provisioned");
        return ESP_OK;
    }
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_OK;
    }
    buf[len] = '\0';
    // Very simple parsing: ssid=...&pass=...&name=...&role=...
    char ssid[WIFI_MAX_SSID_LEN] = {0};
    char pass[WIFI_MAX_PASS_LEN] = {0};
    char name[32] = {0};
    char role[12] = {0};

    // Helper lambda
    auto extract = [&](const char* key, char* out, size_t max)
    {
        const char* p = strstr(buf, key);
        if (!p)
            return;
        p += strlen(key);
        const char* end = strchr(p, '&');
        size_t l = end ? (size_t)(end - p) : strlen(p);
        if (l >= max)
            l = max - 1;
        memcpy(out, p, l);
        out[l] = '\0';
    };

    extract("ssid=", ssid, sizeof(ssid));
    extract("pass=", pass, sizeof(pass));
    extract("name=", name, sizeof(name));
    extract("role=", role, sizeof(role));

    if (ssid[0] == '\0')
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing SSID");
        return ESP_OK;
    }

    nvs_store_write_str(NVS_NS_WIFI, NVS_KEY_SSID, ssid);
    nvs_store_write_str(NVS_NS_WIFI, NVS_KEY_PASS, pass);
    if (name[0])
        nvs_store_write_str(NVS_NS_WIFI, NVS_KEY_NAME, name);
    if (role[0])
        nvs_store_write_str(NVS_NS_WIFI, NVS_KEY_ROLE, role);

    httpd_resp_sendstr(req, "Stored. Reconnecting...");
    ESP_LOGI(TAG, "Provisioned SSID=%s name=%s role=%s", ssid, name, role);

    // Small delay then connect
    vTaskDelay(pdMS_TO_TICKS(500));

    // Stop AP server and switch
    if (s_httpd)
    {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }
    start_sta(ssid, pass);
    return ESP_OK;
}

static void start_http_server(bool provisioning_mode)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    if (httpd_start(&s_httpd, &config) == ESP_OK)
    {
        httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler, .user_ctx = NULL};
        httpd_register_uri_handler(s_httpd, &root_uri);
        if (provisioning_mode)
        {
            httpd_uri_t cfg_uri = {
                .uri = "/config", .method = HTTP_POST, .handler = config_post_handler, .user_ctx = NULL};
            httpd_register_uri_handler(s_httpd, &cfg_uri);
        }
        // Additional API endpoints registered later once connected
    }
}

static void on_got_ip(void* arg, esp_event_base_t base, int32_t id, void* data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)data;
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Got IP: %s", s_ip);
    xEventGroupSetBits(s_event_group, WIFI_EVENT_STA_CONNECTED_BIT);
    start_http_server(false);
    http_api_start(s_httpd);
    ws_server_register(s_httpd);
}

static void start_ap()
{
    ESP_LOGI(TAG, "Starting AP provisioning mode");
    s_boot_mode = WIFI_BOOT_PROVISIONING;

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

    start_http_server(true);
}

static void start_sta(const char* ssid, const char* pass)
{
    s_boot_mode = WIFI_BOOT_STA;
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

static void evaluate_boot_mode()
{
    char ssid[WIFI_MAX_SSID_LEN] = {0};
    char pass[WIFI_MAX_PASS_LEN] = {0};
    bool have = nvs_store_read_str(NVS_NS_WIFI, NVS_KEY_SSID, ssid, sizeof(ssid));
    if (have)
    {
        nvs_store_read_str(NVS_NS_WIFI, NVS_KEY_PASS, pass, sizeof(pass));
        start_sta(ssid, pass);
        xEventGroupSetBits(s_event_group, WIFI_EVENT_PROVISIONED_BIT);
    }
    else
    {
        start_ap();
    }
}

void wifi_manager_factory_reset()
{
    ESP_LOGW(TAG, "Factory reset requested");
    nvs_store_erase_namespace(NVS_NS_WIFI);
    // Simple strategy: restart
    esp_restart();
}

void wifi_manager_init(const char* device_name, const char* role)
{
    if (!s_event_group)
        s_event_group = xEventGroupCreate();
    if (device_name)
        strncpy(s_device_name, device_name, sizeof(s_device_name) - 1);
    if (role)
        strncpy(s_role, role, sizeof(s_role) - 1);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();

    evaluate_boot_mode();
}

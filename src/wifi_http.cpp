#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <errno.h>
#include <string.h>

#include "nvs_store.h"
#include "wifi_internal.h"

static const char* TAG = "WiFiHttp";

static esp_err_t root_get_handler(httpd_req_t* req)
{
    if (g_wifi_boot_mode == WIFI_BOOT_PROVISIONING)
    {
        const char* page = "<!DOCTYPE html>"
                           "<html>"
                           "<head>"
                           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                           "<style>"
                           "*{box-sizing:border-box}"
                           "body{margin:0;min-height:100vh;display:flex;justify-content:center;align-items:center;"
                           "background:#fff;color:#111;font-family:sans-serif}"
                           "form{background:#fff;padding:20px;border-radius:10px;width:100%;max-width:320px;"
                           "box-shadow:0 10px 30px rgba(0,0,0,.08)}"
                           "h2{text-align:center;margin:0 0 12px}"
                           "input,button{width:100%;padding:10px;margin:6px 0;border-radius:8px;font-size:14px}"
                           "input{border:1px solid #e5e7eb}"
                           "button{border:0;background:#111;color:#fff;font-weight:600}"
                           "</style>"
                           "</head>"
                           "<body>"
                           "<form method=\"POST\" action=\"/config\">"
                           "<h2>RayZ Provisioning</h2>"
                           "<input name=\"ssid\" placeholder=\"SSID\" maxlength=\"32\" required>"
                           "<input name=\"pass\" type=\"password\" placeholder=\"Password\" maxlength=\"64\">"
                           "<input name=\"name\" placeholder=\"Device Name\" maxlength=\"32\" required>"
                           "<button>Save &amp; Connect</button>"
                           "</form>"
                           "</body>"
                           "</html>";

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
    if (g_wifi_boot_mode != WIFI_BOOT_PROVISIONING)
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
    char ssid[WIFI_MAX_SSID_LEN] = {0};
    char pass[WIFI_MAX_PASS_LEN] = {0};
    char name[32] = {0};
    char role[12] = {0};

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

    char response[256];
    snprintf(response, sizeof(response),
             "<html><body><h2>RayZ Provisioning</h2>"
             "<p>Information stored. Trying to connect to wifi: <b>%s</b></p>"
             "<p>Device will now switch to station mode...</p></body></html>",
             ssid);
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Provisioned SSID=%s name=%s role=%s", ssid, name, role);
    ESP_LOGI(TAG, "Restarting device to apply WiFi settings...");

    // Give time for HTTP response to be sent
    vTaskDelay(pdMS_TO_TICKS(500));

    // Clean restart is the safest way to switch from AP to STA
    // This avoids race conditions with netif/driver cleanup
    esp_restart();

    return ESP_OK; // Won't reach here
}

static esp_err_t clean_post_handler(httpd_req_t* req)
{
    if (g_wifi_boot_mode != WIFI_BOOT_STA)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not in STA mode");
        return ESP_OK;
    }

    // Delegate to public API (will restart)
    extern void wifi_manager_factory_reset();
    wifi_manager_factory_reset();
    return ESP_OK;
}

void wifi_start_http_server(bool provisioning_mode)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;
    esp_err_t ret = httpd_start(&g_httpd, &config);
    if (ret == ESP_OK)
    {
        httpd_uri_t root_uri = {.uri = "/",
                                .method = HTTP_GET,
                                .handler = root_get_handler,
                                .user_ctx = NULL,
                                .is_websocket = false,
                                .handle_ws_control_frames = false,
                                .supported_subprotocol = NULL};
        httpd_register_uri_handler(g_httpd, &root_uri);
        if (provisioning_mode)
        {
            httpd_uri_t cfg_uri = {.uri = "/config",
                                   .method = HTTP_POST,
                                   .handler = config_post_handler,
                                   .user_ctx = NULL,
                                   .is_websocket = false,
                                   .handle_ws_control_frames = false,
                                   .supported_subprotocol = NULL};
            httpd_register_uri_handler(g_httpd, &cfg_uri);
        }
        else
        {
            httpd_uri_t cln_uri = {.uri = "/clean",
                                   .method = HTTP_POST,
                                   .handler = clean_post_handler,
                                   .user_ctx = NULL,
                                   .is_websocket = false,
                                   .handle_ws_control_frames = false,
                                   .supported_subprotocol = NULL};
            httpd_register_uri_handler(g_httpd, &cln_uri);
        }
        ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s (esp_err=%d)", esp_err_to_name(ret), ret);
        ESP_LOGE(TAG, "listen() errno=%d: %s", errno, strerror(errno));
    }
}

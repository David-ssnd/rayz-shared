#include "http_api.h"
#include <esp_log.h>
#include <string.h>
#include "wifi_manager.h"

static const char* TAG = "HttpApi";

static char s_status[128];

static esp_err_t status_get_handler(httpd_req_t* req)
{
    const char* json = http_api_get_status_json();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_handle_t http_api_start(httpd_handle_t server)
{
    if (!server)
        return NULL;
    httpd_uri_t status_uri = {
        .uri = "/api/status", .method = HTTP_GET, .handler = status_get_handler, .user_ctx = NULL};
    httpd_register_uri_handler(server, &status_uri);
    return server;
}

const char* http_api_get_status_json()
{
    bool connected = wifi_manager_is_connected();
    const char* ip = wifi_manager_get_ip();
    snprintf(s_status, sizeof(s_status), "{\"wifi\":%s,\"ip\":\"%s\"}", connected ? "true" : "false", ip);
    return s_status;
}

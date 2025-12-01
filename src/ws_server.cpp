#include "ws_server.h"
#include <esp_log.h>
#include <string.h>

static const char* TAG = "WsServer";

#define MAX_WS_CLIENTS 4
static int s_client_fds[MAX_WS_CLIENTS];
static httpd_handle_t s_server = NULL;

static esp_err_t ws_handler(httpd_req_t* req)
{
    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "WebSocket handshake");
        return ESP_OK;
    }
    // Simplified: just accept connections, actual frame handling requires more ESP-IDF WebSocket APIs
    return ESP_OK;
}

void ws_server_register(httpd_handle_t server)
{
    s_server = server;
    memset(s_client_fds, 0, sizeof(s_client_fds));
    httpd_uri_t ws = {.uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .user_ctx = NULL};
    httpd_register_uri_handler(server, &ws);
    ESP_LOGI(TAG, "WebSocket endpoint /ws registered (stub)");
}

void ws_server_broadcast(const char* msg)
{
    // Stub: real implementation requires ESP-IDF 5.x httpd_ws_send_frame API
    if (!msg)
        return;
    ESP_LOGD(TAG, "Broadcast (stub): %s", msg);
}

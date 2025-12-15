#include "http_api.h"
#include <esp_log.h>
#include <string.h>
#include "espnow_comm.h"
#include "wifi_manager.h"

static const char* TAG = "HttpApi";

static char s_status[256];

static esp_err_t status_get_handler(httpd_req_t* req)
{
    const char* json = http_api_get_status_json();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t peers_get_handler(httpd_req_t* req)
{
    char peers[256] = {0};
    wifi_manager_load_peer_list(peers, sizeof(peers));
    char response[320];
    snprintf(response, sizeof(response), "{\"peers\":\"%s\"}", peers);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t peers_post_handler(httpd_req_t* req)
{
    char buf[256] = {0};
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No peer list");
        return ESP_OK;
    }
    buf[len] = '\0';
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
    {
        buf[len - 1] = '\0';
        len--;
    }

    wifi_manager_set_peer_list(buf);
    espnow_comm_load_peers_from_csv(buf);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"stored\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

httpd_handle_t http_api_start(httpd_handle_t server)
{
    if (!server)
        return NULL;
    httpd_uri_t status_uri = {.uri = "/api/status",
                              .method = HTTP_GET,
                              .handler = status_get_handler,
                              .user_ctx = NULL,
                              .is_websocket = false,
                              .handle_ws_control_frames = false,
                              .supported_subprotocol = NULL};
    httpd_uri_t peers_uri_get = {.uri = "/api/peers",
                                 .method = HTTP_GET,
                                 .handler = peers_get_handler,
                                 .user_ctx = NULL,
                                 .is_websocket = false,
                                 .handle_ws_control_frames = false,
                                 .supported_subprotocol = NULL};
    httpd_uri_t peers_uri_post = {.uri = "/api/peers",
                                  .method = HTTP_POST,
                                  .handler = peers_post_handler,
                                  .user_ctx = NULL,
                                  .is_websocket = false,
                                  .handle_ws_control_frames = false,
                                  .supported_subprotocol = NULL};
    httpd_register_uri_handler(server, &status_uri);
    httpd_register_uri_handler(server, &peers_uri_get);
    httpd_register_uri_handler(server, &peers_uri_post);
    ESP_LOGI(TAG, "HTTP API registered");
    return server;
}

const char* http_api_get_status_json()
{
    bool connected = wifi_manager_is_connected();
    const char* ip = wifi_manager_get_ip();
    const char* peers = wifi_manager_get_peer_list();
    snprintf(s_status, sizeof(s_status),
             "{\"wifi\":%s,\"ip\":\"%s\",\"channel\":%u,\"peers\":\"%s\",\"espnow_peers\":%u}",
             connected ? "true" : "false", ip, wifi_manager_get_channel(), peers ? peers : "",
             espnow_comm_peer_count());
    return s_status;
}

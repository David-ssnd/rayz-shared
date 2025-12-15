#include "ws_server.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <stdlib.h>
#include <string.h>
#include "game_state.h"

static const char* TAG = "WsServer";

#define MAX_WS_CLIENTS 4
#define WS_MAX_FRAME_SIZE 512

typedef struct
{
    int fd;
    bool active;
} ws_client_t;

static ws_client_t s_clients[MAX_WS_CLIENTS];
static httpd_handle_t s_server = NULL;
static WsServerConfig s_config = {0};
static bool s_initialized = false;

static char s_send_buffer[WS_MAX_FRAME_SIZE];

// Forward declaration so we can log client count during handshake
int ws_server_client_count(void);

static int find_client_slot(void)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++)
    {
        if (!s_clients[i].active)
            return i;
    }
    return -1;
}

static int find_client_by_fd(int fd)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++)
    {
        if (s_clients[i].active && s_clients[i].fd == fd)
            return i;
    }
    return -1;
}

static void add_client(int fd)
{
    int slot = find_client_slot();
    if (slot >= 0)
    {
        s_clients[slot].fd = fd;
        s_clients[slot].active = true;
        if (s_config.on_connect)
            s_config.on_connect(fd, true);
    }
}

static void remove_client(int fd)
{
    int slot = find_client_by_fd(fd);
    if (slot >= 0)
    {
        s_clients[slot].active = false;
        s_clients[slot].fd = -1;
        if (s_config.on_connect)
            s_config.on_connect(fd, false);
    }
}

static bool json_get_string(const char* json, const char* key, char* out, size_t max_len)
{
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);
    const char* start = strstr(json, search_key);
    if (!start)
        return false;
    start += strlen(search_key);
    const char* end = strchr(start, '"');
    if (!end)
        return false;
    size_t len = end - start;
    if (len >= max_len)
        len = max_len - 1;
    strncpy(out, start, len);
    out[len] = '\0';
    return true;
}

static esp_err_t ws_handler(httpd_req_t* req)
{
    if (req->method == HTTP_GET)
    {
        // Treat handshake as a connect event so we don't wait for first payload
        int client_fd = httpd_req_to_sockfd(req);
        if (find_client_by_fd(client_fd) < 0)
        {
            add_client(client_fd);
            ESP_LOGI(TAG, "WebSocket handshake: fd=%d connected (total=%d)", client_fd, ws_server_client_count());
        }
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK || ws_pkt.len == 0 || ws_pkt.len >= WS_MAX_FRAME_SIZE)
        return ret;

    static char msg[WS_MAX_FRAME_SIZE];
    ws_pkt.payload = (uint8_t*)msg;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK)
        return ret;
    msg[ws_pkt.len] = '\0';

    int client_fd = httpd_req_to_sockfd(req);
    if (find_client_by_fd(client_fd) < 0)
        add_client(client_fd);

    char type[32] = "";
    json_get_string(msg, "type", type, sizeof(type));

    if (s_config.on_message)
        s_config.on_message(client_fd, type, msg);

    return ESP_OK;
}

void ws_server_init(const WsServerConfig* config)
{
    if (config)
        memcpy(&s_config, config, sizeof(WsServerConfig));
    memset(s_clients, 0, sizeof(s_clients));
    for (int i = 0; i < MAX_WS_CLIENTS; i++)
    {
        s_clients[i].fd = -1;
    }
    s_initialized = true;
}

void ws_server_register(httpd_handle_t server)
{
    if (!s_initialized)
        ws_server_init(NULL);
    s_server = server;

    static const httpd_uri_t ws_uri = {.uri = "/ws",
                                       .method = HTTP_GET,
                                       .handler = ws_handler,
                                       .user_ctx = NULL,
#ifdef CONFIG_HTTPD_WS_SUPPORT
                                       .is_websocket = true,
                                       .handle_ws_control_frames = true,
                                       .supported_subprotocol = NULL,
#else
                                       .is_websocket = false,
                                       .handle_ws_control_frames = false,
                                       .supported_subprotocol = NULL,
#endif
    };

    httpd_register_uri_handler(server, &ws_uri);
}

bool ws_server_is_connected(void)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++)
        if (s_clients[i].active)
            return true;
    return false;
}

int ws_server_client_count(void)
{
    int c = 0;
    for (int i = 0; i < MAX_WS_CLIENTS; i++)
        if (s_clients[i].active)
            c++;
    return c;
}

static bool ws_server_send_frame(int fd, const char* message)
{
    if (!s_server || !message)
        return false;
    struct async_send_arg
    {
        httpd_handle_t hd;
        int fd;
        char data[WS_MAX_FRAME_SIZE];
    };
    struct async_send_arg* arg = (struct async_send_arg*)malloc(sizeof(struct async_send_arg));
    if (!arg)
        return false;
    arg->hd = s_server;
    arg->fd = fd;
    strncpy(arg->data, message, sizeof(arg->data) - 1);
    arg->data[sizeof(arg->data) - 1] = '\0';

    auto sender = [](void* a) {
        struct async_send_arg* send_arg = (struct async_send_arg*)a;
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(ws_pkt));
        ws_pkt.payload = (uint8_t*)send_arg->data;
        ws_pkt.len = strlen(send_arg->data);
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
        esp_err_t r = httpd_ws_send_frame_async(send_arg->hd, send_arg->fd, &ws_pkt);
        if (r != ESP_OK)
            ESP_LOGW(TAG, "Send failed fd=%d err=%d", send_arg->fd, r);
        free(send_arg);
    };

    if (httpd_queue_work(s_server, sender, arg) != ESP_OK)
    {
        free(arg);
        return false;
    }
    return true;
}

bool ws_server_send(int client_fd, const char* message)
{
    return ws_server_send_frame(client_fd, message);
}

void ws_server_broadcast(const char* message)
{
    for (int i = 0; i < MAX_WS_CLIENTS; i++)
        if (s_clients[i].active)
            ws_server_send(s_clients[i].fd, message);
}

void ws_server_send_status(void)
{
    const DeviceConfig* cfg = game_state_get_config();
    const GameStateData* st = game_state_get();
    int len = snprintf(s_send_buffer, sizeof(s_send_buffer),
                       "{\"type\":\"status\",\"device_id\":%u,\"player_id\":%u,\"team_id\":%u,\"color_rgb\":%u,"
                       "\"shots\":%lu,\"hits\":%lu,\"kills\":%lu,\"deaths\":%lu,\"hearts\":%u,\"respawning\":%s}",
                       cfg->device_id, cfg->player_id, cfg->team_id, (unsigned)cfg->color_rgb,
                       (unsigned long)st->shots_fired, (unsigned long)st->hits_landed, (unsigned long)st->kills,
                       (unsigned long)st->deaths, (unsigned)st->hearts_remaining, st->respawning ? "true" : "false");
    if (len > 0 && len < (int)sizeof(s_send_buffer))
        ws_server_broadcast(s_send_buffer);
}

void ws_server_send_heartbeat_ack(int client_fd)
{
    const GameStateData* st = game_state_get();
    int len = snprintf(s_send_buffer, sizeof(s_send_buffer),
                       "{\"type\":\"heartbeat_ack\",\"kills\":%lu,\"deaths\":%lu,\"hearts\":%u,\"respawning\":%s}",
                       (unsigned long)st->kills, (unsigned long)st->deaths, (unsigned)st->hearts_remaining,
                       st->respawning ? "true" : "false");
    if (len > 0 && len < (int)sizeof(s_send_buffer))
        ws_server_send(client_fd, s_send_buffer);
}

void ws_server_broadcast_hit(const char* shooter_id_str)
{
    int len = snprintf(s_send_buffer, sizeof(s_send_buffer), "{\"type\":\"hit_report\",\"shooter\":%s}",
                       shooter_id_str ? shooter_id_str : "\"unknown\"");
    if (len > 0 && len < (int)sizeof(s_send_buffer))
        ws_server_broadcast(s_send_buffer);
}

void ws_server_broadcast_shot(void)
{
    const DeviceConfig* cfg = game_state_get_config();
    const GameStateData* st = game_state_get();
    int len = snprintf(s_send_buffer, sizeof(s_send_buffer),
                       "{\"type\":\"shot_fired\",\"device_id\":%u,\"shots\":%lu}",
                       cfg->device_id, (unsigned long)st->shots_fired);
    if (len > 0 && len < (int)sizeof(s_send_buffer))
        ws_server_broadcast(s_send_buffer);
}

void ws_server_broadcast_game_state(void)
{
    ws_server_send_status();
}

void ws_server_broadcast_respawn(void)
{
    const DeviceConfig* cfg = game_state_get_config();
    const GameStateData* st = game_state_get();
    int len = snprintf(s_send_buffer, sizeof(s_send_buffer),
                       "{\"type\":\"respawn\",\"device_id\":%u,\"hearts\":%u,\"respawning\":%s}", cfg->device_id,
                       (unsigned)st->hearts_remaining, st->respawning ? "true" : "false");
    if (len > 0 && len < (int)sizeof(s_send_buffer))
        ws_server_broadcast(s_send_buffer);
}

#include "ws_server.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include "game_state.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char* TAG = "WsServer";

#define MAX_WS_CLIENTS 4
#define WS_MAX_FRAME_SIZE 1024

typedef struct
{
    int fd;
    bool active;
} ws_client_t;

static ws_client_t s_clients[MAX_WS_CLIENTS];
static httpd_handle_t s_server = NULL;
static WsServerConfig s_config = {};
static bool s_initialized = false;
static SemaphoreHandle_t s_ws_mutex = NULL;

// Forward declaration
int ws_server_client_count(void);
void ws_server_send_status_to(int fd);

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
    if (s_ws_mutex) xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    int slot = find_client_slot();
    if (slot >= 0)
    {
        s_clients[slot].fd = fd;
        s_clients[slot].active = true;
        if (s_config.on_connect)
            s_config.on_connect(fd, true);
    }
    if (s_ws_mutex) xSemaphoreGive(s_ws_mutex);
}

// static void remove_client(int fd)
// {
//     if (s_ws_mutex) xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
//     int slot = find_client_by_fd(fd);
//     if (slot >= 0)
//     {
//         s_clients[slot].active = false;
//         s_clients[slot].fd = -1;
//         if (s_config.on_connect)
//             s_config.on_connect(fd, false);
//     }
//     if (s_ws_mutex) xSemaphoreGive(s_ws_mutex);
// }

static void handle_config_update(cJSON* root)
{
    cJSON* reset_item = cJSON_GetObjectItem(root, "reset_to_defaults");
    if (cJSON_IsTrue(reset_item))
    {
        game_state_load_default_game_config();
    }

    DeviceConfig* dev = game_state_get_config_mut();
    cJSON* item = cJSON_GetObjectItem(root, "device_id");
    if (item)
        dev->device_id = item->valueint;
    item = cJSON_GetObjectItem(root, "player_id");
    if (item)
        dev->player_id = item->valueint;
    item = cJSON_GetObjectItem(root, "team_id");
    if (item)
        dev->team_id = item->valueint;
    item = cJSON_GetObjectItem(root, "color_rgb");
    if (item)
        dev->color_rgb = item->valueint;

    GameConfig* game = game_state_get_game_config_mut();
    item = cJSON_GetObjectItem(root, "max_hearts");
    if (item)
        game->max_hearts = item->valueint;
    item = cJSON_GetObjectItem(root, "max_ammo");
    if (item)
        game->max_ammo = item->valueint;
    item = cJSON_GetObjectItem(root, "reload_time_ms");
    if (item)
        game->reload_time_ms = item->valueint;
    item = cJSON_GetObjectItem(root, "game_duration_s");
    if (item)
        game->time_limit_s = item->valueint;
    item = cJSON_GetObjectItem(root, "enable_ammo");
    if (item)
        game->unlimited_ammo = !cJSON_IsTrue(item);
    item = cJSON_GetObjectItem(root, "friendly_fire");
    if (item)
        game->friendly_fire_enabled = cJSON_IsTrue(item);
    item = cJSON_GetObjectItem(root, "respawn_time_s");
    if (item)
        game->respawn_cooldown_ms = item->valueint * 1000;

    // Save device ID changes
    game_state_save_ids();

    // Broadcast update
    ws_server_broadcast_game_state();
}

static void handle_game_command(cJSON* root)
{
    cJSON* cmd_item = cJSON_GetObjectItem(root, "command");
    if (!cmd_item)
        return;

    int cmd = cmd_item->valueint;
    switch (cmd)
    {
        case CMD_RESET:
            game_state_reset_stats();
            game_state_reset_runtime();
            break;
        case CMD_START:
            game_state_reset_runtime();
            // TODO: Start timer/enable game mechanism if separate from runtime
            break;
        case CMD_STOP:
            // TODO: Stop game logic
            break;
    }
    ws_server_broadcast_game_state();
}

static void process_message(int fd, const char* payload)
{
    cJSON* root = cJSON_Parse(payload);
    if (!root)
        return;

    cJSON* op_item = cJSON_GetObjectItem(root, "op");
    int op = op_item ? op_item->valueint : 0;

    // Fallback to type if op is missing (legacy/safety)
    if (op == 0)
    {
        cJSON* type_item = cJSON_GetObjectItem(root, "type");
        if (type_item && type_item->valuestring)
        {
            if (strcmp(type_item->valuestring, "get_status") == 0)
                op = OP_GET_STATUS;
            else if (strcmp(type_item->valuestring, "heartbeat") == 0)
                op = OP_HEARTBEAT;
            else if (strcmp(type_item->valuestring, "config_update") == 0)
                op = OP_CONFIG_UPDATE;
        }
    }

    switch (op)
    {
        case OP_GET_STATUS:
            ws_server_send_status_to(fd);
            break;
        case OP_HEARTBEAT:
            ws_server_send_heartbeat_ack(fd);
            break;
        case OP_CONFIG_UPDATE:
            handle_config_update(root);
            break;
        case OP_GAME_COMMAND:
            handle_game_command(root);
            break;
        case OP_KILL_CONFIRMED:
            game_state_record_kill();
            ws_server_broadcast_game_state();
            break;
    }

    cJSON_Delete(root);
}

static esp_err_t ws_handler(httpd_req_t* req)
{
    if (req->method == HTTP_GET)
    {
        // Treat handshake as a connect event
        int client_fd = httpd_req_to_sockfd(req);
        
        // Lock only when checking/modifying list
        if (s_ws_mutex) xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
        int found = find_client_by_fd(client_fd);
        if (s_ws_mutex) xSemaphoreGive(s_ws_mutex);

        if (found < 0)
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
    
    // Ensure client is tracked
    if (s_ws_mutex) xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    int found = find_client_by_fd(client_fd);
    if (s_ws_mutex) xSemaphoreGive(s_ws_mutex);
    
    if (found < 0)
        add_client(client_fd);

    process_message(client_fd, msg);

    if (s_config.on_message)
        s_config.on_message(client_fd, "", msg);

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
    
    if (!s_ws_mutex) {
        s_ws_mutex = xSemaphoreCreateMutex();
    }
    
    s_initialized = true;
}

void ws_server_register(httpd_handle_t server)
{
    if (!s_initialized)
        ws_server_init(NULL);
    s_server = server;

    static const httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = true,
        .supported_subprotocol = NULL,
    };

    httpd_register_uri_handler(server, &ws_uri);
}

bool ws_server_is_connected(void)
{
    bool connected = false;
    if (s_ws_mutex) xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++)
        if (s_clients[i].active) {
            connected = true;
            break;
        }
    if (s_ws_mutex) xSemaphoreGive(s_ws_mutex);
    return connected;
}

int ws_server_client_count(void)
{
    int c = 0;
    if (s_ws_mutex) xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++)
        if (s_clients[i].active)
            c++;
    if (s_ws_mutex) xSemaphoreGive(s_ws_mutex);
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

    auto sender = [](void* a)
    {
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
    if (s_ws_mutex) xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++)
        if (s_clients[i].active)
            ws_server_send(s_clients[i].fd, message);
    if (s_ws_mutex) xSemaphoreGive(s_ws_mutex);
}

static cJSON* create_status_json()
{
    const DeviceConfig* cfg = game_state_get_config();
    const GameStateData* st = game_state_get();
    const GameConfig* game = game_state_get_game_config();

    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "op", OP_STATUS);
    cJSON_AddStringToObject(root, "type", "status");
    cJSON_AddNumberToObject(root, "uptime_ms", esp_timer_get_time() / 1000);

    cJSON* config = cJSON_CreateObject();
    cJSON_AddNumberToObject(config, "device_id", cfg->device_id);
    cJSON_AddNumberToObject(config, "player_id", cfg->player_id);
    cJSON_AddNumberToObject(config, "team_id", cfg->team_id);
    cJSON_AddNumberToObject(config, "color_rgb", cfg->color_rgb);
    cJSON_AddBoolToObject(config, "enable_hearts", true);
    cJSON_AddNumberToObject(config, "max_hearts", game->max_hearts);
    cJSON_AddNumberToObject(config, "enable_ammo", !game->unlimited_ammo);
    cJSON_AddNumberToObject(config, "max_ammo", game->max_ammo);
    cJSON_AddNumberToObject(config, "game_duration_s", game->time_limit_s);
    cJSON_AddBoolToObject(config, "friendly_fire", game->friendly_fire_enabled);
    cJSON_AddItemToObject(root, "config", config);

    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "shots", st->shots_fired);
    cJSON_AddNumberToObject(stats, "enemy_kills", st->kills);
    cJSON_AddNumberToObject(stats, "friendly_kills", st->friendly_fire_count);
    cJSON_AddNumberToObject(stats, "deaths", st->deaths);
    cJSON_AddItemToObject(root, "stats", stats);

    cJSON* state = cJSON_CreateObject();
    cJSON_AddNumberToObject(state, "current_hearts", st->hearts_remaining);
    cJSON_AddNumberToObject(state, "current_ammo", 0);
    cJSON_AddBoolToObject(state, "is_respawning", st->respawning);
    cJSON_AddBoolToObject(state, "is_reloading", false);
    cJSON_AddItemToObject(root, "state", state);

    return root;
}

void ws_server_send_status_to(int fd)
{
    cJSON* root = create_status_json();
    char* str = cJSON_PrintUnformatted(root);
    ws_server_send(fd, str);
    free(str);
    cJSON_Delete(root);
}

void ws_server_send_status(void)
{
    cJSON* root = create_status_json();
    char* str = cJSON_PrintUnformatted(root);
    ws_server_broadcast(str);
    free(str);
    cJSON_Delete(root);
}

void ws_server_send_heartbeat_ack(int client_fd)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "op", OP_HEARTBEAT_ACK);
    cJSON_AddStringToObject(root, "type", "heartbeat_ack");
    char* str = cJSON_PrintUnformatted(root);
    ws_server_send(client_fd, str);
    free(str);
    cJSON_Delete(root);
}

void ws_server_broadcast_hit(const char* shooter_id_str)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "op", OP_HIT_REPORT);
    cJSON_AddStringToObject(root, "type", "hit_report");
    cJSON_AddNumberToObject(root, "timestamp_ms", esp_timer_get_time() / 1000);
    int shooter = shooter_id_str ? atoi(shooter_id_str) : 0;
    cJSON_AddNumberToObject(root, "shooter_id", shooter);

    char* str = cJSON_PrintUnformatted(root);
    ws_server_broadcast(str);
    free(str);
    cJSON_Delete(root);
}

void ws_server_broadcast_shot(void)
{
    const GameStateData* st = game_state_get();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "op", OP_SHOT_FIRED);
    cJSON_AddStringToObject(root, "type", "shot_fired");
    cJSON_AddNumberToObject(root, "timestamp_ms", esp_timer_get_time() / 1000);
    cJSON_AddNumberToObject(root, "seq_id", st->shots_fired);

    char* str = cJSON_PrintUnformatted(root);
    ws_server_broadcast(str);
    free(str);
    cJSON_Delete(root);
}

void ws_server_broadcast_game_state(void)
{
    ws_server_send_status();
}

void ws_server_broadcast_respawn(void)
{
    const GameStateData* st = game_state_get();
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "op", OP_RESPAWN);
    cJSON_AddStringToObject(root, "type", "respawn");
    cJSON_AddNumberToObject(root, "timestamp_ms", esp_timer_get_time() / 1000);
    cJSON_AddNumberToObject(root, "current_hearts", st->hearts_remaining);

    char* str = cJSON_PrintUnformatted(root);
    ws_server_broadcast(str);
    free(str);
    cJSON_Delete(root);
}

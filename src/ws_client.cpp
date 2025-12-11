#include "ws_client.h"
#include "game_state.h"
#include "game_protocol.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char* TAG = "WsClient";

// ============================================================================
// STATIC DATA
// ============================================================================

static esp_websocket_client_handle_t s_client = NULL;
static WsClientConfig s_config;
static bool s_initialized = false;
static bool s_connected = false;
static char s_server_uri[128] = "";

static EventGroupHandle_t s_event_group = NULL;
#define WS_CONNECTED_BIT    BIT0
#define WS_DISCONNECTED_BIT BIT1

// Message buffer
#define WS_MSG_BUFFER_SIZE 1024
static char s_msg_buffer[WS_MSG_BUFFER_SIZE];

// ============================================================================
// JSON PARSING HELPERS
// ============================================================================

// Simple JSON string value extraction (no external library needed)
static bool json_get_string(const char* json, const char* key, char* out, size_t max_len) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", key);
    
    const char* start = strstr(json, search_key);
    if (!start) return false;
    
    start += strlen(search_key);
    const char* end = strchr(start, '"');
    if (!end) return false;
    
    size_t len = end - start;
    if (len >= max_len) len = max_len - 1;
    
    strncpy(out, start, len);
    out[len] = '\0';
    return true;
}

static bool json_get_bool(const char* json, const char* key, bool* out) {
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char* start = strstr(json, search_key);
    if (!start) return false;
    
    start += strlen(search_key);
    while (*start == ' ') start++;
    
    if (strncmp(start, "true", 4) == 0) {
        *out = true;
        return true;
    } else if (strncmp(start, "false", 5) == 0) {
        *out = false;
        return true;
    }
    return false;
}

// Get message type from JSON
static ServerMessageType parse_server_msg_type(const char* json) {
    char type_str[32] = "";
    if (!json_get_string(json, JSON_KEY_TYPE, type_str, sizeof(type_str))) {
        return MSG_SERVER_COUNT;  // Invalid
    }
    
    for (int i = 0; i < MSG_SERVER_COUNT; i++) {
        if (strcmp(type_str, SERVER_MSG_NAMES[i]) == 0) {
            return (ServerMessageType)i;
        }
    }
    
    return MSG_SERVER_COUNT;  // Unknown
}

// Parse gamemode from string
static GameMode parse_gamemode(const char* mode_str) {
    for (int i = 0; i < GAMEMODE_COUNT; i++) {
        if (strcmp(mode_str, GAMEMODE_NAMES[i]) == 0) {
            return (GameMode)i;
        }
    }
    return GAMEMODE_FREE;
}

// Parse game state from string
static GameState parse_game_state(const char* state_str) {
    for (int i = 0; i < GAME_STATE_COUNT; i++) {
        if (strcmp(state_str, GAME_STATE_NAMES[i]) == 0) {
            return (GameState)i;
        }
    }
    return GAME_STATE_IDLE;
}

// ============================================================================
// MESSAGE HANDLERS
// ============================================================================

static void handle_register_ack(const char* json) {
    ESP_LOGI(TAG, "Registration acknowledged by server");
    
    // Server may have assigned/updated our device ID
    char new_id[MAX_DEVICE_ID_LEN];
    if (json_get_string(json, JSON_KEY_DEVICE_ID, new_id, sizeof(new_id))) {
        DeviceConfig* config = game_state_get_config_mut();
        if (strcmp(config->device_id, new_id) != 0) {
            strncpy(config->device_id, new_id, sizeof(config->device_id) - 1);
            game_state_save_config();
            ESP_LOGI(TAG, "Device ID updated to: %s", new_id);
        }
    }
}

static void handle_heartbeat_ack(const char* json) {
    ESP_LOGD(TAG, "Heartbeat acknowledged");
    game_state_update_heartbeat();
}

static void handle_config_update(const char* json) {
    ESP_LOGI(TAG, "Config update received");
    
    DeviceConfig* config = game_state_get_config_mut();
    
    // Update individual fields if present
    char buffer[64];
    
    if (json_get_string(json, JSON_KEY_DEVICE_ID, buffer, sizeof(buffer))) {
        strncpy(config->device_id, buffer, sizeof(config->device_id) - 1);
    }
    if (json_get_string(json, JSON_KEY_DEVICE_NAME, buffer, sizeof(buffer))) {
        strncpy(config->device_name, buffer, sizeof(config->device_name) - 1);
    }
    if (json_get_string(json, JSON_KEY_PLAYER_ID, buffer, sizeof(buffer))) {
        strncpy(config->player_id, buffer, sizeof(config->player_id) - 1);
    }
    if (json_get_string(json, JSON_KEY_TEAM, buffer, sizeof(buffer))) {
        strncpy(config->team, buffer, sizeof(config->team) - 1);
    }
    if (json_get_string(json, JSON_KEY_COLOR, buffer, sizeof(buffer))) {
        strncpy(config->color, buffer, sizeof(config->color) - 1);
    }
    
    // Handle teammates array (simplified - expects comma-separated in JSON)
    if (json_get_string(json, JSON_KEY_TEAMMATES, buffer, sizeof(buffer))) {
        strncpy(config->teammates, buffer, sizeof(config->teammates) - 1);
    }
    
    // Handle enemies array
    if (json_get_string(json, JSON_KEY_ENEMIES, buffer, sizeof(buffer))) {
        strncpy(config->enemies, buffer, sizeof(config->enemies) - 1);
    }
    
    game_state_save_config();
    
    if (s_config.on_config) {
        s_config.on_config(json);
    }
}

static void handle_game_start(const char* json) {
    ESP_LOGI(TAG, "Game starting!");
    
    char mode_str[32] = "";
    json_get_string(json, JSON_KEY_GAMEMODE, mode_str, sizeof(mode_str));
    
    GameMode mode = parse_gamemode(mode_str);
    game_state_set_mode(mode);
    game_state_set_state(GAME_STATE_PLAYING);
    game_state_reset_stats();
    
    if (s_config.on_game_state) {
        s_config.on_game_state(mode, GAME_STATE_PLAYING);
    }
}

static void handle_game_end(const char* json) {
    ESP_LOGI(TAG, "Game ended!");
    game_state_set_state(GAME_STATE_ENDED);
    
    if (s_config.on_game_state) {
        s_config.on_game_state(game_state_get()->mode, GAME_STATE_ENDED);
    }
}

static void handle_game_mode_change(const char* json) {
    char mode_str[32] = "";
    json_get_string(json, JSON_KEY_GAMEMODE, mode_str, sizeof(mode_str));
    
    GameMode mode = parse_gamemode(mode_str);
    ESP_LOGI(TAG, "Game mode changed to: %s", GAMEMODE_NAMES[mode]);
    game_state_set_mode(mode);
    
    if (s_config.on_game_state) {
        s_config.on_game_state(mode, game_state_get()->state);
    }
}

static void handle_hit_confirmed(const char* json) {
    char shooter_id[MAX_DEVICE_ID_LEN] = "";
    char target_id[MAX_DEVICE_ID_LEN] = "";
    
    json_get_string(json, JSON_KEY_SHOOTER_ID, shooter_id, sizeof(shooter_id));
    json_get_string(json, JSON_KEY_TARGET_ID, target_id, sizeof(target_id));
    
    ESP_LOGI(TAG, "Hit confirmed: %s -> %s", shooter_id, target_id);
    
    // If we're the shooter, record the hit
    const DeviceConfig* config = game_state_get_config();
    if (strcmp(config->device_id, shooter_id) == 0) {
        game_state_record_hit();
    }
    
    if (s_config.on_hit) {
        s_config.on_hit(shooter_id, target_id, true);
    }
}

static void handle_hit_invalid(const char* json) {
    char shooter_id[MAX_DEVICE_ID_LEN] = "";
    char target_id[MAX_DEVICE_ID_LEN] = "";
    
    json_get_string(json, JSON_KEY_SHOOTER_ID, shooter_id, sizeof(shooter_id));
    json_get_string(json, JSON_KEY_TARGET_ID, target_id, sizeof(target_id));
    
    ESP_LOGW(TAG, "Hit invalid (friendly fire?): %s -> %s", shooter_id, target_id);
    game_state_record_friendly_fire();
    
    if (s_config.on_hit) {
        s_config.on_hit(shooter_id, target_id, false);
    }
}

static void handle_you_were_hit(const char* json) {
    char shooter_id[MAX_DEVICE_ID_LEN] = "";
    json_get_string(json, JSON_KEY_SHOOTER_ID, shooter_id, sizeof(shooter_id));
    
    ESP_LOGI(TAG, "We were hit by: %s", shooter_id);
    game_state_record_death();
    
    if (s_config.on_hit) {
        s_config.on_hit(shooter_id, game_state_get_config()->device_id, true);
    }
}

static void handle_player_update(const char* json) {
    // Update our stats from server (authoritative)
    ESP_LOGD(TAG, "Player update received");
    
    // Could parse and update kills, deaths, etc. if server is authoritative
}

// ============================================================================
// WEBSOCKET EVENT HANDLER
// ============================================================================

static void ws_event_handler(void* handler_args, esp_event_base_t base, 
                            int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected to server");
            s_connected = true;
            game_state_set_connected(true);
            
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, WS_CONNECTED_BIT);
                xEventGroupClearBits(s_event_group, WS_DISCONNECTED_BIT);
            }
            
            if (s_config.on_connect) {
                s_config.on_connect(true);
            }
            
            // Auto-register on connect
            ws_client_send_register();
            break;
            
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected");
            s_connected = false;
            game_state_set_connected(false);
            
            if (s_event_group) {
                xEventGroupSetBits(s_event_group, WS_DISCONNECTED_BIT);
                xEventGroupClearBits(s_event_group, WS_CONNECTED_BIT);
            }
            
            if (s_config.on_connect) {
                s_config.on_connect(false);
            }
            break;
            
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == 0x01) {  // Text frame
                // Copy data to buffer (may be fragmented)
                size_t copy_len = data->data_len;
                if (copy_len >= WS_MSG_BUFFER_SIZE) {
                    copy_len = WS_MSG_BUFFER_SIZE - 1;
                }
                memcpy(s_msg_buffer, data->data_ptr, copy_len);
                s_msg_buffer[copy_len] = '\0';
                
                ESP_LOGD(TAG, "Received: %s", s_msg_buffer);
                
                // Parse and handle message
                ServerMessageType msg_type = parse_server_msg_type(s_msg_buffer);
                
                switch (msg_type) {
                    case MSG_REGISTER_ACK:
                        handle_register_ack(s_msg_buffer);
                        break;
                    case MSG_HEARTBEAT_ACK:
                        handle_heartbeat_ack(s_msg_buffer);
                        break;
                    case MSG_CONFIG_UPDATE:
                        handle_config_update(s_msg_buffer);
                        break;
                    case MSG_GAME_START:
                        handle_game_start(s_msg_buffer);
                        break;
                    case MSG_GAME_END:
                        handle_game_end(s_msg_buffer);
                        break;
                    case MSG_GAME_MODE_CHANGE:
                        handle_game_mode_change(s_msg_buffer);
                        break;
                    case MSG_HIT_CONFIRMED:
                        handle_hit_confirmed(s_msg_buffer);
                        break;
                    case MSG_HIT_INVALID:
                        handle_hit_invalid(s_msg_buffer);
                        break;
                    case MSG_YOU_WERE_HIT:
                        handle_you_were_hit(s_msg_buffer);
                        break;
                    case MSG_PLAYER_UPDATE:
                        handle_player_update(s_msg_buffer);
                        break;
                    default:
                        ESP_LOGW(TAG, "Unknown message type");
                        break;
                }
                
                // General callback
                if (s_config.on_message && msg_type < MSG_SERVER_COUNT) {
                    s_config.on_message(msg_type, s_msg_buffer);
                }
            }
            break;
            
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            break;
            
        default:
            break;
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

bool ws_client_init(const WsClientConfig* config) {
    if (s_initialized) {
        return true;
    }
    
    if (!config || !config->server_uri) {
        ESP_LOGE(TAG, "Invalid config");
        return false;
    }
    
    memcpy(&s_config, config, sizeof(WsClientConfig));
    strncpy(s_server_uri, config->server_uri, sizeof(s_server_uri) - 1);
    
    s_event_group = xEventGroupCreate();
    if (!s_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return false;
    }
    
    s_initialized = true;
    ESP_LOGI(TAG, "WebSocket client initialized for %s", s_server_uri);
    
    return true;
}

bool ws_client_start(void) {
    if (!s_initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return false;
    }
    
    if (s_client) {
        ESP_LOGW(TAG, "Already started");
        return true;
    }
    
    esp_websocket_client_config_t ws_cfg = {};
    ws_cfg.uri = s_server_uri;
    ws_cfg.reconnect_timeout_ms = 5000;
    ws_cfg.network_timeout_ms = 10000;
    
    s_client = esp_websocket_client_init(&ws_cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init websocket client");
        return false;
    }
    
    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL);
    
    esp_err_t err = esp_websocket_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start websocket client: %s", esp_err_to_name(err));
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
        return false;
    }
    
    ESP_LOGI(TAG, "WebSocket client started, connecting to %s", s_server_uri);
    return true;
}

void ws_client_stop(void) {
    if (s_client) {
        esp_websocket_client_stop(s_client);
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
        s_connected = false;
    }
}

bool ws_client_is_connected(void) {
    return s_connected && s_client && esp_websocket_client_is_connected(s_client);
}

void ws_client_set_server_uri(const char* uri) {
    if (uri) {
        strncpy(s_server_uri, uri, sizeof(s_server_uri) - 1);
        
        // If already running, restart with new URI
        if (s_client) {
            ws_client_stop();
            ws_client_start();
        }
    }
}

bool ws_client_send(const char* json) {
    if (!ws_client_is_connected() || !json) {
        return false;
    }
    
    int len = strlen(json);
    int sent = esp_websocket_client_send_text(s_client, json, len, portMAX_DELAY);
    
    if (sent < 0) {
        ESP_LOGE(TAG, "Failed to send message");
        return false;
    }
    
    ESP_LOGD(TAG, "Sent: %s", json);
    return true;
}

bool ws_client_send_register(void) {
    char buffer[512];
    if (game_state_create_register_json(buffer, sizeof(buffer)) > 0) {
        return ws_client_send(buffer);
    }
    return false;
}

bool ws_client_send_heartbeat(void) {
    char buffer[512];
    if (game_state_create_heartbeat_json(buffer, sizeof(buffer)) > 0) {
        bool sent = ws_client_send(buffer);
        if (sent) {
            game_state_update_heartbeat();
        }
        return sent;
    }
    return false;
}

bool ws_client_send_hit_report(const char* shooter_id) {
    char buffer[256];
    if (game_state_create_hit_report_json(buffer, sizeof(buffer), shooter_id) > 0) {
        return ws_client_send(buffer);
    }
    return false;
}

bool ws_client_send_shot_fired(void) {
    game_state_record_shot();
    
    char buffer[256];
    if (game_state_create_shot_fired_json(buffer, sizeof(buffer)) > 0) {
        return ws_client_send(buffer);
    }
    return false;
}

bool ws_client_send_respawn_complete(void) {
    char buffer[128];
    const DeviceConfig* config = game_state_get_config();
    
    int len = snprintf(buffer, sizeof(buffer),
        "{\"%s\":\"%s\",\"%s\":\"%s\"}",
        JSON_KEY_TYPE, CLIENT_MSG_NAMES[MSG_RESPAWN_COMPLETE],
        JSON_KEY_DEVICE_ID, config->device_id
    );
    
    if (len > 0 && len < (int)sizeof(buffer)) {
        return ws_client_send(buffer);
    }
    return false;
}

void ws_client_task(void* params) {
    ESP_LOGI(TAG, "WebSocket client task started");
    
    // Wait for WiFi connection before starting
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    if (!ws_client_start()) {
        ESP_LOGE(TAG, "Failed to start WebSocket client");
        vTaskDelete(NULL);
        return;
    }
    
    while (1) {
        // Check if heartbeat is due
        if (ws_client_is_connected() && game_state_heartbeat_due()) {
            ws_client_send_heartbeat();
        }
        
        // Check respawn timer
        if (game_state_check_respawn()) {
            ws_client_send_respawn_complete();
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // Check every second
    }
}

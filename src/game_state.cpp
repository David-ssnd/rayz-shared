#include "game_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_store.h"
#include "wifi_manager.h"


static const char* TAG = "game_state";

// ============================================================================
// STATIC DATA
// ============================================================================

static DeviceConfig s_config;
static GameStateData s_game_state;
static SemaphoreHandle_t s_mutex = NULL;
static bool s_initialized = false;

// NVS namespace and keys for game config
#define NVS_GAME_NS "game"
#define NVS_KEY_DEVICE_ID "device_id"
#define NVS_KEY_DEVICE_NAME "device_name"
#define NVS_KEY_PLAYER_ID "player_id"
#define NVS_KEY_ROLE "role"
#define NVS_KEY_TEAM "team"
#define NVS_KEY_COLOR "color"
#define NVS_KEY_TEAMMATES "teammates"
#define NVS_KEY_ENEMIES "enemies"
#define NVS_KEY_RESPAWN_MS "respawn_ms"
#define NVS_KEY_FF_ENABLED "ff_enabled"

// ============================================================================
// HELPER MACROS
// ============================================================================

#define LOCK()                                                                                                         \
    if (s_mutex)                                                                                                       \
    xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK()                                                                                                       \
    if (s_mutex)                                                                                                       \
    xSemaphoreGive(s_mutex)

// ============================================================================
// INITIALIZATION
// ============================================================================

bool game_state_init(DeviceRole role)
{
    if (s_initialized)
    {
        return true;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }

    // Initialize config with defaults
    memset(&s_config, 0, sizeof(s_config));
    s_config.role = role;
    s_config.respawn_time_ms = DEFAULT_RESPAWN_TIME_MS;
    s_config.friendly_fire_enabled = false;
    snprintf(s_config.color, sizeof(s_config.color), "#FF0000"); // Default red

    // Initialize game state
    memset(&s_game_state, 0, sizeof(s_game_state));
    s_game_state.mode = GAMEMODE_FREE;
    s_game_state.state = GAME_STATE_IDLE;
    s_game_state.health = DEFAULT_HEALTH;
    s_game_state.max_health = DEFAULT_HEALTH;
    s_game_state.server_connected = false;

    // Try to load from NVS
    if (!game_state_load_config())
    {
        // Generate new device ID if not found
        game_state_generate_id(s_config.device_id, sizeof(s_config.device_id));
        ESP_LOGI(TAG, "Generated new device ID: %s", s_config.device_id);

        // Player ID defaults to device ID
        strncpy(s_config.player_id, s_config.device_id, sizeof(s_config.player_id) - 1);
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Game state initialized for %s", DEVICE_ROLE_NAMES[role]);

    return true;
}

void game_state_generate_id(char* out_id, size_t max_len)
{
    // Generate 8 random hex characters
    uint32_t rand_val = esp_random();
    snprintf(out_id, max_len, "%08lX", (unsigned long)rand_val);
}

void game_state_generate_name(char* out_name, size_t max_len, DeviceRole role, const char* ip_addr)
{
    if (ip_addr && strlen(ip_addr) > 0)
    {
        snprintf(out_name, max_len, "%s %s", DEVICE_ROLE_NAMES[role], ip_addr);
    }
    else
    {
        snprintf(out_name, max_len, "%s", DEVICE_ROLE_NAMES[role]);
    }
}

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================

const DeviceConfig* game_state_get_config(void)
{
    return &s_config;
}

DeviceConfig* game_state_get_config_mut(void)
{
    return &s_config;
}

bool game_state_update_config(const DeviceConfig* new_config)
{
    if (!new_config)
        return false;

    LOCK();

    // Update individual fields (preserve role)
    DeviceRole saved_role = s_config.role;
    memcpy(&s_config, new_config, sizeof(DeviceConfig));
    s_config.role = saved_role; // Don't allow role change from server

    UNLOCK();

    ESP_LOGI(TAG, "Config updated - ID: %s, Name: %s, Team: %s", s_config.device_id, s_config.device_name,
             s_config.team);

    return game_state_save_config();
}

bool game_state_save_config(void)
{
    LOCK();

    bool success = true;
    success &= nvs_store_write_str(NVS_GAME_NS, NVS_KEY_DEVICE_ID, s_config.device_id);
    success &= nvs_store_write_str(NVS_GAME_NS, NVS_KEY_DEVICE_NAME, s_config.device_name);
    success &= nvs_store_write_str(NVS_GAME_NS, NVS_KEY_PLAYER_ID, s_config.player_id);
    success &= nvs_store_write_str(NVS_GAME_NS, NVS_KEY_TEAM, s_config.team);
    success &= nvs_store_write_str(NVS_GAME_NS, NVS_KEY_COLOR, s_config.color);
    success &= nvs_store_write_str(NVS_GAME_NS, NVS_KEY_TEAMMATES, s_config.teammates);
    success &= nvs_store_write_str(NVS_GAME_NS, NVS_KEY_ENEMIES, s_config.enemies);

    UNLOCK();

    if (success)
    {
        ESP_LOGI(TAG, "Config saved to NVS");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to save config to NVS");
    }

    return success;
}

bool game_state_load_config(void)
{
    LOCK();

    bool found = false;
    char buffer[256];

    if (nvs_store_read_str(NVS_GAME_NS, NVS_KEY_DEVICE_ID, buffer, sizeof(buffer)))
    {
        strncpy(s_config.device_id, buffer, sizeof(s_config.device_id) - 1);
        found = true;
    }

    if (nvs_store_read_str(NVS_GAME_NS, NVS_KEY_DEVICE_NAME, buffer, sizeof(buffer)))
    {
        strncpy(s_config.device_name, buffer, sizeof(s_config.device_name) - 1);
    }

    if (nvs_store_read_str(NVS_GAME_NS, NVS_KEY_PLAYER_ID, buffer, sizeof(buffer)))
    {
        strncpy(s_config.player_id, buffer, sizeof(s_config.player_id) - 1);
    }

    if (nvs_store_read_str(NVS_GAME_NS, NVS_KEY_TEAM, buffer, sizeof(buffer)))
    {
        strncpy(s_config.team, buffer, sizeof(s_config.team) - 1);
    }

    if (nvs_store_read_str(NVS_GAME_NS, NVS_KEY_COLOR, buffer, sizeof(buffer)))
    {
        strncpy(s_config.color, buffer, sizeof(s_config.color) - 1);
    }

    if (nvs_store_read_str(NVS_GAME_NS, NVS_KEY_TEAMMATES, buffer, sizeof(buffer)))
    {
        strncpy(s_config.teammates, buffer, sizeof(s_config.teammates) - 1);
    }

    if (nvs_store_read_str(NVS_GAME_NS, NVS_KEY_ENEMIES, buffer, sizeof(buffer)))
    {
        strncpy(s_config.enemies, buffer, sizeof(s_config.enemies) - 1);
    }

    UNLOCK();

    if (found)
    {
        ESP_LOGI(TAG, "Config loaded from NVS - ID: %s", s_config.device_id);
    }

    return found;
}

void game_state_reset_config(void)
{
    LOCK();

    DeviceRole role = s_config.role;
    memset(&s_config, 0, sizeof(s_config));
    s_config.role = role;
    s_config.respawn_time_ms = DEFAULT_RESPAWN_TIME_MS;
    snprintf(s_config.color, sizeof(s_config.color), "#FF0000");

    // Generate new ID
    game_state_generate_id(s_config.device_id, sizeof(s_config.device_id));
    strncpy(s_config.player_id, s_config.device_id, sizeof(s_config.player_id) - 1);

    UNLOCK();

    nvs_store_erase_namespace(NVS_GAME_NS);
    ESP_LOGI(TAG, "Config reset to defaults");
}

// ============================================================================
// GAME STATE MANAGEMENT
// ============================================================================

const GameStateData* game_state_get(void)
{
    return &s_game_state;
}

GameStateData* game_state_get_mut(void)
{
    return &s_game_state;
}

void game_state_set_mode(GameMode mode)
{
    LOCK();
    s_game_state.mode = mode;
    UNLOCK();
    ESP_LOGI(TAG, "Game mode set to: %s", GAMEMODE_NAMES[mode]);
}

void game_state_set_state(GameState state)
{
    LOCK();
    s_game_state.state = state;
    UNLOCK();
    ESP_LOGI(TAG, "Game state set to: %s", GAME_STATE_NAMES[state]);
}

void game_state_start_respawn(void)
{
    LOCK();
    s_game_state.state = GAME_STATE_RESPAWNING;
    s_game_state.respawn_end_time = (uint32_t)(esp_timer_get_time() / 1000) + s_config.respawn_time_ms;
    UNLOCK();
    ESP_LOGI(TAG, "Respawn started, ends in %lu ms", (unsigned long)s_config.respawn_time_ms);
}

bool game_state_is_respawning(void)
{
    return s_game_state.state == GAME_STATE_RESPAWNING;
}

bool game_state_check_respawn(void)
{
    if (s_game_state.state != GAME_STATE_RESPAWNING)
    {
        return false;
    }

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (now >= s_game_state.respawn_end_time)
    {
        LOCK();
        s_game_state.state = GAME_STATE_PLAYING;
        s_game_state.health = s_game_state.max_health;
        UNLOCK();
        ESP_LOGI(TAG, "Respawn complete");
        return true;
    }

    return false;
}

// ============================================================================
// STATS & EVENTS
// ============================================================================

void game_state_record_shot(void)
{
    LOCK();
    s_game_state.shots_fired++;
    UNLOCK();
}

void game_state_record_hit(void)
{
    LOCK();
    s_game_state.hits_landed++;
    UNLOCK();
}

void game_state_record_kill(void)
{
    LOCK();
    s_game_state.kills++;
    UNLOCK();
    ESP_LOGI(TAG, "Kill recorded! Total: %lu", (unsigned long)s_game_state.kills);
}

void game_state_record_death(void)
{
    LOCK();
    s_game_state.deaths++;
    UNLOCK();
    ESP_LOGI(TAG, "Death recorded! Total: %lu", (unsigned long)s_game_state.deaths);
    game_state_start_respawn();
}

void game_state_record_friendly_fire(void)
{
    LOCK();
    s_game_state.friendly_fire_count++;
    UNLOCK();
}

void game_state_reset_stats(void)
{
    LOCK();
    s_game_state.kills = 0;
    s_game_state.deaths = 0;
    s_game_state.shots_fired = 0;
    s_game_state.hits_landed = 0;
    s_game_state.friendly_fire_count = 0;
    s_game_state.health = s_game_state.max_health;
    UNLOCK();
    ESP_LOGI(TAG, "Stats reset");
}

// ============================================================================
// PLAYER RELATIONSHIP CHECKS
// ============================================================================

// Helper to check if ID exists in comma-separated list
static bool id_in_list(const char* list, const char* id)
{
    if (!list || !id || strlen(list) == 0 || strlen(id) == 0)
    {
        return false;
    }

    char list_copy[512];
    strncpy(list_copy, list, sizeof(list_copy) - 1);
    list_copy[sizeof(list_copy) - 1] = '\0';

    char* token = strtok(list_copy, ",");
    while (token)
    {
        // Trim whitespace
        while (*token == ' ')
            token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ')
            *end-- = '\0';

        if (strcmp(token, id) == 0)
        {
            return true;
        }
        token = strtok(NULL, ",");
    }

    return false;
}

bool game_state_is_teammate(const char* player_id)
{
    if (!player_id)
        return false;

    // Same player is always "teammate"
    if (strcmp(player_id, s_config.player_id) == 0)
    {
        return true;
    }

    return id_in_list(s_config.teammates, player_id);
}

bool game_state_is_enemy(const char* player_id)
{
    if (!player_id)
        return false;

    // Can't be enemy of self
    if (strcmp(player_id, s_config.player_id) == 0)
    {
        return false;
    }

    // If teammates list is empty, check enemies list
    if (strlen(s_config.teammates) == 0)
    {
        // If enemies list is empty, everyone is enemy
        if (strlen(s_config.enemies) == 0)
        {
            return true;
        }
        return id_in_list(s_config.enemies, player_id);
    }

    // If not a teammate, they're an enemy
    return !game_state_is_teammate(player_id);
}

bool game_state_friendly_fire_counts(void)
{
    // Friendly fire handling depends on gamemode
    switch (s_game_state.mode)
    {
        case GAMEMODE_FREE:
            return true; // Count but no penalty
        case GAMEMODE_DEATHMATCH:
            return false; // No teams, everyone is enemy
        case GAMEMODE_TEAM:
            return false; // Friendly fire ignored
        case GAMEMODE_CAPTURE_FLAG:
            return false; // Friendly fire ignored
        case GAMEMODE_TIMED:
            return true; // Count
        default:
            return s_config.friendly_fire_enabled;
    }
}

// ============================================================================
// CONNECTION STATE
// ============================================================================

void game_state_set_connected(bool connected)
{
    LOCK();
    s_game_state.server_connected = connected;
    UNLOCK();
    ESP_LOGI(TAG, "Server connection: %s", connected ? "CONNECTED" : "DISCONNECTED");
}

void game_state_update_heartbeat(void)
{
    LOCK();
    s_game_state.last_heartbeat = (uint32_t)(esp_timer_get_time() / 1000);
    UNLOCK();
}

bool game_state_heartbeat_due(void)
{
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t interval = HEARTBEAT_INTERVAL_FREE; // Default 1 minute

    return (now - s_game_state.last_heartbeat) >= interval;
}

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

int game_state_config_to_json(char* buffer, size_t max_len)
{
    LOCK();
    int len =
        snprintf(buffer, max_len,
                 "{"
                 "\"%s\":\"%s\","
                 "\"%s\":\"%s\","
                 "\"%s\":\"%s\","
                 "\"%s\":\"%s\","
                 "\"%s\":\"%s\","
                 "\"%s\":\"%s\","
                 "\"%s\":[%s],"
                 "\"%s\":[%s],"
                 "\"%s\":%lu"
                 "}",
                 JSON_KEY_DEVICE_ID, s_config.device_id, JSON_KEY_DEVICE_NAME, s_config.device_name, JSON_KEY_PLAYER_ID,
                 s_config.player_id, JSON_KEY_ROLE, DEVICE_ROLE_NAMES[s_config.role], JSON_KEY_TEAM, s_config.team,
                 JSON_KEY_COLOR, s_config.color, JSON_KEY_TEAMMATES, s_config.teammates, JSON_KEY_ENEMIES,
                 s_config.enemies, JSON_KEY_RESPAWN_TIME, (unsigned long)s_config.respawn_time_ms);
    UNLOCK();

    return len < (int)max_len ? len : -1;
}

int game_state_to_json(char* buffer, size_t max_len)
{
    LOCK();
    int len = snprintf(
        buffer, max_len,
        "{"
        "\"%s\":\"%s\","
        "\"%s\":\"%s\","
        "\"%s\":%lu,"
        "\"%s\":%lu,"
        "\"%s\":%lu,"
        "\"%s\":%lu,"
        "\"%s\":%u,"
        "\"%s\":%s"
        "}",
        JSON_KEY_GAMEMODE, GAMEMODE_NAMES[s_game_state.mode], JSON_KEY_GAME_STATE, GAME_STATE_NAMES[s_game_state.state],
        JSON_KEY_KILLS, (unsigned long)s_game_state.kills, JSON_KEY_DEATHS, (unsigned long)s_game_state.deaths,
        JSON_KEY_SHOTS, (unsigned long)s_game_state.shots_fired, JSON_KEY_HITS, (unsigned long)s_game_state.hits_landed,
        JSON_KEY_HEALTH, s_game_state.health, "server_connected", s_game_state.server_connected ? "true" : "false");
    UNLOCK();

    return len < (int)max_len ? len : -1;
}

int game_state_create_heartbeat_json(char* buffer, size_t max_len)
{
    uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000000); // seconds

    LOCK();
    int len = snprintf(buffer, max_len,
                       "{"
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":%lu,"
                       "\"%s\":%lu,"
                       "\"%s\":%lu,"
                       "\"%s\":%lu,"
                       "\"%s\":%u,"
                       "\"%s\":%lu"
                       "}",
                       JSON_KEY_TYPE, CLIENT_MSG_NAMES[MSG_HEARTBEAT], JSON_KEY_DEVICE_ID, s_config.device_id,
                       JSON_KEY_GAMEMODE, GAMEMODE_NAMES[s_game_state.mode], JSON_KEY_GAME_STATE,
                       GAME_STATE_NAMES[s_game_state.state], JSON_KEY_KILLS, (unsigned long)s_game_state.kills,
                       JSON_KEY_DEATHS, (unsigned long)s_game_state.deaths, JSON_KEY_SHOTS,
                       (unsigned long)s_game_state.shots_fired, JSON_KEY_HITS, (unsigned long)s_game_state.hits_landed,
                       JSON_KEY_HEALTH, s_game_state.health, JSON_KEY_UPTIME, (unsigned long)uptime);
    UNLOCK();

    return len < (int)max_len ? len : -1;
}

int game_state_create_register_json(char* buffer, size_t max_len)
{
    const char* ip_str = wifi_manager_get_ip();

    LOCK();
    int len = snprintf(buffer, max_len,
                       "{"
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\""
                       "}",
                       JSON_KEY_TYPE, CLIENT_MSG_NAMES[MSG_REGISTER], JSON_KEY_DEVICE_ID, s_config.device_id,
                       JSON_KEY_DEVICE_NAME, s_config.device_name, JSON_KEY_PLAYER_ID, s_config.player_id,
                       JSON_KEY_ROLE, DEVICE_ROLE_NAMES[s_config.role], JSON_KEY_TEAM, s_config.team, JSON_KEY_COLOR,
                       s_config.color, JSON_KEY_IP, ip_str);
    UNLOCK();

    return len < (int)max_len ? len : -1;
}

int game_state_create_hit_report_json(char* buffer, size_t max_len, const char* shooter_id)
{
    LOCK();
    int len = snprintf(buffer, max_len,
                       "{"
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":%lu"
                       "}",
                       JSON_KEY_TYPE, CLIENT_MSG_NAMES[MSG_HIT_REPORT], JSON_KEY_TARGET_ID, s_config.device_id,
                       JSON_KEY_SHOOTER_ID, shooter_id ? shooter_id : "unknown", JSON_KEY_TIMESTAMP,
                       (unsigned long)(esp_timer_get_time() / 1000));
    UNLOCK();

    return len < (int)max_len ? len : -1;
}

int game_state_create_shot_fired_json(char* buffer, size_t max_len)
{
    LOCK();
    int len = snprintf(buffer, max_len,
                       "{"
                       "\"%s\":\"%s\","
                       "\"%s\":\"%s\","
                       "\"%s\":%lu,"
                       "\"%s\":%lu"
                       "}",
                       JSON_KEY_TYPE, CLIENT_MSG_NAMES[MSG_SHOT_FIRED], JSON_KEY_DEVICE_ID, s_config.device_id,
                       JSON_KEY_SHOTS, (unsigned long)s_game_state.shots_fired, JSON_KEY_TIMESTAMP,
                       (unsigned long)(esp_timer_get_time() / 1000));
    UNLOCK();

    return len < (int)max_len ? len : -1;
}

bool game_state_config_from_json(const char* json, DeviceConfig* out_config)
{
    // Simple JSON parsing for config updates
    // In production, use a proper JSON library like cJSON

    if (!json || !out_config)
        return false;

    // For now, just log that we received config
    // TODO: Implement proper JSON parsing with cJSON
    ESP_LOGI(TAG, "Received config JSON: %s", json);

    return true;
}

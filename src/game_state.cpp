#include "game_state.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_store.h"

static const char* TAG = "game_state";

static DeviceConfig s_config;
static GameConfig s_game_cfg;
static GameStateData s_state;
static SemaphoreHandle_t s_mutex = NULL;
static bool s_initialized = false;

#define NVS_GAME_NS "game"
#define NVS_KEY_DEVICE_ID "device_id_u8"
#define NVS_KEY_PLAYER_ID "player_id_u8"
#define NVS_KEY_TEAM_ID "team_id_u8"
#define NVS_KEY_COLOR "color_u32"

#define LOCK()                                                                                                         \
    if (s_mutex)                                                                                                       \
    xSemaphoreTake(s_mutex, portMAX_DELAY)
#define UNLOCK()                                                                                                       \
    if (s_mutex)                                                                                                       \
    xSemaphoreGive(s_mutex)

static uint8_t rand_u8()
{
    return (uint8_t)(esp_random() & 0xFF);
}

bool game_state_init(DeviceRole role)
{
    if (s_initialized)
        return true;

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex)
    {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }

    memset(&s_config, 0, sizeof(s_config));
    memset(&s_game_cfg, 0, sizeof(s_game_cfg));
    memset(&s_state, 0, sizeof(s_state));

    s_config.role = role;
    game_state_load_default_game_config();
    game_state_generate_ids();
    game_state_load_ids();
    game_state_reset_runtime();

    s_initialized = true;
    ESP_LOGI(TAG, "Game state initialized role=%d device=%u player=%u", role, s_config.device_id, s_config.player_id);
    return true;
}

// ID management

const DeviceConfig* game_state_get_config(void)
{
    return &s_config;
}

DeviceConfig* game_state_get_config_mut(void)
{
    return &s_config;
}

bool game_state_load_ids(void)
{
    LOCK();
    bool loaded = false;
    uint8_t id = 0;
    if (nvs_store_read_u8(NVS_GAME_NS, NVS_KEY_DEVICE_ID, &id))
    {
        s_config.device_id = id;
        loaded = true;
    }
    if (nvs_store_read_u8(NVS_GAME_NS, NVS_KEY_PLAYER_ID, &id))
    {
        s_config.player_id = id;
    }
    if (nvs_store_read_u8(NVS_GAME_NS, NVS_KEY_TEAM_ID, &id))
    {
        s_config.team_id = id;
    }
    uint32_t color = 0;
    if (nvs_store_read_u32(NVS_GAME_NS, NVS_KEY_COLOR, &color))
    {
        s_config.color_rgb = color;
    }
    
    // Load device name
    char name_buf[32] = {0};
    if (nvs_store_read_str(NVS_GAME_NS, "device_name", name_buf, sizeof(name_buf)))
    {
        strncpy(s_config.device_name, name_buf, sizeof(s_config.device_name) - 1);
        s_config.device_name[sizeof(s_config.device_name) - 1] = '\0';
    }
    
    UNLOCK();
    return loaded;
}

bool game_state_save_ids(void)
{
    LOCK();
    bool ok = true;
    ok &= nvs_store_write_u8(NVS_GAME_NS, NVS_KEY_DEVICE_ID, s_config.device_id);
    ok &= nvs_store_write_u8(NVS_GAME_NS, NVS_KEY_PLAYER_ID, s_config.player_id);
    ok &= nvs_store_write_u8(NVS_GAME_NS, NVS_KEY_TEAM_ID, s_config.team_id);
    ok &= nvs_store_write_u32(NVS_GAME_NS, NVS_KEY_COLOR, s_config.color_rgb);
    
    // Save device name
    if (strlen(s_config.device_name) > 0)
    {
        ok &= nvs_store_write_str(NVS_GAME_NS, "device_name", s_config.device_name);
    }
    
    UNLOCK();
    return ok;
}

void game_state_generate_ids(void)
{
    LOCK();
    if (s_config.device_id == 0)
        s_config.device_id = rand_u8();
    if (s_config.player_id == 0)
        s_config.player_id = s_config.device_id;
    UNLOCK();
}

void game_state_reset_runtime(void)
{
    LOCK();
    memset(&s_state, 0, sizeof(s_state));
    s_state.hearts_remaining = s_game_cfg.max_hearts;
    UNLOCK();
}

// Game config

const GameConfig* game_state_get_game_config(void)
{
    return &s_game_cfg;
}

GameConfig* game_state_get_game_config_mut(void)
{
    return &s_game_cfg;
}

static uint32_t clamp_u32(uint32_t v, uint32_t lo, uint32_t hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

void game_state_apply_game_config(const GameConfig* cfg, bool* clamped)
{
    if (!cfg)
        return;
    bool local_clamped = false;
    GameConfig nc = *cfg;

    auto clamp8 = [&](uint8_t v, uint8_t lo, uint8_t hi) {
        if (v < lo)
        {
            local_clamped = true;
            return lo;
        }
        if (v > hi)
        {
            local_clamped = true;
            return hi;
        }
        return v;
    };
    auto clamp16 = [&](uint16_t v, uint16_t lo, uint16_t hi) {
        if (v < lo)
        {
            local_clamped = true;
            return lo;
        }
        if (v > hi)
        {
            local_clamped = true;
            return hi;
        }
        return v;
    };

    nc.max_hearts = clamp8(nc.max_hearts, 1, 99);
    nc.time_limit_s = clamp16(nc.time_limit_s, 0, 7200);
    nc.score_to_win = clamp16(nc.score_to_win, 0, 65535);
    nc.respawn_cooldown_ms = clamp_u32(nc.respawn_cooldown_ms, 0, 30000);
    nc.invulnerability_ms = clamp16(nc.invulnerability_ms, 0, 30000);
    nc.mag_capacity = clamp8(nc.mag_capacity, 0, 255);
    nc.max_ammo = clamp16(nc.max_ammo, 0, 65535);
    nc.reload_time_ms = clamp16(nc.reload_time_ms, 0, 30000);
    nc.shot_rate_limit_ms = clamp16(nc.shot_rate_limit_ms, 50, 2000);

    LOCK();
    s_game_cfg = nc;
    UNLOCK();
    if (clamped)
        *clamped = local_clamped;
}

void game_state_load_default_game_config(void)
{
    GameConfig cfg = {};
    cfg.max_hearts = 5;
    cfg.respawn_cooldown_ms = 5000;
    cfg.invulnerability_ms = 500;
    cfg.kill_score = 1;
    cfg.hit_score = 1;
    cfg.assist_score = 0;
    cfg.score_to_win = 0;
    cfg.time_limit_s = 0;
    cfg.overtime_enabled = false;
    cfg.sudden_death = false;
    cfg.max_ammo = 0;
    cfg.mag_capacity = 0;
    cfg.reload_time_ms = 0;
    cfg.shot_rate_limit_ms = 100;
    cfg.team_play = false;
    cfg.friendly_fire_enabled = false;
    cfg.unlimited_ammo = true;
    cfg.unlimited_respawn = true;
    cfg.random_teams_on_start = false;
    cfg.hit_sound_enabled = true;

    bool cl = false;
    game_state_apply_game_config(&cfg, &cl);
}

// Runtime state

const GameStateData* game_state_get(void)
{
    return &s_state;
}

GameStateData* game_state_get_mut(void)
{
    return &s_state;
}

void game_state_record_shot(void)
{
    LOCK();
    s_state.shots_fired++;
    UNLOCK();
}

void game_state_record_hit(void)
{
    LOCK();
    s_state.hits_landed++;
    UNLOCK();
}

void game_state_record_kill(void)
{
    LOCK();
    s_state.kills++;
    UNLOCK();
}

void game_state_record_death(void)
{
    LOCK();
    s_state.deaths++;
    if (s_state.hearts_remaining > 0)
        s_state.hearts_remaining--;
    s_state.respawning = true;
    s_state.respawn_end_time_ms = (uint32_t)(esp_timer_get_time() / 1000) + s_game_cfg.respawn_cooldown_ms;
    UNLOCK();
}

void game_state_record_friendly_fire(void)
{
    LOCK();
    s_state.friendly_fire_count++;
    UNLOCK();
}

void game_state_reset_stats(void)
{
    LOCK();
    s_state.kills = 0;
    s_state.deaths = 0;
    s_state.shots_fired = 0;
    s_state.hits_landed = 0;
    s_state.friendly_fire_count = 0;
    s_state.hearts_remaining = s_game_cfg.max_hearts;
    s_state.rx_count = 0;
    s_state.tx_count = 0;
    s_state.last_rx_ms = 0;
    UNLOCK();
}

uint8_t game_state_get_player_id(void)
{
    return s_config.player_id;
}

uint32_t game_state_last_rx_ms_ago(void)
{
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (s_state.last_rx_ms == 0)
        return 0;
    return now > s_state.last_rx_ms ? now - s_state.last_rx_ms : 0;
}

uint32_t game_state_rx_count(void)
{
    return s_state.rx_count;
}

uint32_t game_state_tx_count(void)
{
    return s_state.tx_count;
}

int game_state_get_ammo(void)
{
    return 0;
}

bool game_state_check_respawn(void)
{
    if (!s_state.respawning)
        return false;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (now >= s_state.respawn_end_time_ms)
    {
        LOCK();
        s_state.respawning = false;
        s_state.hearts_remaining = s_game_cfg.max_hearts;
        UNLOCK();
        return true;
    }
    return false;
}

bool game_state_is_respawning(void)
{
    return s_state.respawning;
}

void game_state_start_respawn(void)
{
    LOCK();
    s_state.respawning = true;
    s_state.respawn_end_time_ms = (uint32_t)(esp_timer_get_time() / 1000) + s_game_cfg.respawn_cooldown_ms;
    UNLOCK();
}

bool game_state_friendly_fire_counts(void)
{
    return s_game_cfg.friendly_fire_enabled;
}

// Connection

void game_state_set_connected(bool connected)
{
    LOCK();
    s_state.server_connected = connected;
    UNLOCK();
}

void game_state_update_heartbeat(void)
{
    LOCK();
    s_state.last_heartbeat_ms = (uint32_t)(esp_timer_get_time() / 1000);
    UNLOCK();
}

bool game_state_heartbeat_due(void)
{
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    return (now - s_state.last_heartbeat_ms) >= 60000;
}

// JSON stubs (to be implemented with actual JSON lib)

int game_state_config_to_json(char* buffer, size_t max_len, bool clamp_noted)
{
    if (!buffer || max_len == 0)
        return -1;
    int len = snprintf(buffer, max_len,
                       "{"
                       "\"device_id\":%u,\"player_id\":%u,\"team_id\":%u,\"color_rgb\":%u,"
                       "\"clamped\":%s"
                       "}",
                       s_config.device_id, s_config.player_id, s_config.team_id, (unsigned)s_config.color_rgb,
                       clamp_noted ? "true" : "false");
    return len < (int)max_len ? len : -1;
}

int game_state_to_json(char* buffer, size_t max_len)
{
    if (!buffer || max_len == 0)
        return -1;
    uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000);
    int len = snprintf(buffer, max_len,
                       "{"
                       "\"shots\":%lu,\"hits\":%lu,\"kills\":%lu,\"deaths\":%lu,\"hearts\":%u,"
                       "\"respawning\":%s,"
                       "\"server_connected\":%s,\"uptime\":%lu"
                       "}",
                       (unsigned long)s_state.shots_fired, (unsigned long)s_state.hits_landed,
                       (unsigned long)s_state.kills, (unsigned long)s_state.deaths, (unsigned)s_state.hearts_remaining,
                       s_state.respawning ? "true" : "false", s_state.server_connected ? "true" : "false",
                       (unsigned long)uptime);
    return len < (int)max_len ? len : -1;
}

bool game_state_config_from_json(const char* json, GameConfig* out_config, bool* clamped)
{
    if (!json || !out_config)
        return false;
    *out_config = s_game_cfg;
    if (clamped)
        *clamped = false;
    return true;
}

int game_state_create_heartbeat_json(char* buffer, size_t max_len)
{
    if (!buffer || max_len == 0)
        return -1;
    return game_state_to_json(buffer, max_len);
}

int game_state_create_register_json(char* buffer, size_t max_len)
{
    if (!buffer || max_len == 0)
        return -1;
    return game_state_config_to_json(buffer, max_len, false);
}

int game_state_create_hit_report_json(char* buffer, size_t max_len, uint8_t shooter_id)
{
    if (!buffer || max_len == 0)
        return -1;
    int len = snprintf(buffer, max_len, "{\"shooter_id\":%u,\"ts\":%lu}", shooter_id,
                       (unsigned long)(esp_timer_get_time() / 1000));
    return len < (int)max_len ? len : -1;
}

int game_state_create_shot_fired_json(char* buffer, size_t max_len)
{
    if (!buffer || max_len == 0)
        return -1;
    int len = snprintf(buffer, max_len, "{\"shots\":%lu,\"ts\":%lu}", (unsigned long)s_state.shots_fired,
                       (unsigned long)(esp_timer_get_time() / 1000));
    return len < (int)max_len ? len : -1;
}

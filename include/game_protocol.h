#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        DEVICE_ROLE_WEAPON = 0,
        DEVICE_ROLE_TARGET,
        DEVICE_ROLE_COUNT
    } DeviceRole;

    // WebSocket Protocol v2.2 OpCodes
    typedef enum
    {
        // Client -> ESP32
        OP_GET_STATUS = 1,
        OP_HEARTBEAT = 2,
        OP_CONFIG_UPDATE = 3,
        OP_GAME_COMMAND = 4,
        OP_HIT_FORWARD = 5,
        OP_KILL_CONFIRMED = 6,
        OP_REMOTE_SOUND = 7,

        // ESP32 -> Client
        OP_STATUS = 10,
        OP_HEARTBEAT_ACK = 11,
        OP_SHOT_FIRED = 12,
        OP_HIT_REPORT = 13,
        OP_RESPAWN = 14,
        OP_RELOAD_EVENT = 15,
        OP_GAME_OVER = 16,
        OP_ACK = 20
    } OpCode;

    typedef enum
    {
        CMD_STOP = 0,
        CMD_START = 1,
        CMD_RESET = 2,
        CMD_PAUSE = 3,
        CMD_UNPAUSE = 4
    } GameCommandType;

    typedef struct
    {
        uint8_t device_id;  // unique per device
        uint8_t player_id;  // unique per player (may equal device_id)
        uint8_t team_id;    // 0 = no team
        uint32_t color_rgb; // 0xRRGGBB
        DeviceRole role;
        char device_name[32]; // Device display name (e.g., "Player 1 - Target")
    } DeviceConfig;

    typedef struct
    {
        uint8_t max_hearts;
        uint32_t respawn_cooldown_ms;
        uint16_t invulnerability_ms;

        uint8_t kill_score;
        uint8_t hit_score;
        uint8_t assist_score;
        uint16_t score_to_win;

        uint16_t time_limit_s;
        bool overtime_enabled;
        bool sudden_death;

        uint16_t max_ammo;
        uint8_t mag_capacity;
        uint16_t reload_time_ms;
        uint16_t shot_rate_limit_ms;

        bool team_play;
        bool friendly_fire_enabled;
        bool unlimited_ammo;
        bool unlimited_respawn;

        bool random_teams_on_start;
        bool hit_sound_enabled;
    } GameConfig;

    typedef struct
    {
        uint32_t shots_fired;
        uint32_t hits_landed;
        uint32_t kills;
        uint32_t deaths;
        uint32_t friendly_fire_count;
        uint32_t rx_count;
        uint32_t tx_count;
        uint32_t last_rx_ms;

        uint8_t hearts_remaining;
        bool respawning;
        uint32_t respawn_end_time_ms;

        uint32_t game_start_time_ms;
        uint32_t last_heartbeat_ms;
        bool server_connected;
    } GameStateData;

#ifdef __cplusplus
}
#endif

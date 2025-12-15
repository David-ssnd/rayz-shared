#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "game_protocol.h"


#ifdef __cplusplus
extern "C"
{
#endif

    // ============================================================================
    // INITIALIZATION
    // ============================================================================

    bool game_state_init(DeviceRole role);

    const DeviceConfig* game_state_get_config(void);
    DeviceConfig* game_state_get_config_mut(void);
    bool game_state_load_ids(void);
    bool game_state_save_ids(void);
    void game_state_generate_ids(void);

    void game_state_reset_runtime(void);

    const GameConfig* game_state_get_game_config(void);
    GameConfig* game_state_get_game_config_mut(void);
    void game_state_apply_game_config(const GameConfig* cfg, bool* clamped);
    void game_state_load_default_game_config(void);

    const GameStateData* game_state_get(void);
    GameStateData* game_state_get_mut(void);

    void game_state_record_shot(void);
    void game_state_record_hit(void);
    void game_state_record_kill(void);
    void game_state_record_death(void);
    void game_state_record_friendly_fire(void);
    void game_state_reset_stats(void);
    uint8_t game_state_get_player_id(void);
    uint32_t game_state_last_rx_ms_ago(void);
    uint32_t game_state_rx_count(void);
    uint32_t game_state_tx_count(void);
    int game_state_get_ammo(void);
    bool game_state_check_respawn(void);
    bool game_state_is_respawning(void);
    void game_state_start_respawn(void);
    bool game_state_friendly_fire_counts(void);

    void game_state_set_connected(bool connected);
    void game_state_update_heartbeat(void);
    bool game_state_heartbeat_due(void);

    int game_state_config_to_json(char* buffer, size_t max_len, bool clamp_noted);
    int game_state_to_json(char* buffer, size_t max_len);
    bool game_state_config_from_json(const char* json, GameConfig* out_config, bool* clamped);
    int game_state_create_heartbeat_json(char* buffer, size_t max_len);
    int game_state_create_register_json(char* buffer, size_t max_len);
    int game_state_create_hit_report_json(char* buffer, size_t max_len, uint8_t shooter_id);
    int game_state_create_shot_fired_json(char* buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

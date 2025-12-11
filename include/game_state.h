#pragma once

#include "game_protocol.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * @brief Initialize the game state manager
 * @param role Device role (weapon or target)
 * @return true if successful
 */
bool game_state_init(DeviceRole role);

/**
 * @brief Generate a random device ID
 * @param out_id Output buffer for the ID
 * @param max_len Maximum length of output buffer
 */
void game_state_generate_id(char* out_id, size_t max_len);

/**
 * @brief Generate default device name (role + IP)
 * @param out_name Output buffer for the name
 * @param max_len Maximum length of output buffer
 * @param role Device role
 * @param ip_addr IP address string
 */
void game_state_generate_name(char* out_name, size_t max_len, DeviceRole role, const char* ip_addr);

// ============================================================================
// CONFIGURATION MANAGEMENT
// ============================================================================

/**
 * @brief Get current device configuration
 * @return Pointer to device config (read-only)
 */
const DeviceConfig* game_state_get_config(void);

/**
 * @brief Get mutable device configuration for updates
 * @return Pointer to device config
 */
DeviceConfig* game_state_get_config_mut(void);

/**
 * @brief Update device configuration from server
 * @param new_config New configuration to apply
 * @return true if successful
 */
bool game_state_update_config(const DeviceConfig* new_config);

/**
 * @brief Save current config to NVS
 * @return true if successful
 */
bool game_state_save_config(void);

/**
 * @brief Load config from NVS
 * @return true if found and loaded
 */
bool game_state_load_config(void);

/**
 * @brief Reset config to defaults
 */
void game_state_reset_config(void);

// ============================================================================
// GAME STATE MANAGEMENT
// ============================================================================

/**
 * @brief Get current game state data
 * @return Pointer to game state (read-only)
 */
const GameStateData* game_state_get(void);

/**
 * @brief Get mutable game state for updates
 * @return Pointer to game state
 */
GameStateData* game_state_get_mut(void);

/**
 * @brief Set the current game mode
 * @param mode New game mode
 */
void game_state_set_mode(GameMode mode);

/**
 * @brief Set the current game state
 * @param state New game state
 */
void game_state_set_state(GameState state);

/**
 * @brief Start respawn cooldown
 */
void game_state_start_respawn(void);

/**
 * @brief Check if currently respawning
 * @return true if in respawn cooldown
 */
bool game_state_is_respawning(void);

/**
 * @brief Check respawn timer and update state if complete
 * @return true if respawn just completed
 */
bool game_state_check_respawn(void);

// ============================================================================
// STATS & EVENTS
// ============================================================================

/**
 * @brief Record a shot fired
 */
void game_state_record_shot(void);

/**
 * @brief Record a hit landed (confirmed by server)
 */
void game_state_record_hit(void);

/**
 * @brief Record a kill
 */
void game_state_record_kill(void);

/**
 * @brief Record a death and start respawn
 */
void game_state_record_death(void);

/**
 * @brief Record friendly fire incident
 */
void game_state_record_friendly_fire(void);

/**
 * @brief Reset all stats (for new game)
 */
void game_state_reset_stats(void);

// ============================================================================
// PLAYER RELATIONSHIP CHECKS
// ============================================================================

/**
 * @brief Check if a player ID is a teammate
 * @param player_id Player ID to check
 * @return true if teammate
 */
bool game_state_is_teammate(const char* player_id);

/**
 * @brief Check if a player ID is an enemy
 * @param player_id Player ID to check
 * @return true if enemy (or if enemies list is empty = all are enemies)
 */
bool game_state_is_enemy(const char* player_id);

/**
 * @brief Check if friendly fire should be processed based on gamemode
 * @return true if friendly fire should be counted
 */
bool game_state_friendly_fire_counts(void);

// ============================================================================
// CONNECTION STATE
// ============================================================================

/**
 * @brief Set server connection state
 * @param connected true if connected to server
 */
void game_state_set_connected(bool connected);

/**
 * @brief Update last heartbeat timestamp
 */
void game_state_update_heartbeat(void);

/**
 * @brief Check if heartbeat is due
 * @return true if should send heartbeat
 */
bool game_state_heartbeat_due(void);

// ============================================================================
// JSON SERIALIZATION
// ============================================================================

/**
 * @brief Serialize config to JSON string
 * @param buffer Output buffer
 * @param max_len Maximum buffer length
 * @return Number of bytes written, or -1 on error
 */
int game_state_config_to_json(char* buffer, size_t max_len);

/**
 * @brief Serialize game state to JSON string
 * @param buffer Output buffer
 * @param max_len Maximum buffer length
 * @return Number of bytes written, or -1 on error
 */
int game_state_to_json(char* buffer, size_t max_len);

/**
 * @brief Parse config from JSON string
 * @param json JSON string
 * @param out_config Output config structure
 * @return true if successful
 */
bool game_state_config_from_json(const char* json, DeviceConfig* out_config);

/**
 * @brief Create a heartbeat message JSON
 * @param buffer Output buffer
 * @param max_len Maximum buffer length
 * @return Number of bytes written, or -1 on error
 */
int game_state_create_heartbeat_json(char* buffer, size_t max_len);

/**
 * @brief Create a registration message JSON
 * @param buffer Output buffer
 * @param max_len Maximum buffer length
 * @return Number of bytes written, or -1 on error
 */
int game_state_create_register_json(char* buffer, size_t max_len);

/**
 * @brief Create a hit report message JSON
 * @param buffer Output buffer
 * @param max_len Maximum buffer length
 * @param shooter_id ID of the shooter
 * @return Number of bytes written, or -1 on error
 */
int game_state_create_hit_report_json(char* buffer, size_t max_len, const char* shooter_id);

/**
 * @brief Create a shot fired message JSON
 * @param buffer Output buffer
 * @param max_len Maximum buffer length
 * @return Number of bytes written, or -1 on error
 */
int game_state_create_shot_fired_json(char* buffer, size_t max_len);

#ifdef __cplusplus
}
#endif

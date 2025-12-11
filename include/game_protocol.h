#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// GAME MODES
// ============================================================================

typedef enum {
    GAMEMODE_FREE = 0,          // Free play, hits counted, 5s respawn
    GAMEMODE_DEATHMATCH,        // Free-for-all, first to X kills wins
    GAMEMODE_TEAM,              // Team vs team, friendly fire ignored
    GAMEMODE_CAPTURE_FLAG,      // Capture the flag mode
    GAMEMODE_TIMED,             // Timed match, most kills wins
    GAMEMODE_COUNT
} GameMode;

static const char* GAMEMODE_NAMES[] = {
    "free",
    "deathmatch",
    "team",
    "capture_flag",
    "timed"
};

// ============================================================================
// GAME STATES
// ============================================================================

typedef enum {
    GAME_STATE_IDLE = 0,        // Waiting for game to start
    GAME_STATE_COUNTDOWN,       // Game starting countdown
    GAME_STATE_PLAYING,         // Game in progress
    GAME_STATE_RESPAWNING,      // Player is respawning (can't shoot/be hit)
    GAME_STATE_ENDED,           // Game has ended
    GAME_STATE_COUNT
} GameState;

static const char* GAME_STATE_NAMES[] = {
    "idle",
    "countdown",
    "playing",
    "respawning",
    "ended"
};

// ============================================================================
// DEVICE ROLES
// ============================================================================

typedef enum {
    DEVICE_ROLE_WEAPON = 0,
    DEVICE_ROLE_TARGET,
    DEVICE_ROLE_COUNT
} DeviceRole;

static const char* DEVICE_ROLE_NAMES[] = {
    "weapon",
    "target"
};

// ============================================================================
// WEBSOCKET MESSAGE TYPES (Client -> Server)
// ============================================================================

typedef enum {
    // Registration & Heartbeat
    MSG_REGISTER = 0,           // Device registers with server
    MSG_HEARTBEAT,              // Periodic heartbeat
    
    // Game Events
    MSG_HIT_REPORT,             // Target reports being hit
    MSG_SHOT_FIRED,             // Weapon reports shot fired
    MSG_RESPAWN_COMPLETE,       // Player finished respawning
    
    // Requests
    MSG_REQUEST_CONFIG,         // Request current config from server
    MSG_REQUEST_GAME_STATE,     // Request current game state
    
    MSG_CLIENT_COUNT
} ClientMessageType;

static const char* CLIENT_MSG_NAMES[] = {
    "register",
    "heartbeat",
    "hit_report",
    "shot_fired",
    "respawn_complete",
    "request_config",
    "request_game_state"
};

// ============================================================================
// WEBSOCKET MESSAGE TYPES (Server -> Client)
// ============================================================================

typedef enum {
    // Responses
    MSG_REGISTER_ACK = 0,       // Registration acknowledged
    MSG_HEARTBEAT_ACK,          // Heartbeat acknowledged
    MSG_CONFIG_UPDATE,          // Server sends config update
    
    // Game Control
    MSG_GAME_START,             // Game is starting
    MSG_GAME_END,               // Game has ended
    MSG_GAME_MODE_CHANGE,       // Game mode changed
    
    // Hit Confirmation
    MSG_HIT_CONFIRMED,          // Server confirms a hit was valid
    MSG_HIT_INVALID,            // Hit was invalid (friendly fire, etc.)
    MSG_YOU_WERE_HIT,           // Notify weapon that its player was hit
    
    // Status Updates
    MSG_PLAYER_UPDATE,          // Player stats update
    MSG_SCOREBOARD,             // Full scoreboard update
    
    MSG_SERVER_COUNT
} ServerMessageType;

static const char* SERVER_MSG_NAMES[] = {
    "register_ack",
    "heartbeat_ack",
    "config_update",
    "game_start",
    "game_end",
    "game_mode_change",
    "hit_confirmed",
    "hit_invalid",
    "you_were_hit",
    "player_update",
    "scoreboard"
};

// ============================================================================
// CONFIGURATION LIMITS
// ============================================================================

#define MAX_DEVICE_ID_LEN       16
#define MAX_DEVICE_NAME_LEN     32
#define MAX_TEAM_NAME_LEN       16
#define MAX_TEAMMATES           16
#define MAX_ENEMIES             32
#define MAX_COLOR_LEN           8       // "#RRGGBB\0"

// ============================================================================
// GAME CONFIGURATION STRUCTURE
// ============================================================================

typedef struct {
    char device_id[MAX_DEVICE_ID_LEN];      // Unique device ID (random or server-assigned)
    char device_name[MAX_DEVICE_NAME_LEN];  // Display name (default: role + IP)
    char player_id[MAX_DEVICE_ID_LEN];      // Player this device belongs to
    DeviceRole role;                         // Weapon or Target
    
    // Team configuration
    char team[MAX_TEAM_NAME_LEN];           // Team name/ID
    char color[MAX_COLOR_LEN];              // LED color "#RRGGBB"
    
    // Relationship arrays (stored as comma-separated IDs)
    char teammates[MAX_TEAMMATES * MAX_DEVICE_ID_LEN];  // Friendly player IDs
    char enemies[MAX_ENEMIES * MAX_DEVICE_ID_LEN];      // Enemy player IDs (empty = all are enemies)
    
    // Game settings
    uint32_t respawn_time_ms;               // Respawn cooldown (default 5000ms)
    bool friendly_fire_enabled;             // Whether friendly fire counts
} DeviceConfig;

// ============================================================================
// GAME STATE STRUCTURE
// ============================================================================

typedef struct {
    GameMode mode;
    GameState state;
    
    // Player stats
    uint32_t kills;
    uint32_t deaths;
    uint32_t shots_fired;
    uint32_t hits_landed;
    uint32_t friendly_fire_count;
    
    // Timing
    uint32_t game_start_time;               // Timestamp when game started
    uint32_t game_duration_ms;              // Total game duration (for timed modes)
    uint32_t respawn_end_time;              // When respawn cooldown ends
    
    // Health (for future use)
    uint8_t health;
    uint8_t max_health;
    
    // Connection state
    bool server_connected;
    uint32_t last_heartbeat;
} GameStateData;

// ============================================================================
// DEFAULT VALUES
// ============================================================================

#define DEFAULT_RESPAWN_TIME_MS     5000
#define DEFAULT_GAME_DURATION_MS    300000  // 5 minutes
#define DEFAULT_HEALTH              100
#define DEFAULT_HEARTBEAT_INTERVAL  60000   // 1 minute
#define HEARTBEAT_INTERVAL_FREE     60000   // 1 minute in free mode

// ============================================================================
// JSON KEYS (for consistency)
// ============================================================================

#define JSON_KEY_TYPE           "type"
#define JSON_KEY_DEVICE_ID      "device_id"
#define JSON_KEY_DEVICE_NAME    "device_name"
#define JSON_KEY_PLAYER_ID      "player_id"
#define JSON_KEY_ROLE           "role"
#define JSON_KEY_TEAM           "team"
#define JSON_KEY_COLOR          "color"
#define JSON_KEY_TEAMMATES      "teammates"
#define JSON_KEY_ENEMIES        "enemies"
#define JSON_KEY_GAMEMODE       "gamemode"
#define JSON_KEY_GAME_STATE     "game_state"
#define JSON_KEY_KILLS          "kills"
#define JSON_KEY_DEATHS         "deaths"
#define JSON_KEY_SHOTS          "shots"
#define JSON_KEY_HITS           "hits"
#define JSON_KEY_HEALTH         "health"
#define JSON_KEY_RESPAWN_TIME   "respawn_time"
#define JSON_KEY_TIMESTAMP      "timestamp"
#define JSON_KEY_SHOOTER_ID     "shooter_id"
#define JSON_KEY_TARGET_ID      "target_id"
#define JSON_KEY_IP             "ip"
#define JSON_KEY_UPTIME         "uptime"
#define JSON_KEY_SUCCESS        "success"
#define JSON_KEY_MESSAGE        "message"

#ifdef __cplusplus
}
#endif

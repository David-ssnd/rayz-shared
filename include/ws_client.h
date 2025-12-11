#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "game_protocol.h"


#ifdef __cplusplus
extern "C"
{
#endif

    // ============================================================================
    // CALLBACK TYPES
    // ============================================================================

    /**
     * @brief Callback for when connection state changes
     * @param connected true if connected, false if disconnected
     */
    typedef void (*ws_client_connect_cb_t)(bool connected);

    /**
     * @brief Callback for server messages
     * @param type Server message type
     * @param json Full JSON message
     */
    typedef void (*ws_client_message_cb_t)(ServerMessageType type, const char* json);

    /**
     * @brief Callback for hit confirmation
     * @param shooter_id ID of the shooter
     * @param target_id ID of the target
     * @param valid true if hit was valid
     */
    typedef void (*ws_client_hit_cb_t)(const char* shooter_id, const char* target_id, bool valid);

    /**
     * @brief Callback for game state changes from server
     * @param mode New game mode
     * @param state New game state
     */
    typedef void (*ws_client_game_state_cb_t)(GameMode mode, GameState state);

    /**
     * @brief Callback for config updates from server
     * @param json JSON containing config updates
     */
    typedef void (*ws_client_config_cb_t)(const char* json);

    // ============================================================================
    // CLIENT CONFIGURATION
    // ============================================================================

    typedef struct
    {
        const char* server_uri; // WebSocket server URI (e.g., "ws://192.168.1.100:80/ws")
        ws_client_connect_cb_t on_connect;
        ws_client_message_cb_t on_message;
        ws_client_hit_cb_t on_hit;
        ws_client_game_state_cb_t on_game_state;
        ws_client_config_cb_t on_config;
    } WsClientConfig;

    // ============================================================================
    // INITIALIZATION & CONNECTION
    // ============================================================================

    /**
     * @brief Initialize the WebSocket client
     * @param config Client configuration
     * @return true if successful
     */
    bool ws_client_init(const WsClientConfig* config);

    /**
     * @brief Start the WebSocket client (connects to server)
     * @return true if connection started
     */
    bool ws_client_start(void);

    /**
     * @brief Stop the WebSocket client
     */
    void ws_client_stop(void);

    /**
     * @brief Check if connected to server
     * @return true if connected
     */
    bool ws_client_is_connected(void);

    /**
     * @brief Set the server URI (can be changed at runtime)
     * @param uri New server URI
     */
    void ws_client_set_server_uri(const char* uri);

    // ============================================================================
    // MESSAGE SENDING
    // ============================================================================

    /**
     * @brief Send a raw JSON message to server
     * @param json JSON string to send
     * @return true if sent successfully
     */
    bool ws_client_send(const char* json);

    /**
     * @brief Send registration message to server
     * @return true if sent successfully
     */
    bool ws_client_send_register(void);

    /**
     * @brief Send heartbeat message to server
     * @return true if sent successfully
     */
    bool ws_client_send_heartbeat(void);

    /**
     * @brief Send hit report to server (when this device was hit)
     * @param shooter_id ID of the device that shot us
     * @return true if sent successfully
     */
    bool ws_client_send_hit_report(const char* shooter_id);

    /**
     * @brief Send shot fired notification to server
     * @return true if sent successfully
     */
    bool ws_client_send_shot_fired(void);

    /**
     * @brief Send respawn complete notification
     * @return true if sent successfully
     */
    bool ws_client_send_respawn_complete(void);

    // ============================================================================
    // TASK
    // ============================================================================

    /**
     * @brief WebSocket client task function
     * @param params Task parameters (WsClientConfig*)
     */
    void ws_client_task(void* params);

#ifdef __cplusplus
}
#endif

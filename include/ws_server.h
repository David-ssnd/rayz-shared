#pragma once

#include <esp_http_server.h>
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
     * @brief Callback for when a client connects/disconnects
     * @param client_fd Client file descriptor
     * @param connected true if connected, false if disconnected
     */
    typedef void (*ws_server_connect_cb_t)(int client_fd, bool connected);

    /**
     * @brief Callback for incoming messages from browser
     * @param client_fd Client file descriptor
     * @param type Message type parsed from JSON
     * @param json Full JSON message
     */
    typedef void (*ws_server_message_cb_t)(int client_fd, const char* type, const char* json);

    // ============================================================================
    // SERVER CONFIGURATION
    // ============================================================================

    typedef struct
    {
        ws_server_connect_cb_t on_connect;
        ws_server_message_cb_t on_message;
    } WsServerConfig;

    // ============================================================================
    // INITIALIZATION
    // ============================================================================

    /**
     * @brief Initialize WebSocket server callbacks
     * @param config Server configuration with callbacks
     */
    void ws_server_init(const WsServerConfig* config);

    /**
     * @brief Register WebSocket endpoint on HTTP server
     * @param server HTTP server handle
     */
    void ws_server_register(httpd_handle_t server);

    // ============================================================================
    // CONNECTION MANAGEMENT
    // ============================================================================

    /**
     * @brief Check if any client is connected
     * @return true if at least one client connected
     */
    bool ws_server_is_connected(void);

    /**
     * @brief Get number of connected clients
     * @return Number of connected WebSocket clients
     */
    int ws_server_client_count(void);

    /**
     * @brief Cleanup stale clients that haven't sent activity recently
     */
    void ws_server_cleanup_stale(void);

    // ============================================================================
    // MESSAGE SENDING
    // ============================================================================

    /**
     * @brief Send message to a specific client
     * @param client_fd Client file descriptor
     * @param message JSON message string
     * @return true if sent successfully
     */
    bool ws_server_send(int client_fd, const char* message);

    /**
     * @brief Broadcast message to all connected clients
     * @param message JSON message string
     */
    void ws_server_broadcast(const char* message);

    // ============================================================================
    // GAME-SPECIFIC MESSAGES
    // ============================================================================

    /**
     * @brief Send registration/status response to browser
     */
    void ws_server_send_status(void);

    /**
     * @brief Send heartbeat acknowledgment
     * @param client_fd Client to respond to
     */
    void ws_server_send_heartbeat_ack(int client_fd);

    /**
     * @brief Broadcast hit event (when this device is hit)
     * @param shooter_id ID of the device that hit us
     */
    void ws_server_broadcast_hit(const char* shooter_id);

    /**
     * @brief Broadcast shot fired event (weapon only)
     */
    void ws_server_broadcast_shot(void);

    /**
     * @brief Broadcast game state change
     */
    void ws_server_broadcast_game_state(void);

    /**
     * @brief Broadcast respawn event
     */
    void ws_server_broadcast_respawn(void);

#ifdef __cplusplus
}
#endif
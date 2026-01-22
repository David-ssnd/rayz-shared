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
    // CONFIGURATION FLAGS
    // ============================================================================

    /**
     * @brief Enable MessagePack binary protocol instead of JSON
     * Reduces message size by ~60% and parsing CPU by ~70%
     */
#ifndef WS_ENABLE_MSGPACK
#define WS_ENABLE_MSGPACK 1
#endif

    /**
     * @brief Enable async WebSocket frame sending
     * Prevents blocking when clients are slow
     */
#ifndef WS_ENABLE_ASYNC_SEND
#define WS_ENABLE_ASYNC_SEND 1
#endif

    /**
     * @brief Use WebSocket PING/PONG instead of application-level heartbeat
     */
#ifndef WS_USE_NATIVE_PING
#define WS_USE_NATIVE_PING 1
#endif

    /**
     * @brief Disable HTTP API endpoints (WebSocket only mode)
     * Saves ~8KB RAM per device
     */
#ifndef WS_DISABLE_HTTP_API
#define WS_DISABLE_HTTP_API 0
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
     * @param type Message type parsed from data
     * @param data Raw message data (JSON string or MessagePack binary)
     * @param len Length of data
     */
    typedef void (*ws_server_message_cb_t)(int client_fd, const char* type, const uint8_t* data,
                                           size_t len);

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
    void ws_server_init_optimized(const WsServerConfig* config);

    /**
     * @brief Register WebSocket endpoint on HTTP server
     * @param server HTTP server handle
     */
    void ws_server_register_optimized(httpd_handle_t server);

    // ============================================================================
    // CONNECTION MANAGEMENT
    // ============================================================================

    /**
     * @brief Check if any client is connected
     * @return true if at least one client connected
     */
    bool ws_server_is_connected_optimized(void);

    /**
     * @brief Get number of connected clients
     * @return Number of connected WebSocket clients
     */
    int ws_server_client_count_optimized(void);

    /**
     * @brief Cleanup stale clients that haven't sent activity recently
     */
    void ws_server_cleanup_stale_optimized(void);

    /**
     * @brief Send WebSocket PING to all clients
     */
    void ws_server_ping_clients(void);

    // ============================================================================
    // MESSAGE SENDING (OPTIMIZED)
    // ============================================================================

    /**
     * @brief Send message to a specific client (async or sync based on config)
     * @param client_fd Client file descriptor
     * @param data Message data (JSON or MessagePack)
     * @param len Length of data
     * @param binary true for binary (MessagePack), false for text (JSON)
     * @return true if sent successfully
     */
    bool ws_server_send_raw_optimized(int client_fd, const uint8_t* data, size_t len, bool binary);

    /**
     * @brief Broadcast message to all connected clients
     * @param data Message data (JSON or MessagePack)
     * @param len Length of data
     * @param binary true for binary (MessagePack), false for text (JSON)
     */
    void ws_server_broadcast_raw_optimized(const uint8_t* data, size_t len, bool binary);

    // ============================================================================
    // CONVENIENCE FUNCTIONS (AUTO-DETECT FORMAT)
    // ============================================================================

    /**
     * @brief Send JSON or MessagePack message to client (auto-detects format)
     * @param client_fd Client file descriptor
     * @param json_or_msgpack Message data
     * @param len Length of data (0 = auto-calculate for JSON)
     * @return true if sent successfully
     */
    bool ws_server_send_auto(int client_fd, const void* json_or_msgpack, size_t len);

    /**
     * @brief Broadcast JSON or MessagePack message to all clients
     * @param json_or_msgpack Message data
     * @param len Length of data (0 = auto-calculate for JSON)
     */
    void ws_server_broadcast_auto(const void* json_or_msgpack, size_t len);

#ifdef __cplusplus
}
#endif

#pragma once
#include <esp_http_server.h>

// Start websocket handler (register URI /ws). Requires existing server handle.
void ws_server_register(httpd_handle_t server);

// Broadcast textual message to all connected websocket clients.
void ws_server_broadcast(const char* msg);

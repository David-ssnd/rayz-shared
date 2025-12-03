#pragma once
#include <esp_http_server.h>
#include <stdint.h>

// Initialize REST endpoints after WiFi connected
httpd_handle_t http_api_start(httpd_handle_t server);

// Provide status JSON (battery, role, ip etc.)
// Implementation will build JSON string into a static buffer
const char* http_api_get_status_json();

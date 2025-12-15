#pragma once

// Internal WiFi/HTTP server state and helpers (component-private)
// Do not include from application code; use wifi_manager.h instead.

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <esp_http_server.h>
#include <stdint.h>
#include "wifi_manager.h"

// NVS namespace + keys
#define NVS_NS_WIFI "wifi"
#define NVS_KEY_SSID "ssid"
#define NVS_KEY_PASS "pass"
#define NVS_KEY_NAME "name"
#define NVS_KEY_ROLE "role"
#define NVS_KEY_PEERS "peers"

// Shared state
extern EventGroupHandle_t g_wifi_events;
extern wifi_boot_mode_t g_wifi_boot_mode;
extern char g_wifi_ip[16];
extern httpd_handle_t g_httpd;
extern char g_device_name[32];
extern char g_role[12];
extern uint8_t g_wifi_channel;
extern char g_peer_list[256];

#ifndef WIFI_COUNTRY_CODE
#define WIFI_COUNTRY_CODE "SK"
#endif

// Internal functions split across units
void wifi_start_http_server(bool provisioning_mode);
void wifi_evaluate_boot_mode();
void wifi_start_ap();
void wifi_start_sta(const char* ssid, const char* pass);

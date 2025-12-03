/**
 * WiFi provisioning and runtime manager for RayZ devices.
 *
 * Responsibilities:
 *  - Initialize NVS (if not already)
 *  - Determine if provisioning data (SSID/PASS) exists
 *  - Start AP + minimal HTTP config page if not provisioned
 *  - Accept POST form to store credentials and reboot into STA
 *  - Connect in STA mode and start REST + WebSocket services
 *  - Expose event group bits for other tasks to wait on (e.g., BLE start after WiFi)
 *  - Provide factory reset (erase NVS keys and restart provisioning)
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <stdbool.h>
#include <stdint.h>

// Event group bits
#define WIFI_EVENT_PROVISIONED_BIT (1 << 0)
#define WIFI_EVENT_STA_CONNECTED_BIT (1 << 1)

// Maximum sizes for stored credentials
#define WIFI_MAX_SSID_LEN 32
#define WIFI_MAX_PASS_LEN 64

    typedef enum
    {
        WIFI_BOOT_PROVISIONING, // AP config mode
        WIFI_BOOT_STA,          // Normal station mode
    } wifi_boot_mode_t;

    // Public API
    EventGroupHandle_t wifi_manager_event_group();
    void wifi_manager_init(const char* device_name, const char* role); // role: "weapon" / "target"
    bool wifi_manager_is_connected();
    void wifi_manager_factory_reset();

    // Retrieve current IP as string (returns pointer to static buffer; empty if not connected)
    const char* wifi_manager_get_ip();

#ifdef __cplusplus
}
#endif

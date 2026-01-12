#pragma once
#include <lvgl.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        DM_EVT_NONE = 0,
        DM_EVT_HIT,
        DM_EVT_MSG,
        DM_EVT_ERROR_SET,
        DM_EVT_ERROR_CLEAR
    } dm_event_type_t;

    typedef struct
    {
        dm_event_type_t type;
        union
        {
            struct
            {
                uint32_t error_code;
            } err;
            struct
            {
                char text[24];
            } msg;
        };
    } dm_event_t;

    typedef struct
    {
        // "pull" callbacks so DM doesn't depend on your modules directly
        bool (*wifi_connected)(void);
        const char* (*wifi_ip)(void);
        const char* (*wifi_ssid)(void);
        const char* (*wifi_status)(void);
        int (*wifi_rssi)(void);

        uint32_t (*uptime_ms)(void);
        uint32_t (*free_heap)(void);

        bool (*ws_connected)(void);
        const char* (*device_name)(void);

        // optional game data
        int (*player_id)(void);
        int (*device_id)(void);
        int (*ammo)(void);
        uint32_t (*last_rx_ms_ago)(void);
        uint32_t (*rx_count)(void);
        uint32_t (*tx_count)(void);

        // target-specific
        int (*hit_count)(void);
        uint32_t (*last_hit_ms_ago)(void);
    } dm_sources_t;

    bool display_manager_init(lv_disp_t* disp, const dm_sources_t* src);
    bool display_manager_post(const dm_event_t* evt);
    void display_manager_task(void* pv); // pin to a core like your other tasks

#ifdef __cplusplus
}
#endif

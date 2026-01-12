#include "display_manager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
    DM_ST_BOOT = 0,
    DM_ST_DEBUG,
    DM_ST_OVERLAY_HIT,
    DM_ST_OVERLAY_MSG,
    DM_ST_ERROR
} dm_state_t;

static QueueHandle_t s_q;
static dm_sources_t s_src;
static dm_state_t s_state;
static dm_state_t s_return_state;
static uint32_t s_state_until_ms = 0;
static uint32_t s_last_slow_ms = 0;
static uint32_t s_last_fast_ms = 0;
static uint32_t s_error_code = 0;
static lv_obj_t* s_row1;
static lv_obj_t* s_row2;
static lv_obj_t* s_row3;
static lv_obj_t* s_overlay;

static uint32_t now_ms(void)
{
    return s_src.uptime_ms ? s_src.uptime_ms() : (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void set_rows(const char* r1, const char* r2, const char* r3)
{
    lv_label_set_text(s_row1, r1);
    lv_label_set_text(s_row2, r2);
    lv_label_set_text(s_row3, r3);
}

static void overlay_show(const char* txt)
{
    lv_label_set_text(s_overlay, txt);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void overlay_hide(void)
{
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void ui_init(lv_disp_t* disp)
{
    lv_obj_t* scr = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_row1 = lv_label_create(scr);
    s_row2 = lv_label_create(scr);
    s_row3 = lv_label_create(scr);

    lv_obj_align(s_row1, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_align(s_row2, LV_ALIGN_TOP_LEFT, 0, 11);
    lv_obj_align(s_row3, LV_ALIGN_TOP_LEFT, 0, 22);

    lv_obj_set_style_text_color(s_row1, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_row2, lv_color_white(), 0);
    lv_obj_set_style_text_color(s_row3, lv_color_white(), 0);

    s_overlay = lv_label_create(scr);
    lv_obj_set_style_text_color(s_overlay, lv_color_white(), 0);
    lv_obj_align(s_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void render_debug(uint8_t page, bool slow)
{
    (void)page;
    (void)slow;
    char r1[32], r2[32], r3[32];

    const bool wifi = s_src.wifi_connected ? s_src.wifi_connected() : false;
    const bool ws = s_src.ws_connected ? s_src.ws_connected() : false;

    // Data Sources
    const char* ssid = s_src.wifi_ssid ? s_src.wifi_ssid() : "?";
    const char* status = s_src.wifi_status ? s_src.wifi_status() : "?";
    const char* ip = (s_src.wifi_ip && wifi) ? s_src.wifi_ip() : "--";
    const char* dname = s_src.device_name ? s_src.device_name() : "Device";
    const int pid = s_src.player_id ? s_src.player_id() : -1;
    const int did = s_src.device_id ? s_src.device_id() : -1;

    // Weapon-specific
    const int ammo = s_src.ammo ? s_src.ammo() : -1;

    // Target-specific
    const int hits = s_src.hit_count ? s_src.hit_count() : 0;
    const uint32_t last_hit_ago = s_src.last_hit_ms_ago ? s_src.last_hit_ms_ago() : 0;

    // Prioritized Display Logic
    if (ws)
    {
        // WS Connected: Show Name and IDs
        snprintf(r1, sizeof(r1), "%s", dname);
        snprintf(r2, sizeof(r2), "DevID:%d", did);

        // Different display for weapon vs target
        if (ammo >= 0)
        {
            // Weapon: show player ID and ammo
            snprintf(r3, sizeof(r3), "PlyID:%d A:%d", pid, ammo);
        }
        else if (s_src.hit_count)
        {
            // Target: show hits and last hit time
            if (last_hit_ago < 60000)
                snprintf(r3, sizeof(r3), "Hits:%d (%lus)", hits, (unsigned long)(last_hit_ago / 1000));
            else
                snprintf(r3, sizeof(r3), "Hits:%d", hits);
        }
        else
        {
            // Fallback
            snprintf(r3, sizeof(r3), "PlyID:%d", pid);
        }
    }
    else if (wifi)
    {
        // WiFi Connected: Show IP and SSID
        snprintf(r1, sizeof(r1), "%s - OK", ssid);
        snprintf(r2, sizeof(r2), "RSSI:%d", s_src.wifi_rssi ? s_src.wifi_rssi() : 0);
        snprintf(r3, sizeof(r3), "%s", ip);
    }
    else
    {
        // Not Connected: Show Status and SSID (AP or Connecting)
        snprintf(r1, sizeof(r1), "%s", status);
        snprintf(r2, sizeof(r2), "%s", ssid);
        snprintf(r3, sizeof(r3), "RSSI:%d", s_src.wifi_rssi ? s_src.wifi_rssi() : 0);
    }

    set_rows(r1, r2, r3);
}

static void render_error(void)
{
    char r1[32], r2[32], r3[32];
    snprintf(r1, sizeof(r1), "ERROR");
    snprintf(r2, sizeof(r2), "C:%lu", (unsigned long)s_error_code);
    snprintf(r3, sizeof(r3), "Fix & reboot");
    set_rows(r1, r2, r3);
}

static void enter_state(dm_state_t st, uint32_t dur_ms)
{
    s_state = st;
    s_state_until_ms = dur_ms ? now_ms() + dur_ms : 0;
}

bool display_manager_init(lv_disp_t* disp, const dm_sources_t* src)
{
    if (!disp || !src)
        return false;
    s_src = *src;
    s_q = xQueueCreate(8, sizeof(dm_event_t));
    if (!s_q)
        return false;

    ui_init(disp);

    set_rows("RayZ", "BOOT", "");
    s_state = DM_ST_BOOT;
    enter_state(DM_ST_BOOT, 800);
    return true;
}

bool display_manager_post(const dm_event_t* evt)
{
    if (!s_q || !evt)
        return false;
    return xQueueSend(s_q, evt, 0) == pdTRUE;
}

static void handle_event(const dm_event_t* e)
{
    switch (e->type)
    {
        case DM_EVT_ERROR_SET:
            s_error_code = e->err.error_code;
            enter_state(DM_ST_ERROR, 0);
            render_error();
            break;
        case DM_EVT_ERROR_CLEAR:
            s_error_code = 0;
            enter_state(DM_ST_DEBUG, 0);
            break;
        case DM_EVT_HIT:
            s_return_state = s_state;
            enter_state(DM_ST_OVERLAY_HIT, 600);
            overlay_show("HIT!");
            break;
        case DM_EVT_MSG:
            s_return_state = s_state;
            enter_state(DM_ST_OVERLAY_MSG, 800);
            overlay_show(e->msg.text);
            break;
        default:
            break;
    }
}

void display_manager_task(void* pv)
{
    (void)pv;
    for (;;)
    {
        dm_event_t e;
        while (xQueueReceive(s_q, &e, 0) == pdTRUE)
        {
            handle_event(&e);
        }

        const uint32_t t = now_ms();

        if (s_state_until_ms && t >= s_state_until_ms)
        {
            if (s_state == DM_ST_OVERLAY_HIT || s_state == DM_ST_OVERLAY_MSG)
            {
                overlay_hide();
                enter_state(s_return_state == DM_ST_ERROR ? DM_ST_ERROR : DM_ST_DEBUG, 0);
            }
            else if (s_state == DM_ST_BOOT)
            {
                enter_state(DM_ST_DEBUG, 0);
            }
            s_state_until_ms = 0;
        }

        const bool slow = (t - s_last_slow_ms) >= 1000;
        const bool fast = (t - s_last_fast_ms) >= 100;

        if (s_state == DM_ST_ERROR)
        {
            if (slow)
            {
                render_error();
                s_last_slow_ms = t;
            }
        }
        else if (s_state == DM_ST_DEBUG)
        {
            if (fast)
            {
                render_debug(0, slow);
                s_last_fast_ms = t;
            }
            if (slow)
                s_last_slow_ms = t;
        }
        else if (s_state == DM_ST_OVERLAY_HIT)
        {
            if (fast)
            {
                static bool on = false;
                on = !on;
                overlay_show(on ? "HIT!" : "    ");
                s_last_fast_ms = t;
            }
        }

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

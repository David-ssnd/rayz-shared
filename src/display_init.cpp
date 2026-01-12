#include "display_init.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/i2c.h>
#include <lvgl.h>

#include "config.h"

static const char* TAG = "DisplayInit";

// Owned ONLY here
static esp_lcd_panel_handle_t s_panel = NULL;

// LVGL tick
static void lvgl_tick_timer_cb(void* arg)
{
    lv_tick_inc(1);
}

// ---- LVGL callbacks ----

static void ssd1306_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_map)
{
    if (!s_panel)
    {
        lv_disp_flush_ready(drv);
        return;
    }

    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2 + 1;
    int y2 = area->y2 + 1;

    if (x1 >= 0 && y1 >= 0 && x2 > x1 && y2 > y1 && x2 <= OLED_WIDTH && y2 <= OLED_HEIGHT)
    {
        esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2, y2, color_map);
    }

    lv_disp_flush_ready(drv);
}

static void ssd1306_rounder_cb(lv_disp_drv_t* drv, lv_area_t* area)
{
    area->y1 = (area->y1 / 8) * 8;
    area->y2 = (area->y2 / 8) * 8 + 7;

    if (area->x1 < 0)
        area->x1 = 0;
    if (area->y1 < 0)
        area->y1 = 0;
    if (area->x2 >= OLED_WIDTH)
        area->x2 = OLED_WIDTH - 1;
    if (area->y2 >= OLED_HEIGHT)
        area->y2 = OLED_HEIGHT - 1;
}

static void ssd1306_set_px_cb(lv_disp_drv_t* drv, uint8_t* buf, lv_coord_t buf_w, lv_coord_t x, lv_coord_t y,
                              lv_color_t color, lv_opa_t opa)
{
    if (x < 0 || y < 0 || x >= buf_w || y >= OLED_HEIGHT)
        return;

    const uint16_t byte_index = (uint16_t)x + (uint16_t)(y / 8) * (uint16_t)buf_w;
    const uint8_t bit_index = (uint8_t)(y & 0x7);

    if (lv_color_brightness(color) > 128)
        buf[byte_index] |= (1 << bit_index);
    else
        buf[byte_index] &= ~(1 << bit_index);
}

lv_disp_t* init_display(void)
{
    // ---------------------------------------------------------------------
    // I2C
    // ---------------------------------------------------------------------
    i2c_config_t i2c_conf = {};
    i2c_conf.mode = I2C_MODE_MASTER;
    i2c_conf.sda_io_num = (gpio_num_t)I2C_SDA_PIN;
    i2c_conf.scl_io_num = (gpio_num_t)I2C_SCL_PIN;
    i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_conf.master.clk_speed = 400000;

    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

    // ---------------------------------------------------------------------
    // SSD1306 panel
    // ---------------------------------------------------------------------
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {};
    io_config.dev_addr = OLED_I2C_ADDR;
    io_config.control_phase_bytes = 1;
    io_config.dc_bit_offset = 6;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_NUM_0, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.bits_per_pixel = 1;
    panel_config.reset_gpio_num = -1;

    typedef struct
    {
        uint8_t height;
    } ssd1306_vendor_config_t;
    static ssd1306_vendor_config_t vendor_cfg = {.height = OLED_HEIGHT};
    panel_config.vendor_config = &vendor_cfg;

    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &s_panel));

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_disp_on_off(s_panel, true);

    // ---------------------------------------------------------------------
    // LVGL core
    // ---------------------------------------------------------------------
    lv_init();

    static esp_timer_handle_t tick_timer;
    esp_timer_create_args_t tick_args = {};
    tick_args.callback = lvgl_tick_timer_cb;
    tick_args.name = "lvgl_tick";
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));

    // ---------------------------------------------------------------------
    // LVGL display driver (MONO buffer!)
    // ---------------------------------------------------------------------
    static lv_disp_draw_buf_t draw_buf;
    // One 8-pixel page worth of lines; set_px_cb packs bits into bytes.
    static uint8_t mono_buf[OLED_WIDTH * 8];

    lv_disp_draw_buf_init(&draw_buf, (lv_color_t*)mono_buf, NULL, OLED_WIDTH * 8);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = OLED_WIDTH;
    disp_drv.ver_res = OLED_HEIGHT;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.flush_cb = ssd1306_flush_cb;
    disp_drv.rounder_cb = ssd1306_rounder_cb;
    disp_drv.set_px_cb = ssd1306_set_px_cb;

    lv_disp_t* disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "SSD1306 display initialized");
    return disp;
}

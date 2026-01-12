#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Initializes I2C + SSD1306 + LVGL display driver
    // Returns lv_disp_t* on success, NULL on failure
    lv_disp_t* init_display(void);

#ifdef __cplusplus
}
#endif

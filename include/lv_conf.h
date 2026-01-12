#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// Color depth: 1 (monochrome for SSD1306)
#define LV_COLOR_DEPTH 1

// Display resolution
#define LV_HOR_RES_MAX 128
#define LV_VER_RES_MAX 32

// Memory management
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (16U * 1024U)

// Display refresh
#define LV_DISP_DEF_REFR_PERIOD 30

// Input device
#define LV_INDEV_DEF_READ_PERIOD 30

// Feature usage
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_REFR_DEBUG 0

// Font usage - enable built-in fonts including large 32px
#define LV_FONT_MONTSERRAT_8 0
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_20 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_48 0

#define LV_FONT_DEFAULT &lv_font_montserrat_10

// Themes
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_MONO 1

// Logging
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

// Asserts
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

#endif

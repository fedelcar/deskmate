/**
 * LVGL v8 configuration for Deskmate (466×466 round AMOLED)
 */
#if 1  /* Set this to "1" to enable the content (required by the build system) */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/*====================
   MEMORY SETTINGS
 *====================*/
/* Use heap_caps so we can put draw buffers in PSRAM */
#define LV_MEM_CUSTOM 1
#if LV_MEM_CUSTOM
#  define LV_MEM_CUSTOM_INCLUDE <esp32-hal-psram.h>
#  define LV_MEM_CUSTOM_ALLOC   ps_malloc
#  define LV_MEM_CUSTOM_FREE    free
#  define LV_MEM_CUSTOM_REALLOC ps_realloc
#endif

/*====================
   HAL SETTINGS
 *====================*/
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
#  define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
#  define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/*====================
   FEATURES
 *====================*/
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0
#define LV_USE_LOG          0
#define LV_USE_ASSERT_NULL  1
#define LV_USE_ASSERT_MALLOC 1

/*====================
   FONT USAGE
 *====================*/
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1

#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   WIDGETS
 *====================*/
#define LV_USE_ARC      1
#define LV_USE_BAR      1
#define LV_USE_BTN      1
#define LV_USE_LABEL    1
#define LV_USE_IMG      1
#define LV_USE_LINE     1
#define LV_USE_TABLE    1
#define LV_USE_ANIMIMG  1

/*====================
   THEMES
 *====================*/
#define LV_USE_THEME_DEFAULT 0
#define LV_THEME_DEFAULT_DARK 1

#endif /* LV_CONF_H */
#endif /* End "Content enable" */

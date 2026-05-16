#pragma once
#include <lvgl.h>

void display_init();
void display_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p);

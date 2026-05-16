#pragma once
#include <lvgl.h>
#include <ArduinoJson.h>

void ui_init();
void ui_update(JsonDocument &doc);
void ui_set_status(const char *msg);
void ui_update_clock();  // call once NTP syncs; also called by internal timer

#pragma once

// ─── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID   "freebox-flc"
#define WIFI_PASS   "wififede"

// ─── Server ──────────────────────────────────────────────────────────────────
// mDNS hostname advertised by the Node server ("deskmate.local")
#define SERVER_HOST "deskmate.local"
#define SERVER_PORT 3001
#define FETCH_INTERVAL_MS 60000   // refresh every 60 seconds

// ─── Display pins  (Waveshare ESP32-S3-Touch-AMOLED-1.75) ───────────────────
// Verify against: https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75
#define LCD_CS    6
#define LCD_SCLK  47
#define LCD_D0    18
#define LCD_D1    7
#define LCD_D2    8
#define LCD_D3    9
#define LCD_RST   17
#define LCD_W     466
#define LCD_H     466

// ─── Touch pins  (CST9217, I2C) ─────────────────────────────────────────────
#define TOUCH_SDA   1
#define TOUCH_SCL   2
#define TOUCH_INT   3
#define TOUCH_RST   -1
#define TOUCH_ADDR  0x1A

// ─── LVGL ────────────────────────────────────────────────────────────────────
// Two full-screen draw buffers in PSRAM for tear-free rendering
#define LV_BUF_LINES  80    // lines per buffer (80 × 466 × 2 bytes ≈ 74 KB each)

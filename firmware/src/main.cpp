#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <lvgl.h>
#include <esp_sntp.h>

#include "config.h"
#include "display.h"
#include "touch.h"
#include "ui.h"

// ─── LVGL draw buffers (allocated in PSRAM) ──────────────────────────────────
static lv_disp_drv_t    disp_drv;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t        *buf1 = nullptr;
static lv_color_t        *buf2 = nullptr;

// ─── Input device ────────────────────────────────────────────────────────────
static lv_indev_drv_t indev_drv;

// ─── Data fetch ──────────────────────────────────────────────────────────────
static unsigned long last_fetch = 0;

static void fetch_and_update() {
    ui_set_status("Updating...");

    // mDNS resolution: HTTPClient doesn't resolve .local via mDNS automatically
    char host[40];
    IPAddress ip = MDNS.queryHost("deskmate", 2000);
    if (ip != INADDR_NONE) {
        ip.toString().toCharArray(host, sizeof(host));
    } else {
        strncpy(host, SERVER_HOST, sizeof(host));
        Serial.printf("[fetch] mDNS failed, using %s\n", host);
    }

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/api/all", host, SERVER_PORT);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(8000);
    int code = http.GET();

    if (code == 200) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getStream());
        if (!err) {
            ui_update(doc);
        } else {
            ui_set_status("JSON error");
            Serial.printf("[fetch] JSON: %s\n", err.c_str());
        }
    } else {
        char msg[32];
        snprintf(msg, sizeof(msg), "HTTP %d", code);
        ui_set_status(msg);
        Serial.printf("[fetch] HTTP %d from %s\n", code, url);
    }
    http.end();
    last_fetch = millis();
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(3000); // wait for serial monitor to attach
    Serial.println("\n=== Deskmate booting ===");

    // 1. Display
    display_init();

    // 2. LVGL
    lv_init();

    buf1 = (lv_color_t *)ps_malloc(LCD_W * LV_BUF_LINES * sizeof(lv_color_t));
    buf2 = (lv_color_t *)ps_malloc(LCD_W * LV_BUF_LINES * sizeof(lv_color_t));
    if (!buf1 || !buf2) {
        Serial.println("[lvgl] PSRAM alloc failed — check PSRAM is enabled");
        while (1) delay(1000);
    }
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LCD_W * LV_BUF_LINES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = LCD_W;
    disp_drv.ver_res  = LCD_H;
    disp_drv.flush_cb = display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // 3. Touch
    touch_init();
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touch_read;
    lv_indev_drv_register(&indev_drv);

    // 4. LVGL tick is driven by LV_TICK_CUSTOM in lv_conf.h (calls millis())
    //    No manual timer needed.

    // 5. Build UI
    ui_init();
    ui_set_status("Connecting WiFi...");
    lv_timer_handler();

    // 6. WiFi
    Serial.printf("[wifi] Connecting to %s\n", WIFI_SSID);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 60) {
        lv_timer_handler();
        delay(500);
        if (tries % 10 == 9) Serial.printf("[wifi] still trying... (%d)\n", tries + 1);
        tries++;
    }

    if (WiFi.status() != WL_CONNECTED) {
        ui_set_status("WiFi failed — check config.h");
        Serial.println("[wifi] Failed");
        return;
    }
    Serial.printf("[wifi] Connected: %s\n", WiFi.localIP().toString().c_str());

    // 7. mDNS
    if (!MDNS.begin("deskmate-esp32")) {
        Serial.println("[mdns] begin failed");
    }

    // 8. NTP (for localtime() in UI)
    configTime(0, 0, "pool.ntp.org");
    // timezone: set your UTC offset here, e.g. -5*3600 for EST, -8*3600 for PST
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1); // Europe/Paris
    tzset();
    Serial.println("[ntp] Syncing...");

    // 9. First data fetch
    fetch_and_update();
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
    lv_timer_handler();

    if (millis() - last_fetch > FETCH_INTERVAL_MS) {
        fetch_and_update();
    }

    delay(5);
}

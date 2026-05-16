/**
 * Deskmate UI — three tap-swappable screens on a 466×466 round AMOLED.
 *
 * Screen 0 (Main):   time + date, weather summary, next event, Claude dot
 * Screen 1 (Weather): conditions + 8-hour temp strip
 * Screen 2 (Calendar): next 5 events list
 *
 * A single tap anywhere cycles to the next screen.
 */

#include "ui.h"
#include "config.h"

// ─── Palette ──────────────────────────────────────────────────────────────────
#define CLR_BG      lv_color_hex(0x000000)
#define CLR_WHITE   lv_color_hex(0xFFFFFF)
#define CLR_GRAY    lv_color_hex(0x888888)
#define CLR_CYAN    lv_color_hex(0x00D4FF)
#define CLR_ORANGE  lv_color_hex(0xFF8C00)
#define CLR_BLUE    lv_color_hex(0x4A90E2)
#define CLR_GREEN   lv_color_hex(0x4CAF50)
#define CLR_YELLOW  lv_color_hex(0xFFD700)
#define CLR_RED     lv_color_hex(0xFF4444)

// ─── Shared state ─────────────────────────────────────────────────────────────
static lv_obj_t *screens[3];
static int        current_screen = 0;

// Main screen widgets
static lv_obj_t *lbl_time, *lbl_date;
static lv_obj_t *lbl_temp, *lbl_weather_desc;
static lv_obj_t *lbl_hilo;
static lv_obj_t *lbl_event, *lbl_event_time;
static lv_obj_t *arc_claude;
static lv_obj_t *lbl_claude;

// Weather screen widgets
static lv_obj_t *lbl_w_main_temp, *lbl_w_desc, *lbl_w_details;
static lv_obj_t *lbl_w_hours[8];
static lv_obj_t *lbl_w_hour_times[8];

// Calendar screen widgets
static lv_obj_t *lbl_events[5];
static lv_obj_t *lbl_event_times[5];

// Status banner (loading / errors)
static lv_obj_t *lbl_status;

static lv_timer_t *clock_timer = nullptr;

// ─── Helpers ──────────────────────────────────────────────────────────────────
static lv_style_t style_screen;

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                              lv_color_t color, lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_text(l, "");
    return l;
}

// Tap handler — advance to next screen
static void tap_cb(lv_event_t *e) {
    current_screen = (current_screen + 1) % 3;
    lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
}

static void attach_tap(lv_obj_t *screen) {
    lv_obj_add_event_cb(screen, tap_cb, LV_EVENT_CLICKED, nullptr);
}

// Weather icon → short emoji-like ASCII (LVGL built-in fonts are ASCII-only)
static const char *icon_str(const char *icon) {
    if (!icon) return "?";
    if (strcmp(icon, "CLEAR")      == 0) return "SUNNY";
    if (strcmp(icon, "FEW_CLOUDS") == 0) return "PARTLY";
    if (strcmp(icon, "CLOUDS")     == 0) return "CLOUDY";
    if (strcmp(icon, "RAIN")       == 0) return "RAINY";
    if (strcmp(icon, "STORM")      == 0) return "STORM";
    if (strcmp(icon, "SNOW")       == 0) return "SNOW";
    if (strcmp(icon, "MIST")       == 0) return "FOGGY";
    return "?";
}

static lv_color_t icon_color(const char *icon) {
    if (!icon) return CLR_GRAY;
    if (strcmp(icon, "CLEAR")  == 0) return CLR_YELLOW;
    if (strcmp(icon, "RAIN")   == 0) return CLR_BLUE;
    if (strcmp(icon, "STORM")  == 0) return CLR_RED;
    if (strcmp(icon, "SNOW")   == 0) return CLR_CYAN;
    return CLR_WHITE;
}

// Format epoch ms → "3:45 PM"
static void fmt_time(char *buf, size_t sz, long long ms) {
    time_t t = ms / 1000;
    struct tm *tm = localtime(&t);
    strftime(buf, sz, "%l:%M %p", tm);
    // trim leading space
    if (buf[0] == ' ') memmove(buf, buf + 1, strlen(buf));
}

// ─── Screen 0: Main dashboard ─────────────────────────────────────────────────
static void build_main(lv_obj_t *scr) {
    // Time — large, centered near top
    lbl_time = make_label(scr, &lv_font_montserrat_48, CLR_WHITE);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_MID, 0, 72);

    // Date
    lbl_date = make_label(scr, &lv_font_montserrat_16, CLR_GRAY);
    lv_obj_align_to(lbl_date, lbl_time, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    // Weather temp
    lbl_temp = make_label(scr, &lv_font_montserrat_28, CLR_YELLOW);
    lv_obj_align(lbl_temp, LV_ALIGN_CENTER, 0, -20);

    // Weather description
    lbl_weather_desc = make_label(scr, &lv_font_montserrat_14, CLR_GRAY);
    lv_obj_align_to(lbl_weather_desc, lbl_temp, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    // High / Low
    lbl_hilo = make_label(scr, &lv_font_montserrat_12, CLR_GRAY);
    lv_obj_align_to(lbl_hilo, lbl_weather_desc, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    // Next event time
    lbl_event_time = make_label(scr, &lv_font_montserrat_12, CLR_CYAN);
    lv_obj_align(lbl_event_time, LV_ALIGN_BOTTOM_MID, 0, -72);

    // Next event title (wraps to 2 lines max)
    lbl_event = make_label(scr, &lv_font_montserrat_14, CLR_WHITE);
    lv_label_set_long_mode(lbl_event, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_event, 260);
    lv_obj_align_to(lbl_event, lbl_event_time, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);

    // Claude usage arc (bottom right quadrant)
    arc_claude = lv_arc_create(scr);
    lv_obj_set_size(arc_claude, 64, 64);
    lv_arc_set_rotation(arc_claude, 135);
    lv_arc_set_bg_angles(arc_claude, 0, 270);
    lv_arc_set_value(arc_claude, 0);
    lv_obj_set_style_arc_color(arc_claude, CLR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_claude, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_claude, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_claude, 5, LV_PART_MAIN);
    lv_obj_clear_flag(arc_claude, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(arc_claude, LV_ALIGN_BOTTOM_RIGHT, -52, -52);

    lbl_claude = make_label(scr, &lv_font_montserrat_12, CLR_ORANGE);
    lv_obj_align_to(lbl_claude, arc_claude, LV_ALIGN_OUT_TOP_MID, 0, -4);
}

// ─── Screen 1: Weather detail ─────────────────────────────────────────────────
static void build_weather(lv_obj_t *scr) {
    lv_obj_t *title = make_label(scr, &lv_font_montserrat_16, CLR_CYAN);
    lv_label_set_text(title, "WEATHER");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    lbl_w_main_temp = make_label(scr, &lv_font_montserrat_48, CLR_WHITE);
    lv_obj_align(lbl_w_main_temp, LV_ALIGN_TOP_MID, 0, 90);

    lbl_w_desc = make_label(scr, &lv_font_montserrat_16, CLR_GRAY);
    lv_obj_align_to(lbl_w_desc, lbl_w_main_temp, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    lbl_w_details = make_label(scr, &lv_font_montserrat_12, CLR_GRAY);
    lv_obj_align_to(lbl_w_details, lbl_w_desc, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    // 8-hour strip at bottom
    for (int i = 0; i < 8; i++) {
        int x = -154 + i * 44;
        lbl_w_hours[i] = make_label(scr, &lv_font_montserrat_12, CLR_WHITE);
        lv_obj_align(lbl_w_hours[i], LV_ALIGN_BOTTOM_MID, x, -62);

        lbl_w_hour_times[i] = make_label(scr, &lv_font_montserrat_12, CLR_GRAY);
        lv_obj_align(lbl_w_hour_times[i], LV_ALIGN_BOTTOM_MID, x, -44);
    }
}

// ─── Screen 2: Calendar ───────────────────────────────────────────────────────
static void build_calendar(lv_obj_t *scr) {
    lv_obj_t *title = make_label(scr, &lv_font_montserrat_16, CLR_CYAN);
    lv_label_set_text(title, "UPCOMING");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    for (int i = 0; i < 5; i++) {
        int y = -80 + i * 54;
        lbl_event_times[i] = make_label(scr, &lv_font_montserrat_12, CLR_CYAN);
        lv_obj_align(lbl_event_times[i], LV_ALIGN_CENTER, 0, y);

        lbl_events[i] = make_label(scr, &lv_font_montserrat_14, CLR_WHITE);
        lv_label_set_long_mode(lbl_events[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl_events[i], 280);
        lv_obj_align_to(lbl_events[i], lbl_event_times[i], LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────
void ui_update_clock() {
    time_t now = time(nullptr);
    if (now < 100000) return;  // NTP not synced yet
    struct tm *t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M", t);   // 24h — standard in France
    lv_label_set_text(lbl_time, buf);
    strftime(buf, sizeof(buf), "%a, %b %d", t);
    lv_label_set_text(lbl_date, buf);
}

static void clock_tick_cb(lv_timer_t *) { ui_update_clock(); }

void ui_init() {
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, CLR_BG);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_border_width(&style_screen, 0);
    lv_style_set_radius(&style_screen, LV_RADIUS_CIRCLE);

    for (int i = 0; i < 3; i++) {
        screens[i] = lv_obj_create(nullptr);
        lv_obj_add_style(screens[i], &style_screen, 0);
        lv_obj_set_size(screens[i], LCD_W, LCD_H);
        attach_tap(screens[i]);
    }

    build_main(screens[0]);
    build_weather(screens[1]);
    build_calendar(screens[2]);

    // Status banner floats above everything on the active screen
    lbl_status = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_status, CLR_GRAY, 0);
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lbl_status, "Connecting...");

    lv_disp_load_scr(screens[0]);

    clock_timer = lv_timer_create(clock_tick_cb, 10000, nullptr);  // update every 10 s
}

void ui_set_status(const char *msg) {
    lv_label_set_text(lbl_status, msg ? msg : "");
}

void ui_update(JsonDocument &doc) {
    // ── Time & date (always live) ────────────────────────────────────────────
    ui_update_clock();
    char buf[64];

    // ── Weather ──────────────────────────────────────────────────────────────
    JsonObject w = doc["weather"];
    if (w && !w["error"]) {
        const char *icon = w["icon"] | "?";
        const char *deg  = w["deg"]  | "F";
        int temp  = w["temp"]  | 0;
        int high  = w["high"]  | 0;
        int low   = w["low"]   | 0;
        int hum   = w["humidity"] | 0;
        int wind  = w["wind_speed"] | 0;

        snprintf(buf, sizeof(buf), "%d\xB0%s  %s", temp, deg, icon_str(icon));
        lv_label_set_text(lbl_temp, buf);
        lv_obj_set_style_text_color(lbl_temp, icon_color(icon), 0);

        const char *desc = w["description"] | "";
        lv_label_set_text(lbl_weather_desc, desc);

        snprintf(buf, sizeof(buf), "H %d\xB0  L %d\xB0", high, low);
        lv_label_set_text(lbl_hilo, buf);

        // Weather detail screen
        snprintf(buf, sizeof(buf), "%d\xB0%s", temp, deg);
        lv_label_set_text(lbl_w_main_temp, buf);
        lv_label_set_text(lbl_w_desc, desc);
        snprintf(buf, sizeof(buf), "H %d\xB0  L %d\xB0  Hum %d%%  Wind %d", high, low, hum, wind);
        lv_label_set_text(lbl_w_details, buf);

        JsonArray hourly = w["hourly"];
        int count = 0;
        for (JsonObject h : hourly) {
            if (count >= 8) break;
            int htemp = h["temp"] | 0;
            long long htime = h["time"] | 0LL;
            char tbuf[8];
            fmt_time(tbuf, sizeof(tbuf), htime);
            snprintf(buf, sizeof(buf), "%d\xB0", htemp);
            lv_label_set_text(lbl_w_hours[count], buf);
            lv_label_set_text(lbl_w_hour_times[count], tbuf);
            count++;
        }
        for (int i = count; i < 8; i++) {
            lv_label_set_text(lbl_w_hours[i], "");
            lv_label_set_text(lbl_w_hour_times[i], "");
        }
    }

    // ── Calendar ─────────────────────────────────────────────────────────────
    JsonArray events = doc["calendar"];
    int ev_count = 0;
    for (JsonObject ev : events) {
        int mins = ev["minutes_until"] | -1;
        const char *title = ev["title"] | "(no title)";

        // Main screen: first event only
        if (ev_count == 0) {
            if (mins < 60) snprintf(buf, sizeof(buf), "in %d min", mins);
            else           snprintf(buf, sizeof(buf), "in %dh %dm", mins / 60, mins % 60);
            lv_label_set_text(lbl_event_time, buf);
            lv_label_set_text(lbl_event, title);
        }

        // Calendar screen: up to 5
        if (ev_count < 5) {
            if (mins < 60) snprintf(buf, sizeof(buf), "in %d min", mins);
            else           snprintf(buf, sizeof(buf), "in %dh", mins / 60);
            lv_label_set_text(lbl_event_times[ev_count], buf);
            lv_label_set_text(lbl_events[ev_count], title);
        }
        ev_count++;
    }
    if (ev_count == 0) {
        lv_label_set_text(lbl_event_time, "");
        lv_label_set_text(lbl_event, "No events today");
    }
    for (int i = ev_count; i < 5; i++) {
        lv_label_set_text(lbl_event_times[i], "");
        lv_label_set_text(lbl_events[i], "");
    }

    // ── Claude usage ─────────────────────────────────────────────────────────
    JsonObject usage = doc["usage"];
    if (usage && !usage["error"]) {
        int days = usage["days_until_reset"] | 0;
        int msgs = usage["messages_this_month"] | 0;
        int today_msgs = usage["messages_today"] | 0;

        // Arc: rough indicator — scale messages to 0-100 assuming ~500/month is "full"
        int pct = min(100, msgs * 100 / 500);
        lv_arc_set_value(arc_claude, pct);

        snprintf(buf, sizeof(buf), "%d msg\nresets %dd", today_msgs, days);
        lv_label_set_text(lbl_claude, buf);
    }

    lv_label_set_text(lbl_status, "");
}

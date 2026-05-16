/**
 * Deskmate UI — three tap-swappable screens on a 466×466 round AMOLED.
 *
 * Screen 0 (Main):   time + date, weather summary, next event, Claude dot
 * Screen 1 (Weather): conditions + 8-hour temp strip
 * Screen 2 (Calendar): next 5 events list
 *
 * Tap anywhere to cycle to the next screen.
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
#define CLR_YELLOW  lv_color_hex(0xFFD700)
#define CLR_RED     lv_color_hex(0xFF4444)

// ─── Shared state ─────────────────────────────────────────────────────────────
static lv_obj_t *screens[3];
static int        current_screen = 0;

// Main screen widgets
static lv_obj_t *lbl_time, *lbl_date;
static lv_obj_t *lbl_temp, *lbl_weather_desc, *lbl_hilo;
static lv_obj_t *lbl_event, *lbl_event_time;
static lv_obj_t *arc_claude, *lbl_claude;

// Weather screen widgets
static lv_obj_t *lbl_w_main_temp, *lbl_w_desc, *lbl_w_details;
static lv_obj_t *lbl_w_hours[8], *lbl_w_hour_times[8];

// Calendar screen widgets
static lv_obj_t *lbl_events[5], *lbl_event_times[5];

// Status banner
static lv_obj_t *lbl_status;

static lv_timer_t *clock_timer = nullptr;
static lv_style_t  style_screen;

// ─── Helpers ──────────────────────────────────────────────────────────────────
static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                              lv_color_t color,
                              lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_text(l, "");
    return l;
}

static void tap_cb(lv_event_t *) {
    current_screen = (current_screen + 1) % 3;
    lv_scr_load_anim(screens[current_screen], LV_SCR_LOAD_ANIM_FADE_ON, 250, 0, false);
}

static void attach_tap(lv_obj_t *screen) {
    lv_obj_add_event_cb(screen, tap_cb, LV_EVENT_CLICKED, nullptr);
}

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

static void fmt_time(char *buf, size_t sz, long long ms) {
    time_t t = ms / 1000;
    struct tm *tm = localtime(&t);
    strftime(buf, sz, "%H:%M", tm);
}

// Strip non-ASCII bytes so Montserrat doesn't show □ for emoji/arrows.
static void strip_nonascii(char *dst, size_t dsz, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < dsz; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c >= 0x20 && c < 0x80) dst[j++] = src[i];
        // skip all multi-byte UTF-8 sequences (lead bytes 0x80-0xFF)
    }
    dst[j] = '\0';
}

// ─── Screen 0: Main dashboard ─────────────────────────────────────────────────
// All positions are absolute (LV_ALIGN_*) to avoid stacking issues when
// labels are empty at build time. Round-display safe zone is ≈380px
// diameter inscribed circle; keep content x ±150 from center (233,233).
static void build_main(lv_obj_t *scr) {
    // Time — big, top-center
    lbl_time = make_label(scr, &lv_font_montserrat_48, CLR_WHITE);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_MID, 0, 60);

    // Date — below time, fixed offset from top
    lbl_date = make_label(scr, &lv_font_montserrat_16, CLR_GRAY);
    lv_obj_align(lbl_date, LV_ALIGN_TOP_MID, 0, 120);

    // Weather temp + icon — center
    lbl_temp = make_label(scr, &lv_font_montserrat_28, CLR_YELLOW);
    lv_obj_align(lbl_temp, LV_ALIGN_CENTER, 0, -35);

    // Weather description — just below temp
    lbl_weather_desc = make_label(scr, &lv_font_montserrat_14, CLR_GRAY);
    lv_obj_align(lbl_weather_desc, LV_ALIGN_CENTER, 0, 5);

    // High / Low — below description
    lbl_hilo = make_label(scr, &lv_font_montserrat_12, CLR_GRAY);
    lv_obj_align(lbl_hilo, LV_ALIGN_CENTER, 0, 26);

    // Next event "in Xh Ym" — lower area
    lbl_event_time = make_label(scr, &lv_font_montserrat_12, CLR_CYAN);
    lv_obj_align(lbl_event_time, LV_ALIGN_BOTTOM_MID, 0, -95);

    // Next event title
    lbl_event = make_label(scr, &lv_font_montserrat_14, CLR_WHITE);
    lv_label_set_long_mode(lbl_event, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_event, 240);
    lv_obj_align(lbl_event, LV_ALIGN_BOTTOM_MID, 0, -72);

    // Claude usage arc — bottom-right
    arc_claude = lv_arc_create(scr);
    lv_obj_set_size(arc_claude, 70, 70);
    lv_arc_set_rotation(arc_claude, 135);
    lv_arc_set_bg_angles(arc_claude, 0, 270);
    lv_arc_set_value(arc_claude, 0);
    lv_obj_set_style_arc_color(arc_claude, CLR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_claude, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_claude, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_claude, 5, LV_PART_MAIN);
    lv_obj_clear_flag(arc_claude, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(arc_claude, LV_ALIGN_BOTTOM_RIGHT, -44, -44);

    lbl_claude = make_label(scr, &lv_font_montserrat_12, CLR_ORANGE);
    lv_obj_align(lbl_claude, LV_ALIGN_BOTTOM_RIGHT, -44, -118);
}

// ─── Screen 1: Weather detail ─────────────────────────────────────────────────
static void build_weather(lv_obj_t *scr) {
    lv_obj_t *title = make_label(scr, &lv_font_montserrat_16, CLR_CYAN);
    lv_label_set_text(title, "WEATHER");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    lbl_w_main_temp = make_label(scr, &lv_font_montserrat_48, CLR_WHITE);
    lv_obj_align(lbl_w_main_temp, LV_ALIGN_TOP_MID, 0, 90);

    lbl_w_desc = make_label(scr, &lv_font_montserrat_16, CLR_GRAY);
    lv_obj_align(lbl_w_desc, LV_ALIGN_TOP_MID, 0, 155);

    lbl_w_details = make_label(scr, &lv_font_montserrat_12, CLR_GRAY);
    lv_obj_align(lbl_w_details, LV_ALIGN_TOP_MID, 0, 180);

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
        int y = -80 + i * 52;
        lbl_event_times[i] = make_label(scr, &lv_font_montserrat_12, CLR_CYAN);
        lv_obj_align(lbl_event_times[i], LV_ALIGN_CENTER, 0, y);

        lbl_events[i] = make_label(scr, &lv_font_montserrat_14, CLR_WHITE);
        lv_label_set_long_mode(lbl_events[i], LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl_events[i], 280);
        lv_obj_align(lbl_events[i], LV_ALIGN_CENTER, 0, y + 18);
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────
void ui_update_clock() {
    time_t now = time(nullptr);
    if (now < 100000) return;  // NTP not synced yet
    struct tm *t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M", t);
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
    // No LV_RADIUS_CIRCLE — round appearance comes from the physical hardware,
    // not LVGL clipping. Clipping to a circle breaks content near the edges.

    for (int i = 0; i < 3; i++) {
        screens[i] = lv_obj_create(nullptr);
        lv_obj_add_style(screens[i], &style_screen, 0);
        lv_obj_set_size(screens[i], LCD_W, LCD_H);
        attach_tap(screens[i]);
    }

    build_main(screens[0]);
    build_weather(screens[1]);
    build_calendar(screens[2]);

    lbl_status = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_status, CLR_GRAY, 0);
    lv_obj_align(lbl_status, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_label_set_text(lbl_status, "Connecting...");

    lv_disp_load_scr(screens[0]);

    clock_timer = lv_timer_create(clock_tick_cb, 10000, nullptr);
}

void ui_set_status(const char *msg) {
    lv_label_set_text(lbl_status, msg ? msg : "");
}

void ui_update(JsonDocument &doc) {
    ui_update_clock();
    char buf[128];

    // ── Weather ──────────────────────────────────────────────────────────────
    JsonObject w = doc["weather"];
    if (w && !w["error"]) {
        const char *icon = w["icon"] | "?";
        const char *deg  = w["deg"]  | "C";
        int temp  = w["temp"]  | 0;
        int high  = w["high"]  | 0;
        int low   = w["low"]   | 0;
        int hum   = w["humidity"]   | 0;
        int wind  = w["wind_speed"] | 0;

        // \xC2\xB0 = UTF-8 degree sign (U+00B0)
        snprintf(buf, sizeof(buf), "%d\xC2\xB0%s  %s", temp, deg, icon_str(icon));
        lv_label_set_text(lbl_temp, buf);
        lv_obj_set_style_text_color(lbl_temp, icon_color(icon), 0);

        const char *desc = w["description"] | "";
        lv_label_set_text(lbl_weather_desc, desc);

        snprintf(buf, sizeof(buf), "H %d\xC2\xB0  L %d\xC2\xB0", high, low);
        lv_label_set_text(lbl_hilo, buf);

        // Weather detail screen
        snprintf(buf, sizeof(buf), "%d\xC2\xB0%s", temp, deg);
        lv_label_set_text(lbl_w_main_temp, buf);
        lv_label_set_text(lbl_w_desc, desc);
        snprintf(buf, sizeof(buf), "H%d\xC2\xB0 L%d\xC2\xB0  Hum %d%%  Wind %d",
                 high, low, hum, wind);
        lv_label_set_text(lbl_w_details, buf);

        JsonArray hourly = w["hourly"];
        int count = 0;
        for (JsonObject h : hourly) {
            if (count >= 8) break;
            char tbuf[8];
            fmt_time(tbuf, sizeof(tbuf), (long long)(h["time"] | 0LL));
            snprintf(buf, sizeof(buf), "%d\xC2\xB0", (int)(h["temp"] | 0));
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
    char title_buf[64];
    for (JsonObject ev : events) {
        int mins = ev["minutes_until"] | -1;
        strip_nonascii(title_buf, sizeof(title_buf), ev["title"] | "(no title)");

        if (ev_count == 0) {
            if (mins < 60) snprintf(buf, sizeof(buf), "in %d min", mins);
            else           snprintf(buf, sizeof(buf), "in %dh %dm", mins / 60, mins % 60);
            lv_label_set_text(lbl_event_time, buf);
            lv_label_set_text(lbl_event, title_buf);
        }
        if (ev_count < 5) {
            if (mins < 60) snprintf(buf, sizeof(buf), "in %d min", mins);
            else           snprintf(buf, sizeof(buf), "in %dh", mins / 60);
            lv_label_set_text(lbl_event_times[ev_count], buf);
            lv_label_set_text(lbl_events[ev_count], title_buf);
        }
        ev_count++;
    }
    if (ev_count == 0) {
        lv_label_set_text(lbl_event_time, "");
        lv_label_set_text(lbl_event, "No upcoming events");
    }
    for (int i = ev_count; i < 5; i++) {
        lv_label_set_text(lbl_event_times[i], "");
        lv_label_set_text(lbl_events[i], "");
    }

    // ── Claude usage ─────────────────────────────────────────────────────────
    JsonObject usage = doc["usage"];
    if (usage && !usage["error"]) {
        int days      = usage["days_until_reset"]    | 0;
        int today_msg = usage["messages_today"]      | 0;
        int total_msg = usage["messages_this_month"] | 0;

        int pct = min(100, total_msg * 100 / 500);
        lv_arc_set_value(arc_claude, pct);

        snprintf(buf, sizeof(buf), "%d msg\nresets %dd", today_msg, days);
        lv_label_set_text(lbl_claude, buf);
    }

    lv_label_set_text(lbl_status, "");
}

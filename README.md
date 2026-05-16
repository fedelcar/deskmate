# Deskmate

A personal companion display on a **Waveshare ESP32-S3-Touch-AMOLED-1.75** (466×466 round AMOLED). Shows weather, upcoming calendar events, and Claude Code usage at a glance. Tap the screen to cycle between three views.

```
         SUNNY  74°F          ← weather arc
        H 81°  L 62°

           2:34 PM            ← time/date center
          Fri, May 16

       in 25 min              ← next event
      Team Standup          ○  ← Claude usage dot
```

**Tap** → cycles: Main → Weather detail → Calendar → back.

---

## Architecture

```
Mac (you)
  └── server/          Node.js companion server
        ├── OpenWeatherMap API  (weather)
        ├── iCal URL           (calendar)
        └── ~/.claude/history.jsonl  (Claude usage)
            ↕  HTTP  (mDNS: deskmate.local:3001)
ESP32-S3
  └── firmware/        PlatformIO + LVGL
```

---

## Hardware

| What | Part |
|------|------|
| Board | Waveshare ESP32-S3-Touch-AMOLED-1.75 |
| Display | 1.75" round AMOLED, 466×466, CO5300 driver |
| Touch | CST9217, I2C |
| MCU | ESP32-S3R8 — 240 MHz, 8 MB PSRAM, 16 MB Flash |

---

## Setup

### 1. Install tools

```bash
# Install Node.js (if needed)
brew install node

# Install VS Code + PlatformIO extension
# https://platformio.org/install/ide?install=vscode
```

### 2. Clone & install server deps

```bash
cd deskmate/server
cp .env.example .env
npm install
```

Edit `server/.env`:

```env
OWM_API_KEY=<your OpenWeatherMap key>   # free at openweathermap.org/api
LATITUDE=40.7128
LONGITUDE=-74.0060
UNITS=imperial                           # or metric
ICAL_URL=https://calendar.google.com/calendar/ical/...   # see below
```

**Getting your iCal URL:**
- **Google Calendar**: Settings → click your calendar → scroll to "Secret address in iCal format" → copy the link.
- **Apple iCloud**: Calendar.app → right-click a calendar → Share Calendar → enable public calendar → copy URL.

### 3. Configure WiFi in firmware

Edit `firmware/src/config.h`:

```c
#define WIFI_SSID  "YourWiFiName"
#define WIFI_PASS  "YourWiFiPassword"
```

Also set your timezone in `firmware/src/main.cpp` → look for `setenv("TZ", ...)`.  
Standard values: `"EST5EDT,M3.2.0,M11.1.0"` / `"PST8PDT,M3.2.0,M11.1.0"` / `"UTC0"`.

### 4. Install the GFX library with CO5300 support

PlatformIO will pull most libraries automatically, but the CO5300 display driver requires the Arduino_GFX library. If the build fails with `Arduino_CO5300 not found`:

1. Open VS Code → PlatformIO → Libraries
2. Search for **"Arduino_GFX"** by moononournation and install v1.4.7+  
   *Or* clone Waveshare's fork which guarantees CO5300:
   ```bash
   git clone https://github.com/waveshareteam/Arduino_GFX \
     ~/.platformio/lib/Arduino_GFX
   ```

### 5. Flash the firmware

1. Connect the board via USB-C.
2. Open `firmware/` in VS Code with PlatformIO.
3. Click **Upload** (→ button in the bottom bar).  
   First build takes ~2 minutes (LVGL compiles from source).
4. Open **Serial Monitor** at 115200 baud to watch the boot log.

### 6. Start the server

```bash
cd server
npm start
```

You should see:
```
Deskmate server running on port 3001
Local:   http://localhost:3001/api/all
mDNS:    http://deskmate.local:3001/api/all
```

The ESP32 will find the server automatically via mDNS (`deskmate.local`). No IP address to configure.

---

## Pin reference (Waveshare ESP32-S3-Touch-AMOLED-1.75)

> Verify against the [official wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75) if anything looks wrong.

| Signal | GPIO |
|--------|------|
| LCD CS | 6 |
| LCD SCLK | 47 |
| LCD D0 (MOSI) | 18 |
| LCD D1 | 7 |
| LCD D2 | 8 |
| LCD D3 | 9 |
| LCD RST | 17 |
| Touch SDA | 1 |
| Touch SCL | 2 |
| Touch INT | 3 |

---

## API endpoints (server)

| Endpoint | Returns |
|----------|---------|
| `GET /api/all` | All data in one payload (ESP32 uses this) |
| `GET /api/weather` | Current conditions + 8-hour hourly |
| `GET /api/calendar` | Next 5 events within 7 days |
| `GET /api/usage` | Claude Code session + message counts for this month |
| `GET /health` | `{"ok":true}` |

---

## Display screens

| Screen | How to reach | Shows |
|--------|-------------|-------|
| Main | default | Time, weather, next event, Claude dot |
| Weather | 1 tap | Full conditions + 8-hour strip |
| Calendar | 2 taps | Next 5 events with countdowns |

---

## Troubleshooting

**Display stays black**  
Check the pin definitions in `config.h` against the Waveshare wiki. Also confirm the GFX library has CO5300 support (see step 4 above).

**"WiFi failed"**  
Double-check `WIFI_SSID` / `WIFI_PASS` in `config.h`. The board is 2.4 GHz only.

**"HTTP 0" or can't reach server**  
Make sure the server is running and on the same network. mDNS (`deskmate.local`) requires the Mac and ESP32 on the same subnet. If mDNS fails, hard-code your Mac's IP in `config.h`:
```c
#define SERVER_HOST "192.168.1.42"   // your Mac's local IP (System Settings → Wi-Fi)
```

**Calendar shows no events**  
Check that `ICAL_URL` is the *secret* iCal URL (not the public HTML link). Test it in a browser — it should download a `.ics` file.

**Time is wrong**  
Update the `TZ` value in `main.cpp`. Full list: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

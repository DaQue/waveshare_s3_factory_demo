# waveshare_s3_factory_display

Weather dashboard firmware for the Waveshare ESP32-S3 3.5" touch display, built with ESP-IDF.

## Overview
- Framework: ESP-IDF 5.4
- Language: C + C++
- Entry point: `main/main.cpp` (`app_main`)
- UI stack: BSP display + BSP touch + LVGL + custom drawing layer

## Features
- Current conditions page ("Now")
- Indoor sensor page ("Indoor")
- Forecast page with daily rows and hourly drill-down
- Home-screen 3-day preview with larger side-by-side cards
- I2C scan page
- Wi-Fi scan page
- About page
- BME280 indoor sensor readout (temperature, humidity, pressure)
- OpenWeather HTTPS sync for current + forecast

## Version
- Current release target: `0.10.0`
- Update is USB flash only (no OTA flow in this repo)

## Build
```bash
source /media/david/Shared/PutDownloadsHere/DownloadsGoHere/esp-idf-v5.4/export.sh
idf.py set-target esp32s3
idf.py build
```

## Flash
```bash
source /media/david/Shared/PutDownloadsHere/DownloadsGoHere/esp-idf-v5.4/export.sh
idf.py -p /dev/ttyACM0 flash
```

## Monitor Logs
```bash
source /media/david/Shared/PutDownloadsHere/DownloadsGoHere/esp-idf-v5.4/export.sh
idf.py -p /dev/ttyACM0 monitor
```

Exit monitor with `Ctrl+]`.

## OpenWeather + Local Config Setup
1. Create your local config file from the example:
```bash
cp main/wifi_local.h.example main/wifi_local.h
```
2. Get an API key:
- Create/login account at `https://openweathermap.org/`
- Generate API key at `https://home.openweathermap.org/api_keys`
- New keys can take a few minutes to activate
3. Edit `main/wifi_local.h` and set:
- `WIFI_SSID_LOCAL`
- `WIFI_PASS_LOCAL`
- `WEATHER_API_KEY_LOCAL`
- `WEATHER_QUERY_LOCAL`
4. `WEATHER_QUERY_LOCAL` format examples:
- ZIP: `zip=63301,US`
- City/Country: `q=Saint Charles,US`
- Coordinates: `lat=38.7812&lon=-90.4810`

`main/wifi_local.h` is intentionally gitignored and should never be committed.

## Runtime Wi-Fi/API Override (Persistent)
Default credentials/config are read from `main/wifi_local.h`.
Interactive config mode is available at boot.

**Entering config mode:**
- Press the **BOOT button** anytime during normal operation
- Device checks the button 5 times per second
- Enters interactive console until you type `continue`

**In config mode:**
- Stays open until you type `continue`, `exit`, or `done`
- Characters echo as you type, backspace works
- `> ` prompt indicates ready for input

**Commands:**
```text
wifi show                  # Show current Wi-Fi config
wifi set <ssid> <pass>     # Set Wi-Fi credentials
wifi clear                 # Clear saved override

api show                   # Show API config (key visible)
api set-key <key>          # Set OpenWeather API key
api set-query <query>      # Set location query
api clear                  # Clear API overrides

continue / exit / done     # Exit config, boot normally
wifi reboot / api reboot   # Save and reboot immediately
```

**Notes:**
- Settings save to NVS (persistent storage)
- Changes require reboot to take effect
- Override precedence: NVS override > `main/wifi_local.h` defaults
- NVS overrides persist across reflashes

## Touch And Sensor Troubleshooting
- If flash works but monitor fails to open `/dev/ttyACM0`, close old monitor sessions first.
- If touch gestures stop working, verify touch fallback taps still navigate:
  - bottom-left tap: previous screen
  - bottom-right tap: next screen
- If BME280 shows `--`, check hardware/wiring first, then confirm logs include:
  - `BME280 initialized at address 0x77`
  - `Indoor sensor ready (BME280)`

## Developer Notes
- Main UI flow and touch logic: `main/app_touch_forecast.cpp`
- Forecast parsing and icon mapping: `main/app_weather.cpp`
- Screen composition: `main/drawing_screen.c`
- BME280 BSP: `components/esp_bsp/bsp_bme280.c`
- Touch BSP: `components/esp_bsp/bsp_touch.c`

## Lint (Optional)
Build once to generate `build/compile_commands.json`, then:

```bash
tools/run-clang-tidy.sh
```

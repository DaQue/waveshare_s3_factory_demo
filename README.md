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
- Current release target: `0.8.1`
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
Interactive serial commands are available during the boot config window.

**Console behavior:**
- 15-second initial window after boot
- Typing any key extends the timeout by 5 seconds (so you have unlimited time while actively typing)
- Countdown shows at 10, 5, 3, 2, 1 seconds remaining
- Characters echo as you type, backspace works
- A `> ` prompt indicates the console is ready

**Commands:**
```text
wifi show                         # Show current Wi-Fi config
wifi set <ssid> <password>        # Set Wi-Fi credentials
wifi set "My SSID" "My Password"  # Quoted form for spaces
wifi clear                        # Clear saved override
wifi reboot                       # Reboot to apply changes

api show                          # Show current API config
api set-key <openweather_api_key> # Set API key
api set-query <query_string>      # Set location query
api set-query "zip=63301,US"      # Example query
api clear                         # Clear API overrides
api reboot                        # Reboot to apply changes
```

**Notes:**
- `wifi set` and `api set-*` commands save to NVS (persistent storage)
- Changes require reboot to take effect (`wifi reboot` or `api reboot`)
- Override precedence: NVS override > `main/wifi_local.h` defaults
- NVS overrides persist across reflashes
- To return to header defaults, run `wifi clear` and/or `api clear`
- Recommended path for now is `main/wifi_local.h` + USB flash updates.

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

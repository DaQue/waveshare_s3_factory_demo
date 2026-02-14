# waveshare_s3_factory_display

Weather dashboard firmware for the Waveshare ESP32-S3 3.5" touch display, built with ESP-IDF.

## Overview
- Framework: ESP-IDF 5.4
- Language: C + C++
- Entry point: `main/main.cpp` (`app_main`)
- UI stack: BSP display + BSP touch + LVGL + custom drawing layer

## Features
- Current conditions page ("Now")
- Forecast page with daily rows and hourly drill-down
- Home-screen 3-day preview with per-day icon and Hi/Low
- I2C scan page
- Wi-Fi scan page
- About page
- BME280 indoor sensor readout (temperature, humidity, pressure)
- OpenWeather HTTPS sync for current + forecast

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

## Runtime Wi-Fi Override (Persistent)
Default credentials are read from `main/wifi_local.h`.
During the startup config window (about 8 seconds), use:

```text
wifi show
wifi set <ssid> <password>
wifi set "My SSID" "My Password"
wifi clear
wifi reboot
```

- `wifi set` and `wifi clear` update NVS.
- `wifi reboot` applies saved credentials immediately.
- If no saved override exists, firmware falls back to `wifi_local.h`.

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

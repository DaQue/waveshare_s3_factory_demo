# Project Status

Date: 2026-02-14
Repo: `waveshare_s3_factory_display`
Path: `/media/david/Shared/cprojects/waveshare_s3_factory_display`
Remote: `git@github.com:DaQue/waveshare_s3_factory_demo.git`

## Current Runtime State
- Firmware builds and flashes successfully with ESP-IDF 5.4.
- Touch navigation is working again (swipe plus bottom-edge tap fallback).
- BME280 startup path is stable and detects sensor on `0x77` when hardware is present.
- Weather sync (current + forecast) is succeeding over HTTPS.

## Active UI Flow
- Page order: `Now -> Forecast -> I2C Scan -> Wi-Fi Scan -> About -> Now`
- Home (`Now`) includes:
  - Current weather icon + temp
  - Indoor sensor lines
  - Compact 3-day preview with icon and Hi/Low
- Forecast page supports hourly drill-down.

## Key Fixes Landed (2026-02-14)
- Touch reliability and coordinates:
  - `components/esp_bsp/bsp_touch.c`
  - Removed over-aggressive rejection and fixed touch count propagation.
- BME280 robustness:
  - `components/esp_bsp/bsp_bme280.c`
  - Added direct chip-id read fallback after probe miss.
  - Reduced I2C transfer timeout to avoid long bus stalls.
- Runtime stability:
  - `main/app_runtime.cpp`
  - Removed aggressive BME re-init loop that could trigger watchdog under bus issues.
- Forecast content and home preview:
  - `main/app_weather.cpp`
  - `main/app_touch_forecast.cpp`
  - `main/app_state_ui.cpp`
  - `main/drawing_screen.c`
  - Added compact daily preview data path (day/icon/hi/low) to the home screen.

## Known Caveats
- If `/dev/ttyACM0` monitor access fails intermittently, a stale serial monitor process is usually holding the port.
- `clouds` and `overcast` icon assets are currently identical image files, so those two conditions appear the same.

## Verification Checklist
- Build: `idf.py build`
- Flash: `idf.py -p /dev/ttyACM0 flash`
- Monitor: `idf.py -p /dev/ttyACM0 monitor`
- Expected logs include:
  - `BME280 initialized at address 0x77`
  - `Indoor sensor ready (BME280)`
  - Weather HTTPS status `200`

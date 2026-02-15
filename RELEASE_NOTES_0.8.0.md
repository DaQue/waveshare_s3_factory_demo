# Release Notes - 0.8.0

Date: 2026-02-15

## Summary
Public preview release for USB-flash firmware on Waveshare ESP32-S3 3.5" display.

## Highlights
- New dedicated Indoor page for BME280 readout (large text for readability)
- Home screen redesigned with larger 3-day forecast cards
- Forecast hourly view swipe behavior improved:
  - Vertical swipes can move across days
  - Swiping earlier on first day exits hourly view to main forecast list
- Weather icon mapping improved for atmospheric conditions (mist/haze etc.)
- Runtime Wi-Fi/NTP flow refactored for non-blocking startup retry behavior
- Fixed app version to `0.8.0`

## Notes
- USB flashing is the supported update method for this release.
- Local secrets are not part of source control:
  - `main/wifi_local.h` is gitignored
  - use `main/wifi_local.h.example` as the template

# waveshare_s3_factory_display

ESP-IDF C/C++ starter project cloned from a known-working Waveshare S3 factory demo on 2026-02-08.

This project keeps the original display/touch/LVGL stack so you can start from a verified working baseline.

## Project Type
- ESP-IDF project
- Language mix: C + C++
- Main entrypoint: `main/main.cpp` (`app_main`)

## Display Baseline
The display pipeline is initialized in `main/main.cpp` via:
- `bsp_display_init(...)`
- `bsp_touch_init(...)`
- `lv_port_init()`
- `drawing_screen_init()`

## Build
1. Export ESP-IDF environment.
2. Build:

```bash
idf.py set-target esp32s3
idf.py build
```

## Flash
```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Adjust the serial port as needed.

## clang-tidy
Build once first so `build/compile_commands.json` exists, then run:

```bash
tools/run-clang-tidy.sh main/main.cpp
```

Lint all C/C++ files under `main/`:

```bash
tools/run-clang-tidy.sh
```

## Notes
- This copy intentionally excludes the old `build/` directory.
- `sdkconfig` is included from the known-working baseline.

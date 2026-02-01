Waveshare ESP32-S3 3.5" Weather Station Bring-up
=================================================

This repo is a bring-up playground for an ESP32-S3 board. It currently focuses on
LCD bring-up with a trial harness.

Features
--------
- ESP-IDF + Rust std setup
- USB-Serial-JTAG input for trial prompts
- LCD trial harness (solid green, then box/X on success)

Prereqs
-------
- ESP Rust toolchain installed via `espup`
- ESP environment vars available from `/home/david/export-esp.sh`

Build
-----
```
source /home/david/export-esp.sh
cargo +esp build -Zbuild-std=std,panic_abort
```

Flash + Monitor
--------------
```
source /home/david/export-esp.sh
cargo +esp run -Zbuild-std=std,panic_abort
```

Trial Flow
----------
1) Open the monitor (via `cargo +esp run ...`).
2) The firmware cycles through LCD init trials and draws solid green.
3) Enter result: `p` / `b` / `n1` / `n2` when prompted.
4) If `p`, it draws a blue box with a yellow X and asks again.

Notes
-----
- USB-Serial-JTAG is used for input, not UART0.
- `sdkconfig.defaults` sets `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384`.

Troubleshooting
---------------
- No response to typing: make sure the board is using USB-Serial-JTAG, not an
  external USB-UART bridge.
- Build errors about the target: ensure `espup` is installed and the environment
  is loaded with `source /home/david/export-esp.sh`.

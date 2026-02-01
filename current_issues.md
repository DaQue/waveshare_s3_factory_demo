# Current Issues (waveshare_s3_3p)

## Primary symptom
- LCD backlight turns on, but the panel shows random noise (sometimes briefly) and then goes to a dark/black screen.
- Occasionally shows vertical jagged green lines or static noise right after flash.
- Status: still broken (no pass yet).

## What is working
- USB-Serial-JTAG input/output works (ping/pong, logging).
- PMU (AXP2101) responds over I2C and rails are enabled; logs show:
  - IC type: `0x4A`
  - LDO/DCDC enable registers set
  - BLDO1/BLDO2 voltages set
- LCD driver initializes without errors:
  - `lcd_panel.axs15231b: LCD panel create success`
  - `lcd_panel.axs15231b: send init commands success`

## What appears broken
- LCD panel does not display correct framebuffer data (likely QSPI communication or panel init mismatch).
- QSPI reads return `0xFF` for RDID/RDSD/RDDPM/RDDB, indicating reads may be unsupported or opcode mismatch.

## Steps already tried
### Power / PMU
- Added AXP2101 init over I2C.
- Enabled BLDO1/BLDO2 and ALDO1-4, CPUSLDO, DLDO1/2; also enabled DC3/DC5.
- Verified via register logs (LDO/DCDC on, voltage registers set).

### Panel init
- Used custom init table copied from Waveshare demo.
- Switched to **driver default init** (no custom table) to avoid bad sequences.
- Forced normal display mode after init with `0x13`.

### SPI/QSPI config
- SPI host: `SPI2_HOST`
- Pins: CS=12, SCLK=5, D0=1, D1=2, D2=3, D3=4, RST=NC, BL=6
- QSPI enabled (`quad_mode = 1`)
- SPI mode 3, cmd bits 32, param bits 8
- Clock reduced from 40 MHz → 20 MHz
- DMA disabled to avoid non‑DMA buffers
- `SPICOMMON_BUSFLAG_QUAD` set on bus

### Drawing / buffer strategies
- Full-frame draw now used (single `esp_lcd_panel_draw_bitmap`) to avoid RAMWRC and match factory demo behavior.
- Buffer size: 320×480×2 bytes; `max_transfer_sz` set to full-frame.
- Driver patched to always use `RAMWR` (2C) and never `RAMWRC` (3C) for chunked draws.

### QSPI reads
- Added readback using QSPI read opcode (0x0B). All reads returned `FF`.

## Latest behavior
- After flash: brief green/noise, then dark/black screen with backlight on.
- On replug: black screen with backlight on.
- Full-frame draw attempt caused OOM (307200 bytes) and repeated reboots.
- User observed (landscape, USB right): left vertical blue bar, jagged green/yellow lines, wide green block on far right (likely uninitialized/garbled frame).

## Latest trial results (2026-02-01)
- Automated sweep (QSPI cmd32/param8 @ 5 MHz): L0123 produced nf/gf flashes; all other lane maps blank.
- SPI fallback: SPI m0 cmd8 -> gf, SPI m3 cmd8 -> blank.
- User observation: brief noise on replug, then blank/black.
- Sweep v2 (RAMWR forced, still chunked): trials 01–05 all gf (trial 01 noted “gf then blank”).
- Sweep v3 (qspi_if toggle): QIO cmd8 @ 5 MHz (mode 3) showed blue-left/red-right then off; QIO cmd8 @ 5 MHz mode 0 blank; QIO cmd8 @ 40 MHz blank; QSPI cmd32 @ 5 MHz blank; QSPI cmd32 @ 40 MHz gf.

## AXS15231B port (2026-02-01, pending test)
- Rust harness now uses `esp_lcd_new_panel_axs15231b` with vendor init table copied from factory `bsp_display.c`.
- AXS15231B component pulled via ESP-IDF component manager (`espressif/esp_lcd_axs15231b` v1.0.0).
- QSPI config matches factory macro: SPI mode 3, cmd bits 32, param bits 8, quad mode enabled.
- Trial set now focuses on L0123 only and adds 5/40 MHz full-frame draws; ~3s between trials, stop-on-pass.

## Suspected root causes
- QSPI command framing or data lane mapping mismatch (opcode/bit packing, data line order).
- Panel init sequence still not matched to this specific LCD revision.
- Read opcode not supported or requires different command format.

## Next ideas (after v2 sweep)
- If full-frame + 40 MHz still fail, consider forcing RAMWR-only in the driver or sending RASET even in QSPI mode.
- Try MADCTL swaps/mirrors or a small Y gap to see if content appears off-screen.
- Verify hardware path with the factory ESP-IDF demo to rule out a bad panel.

# Project Status And Known Issues

## Current Version: 0.10.0

### Changes in 0.10.0
- **BOOT button trigger for config mode** - press anytime
  - No boot delay - just press BOOT button whenever you need config
  - Button checked 5 times per second during normal operation
- Interactive mode stays open until you type `continue`
- Character echo, backspace support, `> ` prompt
- CR and LF both work as Enter (fixes ESP-IDF monitor compatibility)
- API key now visible in `api show` output for verification
- Cleaner help output with exit instructions
- RX buffer increased to 512 bytes

### Previous (0.8.0)
- About screen version fix (uses runtime app metadata)
- Local API config template added

## Config Mode

**To enter config mode:**
1. Press the BOOT button anytime during normal operation
2. Console shows "BOOT button pressed - entering config mode"
3. Type commands, then `continue` to resume or `wifi reboot` to apply

**Commands:**
- `wifi show` / `wifi set <ssid> <pass>` / `wifi clear`
- `api show` / `api set-key <key>` / `api set-query <query>` / `api clear`
- `continue` / `exit` / `done` - exit config and boot
- `wifi reboot` / `api reboot` - save and reboot

## Setup Paths

**Option 1: BOOT button config (recommended)**
- Press BOOT button anytime, set credentials interactively

**Option 2: File-based config**
- Set credentials in `main/wifi_local.h`
- Build/flash with ESP-IDF

## No Known Issues

All previously tracked issues have been resolved.

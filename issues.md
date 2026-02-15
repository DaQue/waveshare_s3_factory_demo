# Project Status And Known Issues

## Current Version: 0.8.1

### Changes in 0.8.1
- Serial console completely overhauled for reliability
- Interactive mode: any valid command keeps console open until you type `continue`
- 15-second initial window with countdown (10, 5, 3, 2, 1)
- Character echo, backspace support, `> ` prompt
- CR and LF both work as Enter (fixes ESP-IDF monitor compatibility)
- API key now visible in `api show` output for verification
- Cleaner help output with exit instructions
- RX buffer increased to 512 bytes

### Previous (0.8.0)
- About screen version fix (uses runtime app metadata)
- Local API config template added

## Serial Console Usage

The serial console is now reliable and stays open as long as you need:

1. Boot the device, watch for `> ` prompt
2. Type any command (e.g., `wifi show`) to enter interactive mode
3. Configure as needed - no timeout while in interactive mode
4. Type `continue` to exit and boot normally, or `wifi reboot` to apply changes

**Commands:**
- `wifi show` / `wifi set <ssid> <pass>` / `wifi clear`
- `api show` / `api set-key <key>` / `api set-query <query>` / `api clear`
- `continue` / `exit` / `done` - exit console and boot
- `wifi reboot` / `api reboot` - save and reboot immediately

## Setup Paths

**Option 1: Serial console (recommended)**
- Boot and use interactive console as above

**Option 2: File-based config**
- Set credentials in `main/wifi_local.h`
- Build/flash with ESP-IDF

## No Known Issues

All previously tracked issues have been resolved.

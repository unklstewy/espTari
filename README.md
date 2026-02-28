# espTari

An Atari homecomputer emulator for the Waveshare ESP32-P4-NANO development board.

## Features

- **Modular Architecture**: Emulator components (CPU, Video, Audio, I/O) are dynamically loaded from EBIN files
- **Position-Independent Code**: Components compiled as position-independent code that can be loaded into PSRAM
- **ESP32-P4 Optimized**: Leverages the dual RISC-V cores @ 360MHz and 32MB PSRAM

## Hardware Target

- **Board**: Waveshare ESP32-P4-NANO
- **Processor**: ESP32-P4 (dual RISC-V cores @ 360MHz)
- **PSRAM**: 32MB
- **Flash**: 16MB

## Project Structure

- `components/esptari_loader/` - Dynamic EBIN component loader
- `tools/ebin_builder/` - Python tool to compile C sources to EBIN format
- `test_apps/esptari_loader_integration/` - Integration test for the loader

## Building

Requires ESP-IDF v5.4 or later.

```bash
# Build the integration test
cd test_apps/esptari_loader_integration
idf.py build flash -p /dev/ttyACM0

# Monitor output
idf.py -p /dev/ttyACM0 monitor
```

## EBIN Format

EBIN (Embedded Binary) is a custom format for position-independent components:
- 60-byte header with code/data/bss sizes and relocation info
- Position-independent code using PC-relative addressing  
- Relocations for data section pointers

## Notes on ESP32-P4 Dynamic Code Loading

To execute dynamically loaded code on ESP32-P4:

1. Use `CONFIG_ESP_SYSTEM_PMP_IDRAM_SPLIT=n` to enable `MALLOC_CAP_EXEC`
2. Compile with `-mno-relax` to prevent linker relaxation
3. Before execution, use `cache_hal_writeback_addr()` + `fence.i` for proper cache sync

# espTari Agent Guide

## AI Assistant Guidelines for espTari Development

This document establishes rules, conventions, and best practices for AI agents assisting with the espTari project development.

---

## Project Identity

**Project:** espTari - Atari ST/STe/Mega/Falcon Emulator  
**Platform:** Waveshare ESP32-P4-NANO  
**Toolchain:** ESP-IDF v5.x (`/home/sannis/esp/esp-idf`)  
**Examples Reference:** `/home/sannis/electronics/esp32p4/projects/espTari/examples`

---

## Core Principles

### 1. Code Quality

- **Write atomic, modular code** - Each component must have a single, well-defined responsibility
- **Maintain clear separation of concerns** - CPU, memory, video, audio, input, and web must be independent components
- **Prefer readability over cleverness** - Future maintenance is critical for complex emulation code
- **Document cycle-accurate behavior** - Comment timing-sensitive code with reference to hardware documentation

### 2. Architecture

```
espTari Architecture:

┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                       │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────────┐ │
│  │ Web UI  │  │ Config  │  │ State   │  │ Debug Console   │ │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────────┬────────┘ │
└───────┼────────────┼───────────┼─────────────────┼──────────┘
        │            │           │                 │
┌───────┴────────────┴───────────┴─────────────────┴──────────┐
│                     Emulation Core                          │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────────┐ │
│  │ CPU     │  │ Memory  │  │ Chips   │  │ Peripherals     │ │
│  │ (68000) │  │ Manager │  │ (Custom)│  │ (FDC,ACIA,MFP)  │ │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────────┬────────┘ │
└───────┼────────────┼───────────┼─────────────────┼──────────┘
        │            │           │                 │
┌───────┴────────────┴───────────┴─────────────────┴──────────┐
│                     Hardware Abstraction                    │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────────────┐ │
│  │ Display │  │ Audio   │  │ Storage │  │ Input           │ │
│  │ (DSI)   │  │ (I2S)   │  │ (SDMMC) │  │ (USB/Web)       │ │
│  └─────────┘  └─────────┘  └─────────┘  └─────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 3. Memory Efficiency

- **PSRAM is plentiful (32MB) but not unlimited** - Budget allocations carefully
- **Use static allocation where possible** - Avoid heap fragmentation
- **Implement proper cleanup** - Free resources when switching machine types
- **Cache hot data in L2MEM** - Use `__attribute__((section(".l2_mem")))` for critical paths

---

## Git Workflow

### Branch Naming Convention

```
main                    # Stable releases only
develop                 # Integration branch
feature/<name>          # New features
bugfix/<name>           # Bug fixes
refactor/<name>         # Code refactoring
docs/<name>             # Documentation updates
```

### Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat` - New feature
- `fix` - Bug fix
- `refactor` - Code refactoring
- `docs` - Documentation
- `test` - Tests
- `perf` - Performance improvement
- `chore` - Build/config changes

**Scopes:**
- `cpu` - CPU emulation
- `mem` - Memory system
- `video` - Video/frame buffer
- `audio` - Audio system
- `input` - Input handling
- `web` - Web interface
- `stream` - A/V streaming
- `network` - Network management (WiFi/Ethernet)
- `storage` - SD card/ROM management
- `ota` - Over-the-air updates
- `loader` - Dynamic component loading
- `machine` - Machine profiles/configuration
- `core` - Core framework

**Examples:**
```
feat(cpu): implement DIVU instruction with cycle accuracy
fix(video): correct palette conversion for STe mode
refactor(mem): optimize memory access dispatch
docs(api): add web API endpoint documentation
```

### Branch Creation Rules

**ALWAYS create a feature branch before implementing new functionality:**

```bash
# Create feature branch from develop
git checkout develop
git pull origin develop
git checkout -b feature/<feature-name>

# Work on feature...
git add .
git commit -m "feat(<scope>): <description>"

# Merge back to develop when complete
git checkout develop
git merge feature/<feature-name>
git push origin develop

# Delete feature branch
git branch -d feature/<feature-name>
```

---

## Coding Standards

### C Code Style

```c
/**
 * @file m68k_core.c
 * @brief MC68000 CPU emulation core
 *
 * Implements the main execution loop for the Motorola 68000 CPU.
 * Cycle timing is based on the MC68000 User Manual.
 */

#include "m68k.h"
#include "m68k_internal.h"

// Use snake_case for functions and variables
static int execute_instruction(m68k_cpu_t *cpu, uint16_t opcode);

// Constants use SCREAMING_SNAKE_CASE
#define M68K_CLOCK_HZ       8000000
#define CYCLES_PER_WORD     4

// Enums are prefixed with scope
typedef enum {
    M68K_FLAG_CARRY     = (1 << 0),
    M68K_FLAG_OVERFLOW  = (1 << 1),
    M68K_FLAG_ZERO      = (1 << 2),
    M68K_FLAG_NEGATIVE  = (1 << 3),
    M68K_FLAG_EXTEND    = (1 << 4),
} m68k_flags_t;

// Structs use _t suffix
typedef struct {
    uint32_t d[8];
    uint32_t a[8];
    uint32_t pc;
    uint16_t sr;
} m68k_cpu_t;

/**
 * @brief Execute a single instruction
 *
 * Fetches, decodes, and executes one instruction. Updates cycle count.
 *
 * @param cpu Pointer to CPU state
 * @param opcode Pre-fetched opcode
 * @return Number of cycles consumed
 *
 * @note Cycle timing is approximate; bus timing may vary.
 */
static int execute_instruction(m68k_cpu_t *cpu, uint16_t opcode)
{
    // Implementation
}
```

### Component Structure

Each component follows this structure:

```
components/
└── esptari_<component>/
    ├── CMakeLists.txt
    ├── Kconfig.projbuild         # Optional configuration
    ├── idf_component.yml         # Dependencies
    ├── include/
    │   └── esptari_<component>.h # Public API (minimal)
    ├── src/
    │   ├── <component>.c         # Public implementation
    │   └── <component>_internal.c # Internal helpers
    └── test/                      # Unit tests
        └── test_<component>.c
```

### CMakeLists.txt Template

```cmake
idf_component_register(
    SRCS 
        "src/component.c"
        "src/component_internal.c"
    INCLUDE_DIRS 
        "include"
    PRIV_INCLUDE_DIRS
        "src"
    REQUIRES 
        "esptari_core"
    PRIV_REQUIRES
        "esp_timer"
)

# Enable warnings
target_compile_options(${COMPONENT_LIB} PRIVATE 
    -Wall -Wextra -Werror
    -Wno-unused-parameter
)
```

---

## Emulation Guidelines

### Cycle Accuracy

```c
// Document cycle costs explicitly
// MC68000 User Manual Table 8-1

// MOVE.W D0,D1 - 4 cycles
// Data register to data register move
cpu->cycles += 4;

// MOVE.L (A0)+,D0 - 12 cycles  
// Memory read (4) + postincrement + long access (8)
cpu->cycles += 12;
```

### Memory Access

```c
// All memory access goes through dispatch
// Never access memory arrays directly from CPU code

uint16_t value = mem_read_word(cpu->pc);
cpu->pc += 2;  // Always manually increment PC

// For I/O regions, dispatch to chip handlers
void mem_write_byte(uint32_t addr, uint8_t val)
{
    if (addr >= 0xFF8000 && addr < 0xFFFFFF) {
        io_write(addr, val);
    } else if (addr < ram_size) {
        ram[addr] = val;
    }
}
```

### Interrupt Handling

```c
// Priority: 
// Level 7: NMI (non-maskable)
// Level 6: MFP
// Level 4: VBL
// Level 2: HBL
// Level 1: (unused on ST)

void check_interrupts(m68k_cpu_t *cpu)
{
    int pending = get_pending_interrupt();
    int mask = (cpu->sr >> 8) & 0x7;
    
    if (pending > mask || pending == 7) {
        take_interrupt(cpu, pending);
    }
}
```

---

## Web Interface Guidelines

### Important: Browser-Only Output

**All video and audio output is rendered in the web browser.** There is no physical display attached to the ESP32-P4-NANO. This means:

- Video frames are encoded and streamed via WebSocket
- Audio samples are streamed via WebSocket
- The browser uses Canvas/WebGL for video and Web Audio API for sound
- Low latency (<50ms) is critical for interactive use

### Streaming Protocol

```typescript
// WebSocket binary message format
const MESSAGE_TYPE = {
    VIDEO_FRAME: 0x01,
    AUDIO_CHUNK: 0x02,
    SYNC_PACKET: 0x03,
    INPUT_EVENT: 0x10,
};

// Video frame packet
interface VideoFrame {
    type: 0x01;
    timestamp: number;      // microseconds
    frameNumber: number;
    width: number;
    height: number;
    format: 'rgb565' | 'jpeg';
    data: ArrayBuffer;
}

// Audio chunk packet
interface AudioChunk {
    type: 0x02;
    timestamp: number;
    sampleRate: number;
    channels: number;
    samples: Int16Array;
}
```

### API Design

```typescript
// RESTful principles
// GET for read operations
// POST for actions (start, stop, reset)
// PUT for updates (configuration)
// DELETE for removal (mounted disks)

// Response format
interface ApiResponse<T> {
  success: boolean;
  data?: T;
  error?: string;
}

// Example endpoints
GET  /api/system          → { state: "running", machine: "stfm", ... }
POST /api/system/reset    → { success: true }
PUT  /api/config         → { success: true }
GET  /api/roms/tos       → { files: [...] }
GET  /api/network/status → { eth0: {...}, wlan0: {...} }
```

### Frontend Structure

```
frontend/src/
├── components/           # Reusable Vue components
│   ├── common/          # Generic UI components
│   ├── emulator/        # Emulator-specific components
│   └── config/          # Configuration components
├── composables/         # Vue composition API hooks
├── services/            # API and WebSocket clients
├── stores/              # Pinia state management
├── types/               # TypeScript interfaces
└── views/               # Page-level components
```

---

## Network Configuration Guidelines

### Overview

espTari supports dual network interfaces with automatic failover:
- **Ethernet** (internal MAC + external PHY) - Priority 1
- **WiFi 6** (via ESP32-C6 co-processor) - Priority 2 (fallback)

### Configuration Storage

Network configuration is stored in SPIFFS (not SD card) to ensure it's available before any other storage is mounted. This prevents race conditions during boot.

```yaml
# /spiffs/network.yaml (Netplan-style)
network:
  version: 1
  ethernets:
    eth0:
      dhcp4: true
  wifis:
    wlan0:
      dhcp4: true
      access-points:
        "NetworkSSID":
          password: "password"
```

### Network Manager API

```c
// Initialize network subsystem
esp_err_t network_init(void);

// Get interface status
network_status_t network_get_status(const char *interface);

// Check if any network is connected
bool network_is_connected(void);

// Get primary (active) interface
const char* network_get_primary_interface(void);

// Force interface switch
esp_err_t network_set_primary(const char *interface);
```

### Best Practices

1. **Always check network status** before starting HTTP/WebSocket servers
2. **Use mDNS** (`esptari.local`) for discovery - don't rely on IP addresses
3. **Handle reconnection** gracefully in streaming code
4. **Prefer Ethernet** when available for lower latency
5. **Store network config in SPIFFS**, emulator config on SD card

---

## OTA (Over-The-Air) Update Guidelines

### Overview

espTari supports Over-The-Air firmware updates via the web interface. The system uses A/B partitioning for safe, rollback-capable updates.

### Partition Layout

```
┌─────────────┐
│   ota_0     │ ← Active (or)
│   (3.5MB)   │
├─────────────┤
│   ota_1     │ ← Standby (receives update)
│   (3.5MB)   │
├─────────────┤
│   otadata   │ ← Boot selection
└─────────────┘
```

### OTA API

```c
// Check for update availability
esp_err_t ota_check_update(ota_update_info_t *info);

// Start OTA update from HTTP stream
esp_err_t ota_begin(size_t image_size);

// Write chunk of firmware data
esp_err_t ota_write(const void *data, size_t len);

// Finalize and verify update
esp_err_t ota_end(void);

// Mark current firmware as valid (call after successful boot)
esp_err_t ota_mark_valid(void);

// Rollback to previous firmware
esp_err_t ota_rollback(void);

// Get current firmware info
esp_err_t ota_get_info(ota_firmware_info_t *info);
```

### Web Upload Flow

```typescript
// Frontend OTA upload
async function uploadFirmware(file: File): Promise<void> {
    const formData = new FormData();
    formData.append('firmware', file);
    
    const response = await fetch('/api/ota/upload', {
        method: 'POST',
        body: formData,
        // Note: Don't set Content-Type, let browser set multipart boundary
    });
    
    if (response.ok) {
        // Device will reboot automatically
        await waitForReboot();
    }
}
```

### Safety Guidelines

1. **Always call `ota_mark_valid()`** after successful boot to prevent auto-rollback
2. **Verify firmware signature** before applying (if implemented)
3. **Check minimum version** - prevent downgrade attacks
4. **Preserve user settings** across updates (stored in NVS/SPIFFS, not app partition)
5. **Handle update interruption** - partial updates are safe due to A/B scheme
6. **Test rollback** - ensure previous firmware is always bootable

### Rollback Behavior

```c
// In app_main() after successful initialization:
void app_main(void) {
    // ... initialization code ...
    
    // If we reach here, boot was successful
    esp_ota_mark_app_valid_cancel_rollback();
    
    // If boot fails before this point, bootloader will
    // automatically roll back to previous firmware
}
```

### Version Information

Store version info in firmware for display and update checks:

```c
// In version.h
#define ESPTARI_VERSION_MAJOR 1
#define ESPTARI_VERSION_MINOR 0
#define ESPTARI_VERSION_PATCH 0
#define ESPTARI_VERSION_STRING "1.0.0"
#define ESPTARI_BUILD_DATE __DATE__
#define ESPTARI_BUILD_TIME __TIME__
```

---

## Dynamic Component Loading

espTari uses dynamic component loading to support multiple CPU architectures and machine configurations without increasing firmware size. Components are loaded from SD card into PSRAM at runtime.

### Component Types

| Type | Interface | Examples |
|------|-----------|----------|
| CPU | `cpu_interface_t` | MC68000, MC68030, MC68060 |
| Video | `video_interface_t` | Shifter ST, Shifter STe, VIDEL |
| Audio | `audio_interface_t` | YM2149, DMA Sound, DSP56001 |
| I/O | `io_interface_t` | MFP, Blitter, ACIA |

### Building Components

Components are built as Position-Independent Code (PIC) with a custom linker script:

```cmake
# cores/cpu_68000/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Use PIC compilation
add_library(cpu_68000 OBJECT
    src/m68k_core.c
    src/m68k_opcodes.c
    src/m68k_addressing.c
)

target_compile_options(cpu_68000 PRIVATE
    -fPIC
    -fno-common
    -ffunction-sections
    -fdata-sections
)

# Custom link to produce .ebin
add_custom_command(OUTPUT cpu_68000.ebin
    COMMAND ${EBIN_TOOL} 
        --input $<TARGET_OBJECTS:cpu_68000>
        --output cpu_68000.ebin
        --type cpu
        --interface-version 0x00010000
    DEPENDS cpu_68000
)
```

### Component Development Guidelines

1. **No global state** - All state must be in context structures passed to functions
2. **No direct hardware access** - Use provided bus interface for memory/IO
3. **Interface stability** - Interface version must increment for breaking changes
4. **Size awareness** - Components loaded to PSRAM; target <512KB per component
5. **Self-contained** - No external dependencies except provided interfaces

### Component Lifecycle

```c
// Component loading sequence:
1. loader_load_component("/sdcard/cores/cpu_68000.ebin", COMPONENT_CPU, &cpu_if);
2. cpu_if->init(config);        // Initialize component
3. cpu_if->reset();             // Reset to initial state
4. cpu_if->execute(cycles);     // Run emulation cycles
5. cpu_if->shutdown();          // Cleanup before unload
6. loader_unload_component(cpu_if);
```

### Machine Profile Format

Machine profiles are JSON files that define complete system configurations:

```json
{
    "machine": "atari_st",
    "display_name": "Atari 520ST",
    "year": 1985,
    "components": {
        "cpu": {"file": "cpu_68000.ebin", "clock_hz": 8000000},
        "video": {"file": "shifter_st.ebin"},
        "audio": [{"file": "ym2149.ebin"}],
        "mmu": {"file": "mmu_st.ebin"}
    },
    "memory": {
        "ram_kb": 512,
        "tos_file": "tos102.img"
    }
}
```

### Error Handling

```c
// Always check component loading results
esp_err_t err = loader_load_component(path, type, &interface);
if (err == ESP_ERR_NO_MEM) {
    // Out of PSRAM - unload something or report error
} else if (err == ESP_ERR_INVALID_VERSION) {
    // Component interface version mismatch
} else if (err == ESP_ERR_NOT_FOUND) {
    // Component file not found on SD card
}
```

---

## Testing Requirements

### Unit Tests

Each component must have unit tests for:
- Core functionality
- Edge cases
- Error handling

```c
// test/test_m68k.c
#include "unity.h"
#include "m68k.h"

void test_move_word_register_to_register(void)
{
    m68k_cpu_t cpu = {0};
    cpu.d[0] = 0x1234;
    
    // Execute MOVE.W D0,D1
    execute_opcode(&cpu, 0x3200);
    
    TEST_ASSERT_EQUAL_HEX16(0x1234, cpu.d[1] & 0xFFFF);
    TEST_ASSERT_EQUAL(4, cpu.cycles);
}
```

### Integration Tests

- TOS boot sequence must complete successfully
- GEM desktop must be usable
- Basic applications must run

---

## Performance Optimization

### Priorities

1. **CPU execution loop** - Most cycles spent here
2. **Memory access dispatch** - Called millions of times per second
3. **Video rendering** - Frame rate critical
4. **Audio generation** - Latency sensitive

### Profiling

```c
// Use ESP-IDF performance tools
#include "esp_timer.h"

int64_t start = esp_timer_get_time();
// ... code to profile ...
int64_t elapsed = esp_timer_get_time() - start;
ESP_LOGI(TAG, "Operation took %lld us", elapsed);
```

### Hot Path Rules

- Avoid function calls in tight loops (use inline or macros)
- Use lookup tables for opcode dispatch
- Pre-compute values where possible
- Consider IRAM placement for critical functions

---

## Error Handling

### Logging

```c
#include "esp_log.h"

static const char *TAG = "m68k";

// Levels: ERROR, WARN, INFO, DEBUG, VERBOSE
ESP_LOGE(TAG, "Invalid opcode: %04X at PC=%08X", opcode, pc);
ESP_LOGW(TAG, "Unimplemented instruction, using NOP");
ESP_LOGI(TAG, "CPU reset, PC=%08X", cpu->pc);
ESP_LOGD(TAG, "Executing %04X", opcode);
```

### Error Recovery

- Emulation errors should be recoverable (bad opcodes → trap)
- Hardware errors should fail gracefully (SD card removed → pause)
- Never crash the ESP32 due to emulation issues

---

## Documentation Requirements

### Code Comments

```c
/**
 * Required for:
 * - All public API functions
 * - Complex algorithms
 * - Non-obvious behavior
 * - Hardware-specific timing
 */

// Inline comments for:
// - Cycle counts
// - Register bit fields
// - Magic numbers with references
```

### External Documentation

- Update IMPLEMENTATION_PLAN.md when phases complete
- Maintain README.md for build instructions
- Document web API in api.md

---

## Dependency Management

### Allowed External Components

```yaml
# idf_component.yml
dependencies:
  idf: ">=5.3"
  
  # ESP-IDF components (allowed)
  espressif/mdns: "*"
  espressif/cjson: "*"
  
  # Display/UI (allowed)
  waveshare/esp32_p4_platform: "*"
  lvgl/lvgl: "^9"
  
  # DO NOT add external emulation libraries
  # All emulation code must be original or properly licensed
```

### License Compliance

- This project must respect all source licenses
- Reference implementations can be studied but not copied
- Document any code derived from reference materials

---

## Machine-Specific Notes

### Atari ST/STFM

- Base system, implement first
- 68000 @ 8MHz
- YM2149 PSG audio
- 512KB-4MB RAM

### Atari STe

- Enhanced palette (4096 colors)
- DMA audio
- Hardware scrolling
- Blitter chip
- Requires separate configuration

### Mega ST

- Same as ST but different form factor
- May have real-time clock
- Often has TT-style monitor connector

### Falcon

- Long-term goal
- 68030 CPU + DSP56001
- VIDEL video chip
- Complex audio (16-bit, matrix mixer)

---

## Commands Reference

### Build Commands

```bash
# Configure environment
source /home/sannis/esp/esp-idf/export.sh

# Build project
cd /home/sannis/electronics/esp32p4/projects/espTari
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash

# Monitor output
idf.py -p /dev/ttyUSB0 monitor

# Clean build
idf.py fullclean
```

### Frontend Development

```bash
cd frontend
npm install
npm run dev      # Development server
npm run build    # Production build
./compress_dist.sh  # Compress for embedding
```

---

## Agent Behavior Rules

### DO:

1. ✅ Create feature branches for new work
2. ✅ Write comprehensive commit messages
3. ✅ Follow existing code patterns and style
4. ✅ Add unit tests for new functionality
5. ✅ Update documentation when adding features
6. ✅ Consider cycle accuracy in CPU/chip emulation
7. ✅ Use the examples folder as reference for ESP32-P4 patterns
8. ✅ Validate memory accesses and handle edge cases
9. ✅ Profile performance-critical code
10. ✅ Keep components loosely coupled

### DON'T:

1. ❌ Commit directly to main or develop without review
2. ❌ Ignore cycle timing in emulation code
3. ❌ Copy code from other emulators without proper licensing
4. ❌ Add unnecessary dependencies
5. ❌ Break existing functionality without discussion
6. ❌ Ignore compiler warnings
7. ❌ Use global variables for component state
8. ❌ Hardcode configuration values
9. ❌ Skip error handling
10. ❌ Leave TODO comments without tracking issues

---

## Contact & Resources

### Project Resources

- **Implementation Plan:** `IMPLEMENTATION_PLAN.md`
- **Goals Document:** `espTari.goals`
- **Examples:** `examples/` directory

### External Resources

- ESP-IDF Documentation: https://docs.espressif.com/projects/esp-idf/
- Waveshare Wiki: https://www.waveshare.com/wiki/ESP32-P4-Nano-StartPage
- Atari TOS ROMs Collection: https://avtandil33.pythonanywhere.com/tose
- Atari Hardware Reference: (various)
- MC68000 Programmer's Reference: Motorola

---

*This guide should be updated as the project evolves. All contributors (human and AI) should follow these guidelines.*

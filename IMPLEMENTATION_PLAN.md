# espTari Implementation Plan

## Atari ST/STe/Mega/Falcon Emulator for ESP32-P4-NANO

**Version:** 1.0  
**Target Hardware:** Waveshare ESP32-P4-NANO Development Board  
**Last Updated:** February 2026

---

## Executive Summary

This plan outlines a phased approach to building cycle-accurate Atari home computer emulators for the ESP32-P4 platform. The project leverages the ESP32-P4's dual-core RISC-V processor (400MHz), 32MB PSRAM, and robust networking capabilities (WiFi 6 via ESP32-C6 co-processor + 100Mbps Ethernet) to create a fully-featured emulation system. **All video and audio output is rendered exclusively in the web browser** using low-latency streaming protocols, eliminating the need for a physical display.

---

## Target Systems

| System | CPU | Year | Complexity |
|--------|-----|------|------------|
| Atari ST | MC68000 @ 8MHz | 1985 | Medium |
| Atari STM | MC68000 @ 8MHz | 1986 | Medium |
| Atari STFM | MC68000 @ 8MHz | 1986 | Medium |
| Atari Mega ST | MC68000 @ 8MHz | 1987 | Medium-High |
| Atari STe | MC68000 @ 8MHz | 1989 | High |
| Atari TT | MC68030 @ 32MHz | 1990 | Very High |
| Atari Falcon | MC68030 @ 16MHz + DSP56001 | 1992 | Extreme |

---

## Phase Overview

```
Phase 0: Foundation Setup (Week 1-2)
    ↓
Phase 0.5: Network Interface Manager (Week 2-3)
    ↓
Phase 1: Core CPU Emulation - MC68000 (Week 4-7)
    ↓
Phase 2: Basic ST Hardware (Week 8-11)
    ↓
Phase 3: Web Streaming (Video + Audio) (Week 12-16)
    ↓
Phase 4: Web Interface & Configuration (Week 17-20)
    ↓
Phase 5: Storage & ROM Management (Week 21-23)
    ↓
Phase 6: Input Handling (Week 24-26)
    ↓
Phase 7: Enhanced Hardware - STe (Week 27-30)
    ↓
Phase 8: Advanced Systems (Week 31+)
```

---

## Phase 0: Foundation Setup

### Goals
- Initialize project structure
- Set up build system
- Establish coding standards
- Configure Git workflow

### Tasks

#### 0.1 Project Scaffolding
```
espTari/
├── CMakeLists.txt
├── sdkconfig.defaults
├── sdkconfig.defaults.esp32p4
├── partitions.csv
├── components/
│   ├── esptari_core/           # Core emulation framework
│   ├── esptari_loader/         # Dynamic component loader (CPU, MMU, etc.)
│   ├── esptari_memory/         # Memory management
│   ├── esptari_video/          # Video subsystem (frame generation)
│   ├── esptari_audio/          # Audio subsystem (sample generation)
│   ├── esptari_storage/        # SD card / ROM management
│   ├── esptari_input/          # Input handling
│   ├── esptari_web/            # Web interface & streaming
│   ├── esptari_network/        # Network interface manager
│   └── esptari_stream/         # Low-latency A/V streaming
├── main/
│   ├── CMakeLists.txt
│   ├── main.c
│   ├── Kconfig.projbuild
│   └── idf_component.yml
├── frontend/                    # Vue.js web interface
│   ├── src/
│   ├── public/
│   └── vite.config.mts
├── spiffs/                      # SPIFFS partition contents
│   └── network.yaml             # Network configuration (Netplan-style)
├── cores/                       # Source for dynamic components (built separately)
│   ├── cpu/
│   │   ├── m68000/              # MC68000 CPU core
│   │   ├── m68010/              # MC68010 CPU core
│   │   ├── m68020/              # MC68020 CPU core
│   │   ├── m68030/              # MC68030 CPU core
│   │   ├── m68040/              # MC68040 CPU core
│   │   └── m68060/              # MC68060 CPU core (accelerators)
│   ├── mmu/
│   │   ├── st_mmu/              # Basic ST MMU
│   │   ├── ste_mmu/             # STe MMU
│   │   └── falcon_mmu/          # Falcon/TT MMU
│   ├── video/
│   │   ├── shifter/             # ST Shifter
│   │   ├── ste_shifter/         # STe Enhanced Shifter
│   │   └── videl/               # Falcon VIDEL
│   ├── audio/
│   │   ├── ym2149/              # YM2149 PSG
│   │   ├── dma_audio/           # STe DMA audio
│   │   └── dsp56001/            # Falcon DSP
│   └── misc/
│       ├── blitter/             # STe/TT/Falcon Blitter
│       ├── fdc_wd1772/          # Floppy controller
│       └── mfp68901/            # MFP chip
└── sdcard/                      # SD card structure template
    ├── cores/                   # Compiled dynamic components (.ebin)
    │   ├── cpu/
    │   ├── mmu/
    │   ├── video/
    │   ├── audio/
    │   └── misc/
    ├── machines/                # Machine definitions (JSON)
    │   ├── st.json
    │   ├── stfm.json
    │   ├── ste.json
    │   ├── mega_st.json
    │   ├── mega_ste.json
    │   ├── tt030.json
    │   └── falcon030.json
    ├── roms/
    │   ├── tos/
    │   ├── cartridges/
    │   └── bios/
    ├── disks/
    │   ├── floppy/
    │   └── hard/
    ├── config/
    ├── states/
    └── screenshots/
```

#### 0.2 Partition Table
```csv
# Name,    Type, SubType,  Offset,   Size,    Flags
nvs,       data, nvs,      0x9000,   0x6000,
otadata,   data, ota,      0xf000,   0x2000,         # OTA data (boot selection)
phy_init,  data, phy,      0x11000,  0x1000,
spiffs,    data, spiffs,   0x12000,  0x20000,        # 128KB for network config
ota_0,     app,  ota_0,    0x32000,  0x380000,       # 3.5MB for app slot 0
ota_1,     app,  ota_1,    0x3B2000, 0x380000,       # 3.5MB for app slot 1
storage,   data, fat,      0x732000, 0x8CE000,       # ~9MB for storage
```

**OTA Partition Layout:**
- Two equal-sized app partitions (ota_0, ota_1) for A/B updates
- `otadata` partition tracks which slot is active
- Rollback supported if new firmware fails to boot

#### 0.3 Build Configuration
- Configure sdkconfig.defaults for ESP32-P4:
  - Enable PSRAM (32MB @ 200MHz)
  - Enable L2 cache (256KB)
  - Set compiler optimization for performance
  - Configure custom partition table (16MB flash)
  - Enable SPIFFS for network configuration
  - Enable WiFi (via ESP32-C6 co-processor)
  - Enable Ethernet (internal MAC + PHY)
  - **Enable OTA support with rollback**

#### 0.4 Git Workflow
- Main branch: `main` (stable releases)
- Development branch: `develop`
- Feature branches: `feature/<feature-name>`
- Release branches: `release/<version>`
- Hotfix branches: `hotfix/<issue>`

### Deliverables
- [ ] Working build system with `idf.py build`
- [ ] Empty component skeletons
- [ ] SPIFFS partition with default network.yaml
- [ ] Git repository initialized with branch strategy
- [ ] Basic README.md with build instructions

---

## Phase 0.5: Network Interface Manager

### Goals
- Implement dual-interface network management (WiFi 6 + Ethernet)
- Create SPIFFS-based configuration system
- Build Netplan-style YAML configuration parser
- Implement automatic failover and priority routing

### Network Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Network Interface Manager                  │
├─────────────┬─────────────┬─────────────┬───────────────────┤
│  Config     │  Interface  │  DHCP/      │  mDNS             │
│  Parser     │  Monitor    │  Static IP  │  Responder        │
└──────┬──────┴──────┬──────┴──────┬──────┴─────────┬─────────┘
       │             │             │                │
       ▼             ▼             ▼                ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│   SPIFFS    │ │  Ethernet   │ │   WiFi 6    │ │   DNS-SD    │
│ (YAML cfg)  │ │  (Internal) │ │  (ESP32-C6) │ │  Services   │
└─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘
```

### Configuration Format (Netplan-style YAML)

```yaml
# /spiffs/network.yaml
network:
  version: 1
  renderer: esptari
  
  ethernets:
    eth0:
      dhcp4: true
      optional: false
      # Static IP alternative:
      # addresses:
      #   - 192.168.1.100/24
      # gateway4: 192.168.1.1
      # nameservers:
      #   addresses: [8.8.8.8, 8.8.4.4]
  
  wifis:
    wlan0:
      dhcp4: true
      optional: true
      access-points:
        "MyNetwork":
          password: "secretpassword"
        "BackupNetwork":
          password: "backuppassword"
      # Priority: lower number = higher priority
      # Ethernet is always priority 0 when connected
      priority: 10

  routing:
    # Prefer ethernet when both connected
    default-interface: eth0
    failover: true
    failover-timeout-ms: 5000

  services:
    mdns:
      enabled: true
      hostname: esptari
      # Advertised services
      services:
        - type: _http._tcp
          port: 80
          txt:
            - "path=/"
        - type: _esptari._tcp
          port: 8080
          txt:
            - "version=1.0"
```

### Tasks

#### 0.5.1 SPIFFS Setup
```c
// Initialize SPIFFS for network configuration
esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = "spiffs",
    .max_files = 5,
    .format_if_mount_failed = true
};
esp_vfs_spiffs_register(&conf);
```

#### 0.5.2 YAML Parser
- Lightweight YAML parser for configuration
- Support subset needed for network configuration
- Graceful handling of missing/malformed config

#### 0.5.3 Ethernet Interface
```c
// Internal Ethernet MAC + External PHY (IP101/RTL8201)
eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
```

#### 0.5.4 WiFi Interface (via ESP32-C6 Co-processor)
```c
// WiFi via SDIO to ESP32-C6
// Uses esp_wifi_remote / esp_hosted component
esp_hosted_init();
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
esp_wifi_init(&cfg);
esp_wifi_set_mode(WIFI_MODE_STA);
```

#### 0.5.5 Interface Priority & Failover
- Ethernet takes priority when cable connected
- Automatic WiFi fallback on Ethernet disconnect
- Configurable failover timeout
- Event-driven interface state tracking

#### 0.5.6 mDNS Service Advertisement
```c
// Advertise espTari on local network
mdns_init();
mdns_hostname_set("esptari");
mdns_instance_name_set("espTari Emulator");
mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
mdns_service_add(NULL, "_esptari", "_tcp", 8080, NULL, 0);
```

### Web Configuration API

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/network/status` | GET | Current network status (interfaces, IPs) |
| `/api/network/config` | GET | Current network configuration |
| `/api/network/config` | PUT | Update network configuration |
| `/api/network/scan` | GET | Scan for WiFi networks |
| `/api/network/apply` | POST | Apply configuration changes |

### Deliverables
- [ ] SPIFFS partition mounted and accessible
- [ ] YAML configuration parser
- [ ] Ethernet interface management
- [ ] WiFi interface management (via ESP32-C6)
- [ ] Automatic failover between interfaces
- [ ] mDNS service advertisement
- [ ] Web API for network configuration

---

## Phase 1: Dynamic Component Loader & CPU Architecture

### Goals
- Create binary component loader for dynamic CPU/chip loading from SD card
- Define standard component interfaces for CPUs, video, audio, and I/O chips
- Implement component registry and lifecycle management
- Support hot-swapping components without firmware reflash

### Rationale

Loading emulation components dynamically from SD card provides:
- **Reduced firmware size**: Core firmware stays under 3.5MB for A/B OTA
- **Flexible CPU support**: MC68000 through MC68060 without firmware changes
- **Machine profiles**: Different Atari machines load different component sets
- **Easy updates**: Update individual components without full OTA
- **Development agility**: Test new component versions by copying to SD card

### Supported CPUs (via Dynamic Loading)

| CPU | Target Machines | Component File |
|-----|-----------------|----------------|
| MC68000 | ST, STM, STFM, STe | `cpu_68000.ebin` |
| MC68010 | Mega ST (optional) | `cpu_68010.ebin` |
| MC68020 | TT030 (bus interface) | `cpu_68020.ebin` |
| MC68030 | TT030, Falcon030 | `cpu_68030.ebin` |
| MC68040 | Afterburner040 | `cpu_68040.ebin` |
| MC68060 | Afterburner060 | `cpu_68060.ebin` |

### Component Binary Format (.ebin)

```
┌────────────────────────────────────────────────┐
│ EBIN Header (64 bytes)                         │
├────────────────────────────────────────────────┤
│ Magic: "EBIN" (4 bytes)                        │
│ Version: uint16_t                              │
│ Type: uint16_t (CPU=1, VIDEO=2, AUDIO=3, IO=4) │
│ Flags: uint32_t                                │
│ Code Size: uint32_t                            │
│ Data Size: uint32_t                            │
│ BSS Size: uint32_t                             │
│ Entry Offset: uint32_t                         │
│ Interface Version: uint32_t                    │
│ Min RAM: uint32_t (bytes required)             │
│ Reserved: 28 bytes                             │
├────────────────────────────────────────────────┤
│ Relocation Table                               │
│ - Count: uint32_t                              │
│ - Entries: [offset: uint32_t, type: uint8_t]   │
├────────────────────────────────────────────────┤
│ Code Section (Position Independent)            │
├────────────────────────────────────────────────┤
│ Data Section                                   │
├────────────────────────────────────────────────┤
│ Symbol Table (optional, for debugging)         │
└────────────────────────────────────────────────┘
```

### Component Interfaces

#### 1.1 CPU Interface
```c
// Standard CPU component interface
typedef struct {
    uint32_t interface_version;     // Must be CPU_INTERFACE_V1
    const char *name;               // "MC68000", "MC68030", etc.
    uint32_t features;              // Feature flags
    
    // Lifecycle
    int  (*init)(void *config);
    void (*reset)(void);
    void (*shutdown)(void);
    
    // Execution
    int  (*execute)(int cycles);    // Returns cycles consumed
    void (*stop)(void);
    
    // State
    void (*get_state)(cpu_state_t *state);
    void (*set_state)(const cpu_state_t *state);
    
    // Interrupts
    void (*set_irq)(int level);
    void (*set_nmi)(void);
    
    // Bus interface (set by loader)
    void (*set_bus)(bus_interface_t *bus);
    
    // Debug (optional)
    int  (*disassemble)(uint32_t pc, char *buf, int len);
    void (*set_breakpoint)(uint32_t addr);
} cpu_interface_t;

#define CPU_INTERFACE_V1 0x00010000
```

#### 1.2 Video Interface
```c
// Standard video component interface (Shifter, VIDEL, etc.)
typedef struct {
    uint32_t interface_version;
    const char *name;
    
    int  (*init)(void *config);
    void (*reset)(void);
    void (*shutdown)(void);
    
    // Rendering
    void (*render_scanline)(int line, uint8_t *buffer);
    void (*render_frame)(uint8_t *framebuffer);
    
    // Timing
    int  (*get_hpos)(void);
    int  (*get_vpos)(void);
    bool (*in_vblank)(void);
    bool (*in_hblank)(void);
    
    // Register access
    uint16_t (*read_reg)(uint32_t addr);
    void     (*write_reg)(uint32_t addr, uint16_t val);
    
    // Mode info
    void (*get_mode)(video_mode_t *mode);
} video_interface_t;

#define VIDEO_INTERFACE_V1 0x00010000
```

#### 1.3 Audio Interface
```c
// Standard audio component interface (YM2149, DMA Sound, DSP)
typedef struct {
    uint32_t interface_version;
    const char *name;
    
    int  (*init)(uint32_t sample_rate);
    void (*reset)(void);
    void (*shutdown)(void);
    
    // Audio generation
    void (*generate)(int16_t *buffer, int samples);
    
    // Register access
    uint8_t (*read_reg)(uint32_t addr);
    void    (*write_reg)(uint32_t addr, uint8_t val);
    
    // Timing
    void (*clock)(int cycles);
} audio_interface_t;

#define AUDIO_INTERFACE_V1 0x00010000
```

### Machine Configuration

Machine profiles define which components to load:

```json
{
    "machine": "atari_ste",
    "display_name": "Atari STe",
    "description": "Atari STe with enhanced features",
    "components": {
        "cpu": {
            "file": "cpu_68000.ebin",
            "clock_hz": 8000000
        },
        "mmu": {
            "file": "mmu_ste.ebin",
            "ram_size": 4194304
        },
        "video": {
            "file": "shifter_ste.ebin"
        },
        "audio": [
            {"file": "ym2149.ebin", "role": "psg"},
            {"file": "dma_sound.ebin", "role": "dma"}
        ],
        "blitter": {
            "file": "blitter.ebin"
        }
    },
    "memory": {
        "ram_kb": 4096,
        "tos_file": "tos206.img"
    }
}
```

### Component Loader Architecture

```
esptari_loader/
├── CMakeLists.txt
├── include/
│   ├── ebin_format.h       # Binary format definitions
│   ├── component_api.h     # Component interfaces
│   ├── loader.h            # Loader public API
│   └── machine.h           # Machine configuration
├── src/
│   ├── ebin_parser.c       # Parse .ebin files
│   ├── relocator.c         # Relocation handling
│   ├── loader.c            # Load to PSRAM, link
│   ├── registry.c          # Component registry
│   └── machine_config.c    # JSON machine parser
└── test/
    └── loader_test.c       # Unit tests
```

### Tasks

#### 1.1 EBIN Parser
- Validate header magic and version
- Parse relocation table
- Verify code/data sizes fit in PSRAM
- Support versioned interface checking

#### 1.2 Component Loader
```c
// Load component from SD card to PSRAM
esp_err_t loader_load_component(
    const char *path,           // "/sdcard/cores/cpu_68000.ebin"
    component_type_t type,      // COMPONENT_CPU
    void **interface_out        // Returns cpu_interface_t*
);

// Unload component, free PSRAM
esp_err_t loader_unload_component(void *interface);

// Get loaded component info
esp_err_t loader_get_info(void *interface, component_info_t *info);
```

#### 1.3 Machine Profile Loader
```c
// Load machine configuration
esp_err_t machine_load(const char *profile);  // "atari_ste"

// Get active interfaces
cpu_interface_t   *machine_get_cpu(void);
video_interface_t *machine_get_video(void);
audio_interface_t *machine_get_audio(int index);

// Hot-swap component (e.g., accelerator board)
esp_err_t machine_swap_cpu(const char *cpu_file);
```

#### 1.4 PSRAM Memory Management
- Allocate contiguous blocks for component code
- Handle alignment requirements (4-byte code, 8-byte data)
- Track allocated regions for cleanup
- Reserve space for component working memory

### Web API Endpoints (Component Management)

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/components` | GET | List loaded components |
| `/api/components/available` | GET | List components on SD card |
| `/api/machines` | GET | List available machine profiles |
| `/api/machine/current` | GET | Current machine configuration |
| `/api/machine/load` | POST | Load a machine profile |
| `/api/machine/swap` | POST | Hot-swap a component |

### Deliverables
- [ ] EBIN binary format specification
- [ ] EBIN parser with validation
- [ ] Relocation and linking to PSRAM
- [ ] Component registry with lifecycle
- [ ] Machine profile JSON parser
- [ ] CPU interface adapter for existing code
- [ ] Web API for component management
- [ ] Unit tests for loader

---

## Phase 2: Basic ST Hardware

### Goals
- Implement ST memory map
- Create basic chip emulation (MMU, GLUE, Shifter)
- Build interrupt controller
- Implement MFP 68901

### Memory Map (Atari ST)

```
$000000-$0007FF   Exception vectors & system variables
$000800-$DFFFFF   RAM (up to 4MB, typically 512KB-4MB)
$E00000-$E3FFFF   Reserved
$E40000-$EBFFFF   Reserved
$F00000-$F7FFFF   Reserved
$F80000-$FBFFFF   Reserved  
$FC0000-$FEFFFF   TOS ROM (192KB on most STs)
$FF0000-$FFFFFF   I/O area
```

### I/O Space ($FF8000-$FFFFFF)

| Address | Name | Description |
|---------|------|-------------|
| $FF8001 | MMU | Memory configuration |
| $FF8200-$FF820F | Shifter Video | Video controller |
| $FF8240-$FF825F | Shifter Palette | Color palette |
| $FF8260 | Shifter Resolution | Resolution register |
| $FF8604-$FF860F | FDC/HDC | Floppy/Hard disk |
| $FF8800-$FF8803 | YM2149 PSG | Sound chip |
| $FFFA00-$FFFA3F | MFP 68901 | Multi-function peripheral |
| $FFFC00-$FFFC06 | ACIA 6850 | Keyboard/MIDI |

### Tasks

#### 2.1 Memory System
```c
// Memory regions
typedef struct {
    uint8_t *ram;           // Main RAM (PSRAM)
    uint8_t *rom;           // TOS ROM (from SD card)
    uint8_t *cartridge;     // Cartridge ROM (optional)
} esptari_memory_t;

// Memory access callbacks
uint8_t  mem_read_byte(uint32_t addr);
uint16_t mem_read_word(uint32_t addr);
uint32_t mem_read_long(uint32_t addr);
void     mem_write_byte(uint32_t addr, uint8_t val);
void     mem_write_word(uint32_t addr, uint16_t val);
void     mem_write_long(uint32_t addr, uint32_t val);
```

#### 2.2 GLUE Chip (Interrupt Controller)
- HBL interrupt (line 2)
- VBL interrupt (line 4)
- MFP interrupt (line 6)
- Timer synchronization

#### 2.3 Shifter (Video Controller)
- Video base address
- Horizontal/vertical counters
- Resolution control (320x200x16, 640x200x4, 640x400x2)
- Palette registers

#### 2.4 MFP 68901
- 8 GPIO pins
- 4 timers (A-D)
- USART
- Interrupt controller (16 sources)

#### 2.5 YM2149 PSG
- 3 square wave channels
- 1 noise generator
- Envelope generator
- I/O ports (FDC control, parallel port)

### Deliverables
- [ ] Complete memory map implementation
- [ ] Working GLUE chip with interrupts
- [ ] Basic Shifter (video timing only)
- [ ] MFP with timers and interrupts
- [ ] YM2149 register emulation

---

## Phase 3: Web Streaming (Video + Audio)

### Goals
- Implement low-latency video streaming to web browser
- Stream synchronized audio to web browser
- Achieve target latency <50ms for interactive use
- Support multiple streaming protocols for compatibility

### Streaming Architecture

**All display and audio output is rendered exclusively in the web browser.**

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Shifter        │     │  Frame Buffer   │     │  Video Encoder  │
│  Emulation      │────▶│  (PSRAM RGB)    │────▶│  (H.264/JPEG)   │
└─────────────────┘     └─────────────────┘     └────────┬────────┘
                                                      │
┌─────────────────┐     ┌─────────────────┐     ┌────────▼────────┐
│  YM2149 PSG     │     │  Audio Buffer   │     │  Stream Muxer   │
│  + DMA Audio    │────▶│  (PCM samples)  │────▶│  (A/V sync)     │
└─────────────────┘     └─────────────────┘     └────────┬────────┘
                                                      │
                                                      ▼
                                          ┌─────────────────────────┐
                                          │   Web Browser Client    │
                                          │   (Canvas + Web Audio)  │
                                          └─────────────────────────┘
```

### Streaming Protocol Options

| Protocol | Latency | Audio | Complexity | Browser Support |
|----------|---------|-------|------------|----------------|
| **WebSocket + Canvas** | ~50-100ms | Separate | Low | Excellent |
| **WebRTC** | ~20-50ms | Integrated | High | Good |
| **MJPEG + WebSocket Audio** | ~100-200ms | Separate | Low | Excellent |
| **MSE (Media Source Ext)** | ~200-500ms | Integrated | Medium | Good |

**Recommended: Dual-protocol approach**
1. **Primary:** WebSocket + Raw frames + Web Audio API (lowest latency, best control)
2. **Fallback:** MJPEG + WebSocket Audio (maximum compatibility)

### Tasks

#### 3.1 Frame Buffer Management
```c
// Double-buffered frame buffer in PSRAM
typedef struct {
    uint8_t *buffer[2];         // RGB565 or RGB888 frames
    uint8_t  current;           // Current write buffer
    uint32_t width;             // Current resolution width
    uint32_t height;            // Current resolution height
    uint32_t frame_number;      // Frame counter for sync
    int64_t  timestamp_us;      // Frame timestamp
} framebuffer_t;

// ST resolutions (scaled 2x for web display)
// Low:    320x200 -> 640x400 (16 colors)
// Medium: 640x200 -> 640x400 (4 colors)  
// High:   640x400 -> 640x400 (2 colors)
```

#### 3.2 Video Encoding

**Option A: Hardware JPEG (ESP32-P4 built-in)**
```c
// Use ESP32-P4 hardware JPEG encoder for each frame
jpeg_encoder_config_t config = {
    .src_type = JPEG_ENCODE_IN_FORMAT_RGB888,
    .sub_sample = JPEG_DOWN_SAMPLING_YUV420,
    .quality = 80,  // Adjustable 1-100
};
jpeg_encoder_handle_t encoder;
esp_jpeg_new_encoder(&config, &encoder);
```

**Option B: Raw RGB via WebSocket**
```c
// Send raw RGB565 frames (lower latency, higher bandwidth)
// 640x400 @ RGB565 = 512KB per frame
// At 50fps = 25.6 MB/s (requires good network)
typedef struct {
    uint32_t frame_number;
    uint16_t width;
    uint16_t height;
    uint8_t  format;  // 0=RGB565, 1=RGB888
    uint8_t  data[];  // Pixel data
} raw_frame_packet_t;
```

#### 3.3 Audio Streaming

```c
// Audio buffer for web streaming
typedef struct {
    int16_t *buffer;            // PCM samples (stereo)
    uint32_t sample_rate;       // 44100 or 48000 Hz
    uint32_t samples_per_frame; // ~882 samples @ 50fps/44.1kHz
    uint32_t write_pos;
    uint32_t read_pos;
} audio_stream_t;

// Opus encoding for efficient audio streaming (optional)
// Or send raw PCM via WebSocket for lowest latency
```

#### 3.4 WebSocket Streaming Protocol

```typescript
// Client-side (TypeScript)
interface StreamMessage {
    type: 'video' | 'audio' | 'sync';
    timestamp: number;
    frameNumber?: number;
    data: ArrayBuffer;
}

// Video frames: binary WebSocket messages
// [1 byte type][4 byte timestamp][4 byte frame#][2 byte width][2 byte height][pixel data]

// Audio chunks: binary WebSocket messages  
// [1 byte type][4 byte timestamp][4 byte sample count][PCM samples]
```

#### 3.5 Browser Rendering (Canvas + Web Audio)

```typescript
// Frontend video rendering
const canvas = document.getElementById('display') as HTMLCanvasElement;
const ctx = canvas.getContext('2d')!;

function renderFrame(pixels: Uint8ClampedArray, width: number, height: number) {
    const imageData = new ImageData(pixels, width, height);
    ctx.putImageData(imageData, 0, 0);
}

// Frontend audio playback
const audioContext = new AudioContext({ sampleRate: 44100 });
const scriptNode = audioContext.createScriptProcessor(1024, 0, 2);

scriptNode.onaudioprocess = (e) => {
    const left = e.outputBuffer.getChannelData(0);
    const right = e.outputBuffer.getChannelData(1);
    // Fill from audio buffer queue
    fillFromBuffer(left, right);
};
scriptNode.connect(audioContext.destination);
```

#### 3.6 A/V Synchronization

```c
// Frame timing for 50Hz PAL / 60Hz NTSC
typedef struct {
    int64_t video_pts;          // Video presentation timestamp
    int64_t audio_pts;          // Audio presentation timestamp  
    int64_t frame_duration_us;  // 20000us for 50Hz, 16667us for 60Hz
} av_sync_t;

// Sync audio to video frames
void sync_av_stream(av_sync_t *sync) {
    int64_t drift = sync->audio_pts - sync->video_pts;
    if (abs(drift) > 5000) {  // >5ms drift
        // Adjust audio buffer read position
        adjust_audio_sync(drift);
    }
}
```

### Video Modes

| Mode | ST Resolution | Stream Resolution | Colors | Frame Rate |
|------|---------------|-------------------|--------|------------|
| Low | 320×200 | 640×400 (2x) | 16 | 50/60 Hz |
| Medium | 640×200 | 640×400 | 4 | 50/60 Hz |
| High | 640×400 | 640×400 | 2 | ~71 Hz |

#### 3.7 Palette Conversion
```c
// ST palette: 3-bit per component (512 colors)
// STe palette: 4-bit per component (4096 colors)
// Convert to 24-bit RGB for streaming
uint32_t st_to_rgb(uint16_t st_color) {
    uint8_t r = ((st_color >> 8) & 0x7) * 36;  // 0-252
    uint8_t g = ((st_color >> 4) & 0x7) * 36;
    uint8_t b = ((st_color >> 0) & 0x7) * 36;
    return (r << 16) | (g << 8) | b;
}

uint32_t ste_to_rgb(uint16_t ste_color) {
    uint8_t r = ((ste_color >> 8) & 0xF) * 17;  // 0-255
    uint8_t g = ((ste_color >> 4) & 0xF) * 17;
    uint8_t b = ((ste_color >> 0) & 0xF) * 17;
    return (r << 16) | (g << 8) | b;
}
```

#### 3.8 YM2149 Audio Rendering
```c
// Generate samples for streaming (per emulated frame)
void ym2149_render(int16_t *buffer, int samples) {
    for (int i = 0; i < samples; i++) {
        int16_t output = 0;
        // Mix three channels + noise
        output += channel_a_sample();
        output += channel_b_sample();
        output += channel_c_sample();
        buffer[i * 2] = output;      // Left
        buffer[i * 2 + 1] = output;  // Right
    }
}
```

### Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Video Latency | <50ms | End-to-end |
| Audio Latency | <30ms | Critical for games |
| Frame Rate | 50/60 fps | Match emulated system |
| Bandwidth | <5 Mbps | For JPEG stream |
| CPU Usage | <50% | Leave room for emulation |

### Deliverables
- [ ] Frame buffer management with double-buffering
- [ ] All ST video modes supported
- [ ] WebSocket video streaming
- [ ] WebSocket audio streaming
- [ ] A/V synchronization
- [ ] Browser canvas rendering
- [ ] Web Audio API playback
- [ ] Adaptive quality based on network conditions

---

## Phase 4: Web Interface & Configuration

### Goals
- Create configuration menu system
- **Primary display output via web browser**
- Build ROM/disk management interface
- Add system status monitoring
- Implement CRT visual effects (optional)

### Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   Frontend (Vue.js 3)                   │
├─────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────┐ │
│  │              Display Canvas (Primary Output)          │ │
│  │     ┌───────────────────────────────────────────┐  │ │
│  │     │  Video Stream (WebSocket/Canvas)         │  │ │
│  │     │  + Optional CRT Shader Effects           │  │ │
│  │     │  + Audio Playback (Web Audio API)        │  │ │
│  │     └───────────────────────────────────────────┘  │ │
│  └─────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────┤
│ Configuration │ File Manager │ Network Setup │ Status     │
└───────────────┴──────────────┴───────────────┴────────────┘
         │              │               │            │
         ▼              ▼               ▼            ▼
┌─────────────────────────────────────────────────────────┐
│                      REST + WebSocket API                  │
├─────────────────────────────────────────────────────────┤
│ /api/*     │ /ws/stream  │ /ws/input   │ /api/network  │
└────────────┴─────────────┴─────────────┴───────────────┘
```

### API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/api/system` | GET | Get current emulation state |
| `/api/system` | POST | Control (start/stop/reset) |
| `/api/config` | GET | Get configuration |
| `/api/config` | PUT | Update configuration |
| `/api/config/machine` | GET/PUT | Machine type selection |
| `/api/roms` | GET | List available ROMs |
| `/api/roms/{type}` | GET | List ROMs by type (TOS, etc.) |
| `/api/disks` | GET | List disk images |
| `/api/disks/mount` | POST | Mount disk image |
| `/api/network/status` | GET | Network interface status |
| `/api/network/config` | GET/PUT | Network configuration |
| `/api/network/scan` | GET | Scan WiFi networks |
| `/api/ota/status` | GET | Current firmware info & update status |
| `/api/ota/upload` | POST | Upload new firmware (multipart) |
| `/api/ota/rollback` | POST | Rollback to previous firmware |
| `/ws/stream` | WebSocket | A/V streaming endpoint |
| `/ws/input` | WebSocket | Input events (keyboard/mouse/joystick) |

### Frontend Structure

```
frontend/
├── src/
│   ├── main.ts
│   ├── App.vue
│   ├── components/
│   │   ├── MenuBar.vue
│   │   ├── DisplayCanvas.vue      # Primary video output
│   │   ├── AudioPlayer.vue        # Web Audio integration
│   │   ├── ConfigPanel.vue
│   │   ├── FileManager.vue
│   │   ├── NetworkConfig.vue       # Network setup UI
│   │   ├── OtaUpdate.vue           # Firmware update UI
│   │   ├── StatusBar.vue
│   │   ├── CRTEffect.vue           # Optional CRT shader
│   │   └── VirtualKeyboard.vue     # On-screen keyboard
│   ├── composables/
│   │   ├── useWebSocket.ts
│   │   ├── useVideoStream.ts
│   │   ├── useAudioStream.ts
│   │   └── useInput.ts
│   ├── pages/
│   │   ├── EmulatorPage.vue
│   │   ├── ConfigPage.vue
│   │   ├── NetworkPage.vue
│   │   ├── FilesPage.vue
│   │   └── UpdatePage.vue          # OTA firmware update page
│   ├── services/
│   │   ├── api.ts
│   │   ├── stream.ts               # Video/audio streaming
│   │   ├── ota.ts                  # OTA update service
│   │   └── input.ts
│   └── store/
│       └── emulator.ts
├── public/
│   └── assets/
│       ├── crt-frame.png           # CRT monitor frame
│       └── scanlines.png           # Scanline overlay
└── vite.config.mts
```

### Display Modes

1. **Fullscreen Mode** - Scaled display fills browser window
2. **CRT Mode** - Display inside virtual CRT monitor frame with scanlines and curvature
3. **Pixel Perfect Mode** - Integer scaling, no filtering
4. **Debug Mode** - Split view with registers/memory/disassembly

### CRT Effect Options (WebGL Shader)

```typescript
interface CRTEffectOptions {
    enabled: boolean;
    scanlines: boolean;
    scanlineIntensity: number;    // 0.0 - 1.0
    curvature: boolean;
    curvatureAmount: number;      // 0.0 - 0.5
    bloom: boolean;
    bloomIntensity: number;       // 0.0 - 1.0
    vignette: boolean;
    phosphorGlow: boolean;
    colorBleed: boolean;
}
```

### Tasks

#### 4.1 HTTP/WebSocket Server Setup
- Port 80: Main web interface (REST API + static files)
- WebSocket `/ws/stream`: A/V streaming
- WebSocket `/ws/input`: Input events
- mDNS: `esptari.local`

#### 4.2 Frontend Development
- Vue.js 3 with TypeScript
- Vite build system
- TailwindCSS for styling
- Gzipped bundle embedded in firmware (~500KB)

#### 4.3 Video/Audio Streaming Integration
- WebSocket binary protocol for frames
- Web Audio API for low-latency audio
- Canvas 2D or WebGL for rendering
- Optional CRT shader effects

#### 4.4 Input Handling
- Keyboard mapping (browser to ST scan codes)
- Mouse capture (Pointer Lock API)
- Virtual joystick (touch/mobile)
- Gamepad API for USB controllers

#### 4.5 Network Configuration UI
- WiFi network scanner and selector
- Ethernet status display
- IP configuration (DHCP/static)
- Connection priority settings

### Deliverables
- [ ] Working HTTP/WebSocket server
- [ ] Vue.js frontend with all pages
- [ ] Real-time A/V streaming in browser
- [ ] Machine selection menu
- [ ] ROM/disk browser
- [ ] Network configuration interface
- [ ] Full input support (keyboard/mouse/joystick)
- [ ] CRT shader effects (optional)
- [ ] OTA firmware update via web interface
- [ ] OTA rollback capability

---

## Phase 5: Storage & ROM Management

### Goals
- Implement SD card file system
- Create ROM management system
- Build floppy disk emulation
- Implement hard disk emulation

### SD Card Structure

```
/sdcard/
├── cores/                       # Dynamic emulation components (.ebin)
│   ├── cpu/
│   │   ├── m68000.ebin          # MC68000 @ 8MHz (ST, STe)
│   │   ├── m68010.ebin          # MC68010 (rare upgrades)
│   │   ├── m68020.ebin          # MC68020 (accelerators)
│   │   ├── m68030.ebin          # MC68030 @ 16-50MHz (TT, Falcon)
│   │   ├── m68040.ebin          # MC68040 @ 25-40MHz (accelerators)
│   │   └── m68060.ebin          # MC68060 @ 50-66MHz (accelerators)
│   ├── mmu/
│   │   ├── st_glue.ebin         # ST GLUE chip (basic memory mapping)
│   │   ├── ste_gstmcu.ebin      # STe GST MCU
│   │   └── tt_mmu.ebin          # TT/Falcon MMU
│   ├── video/
│   │   ├── shifter.ebin         # ST Shifter (320x200/640x200/640x400)
│   │   ├── ste_shifter.ebin     # STe Shifter (hardware scroll, 4096 colors)
│   │   └── videl.ebin           # Falcon VIDEL (true color, resolutions)
│   ├── audio/
│   │   ├── ym2149.ebin          # YM2149 PSG (all systems)
│   │   ├── dma_audio.ebin       # STe/TT DMA audio
│   │   ├── dsp56001.ebin        # Falcon DSP56001 @ 32MHz
│   │   └── codec.ebin           # Falcon audio codec
│   └── misc/
│       ├── blitter.ebin         # BLiTTER chip (STe/TT/Falcon)
│       ├── mfp68901.ebin        # MFP (timers, interrupts, serial)
│       ├── wd1772.ebin          # Floppy disk controller
│       ├── acia6850.ebin        # ACIA (keyboard, MIDI)
│       ├── acsi.ebin            # ACSI hard disk interface
│       ├── ide.ebin             # IDE interface (Falcon)
│       ├── scsi.ebin            # SCSI interface (TT/Falcon)
│       └── fpu68881.ebin        # FPU 68881/68882
├── machines/                    # Machine configuration profiles
│   ├── st.json                  # Atari ST (1985)
│   ├── stfm.json                # Atari STFM (1986)
│   ├── mega_st.json             # Atari Mega ST (1987)
│   ├── ste.json                 # Atari STe (1989)
│   ├── mega_ste.json            # Atari Mega STe (1991)
│   ├── tt030.json               # Atari TT030 (1990)
│   ├── falcon030.json           # Atari Falcon030 (1992)
│   └── custom/                  # User-defined machine configs
│       └── falcon060.json       # Falcon with 68060 accelerator
├── config/
│   ├── esptari.json             # Main configuration
│   └── keymaps/                 # Keyboard mappings
├── roms/
│   ├── tos/
│   │   ├── tos100.img           # TOS 1.00 (ST)
│   │   ├── tos102.img           # TOS 1.02 (ST)
│   │   ├── tos104.img           # TOS 1.04 Rainbow (ST)
│   │   ├── tos106.img           # TOS 1.06 (ST/STFM)
│   │   ├── tos162.img           # TOS 1.62 (STe)
│   │   ├── tos206.img           # TOS 2.06 (STe/Mega STe)
│   │   ├── tos306.img           # TOS 3.06 (TT)
│   │   └── tos404.img           # TOS 4.04 (Falcon)
│   ├── cartridges/
│   └── bios/
├── disks/
│   ├── floppy/
│   │   ├── games/
│   │   └── apps/
│   └── hard/
│       └── c.hdd                # Hard disk image
├── states/                      # Save states
└── screenshots/
```

### Tasks

#### 5.1 SD Card Initialization
- FAT32 file system via VFS
- Auto-create directory structure
- Configuration file parsing (JSON)

#### 5.2 ROM Loader
- TOS ROM detection and validation
- ROM version identification
- Checksum verification
- Loading into PSRAM

#### 5.3 Floppy Disk Controller (WD1772)
- Read/write sector emulation
- Track seeking
- Disk image formats: ST, MSA, STX
- Write protection support

#### 5.4 Hard Disk Emulation
- ACSI protocol emulation
- HD image format support
- Partition table handling
- Large disk support (>504MB via ICD extensions)

### Deliverables
- [ ] SD card initialization and VFS
- [ ] ROM loading from SD card
- [ ] FDC emulation with ST/MSA support
- [ ] Basic ACSI hard disk emulation
- [ ] Save state support

---

## Phase 6: Input Handling

### Goals
- Implement keyboard emulation
- Create joystick/mouse emulation
- Add USB gamepad support
- Build virtual keyboard overlay

### Input Architecture

```
┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
│ USB Keyboard    │   │ USB Gamepad     │   │ Web Interface   │
└────────┬────────┘   └────────┬────────┘   └────────┬────────┘
         │                     │                      │
         ▼                     ▼                      ▼
┌─────────────────────────────────────────────────────────────┐
│                    Input Manager                             │
├─────────────────────────────────────────────────────────────┤
│  Keyboard Map  │  Joystick State  │  Mouse State            │
└────────┬───────┴────────┬─────────┴────────┬────────────────┘
         │                │                   │
         ▼                ▼                   ▼
┌─────────────────────────────────────────────────────────────┐
│                    IKBD Emulation (6301)                    │
└─────────────────────────────────────────────────────────────┘
```

### Keyboard Matrix

The Atari ST uses an intelligent keyboard (IKBD) with its own 6301 microcontroller:
- 96-key matrix
- Mouse port (directly connected)
- Joystick ports (directly connected)
- MIDI through

### Tasks

#### 6.1 IKBD Emulation
- 6301 microcontroller emulation (simplified)
- Keyboard scan code generation
- Mouse packet generation
- Joystick state reporting

#### 6.2 USB HID Support
- USB host mode for keyboard/gamepad
- HID report parsing
- Key code translation to ST scan codes

#### 6.3 Gamepad Mapping
```c
typedef struct {
    uint8_t joystick_port;      // 0 or 1
    struct {
        uint8_t dpad_up;
        uint8_t dpad_down;
        uint8_t dpad_left;
        uint8_t dpad_right;
        uint8_t button_fire;
        // Optional additional mappings
        uint8_t button_autofire;
        uint8_t button_pause;
    } mapping;
} gamepad_config_t;
```

#### 6.4 Web Input Interface
- Keyboard events via WebSocket
- Virtual joystick (touch)
- Mouse capture mode

### Deliverables
- [ ] Full keyboard emulation
- [ ] Mouse emulation with relative movement
- [ ] Dual joystick support
- [ ] USB HID keyboard/gamepad support
- [ ] Web keyboard/gamepad input

---

## Phase 7: Enhanced Hardware - STe

### Goals
- Implement STe hardware enhancements
- Add DMA audio
- Implement enhanced Shifter
- Add Blitter chip

### STe Enhancements

| Feature | Description |
|---------|-------------|
| DMA Audio | 8-bit stereo sampling, 50kHz max |
| Hardware Scroll | Pixel-level horizontal scroll |
| Enhanced Palette | 4-bit per component (4096 colors) |
| Blitter | Hardware block transfer/fill |
| Extended Joysticks | Analog paddles, light pen |

### Tasks

#### 7.1 DMA Audio
- Frame-based DMA
- Sample rates: 6.25, 12.5, 25, 50 kHz
- Mono/stereo modes
- Mixing with YM2149

#### 7.2 Enhanced Shifter
- Hardware horizontal scrolling
- Extended palette (4-bit RGB)
- Smooth mid-screen palette changes

#### 7.3 Blitter (BLiTTER)
- Block copy operations
- Line drawing
- Pattern fill
- Halftone modes
- All 16 logic operations

```c
typedef struct {
    uint16_t halftone[16];      // Halftone pattern
    uint16_t src_xinc;          // Source X increment
    uint16_t src_yinc;          // Source Y increment
    uint32_t src_addr;          // Source address
    uint16_t endmask[3];        // End masks
    uint16_t dst_xinc;          // Dest X increment
    uint16_t dst_yinc;          // Dest Y increment
    uint32_t dst_addr;          // Dest address
    uint16_t x_count;           // X count (words)
    uint16_t y_count;           // Y count (lines)
    uint8_t  hop;               // Halftone operation
    uint8_t  op;                // Logic operation
    uint8_t  control;           // Control register
    uint8_t  skew;              // Skew value
} blitter_t;
```

### Deliverables
- [ ] DMA audio playback
- [ ] Hardware scrolling
- [ ] 4096-color palette
- [ ] Full Blitter emulation
- [ ] Extended joystick support

---

## Phase 8: Advanced Systems

### Goals
- MC68030 CPU support (TT/Falcon)
- DSP56001 emulation (Falcon)
- VIDEL video chip (Falcon)
- Advanced TOS versions

### Tasks (Long-term)

#### 8.1 MC68030 Enhancements
- Cache emulation
- MMU emulation
- New instructions (68020/030)
- FPU emulation (68881/68882)

#### 8.2 DSP56001 (Falcon)
- 24-bit DSP core
- X/Y/P memory spaces
- DSP-to-host interface
- Audio processing

#### 8.3 VIDEL (Falcon)
- True color modes (up to 65536 colors)
- Multiple resolutions
- Hardware overlay
- VGA/RGB output modes

### Deliverables
- [ ] TT system emulation
- [ ] Falcon base system
- [ ] DSP56001 core
- [ ] VIDEL video chip

---

## Resource Requirements

### Memory Budget

| Resource | Allocation | Notes |
|----------|------------|-------|
| ST RAM | 4MB | From PSRAM |
| TOS ROM | 512KB | Loaded from SD |
| Frame Buffer | 1MB | Double-buffered (640x400 RGB565 x2) |
| Audio Buffer | 128KB | Ring buffer for streaming |
| Stream Buffer | 256KB | Encoded video frames |
| Web Assets | 512KB | Gzipped frontend |
| Network Stack | 64KB | WiFi + Ethernet |
| CPU Stack | 32KB | Per task |
| **Total PSRAM** | ~7MB | Of 32MB available |

### Flash Partitions (16MB)

| Partition | Size | Purpose |
|-----------|------|--------|
| nvs | 24KB | Non-volatile storage |
| otadata | 8KB | OTA boot selection |
| phy_init | 4KB | PHY calibration |
| spiffs | 128KB | Network configuration |
| ota_0 | 3.5MB | Application slot 0 |
| ota_1 | 3.5MB | Application slot 1 |
| storage | ~9MB | Additional storage |

### CPU Budget

| Task | Core | Priority | Stack |
|------|------|----------|-------|
| Emulation | HP Core 0 | 24 | 16KB |
| Video Encode | HP Core 1 | 22 | 8KB |
| Audio Stream | HP Core 1 | 23 | 4KB |
| WebSocket Server | HP Core 1 | 18 | 8KB |
| HTTP Server | HP Core 1 | 10 | 8KB |
| Network Manager | HP Core 1 | 12 | 4KB |
| Input Handler | HP Core 1 | 20 | 4KB |
| SD Card I/O | LP Core | 5 | 4KB |

---

## Testing Strategy

### Unit Tests
- CPU instruction tests (each opcode)
- Memory access tests
- Timer accuracy tests

### Integration Tests
- TOS boot sequence
- GEM desktop loading
- Disk I/O operations

### Compatibility Tests
- Known commercial games
- Demo scene productions
- Diagnostic software (YAART, etc.)

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| PSRAM bandwidth | High | Optimize memory access patterns, use L2 cache |
| CPU performance | High | Profile and optimize hot paths, use IRAM |
| Network latency | High | WebSocket binary protocol, dual-interface failover |
| Audio sync | Medium | Adaptive buffering, timestamp synchronization |
| Web streaming lag | Medium | Tune encoder quality, use raw frames on good networks |
| TOS compatibility | High | Extensive testing, reference documentation |
| WiFi connectivity | Medium | Ethernet fallback, robust reconnection |
| Browser compatibility | Low | Standard APIs (WebSocket, Canvas, Web Audio) |

---

## References

### Documentation
- Atari ST/STe/TT/Falcon Hardware Reference
- MC68000 Family Programmer's Reference Manual
- YM2149 Application Notes
- WD1772 Floppy Disk Controller Datasheet
- MFP 68901 Datasheet
- BLiTTER Technical Reference

### Emulator References
- Hatari (open source, comprehensive)
- Steem SSE (accurate STe emulation)
- ARAnyM (Falcon/TT focus)

### ESP32-P4 Documentation
- ESP32-P4 Technical Reference Manual
- ESP-IDF Programming Guide
- Waveshare ESP32-P4-NANO Wiki

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | Feb 2026 | Initial plan |
| 1.1 | Feb 2026 | Removed MIPI-DSI, all output via web browser; Added network interface manager with SPIFFS config; Updated streaming to WebSocket+Canvas; Added Phase 0.5 for network management |
| 1.2 | Feb 2026 | Added OTA (Over-The-Air) firmware update support with A/B partitioning and rollback |

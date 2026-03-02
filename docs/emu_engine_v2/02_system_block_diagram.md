<!-- NAV_META: doc=02; index=README.md; prev=01_executive_summary.md; next=03_component_spec_table.md -->
[← Index](README.md) | [← Previous: Executive summary](01_executive_summary.md) | [Next: Component table →](03_component_spec_table.md)

# 2) System block diagram (textual)

**Purpose:** Define buses, chips, clock domains, DMA paths, and interrupt topology.

## 2.1 Major chips and buses

- **CPU domain**
  - Motorola **68000-class CPU** (8 MHz baseline; Mega STe supports higher-speed mode) [V][R]
  - 24-bit address bus, 16-bit data bus
  - Asynchronous bus handshake and exception acknowledge cycles

- **Core chipset domain**
  - **GLUE**: address decode, interrupt glue logic, timing glue [V][R]
  - **MMU**: ST-RAM access timing, video RAM fetch scheduling, refresh-related behavior [V][R]
  - **SHIFTER**: video fetch, shift/serialize, palette output timing [V][R]

- **I/O / control domain**
  - **MFP 68901**: timers, GPIO, interrupt controller, RS232 USART [V]
  - **ACIA (2x 6850)**: IKBD serial and MIDI serial [V]
  - **IKBD MCU** (HD6301-family firmware): keyboard/mouse/joystick protocol engine [V][R]
  - **YM2149 PSG**: audio + GPIO sideband control [V]

- **DMA/storage domain**
  - Atari DMA controller [V][R]
  - **FDC** (WD1772-class, AJAX in some later systems) [V][R]
  - **ACSI** DMA hard-disk path [V]

- **Model-dependent accelerators/peripherals**
  - **Blitter** (present by configuration/model) [V][R]
  - **STe/Mega STe DMA sound + mixer control (Microwire)** [V][R]
  - RTC / cache / speed control in Mega-class boards [R]

## 2.2 Signal and dataflow summary

- CPU, SHIFTER, DMA (and Blitter when active) contend for ST-RAM bus slots.
- SHIFTER consumes deterministic fetch windows each line; MMU enforces memory schedule.
- DMA uses bus master slots during FDC/ACSI transfer service.
- Interrupt sources (MFP, video-related glue events, peripheral completion) converge to CPU IPL handling.
- IKBD and MIDI traverse ACIAs; IKBD itself is asynchronous MCU firmware-driven.

## 2.3 Interrupt topology (logical)

- **Video timing interrupts**: VBL/HBL-related events through GLUE/video path [V][R]
- **MFP interrupt output**: single prioritized interrupt source to CPU (vectored in MFP space) [V]
- **ACIA/FDC/DMA completion**: routed by board glue/MFP GPIO/interrupt logic depending on source [V][R]
- **Reset/NMI/power-fail**: model and board dependent [R]

---

[← Index](README.md) | [← Previous: Executive summary](01_executive_summary.md) | [Next: Component table →](03_component_spec_table.md)

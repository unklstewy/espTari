# Atari ST Family Cycle-Accurate Emulation Specification (ST / Mega ST / STe / Mega STe)

Version: 1.0 (Specification baseline)  
Date: 2026-03-01  
Target implementation platform (future work): ESP32-P4-NANO (Waveshare), ESP-IDF v5.5.2  
Scope: Specification only (no implementation code)

Cross-linked chapter index: [README.md](README.md)

---

## Evidence and certainty legend

- **[V] Verified**: Behavior backed by vendor datasheets, Atari technical/service documentation, or repeatable hardware tests.
- **[I] Inferred**: Behavior derived from reverse engineering, community test suites, or observed software dependence; requires per-board validation for final 100% claim.
- **[R] Revision-specific**: Known to vary by chip revision, motherboard revision, TOS revision, or region.

---

## 1) Executive summary

### 1.1 Definition of “100% compatibility + cycle accuracy”

For this project, **100% compatibility** means any software that runs on original Atari ST-family hardware (in-scope models) must produce identical software-visible outcomes: same control flow, same interrupt sequencing, same media behavior, same video/audio temporal output, and same peripheral protocol behavior.

**Cycle accuracy** means all software-visible hardware interactions are reproduced at deterministic clock-step granularity so timing-sensitive software (demos, copy protection, trackers, MIDI sequencers, raster code) behaves identically.

### 1.2 Measurable acceptance targets

1. **Clock-step determinism**: chip state evolution is deterministic at master-clock-derived tick resolution.
2. **CPU-visible timing error budget**: 0 bus-slot mismatch for interrupt acknowledge, DMA contention, and exception entry ordering.
3. **Video phase accuracy**: register-write-to-pixel effect occurs on matching scanline and sub-line phase.
4. **Audio cadence accuracy**: PSG and STe DMA audio sample cadence drift = 0 over infinite deterministic replay.
5. **Storage protocol accuracy**: FDC/DMA command and DRQ/IRQ sequencing matches hardware traces for protected and non-protected media.
6. **Boot-state accuracy**: reset vectors, ROM overlay behavior, and early hardware defaults match model + TOS revision profile.

### 1.3 Required implementation philosophy for future engine

- Simulate **chips and buses**, not just APIs.
- Use **event ordering rules tied to clock domains**, not frame-based shortcuts.
- Maintain **model profiles** for ST, Mega ST, STe, Mega STe and selected chip/TOS revisions.
- Keep **undefined/open-bus** behavior deterministic and profile-selectable.

---

## 2) System block diagram (textual)

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

## 3) Component-by-component specification table

| Component name | Real hardware part/chip | Function | Clock/timing domain | Bus interactions | Registers/state that must be modeled | Interrupts/exceptions generated/consumed | DMA/arbitration effects | Cycle-accuracy requirements | Model variants | Common emulation pitfalls |
|---|---|---|---|---|---|---|---|---|---|---|
| CPU core | Motorola 68000-class [V] | Main instruction execution, exceptions, bus control | CPU clock (model profile) | Primary bus initiator; exception and IACK cycles | Full programmer-visible state, SR/CCR semantics, supervisor/user stack behavior, prefetch queue visibility, bus cycle type state | Consumes IPL; generates all architected exceptions | Loses bus slots under contention | Instruction timing + bus-cycle timing must match; prefetch-visible effects mandatory | ST, Mega ST, STe, Mega STe | Treating CPU as instruction-only without external wait/slot model |
| GLUE | Atari GLUE ASIC [V][R] | Address decode, interrupt glue, system timing glue | Master/video-derived domain | Decode/route of ROM/RAM/I/O | Decode maps, line states, timing edges relevant to IRQ/video boundaries | Produces/forwards timing-related IRQ conditions | Governs ownership logic with MMU | Edge timing around HBL/VBL and decode side effects must be exact | All | Incorrect decode mirrors or IRQ edge ordering |
| MMU | Atari MMU ASIC [V][R] | DRAM access scheduling, shared memory timing | Master/video-derived domain | Mediates RAM accesses among masters | RAM config profile, fetch schedule state, refresh-adjacent behavior | None direct | Enforces contention slots | Must reproduce per-slot access windows, not average wait states | All | Modeling contention statistically instead of structurally |
| SHIFTER | Atari SHIFTER [V][R] | Video memory fetch and pixel serialization | Pixel/master domain | Periodic RAM fetch bursts | Video base/counter state, mode control, palette state, line progression | Contributes to line/frame timing events | Steals fixed RAM slots | Fetch phase and palette write visibility must match sub-line timing | ST/Mega ST, enhanced in STe/Mega STe | Wrong border/fetch timing breaks demo raster effects |
| MFP | MC68901 [V] | Timers, GPIO, interrupt controller, serial | MFP clock domain | Byte-wide mapped I/O registers | GPIP/AER/DDR, IER/IPR/ISR/IMR, timer control/data, vector register, USART regs | Generates prioritized vectored interrupts | No bus mastering | Timer decrement phase, pending/in-service behavior, and IRQ clear semantics must be exact | All | Mis-modeling ISR/IPR interactions, edge-vs-level GPIO interrupts |
| ACIA (IKBD) | MC6850 [V] | Serial interface between host and IKBD MCU | ACIA baud clock | Mapped control/status/data | Control, status flags, data buffer, overrun/framing state | Generates IRQ as wired on board | None | Byte-complete timing and status transitions exact | All | HLE event injection bypassing ACIA timing |
| ACIA (MIDI) | MC6850 [V] | MIDI TX/RX serial interface | MIDI baud clock | Mapped control/status/data | Same as above with independent channel state | Generates IRQ as configured | None | Correct TDRE/RDRF transitions and IRQ timing | All | MIDI running-status timing bugs from buffered shortcuts |
| IKBD MCU | HD6301-family keyboard MCU + firmware [V][R] | Keyboard scan, mouse/joystick packetization, command handling | Independent MCU clock | Serial to host via ACIA | Command parser state machine, mode flags, typematic state, internal queues, reset state | Indirect via ACIA output | None | Packet cadence and command side effects must follow firmware behavior | All (firmware rev dependent) | Modeling as direct keyboard matrix without firmware protocol |
| PSG | YM2149F [V] | 3-voice tone/noise/envelope audio + GPIO | PSG internal clock | Address/data latch via I/O bus | 16 PSG regs, envelope phase, noise LFSR, GPIO port state | Typically none direct | GPIO controls external lines (e.g., drive/side select usage) | Audio phase continuity and register-write timing required | All | Ignoring GPIO sideband effects used by floppy control logic |
| DMA controller | Atari DMA ASIC [V][R] | RAM transfer engine for FDC/ACSI | Shared bus + peripheral handshakes | Secondary bus master | DMA address/counter/control/status state, command phase state | Completion/error routed through system interrupts | Deterministic bus steals while active | DRQ-driven transfer cadence and terminal behavior must be exact | All | Instant transfer abstraction breaks loaders/protection |
| FDC | WD1772 / AJAX-class [V][R] | Floppy command execution and data separation | FDC clock domain | Accessed via DMA/FDC register path | Command/status/track/sector/data, DRQ/INTRQ FSM, internal phase | Generates DRQ/INTRQ | Drives DMA request pattern | Command latency, DRQ spacing, index/track timing must match | All (chip revision dependent) | Ignoring rotational position and non-standard format behavior |
| Blitter | Atari Blitter [V][R] | Block transfer/raster operations | Blitter + system bus domain | Additional bus master | Source/dest, increments, masks, halftone, op mode, control/status | Completion signaling per model usage | Hog/cooperative modes contend with CPU | Bus-cycle exact steal pattern required | Optional/upgrade on some ST, present in STe/Mega STe | Treating blits as immediate memory copy |
| DMA sound | Atari STe DMA sound [V][R] | 8-bit stereo sample playback | Audio sample clock domain | DMA reads from ST-RAM | Start/end/current pointers, mode, control, frequency divisor state | Event/IRQ behavior per model/software use | Competes for memory slots | Sample period, loop boundary, and channel timing must be exact | STe/Mega STe only | Off-by-one at loop/end, wrong sample divisor mapping |
| Mixer control | STe Microwire-controlled analog path [V][R] | Volume/tone/mix control | Serial control domain | I/O mapped control writes | Shift register/latch state, programmed attenuation/tone control | None | None | Control-to-audio state change timing deterministic | STe/Mega STe only | Applying settings instantaneously without serial/latch timing |
| ACSI host interface | Atari DMA-to-ACSI path [V] | Hard-disk command/data transfer | DMA + external device timing | DMA-mediated bus transactions | Command phase, timeout, status/result state | Completion/error to interrupt chain | DMA contention during transfer | Boot probe order/timeouts must be reproducible | All | Simplified SCSI-like model missing ACSI timing quirks |
| Cartridge port | Cartridge ROM interface [V][R] | External ROM and protection peripherals | CPU/bus domain | Memory-mapped reads | Mapping window, mirror behavior, bus-read characteristics | Usually none | None | Read timing/open-bus behavior can be software-visible | All | Assuming cartridge is always absent and unprobed |
| Parallel port | Centronics-compatible interface [V][R] | Printer + dongle I/O | I/O + handshake domain | Data/strobe/status via mapped bits | Port state, handshake edge timing | Through GPIO/interrupt path as wired | None | Strobe/busy edge ordering deterministic | All | Emulating as write-only sink |
| RS232 | MFP USART + board transceiver [V][R] | Serial communications | MFP timer/USART domain | MFP register interactions | Full USART state and error flags | MFP interrupts | None | Bit timing and framing flags accurate | All | Host-OS UART behavior leaking into emulation timing |
| Mega STe speed/cache controls | Board/chipset-specific logic [R] | CPU speed mode and cache behavior | CPU + board control domain | Memory-mapped control registers | Speed-mode state, cache enable/disable state, transition behavior | Can affect exception timing indirectly | Alters contention profile via CPU pace | Mode-switch timing and cache visibility must match profile | Mega STe only | Treating 16 MHz as simple multiplier without cache/bus coupling |

---

## 4) Full memory map and I/O map

## 4.1 Global memory map (24-bit physical)

> Note: exact mirror behavior differs by motherboard decode and TOS ROM size [R].

| Address range | Region | Behavior requirements |
|---|---|---|
| 0x000000-0x000007 | Reset vectors (initial SSP/PC fetch) | On reset, vector fetch must honor ROM overlay behavior [V][R] |
| 0x000000-(RAM_TOP-1) | ST-RAM | Shared between CPU/video/DMA/blitter; contention fully modeled [V] |
| RAM_TOP-implementation max | Unmapped RAM space | Bus error or deterministic open-bus profile [R] |
| 0xFA0000-0xFBFFFF | Cartridge ROM window (128 KB) | Read mapping + mirrors/profile behavior required [V][R] |
| 0xFC0000-0xFEFFFF | TOS ROM (common mapping window) | ROM size/revision may mirror/alias [R] |
| 0xFF0000-0xFFFFFF | High memory I/O + upper ROM decode interactions | I/O decode precedence and undefined-region behavior modeled [V][R] |

## 4.2 I/O map (core blocks)

| Address range (family-level) | Block | Required modeling |
|---|---|---|
| 0xFF8000-0xFF82FF | Video/MMU/SHIFTER control | Video base/counter, sync/mode, palette; STe extended controls where applicable |
| 0xFF8600-0xFF86FF | DMA + FDC/ACSI interface | DMA addr/count/control; command/status path timing and side effects |
| 0xFF8800-0xFF88FF | YM2149 PSG | Address latch + data semantics, GPIO side effects |
| 0xFF8900-0xFF89FF | STe DMA sound | Playback pointers, control, sample-rate divisors |
| 0xFF8920-0xFF892F | STe Microwire control | Serial command/latch behavior |
| 0xFF8A00-0xFF8AFF | Blitter registers | Full operation state and start/finish semantics |
| 0xFFFA00-0xFFFA3F | MFP 68901 | Full register map with true read/write side effects |
| 0xFFFC00-0xFFFC07 | ACIA pair (IKBD + MIDI) | Separate control/status/data channels |
| 0xFFFDxx (model dep.) | RTC / board-specific controls | Mega-class model profile required |
| Undecoded in 0xFFxxxx | Undefined I/O | Deterministic bus error/open-bus result by profile |

## 4.3 Mirrors, undefined regions, and side effects

Mandatory behavior classes:

1. **Decoded mirrors** [R]:
   - Partial decode in ASICs causes repeated register windows; emulate per profile.
2. **Write-only/read-as-last/open values** [R]:
   - Undefined reads may return last bus value, pull-up value, or device-specific patterns.
3. **Read side effects** [V]:
   - MFP/ACIA/FDC status accesses can clear flags or advance internal state.
4. **Byte-lane effects** [V][R]:
   - 8-bit peripheral registers on 16-bit bus may ignore wrong-lane writes.

---

## 5) Timing chapter

## 5.1 CPU cycle timing, wait states, bus contention

- Base cycle model must start from real 68000 bus cycle sequencing [V].
- Add external wait/hold effects from MMU arbitration, SHIFTER fetch, DMA/Blitter masters [V][R].
- Preserve prefetch queue visibility for self-modifying code, trap/interrupt entry, and cycle-exact loops.
- RMW instructions must lock bus sequence exactly as hardware does.
- Mega STe speed/cache mode transitions must update timing behavior immediately at documented boundaries [R].

## 5.2 Video timing: scanlines, blanking, sync, fetch windows

Required outputs:

1. Exact line and frame cadence for PAL/NTSC profiles [V][R].
2. Correct active display, border, and blanking windows [V].
3. SHIFTER fetch bursts at exact slot positions each line [V][I].
4. Mid-line register write effects (palette/mode/base changes) at correct phase [V][I].
5. STe-specific fine scroll/offset behavior integrated into fetch/visible timing [V][R].

## 5.3 Audio timing and mixing paths

- **PSG path**:
  - Tone/noise/envelope phase-accurate advancement.
  - Register write timing visible in waveform phase.
- **STe DMA path**:
  - Sample clock divisors exact.
  - Stereo channel and pointer wrap semantics exact.
  - End-of-buffer/loop events deterministic.
- **Mixer path**:
  - Microwire command shift/latch timing.
  - Deterministic combination of PSG + DMA output.

## 5.4 Interrupt latency and jitter requirements

- CPU IPL sampling at instruction boundaries per 68000 rules [V].
- MFP interrupt request, mask, and in-service transitions clock-accurate [V].
- VBL/HBL interrupt assertions aligned to line timing edges [V][R].
- Latency tolerance for compatibility target: effectively 0 bus-slot mismatch for timing-critical software.

## 5.5 Event ordering rules for simultaneous events

When two or more events become eligible in same master tick, resolve in deterministic order:

1. Reset/NMI/power-fail class events.
2. Bus fault-class events (BERR-adjacent model behavior).
3. Video timing edge transitions (line/frame boundaries).
4. DMA handshake edges (DRQ/terminal count completion).
5. MFP timer underflow and pending-bit updates.
6. ACIA byte completion edges.
7. CPU interrupt sampling/exception entry.

If board-level captures prove alternate ordering for a specific revision, override in that model profile [R].

---

## 6) Storage/media chapter

## 6.1 Floppy subsystem behavior

### 6.1.1 Required mechanical/logical model elements

- Motor spin-up/down timing windows.
- Head step rate and settle delay.
- Track/side position state.
- Index pulse rotational phase.
- Read/write gate and write splice timing.

### 6.1.2 Required controller behavior (FDC + DMA)

- Full command FSM and status flags.
- DRQ/INTRQ signal timing.
- Sector ID/gap/CRC handling.
- Multi-sector transfer pacing and terminal count behavior.
- Error/retry semantics visible to TOS and loaders.

### 6.1.3 Protection-sensitive edge cases

Must support deterministic representations for:

- Weak/fuzzy bits.
- Deliberate CRC errors.
- Overlong/short tracks.
- Illegal/duplicate sector IDs.
- Sync/address mark anomalies.
- Nonstandard rotational layouts.

## 6.2 ACSI/hard-disk behavior and boot interactions

- Implement command/status sequencing with realistic timeout windows.
- Respect boot probe ordering and device ID expectations.
- Emulate DMA transfer pacing and completion interrupt behavior.
- Preserve bus contention impact during active disk DMA.
- Ensure compatibility with early AHDI and boot-sector-driven loaders.

---

## 7) Input/peripherals chapter

## 7.1 Keyboard/IKBD protocol and MCU behavior

- Emulate IKBD as autonomous MCU protocol endpoint, not direct key state memory.
- Implement command set, replies, error handling, and reset behavior.
- Preserve typematic delays/rates and mode-dependent packet cadence.
- Handle queue overflow and command interleaving deterministically.
- Model firmware revision differences where known [R].

## 7.2 Mouse/joystick timing and packet semantics

- Mouse deltas packetized per IKBD mode and timing.
- Joystick modes (event/direct semantics as used by software) must match protocol behavior.
- Packet boundary timing and ordering relative to keyboard events must be deterministic.

## 7.3 MIDI and serial (ACIA) behavior

- MIDI ACIA RX/TX status transitions exact.
- Interrupt generation timing exact under heavy stream conditions.
- Serial/MFP USART framing/parity/overrun flags exact.
- Host I/O abstraction must not leak non-deterministic scheduling into emulated timing.

## 7.4 Printer/parallel and cartridge/expansion expectations

- Parallel handshake behavior deterministic (strobe/busy/ack semantics).
- Cartridge ROM mapping/probe behavior available at boot/runtime.
- Expansion/control-register expectations on Mega models exposed through model profile.

---

## 8) ROM/TOS chapter

## 8.1 TOS/OS ROM interactions required by software

- Correct reset vector source and early ROM overlay mechanics [V][R].
- Accurate exception vector table usage from RAM after handover.
- Preserve TOS assumptions about DMA/FDC timing, MFP setup, IKBD initialization order.

## 8.2 Boot-sequence dependencies

Boot-time order must preserve:

1. CPU reset and initial vector fetch behavior.
2. Early memory sizing/tests dependent on contention timing.
3. Peripheral initialization order (MFP, ACIA, IKBD, storage).
4. Boot media probing order and timeout behavior.

## 8.3 Region/version compatibility considerations

- Support at minimum representative TOS sets across ST-era and STe-era branches.
- Distinguish PAL/NTSC timing profiles.
- Distinguish keyboard/locale assumptions affecting IKBD and TOS behavior.
- Expose per-profile ROM size/decode mirror differences [R].

---

## 9) Compatibility risk matrix

| Category | Subsystems/features | Requirement level |
|---|---|---|
| Must be cycle-perfect | CPU bus + prefetch-visible behavior, GLUE/MMU/SHIFTER arbitration, MFP timer/IRQ sequencing, FDC+DMA handshake timing, Blitter arbitration, VBL/HBL edge timing, IKBD/ACIA byte timing | Non-negotiable for 100% claim |
| Can be high-level but deterministic | Analog filtering/monitor response nuances, non-critical audio coloration, RTC drift modeling, host file-backed media caching internals | Acceptable if externally deterministic and software-invisible |
| Optional for most software | Rare dongle edge cases, uncommon expansion board electrical quirks, niche cartridge sideband protocols | Implement as optional model extensions |

---

## 10) Validation plan

## 10.1 Test categories

1. **CPU/exceptions**: 68000 trap/frame/prefetch timing test ROMs.
2. **Video/demo timing**: border, raster, split-palette, sync-sensitive demo tests.
3. **Audio**: PSG phase tests, STe DMA loop and rate tests, mixer control tests.
4. **Storage**: standard ST disks, malformed/protected images, ACSI boot/runtime tests.
5. **Input/peripherals**: IKBD command suite, mouse/joystick stress, MIDI throughput/jitter.
6. **TOS/GEM apps**: desktop, accessories, productivity apps across TOS variants.
7. **Model regression**: ST vs Mega ST vs STe vs Mega STe profile matrix.

## 10.2 Observable pass/fail criteria

- IRQ timeline traces match expected ordering and slot position.
- Video test patterns match scanline-phase effect points.
- FDC/DMA traces match command/DRQ/INTRQ sequences and completion timing.
- IKBD protocol transcript matches reference for command/reply ordering.
- MIDI stream tests show no timing-induced semantic errors.
- Boot behavior reproducible across cold/warm reset scenarios.

## 10.3 Suggested conformance checklist

- [ ] Reset/ROM overlay and vector behavior matches selected model+TOS profile.
- [ ] CPU exception timing and stack frame forms validated.
- [ ] MFP timer and interrupt nesting semantics validated.
- [ ] ACIA channels validated independently and under load.
- [ ] SHIFTER fetch windows and palette timing validated in all modes.
- [ ] DMA/FDC rotational and protection edge cases validated.
- [ ] Blitter bus arbitration validated (where present).
- [ ] STe DMA sound + Microwire behavior validated (STe/Mega STe).
- [ ] Undefined/open-bus behavior deterministic and profile-selectable.
- [ ] PAL/NTSC and region/TOS matrix passes representative software suites.

---

## Appendix A: Exact interrupt hierarchy and vector behavior requirements

1. CPU interrupt priority evaluation must follow 68000 IPL rules.
2. MFP internal priority resolver and vector base behavior must match MC68901 behavior.
3. Simultaneous interrupt source assertions must follow deterministic chip-level event order (Section 5.5).
4. Interrupt acknowledge cycles must have correct bus timing and side effects.
5. Exception overlap cases (interrupt during trace, bus/access fault windows, stacked exceptions) must use real 68000 ordering.

---

## Appendix B: Startup/reset/power-on assumptions and undocumented quirks

Mandatory profile fields (per model/revision):

- Power-on register defaults for MFP, ACIA, PSG, DMA/FDC interface.
- Initial video mode and sync defaults.
- IKBD reset response timing and default mode.
- ROM overlay enable/disable transition point.
- Open-bus default pattern assumptions.
- Warm reset vs cold reset differences.

Undocumented but software-relevant quirks to preserve where verified [R]:

- Register write timing races during early VBL/HBL windows.
- DMA completion edge behavior in command boundary conditions.
- MFP pending/in-service interactions under rapid reassertion.
- Model/revision-specific mirror decode oddities.

---

## Appendix C: Model-delta summary

### C.1 520ST / 1040ST baseline

- Core: 68000 + GLUE/MMU/SHIFTER + MFP + ACIAx2 + IKBD + PSG + DMA/FDC.
- No STe DMA audio or Microwire features.
- Blitter may be absent/present depending on board/upgrades.

### C.2 Mega ST

- Baseline ST compatibility plus Mega-specific board integrations (RTC/model options).
- Blitter commonly present (profile-dependent by exact unit/revision).

### C.3 STe

- Enhanced video controls (fine scroll/extended palette behavior).
- DMA sound and Microwire-controlled mixer path.
- Additional registers and timing interactions in video/audio subsystems.

### C.4 Mega STe

- STe-class multimedia features plus speed/cache and board-control differences.
- Requires explicit model profile for CPU speed mode transitions and cache behavior.

---

## Appendix D: Requirements for evidence closure before claiming 100%

Before claiming 100% compatibility, each [I] and [R] item must be promoted to measured profile behavior by:

1. Capturing hardware traces on representative boards/chip revisions.
2. Running regression corpus with timing-signature comparison.
3. Recording profile-specific deltas in versioned documentation.
4. Locking deterministic replay hashes for each test configuration.

<!-- NAV_META: doc=07; index=README.md; prev=06_storage_media.md; next=08_rom_tos.md -->
[← Index](README.md) | [← Previous: Storage/media chapter](06_storage_media.md) | [Next: ROM/TOS chapter →](08_rom_tos.md)

# 7) Input/peripherals chapter

**Purpose:** Define IKBD, ACIA, MIDI, serial, joystick, parallel, and cartridge interface behavior.

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

[← Index](README.md) | [← Previous: Storage/media chapter](06_storage_media.md) | [Next: ROM/TOS chapter →](08_rom_tos.md)

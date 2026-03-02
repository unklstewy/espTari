# 3.1 Planes

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 04

## Subsections

- A) Control plane
- B) Emulation plane
- C) I/O and media plane
- D) Observability plane
- E) Streaming plane

---

## 3.1 Planes

### A) Control plane

Responsible for lifecycle and orchestration:

- Engine boot/shutdown/reset
- Machine profile selection
- EBIN dependency resolution and load order
- Session state transitions (`stopped`, `starting`, `running`, `paused`, `suspended`, `faulted`)

### B) Emulation plane

Responsible for deterministic emulation execution:

- Global master-tick scheduler
- Bus arbitration and component stepping
- Memory map dispatch
- Interrupt/event ordering
- Deterministic replay checkpoints (optional but recommended)

### C) I/O and media plane

Responsible for machine media and external interfaces:

- SD-card media mounts and path policy
- Disk/cartridge/ROM attach-detach
- Remote file manager operations
- Host input ingress and normalization (keyboard/mouse/controller)
- Input-to-virtual-machine mapping profiles

### D) Observability plane

Responsible for debugging and inspection:

- Register snapshots + streaming changes
- Bus transaction stream
- Memory map access stream
- Timing/cycle metrics

### E) Streaming plane

Responsible for browser-consumable outputs:

- Video frame stream (and optional scanline debug stream)
- Audio PCM stream
- Metadata/event channels (state changes, dropped frame counters, sync status)

---

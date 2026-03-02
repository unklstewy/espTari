# 1. Purpose and scope

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 01

## 1. Purpose and scope

This document defines the implementation architecture and delivery plan for **Emulation Engine v2** for espTari, using the `docs/emu_engine_v2` hardware specification as the source of truth for machine behavior.

Initial machine target is:

- **Atari ST baseline** (520ST/1040ST behavior profile)

The architecture is intentionally designed to scale to:

- Mega ST
- STe
- Mega STe

without rebuilding the entire engine, by using dynamic machine/component loading through SD-card-resident EBIN modules.

---

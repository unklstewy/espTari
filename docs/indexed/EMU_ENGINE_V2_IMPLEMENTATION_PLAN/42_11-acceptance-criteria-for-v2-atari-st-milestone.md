# 11. Acceptance criteria for v2 Atari ST milestone

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 42

## 11. Acceptance criteria for v2 Atari ST milestone

A milestone is complete when all are true:

1. Engine can start/pause/resume/reset/stop Atari ST session through v2 APIs.
2. ROM/disk/cartridge assets are loaded from SD-card only.
3. EBIN modules for Atari ST profile can be remotely uploaded, validated, loaded, and unloaded.
4. Video and audio are available via streaming APIs (browser-consumable transport).
5. Register inspection is available as both snapshot and stream.
6. Bus and memory-map access traces are available via filtered stream endpoints.
7. Input API can translate keyboard, mouse, and game-controller events into Atari ST virtual inputs with profile-driven mappings.
8. Trace system remains stable under backpressure with explicit dropped-event metrics.
9. Browser-session input capture supports enable/disable, mouse-over capture mode, click-to-capture mode, and escape-sequence release.
10. Catalog-backed media resolution works for ROM/floppy/TOS IDs and supports download of missing local assets from hosted URLs.
11. Dead hosted links are marked in catalogs with status/timestamps, and scheduler-driven sync jobs can refresh catalogs on-device.
12. Machine state saving supports suspend-save and restore-resume with compatibility checks.
13. Hard performance SLO targets are measured and exposed: input latency <= 50 ms, jitter < 30 ms, dropped-frame rate < 1 percent.
14. Debug clock controls support slow-motion and single-step execution while preserving observability correctness.

---

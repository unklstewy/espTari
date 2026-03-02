# 14. Immediate next actions

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 45

## 14. Immediate next actions

1. Freeze API contract draft (`/api/v2/*`) and event schemas.
2. Define EBIN ABI v1 and module manifest schema.
3. Implement SD-card catalog service + remote file manager staging flow.
4. Implement Atari ST baseline machine profile loader.
5. Implement first end-to-end run path (ROM load -> session start -> video/audio stream + register stream).
6. Keep API implementation aligned with `EMU_ENGINE_V2_API_SPEC.md` and publish changelog deltas on contract updates.
7. Implement input translation pipeline and default Atari ST mapping profiles for keyboard, mouse, and controller.
8. Implement catalog sync service with hosted-link probing, dead-link marking, and missing-asset downloader.
9. Implement machine save-state persistence with suspend-save and restore-resume lifecycle APIs.
10. Implement performance SLO metrics collection and reporting endpoints.
11. Implement debug clock mode and single-step execution controls for low-speed trace capture.
9. Implement on-device periodic scraper jobs for floppy/ROM/TOS catalog refresh.

# 6.1 Control APIs

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 16

## 6.1 Control APIs

- `POST /api/v2/engine/session`
  - Start machine session with profile + module set
- `POST /api/v2/engine/session/stop`
- `POST /api/v2/engine/session/pause`
- `POST /api/v2/engine/session/resume`
- `POST /api/v2/engine/session/reset`
- `POST /api/v2/engine/session/suspend-save`
- `POST /api/v2/engine/session/restore-resume`
- `GET /api/v2/engine/session`
  - Current state, loaded modules, cycle counters, stream health
- `GET /api/v2/engine/status`
  - Session/runtime status snapshot with lifecycle state semantics (see `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.6B`)
- `GET /api/v2/engine/health`
  - Engine health summary and component-level health states (see `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.6A`)

Request payload (start):

- `machine`: `"atari_st"`
- `profile`: e.g. `"st_520_pal"`, `"st_1040_ntsc"`
- `rom_id`
- optional `disk_ids[]`
- optional `cartridge_id`
- optional `module_overrides{}`

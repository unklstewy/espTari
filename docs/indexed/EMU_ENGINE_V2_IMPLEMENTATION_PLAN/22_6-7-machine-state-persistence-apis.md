# 6.7 Machine state persistence APIs

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 22

## 6.7 Machine state persistence APIs

- `POST /api/v2/engine/state/save`
  - Persist complete machine state snapshot and return `snapshot_id`
- `POST /api/v2/engine/state/restore`
  - Restore machine state from `snapshot_id` with compatibility validation
- `GET /api/v2/engine/state/list`
  - Enumerate available machine state snapshots

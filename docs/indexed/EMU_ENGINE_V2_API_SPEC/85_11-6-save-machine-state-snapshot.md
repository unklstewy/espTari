# 11.6 Save machine state snapshot

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 85

## 11.6 Save machine state snapshot

- `POST /api/v2/engine/state/save`

Snapshot schema contract (`snapshot_schema_v1`):

- Required top-level metadata fields:
  - `snapshot_id` (string)
  - `schema_version` (uint32, fixed `1` for v1)
  - `profile` (string)
  - `abi` (object with `engine` and `modules` map)
  - `hash` (string)
  - `created_at_us` (uint64)
- Scheduler restore fields:
  - `scheduler.tick_hz` (uint32)
  - `scheduler.step_order[]` (deterministic component ordering)
- Media binding restore fields:
  - `media_bindings.rom_id` (string)
  - `media_bindings.disk_ids[]` (array)
  - `media_bindings.cartridge_id` (string or `null`)

Component state block contract (`snapshot_component_state_blocks_v1`):

- `state_blocks.cpu.required`: `pc`, `sr`, `d`, `a`
- `state_blocks.glue_mmu_shifter.required`: `video_base`, `sync_mode`, `mmu_bank`
- `state_blocks.mfp.required`: `iera`, `ierb`, `isra`, `isrb`, `timers`
- `state_blocks.acia_ikbd.required`: `acia_status`, `acia_control`, `ikbd_queue`
- `state_blocks.dma_fdc.required`: `dma_addr`, `dma_mode`, `fdc_command`, `fdc_status`
- `state_blocks.psg.required`: `registers`, `mixer`, `gpio`

Compatibility metadata requirements for restore validation:

- `schema_version` participates in `RCOMP-01` (accepted schema set membership).
- `profile` participates in `RCOMP-02` (runtime profile equality).
- `abi.engine` and `abi.modules` participate in `RCOMP-03` and `RCOMP-04`.
- `hash` is integrity evidence used by restore read-path validation before commit.

Request:

```json
{
  "session_id": "ses_01H...",
  "name": "suspend_after_boot",
  "tags": ["boot", "debug"]
}
```

Response `data`:

```json
{
  "snapshot_id": "snap_01H...",
  "name": "suspend_after_boot",
  "schema_version": 1,
  "profile": "st_520_pal",
  "abi": {
    "engine": "2.0.0",
    "modules": {
      "cpu": "2.0.0",
      "video": "2.0.0",
      "io": "2.0.0",
      "storage": "2.0.0",
      "audio": "2.0.0"
    }
  },
  "hash": "sha256:...",
  "created_at_us": 1710000004321,
  "saved_at_us": 1710000004321
}
```

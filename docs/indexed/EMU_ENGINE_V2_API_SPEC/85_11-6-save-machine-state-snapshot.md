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

Serializer contract (`snapshot_component_serializers_v1`):

- Each baseline component exposes deterministic serializer and deserializer routines for snapshot state blocks:
  - `cpu.serialize` / `cpu.deserialize`
  - `glue_mmu_shifter.serialize` / `glue_mmu_shifter.deserialize`
  - `mfp.serialize` / `mfp.deserialize`
  - `acia_ikbd.serialize` / `acia_ikbd.deserialize`
  - `dma_fdc.serialize` / `dma_fdc.deserialize`
  - `psg.serialize` / `psg.deserialize`
- Deterministic ordering and validation checks:
  - `SER-ORD-01`: stable key ordering for every serialized component block.
  - `SER-ORD-02`: explicit `endianness` field is present and fixed to `little`.
  - `SER-VAL-01`: missing or invalid mandatory fields fail serialization with deterministic error mapping.
- Save response includes `serializer_checks` and `serializer_fingerprint` to provide repeatability evidence.

Persistence backend contract (`snapshot_persistence_atomic_v1`):

- Snapshot metadata and index writes use staged temp-path writes followed by atomic rename commit.
- Commit strategy must prevent partially written committed records across interruption windows.
- Save handler must commit in-memory active snapshot pointers only after persistence commits succeed.
- Deterministic persistence checks:
  - `PERSIST-ATOMIC-01`: interruption before metadata commit.
  - `PERSIST-ATOMIC-02`: interruption before index commit.
  - `PERSIST-WRITE-01`: metadata write failure.
  - `PERSIST-WRITE-02`: index write failure.
- Fault-injection query `force_persist_interrupt=1|true` is available for conformance smoke simulation of interrupted write windows.

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
  "saved_at_us": 1710000004321,
  "persistence": {
    "strategy": "staging_rename",
    "atomic": true,
    "meta_path": "/spiffs/snapshot_meta_v1_<hash>.meta",
    "index_path": "/spiffs/snapshot_index_v1.log"
  }
}
```

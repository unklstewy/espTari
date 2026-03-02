# 11.3K Startup defaults and power-on register baseline table

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 81

## 11.3K Startup defaults and power-on register baseline table

Startup defaults contract:

- `GET /api/v2/inspect/chipset/startup/defaults?session_id=...`
- Response `data` must expose startup defaults validated by `startup_defaults_v1`.
- `startup_defaults_v1` required fields:
  - `session_id` (string)
  - `machine_profile` (string)
  - `video_standard` (enum: `pal`, `ntsc`)
  - `ram_size_kib` (uint32)
  - `boot_device` (enum: `floppy`, `harddisk`, `rom`)
  - `defaults_revision` (string)
  - `applied_tick` (uint64)
  - `applied_timestamp_us` (uint64)

Power-on register baseline contract:

- `GET /api/v2/inspect/chipset/startup/baseline?session_id=...&group=...`
- Baseline entries are validated by `power_on_register_baseline_v1`.
- `power_on_register_baseline_v1` required fields:
  - `group` (enum: `glue`, `mmu`, `shifter`, `mfp`, `acia`, `fdc`, `psg`)
  - `registers` (array of `power_on_register_entry_v1`)
  - `baseline_revision` (string)
  - `source` (enum: `profile_defaults`, `hardware_boot_table`)
- `power_on_register_entry_v1` required fields:
  - `name` (string)
  - `address` (string, hex)
  - `width_bits` (uint8)
  - `reset_value` (string, hex)
  - `mask` (string, hex)

Startup default/baseline conformance checks:

- `PWR-BASE-01`: each required startup default field must be populated before `running` lifecycle state is entered.
- `PWR-BASE-02`: every baseline register entry must have deterministic `reset_value` and `mask` for the selected `machine_profile`.
- `PWR-BASE-03`: duplicate register addresses within a `group` are forbidden.
- `PWR-BASE-04`: `applied_timestamp_us` must be monotonic across restart cycles for the same persisted session timeline.

Startup default/baseline deterministic guard failures:

- Missing/invalid query params (`session_id`, `group`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Baseline table unavailable or unresolved startup-default revision -> `INTERNAL_ERROR`.

Startup defaults example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "machine_profile": "atari_st_520_1040_baseline",
  "video_standard": "pal",
  "ram_size_kib": 1024,
  "boot_device": "floppy",
  "defaults_revision": "startup_rev_03",
  "applied_tick": 12,
  "applied_timestamp_us": 1710000035200
}
```

Power-on register baseline example (`data` excerpt):

```json
{
  "group": "mfp",
  "registers": [
    {
      "name": "IERA",
      "address": "0x00FFFA07",
      "width_bits": 8,
      "reset_value": "0x00",
      "mask": "0xFF"
    }
  ],
  "baseline_revision": "baseline_rev_07",
  "source": "hardware_boot_table"
}
```

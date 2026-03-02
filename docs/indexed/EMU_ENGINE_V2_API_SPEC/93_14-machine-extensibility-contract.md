# 14. Machine extensibility contract

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 93

## 14. Machine extensibility contract

Machine IDs:

- `atari_st` (initial)
- `mega_st` (future)
- `atari_ste` (future)
- `mega_ste` (future)

API stability requirement:

- Existing `atari_st` fields remain backward compatible.
- New machine-specific fields are additive under `data.machine_extensions`.

---

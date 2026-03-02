# 6.3 Media APIs

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 18

## 6.3 Media APIs

- `POST /api/v2/media/rom/attach`
- `POST /api/v2/media/disk/attach`
- `POST /api/v2/media/disk/eject`
- `POST /api/v2/media/cartridge/attach`
- `POST /api/v2/media/cartridge/eject`
- ROM attach request validation + catalog binding check contract (`session_id`/`rom_id` validation, ROM-catalog binding enforcement, profile-compatibility checks, and deterministic blocker mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.1`.
- ROM mount/apply flow and attach-status event contract (phase progression `validated -> mounted -> applied`, rollback-on-apply-failure behavior, and `media_attach_status` stream events with deterministic sequencing fields) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `9.1`.
- Disk attach/eject request validation and binding-check contracts (`drive`/`disk_id` request validation, disk-catalog binding/format checks, deterministic no-op eject semantics, and deterministic blocker mapping) are defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.2` and `9.3`.
- Disk mount/eject runtime flow and disk state event contract (attach phase progression, rollback-on-activation-failure behavior, deterministic eject phase semantics, and `media_disk_state` stream events with sequence/timestamp fields) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` sections `9.2` and `9.3`.

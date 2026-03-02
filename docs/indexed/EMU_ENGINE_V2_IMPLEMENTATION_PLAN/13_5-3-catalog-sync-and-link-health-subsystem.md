# 5.3 Catalog sync and link-health subsystem

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 13

## 5.3 Catalog sync and link-health subsystem

Responsibilities:

- Keep local catalogs current against upstream hosted sources.
- Detect assets present in catalogs but missing on SD-card.
- Download missing assets on demand or via scheduled prefetch jobs.
- Probe and classify hosted URLs as online/offline/dead.
- Mark dead links in catalog metadata with timestamp and reason.
- SD-card rescan must produce a canonical presence index snapshot (`catalog + entry_id` keyed records with local presence metadata) as defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10` (`POST /api/v2/catalogs/{catalog}/rescan-local`).
- Missing-asset diff API projection must derive from that canonical presence index (`GET /api/v2/catalogs/{catalog}/missing-report`) and follow deterministic blocker mapping defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10`.
- Hosted download request validation + enqueue contract (`POST /api/v2/catalogs/{catalog}/download-entry`) including deterministic blocker mapping and queue projection fields is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10`.
- Staged download commit + integrity/hash verification contract (staging path, `staged` -> `verified` -> `committed` phases, and deterministic commit blockers) for that same endpoint is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10`.
- Dead-link probe worker and timeout policy contract (`POST /api/v2/catalogs/{catalog}/probe-links`) including timeout classification, dead-link transition thresholds, and deterministic probe blockers is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10`.
- Dead-link mark/retry state machine and telemetry fields contract (`POST /api/v2/catalogs/{catalog}/mark-dead` + `allow_dead_retry=true` retry path on `POST /api/v2/catalogs/{catalog}/download-entry`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.10`.
- Catalog-sync scheduler core and persisted schedules contract (authoritative scheduler clock, deterministic due-order selection, persisted schedule store, and deterministic scheduler blockers across `POST/GET/DELETE /api/v2/catalog-sync/schedules`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.11`.
- Schedule CRUD API and reboot-recovery validation contract (`POST/GET/PATCH/DELETE /api/v2/catalog-sync/schedules*`, startup schedule-store validation/quarantine, and deterministic recovery blocker behavior) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `7.11`.

On-device scraper/sync jobs (aligned with host scraper flow):

- `floppy_catalog_sync` (CSV shard ingestion + catalog update)
- `rom_catalog_sync` (JSON ingestion + catalog update)
- `tos_catalog_sync` (page parse + catalog update)

Job modes:

- `catalog_only`
- `catalog_and_probe_links`
- `catalog_probe_and_prefetch_missing`

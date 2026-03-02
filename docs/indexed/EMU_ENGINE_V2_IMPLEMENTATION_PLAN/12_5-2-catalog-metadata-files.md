# 5.2 Catalog metadata files

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 12

## 5.2 Catalog metadata files

- `rom_catalog.json`
- `disk_catalog.json`
- `cartridge_catalog.json`
- `ebin_catalog.json`
- `tos_catalog.json`

Catalog entry fields (minimum):

- `id`
- `path`
- `sha256`
- `size`
- `machine_tags`
- `format`
- `created_at` / `updated_at`
- `source_url`
- `hosted_url`
- `availability_state` (`unknown`, `online`, `offline`, `dead`, `local_only`)
- `availability_checked_at`
- `download_fail_count`
- optional `dead_link_reason`

Catalog-backed media ID resolution (required):

- `rom_id`, `disk_id`, and `tos_id` resolve through catalog indexes rather than direct file-path assumptions.
- If a catalog entry exists but the file is missing on SD-card, runtime can request immediate or deferred download from the entry hosted URL.
- Canonical loader contract for these IDs (`rom_catalog.json`, `disk_catalog.json`, `tos_catalog.json`), normalized resolved fields, and deterministic blocker mapping (`CATALOG_NOT_FOUND`, `CATALOG_ENTRY_NOT_FOUND`, `CONFLICT`) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.1`.
- Catalog-backed ID resolution API path matrix across session start and media attach endpoints, including stable failure-stage mapping, is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.1` and is normative for endpoint behavior.

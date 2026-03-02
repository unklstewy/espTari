# 6.2 SD-card remote file manager APIs

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 17

## 6.2 SD-card remote file manager APIs

- `GET /api/v2/files/list?path=...`
- `POST /api/v2/files/upload` (multipart/chunked)
- `POST /api/v2/files/move`
- `POST /api/v2/files/delete`
- `GET /api/v2/files/download?path=...`
- `POST /api/v2/files/mkdir`
- `GET /api/v2/files/stat?path=...`

Catalog/asset sync endpoints:

- `GET /api/v2/catalogs/list`
- `GET /api/v2/catalogs/{catalog}/entries`
- `GET /api/v2/catalogs/{catalog}/entries/{entry_id}`
- `POST /api/v2/catalogs/{catalog}/download-missing`
- `POST /api/v2/catalogs/{catalog}/download-entry`
- `POST /api/v2/catalogs/{catalog}/probe-links`
- `POST /api/v2/catalogs/{catalog}/mark-dead`
- `POST /api/v2/catalogs/{catalog}/rescan-local`

Scraper/scheduler endpoints:

- `POST /api/v2/catalog-sync/jobs/run`
- `GET /api/v2/catalog-sync/jobs`
- `GET /api/v2/catalog-sync/jobs/{job_id}`
- `POST /api/v2/catalog-sync/schedules`
- `GET /api/v2/catalog-sync/schedules`
- `DELETE /api/v2/catalog-sync/schedules/{schedule_id}`

EBIN-specific endpoints:

- `GET /api/v2/ebins/catalog`
- `POST /api/v2/ebins/rescan`
- `POST /api/v2/ebins/validate`
- `POST /api/v2/ebins/load`
- `POST /api/v2/ebins/unload`

# 7.9 Catalog list and entry APIs

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 42

## Subsections

- List catalogs
- List catalog entries
- Get one entry

---

## 7.9 Catalog list and entry APIs

Supported catalogs:

- `roms`
- `floppies`
- `tos`

### List catalogs

- `GET /api/v2/catalogs/list`

Response `data`:

```json
{
  "catalogs": [
    {"name": "roms", "path": "/sdcard/config/engine_v2/rom_catalog.json", "entries": 46},
    {"name": "floppies", "path": "/sdcard/config/engine_v2/disk_catalog.json", "entries": 1319},
    {"name": "tos", "path": "/sdcard/config/engine_v2/tos_catalog.json", "entries": 1498}
  ]
}
```

### List catalog entries

- `GET /api/v2/catalogs/{catalog}/entries?query=...&missing_only=false&state=online`

Entry state values:

- `unknown`
- `online`
- `offline`
- `dead`
- `local_only`

### Get one entry

- `GET /api/v2/catalogs/{catalog}/entries/{entry_id}`

Response `data` example:

```json
{
  "id": "disk.automation.a_093",
  "local_path": "/sdcard/disks/st/AUTOMATION/A_093.ST",
  "hosted_url": "http://ataristdb.sidecartridge.com/AUTOMATION/A_093.ST",
  "availability_state": "online",
  "availability_checked_at": "2026-03-01T15:22:01Z",
  "download_fail_count": 0
}
```

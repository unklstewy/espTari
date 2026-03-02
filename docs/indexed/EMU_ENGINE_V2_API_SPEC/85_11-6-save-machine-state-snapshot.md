# 11.6 Save machine state snapshot

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 85

## 11.6 Save machine state snapshot

- `POST /api/v2/engine/state/save`

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
  "schema_version": 1,
  "profile": "st_520_pal",
  "abi": {"engine": "2.0.0", "modules": {"cpu": "2.0.0"}},
  "hash": "sha256:...",
  "saved_at_us": 1710000004321
}
```

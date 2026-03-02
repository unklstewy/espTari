# 7.1 List directory

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 34

## 7.1 List directory

- `GET /api/v2/files/list?path=/sdcard/disks/st`

Response `data`:

```json
{
  "path": "/sdcard/disks/st",
  "entries": [
    {"name": "game.st", "type": "file", "size": 901120, "mtime": 1710000001}
  ]
}
```

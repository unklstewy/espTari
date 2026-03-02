# 7.4 Move/rename

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 37

## 7.4 Move/rename

- `POST /api/v2/files/move`

```json
{
  "from": "/sdcard/.staging/new.ebin",
  "to": "/sdcard/ebins/st/cpu/new.ebin",
  "overwrite": false
}
```

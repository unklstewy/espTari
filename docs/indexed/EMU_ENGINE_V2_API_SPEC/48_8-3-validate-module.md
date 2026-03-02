# 8.3 Validate module

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 48

## 8.3 Validate module

- `POST /api/v2/ebins/validate`

```json
{
  "path": "/sdcard/ebins/st/cpu/st.cpu.m68k-2.0.0.ebin"
}
```

Response `data`:

```json
{
  "valid": true,
  "module_id": "st.cpu.m68k",
  "version": "2.0.0",
  "abi": "2.0",
  "integrity": {"sha256": "...", "signature": "ok"},
  "dependencies": []
}
```

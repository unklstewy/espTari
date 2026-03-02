# 8.4 Load module

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 49

## 8.4 Load module

- `POST /api/v2/ebins/load`

```json
{
  "session_id": "ses_01H...",
  "component": "cpu",
  "module_id": "st.cpu.m68k",
  "version": "2.0.0",
  "policy": "pause_swap_resume"
}
```

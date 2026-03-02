# 4. Common REST response envelope

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 11

## 4. Common REST response envelope

Canonical contract rules:

- Every REST endpoint returns exactly one envelope shape for success (`ok=true`) and one for error (`ok=false`).
- `request_id` and `timestamp_us` are mandatory on both success and error responses.
- Endpoint sections define only the `data` payload for success and must not redefine top-level envelope fields.
- Lifecycle and status contracts in sections `6.1` through `6.8` and `12` are bound to this canonical envelope.

Success:

```json
{
  "ok": true,
  "request_id": "req_01H...",
  "timestamp_us": 1710000000000,
  "data": {}
}
```

Error:

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000000000,
  "error": {
    "code": "EBIN_ABI_MISMATCH",
    "category": "ebin",
    "message": "Requested module ABI 2.1 is incompatible with engine ABI 2.0",
    "retryable": false,
    "details": {
      "module_id": "st.cpu.m68k",
      "required": "2.1.x",
      "actual": "2.0.4"
    }
  }
}
```

---

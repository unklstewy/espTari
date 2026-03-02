# 7.7 Upload (multipart)

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 40

## 7.7 Upload (multipart)

- `POST /api/v2/files/upload`

Multipart fields:

- `path` (target canonical path)
- `file` (binary data)
- optional `sha256`
- optional `machine_tags`

Behavior:

1. Write to staging path
2. Verify integrity/format
3. Atomic move to target path
4. Update corresponding catalog

# 9. Security and integrity model

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 34

## 9. Security and integrity model

1. Restrict file manager to SD-card root allowlist.
2. Normalize and validate paths (prevent traversal).
3. Enforce upload size limits and chunk timeouts.
4. Validate EBIN integrity before load.
5. Maintain audit log for:
   - uploads
   - loads/unloads
   - module validation failures
   - session transitions

---

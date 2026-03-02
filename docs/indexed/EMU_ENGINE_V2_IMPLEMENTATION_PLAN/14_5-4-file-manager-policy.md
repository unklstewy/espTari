# 5.4 File manager policy

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 14

## 5.4 File manager policy

- All user uploads enter staging path first: `/sdcard/.staging/`
- Validate format + hash + optional signature
- Move atomically to canonical path
- Update catalog index transactionally

---

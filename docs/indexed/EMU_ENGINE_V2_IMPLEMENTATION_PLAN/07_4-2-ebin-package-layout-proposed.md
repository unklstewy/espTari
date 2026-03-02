# 4.2 EBIN package layout (proposed)

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 07

## 4.2 EBIN package layout (proposed)

SD-card path convention:

- `/sdcard/ebins/<machine>/<component>/<name>-<version>.ebin`
- `/sdcard/ebins/index.json` (module catalog)

EBIN header metadata (minimum):

- magic/version
- module type (`cpu`, `video`, `io`, `storage`, `machine_profile`, etc.)
- target machine(s) compatibility tags (`st`, `mega_st`, `ste`, `mega_ste`)
- API ABI version
- required exports
- dependency list (module IDs + ABI ranges)
- checksum/hash
- signature block (recommended)

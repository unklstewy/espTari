# 8.2 Profile configuration artifact

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 32

## 8.2 Profile configuration artifact

Store profile manifests under:

- `/sdcard/config/engine_v2/machines/atari_st/*.json`

Profile fields:

- region/video standard (`pal`/`ntsc`)
- RAM size profile
- expected module IDs and ABI requirements
- default ROM recommendation set
- enabled stream defaults
- ST profile manifest parser + schema validation contract (canonical manifest path, required schema fields, normalized parse output, and deterministic blockers for missing/invalid manifests) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.1` and is normative for session bootstrap.
- Profile wiring validation + fail-fast diagnostics contract (module-resolution checks, `scheduler.step_order` validation, startup abort-before-run guarantees, and deterministic wiring blocker mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.1` and is normative for bootstrap safety.

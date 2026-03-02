# Phase 4 — Robustness and conformance

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 40

## Phase 4 — Robustness and conformance

- Stress/fault recovery tests
- Long-run stability tests
- Atari ST conformance suites aligned to `docs/emu_engine_v2/10_validation_plan.md`
- Catalog sync reliability tests (link-probe accuracy, dead-link marking, retry policy)
- On-device scraper schedule tests (manual/periodic runs, restart recovery)
- Hard performance SLO validation (latency/jitter/dropped-frame)
- Debug clock mode and single-step conformance tests
- Conformance harness scaffold + test manifest loader contract (harness session bootstrap, `conformance_manifest_v1` schema, deterministic loader/session guard failures, and initialization evidence) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.1` and is the canonical source.
- Evidence artifact collection + report packaging flow contract (collection endpoint, package endpoint, deterministic state guards, and artifact/report evidence payloads) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.1` and is the canonical source.
- Acceptance checklist execution runner contract (run/status endpoints, runner state progression, selection semantics, and deterministic runner guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.1` and is the canonical source.
- Review pack generation + signoff bundle assembly contract (review-pack endpoint, signoff-bundle endpoint, deterministic precondition guards, and signed bundle evidence payloads) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.1` and is the canonical source.
- Subsystem conformance scaffold + fixture model contract (`conformance_fixture_model_v1`, checks `CONF-FIX-01..04`, deterministic scaffold/fixture guard failures, and scaffold/model evidence payloads) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.2` and is the canonical source.
- Per-subsystem acceptance suite + reporting-output contract (`subsystem_suite_report_v1`, checks `SUB-SUITE-01..04`, deterministic suite/report guard failures, and suite/report evidence payloads) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `15.3` and is the canonical source.

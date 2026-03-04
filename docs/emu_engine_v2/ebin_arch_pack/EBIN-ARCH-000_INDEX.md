# EBIN Architecture Pack Index

## Purpose
Provide a concise, query-friendly entry point for EBIN architecture execution without changing existing V2 contracts.

## Scope
- EBIN development architecture for Atari ST baseline (520ST/1040ST lineage and ST subsystem contracts)
- Dependency ordering, interface matrix, sprint-ready breakdown, determinism/observability plan, and go/no-go gates
- Contract-preserving guidance only (no contract redesign)

## Dependencies
- `docs/emu_engine_v2/EBIN_RUNTIME_ABI_V1.md`
- `docs/emu_engine_v2/03_component_spec_table.md`
- `docs/emu_engine_v2/02_system_block_diagram.md`
- `TRACKING/BACKLOG.md`
- `TRACKING/S5_RUNTIME_UNLOCK_TRACEABILITY_MATRIX_2026-03-04.md`
- `TRACKING/PROGRAM_STATUS_SNAPSHOT_2026-03-04.md`

## Interfaces
- Index entry schema: `doc_id`, `path`, `summary`, `phase`, `status_focus`
- All pack documents use stable IDs: `EBIN-ARCH-*`, `EBIN-INTF-*`, `EBIN-GATE-*`

## Acceptance criteria
1. Every indexed document exists and is cross-linked.
2. Every document contains: Purpose, Scope, Dependencies, Interfaces, Acceptance criteria, Evidence expected, Cross-links.
3. Index summaries are short, deterministic, and phase-tagged.

## Evidence expected
- Presence of all referenced docs in this folder.
- Link traversal from this index to each doc and back.

## Cross-links
- [EBIN-ARCH-001 Constraints and Phased Roadmap](EBIN-ARCH-001_CONSTRAINTS_AND_PHASED_ROADMAP.md)
- [EBIN-ARCH-002 Subsystem Decomposition and Dependency Order](EBIN-ARCH-002_SUBSYSTEM_DECOMPOSITION_AND_ORDER.md)
- [EBIN-INTF-001 Interface and Contract Matrix](EBIN-INTF-001_INTERFACE_CONTRACT_MATRIX.md)
- [EBIN-ARCH-003 Sprint-Ready Work Breakdown](EBIN-ARCH-003_SPRINT_READY_BREAKDOWN.md)
- [EBIN-ARCH-004 Determinism, Performance, Observability Plan](EBIN-ARCH-004_DETERMINISM_PERF_OBSERVABILITY_PLAN.md)
- [EBIN-ARCH-005 Executive Brief (One Page, Layman Terms)](EBIN-ARCH-005_EXECUTIVE_BRIEF_ONE_PAGE.md)
- [EBIN-ARCH-006 Developer Execution Checklist](EBIN-ARCH-006_DEVELOPER_EXECUTION_CHECKLIST.md)
- [EBIN-GATE-001 Integration Gates and Non-Go Conditions](EBIN-GATE-001_INTEGRATION_GATES_AND_NON_GO.md)

---

## Indexed documents

| ID | Path | Phase | Status focus | Short summary |
|---|---|---|---|---|
| EBIN-ARCH-001 | `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-001_CONSTRAINTS_AND_PHASED_ROADMAP.md` | A/B/C | Contract posture | Synthesizes MVP+V2+tracking constraints and defines phased architecture roadmap. |
| EBIN-ARCH-002 | `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-002_SUBSYSTEM_DECOMPOSITION_AND_ORDER.md` | A/B | Dependency order | Defines subsystem bring-up order and integration prerequisites by hardware domain. |
| EBIN-INTF-001 | `docs/emu_engine_v2/ebin_arch_pack/EBIN-INTF-001_INTERFACE_CONTRACT_MATRIX.md` | A/B/C | Interface safety | Defines EBIN and subsystem interface contracts with invariants/failure modes. |
| EBIN-ARCH-003 | `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-003_SPRINT_READY_BREAKDOWN.md` | A/B/C | Execution | Maps epics→stories→tasks to current tracking and one-cycle developer work. |
| EBIN-ARCH-004 | `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-004_DETERMINISM_PERF_OBSERVABILITY_PLAN.md` | A/B/C | Measurability | Defines what can be measured now vs later and required evidence artifacts. |
| EBIN-ARCH-005 | `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-005_EXECUTIVE_BRIEF_ONE_PAGE.md` | A/B/C | Leadership summary | One-page plain-language phase and task brief for executive consumption. |
| EBIN-ARCH-006 | `docs/emu_engine_v2/ebin_arch_pack/EBIN-ARCH-006_DEVELOPER_EXECUTION_CHECKLIST.md` | A/B/C | Implementation checklist | Phase-by-phase developer checklist with Do/Don’t, tests, evidence, and tags. |
| EBIN-GATE-001 | `docs/emu_engine_v2/ebin_arch_pack/EBIN-GATE-001_INTEGRATION_GATES_AND_NON_GO.md` | B/C | Risk control | Defines development/integration/release gates and explicit non-go conditions. |

## Deterministic query keys

- `phase_a_safe_start`
- `phase_b_integration_prereq`
- `phase_c_release_validation`
- `dev_allowed_now`
- `integration_blocked`
- `release_blocked`
- `contract_preserving`
- `executive_one_page`
- `developer_checklist`

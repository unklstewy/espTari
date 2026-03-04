# Sprint 08 - Phase 6 Active Profile Enablement

Duration: 3 working days  
Goal: Convert Sprint 07 conditional acceptance into active multi-profile runtime coverage by enabling non-baseline profile manifests/wiring and revalidating compatibility slices as active-run paths.

## Planning basis

- Sprint 07 packet residual actions (`S7-R1`, `S7-R2`, `S7-R3`).
- Objective: decompose active profile enablement into pullable XS/S slices with deterministic evidence and PO-ready handoff.

## Committed tasks

- S8-001: Enable runtime manifests/wiring for `mega_st_pal`, `ste_pal`, `mega_ste_pal`.
- S8-002: Validate active Mega ST bootstrap/lifecycle parity (no guard-only path).
- S8-003: Validate active STe extension controls and profile-gated payload semantics.
- S8-004: Validate active Mega STe extension compatibility deltas versus STe baseline.
- S8-005: Validate cross-profile media/catalog/session run-path parity across all active profiles.
- S8-006: Execute active pairwise ABI/compatibility regression matrix.
- S8-007: Validate active profile-switch isolation and fallback semantics.
- S8-008: Assemble Sprint 08 active-profile evidence packet and PO decision handoff.

## Sprint demo scenarios

1. Start each active profile (`atari_st`, `mega_st`, `ste`, `mega_ste`) and show deterministic lifecycle probes.
2. Demonstrate active STe and Mega STe extension payloads with canonical guard behavior for invalid inputs.
3. Run active pairwise profile compatibility matrix and show deterministic outcomes.
4. Demonstrate profile-switch isolation across active profiles with preserved control-plane health.
5. Present Sprint 08 objective-to-evidence packet for PO decision.

## Acceptance criteria

1. All committed S8 tasks are decomposed as pullable items with deterministic checks.
2. Non-baseline profiles are enabled as active runtime paths (not guard-only).
3. Active profile compatibility and fallback semantics are explicit, repeatable, and evidence-backed.
4. PO handoff artifacts are complete and review-ready.

## Evidence package (target)

- Active profile manifest/wiring verification captures.
- Active Mega ST/STe/Mega STe lifecycle and extension captures.
- Active pairwise compatibility matrix outputs and regression checks.
- Profile-switch isolation traces across active profiles.
- Sprint 08 traceability matrix + decision template.

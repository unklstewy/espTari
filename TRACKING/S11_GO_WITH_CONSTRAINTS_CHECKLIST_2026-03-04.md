# S11 Go-With-Constraints Operating Checklist (2026-03-04)

## Scope

Applies when decision state is:
- Start EBIN device development: `go_with_constraints`
- First EBIN runtime integration: `no_go`
- Release hard-validation readiness: `no_go`

## Allowed now

- Implement EBIN device internals (CPU/SHIFTER/PSG/DMA/MMU) behind non-default paths.
- Add unit tests, fixture tests, and deterministic validation harnesses.
- Add instrumentation hooks and synthetic metrics producers for observability plumbing.
- Expand ABI/manifest validator and compatibility matrix checks.
- Refactor non-runtime-critical code that does not alter default runtime activation path.

## Blocked until PRE-EBIN-06 and PRE-EBIN-07 are closed

- Enabling any EBIN device in default runtime path.
- Treating gate-13 sustained SLO as production-valid without component-backed metric density.
- Claiming first integration readiness for EBIN device runtime usage.

## Blocked until PRE-EBIN-08 is closed

- Release hard-gate signoff.
- Any release readiness claim based on Section-11 hard-validation pass.

## Mandatory guardrails during go_with_constraints

- Keep S9-002 controls green on every related change.
- Record evidence links for each closure attempt in current-cycle captures.
- Keep gate state language scoped: `development_start`, `integration_readiness`, `release_readiness`.

## Exit criteria from go_with_constraints

1. `PRE-EBIN-06` closed with passing fixture evidence (`GATE-S11-07/09/10/12`).
2. `PRE-EBIN-07` closed with sustained SLO hard-threshold evidence (`GATE-S11-13`).
3. `PRE-EBIN-08` closed with passing hard-validation report and refreshed decision packet.

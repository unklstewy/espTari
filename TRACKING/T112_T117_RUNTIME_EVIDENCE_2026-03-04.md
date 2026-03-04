# T-112 through T-117 Runtime Evidence (2026-03-04)

## Scope

- `T-112`: Suspend-save request contract wiring and transition checks.
- `T-113`: Restore-resume transition guards and error semantics.
- `T-114`: Restore compatibility rule matrix runtime path coverage.
- `T-115`: Restore compatibility validator and error mapping.
- `T-116`: Performance SLO metric collectors and sampling pipeline.
- `T-117`: SLO endpoints and threshold breach alarm events.

## Runtime validation

### Lifecycle + compatibility (`T-112..T-115`)

- Runner: `tools/smoke_t112_t115_lifecycle_runtime.sh`
- Determinism: `determinism_runs=3`, `determinism_check=pass`
- Stable signature:
  - `suspend=suspended:running->suspended:suspended:True:INVALID_SESSION_STATE`
  - `validate=True:SNAPSHOT_NOT_FOUND:BAD_REQUEST`
  - `restore=running:suspended->running:running:True:ENGINE_NOT_SUSPENDED`
  - `force_fail=INTERNAL_ERROR:True`

### SLO (`T-116..T-117`)

- Runner: `tools/smoke_t116_t117_slo_runtime.sh`
- Determinism: `determinism_runs=3`, `determinism_check=pass`
- Stable signature:
  - `collector=active`
  - `thresholds=30.0:1.0`
  - `alarms=breached,recovered`

## Evidence

- `captures/t112_t115_lifecycle_runtime_20260304_172624.txt`
- `captures/t116_t117_slo_runtime_20260304_172633.txt`

## Decision

`T-112` through `T-117` runtime implementation closure is satisfied with deterministic postflash evidence.

# Auth Contract Evidence Index (2026-03-04)

## Purpose

Provide a single audit-ready index for API authentication contract changes, verification method, and evidence artifacts.

Scope: Emulation Engine v2 token authentication and route-authorization behavior.

## Contract baseline

Auth matrix expectations for every protected route:

- No token -> `401 UNAUTHORIZED`
- Wrong-scope token -> `403 FORBIDDEN`
- Correct-scope token -> authorized route response (not `401`/`403`)

## Source-of-truth docs

- `docs/EMU_ENGINE_V2_API_SPEC.md` (Security and access model + verification evidence)
- `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md` (auth registrar and route-gate architecture)
- `docs/emu_engine_v2/E_api_auth_contract_verification_2026-03-04.md` (appendix-level verification record)
- `README.md` (operator-facing auth and verification command)

## Verification method

Primary command:

```bash
BASE_URL=http://192.168.1.196 ./tools/smoke_auth_route_matrix.sh
```

Primary verifier artifact creator:

- `tools/smoke_auth_route_matrix.sh`

## Evidence artifacts

Full protected-route matrix runs:

- `captures/auth_route_matrix_20260304_145513.txt` (`summary_pass=119`, `summary_fail=0`)
- `captures/auth_route_matrix_20260304_145652.txt` (`summary_pass=119`, `summary_fail=0`)

Token-mode phase-A cross-check:

- `captures/ebin_s10_phase_a_20260304_145043.txt`

## Implementation anchors

- `components/esptari_web/esptari_web_auth.c`
- `components/esptari_web/esptari_web_auth.h`
- `components/esptari_web/esptari_web_http_utils.c`
- `tools/smoke_auth_route_matrix.sh`

## Decision snapshot

- Date: 2026-03-04
- Auth mode validated: `token`
- Route coverage: `119/119`
- Matrix result: `pass`
- Open auth-contract blockers: `none`

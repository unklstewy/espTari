<!-- NAV_META: doc=E; index=README.md; prev=D_evidence_closure_requirements.md; next=- -->
[← Index](README.md) | [← Previous: Appendix D](D_evidence_closure_requirements.md)

# Appendix E: API authentication contract verification (2026-03-04)

## Purpose

Record the implementation-aligned contract for token-based API authentication and provide concrete verification evidence artifacts.

This appendix covers control-plane/API authentication contract verification only; it is not a release readiness declaration.

## Contract under verification

Protected route contract:

- No bearer token: `401 UNAUTHORIZED`
- Bearer token with insufficient scope: `403 FORBIDDEN`
- Bearer token with required scope: request is authorized and returns route-specific status (must not return `401` or `403`)

Token mint contract:

- Endpoint: `POST /api/v2/auth/token`
- Grant type: `client_credentials`
- Deterministic request errors: `400 BAD_REQUEST`
- Deterministic credential errors: `401 UNAUTHORIZED`

## Scope model validated

Scopes covered during verification:

- `engine:control`
- `inspect:read`
- `stream:read`
- `files:read`
- `files:write`
- `input:read`
- `input:write`
- `ebin:manage`

## Verification method

Primary matrix runner:

- `smoke/smoke_auth_route_matrix.sh`

Method summary:

1. Discover all protected routes from code registration calls.
2. Execute each route with no token, wrong-scope token, and required-scope token.
3. Assert matrix outcomes for every route:
   - `401` for no token
   - `403` for wrong scope
   - non-auth-denied response for required scope

## Evidence artifacts

Full matrix (complete coverage):

- `captures/auth_route_matrix_20260304_145513.txt` (`summary_pass=119`, `summary_fail=0`)
- `captures/auth_route_matrix_20260304_145652.txt` (`summary_pass=119`, `summary_fail=0`)

Cross-check smoke bundle in token mode:

- `captures/ebin_s10_phase_a_20260304_145043.txt`

## Implementation notes linked to contract

- Protected-route wrapper and token mint: `components/esptari_web/esptari_web_auth.c`
- Auth registration API: `components/esptari_web/esptari_web_auth.h`
- HTTP status mapping for auth responses: `components/esptari_web/esptari_web_http_utils.c`

## Closure status

Authentication contract verification for route authorization behavior is closed for this validation slice (`2026-03-04`) based on the matrix evidence listed above.

---

[← Index](README.md) | [← Previous: Appendix D](D_evidence_closure_requirements.md)

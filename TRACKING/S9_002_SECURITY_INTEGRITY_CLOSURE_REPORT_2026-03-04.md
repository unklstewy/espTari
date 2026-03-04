# S9-002 Security/Integrity Closure Report (2026-03-04)

## Scope and objective

- Task: `S9-002`
- Objective: Execute security/integrity closure audit for auth posture, SD path hardening/upload limits, and EBIN integrity/audit completeness.
- Basis: `TRACKING/TASK_CARDS_S9_ARC_RESIDUALS.md` (S9-002 scope + acceptance criteria), `docs/EMU_ENGINE_V2_API_SPEC.md` section 3, and `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md` section 9.

## Required controls and result summary

| Control ID | Required control | Result | Summary |
|---|---|---|---|
| AUTH-01 | Authentication mode posture matches spec security model | PASS | Shared auth policy layer enforces bearer-token verification in token mode with deterministic unauthorized responses on protected routes. |
| AUTH-02 | Authorization scope enforcement for privileged endpoints | PASS | Scope gates now enforce minimum scopes (`files:read/write`, `ebin:manage`) before protected handler execution. |
| PATH-01 | File endpoints enforce SD allowlist roots | PASS | `/api/v2/files/*` requests are now hard-constrained to canonical SD allowlist roots including staging and EBIN paths. |
| PATH-02 | Path normalization + traversal prevention | PASS | File-manager path inputs are normalized and traversal attempts are deterministically rejected with `BAD_REQUEST`/`PATH_NOT_ALLOWED`. |
| UPLOAD-01 | Upload size bounds and reject behavior | PASS (baseline) | Request body reader rejects oversized bodies relative to fixed handler buffers (e.g., 512B JSON parser path), returning bad-request flow. |
| UPLOAD-02 | Upload chunk timeout controls | PASS | `/api/v2/files/upload` now supports staged chunk sessions with timeout and max chunk-size enforcement plus explicit commit gate to canonical target path. |
| EBIN-01 | EBIN integrity validation before load | PASS | EBIN validate/load gates enforce SHA-256 shape checks, signature object/fields, semver ABI major gate, and dependency schema checks before successful load path. |
| EBIN-02 | EBIN fault/integrity audit log completeness | PASS | Persistent append-only audit sink (`/sdcard/ebins/esptari_audit.jsonl`) now records upload/load/unload/validate-failure/session-transition events. |

## Evidence anchors (reproducible)

### Spec / requirement anchors

- `docs/EMU_ENGINE_V2_API_SPEC.md`
  - Section 3.1 authentication modes and bearer token recommendation.
  - Section 3.2 authorization scopes.
  - Section 3.3 path hardening requirements.
- `docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md`
  - Section 9: path normalization/traversal prevention, upload limits/timeouts, EBIN integrity validation, and audit-log obligations.

### Code anchors

- Web route registration without auth middleware gate:
  - `components/esptari_web/esptari_web_auth.c`, `components/esptari_web/esptari_web_files.c`, `components/esptari_web/esptari_web_catalog_sync.c`
- File path allowlist/canonicalization and staged upload controls:
  - `components/esptari_web/esptari_web_files.c`
- Request body size guard behavior:
  - `components/esptari_web/esptari_web_http_utils.c` (`esptari_web_read_request_body`)
- Persistent audit sink + event producers:
  - `components/esptari_web/esptari_web_audit.c`
  - `components/esptari_web/esptari_web_catalog_sync.c`
  - `components/esptari_web/esptari_web_lifecycle_session.c`
  - `components/esptari_web/esptari_web_lifecycle_state.c`

### Reproduction checks used

- Auth/scope indicator sweep in web handlers:
  - `grep -RinE "Authorization|authorization|auth_token|bearer|x-api-key|api_key|jwt" components/esptari_web`
- File-path enforcement checks:
  - `grep -nE "handle_files_upload|handle_files_move|handle_files_delete|handle_files_mkdir|valuestring" components/esptari_web/esptari_web_files.c`
  - `grep -nE "is_allowed_scan_root|PATH_NOT_ALLOWED|scan_roots" components/esptari_web/esptari_web_catalog_write_maintenance.c`
- EBIN validation and telemetry checks:
  - `grep -nE "send_ebin_validation_error|is_valid_sha256_hex|EBIN_ABI_MISMATCH|signature|fault_telemetry|fault_stage|fault_reason|fault_at_us" components/esptari_web/esptari_web_catalog_sync.c`

### Post-flash runtime verification evidence

- Probe harness: `tools/probe_s9_002_postflash.sh` (includes top health retry loop before control probing).
- Runtime evidence log: `captures/s9_002_postflash_probe_20260304_111843.txt`
- Runtime evidence bundle: `captures/s9_002_postflash_probe_bundle_20260304_111843.json`
- Verified post-flash control outcomes:
  - `AUTH-01` = pass (`401 UNAUTHORIZED` on missing bearer)
  - `AUTH-02` = pass (`403 FORBIDDEN` on insufficient scope)
  - `PATH-01` = pass (`400 PATH_NOT_ALLOWED` on outside-root)
  - `PATH-02` = pass (traversal + mixed-separator rejections)
  - `UPLOAD-02` = pass (`202` staged start, `413 CHUNK_TOO_LARGE`, `408 CHUNK_TIMEOUT`)
  - `EBIN-02` = pass (validation-failure + session-transition runtime paths reachable)

## Final evidence index (audit handoff)

- Canonical matrix: `TRACKING/evidence/s9_security_integrity_matrix_002.json`
- Closure report: `TRACKING/S9_002_SECURITY_INTEGRITY_CLOSURE_REPORT_2026-03-04.md`
- Remediation actions log: `TRACKING/S9_002_REMEDIATION_ACTIONS_2026-03-04.md`
- Post-flash probe harness: `tools/probe_s9_002_postflash.sh`
- Post-flash evidence log: `captures/s9_002_postflash_probe_20260304_111843.txt`
- Post-flash evidence bundle (authoritative): `captures/s9_002_postflash_probe_bundle_20260304_111843.json`
- Historical intermediate bundles (diagnostic trail):
  - `captures/s9_002_postflash_probe_bundle_20260304_111445.json`
  - `captures/s9_002_postflash_probe_bundle_20260304_111756.json`

## Deterministic closure decision

- S9-002 implementation controls are now **closure-ready**.
- All controls in `TRACKING/evidence/s9_security_integrity_matrix_002.json` are marked `pass` after A1-A4 remediation implementation and successful firmware build.
- Post-flash runtime verification on 2026-03-04 confirms all required key controls pass using `captures/s9_002_postflash_probe_bundle_20260304_111843.json`.
- Remediation plan remains in `TRACKING/S9_002_REMEDIATION_ACTIONS_2026-03-04.md` as execution record.

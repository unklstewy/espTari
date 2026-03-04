# S9-002 Remediation Actions (2026-03-04)

This action list resolves control failures/partials identified in `TRACKING/S9_002_SECURITY_INTEGRITY_CLOSURE_REPORT_2026-03-04.md`.

## Action matrix

| Action ID | Controls | Owner | Deterministic implementation action | Verification gate |
|---|---|---|---|---|
| S9-002-A1 | AUTH-01, AUTH-02 | Web API maintainer | Introduce request auth policy layer (token mode toggle + bearer token verifier + per-route scope map) and enforce before route handlers execute. | Unauthorized/forbidden probes return deterministic `UNAUTHORIZED`/`FORBIDDEN`; valid scoped token succeeds for protected routes. |
| S9-002-A2 | PATH-01, PATH-02 | File manager maintainer | Add canonical path resolver for `/api/v2/files/*`: normalize, reject traversal segments, and hard-enforce SD allowlist roots (`/sdcard/roms`, `/sdcard/disks`, `/sdcard/cartridges`, `/sdcard/tos`, `/sdcard/ebins`). | Deterministic negative tests for `..`, mixed separators, and outside-root targets return `PATH_NOT_ALLOWED`/`BAD_REQUEST`; in-root paths succeed. |
| S9-002-A3 | UPLOAD-02 | File manager maintainer | Replace JSON-only upload stub with staged chunk upload session (`/sdcard/.staging`) including max chunk size, per-chunk timeout, and commit gate to canonical target path. | Chunk timeout and oversize-chunk tests fail deterministically; successful staged upload commits with integrity metadata. |
| S9-002-A4 | EBIN-02 | Catalog/EBIN maintainer | Add persistent audit sink for upload/load/unload/validate-failure/session-transition events (append-only JSONL on SD or structured in-memory ring + periodic flush). | Event sequence for each operation is durable and queryable after restart; event schema includes actor, action, target, status, timestamp, reason. |

## Sequencing

1. Implement `S9-002-A1` first (auth policy baseline).
2. Implement `S9-002-A2` and `S9-002-A3` next (file-path and upload hardening).
3. Implement `S9-002-A4` in parallel or immediately after A2/A3.
4. Re-run S9-002 control matrix and update findings to close task.

## Exit criteria for S9-002

- All controls in `TRACKING/evidence/s9_security_integrity_matrix_002.json` are `pass`.
- Reproducible verification checks are captured and linked in closure evidence.
- Backlog/kanban/DB status can advance from `In Progress` to `Done` only after matrix re-run shows no remaining failures/partials.

## Execution status (2026-03-04)

- `S9-002-A1` completed: token-mode auth policy + route scope enforcement added.
- `S9-002-A2` completed: canonical path normalization, traversal rejection, and SD allowlist guard added across `/api/v2/files/*`.
- `S9-002-A3` completed: staged upload session (`start/chunk/commit/abort`) with max chunk-size and per-chunk timeout enforcement added to `/api/v2/files/upload`.
- `S9-002-A4` completed: persistent append-only JSONL audit sink (`/sdcard/ebins/esptari_audit.jsonl`) added with upload/ebin/session event producers.

## Final post-flash verification (2026-03-04)

- Firmware rebuilt and flashed after auth startup + HTTP status-code mapping fixes.
- Post-flash probe harness with startup health loop: `tools/probe_s9_002_postflash.sh`.
- Evidence log: `captures/s9_002_postflash_probe_20260304_111843.txt`.
- Evidence bundle: `captures/s9_002_postflash_probe_bundle_20260304_111843.json`.
- Final outcome: all key S9-002 controls pass at runtime (`AUTH-01`, `AUTH-02`, `PATH-01`, `PATH-02`, `UPLOAD-02`, `EBIN-02`).

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

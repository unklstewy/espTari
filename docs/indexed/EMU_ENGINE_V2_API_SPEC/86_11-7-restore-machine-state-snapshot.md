# 11.7 Restore machine state snapshot

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 86

## 11.7 Restore machine state snapshot

- `POST /api/v2/engine/state/restore`

Request:

```json
{
  "session_id": "ses_01H...",
  "snapshot_id": "snap_01H...",
  "resume_mode": "paused"
}
```

Compatibility rules:

- Snapshot profile must match active machine profile or return `SNAPSHOT_INCOMPATIBLE`.
- Snapshot ABI requirements must be satisfied by active engine/module versions.
- Missing snapshots return `SNAPSHOT_NOT_FOUND`.

Restore compatibility rule matrix (`restore_compatibility_matrix_v1`):

| Rule ID | Dimension | Source field(s) | Runtime field(s) | Predicate | Failure code |
|---|---|---|---|---|---|
| `RCOMP-01` | schema | `snapshot.schema_version` | `engine.accepted_snapshot_schema_versions[]` | `snapshot.schema_version` is present in accepted schema list | `SNAPSHOT_INCOMPATIBLE` |
| `RCOMP-02` | profile | `snapshot.profile` | `session.profile` | exact string equality | `SNAPSHOT_INCOMPATIBLE` |
| `RCOMP-03` | ABI engine | `snapshot.abi.engine` | `engine.abi.engine` | exact semver match | `SNAPSHOT_INCOMPATIBLE` |
| `RCOMP-04` | ABI modules | `snapshot.abi.modules{module_id:version}` | `engine.abi.modules{module_id:version}` | every required module exists and version matches exactly | `SNAPSHOT_INCOMPATIBLE` |

Restore compatibility evaluation contract:

- Compatibility checks execute in deterministic order: `RCOMP-01 -> RCOMP-02 -> RCOMP-03 -> RCOMP-04`.
- First failing rule terminates evaluation and becomes `details.rule_id` in error payload.
- Missing snapshot identity must short-circuit compatibility evaluation and return `SNAPSHOT_NOT_FOUND`.

Restore compatibility deterministic guard failures:

- Malformed restore payload (`session_id`, `snapshot_id`, `resume_mode`) -> `BAD_REQUEST`.
- Unknown/inactive `session_id` -> `ENGINE_NOT_RUNNING`.
- Snapshot record absent for `snapshot_id` -> `SNAPSHOT_NOT_FOUND`.
- Any failed compatibility rule (`RCOMP-01..04`) -> `SNAPSHOT_INCOMPATIBLE`.

Restore compatibility validator contract:

- `POST /api/v2/engine/state/restore/validate`
- Request payload is validated by `restore_compatibility_validate_request_v1`.
- `restore_compatibility_validate_request_v1` required fields:
  - `session_id` (string, required)
  - `snapshot_id` (string, required)
  - `strict` (boolean, optional, default `true`)
- Response `data` is validated by `restore_compatibility_validate_result_v1`.
- `restore_compatibility_validate_result_v1` required fields:
  - `snapshot_id` (string)
  - `compatible` (boolean)
  - `evaluated_rules` (array)
  - `failed_rule_id` (string or `null`)
  - `error_code` (string or `null`)
  - `validated_at_us` (uint64)

Restore compatibility validator checks:

- `RCOMP-VAL-01`: validator executes rules in deterministic order `RCOMP-01..04`.
- `RCOMP-VAL-02`: exactly one terminal outcome is emitted: `compatible=true` with `failed_rule_id=null` OR `compatible=false` with non-null `failed_rule_id`.
- `RCOMP-VAL-03`: on first failed rule, validator must stop evaluating subsequent rules.
- `RCOMP-VAL-04`: when `strict=true`, compatibility mismatch must map to terminal `error_code=SNAPSHOT_INCOMPATIBLE`.

Restore compatibility error mapping matrix:

| Failure source | Rule/guard | error.code | error.category | retryable |
|---|---|---|---|---|
| Request payload invalid | validator input guard | `BAD_REQUEST` | `request` | `false` |
| Session identity unresolved | session guard | `ENGINE_NOT_RUNNING` | `engine` | `false` |
| Snapshot identifier unresolved | snapshot lookup guard | `SNAPSHOT_NOT_FOUND` | `snapshot` | `false` |
| Schema/profile/ABI mismatch | `RCOMP-01..04` | `SNAPSHOT_INCOMPATIBLE` | `snapshot` | `false` |

Restore compatibility validator request example:

```json
{
  "session_id": "ses_01H...",
  "snapshot_id": "snap_01H...",
  "strict": true
}
```

Restore compatibility validator response (`data`) example:

```json
{
  "snapshot_id": "snap_01H...",
  "compatible": false,
  "evaluated_rules": [
    {"rule_id": "RCOMP-01", "result": "pass"},
    {"rule_id": "RCOMP-02", "result": "pass"},
    {"rule_id": "RCOMP-03", "result": "fail"}
  ],
  "failed_rule_id": "RCOMP-03",
  "error_code": "SNAPSHOT_INCOMPATIBLE",
  "validated_at_us": 1710000005004
}
```

Restore compatibility matrix pass example (`data` excerpt):

```json
{
  "snapshot_id": "snap_01H...",
  "evaluated_rules": [
    {"rule_id": "RCOMP-01", "result": "pass"},
    {"rule_id": "RCOMP-02", "result": "pass"},
    {"rule_id": "RCOMP-03", "result": "pass"},
    {"rule_id": "RCOMP-04", "result": "pass"}
  ],
  "compatible": true
}
```

Restore compatibility failure example (`SNAPSHOT_INCOMPATIBLE`):

```json
{
  "ok": false,
  "request_id": "req_01H...",
  "timestamp_us": 1710000005005,
  "error": {
    "code": "SNAPSHOT_INCOMPATIBLE",
    "category": "snapshot",
    "message": "Snapshot compatibility rule failed",
    "retryable": false,
    "details": {
      "snapshot_id": "snap_01H...",
      "rule_id": "RCOMP-03",
      "source_value": "2.1.0",
      "runtime_value": "2.0.0"
    }
  }
}
```

---

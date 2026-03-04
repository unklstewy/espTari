# EBIN Runtime ABI Contract v1 (S5-001)

Status: Draft for implementation
Version: 1.0
Phase: S5 runtime unlock

## 1. Purpose

This contract defines the minimum ABI and metadata requirements that an EBIN module must satisfy before the runtime can load and bind it.

Goals:
- deterministic load admission,
- explicit compatibility gating,
- stable rejection semantics for invalid/incompatible modules.

## 2. Required EBIN Header/Manifest Fields

Each EBIN module MUST provide the fields below.

| Field | Type | Required | Notes |
|---|---|---|---|
| `module_id` | string | yes | Stable module identifier, e.g. `st.cpu.m68k` |
| `module_type` | enum | yes | One of: `cpu`, `video`, `io`, `storage`, `audio`, `machine_profile` |
| `machine_targets` | array[string] | yes | At minimum includes `atari_st` for current scope |
| `abi_version` | string | yes | Semantic version (`MAJOR.MINOR.PATCH`) |
| `api_contract_version` | string | yes | Contract bundle version for interface mapping |
| `exports` | array[string] | yes | Required exported symbols / entrypoints |
| `dependencies` | array[object] | yes | See dependency schema below |
| `build_fingerprint` | string | yes | Deterministic build ID/hash reference |
| `payload_sha256` | string | yes | Hash of module payload |
| `signature` | object | recommended | Signature metadata block (algorithm/key ID/signature bytes) |

Dependency object schema:

| Field | Type | Required | Notes |
|---|---|---|---|
| `module_id` | string | yes | Dependency module ID |
| `abi_range` | string | yes | Semver range accepted by dependent module |
| `required` | bool | yes | If true, load fails when unresolved |

## 3. Compatibility Rules

Runtime compatibility is evaluated in this order:

1. `module_type` is recognized and valid for active profile wiring.
2. `machine_targets` contains current machine profile.
3. `abi_version` satisfies host-supported range for this module type.
4. `api_contract_version` is supported by runtime contract bridge.
5. Dependency graph is resolvable and each required dependency ABI range is satisfied.
6. Integrity/signature checks pass.

Load must fail fast on first failed rule.

## 4. Host Supported ABI Matrix (Initial S5 Baseline)

| Module Type | Supported ABI Range | Notes |
|---|---|---|
| `cpu` | `1.0.x` | 68000 baseline contract |
| `video` | `1.0.x` | ST baseline video bridge |
| `io` | `1.0.x` | ACIA/IKBD and peripheral path |
| `storage` | `1.0.x` | DMA/FDC baseline path |
| `audio` | `1.0.x` | PSG/ST baseline audio bridge |
| `machine_profile` | `1.0.x` | Wiring/defaults contract |

## 5. Deterministic Rejection Semantics

The first failing gate returns canonical error envelope mappings:

| Failure Class | Error Code | Category | Retryable |
|---|---|---|---|
| Missing/invalid required field | `EBIN_INVALID` | `ebin` | false |
| Unsupported module type/profile mismatch | `EBIN_INVALID` | `ebin` | false |
| ABI range mismatch | `EBIN_ABI_MISMATCH` | `ebin` | false |
| Missing required dependency | `EBIN_DEPENDENCY_MISSING` | `ebin` | false |
| Signature check failure | `EBIN_SIGNATURE_INVALID` | `ebin` | false |
| Corrupt payload/hash mismatch | `EBIN_INVALID` | `ebin` | false |
| Runtime guard/internal validation failure | `INTERNAL_ERROR` | `internal` | false |

Required error details fields by class:
- ABI mismatch: `module_id`, `required_range`, `actual_abi`
- Dependency missing: `module_id`, `missing_dependency_id`, `required_range`
- Signature invalid: `module_id`, `key_id`, `algorithm`

## 6. Traceability Map

| Contract Area | Planned Implementation Anchors |
|---|---|
| Manifest/header validation | `components/esptari_loader/*` and/or `components/esptari_core/*` loader pipeline helpers |
| Compatibility gate execution | runtime loader admission sequence in core/loader orchestration |
| Error envelope mapping | `components/esptari_web/*` canonical error response path |
| Dependency resolution | SD-card module catalog/index resolver layer |

## 7. Implementation Notes for S5-002/S5-003

- S5-002 implements structural/schema validation and deterministic `EBIN_INVALID` paths.
- S5-003 implements ordered safety gates (integrity/signature/dependency compatibility) and fail-fast behavior.
- Both tasks must preserve canonical `/api/v2` envelope/error taxonomy.

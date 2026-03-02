# Backlog and Task Index

Legend:

- Priority: P0 (highest) to P3 (lowest)
- Size: XS (<=4h), S (<=1 day), M (1-2 days), L (>2 days, must split before Ready)
- Status: Backlog, Ready, In Progress, In Review, Acceptance, Done, Blocked

AI-speed policy:

- Target pullable task size: XS/S only.
- M tasks should be split whenever parallelization is possible.
- L tasks are planning placeholders and not pullable until decomposed.

## Task table

| ID | Epic | Task | Priority | Size | Sprint Target | Status | Dependencies |
|---|---|---|---|---|---|---|---|
| T-001 | EPIC-01 | Finalize v2 API schema baseline (umbrella, non-pullable) | P0 | M | S1 | Backlog | T-045, T-046, T-047 |
| T-002 | EPIC-01 | Implement session state transition guard rules (umbrella, non-pullable) | P0 | M | S1 | Backlog | T-054, T-055 |
| T-003 | EPIC-01 | Add engine health/status payload contract (umbrella, non-pullable) | P1 | S | S1 | Backlog | T-048, T-049, T-050 |
| T-004 | EPIC-04 | Build catalog resolver for rom_id/disk_id/tos_id (umbrella, non-pullable) | P0 | M | S1 | Backlog | T-056, T-057 |
| T-005 | EPIC-04 | Implement missing-asset detection against SD-card (umbrella, non-pullable) | P0 | M | S1 | Backlog | T-058, T-059 |
| T-006 | EPIC-04 | Implement single-entry hosted download pipeline (umbrella, non-pullable) | P0 | M | S1 | Backlog | T-060, T-061 |
| T-007 | EPIC-04 | Add dead-link probe and mark-dead workflow (umbrella, non-pullable) | P1 | M | S3 | Backlog | T-062, T-063 |
| T-008 | EPIC-04 | Add scheduled catalog-sync job runner (umbrella, non-pullable) | P1 | L | S3 | Backlog | T-064, T-065 |
| T-009 | EPIC-03 | Implement browser input enable/disable policy (umbrella, non-pullable) | P0 | S | S1 | Backlog | T-051, T-052, T-053 |
| T-010 | EPIC-03 | Implement `mouse_over` capture mode behavior (umbrella, non-pullable) | P0 | M | S3 | Backlog | T-066, T-067 |
| T-011 | EPIC-03 | Implement `click_to_capture` + escape release (umbrella, non-pullable) | P0 | M | S3 | Backlog | T-068, T-069 |
| T-012 | EPIC-03 | Implement input mappings load/update/active APIs (umbrella, non-pullable) | P1 | M | S3 | Backlog | T-070, T-071 |
| T-013 | EPIC-03 | Implement input translation event stream (umbrella, non-pullable) | P1 | M | S3 | Backlog | T-072, T-073 |
| T-014 | EPIC-02 | Implement Atari ST profile load path (umbrella, non-pullable) | P0 | L | S2 | Backlog | T-074, T-075 |
| T-015 | EPIC-02 | Implement deterministic scheduler skeleton (umbrella, non-pullable) | P0 | L | S2 | Backlog | T-076, T-077 |
| T-016 | EPIC-02 | Implement ROM attach via catalog-backed IDs (umbrella, non-pullable) | P0 | M | S2 | Backlog | T-078, T-079 |
| T-017 | EPIC-02 | Implement disk attach/eject via catalog-backed IDs (umbrella, non-pullable) | P0 | M | S2 | Backlog | T-080, T-081 |
| T-018 | EPIC-05 | Implement video stream metadata + payload channel (umbrella, non-pullable) | P1 | M | S2 | Backlog | T-082, T-083 |
| T-019 | EPIC-05 | Implement audio stream metadata + payload channel (umbrella, non-pullable) | P1 | M | S2 | Backlog | T-084, T-085 |
| T-020 | EPIC-05 | Implement register snapshot + stream (umbrella, non-pullable) | P1 | M | S4 | Backlog | T-086, T-087 |
| T-021 | EPIC-05 | Implement bus/memory filtered stream (umbrella, non-pullable) | P1 | L | S4 | Backlog | T-088, T-089 |
| T-022 | EPIC-05 | Implement stream backpressure telemetry (umbrella, non-pullable) | P1 | M | S4 | Backlog | T-090, T-091 |
| T-023 | EPIC-06 | Implement conformance evidence harness (umbrella, non-pullable) | P1 | M | S4 | Backlog | T-092, T-093 |
| T-024 | EPIC-06 | Run acceptance checklist and produce review pack (umbrella, non-pullable) | P0 | M | S4 | Backlog | T-094, T-095 |
| T-025 | EPIC-07 | Implement GLUE/MMU/SHIFTER component contracts from scratch (umbrella, non-pullable) | P0 | L | S2 | Backlog | T-096, T-097 |
| T-026 | EPIC-07 | Implement MFP component contract from scratch (umbrella, non-pullable) | P0 | M | S2 | Backlog | T-098, T-099 |
| T-027 | EPIC-07 | Implement ACIA pair + IKBD pipeline contract from scratch (umbrella, non-pullable) | P0 | L | S3 | Backlog | T-100, T-101 |
| T-028 | EPIC-07 | Implement DMA/FDC path contract from scratch (umbrella, non-pullable) | P0 | L | S3 | Backlog | T-102, T-103 |
| T-029 | EPIC-07 | Implement PSG audio + GPIO behavior contract from scratch (umbrella, non-pullable) | P1 | M | S3 | Backlog | T-104, T-105 |
| T-030 | EPIC-07 | Implement interrupt hierarchy wiring and vector contract (umbrella, non-pullable) | P0 | M | S3 | Backlog | T-106, T-107 |
| T-031 | EPIC-07 | Implement startup/reset defaults and power-on sequence contract (umbrella, non-pullable) | P0 | M | S3 | Backlog | T-108, T-109 |
| T-032 | EPIC-07 | Add component-level conformance tests per subsystem (umbrella, non-pullable) | P0 | M | S4 | Backlog | T-110, T-111 |
| T-033 | EPIC-01 | Implement suspend-save and restore-resume session lifecycle APIs (umbrella, non-pullable) | P0 | M | S4 | Backlog | T-112, T-113 |
| T-034 | EPIC-05 | Implement machine save-state snapshot schema and persistence store (umbrella, non-pullable) | P0 | L | S4 | Backlog | T-039, T-040, T-041, T-042, T-043, T-044 |
| T-035 | EPIC-05 | Implement state restore validation and ABI/profile compatibility checks (umbrella, non-pullable) | P0 | M | S4 | Backlog | T-114, T-115 |
| T-036 | EPIC-05 | Implement hard performance SLO metrics pipeline and API endpoints (umbrella, non-pullable) | P0 | M | S4 | Backlog | T-116, T-117 |
| T-037 | EPIC-05 | Implement debug clock control modes realtime and slow_motion (umbrella, non-pullable) | P1 | M | S4 | Backlog | T-118, T-119 |
| T-038 | EPIC-05 | Implement single-step execution and opcode/bus-error capture flow (umbrella, non-pullable) | P1 | M | S4 | Backlog | T-120, T-121 |
| T-039 | EPIC-05 | Define snapshot schema v1 and component state block contracts | P0 | S | S4 | Backlog | T-033 |
| T-040 | EPIC-05 | Implement component state serializers for ST baseline modules | P0 | S | S4 | Backlog | T-039 |
| T-041 | EPIC-05 | Implement snapshot persistence backend with hash and atomic write | P0 | S | S4 | Backlog | T-039 |
| T-042 | EPIC-05 | Implement snapshot index and list metadata API path | P1 | XS | S4 | Backlog | T-041 |
| T-043 | EPIC-05 | Implement suspend-save transaction guards and rollback path | P0 | S | S4 | Backlog | T-040, T-041 |
| T-044 | EPIC-05 | Implement restore read path and minimum state integrity validation | P0 | S | S4 | Backlog | T-040, T-041, T-042 |
| T-045 | EPIC-01 | Define API envelope, versioning, and canonical error schema | P0 | S | S1 | Ready | None |
| T-046 | EPIC-01 | Define lifecycle endpoint request/response schema set | P0 | S | S1 | Ready | T-045 |
| T-047 | EPIC-01 | Publish API schema baseline bundle with examples and cross-links | P0 | XS | S1 | Ready | T-046 |
| T-048 | EPIC-01 | Define `/api/v2/engine/health` payload contract and enums | P1 | XS | S1 | Ready | T-047 |
| T-049 | EPIC-01 | Define `/api/v2/engine/status` payload contract and session/runtime fields | P1 | S | S1 | Ready | T-047 |
| T-050 | EPIC-01 | Define health/status WS event payload contract and ordering notes | P1 | XS | S1 | Ready | T-048, T-049 |
| T-051 | EPIC-03 | Define browser input policy state model and error envelopes | P0 | XS | S1 | Ready | T-047 |
| T-052 | EPIC-03 | Define input enable/disable endpoint schema and idempotency behavior | P0 | S | S1 | Ready | T-051 |
| T-053 | EPIC-03 | Define input-policy change events and transition ordering rules | P0 | XS | S1 | Ready | T-052 |
| T-054 | EPIC-01 | Define lifecycle transition matrix and guard predicates | P0 | XS | S1 | Ready | T-047 |
| T-055 | EPIC-01 | Implement lifecycle guard validator responses and error mapping | P0 | S | S1 | Backlog | T-054 |
| T-056 | EPIC-04 | Build catalog index loader for `rom_id` `disk_id` `tos_id` | P0 | S | S1 | Ready | T-047 |
| T-057 | EPIC-04 | Implement catalog-backed ID resolution API path and error handling | P0 | S | S1 | Backlog | T-056 |
| T-058 | EPIC-04 | Implement SD-card asset scan and canonical presence index | P0 | S | S1 | Backlog | T-057 |
| T-059 | EPIC-04 | Implement missing-asset diff report and API projection | P0 | XS | S1 | Backlog | T-058 |
| T-060 | EPIC-04 | Implement hosted download request validation and enqueue path | P0 | S | S1 | Backlog | T-059 |
| T-061 | EPIC-04 | Implement staged download commit with integrity/hash verification | P0 | S | S1 | Backlog | T-060 |
| T-062 | EPIC-04 | Implement dead-link probe worker and timeout policy | P1 | S | S3 | Backlog | T-061 |
| T-063 | EPIC-04 | Implement dead-link mark/retry state machine and telemetry fields | P1 | XS | S3 | Backlog | T-062 |
| T-064 | EPIC-04 | Implement catalog-sync scheduler core and persisted schedules | P1 | S | S3 | Backlog | T-063 |
| T-065 | EPIC-04 | Implement schedule CRUD API and reboot-recovery validation | P1 | S | S3 | Backlog | T-064 |
| T-066 | EPIC-03 | Implement `mouse_over` capture state machine | P0 | S | S3 | Ready | T-053 |
| T-067 | EPIC-03 | Implement `mouse_over` enter/leave event hooks and release behavior | P0 | XS | S3 | Backlog | T-066 |
| T-068 | EPIC-03 | Implement `click_to_capture` acquisition state machine | P0 | S | S3 | Ready | T-053 |
| T-069 | EPIC-03 | Implement escape-release and focus-recovery behavior | P0 | XS | S3 | Backlog | T-068 |
| T-070 | EPIC-03 | Implement input mapping schema and persistence model | P1 | S | S3 | Ready | T-053 |
| T-071 | EPIC-03 | Implement mapping CRUD and active-profile apply path | P1 | S | S3 | Backlog | T-070 |
| T-072 | EPIC-03 | Define input translation event payload contract and ordering fields | P1 | XS | S3 | Backlog | T-071 |
| T-073 | EPIC-03 | Implement input translation event stream emitter and sequencing checks | P1 | S | S3 | Backlog | T-072 |
| T-074 | EPIC-02 | Implement ST profile manifest parser and schema validation | P0 | S | S2 | Backlog | T-055, T-057 |
| T-075 | EPIC-02 | Implement profile wiring validation and fail-fast diagnostics | P0 | S | S2 | Backlog | T-074 |
| T-076 | EPIC-02 | Implement deterministic tick-loop scheduler core | P0 | S | S2 | Backlog | T-075 |
| T-077 | EPIC-02 | Implement arbitration hook layer and deterministic timestamp emitter | P0 | S | S2 | Backlog | T-076 |
| T-078 | EPIC-02 | Implement ROM attach request validation and catalog binding checks | P0 | XS | S2 | Backlog | T-075 |
| T-079 | EPIC-02 | Implement ROM mount/apply flow and attach status events | P0 | S | S2 | Backlog | T-078 |
| T-080 | EPIC-02 | Implement disk attach/eject request validation and binding checks | P0 | XS | S2 | Backlog | T-075 |
| T-081 | EPIC-02 | Implement disk mount/eject runtime flow and state events | P0 | S | S2 | Backlog | T-080 |
| T-082 | EPIC-05 | Define video stream metadata channel contract and schema | P1 | XS | S2 | Backlog | T-077 |
| T-083 | EPIC-05 | Implement video payload stream emitter and pacing controls | P1 | S | S2 | Backlog | T-082 |
| T-084 | EPIC-05 | Define audio stream metadata channel contract and schema | P1 | XS | S2 | Backlog | T-077 |
| T-085 | EPIC-05 | Implement audio payload stream emitter and pacing controls | P1 | S | S2 | Backlog | T-084 |
| T-086 | EPIC-05 | Define register snapshot schema and selective filter fields | P1 | XS | S4 | Backlog | T-077 |
| T-087 | EPIC-05 | Implement register snapshot stream publisher and validation checks | P1 | S | S4 | Backlog | T-086 |
| T-088 | EPIC-05 | Define bus/memory filter request model and guard rules | P1 | S | S4 | Backlog | T-087 |
| T-089 | EPIC-05 | Implement filtered bus/memory stream and load validation run | P1 | S | S4 | Backlog | T-088 |
| T-090 | EPIC-05 | Implement stream backpressure counters and watermark metrics | P1 | XS | S4 | Backlog | T-083, T-085, T-089 |
| T-091 | EPIC-05 | Expose backpressure telemetry via API and event streams | P1 | S | S4 | Backlog | T-090 |
| T-092 | EPIC-06 | Build conformance harness scaffold and test manifest loader | P1 | S | S4 | Backlog | T-075, T-083, T-085 |
| T-093 | EPIC-06 | Implement evidence artifact collection and report packaging flow | P1 | S | S4 | Backlog | T-092 |
| T-094 | EPIC-06 | Implement acceptance checklist execution runner | P0 | S | S4 | Backlog | T-093 |
| T-095 | EPIC-06 | Implement review pack generation and signoff bundle assembly | P0 | XS | S4 | Backlog | T-094 |
| T-096 | EPIC-07 | Implement GLUE/MMU/SHIFTER register and memory window model | P0 | S | S2 | Backlog | T-077 |
| T-097 | EPIC-07 | Implement GLUE/MMU/SHIFTER arbitration and timing integration checks | P0 | S | S2 | Backlog | T-096 |
| T-098 | EPIC-07 | Implement MFP register and timer model contracts | P0 | S | S2 | Backlog | T-077 |
| T-099 | EPIC-07 | Implement MFP interrupt emission behaviors and conformance checks | P0 | S | S2 | Backlog | T-098 |
| T-100 | EPIC-07 | Implement ACIA host channel bridge and framing rules | P0 | S | S3 | Backlog | T-099 |
| T-101 | EPIC-07 | Implement IKBD parser bridge and keyboard/mouse packet timing path | P0 | S | S3 | Backlog | T-100 |
| T-102 | EPIC-07 | Implement DMA request pacing model and arbitration hooks | P0 | S | S3 | Backlog | T-077, T-081 |
| T-103 | EPIC-07 | Implement FDC command/status FSM bridge and terminal conditions | P0 | S | S3 | Backlog | T-102 |
| T-104 | EPIC-07 | Implement PSG register/audio behavior contract | P1 | S | S3 | Backlog | T-077 |
| T-105 | EPIC-07 | Implement PSG GPIO behavior contract and validation checks | P1 | XS | S3 | Backlog | T-104 |
| T-106 | EPIC-07 | Define interrupt hierarchy map and vector routing contract | P0 | S | S3 | Backlog | T-097, T-099, T-101, T-103 |
| T-107 | EPIC-07 | Implement interrupt wiring integration checks across subsystems | P0 | S | S3 | Backlog | T-106 |
| T-108 | EPIC-07 | Define startup defaults and power-on register baseline table | P0 | XS | S3 | Backlog | T-107 |
| T-109 | EPIC-07 | Implement reset/startup sequence executor and verification checks | P0 | S | S3 | Backlog | T-108 |
| T-110 | EPIC-07 | Build subsystem conformance test scaffold and fixture model | P0 | S | S4 | Backlog | T-109 |
| T-111 | EPIC-07 | Implement per-subsystem acceptance suites and reporting output | P0 | S | S4 | Backlog | T-110 |
| T-112 | EPIC-01 | Implement suspend-save request contract wiring and transition checks | P0 | S | S4 | Backlog | T-055, T-093 |
| T-113 | EPIC-01 | Implement restore-resume transition guards and error semantics | P0 | S | S4 | Backlog | T-112 |
| T-114 | EPIC-05 | Define restore compatibility rule matrix (schema/abi/profile) | P0 | XS | S4 | Backlog | T-044 |
| T-115 | EPIC-05 | Implement restore compatibility validator and error mapping | P0 | S | S4 | Backlog | T-114 |
| T-116 | EPIC-05 | Implement performance SLO metric collectors and sampling pipeline | P0 | S | S4 | Backlog | T-083, T-085, T-091 |
| T-117 | EPIC-05 | Expose SLO endpoints and threshold breach alarm events | P0 | S | S4 | Backlog | T-116 |
| T-118 | EPIC-05 | Define realtime/slow-motion clock control model and bounds | P1 | XS | S4 | Backlog | T-077 |
| T-119 | EPIC-05 | Implement clock mode API and deterministic mode transition flow | P1 | S | S4 | Backlog | T-118 |
| T-120 | EPIC-05 | Implement single-step execution control API and scheduler hook | P1 | S | S4 | Backlog | T-119, T-087, T-089 |
| T-121 | EPIC-05 | Implement opcode/bus-error capture path and diagnostic payloads | P1 | S | S4 | Backlog | T-120 |

## Post-tranche implementation-readiness tasks (CRT)

| ID | Epic | Task | Priority | Size | Sprint Target | Status | Dependencies |
|---|---|---|---|---|---|---|---|
| CRT-001 | EPIC-06 | Prepare lifecycle transition and guard implementation-readiness pack | P0 | S | S5 | In Progress | T-054, T-055 |
| CRT-002 | EPIC-06 | Prepare input mapping CRUD/apply implementation-readiness pack | P0 | S | S5 | In Progress | CRT-001 |
| CRT-003 | EPIC-06 | Prepare save/restore compatibility implementation-readiness pack | P0 | S | S5 | In Progress | CRT-002 |
| CRT-004 | EPIC-06 | Prepare observability stream/telemetry implementation-readiness pack | P1 | S | S5 | In Progress | CRT-003 |
| CRT-005 | EPIC-06 | Assemble CRT readiness handoff pack for runtime phase gate | P0 | S | S5 | In Progress | CRT-004 |

Phase Gate note:
- CRT tasks in this tranche are planning/documentation tasks only.
- Runtime/API verification (curl/HTTP probes) is blocked until core API/runtime implementation exists and the phase gate in `TRACKING/CONTRACT_TO_RUNTIME_VERIFICATION_TASKS.md` is explicitly unlocked.

## S5 runtime unlock prerequisite tasks (PRQ)

| ID | Epic | Task | Priority | Size | Sprint Target | Status | Dependencies |
|---|---|---|---|---|---|---|---|
| PRQ-001 | EPIC-06 | Close prerequisite for core lifecycle/input/save-restore/observability runtime code paths | P0 | S | S5 | In Progress | CRT-005 |
| PRQ-002 | EPIC-06 | Deliver deterministic fixture/scenario package for CRT vectors | P0 | S | S5 | Backlog | PRQ-001 |
| PRQ-003 | EPIC-06 | Produce reproducible firmware/app deployment workflow documentation | P0 | S | S5 | Backlog | PRQ-002 |
| PRQ-004 | EPIC-06 | Assemble unlock review packet and decision-ready PO package | P0 | XS | S5 | Backlog | PRQ-003 |

PRQ phase note:
- PRQ tasks are prerequisite-closure documentation/design/code-ready tasks only.
- Runtime/API/build/flash/test execution remains blocked until explicit unlock decision.

## Decomposition queue (required before pull)

No pending decomposition for currently indexed `M/L` tasks.

All currently indexed `M/L` tasks are now represented as non-pullable umbrella trackers with `XS/S` child tasks.

## Decomposed task status

- T-034 is decomposed into pullable tasks T-039 through T-044.
- T-034 remains as an umbrella tracker and is not pullable.
- T-001 is decomposed into pullable tasks T-045 through T-047.
- T-003 is decomposed into pullable tasks T-048 through T-050.
- T-009 is decomposed into pullable tasks T-051 through T-053.
- T-002 is decomposed into pullable tasks T-054 through T-055.
- T-004 is decomposed into pullable tasks T-056 through T-057.
- T-005 is decomposed into pullable tasks T-058 through T-059.
- T-006 is decomposed into pullable tasks T-060 through T-061.
- T-007 is decomposed into pullable tasks T-062 through T-063.
- T-008 is decomposed into pullable tasks T-064 through T-065.
- T-010 is decomposed into pullable tasks T-066 through T-067.
- T-011 is decomposed into pullable tasks T-068 through T-069.
- T-012 is decomposed into pullable tasks T-070 through T-071.
- T-013 is decomposed into pullable tasks T-072 through T-073.
- T-014 is decomposed into pullable tasks T-074 through T-075.
- T-015 is decomposed into pullable tasks T-076 through T-077.
- T-016 is decomposed into pullable tasks T-078 through T-079.
- T-017 is decomposed into pullable tasks T-080 through T-081.
- T-018 is decomposed into pullable tasks T-082 through T-083.
- T-019 is decomposed into pullable tasks T-084 through T-085.
- T-020 is decomposed into pullable tasks T-086 through T-087.
- T-021 is decomposed into pullable tasks T-088 through T-089.
- T-022 is decomposed into pullable tasks T-090 through T-091.
- T-023 is decomposed into pullable tasks T-092 through T-093.
- T-024 is decomposed into pullable tasks T-094 through T-095.
- T-025 is decomposed into pullable tasks T-096 through T-097.
- T-026 is decomposed into pullable tasks T-098 through T-099.
- T-027 is decomposed into pullable tasks T-100 through T-101.
- T-028 is decomposed into pullable tasks T-102 through T-103.
- T-029 is decomposed into pullable tasks T-104 through T-105.
- T-030 is decomposed into pullable tasks T-106 through T-107.
- T-031 is decomposed into pullable tasks T-108 through T-109.
- T-032 is decomposed into pullable tasks T-110 through T-111.
- T-033 is decomposed into pullable tasks T-112 through T-113.
- T-035 is decomposed into pullable tasks T-114 through T-115.
- T-036 is decomposed into pullable tasks T-116 through T-117.
- T-037 is decomposed into pullable tasks T-118 through T-119.
- T-038 is decomposed into pullable tasks T-120 through T-121.

## Detailed task cards (T-039 to T-053)

Detailed task cards for T-054 through T-121 are maintained in `TRACKING/TASK_CARDS_T054_T121.md`.

### TASK-ID: T-039

- Epic: EPIC-05
- Objective: Define machine snapshot schema v1 that is deterministic, serializable, and restorable.
- Scope:
	- Define required snapshot metadata fields (`snapshot_id`, `schema_version`, `profile`, `abi`, `hash`, `created_at`).
	- Define per-component state block contracts for ST baseline modules (CPU, GLUE/MMU/SHIFTER, MFP, ACIA/IKBD, DMA/FDC, PSG).
	- Define scheduler and media binding state fields required for deterministic restore.
- Out of scope:
	- Persistence storage implementation.
	- Binary compression optimization.
- Dependencies: T-033
- Risks:
	- Missing fields may cause non-deterministic restore behavior.

Acceptance criteria:

1. Snapshot schema v1 document is committed and referenced by API and implementation plan docs.
2. Each ST baseline component has a defined state block contract with mandatory fields.
3. Schema includes compatibility metadata required for restore validation.

Evidence required:

- API evidence: request/response examples align with schema fields in API spec.
- Runtime evidence: serializer stubs compile against schema contract definitions.
- Docs updated: implementation plan and API spec cross-reference schema v1.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-040

- Epic: EPIC-05
- Objective: Implement component state serializers for all ST baseline modules.
- Scope:
	- Implement serialize/deserialize routines for component state blocks defined in T-039.
	- Ensure deterministic field ordering and explicit endianness handling.
	- Add serializer-level unit checks for missing/invalid fields.
- Out of scope:
	- Snapshot file I/O backend.
	- ABI compatibility validation logic.
- Dependencies: T-039
- Risks:
	- Partial serializers can produce silently incomplete snapshots.

Acceptance criteria:

1. Each baseline component exposes serializer and deserializer functions.
2. Serialization output is deterministic for identical runtime state.
3. Invalid or incomplete component blocks are rejected with clear error signals.

Evidence required:

- API evidence: sample serialized payload fragments map to schema fields.
- Runtime evidence: deterministic serializer tests pass for repeated runs.
- Docs updated: serializer coverage matrix added or updated.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-041

- Epic: EPIC-05
- Objective: Implement snapshot persistence backend with integrity and atomicity guarantees.
- Scope:
	- Persist serialized snapshots to SD-card under controlled path policy.
	- Compute and store snapshot hash/checksum metadata.
	- Use atomic write strategy (staging + rename) to prevent partial snapshot corruption.
- Out of scope:
	- Snapshot listing API payload shaping.
	- Restore compatibility checks.
- Dependencies: T-039
- Risks:
	- Power loss during writes may corrupt snapshots without atomic guards.

Acceptance criteria:

1. Snapshot write path is atomic and recoverable after interruption.
2. Hash/checksum is generated and validated on read.
3. Persistence errors surface explicit error codes and messages.

Evidence required:

- API evidence: save responses include hash and persistence metadata fields.
- Runtime evidence: interrupted-write simulation confirms no partial committed snapshot.
- Docs updated: persistence path and integrity behavior documented.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-042

- Epic: EPIC-05
- Objective: Implement snapshot index and list metadata API path.
- Scope:
	- Implement snapshot index structure and update behavior on create/delete.
	- Expose `GET /api/v2/engine/state/list` metadata view.
	- Support basic filtering by session/profile/timestamp.
- Out of scope:
	- Snapshot payload restore path.
	- Deep query/search feature set.
- Dependencies: T-041
- Risks:
	- Index drift may hide valid snapshots or show stale entries.

Acceptance criteria:

1. List endpoint returns complete and consistent metadata for persisted snapshots.
2. Index remains consistent across reboot and rescan.
3. Missing or corrupted entries are flagged and do not crash listing endpoint.

Evidence required:

- API evidence: endpoint output samples for empty, single, and multi-snapshot states.
- Runtime evidence: reboot-resume list consistency check passes.
- Docs updated: list endpoint and metadata fields documented.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-043

- Epic: EPIC-05
- Objective: Implement suspend-save transaction guards and rollback behavior.
- Scope:
	- Enforce valid lifecycle transition into `suspended` for save operation.
	- Ensure suspend-save is all-or-nothing with rollback to prior stable state on failure.
	- Emit state transition and failure diagnostics for observability.
- Out of scope:
	- Full restore compatibility matrix.
	- Debug clock mode controls.
- Dependencies: T-040, T-041
- Risks:
	- Partial suspend path may leave runtime in inconsistent state.

Acceptance criteria:

1. Suspend-save endpoint reaches `suspended` only when snapshot commit succeeds.
2. Any save failure triggers rollback to prior running/paused state without state loss.
3. Transition and rollback events are emitted with deterministic ordering.

Evidence required:

- API evidence: successful and failed suspend-save response examples.
- Runtime evidence: fault-injection run confirms rollback correctness.
- Docs updated: lifecycle transition table reflects suspend-save semantics.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-044

- Epic: EPIC-05
- Objective: Implement restore read path and minimum integrity validation before resume.
- Scope:
	- Load persisted snapshot and validate schema/hash integrity.
	- Rehydrate baseline component states into runtime.
	- Reject invalid snapshots with explicit errors and preserve current session stability.
- Out of scope:
	- Full ABI/profile compatibility policy (handled by T-035).
	- Performance SLO instrumentation.
- Dependencies: T-040, T-041, T-042
- Risks:
	- Invalid restore could corrupt active runtime state if not guarded.

Acceptance criteria:

1. Restore succeeds for valid snapshots and returns deterministic state resume metadata.
2. Corrupt or mismatched snapshot payloads are rejected safely.
3. Failed restore does not alter active session state.

Evidence required:

- API evidence: restore success and failure examples with error envelopes.
- Runtime evidence: corrupt snapshot test confirms safe rejection behavior.
- Docs updated: restore flow and validation boundaries documented.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-045

- Epic: EPIC-01
- Objective: Define the canonical API envelope, versioning markers, and error schema baseline.
- Scope:
	- Define success and error envelope fields used by all v2 endpoints.
	- Define version marker rules and schema naming conventions.
	- Define canonical error codes/categories and required metadata fields.
- Out of scope:
	- Endpoint-specific business payloads.
	- WebSocket stream payload definitions.
- Dependencies: None
- Risks:
	- Inconsistent envelope patterns increase implementation drift.

Acceptance criteria:

1. API spec defines one canonical success envelope and one canonical error envelope.
2. Error taxonomy includes deterministic code namespace and required fields.
3. Envelope and error model are referenced by lifecycle and status contract sections.

Evidence required:

- API evidence: envelope/error examples embedded in API spec.
- Runtime evidence: contract stubs compile against envelope structures.
- Docs updated: implementation plan references canonical contract.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-046

- Epic: EPIC-01
- Objective: Define lifecycle endpoint request/response schemas for start, pause, resume, stop, and reset.
- Scope:
	- Define request/response contracts for lifecycle operations.
	- Define allowed transition-state constraints and validation fields.
	- Define error responses for invalid transitions.
- Out of scope:
	- Suspend-save/restore lifecycle extensions.
	- Runtime scheduler internals.
- Dependencies: T-045
- Risks:
	- Lifecycle ambiguity can cause nondeterministic state handling.

Acceptance criteria:

1. Lifecycle endpoint contracts are documented with required and optional fields.
2. Invalid transition responses are standardized with canonical error codes.
3. Transition guard inputs are unambiguous for implementation work.

Evidence required:

- API evidence: lifecycle request/response examples in spec.
- Runtime evidence: schema validator tests pass for valid/invalid payload sets.
- Docs updated: transition table cross-linked with endpoint contracts.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-047

- Epic: EPIC-01
- Objective: Publish a baseline API schema bundle with complete examples and cross-references.
- Scope:
	- Consolidate envelope, error, and lifecycle contracts into baseline set.
	- Add representative examples for all baseline endpoints.
	- Add cross-links between API spec and implementation plan anchors.
- Out of scope:
	- Health/status endpoint payload specialization.
	- Input policy endpoint/event specialization.
- Dependencies: T-046
- Risks:
	- Missing cross-links delay downstream implementation.

Acceptance criteria:

1. Baseline contract set is complete and link-valid across architecture docs.
2. Baseline examples are present for each baseline endpoint family.
3. Downstream tasks can consume the baseline without additional schema decisions.

Evidence required:

- API evidence: baseline section includes complete examples.
- Runtime evidence: schema examples parse and validate.
- Docs updated: implementation plan and API spec include reciprocal links.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-048

- Epic: EPIC-01
- Objective: Define `/api/v2/engine/health` payload contract and enumerations.
- Scope:
	- Define health summary fields and component-level health states.
	- Define severity/status enums and timestamp semantics.
	- Define health error envelope usage.
- Out of scope:
	- Detailed runtime/session status payload contract.
	- WebSocket eventing.
- Dependencies: T-047
- Risks:
	- Weak enum definitions create incompatible client handling.

Acceptance criteria:

1. Health endpoint contract defines required fields and enum values.
2. Contract specifies deterministic timestamp and freshness semantics.
3. Error handling for unavailable subsystems is explicitly defined.

Evidence required:

- API evidence: health success/error examples in spec.
- Runtime evidence: schema checks validate enum and required fields.
- Docs updated: health contract linked in implementation plan.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-049

- Epic: EPIC-01
- Objective: Define `/api/v2/engine/status` payload contract for session/runtime state.
- Scope:
	- Define session identity, profile, mode, and lifecycle fields.
	- Define runtime metrics fields needed for status snapshots.
	- Define deterministic field semantics for paused/running/error states.
- Out of scope:
	- Full performance SLO endpoint payloads.
	- Input policy controls.
- Dependencies: T-047
- Risks:
	- Status ambiguity can break UI and automation assumptions.

Acceptance criteria:

1. Status payload includes required session and runtime fields.
2. State semantics are defined for each lifecycle mode.
3. Status contract is consistent with lifecycle API rules.

Evidence required:

- API evidence: status success/error examples in spec.
- Runtime evidence: schema validation for mode-specific payloads passes.
- Docs updated: status fields referenced in architecture telemetry section.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-050

- Epic: EPIC-01
- Objective: Define health/status WebSocket event payload contracts and ordering notes.
- Scope:
	- Define event names and payload fields for health/status updates.
	- Define ordering, monotonic sequence, and timestamp requirements.
	- Define backpressure/skip behavior disclosure fields.
- Out of scope:
	- Stream transport implementation.
	- Backpressure control algorithm implementation.
- Dependencies: T-048, T-049
- Risks:
	- Missing ordering rules can produce inconsistent client state.

Acceptance criteria:

1. Event schemas for health/status updates are documented.
2. Event ordering and sequence rules are explicit.
3. Stream error and degraded-delivery signaling is defined.

Evidence required:

- API evidence: WS event examples for normal and degraded delivery.
- Runtime evidence: schema checks validate event payloads.
- Docs updated: WS event contracts linked to stream observability sections.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-051

- Epic: EPIC-03
- Objective: Define browser input policy state model and policy-related error envelopes.
- Scope:
	- Define policy states and transitions for enabled/disabled input handling.
	- Define policy reason/source metadata fields.
	- Define policy violation and invalid-state errors.
- Out of scope:
	- Input mapping CRUD APIs.
	- Capture-mode detailed behavior implementation.
- Dependencies: T-047
- Risks:
	- Undefined policy states can cause inconsistent browser control behavior.

Acceptance criteria:

1. Policy states and transitions are documented and deterministic.
2. Policy error envelopes align with canonical API error schema.
3. Policy model is sufficient for endpoint and event contracts.

Evidence required:

- API evidence: policy state and error examples documented.
- Runtime evidence: transition matrix validation checks pass.
- Docs updated: input section in implementation plan references policy model.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-052

- Epic: EPIC-03
- Objective: Define input enable/disable endpoint contract with idempotency behavior.
- Scope:
	- Define request/response payloads for enable/disable actions.
	- Define idempotent behavior, no-op responses, and transition guards.
	- Define explicit errors for invalid mode/session conditions.
- Out of scope:
	- Mouse capture mode (`mouse_over`, `click_to_capture`) specifics.
	- Input event stream payload design.
- Dependencies: T-051
- Risks:
	- Non-idempotent contract can produce race conditions from browser clients.

Acceptance criteria:

1. Enable/disable endpoint schema and idempotency rules are explicit.
2. Transition guard failures return deterministic error codes.
3. Endpoint semantics align with policy state machine.

Evidence required:

- API evidence: enable/disable success, no-op, and failure examples.
- Runtime evidence: schema validator checks idempotent response shape.
- Docs updated: policy endpoint contract cross-linked in API and plan docs.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

### TASK-ID: T-053

- Epic: EPIC-03
- Objective: Define input-policy change event contract and transition ordering guarantees.
- Scope:
	- Define event payload for policy changes (source, prior_state, new_state, reason, timestamp).
	- Define ordering/sequence guarantees for policy transitions.
	- Define event behavior on duplicate/no-op requests.
- Out of scope:
	- Browser capture mode event family.
	- Physical input translation stream payloads.
- Dependencies: T-052
- Risks:
	- Event ordering drift can desynchronize browser and engine policy state.

Acceptance criteria:

1. Policy-change event payload is fully defined.
2. Ordering guarantees are stated and testable.
3. Duplicate/no-op request behavior is observable through deterministic event rules.

Evidence required:

- API evidence: policy-change event examples for normal and no-op transitions.
- Runtime evidence: ordering assertions pass in event sequencing checks.
- Docs updated: event contract referenced by input subsystem sections.

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

## Task card template (copy for new tasks)

### TASK-ID: <T-XXX>

- Epic: <EPIC-XX>
- Objective:
- Scope:
- Out of scope:
- Dependencies:
- Risks:

Acceptance criteria:

1. 
2. 
3. 

Evidence required:

- API evidence:
- Runtime evidence:
- Docs updated:

Done checklist:

- [ ] Implemented
- [ ] Validated
- [ ] Documented
- [ ] Reviewed
- [ ] Accepted by Product Owner

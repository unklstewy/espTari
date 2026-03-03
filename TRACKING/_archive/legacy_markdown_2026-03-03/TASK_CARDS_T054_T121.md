# Detailed Task Cards (T-054 to T-121)

These cards are generated from the canonical task table in `TRACKING/BACKLOG.md`.
Each card is pullable at XS/S granularity and follows the standard acceptance/evidence contract.

### TASK-ID: T-054

- Epic: EPIC-01
- Objective: Define lifecycle transition matrix and guard predicates
- Scope:
  - Deliver the contract/implementation slice described by: Define lifecycle transition matrix and guard predicates.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: XS
- Sprint target: S1
- Status: Done
- Dependencies: T-047
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-055

- Epic: EPIC-01
- Objective: Implement lifecycle guard validator responses and error mapping
- Scope:
  - Deliver the contract/implementation slice described by: Implement lifecycle guard validator responses and error mapping.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S1
- Status: Done
- Dependencies: T-054
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-056

- Epic: EPIC-04
- Objective: Build catalog index loader for `rom_id` `disk_id` `tos_id`
- Scope:
  - Deliver the contract/implementation slice described by: Build catalog index loader for `rom_id` `disk_id` `tos_id`.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S1
- Status: Done
- Dependencies: T-047
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-057

- Epic: EPIC-04
- Objective: Implement catalog-backed ID resolution API path and error handling
- Scope:
  - Deliver the contract/implementation slice described by: Implement catalog-backed ID resolution API path and error handling.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S1
- Status: Done
- Dependencies: T-056
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-058

- Epic: EPIC-04
- Objective: Implement SD-card asset scan and canonical presence index
- Scope:
  - Deliver the contract/implementation slice described by: Implement SD-card asset scan and canonical presence index.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S1
- Status: Done
- Dependencies: T-057
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-059

- Epic: EPIC-04
- Objective: Implement missing-asset diff report and API projection
- Scope:
  - Deliver the contract/implementation slice described by: Implement missing-asset diff report and API projection.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: XS
- Sprint target: S1
- Status: Done
- Dependencies: T-058
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-060

- Epic: EPIC-04
- Objective: Implement hosted download request validation and enqueue path
- Scope:
  - Deliver the contract/implementation slice described by: Implement hosted download request validation and enqueue path.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S1
- Status: Done
- Dependencies: T-059
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-061

- Epic: EPIC-04
- Objective: Implement staged download commit with integrity/hash verification
- Scope:
  - Deliver the contract/implementation slice described by: Implement staged download commit with integrity/hash verification.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S1
- Status: Done
- Dependencies: T-060
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-062

- Epic: EPIC-04
- Objective: Implement dead-link probe worker and timeout policy
- Scope:
  - Deliver the contract/implementation slice described by: Implement dead-link probe worker and timeout policy.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-061
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-063

- Epic: EPIC-04
- Objective: Implement dead-link mark/retry state machine and telemetry fields
- Scope:
  - Deliver the contract/implementation slice described by: Implement dead-link mark/retry state machine and telemetry fields.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: XS
- Sprint target: S3
- Status: Done
- Dependencies: T-062
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-064

- Epic: EPIC-04
- Objective: Implement catalog-sync scheduler core and persisted schedules
- Scope:
  - Deliver the contract/implementation slice described by: Implement catalog-sync scheduler core and persisted schedules.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-063
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-065

- Epic: EPIC-04
- Objective: Implement schedule CRUD API and reboot-recovery validation
- Scope:
  - Deliver the contract/implementation slice described by: Implement schedule CRUD API and reboot-recovery validation.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-064
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-066

- Epic: EPIC-03
- Objective: Implement `mouse_over` capture state machine
- Scope:
  - Deliver the contract/implementation slice described by: Implement `mouse_over` capture state machine.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-053
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-067

- Epic: EPIC-03
- Objective: Implement `mouse_over` enter/leave event hooks and release behavior
- Scope:
  - Deliver the contract/implementation slice described by: Implement `mouse_over` enter/leave event hooks and release behavior.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: XS
- Sprint target: S3
- Status: Done
- Dependencies: T-066
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-068

- Epic: EPIC-03
- Objective: Implement `click_to_capture` acquisition state machine
- Scope:
  - Deliver the contract/implementation slice described by: Implement `click_to_capture` acquisition state machine.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-053
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-069

- Epic: EPIC-03
- Objective: Implement escape-release and focus-recovery behavior
- Scope:
  - Deliver the contract/implementation slice described by: Implement escape-release and focus-recovery behavior.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: XS
- Sprint target: S3
- Status: Done
- Dependencies: T-068
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-070

- Epic: EPIC-03
- Objective: Implement input mapping schema and persistence model
- Scope:
  - Deliver the contract/implementation slice described by: Implement input mapping schema and persistence model.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-053
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-071

- Epic: EPIC-03
- Objective: Implement mapping CRUD and active-profile apply path
- Scope:
  - Deliver the contract/implementation slice described by: Implement mapping CRUD and active-profile apply path.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-070
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-072

- Epic: EPIC-03
- Objective: Define input translation event payload contract and ordering fields
- Scope:
  - Deliver the contract/implementation slice described by: Define input translation event payload contract and ordering fields.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: XS
- Sprint target: S3
- Status: Done
- Dependencies: T-071
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-073

- Epic: EPIC-03
- Objective: Implement input translation event stream emitter and sequencing checks
- Scope:
  - Deliver the contract/implementation slice described by: Implement input translation event stream emitter and sequencing checks.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-072
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-074

- Epic: EPIC-02
- Objective: Implement ST profile manifest parser and schema validation
- Scope:
  - Deliver the contract/implementation slice described by: Implement ST profile manifest parser and schema validation.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-055, T-057
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-075

- Epic: EPIC-02
- Objective: Implement profile wiring validation and fail-fast diagnostics
- Scope:
  - Deliver the contract/implementation slice described by: Implement profile wiring validation and fail-fast diagnostics.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-074
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-076

- Epic: EPIC-02
- Objective: Implement deterministic tick-loop scheduler core
- Scope:
  - Deliver the contract/implementation slice described by: Implement deterministic tick-loop scheduler core.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-075
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-077

- Epic: EPIC-02
- Objective: Implement arbitration hook layer and deterministic timestamp emitter
- Scope:
  - Deliver the contract/implementation slice described by: Implement arbitration hook layer and deterministic timestamp emitter.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-076
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-078

- Epic: EPIC-02
- Objective: Implement ROM attach request validation and catalog binding checks
- Scope:
  - Deliver the contract/implementation slice described by: Implement ROM attach request validation and catalog binding checks.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: XS
- Sprint target: S2
- Status: Done
- Dependencies: T-075
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-079

- Epic: EPIC-02
- Objective: Implement ROM mount/apply flow and attach status events
- Scope:
  - Deliver the contract/implementation slice described by: Implement ROM mount/apply flow and attach status events.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-078
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-080

- Epic: EPIC-02
- Objective: Implement disk attach/eject request validation and binding checks
- Scope:
  - Deliver the contract/implementation slice described by: Implement disk attach/eject request validation and binding checks.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: XS
- Sprint target: S2
- Status: Done
- Dependencies: T-075
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-081

- Epic: EPIC-02
- Objective: Implement disk mount/eject runtime flow and state events
- Scope:
  - Deliver the contract/implementation slice described by: Implement disk mount/eject runtime flow and state events.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-080
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-082

- Epic: EPIC-05
- Objective: Define video stream metadata channel contract and schema
- Scope:
  - Deliver the contract/implementation slice described by: Define video stream metadata channel contract and schema.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: XS
- Sprint target: S2
- Status: Done
- Dependencies: T-077
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-083

- Epic: EPIC-05
- Objective: Implement video payload stream emitter and pacing controls
- Scope:
  - Deliver the contract/implementation slice described by: Implement video payload stream emitter and pacing controls.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-082
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-084

- Epic: EPIC-05
- Objective: Define audio stream metadata channel contract and schema
- Scope:
  - Deliver the contract/implementation slice described by: Define audio stream metadata channel contract and schema.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: XS
- Sprint target: S2
- Status: Done
- Dependencies: T-077
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-085

- Epic: EPIC-05
- Objective: Implement audio payload stream emitter and pacing controls
- Scope:
  - Deliver the contract/implementation slice described by: Implement audio payload stream emitter and pacing controls.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-084
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-086

- Epic: EPIC-05
- Objective: Define register snapshot schema and selective filter fields
- Scope:
  - Deliver the contract/implementation slice described by: Define register snapshot schema and selective filter fields.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: XS
- Sprint target: S4
- Status: Done
- Dependencies: T-077
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-087

- Epic: EPIC-05
- Objective: Implement register snapshot stream publisher and validation checks
- Scope:
  - Deliver the contract/implementation slice described by: Implement register snapshot stream publisher and validation checks.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-086
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-088

- Epic: EPIC-05
- Objective: Define bus/memory filter request model and guard rules
- Scope:
  - Deliver the contract/implementation slice described by: Define bus/memory filter request model and guard rules.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-087
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-089

- Epic: EPIC-05
- Objective: Implement filtered bus/memory stream and load validation run
- Scope:
  - Deliver the contract/implementation slice described by: Implement filtered bus/memory stream and load validation run.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-088
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-090

- Epic: EPIC-05
- Objective: Implement stream backpressure counters and watermark metrics
- Scope:
  - Deliver the contract/implementation slice described by: Implement stream backpressure counters and watermark metrics.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: XS
- Sprint target: S4
- Status: Done
- Dependencies: T-083, T-085, T-089
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-091

- Epic: EPIC-05
- Objective: Expose backpressure telemetry via API and event streams
- Scope:
  - Deliver the contract/implementation slice described by: Expose backpressure telemetry via API and event streams.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-090
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-092

- Epic: EPIC-06
- Objective: Build conformance harness scaffold and test manifest loader
- Scope:
  - Deliver the contract/implementation slice described by: Build conformance harness scaffold and test manifest loader.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-075, T-083, T-085
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-093

- Epic: EPIC-06
- Objective: Implement evidence artifact collection and report packaging flow
- Scope:
  - Deliver the contract/implementation slice described by: Implement evidence artifact collection and report packaging flow.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-092
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-094

- Epic: EPIC-06
- Objective: Implement acceptance checklist execution runner
- Scope:
  - Deliver the contract/implementation slice described by: Implement acceptance checklist execution runner.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-093
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-095

- Epic: EPIC-06
- Objective: Implement review pack generation and signoff bundle assembly
- Scope:
  - Deliver the contract/implementation slice described by: Implement review pack generation and signoff bundle assembly.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: XS
- Sprint target: S4
- Status: Done
- Dependencies: T-094
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-096

- Epic: EPIC-07
- Objective: Implement GLUE/MMU/SHIFTER register and memory window model
- Scope:
  - Deliver the contract/implementation slice described by: Implement GLUE/MMU/SHIFTER register and memory window model.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-077
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-097

- Epic: EPIC-07
- Objective: Implement GLUE/MMU/SHIFTER arbitration and timing integration checks
- Scope:
  - Deliver the contract/implementation slice described by: Implement GLUE/MMU/SHIFTER arbitration and timing integration checks.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-096
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-098

- Epic: EPIC-07
- Objective: Implement MFP register and timer model contracts
- Scope:
  - Deliver the contract/implementation slice described by: Implement MFP register and timer model contracts.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-077
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-099

- Epic: EPIC-07
- Objective: Implement MFP interrupt emission behaviors and conformance checks
- Scope:
  - Deliver the contract/implementation slice described by: Implement MFP interrupt emission behaviors and conformance checks.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S2
- Status: Done
- Dependencies: T-098
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-100

- Epic: EPIC-07
- Objective: Implement ACIA host channel bridge and framing rules
- Scope:
  - Deliver the contract/implementation slice described by: Implement ACIA host channel bridge and framing rules.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-099
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-101

- Epic: EPIC-07
- Objective: Implement IKBD parser bridge and keyboard/mouse packet timing path
- Scope:
  - Deliver the contract/implementation slice described by: Implement IKBD parser bridge and keyboard/mouse packet timing path.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-100
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-102

- Epic: EPIC-07
- Objective: Implement DMA request pacing model and arbitration hooks
- Scope:
  - Deliver the contract/implementation slice described by: Implement DMA request pacing model and arbitration hooks.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-077, T-081
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-103

- Epic: EPIC-07
- Objective: Implement FDC command/status FSM bridge and terminal conditions
- Scope:
  - Deliver the contract/implementation slice described by: Implement FDC command/status FSM bridge and terminal conditions.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-102
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-104

- Epic: EPIC-07
- Objective: Implement PSG register/audio behavior contract
- Scope:
  - Deliver the contract/implementation slice described by: Implement PSG register/audio behavior contract.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-077
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-105

- Epic: EPIC-07
- Objective: Implement PSG GPIO behavior contract and validation checks
- Scope:
  - Deliver the contract/implementation slice described by: Implement PSG GPIO behavior contract and validation checks.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: XS
- Sprint target: S3
- Status: Done
- Dependencies: T-104
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-106

- Epic: EPIC-07
- Objective: Define interrupt hierarchy map and vector routing contract
- Scope:
  - Deliver the contract/implementation slice described by: Define interrupt hierarchy map and vector routing contract.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-097, T-099, T-101, T-103
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-107

- Epic: EPIC-07
- Objective: Implement interrupt wiring integration checks across subsystems
- Scope:
  - Deliver the contract/implementation slice described by: Implement interrupt wiring integration checks across subsystems.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-106
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-108

- Epic: EPIC-07
- Objective: Define startup defaults and power-on register baseline table
- Scope:
  - Deliver the contract/implementation slice described by: Define startup defaults and power-on register baseline table.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: XS
- Sprint target: S3
- Status: Done
- Dependencies: T-107
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-109

- Epic: EPIC-07
- Objective: Implement reset/startup sequence executor and verification checks
- Scope:
  - Deliver the contract/implementation slice described by: Implement reset/startup sequence executor and verification checks.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S3
- Status: Done
- Dependencies: T-108
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-110

- Epic: EPIC-07
- Objective: Build subsystem conformance test scaffold and fixture model
- Scope:
  - Deliver the contract/implementation slice described by: Build subsystem conformance test scaffold and fixture model.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-109
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-111

- Epic: EPIC-07
- Objective: Implement per-subsystem acceptance suites and reporting output
- Scope:
  - Deliver the contract/implementation slice described by: Implement per-subsystem acceptance suites and reporting output.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-110
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-112

- Epic: EPIC-01
- Objective: Implement suspend-save request contract wiring and transition checks
- Scope:
  - Deliver the contract/implementation slice described by: Implement suspend-save request contract wiring and transition checks.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-055, T-093
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-113

- Epic: EPIC-01
- Objective: Implement restore-resume transition guards and error semantics
- Scope:
  - Deliver the contract/implementation slice described by: Implement restore-resume transition guards and error semantics.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-112
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-114

- Epic: EPIC-05
- Objective: Define restore compatibility rule matrix (schema/abi/profile)
- Scope:
  - Deliver the contract/implementation slice described by: Define restore compatibility rule matrix (schema/abi/profile).
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: XS
- Sprint target: S4
- Status: Done
- Dependencies: T-044
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-115

- Epic: EPIC-05
- Objective: Implement restore compatibility validator and error mapping
- Scope:
  - Deliver the contract/implementation slice described by: Implement restore compatibility validator and error mapping.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-114
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-116

- Epic: EPIC-05
- Objective: Implement performance SLO metric collectors and sampling pipeline
- Scope:
  - Deliver the contract/implementation slice described by: Implement performance SLO metric collectors and sampling pipeline.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-083, T-085, T-091
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-117

- Epic: EPIC-05
- Objective: Expose SLO endpoints and threshold breach alarm events
- Scope:
  - Deliver the contract/implementation slice described by: Expose SLO endpoints and threshold breach alarm events.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P0
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-116
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-118

- Epic: EPIC-05
- Objective: Define realtime/slow-motion clock control model and bounds
- Scope:
  - Deliver the contract/implementation slice described by: Define realtime/slow-motion clock control model and bounds.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: XS
- Sprint target: S4
- Status: Done
- Dependencies: T-077
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-119

- Epic: EPIC-05
- Objective: Implement clock mode API and deterministic mode transition flow
- Scope:
  - Deliver the contract/implementation slice described by: Implement clock mode API and deterministic mode transition flow.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-118
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-120

- Epic: EPIC-05
- Objective: Implement single-step execution control API and scheduler hook
- Scope:
  - Deliver the contract/implementation slice described by: Implement single-step execution control API and scheduler hook.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-119, T-087, T-089
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

### TASK-ID: T-121

- Epic: EPIC-05
- Objective: Implement opcode/bus-error capture path and diagnostic payloads
- Scope:
  - Deliver the contract/implementation slice described by: Implement opcode/bus-error capture path and diagnostic payloads.
  - Keep changes minimal and aligned to existing API/runtime architecture documents.
- Out of scope:
  - Unrelated subsystem refactors or non-dependent feature work.
  - Performance tuning beyond acceptance requirements for this slice.
- Priority: P1
- Size: S
- Sprint target: S4
- Status: Done
- Dependencies: T-120
- Risks:
  - Dependency sequencing drift may delay this slice.

Acceptance criteria:

1. Task scope is implemented/documented exactly as defined in the task title and epic context.
2. Dependency assumptions are respected and invalid dependency states return deterministic errors or blockers.
3. Output is verifiable through API/runtime evidence and cross-referenced docs updates.

Evidence required:

- API evidence: request/response or event examples, where applicable.
- Runtime evidence: build/test output or deterministic validation trace for the slice.
- Docs updated: links/sections updated in tracking and architecture/API docs where impacted.

Done checklist:

- [x] Implemented
- [x] Validated
- [x] Documented
- [x] Reviewed
- [x] Accepted by Product Owner

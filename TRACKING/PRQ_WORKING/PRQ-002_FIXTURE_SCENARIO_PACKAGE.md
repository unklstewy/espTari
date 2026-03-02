# PRQ-002 Deterministic Fixture/Scenario Package

Date: 2026-03-02
Task: PRQ-002
Phase: Pre-runtime-unlock (documentation-only)

## Objective

Define a deterministic fixture and scenario package for CRT validation vectors, without executing runtime tests.

## Fixture Catalog

| Fixture ID | Domain | Description | Deterministic Controls | Owner |
|---|---|---|---|---|
| FX-LC-01 | lifecycle | Transition matrix baseline fixture for session state flow | Fixed initial state map, fixed transition order, fixed error-map expectations | Engineering |
| FX-IN-01 | input mapping | CRUD/apply profile fixture set | Fixed profile IDs/revisions, deterministic apply ordering, conflict probes | Engineering + QA |
| FX-SR-01 | save/restore | Suspend-save/restore compatibility fixture set | Fixed schema/ABI/profile tuple matrix, static expected compatibility outcomes | Engineering |
| FX-OB-01 | observability | Stream/telemetry/alarm fixture set | Fixed stream payload templates, fixed threshold vectors, fixed event chronology | Engineering + QA |

## Scenario Manifest

| Scenario ID | Check Family | Fixture | Expected Outcome Category |
|---|---|---|---|
| SC-LC-01 | Lifecycle transitions and guard denials | FX-LC-01 | Envelope/guard mapping consistency |
| SC-IN-01 | CRUD/apply monotonicity and no-op semantics | FX-IN-01 | Revision/cutover semantics consistency |
| SC-SR-01 | Suspend-save/restore and compatibility paths | FX-SR-01 | Compatibility and state-transition consistency |
| SC-OB-01 | Stream payload/order and SLO alarm sequencing | FX-OB-01 | Telemetry/order/threshold consistency |

## Deterministic Assumptions and Constraints

- Seed values and vector order are fixed per scenario ID.
- No clock-randomized inputs are permitted in fixture definitions.
- Template payloads are version-pinned to accepted contract revisions.
- Any fixture change requires revision bump and rationale entry.

## Packaging Index

- Fixture catalog: this file.
- Downstream workflow constraints: TRACKING/PRQ_WORKING/PRQ-003_DEPLOYMENT_WORKFLOW_RUNBOOK.md
- Final unlock packet cross-link: TRACKING/PRQ_WORKING/PRQ-004_UNLOCK_REVIEW_PACKET.md

## Status

Documentation package prepared for review; runtime execution remains blocked by phase gate.

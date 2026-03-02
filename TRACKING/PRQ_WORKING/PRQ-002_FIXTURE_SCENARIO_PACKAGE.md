# PRQ-002 Deterministic Fixture/Scenario Package

Date: 2026-03-02
Task: PRQ-002
Phase: Runtime-backed fixture package

## Objective

Define and execute a deterministic fixture/scenario package for CRT validation vectors with reproducible API evidence output.

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
- Fixture execution script: `tools/prq/run_prq002_fixture_matrix.sh`
- Fixture evidence output: `captures/prq002_fixture_matrix_20260302_184958.txt`
- Downstream workflow constraints: TRACKING/PRQ_WORKING/PRQ-003_DEPLOYMENT_WORKFLOW_RUNBOOK.md
- Final unlock packet cross-link: TRACKING/PRQ_WORKING/PRQ-004_UNLOCK_REVIEW_PACKET.md

## Status

Fixture package executed and evidenced; deterministic matrix completed with pass/fail accounting and zero failures.

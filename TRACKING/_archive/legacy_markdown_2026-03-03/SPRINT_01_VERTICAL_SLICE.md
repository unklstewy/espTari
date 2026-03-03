# Sprint 1 - Control/API Vertical Slice

Duration: 3 working days  
Goal: Deliver first end-to-end control path with catalog-backed media resolution and browser input policy hooks.

## Committed tasks

- T-001: Finalize v2 API schema baseline
- T-002: Session transition guard rules
- T-003: Engine health/status payload
- T-004: Catalog resolver for rom_id/disk_id/tos_id
- T-009: Browser input enable/disable policy

## Stretch tasks

- T-005: Missing-asset detector
- T-006: Single-entry hosted download pipeline

## Sprint demo scenarios

1. Start session using catalog-backed `rom_id`.
2. Attempt disk attach by `disk_id` where local file is missing, then trigger hosted download.
3. Toggle browser input enabled/disabled and verify ingestion behavior.
4. Query session health/status endpoint.

## Acceptance criteria

1. All committed endpoints return contract-compliant payloads.
2. Missing catalog asset path is actionable (download method works).
3. Session state transitions reject invalid moves.
4. Product Owner signs off demo scenarios.

## Evidence package

- Endpoint request/response traces
- State transition test outputs
- Catalog resolution and download logs
- Acceptance decision entries in Acceptance Log

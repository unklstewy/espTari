# Sprint 3 - Input + Catalog Sync Automation

Duration: 3 working days  
Goal: Complete browser input capture modes and automate catalog sync, dead-link detection, and scheduled scraper jobs.

## Committed tasks

- T-007: Dead-link probe + mark-dead workflow
- T-010: `mouse_over` capture mode
- T-011: `click_to_capture` + escape release
- T-012: Input mapping load/update/active APIs

## Sprint demo scenarios

1. Switch between `mouse_over` and `click_to_capture` modes.
2. Use escape sequence to release capture in click mode.
3. Run catalog sync job and show dead-link state transitions.
4. Trigger missing-asset batch download from hosted links.

## Stretch tasks

- T-008: Scheduled catalog-sync job runner
- T-013: Input translation event stream

## Acceptance criteria

1. Input capture behavior matches policy exactly.
2. Input stream emits translated events and diagnostics.
3. Catalog sync job supports run-now and scheduled modes.
4. Dead-link status is persisted with reason and timestamp.

## Evidence package

- Input event/capture state traces
- Scheduler job status history
- Catalog before/after link-probe snapshots
- Product Owner acceptance notes

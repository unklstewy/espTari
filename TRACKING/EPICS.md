# Epics

## EPIC-01: v2 Control Plane and Session Lifecycle

Outcome:

- Deterministic engine session lifecycle with stable API contracts.

Success criteria:

- Start/pause/resume/reset/stop operational.
- Suspend-save and restore-resume flows operational.
- Valid state transitions enforced.
- Session status and health visible.

## EPIC-02: Atari ST Baseline Runtime (Machine First)

Outcome:

- Atari ST baseline profile executes through v2 runtime architecture.

Success criteria:

- Machine profile loads and runs.
- Core buses/events flow through deterministic scheduler.
- ROM and media attach path works through catalog IDs.

## EPIC-03: Browser Input Translation and Capture

Outcome:

- Browser-origin inputs map to Atari ST virtual inputs with policy-driven capture.

Success criteria:

- Keyboard/mouse/controller translation works.
- Input capture modes (`mouse_over`, `click_to_capture`) function.
- Escape sequence and explicit release behavior validated.

## EPIC-04: Catalog-Backed Media and Sync Automation

Outcome:

- Catalogs drive media resolution, missing-asset download, dead-link tracking, and routine sync jobs.

Success criteria:

- `rom_id`/`disk_id`/`tos_id` resolve correctly.
- Missing assets can be fetched from hosted URLs.
- Dead links are marked with reason/timestamp.
- Scheduled sync jobs execute and report status.

## EPIC-05: Streaming Output and Observability

Outcome:

- Browser-consumable video/audio stream and runtime inspection streams are production-usable.

Success criteria:

- Video/audio streams stable under load.
- Register/bus/memory streams filterable and deterministic.
- Backpressure metrics available.
- Hard runtime metrics are exposed and validated: input latency <= 50 ms, jitter < 30 ms, dropped-frame rate < 1 percent.
- Debug clock controls support realtime, slow-motion, and single-step execution with observability capture.

## EPIC-06: Conformance and Acceptance Hardening

Outcome:

- Engine behavior is objectively validated and acceptance-ready.

Success criteria:

- Validation checklist run with evidence.
- Risks tracked with mitigation status.
- Sprint deliverables accepted by Product Owner.

## EPIC-07: Atari ST Emulated Components (From Scratch)

Outcome:

- All Atari ST machine components required for baseline compatibility are implemented from scratch under v2 contracts.

Success criteria:

- Component set includes CPU integration contract, GLUE/MMU/SHIFTER, MFP, ACIA/IKBD path, DMA/FDC path, PSG audio path, memory map wiring, and interrupt hierarchy behavior.
- Component interfaces are EBIN-loadable and comply with v2 ABI contracts.
- Component-level behavior is testable through register/bus/memory observability endpoints.
- Component bring-up sequence supports deterministic startup/reset flow for Atari ST baseline profile.

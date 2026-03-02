# Emulation Engine v2 Minimum Viable Product (MVP)

Date: 2026-03-01  
Scope owner: Product Owner and Acceptance Master

## 1) MVP objective

Deliver a backend-only Atari ST baseline runtime that can be remotely controlled, streams video/audio to browser clients, supports catalog-backed media, provides core observability, and includes safe suspend/save and restore/resume workflows with measurable performance targets.

## 2) In-scope MVP capabilities (must-have)

1. Session lifecycle:
   - start, pause, resume, reset, stop
   - suspend-save and restore-resume
2. Atari ST baseline profile load path with deterministic scheduler skeleton.
3. SD-card media via catalog IDs (`rom_id`, `disk_id`, `tos_id`) plus missing-asset fetch support.
4. Browser-session input translation and capture policy:
   - enable/disable
   - `mouse_over` and `click_to_capture`
   - escape release
5. Browser-consumable streaming output:
   - video stream
   - audio stream
6. Core observability:
   - register snapshot + stream
   - bus/memory filtered stream
   - backpressure counters
7. Machine state persistence:
   - save snapshot
   - list snapshots
   - restore snapshot with compatibility validation
8. Performance SLO exposure and enforcement signals:
   - input latency <= 50 ms
   - jitter < 30 ms
   - dropped-frame rate < 1 percent
9. Debug execution controls for diagnosis:
   - realtime mode
   - slow-motion mode
   - single-step mode with opcode/bus-error capture visibility

## 3) Out-of-scope for MVP

- Full Mega ST, STe, and Mega STe feature completeness.
- Non-browser frontend UX implementation.
- Advanced optimizations beyond meeting required SLO gates.
- Exhaustive long-tail compatibility/per-title tuning.

## 4) MVP acceptance gates

MVP is accepted only when all gates pass:

1. API conformance checklist in `docs/EMU_ENGINE_V2_API_SPEC.md` passes with evidence.
2. Architecture closure P0 items in `TRACKING/ARCHITECTURE_90_CLOSURE_CHECKLIST.md` are complete.
3. No P0 task remains in Backlog or Blocked status.
4. Product Owner signs acceptance in `TRACKING/ACCEPTANCE_LOG.md`.

## 5) MVP-aligned task set

Primary task IDs:

- Lifecycle and save/restore: T-002, T-033, T-039, T-040, T-041, T-042, T-043, T-044, T-035
- Runtime baseline: T-014, T-015, T-016, T-017, T-025, T-026, T-027, T-028, T-030, T-031
- Streaming and observability: T-018, T-019, T-020, T-021, T-022
- Input and capture: T-009, T-010, T-011, T-012, T-013
- Catalog/sync essentials: T-004, T-005, T-006, T-007, T-008
- Performance/debug: T-036, T-037, T-038
- Conformance and acceptance: T-023, T-024, T-032

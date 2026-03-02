# 11.3E DMA request pacing model and arbitration hooks

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 75

## 11.3E DMA request pacing model and arbitration hooks

DMA request pacing contract:

- `GET /api/v2/inspect/chipset/dma/pacing?session_id=...`
- Response `data` must expose pacing state validated by `dma_pacing_state_v1`.
- `dma_pacing_state_v1` required fields:
  - `session_id` (string)
  - `pacing_mode` (enum: `deterministic_tick`)
  - `request_window_ticks` (uint32)
  - `max_requests_per_window` (uint32)
  - `window_start_tick` (uint64)
  - `window_end_tick` (uint64)
  - `queued_requests` (uint32)
  - `last_request_seq` (uint64)

DMA arbitration hook contract:

- `GET /api/v2/inspect/chipset/dma/arbitration?session_id=...&limit=...`
- Arbitration entries are validated by `dma_arbitration_hook_v1` and emitted in sequence order.
- `dma_arbitration_hook_v1` required fields:
  - `request_seq` (uint64)
  - `requester` (enum: `fdc`, `blitter`, `memory_refresh`)
  - `arbitration_round` (uint32)
  - `grant_state` (enum: `granted`, `deferred`, `denied`)
  - `scheduled_tick` (uint64)
  - `granted_tick` (uint64, nullable when `grant_state!=granted`)
  - `timestamp_us` (uint64)

DMA pacing/arbitration conformance checks:

- `DMA-ARB-01`: `request_seq(next) == request_seq(prev) + 1` for emitted arbitration entries.
- `DMA-ARB-02`: `scheduled_tick(next) >= scheduled_tick(prev)` and `timestamp_us(next) >= timestamp_us(prev)`.
- `DMA-ARB-03`: when `grant_state=granted`, `granted_tick >= scheduled_tick`; when `grant_state!=granted`, `granted_tick` must be `null`.
- `DMA-ARB-04`: grants in each pacing window must not exceed `max_requests_per_window`; excess requests are emitted as `deferred`.

DMA pacing/arbitration deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Arbitration hook source unavailable or pacing window state unresolved -> `INTERNAL_ERROR`.

DMA pacing state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "pacing_mode": "deterministic_tick",
  "request_window_ticks": 128,
  "max_requests_per_window": 16,
  "window_start_tick": 912640,
  "window_end_tick": 912767,
  "queued_requests": 3,
  "last_request_seq": 55301
}
```

DMA arbitration hook stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "events": [
    {
      "request_seq": 55302,
      "requester": "fdc",
      "arbitration_round": 44,
      "grant_state": "granted",
      "scheduled_tick": 912700,
      "granted_tick": 912700,
      "timestamp_us": 1710000031120
    },
    {
      "request_seq": 55303,
      "requester": "blitter",
      "arbitration_round": 45,
      "grant_state": "deferred",
      "scheduled_tick": 912701,
      "granted_tick": null,
      "timestamp_us": 1710000031124
    }
  ]
}
```

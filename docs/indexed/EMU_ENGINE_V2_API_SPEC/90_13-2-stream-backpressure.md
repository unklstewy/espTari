# 13.2 Stream backpressure

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 90

## 13.2 Stream backpressure

- Per-stream ring buffers are bounded.
- When full, policy is `drop_oldest` by default.
- Every drop increments `dropped_events` and emits health updates.
- Backpressure counters (`dropped_events`, `overflow_events_total`, `throttle_transitions_total`) are cumulative and monotonic for each stream connection.
- Watermark metrics (`high_watermark_depth`, `high_watermark_ratio`) track the highest observed queue occupancy for the active connection window.
- Counter/metric snapshots exposed in stream health and degraded-delivery telemetry must satisfy checks `BP-CTR-01..04`.

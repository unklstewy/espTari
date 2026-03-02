# 13.4 Hard performance SLO targets

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 92

## 13.4 Hard performance SLO targets

Required runtime targets:

- Input-device end-to-end latency: <= 50 ms
- Runtime jitter: < 30 ms
- Dropped-frame rate: < 1 percent

Measurement policy:

- Metrics are emitted in rolling windows and expose at least p50, p95, and max where applicable.
- Any target breach marks metric status `violating` and emits health notifications.

---

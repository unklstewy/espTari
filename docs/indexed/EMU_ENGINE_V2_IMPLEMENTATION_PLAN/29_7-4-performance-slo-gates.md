# 7.4 Performance SLO gates

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 29

## 7.4 Performance SLO gates

Runtime SLO targets:

- Input-device end-to-end latency: <= 50 ms
- Runtime jitter: < 30 ms
- Dropped-frame rate: < 1 percent

Measurement requirements:

- Report p50, p95, and max in rolling windows.
- Expose per-session and aggregate runtime views.
- Mark SLO status as `ok`, `degraded`, or `violating`.

---

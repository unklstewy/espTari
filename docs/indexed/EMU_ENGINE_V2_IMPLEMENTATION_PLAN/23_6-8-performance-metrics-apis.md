# 6.8 Performance metrics APIs

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 23

## 6.8 Performance metrics APIs

- `GET /api/v2/metrics/performance`
  - Expose input latency, runtime jitter, dropped-frame percentage, and measurement windows
- `GET /api/v2/metrics/performance/history`
  - Time-windowed SLO trends for acceptance and regressions
- Performance SLO collector + sampling pipeline contract (`slo_collector_config_v1`, `slo_sample_v1`, checks `SLO-COL-01..04`, and deterministic collector/pipeline guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.11` and is the canonical source.
- SLO endpoints + threshold-breach alarm contract (`slo_thresholds_v1`, `slo_breach_alarm_event_v1`, checks `SLO-ALRM-01..04`, and deterministic alarm-state guard failures) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.11` and is the canonical source.

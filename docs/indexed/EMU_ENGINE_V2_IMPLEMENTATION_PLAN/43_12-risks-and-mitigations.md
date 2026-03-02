# 12. Risks and mitigations

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 43

## 12. Risks and mitigations

- **Risk:** EBIN ABI churn breaks compatibility.  
  **Mitigation:** Versioned ABI contracts + compatibility matrix in catalog.

- **Risk:** Streaming overhead perturbs deterministic timing.  
  **Mitigation:** async ring-buffered export path, deterministic core never waits on network I/O.

- **Risk:** SD-card I/O latency stalls runtime.  
  **Mitigation:** staged preload caches for active media/module pages.

- **Risk:** Upstream media URLs become unavailable or stale.  
  **Mitigation:** periodic link probing, dead-link marking, and alternate-mirror policy hooks.

- **Risk:** Full trace mode overwhelms memory/bandwidth.  
  **Mitigation:** strict trace levels, filters, and capped ring buffers.

- **Risk:** Save-state schema drift breaks restore compatibility.  
  **Mitigation:** schema versioning + ABI/profile compatibility checks + restore rejection codes.

- **Risk:** Debug slow-motion or step mode perturbs runtime assumptions.  
  **Mitigation:** explicit debug mode isolation + conformance checks for debug transitions.

---

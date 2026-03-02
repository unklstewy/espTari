# 8.3 Deterministic scheduler contract

Source: ../EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md
Section Index ID: 33

## 8.3 Deterministic scheduler contract

- Single authoritative master tick.
- Component step order from machine profile wiring.
- Shared bus arbitration resolved through profile-defined policy.
- API-visible timestamps derive from this scheduler only.
- Deterministic tick-loop scheduler core contract (tick/cycle invariants, mode-specific execution rules for realtime/slow-motion/single-step, scheduler sequencing checks, and fail-fast guards) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.10A` and is normative for runtime timing behavior.
- Arbitration hook layer + deterministic timestamp emitter contract (hook dispatch order, arbitration metadata fields, timestamp monotonicity/determinism checks, and fail-fast internal error mapping) is defined in `docs/EMU_ENGINE_V2_API_SPEC.md` section `6.10A` and is normative for observability-grade timing consistency.

---

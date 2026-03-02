# 6.9B Clock mode API deterministic transition flow

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 27

## 6.9B Clock mode API deterministic transition flow

Transition model (`POST /api/v2/debug/clock/mode`):

1. Validate payload and bounds (`mode`, `ratio`) using section `6.9A` guards.
2. Resolve target configuration (`target_mode`, `effective_ratio`).
3. Compare with active configuration for idempotency.
4. Commit accepted transition at the next scheduler tick boundary.
5. Persist transition metadata and return deterministic transition result.

Deterministic transition checks:

- `CLOCK-TRANS-01`: accepted transition commits atomically at one tick boundary and never partially applies.
- `CLOCK-TRANS-02`: each applied transition increments `mode_transition_seq` by exactly `+1`.
- `CLOCK-TRANS-03`: idempotent request (`target_mode/effective_ratio` equals active config) returns success with `transition_applied=false` and must not increment `mode_transition_seq`.
- `CLOCK-TRANS-04`: transition response includes both `from_mode` and `to_mode` plus `effective_ratio` and `last_transition_at_us`.

Mode transition matrix:

- `realtime -> slow_motion`: allowed only with `ratio in (0,1]`.
- `slow_motion -> realtime`: allowed and normalizes to `effective_ratio=1.0`.
- `realtime|slow_motion -> single_step`: allowed; runtime enters deterministic step-gated execution path.
- `single_step -> realtime|slow_motion`: allowed after transition guard checks and atomic boundary commit.

Transition guard failures:

- Missing/invalid request fields (`session_id`, `mode`) -> `BAD_REQUEST`.
- Unknown/inactive `session_id` -> `ENGINE_NOT_RUNNING`.
- Invalid mode/ratio pairing or out-of-bounds ratio -> `DEBUG_CLOCK_INVALID`.
- Session lifecycle state not eligible for transition commit -> `INVALID_SESSION_STATE`.
- Scheduler transition commit failure after accepted guards -> `INTERNAL_ERROR`.

Mode-transition response example (`realtime -> slow_motion`):

```json
{
  "session_id": "ses_01H...",
  "transition_applied": true,
  "mode_transition_seq": 42,
  "from_mode": "realtime",
  "to_mode": "slow_motion",
  "effective_ratio": 0.5,
  "last_transition_at_us": 1710000009104
}
```

Idempotent transition response example:

```json
{
  "session_id": "ses_01H...",
  "transition_applied": false,
  "mode_transition_seq": 42,
  "from_mode": "slow_motion",
  "to_mode": "slow_motion",
  "effective_ratio": 0.5,
  "reason": "already_in_target_mode"
}
```

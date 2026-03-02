# 11.3F FDC command/status FSM bridge and terminal conditions

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 76

## 11.3F FDC command/status FSM bridge and terminal conditions

FDC command/status bridge contract:

- `GET /api/v2/inspect/chipset/fdc/fsm?session_id=...`
- Response `data` must expose command/status FSM state validated by `fdc_fsm_state_v1`.
- `fdc_fsm_state_v1` required fields:
  - `session_id` (string)
  - `fsm_state` (enum: `idle`, `command_latched`, `executing`, `result_ready`, `error`)
  - `active_command` (string, nullable)
  - `command_seq` (uint64)
  - `status_register` (uint8)
  - `busy` (boolean)
  - `drq` (boolean)
  - `intrq` (boolean)
  - `last_transition_tick` (uint64)
  - `last_transition_us` (uint64)

FDC terminal-condition event contract:

- `GET /api/v2/inspect/chipset/fdc/terminal?session_id=...&limit=...`
- Terminal events are validated by `fdc_terminal_event_v1` and emitted in sequence order.
- `fdc_terminal_event_v1` required fields:
  - `event_seq` (uint64)
  - `command_seq` (uint64)
  - `terminal_condition` (enum: `ok`, `crc_error`, `record_not_found`, `write_protect`, `lost_data`, `timeout`, `aborted`)
  - `status_register` (uint8)
  - `busy` (boolean)
  - `drq` (boolean)
  - `intrq` (boolean)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

FDC FSM/terminal conformance checks:

- `FDC-FSM-01`: `event_seq(next) == event_seq(prev) + 1` for terminal event stream.
- `FDC-FSM-02`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `FDC-FSM-03`: terminal event must be emitted only from `result_ready` or `error` FSM states.
- `FDC-FSM-04`: when terminal event is emitted, `busy=false` and `intrq=true`; `drq` reflects terminal condition semantics.

FDC FSM/terminal deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- FSM bridge unavailable or unresolved command/status transition -> `INTERNAL_ERROR`.

FDC FSM state example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "fsm_state": "executing",
  "active_command": "READ_SECTOR",
  "command_seq": 8012,
  "status_register": 129,
  "busy": true,
  "drq": false,
  "intrq": false,
  "last_transition_tick": 912840,
  "last_transition_us": 1710000031888
}
```

FDC terminal event stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "events": [
    {
      "event_seq": 20331,
      "command_seq": 8012,
      "terminal_condition": "ok",
      "status_register": 0,
      "busy": false,
      "drq": false,
      "intrq": true,
      "tick_counter": 912864,
      "timestamp_us": 1710000031951
    }
  ]
}
```

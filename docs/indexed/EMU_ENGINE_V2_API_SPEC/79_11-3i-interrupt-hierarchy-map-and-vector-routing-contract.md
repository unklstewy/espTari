# 11.3I Interrupt hierarchy map and vector routing contract

Source: ../EMU_ENGINE_V2_API_SPEC.md
Section Index ID: 79

## 11.3I Interrupt hierarchy map and vector routing contract

Interrupt hierarchy map contract:

- `GET /api/v2/inspect/chipset/interrupts/hierarchy?session_id=...`
- Response `data` must expose hierarchy state validated by `interrupt_hierarchy_v1`.
- `interrupt_hierarchy_v1` required fields:
  - `session_id` (string)
  - `cpu_level_order` (array of uint8, expected levels `1..7`)
  - `sources` (array of `interrupt_source_v1`)
  - `default_vector_base` (uint16)
  - `last_route_seq` (uint64)
  - `last_timestamp_us` (uint64)
- `interrupt_source_v1` required fields:
  - `source_id` (enum: `mfp`, `acia`, `fdc`, `blitter`, `vbl`)
  - `priority_level` (uint8)
  - `vector` (uint16)
  - `enabled` (boolean)

Interrupt vector routing contract:

- `GET /api/v2/inspect/chipset/interrupts/routes?session_id=...&limit=...`
- Route entries are validated by `interrupt_route_event_v1` and emitted in sequence order.
- `interrupt_route_event_v1` required fields:
  - `route_seq` (uint64)
  - `source_id` (string)
  - `priority_level` (uint8)
  - `vector` (uint16)
  - `cpu_interrupt_line` (enum: `irq1`, `irq2`, `irq3`, `irq4`, `irq5`, `irq6`, `irq7`)
  - `delivery_state` (enum: `delivered`, `masked`, `deferred`)
  - `tick_counter` (uint64)
  - `timestamp_us` (uint64)

Interrupt hierarchy/routing conformance checks:

- `INT-MAP-01`: each enabled `source_id` must map to exactly one active (`priority_level`, `vector`) pair.
- `INT-MAP-02`: `route_seq(next) == route_seq(prev) + 1` for route stream entries.
- `INT-MAP-03`: `timestamp_us(next) >= timestamp_us(prev)` and `tick_counter(next) >= tick_counter(prev)`.
- `INT-MAP-04`: when multiple pending sources share a tick, delivered route order must follow `cpu_level_order` then stable `source_id` tie-break.

Interrupt hierarchy/routing deterministic guard failures:

- Missing/invalid query params (`session_id`, `limit`) -> `BAD_REQUEST`.
- Unknown/inactive session -> `ENGINE_NOT_RUNNING`.
- Interrupt map unavailable or unresolved vector routing state -> `INTERNAL_ERROR`.

Interrupt hierarchy map example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "cpu_level_order": [7, 6, 5, 4, 3, 2, 1],
  "sources": [
    {
      "source_id": "mfp",
      "priority_level": 6,
      "vector": 38,
      "enabled": true
    }
  ],
  "default_vector_base": 24,
  "last_route_seq": 9901,
  "last_timestamp_us": 1710000034028
}
```

Interrupt route stream example (`data` excerpt):

```json
{
  "session_id": "ses_01H...",
  "routes": [
    {
      "route_seq": 9902,
      "source_id": "mfp",
      "priority_level": 6,
      "vector": 38,
      "cpu_interrupt_line": "irq6",
      "delivery_state": "delivered",
      "tick_counter": 913601,
      "timestamp_us": 1710000034031
    },
    {
      "route_seq": 9903,
      "source_id": "fdc",
      "priority_level": 3,
      "vector": 54,
      "cpu_interrupt_line": "irq3",
      "delivery_state": "deferred",
      "tick_counter": 913601,
      "timestamp_us": 1710000034032
    }
  ]
}
```

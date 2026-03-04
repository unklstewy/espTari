# API Contract Delta Template

Use this template for every API contract-affecting change.

---

## Delta ID
- `DELTA-YYYYMMDD-###`

## Date
- `YYYY-MM-DD`

## Sprint / Task
- Example: `S9-001`

## Contract Area
- Examples:
  - Envelope/Error model
  - Lifecycle guards
  - Stream metadata contract
  - Save/restore compatibility
  - Input mapping/event ordering

## Change Summary (Plain Language)
- What changed and why, in non-technical terms.

## Technical Summary
- Exact schema/guard/endpoint/runtime behavior changes.

## Compatibility Assessment
- Backward compatible? `Yes/No`
- Breaking impact (if any):
- Migration guidance (if any):

## Affected API Surfaces
- Endpoints:
- Error codes/guards:
- Event payloads:
- Contract sections in API spec:

## Files Changed
- List all edited files.

## Verification Evidence
- Tests/harness/scripts run:
- Captures/bundles:
- Build/route verification:

## Release Note Mapping
- Target release note section title/date:
- Summary line for release note:

## Acceptance Mapping
- Target acceptance log row:
- Acceptance evidence links:

## Decision
- `In Progress | Ready for Review | Accepted | Rejected | Deferred`
- Reviewer:
- Notes:

# Sprint 2 - Atari ST Core Runtime

Duration: 3 working days  
Goal: Establish Atari ST machine runtime path with deterministic scheduler skeleton and media attachment flow.

## Committed tasks

- T-014: Atari ST profile load path
- T-015: Deterministic scheduler skeleton
- T-016: ROM attach via catalog-backed IDs
- T-017: Disk attach/eject via catalog-backed IDs

## Stretch tasks

- T-018: Video stream metadata + payload channel
- T-019: Audio stream metadata + payload channel
- T-020: Register snapshot + stream

## Sprint demo scenarios

1. Load Atari ST profile and transition to running state.
2. Attach ROM and floppy by IDs via catalog resolution.
3. Start video/audio stream channels and verify metadata continuity.

## Acceptance criteria

1. ST profile load is deterministic and reproducible.
2. Media attach/eject by IDs succeeds with clear errors on invalid IDs.
3. Video/audio stream contracts hold under nominal load.

## Evidence package

- Session timeline log
- Media attach/eject trace
- Stream metadata and payload sequence checks
- Product Owner acceptance notes

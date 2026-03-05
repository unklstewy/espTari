# EBIN ST520/ST1040 Initial Build Decomposition (2026-03-04)

## Scope

Deliver the first runnable EBIN bundles for Atari ST 520 and Atari ST 1040 using the existing runtime ABI + loader path.

- Runtime loader anchor: `TRACKING/EBIN_REAL_DATA_LOADER_PIVOT_2026-03-04.md`
- Component readiness anchor: `TRACKING/ST_520_1040_EBIN_COMPONENT_UNBLOCK_MATRIX_2026-03-04.md`
- Backlog task source-of-truth: `TRACKING/BACKLOG.md` (`EBIN-201..EBIN-216`)

## Task decomposition (execution order)

| Order | Task ID | Objective | Primary output |
|---|---|---|---|
| 1 | `EBIN-202` | Freeze module naming/layout and metadata schema for initial ST packs | Stable naming + index/manifest contract |
| 2 | `EBIN-203` | Build `st.cpu.m68k` EBIN | CPU shared binary |
| 3 | `EBIN-204` | Build `st.chipset.glue_mmu_shifter` EBIN | Chipset shared binary |
| 4 | `EBIN-205` | Build `st.chipset.mfp` EBIN | MFP shared binary |
| 5 | `EBIN-206` | Build `st.io.acia_ikbd` EBIN | I/O shared binary |
| 6 | `EBIN-207` | Build `st.storage.dma_fdc` EBIN | Storage shared binary |
| 7 | `EBIN-208` | Build `st.audio_gpio.psg` EBIN | Audio/GPIO shared binary |
| 8 | `EBIN-209` | Build `st.profile.520` machine-profile EBIN and mapping proof | ST520 profile binary + mapping validation |
| 9 | `EBIN-210` | Build `st.profile.1040` machine-profile EBIN and mapping proof | ST1040 profile binary + mapping validation |
| 10 | `EBIN-211` | Assemble ST520 package/index and SD placement | ST520 package tree |
| 11 | `EBIN-212` | Assemble ST1040 package/index and SD placement | ST1040 package tree |
| 12 | `EBIN-213` | Validate deterministic runtime selection for ST520 | ST520 resolve/boot evidence |
| 13 | `EBIN-214` | Validate deterministic runtime selection for ST1040 | ST1040 resolve/boot evidence |
| 14 | `EBIN-215` | Run integration smoke matrix across both bundles | Matrix evidence set |
| 15 | `EBIN-216` | Assemble release packet + acceptance links | Review-ready packet |

Umbrella tracker: `EBIN-201`.

## Build families and IDs

Shared module families (used by both ST520/ST1040):

- `st.cpu.m68k`
- `st.chipset.glue_mmu_shifter`
- `st.chipset.mfp`
- `st.io.acia_ikbd`
- `st.storage.dma_fdc`
- `st.audio_gpio.psg`

Machine-profile modules:

- ST520: `st.profile.520`
- ST1040: `st.profile.1040`

## Acceptance gate checklist

A task is considered complete when all are true:

1. Build artifact exists at expected SD/runtime path.
2. Resolver and startup choose expected profile/module deterministically.
3. Existing S10 smoke families relevant to startup/integration remain deterministic.
4. Evidence artifact is generated and linked from acceptance/report docs.

## Evidence outputs (planned)

- Capture artifacts under `captures/` for each task slice (`EBIN-202..216`).
- Per-task evidence docs under `TRACKING/` following existing naming pattern.
- Consolidated closure summary after `EBIN-216`.

## Risks and controls

- Risk: module naming drift between builder, resolver, and loader mapping.
  - Control: lock naming in `EBIN-202` before component builds.
- Risk: ST1040 mapping ambiguity at loader profile resolution.
  - Control: explicit profile mapping check in `EBIN-210` + `EBIN-214`.
- Risk: package/index mismatch causing runtime fallback.
  - Control: package assembly checks in `EBIN-211`/`EBIN-212` and startup validation tasks.

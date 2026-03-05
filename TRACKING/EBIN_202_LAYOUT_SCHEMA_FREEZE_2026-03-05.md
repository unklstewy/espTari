# EBIN-202 Layout/Schema Freeze (2026-03-05)

## Objective

Freeze canonical ST520/ST1040 EBIN output layout, module naming, and manifest/index schema before component build tasks (`EBIN-203..EBIN-210`).

## Frozen contract

- Contract file: `docs/emu_engine_v2/reference_ebin_packages/st520_st1040_ebin_freeze_v1.json`
- Validator: `tools/validate_st_ebin_layout_schema.py`
- Smoke evidence script: `tools/smoke_ebin_202_layout_schema_freeze.sh`

## Frozen module naming set

- Shared modules:
  - `st.cpu.m68k`
  - `st.chipset.glue_mmu_shifter`
  - `st.chipset.mfp`
  - `st.io.acia_ikbd`
  - `st.storage.dma_fdc`
  - `st.audio_gpio.psg`
- Machine profile modules:
  - `st.profile.520`
  - `st.profile.1040`

## Frozen output layout (relative to SD root)

- `ebins/atari_st/cpu`
- `ebins/atari_st/chipset`
- `ebins/atari_st/io`
- `ebins/atari_st/storage`
- `ebins/atari_st/audio_gpio`
- `ebins/atari_st/machine_profile`
- `ebins/atari_st/packages`

## Frozen metadata schema IDs

- Component index: `st_component_index_v1`
- Component manifest: `st_component_manifest_v1`
- Machine package index: `st_machine_package_index_v1`
- Machine package manifest: `st_machine_package_manifest_v1`

## Notes

- Existing machine-profile fixture flow remains valid; smoke injects frozen schema keys before validator checks.
- This freeze intentionally does not build component EBIN binaries. Build execution starts in `EBIN-203`.
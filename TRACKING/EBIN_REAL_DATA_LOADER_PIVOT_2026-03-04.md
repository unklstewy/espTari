# EBIN Real-Data Loader Pivot (2026-03-04)

## Goal

Move beyond stub startup behavior so boot profile selection consumes real EBIN artifacts from SD card.

## Implemented

1. Runtime machine-profile EBIN discovery in loader startup:
   - Scan path: `/sdcard/ebins/atari_st/machine_profile`
   - Parse `*.ebin` filenames as `<module_id>-<semver>.ebin`
   - Select highest semver candidate
2. Startup machine load wiring:
   - `loader_init()` now resolves profile source and calls `machine_load(resolved_profile)`
3. Resolved profile source logging:
   - Logs module/version source as `runtime_ebin` or `fallback`

## Files changed

- `components/esptari_loader/esptari_loader.c`

## Runtime behavior

- If runtime EBIN candidates exist, loader selects highest semver module and maps known `st.profile.520` to runtime profile `st_520_pal`.
- If no runtime candidates exist, loader falls back to baked defaults and preserves prior startup compatibility (`st_default`).

## Validation

- Build completed successfully after loader changes:
  - `espIdfCommands(build)` on workspace root (successful link + image generation)

## Notes

- This change completes loader-side wiring needed for real-data profile selection at startup.
- Additional profile mapping rules can be extended in loader once more machine-profile module IDs are introduced.

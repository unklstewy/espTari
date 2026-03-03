# Logic Analyzer Capture Summary — 2026-03-02

## Scope

Merged evidence summary for UART + debug strobe captures executed after adding SD mount strobes on exposed pins.

Firmware instrumentation source:
- `main/main.c` (`GPIO2` window strobe, `GPIO3` edge strobe around `esp_vfs_fat_sdmmc_mount`).

## Channel Mapping (Analyzer D0..D3)

- `D0` → `UART0_TXD` (GPIO37)
- `D1` → `UART0_RXD` (GPIO38)
- `D2` → SD mount window strobe (GPIO2)
- `D3` → SD mount edge marker strobe (GPIO3)

## Capture Runs

### Run 1 (free-run)

- Command profile: `fx2lafw`, `24MHz`, channels `D0,D1,D2,D3`, `--time 50s`
- Artifact: `captures/sd_mount_uart_strobe_run1.sr` (959K)
- UART decode (data-only): `captures/sd_mount_uart_strobe_run1_uart_data.txt` (14K, 1135 lines)
- UART decode (full): `captures/sd_mount_uart_strobe_run1_uart.txt` (141K)

### Run 2 (triggered)

- Trigger profile: `fx2lafw`, `24MHz`, channels `D0,D1,D2,D3`, `--triggers D2=r --wait-trigger --time 20s`
- Trigger source: rising edge on SD window strobe (`GPIO2`)
- Artifact: `captures/sd_mount_uart_strobe_run2_triggered.sr` (25K)
- Acquisition note: device reported `10315625` samples captured
- Effective captured span from sample count: `10315625 / 24e6 = 0.429818s`
- UART decode (data-only): `captures/sd_mount_uart_strobe_run2_triggered_uart_data.txt` (19K, 1584 lines)
- UART decode (full): `captures/sd_mount_uart_strobe_run2_triggered_uart.txt` (197K)

## Key Decoded UART Evidence (Run 2)

Reconstructed leading text from `run2_triggered_uart_data`:

- `ESP-ROM:esp32p4-eco2-20240710`
- `Build:Jul 10 2024`
- `rst:0x1 (POWERON),boot:0x307 (DOWNLOAD(USB/UART0/SPI))`
- `waiting for download`

## Outcome

- Triggered logic capture is successful and synchronized to strobe edge (`D2` rising).
- UART decode output is present and readable for both runs.
- Current triggered trace captures ROM/download-mode boot sequence and confirms synchronization path.

## Follow-up Recommendation

For application-level boot timing evidence (beyond ROM downloader banner), arm the same trigger capture and use reset-only sequencing (without flashing) so UART transitions into app logs within the triggered window.

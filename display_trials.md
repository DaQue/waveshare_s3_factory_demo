# Display Bring-up Trials (waveshare_s3_3p)

Log results from the on-device prompts here so we avoid repeating combos.

Legend (current prompt):
- p = pass (solid green or box/X visible)
- gf = green flash
- nf = noise flash
- b = blank
- pending = not run yet
- f = fail (legacy)

## Trials
| ID | Trial name | PCLK | Result (green) | Result (box/X) | Notes |
|----|------------|------|----------------|----------------|-------|
| 01 | QSPI_m0_cmd8_param8_rgb_big | 5 MHz | b | b | Brief noise on replug, then blank |
| 02 | QSPI_m3_cmd8_param8_rgb_big | 5 MHz | b | b | Brief noise on replug, then blank |
| 03 | QSPI_m0_cmd16_param16_rgb_big | 5 MHz | b | b | Brief noise on replug, then blank |
| 04 | SPI_m0_cmd8_param8_rgb_big_sio | 5 MHz | b | b | Brief noise on replug, then blank |
| 05 | SPI_m3_cmd8_param8_rgb_big_sio | 5 MHz | b | b | Brief noise on replug, then blank |
| 06 | QSPI_m0_cmd8_param8_bgr_big_invert | 5 MHz | b | b | Brief noise on replug, then blank |

## Lane Map Trials (QSPI, 5 MHz)
| ID | Trial name | PCLK | Result (green) | Result (box/X) | Notes |
|----|------------|------|----------------|----------------|-------|
| L01 | QSPI_m0_cmd8_param8_rgb_big_L0123 | 5 MHz | b | b | Brief noise on replug, then blank |
| L02 | QSPI_m0_cmd8_param8_rgb_big_L1023 | 5 MHz | b | b | Brief noise on replug, then blank |
| L03 | QSPI_m0_cmd8_param8_rgb_big_L2301 | 5 MHz | b | b | Brief noise on replug, then blank |
| L04 | QSPI_m0_cmd8_param8_rgb_big_L3210 | 5 MHz | b | b | Brief noise on replug, then blank |

## AXS15231B Driver Trials (QSPI, 5 MHz, cmd32/param8, mode3)
| ID | Trial name | PCLK | Result (green) | Result (box/X) | Notes |
|----|------------|------|----------------|----------------|-------|
| A01 | QSPI_m3_cmd32_param8_rgb_big_L0123 | 5 MHz | pending | pending | AXS15231B driver + factory init table |
| A02 | QSPI_m3_cmd32_param8_rgb_big_L1023 | 5 MHz | pending | pending | AXS15231B driver + factory init table |
| A03 | QSPI_m3_cmd32_param8_rgb_big_L2301 | 5 MHz | pending | pending | AXS15231B driver + factory init table |
| A04 | QSPI_m3_cmd32_param8_rgb_big_L3210 | 5 MHz | pending | pending | AXS15231B driver + factory init table |

## AXS15231B Automated Sweep (firmware-driven)
- The current firmware cycles many configs and prompts `p` (pass) / `f` (fail).
- It waits ~3s between trials and stops on the first pass.
- Record the winning trial name from the log, plus any observed behavior.

### Sweep Log (2026-02-01)
| Trial | Result | Notes |
|-------|--------|-------|
| QSPI_m3_cmd32_param8_rgb_big_L0123 | nf | Noise flash then black |
| QSPI_m3_cmd32_param8_rgb_little_L0123 | gf | Green flash then black |
| QSPI_m3_cmd32_param8_bgr_big_L0123 | f | User entered `f` (treated as fail) |
| QSPI_m3_cmd32_param8_rgb_big_inv_L0123 | gf | Green flash then black |
| QSPI_m0_cmd32_param8_rgb_big_L0123 | gf | Green flash then black |
| QSPI_m3_cmd8_param8_rgb_big_L0123 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_sio_L0123 | b | SIO + quad error spam |
| QSPI_m3_cmd32_param8_rgb_big_1MHz_L0123 | gf | Green flash then black |
| QSPI_m3_cmd32_param8_rgb_big_definit_L0123 | gf | Green flash then black |
| QSPI_m3_cmd32_param8_rgb_big_L1023 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_little_L1023 | b | Blank |
| QSPI_m3_cmd32_param8_bgr_big_L1023 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_inv_L1023 | b | Blank |
| QSPI_m0_cmd32_param8_rgb_big_L1023 | b | Blank |
| QSPI_m3_cmd8_param8_rgb_big_L1023 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_sio_L1023 | b | SIO + quad error spam |
| QSPI_m3_cmd32_param8_rgb_big_1MHz_L1023 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_definit_L1023 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_L2301 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_little_L2301 | b | Blank |
| QSPI_m3_cmd32_param8_bgr_big_L2301 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_inv_L2301 | b | Blank |
| QSPI_m0_cmd32_param8_rgb_big_L2301 | b | Blank |
| QSPI_m3_cmd8_param8_rgb_big_L2301 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_sio_L2301 | b | SIO + quad error spam |
| QSPI_m3_cmd32_param8_rgb_big_1MHz_L2301 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_definit_L2301 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_L3210 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_little_L3210 | b | Blank |
| QSPI_m3_cmd32_param8_bgr_big_L3210 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_inv_L3210 | b | Blank |
| QSPI_m0_cmd32_param8_rgb_big_L3210 | b | Blank |
| QSPI_m3_cmd8_param8_rgb_big_L3210 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_sio_L3210 | b | SIO + quad error spam |
| QSPI_m3_cmd32_param8_rgb_big_1MHz_L3210 | b | Blank |
| QSPI_m3_cmd32_param8_rgb_big_definit_L3210 | b | Blank |
| SPI_m3_cmd8_param8_rgb_big | b | Blank |
| SPI_m0_cmd8_param8_rgb_big | gf | Green flash then black |

### Sweep v2 (full-frame attempt, RAMWR forced)
| Trial | PCLK | Result | Notes |
|-------|------|--------|-------|
| QSPI_m3_cmd32_param8_rgb_big_5MHz_L0123_full | 5 MHz | gf | Green flash then blank |
| QSPI_m3_cmd32_param8_rgb_little_5MHz_L0123_full | 5 MHz | gf | Green flash |
| QSPI_m3_cmd32_param8_rgb_big_40MHz_L0123_full | 40 MHz | gf | Green flash |
| QSPI_m0_cmd32_param8_rgb_big_40MHz_L0123_full | 40 MHz | gf | Green flash |
| QSPI_m3_cmd32_param8_rgb_little_40MHz_L0123_full | 40 MHz | gf | Green flash |

Note: full-frame alloc still fails; all trials fell back to chunked draw with RAMWR forced.

### Sweep v3 (QIO cmd8 vs QSPI cmd32, qspi_if toggle)
| Trial | Result | Notes |
|-------|--------|-------|
| QIO_cmd8_qspiif0_m3_5MHz | b/r then off | Blue on left, red on right, then display off |
| QIO_cmd8_qspiif0_m0_5MHz | b | Blank |
| QIO_cmd8_qspiif0_m3_40MHz | b | Blank |
| QSPI_cmd32_qspiif1_m3_5MHz | b | Blank |
| QSPI_cmd32_qspiif1_m3_40MHz | gf | Green flash then blank |

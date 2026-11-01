# MPPT characterization

Bench characterization of the two MPPTs we have run on the car, and the custom
firmware we wrote for the Dilithium Photon 3.

- **TPEE SEC-B175-7A** (Open-SEC firmware 1.12): efficiency map, 114 points.
- **Dilithium Photon 3.2** (unit serial 15): efficiency at the same points as
  the TPEE, a rewritten tracking algorithm, tracking tests against a bench
  supply, and an 8 A overcurrent/soak test.

All power and efficiency figures come from the bench instruments (Sorensen
XPF 60-10DP readback for input, BK 8502 load for output), not from the MPPTs'
own sensing.

## Bench setup

| Item | Use |
| --- | --- |
| Sorensen XPF 60-10DP (USB serial) | CH2 = array input to the MPPT; CH1 = 12 V logic for the Photon and the CAN bridge |
| BK 8502 electronic load, CV mode | Battery stand-in on the MPPT output (115 to 117.5 V) |
| Tritium CAN-Ethernet bridge | Photon CAN at 500 kbit/s; bridge at 169.254.210.192, bus 13, TCP 4876 |
| Phone / laptop camera | Reads the BK load display during TPEE sweeps (`tools/loadcam`) |

The Mac reaches the bridge through a link-local address on the USB Ethernet
adapter (`sudo ifconfig <if> alias 169.254.1.10 255.255.0.0`, or a persistent
second network service with that address). A flaky USB Ethernet adapter
caused hours of dropouts; if the link keeps dropping, swap the adapter first.

## Results

### TPEE efficiency map (`tpee/results/`)

- 114 points, 10 to 60 V input, 0.5 A steps, 115 V output.
- Efficiency rises from about 92 % at 0.5 A to about 98.5 % at 2.5 to 4.5 A
  (full range 83.4 to 99.6 %; the extremes are at the noisiest points).
- Measured with the input voltage held (`--hold`, the firmware's
  SetInputVoltage), so this is conversion efficiency, not tracking efficiency.
- Unverified: readings near 99.6 % at 10 to 15 V and 3.5 to 4 A, and a 2.5 to
  5 point drop at 4 to 5 A on the 10 to 20 V curves (possibly the XPF changing
  current range). Check with a shunt before relying on them.

### Photon vs TPEE (`tpee/results/photon_vs_tpee_comparison.csv`)

- 17 points with the Photon tracking a 60 V supply, 0.25 to 4.25 A, compared
  with the TPEE map at the same input voltage and current.
- From 1.5 A up the two are within 1.4 points of each other (Photon slightly
  lower). Below 1.25 A the Photon reads 1 to 2.5 points higher, but those
  points are noisier and use the nearest TPEE point.
- The Photon also draws about 1.8 W from its 12 V logic supply, which these
  efficiencies do not include (the TPEE draws close to nothing).
- Points at 3.5 A and above hit the XPF power envelope (unregulated).

### Photon tracking firmware (`photon/firmware/`)

The stock firmware does not track a bench supply's corner (set voltage x
current limit). It stalls on the supply's constant-voltage side or sits at
about 73 % of the set voltage. The rewritten tracker draws 95.7 to 99 % of
V x I at every reachable corner tested, including abrupt changes between
corners. Full grids are in `photon/bench/logs/corners_*.csv`.

| Corner | Stock | Custom (v7) |
| --- | --- | --- |
| 20 V / 1 A | 3 % | 96.7 % |
| 45 V / 3 A | 72 % | 98.4 % |
| 60 V / 1.5 A | 74 % | 98.9 % |
| 60 V / 2.8 A | 74 % | 98.5 % |
| 40 V / 4.4 A | 52 % | 98.7 % |
| 15 V / 2 A | n/a | 96.7 % |

What changed (one commit per step in `photon/firmware/patches/`):

- P&O reverses on power averaged over each 100 ms step, not on a 1 V voltage
  move (which never happens against a supply in CV).
- The PI integrator keeps its remainder, so small voltage errors still move
  the current.
- The voltage goal is clamped close to the measured voltage. When the source
  cannot reach the goal even at minimum current, the goal moves below it.
- Input collapses (source current limit hit) are caught, and the firmware
  learns that limit as a soft ceiling and re-probes it.
- The MCP4728 DAC FSM now refreshes the 1.65 V sense references every 10th
  write (see "Known issues").
- A CAN service for reading state and changing tuning live (base+0xD0
  requests, base+0xE0 replies), a hold-input-voltage mode, and a 50 Hz debug
  stream.

### Hardware limits found

- **Minimum input current.** The FPGA's fixed minimum on-time means the Photon
  cannot draw less than about 0.47 A at about 45 V input. A 60 V / 0.5 A
  corner is out of reach (74 %); the stock firmware stops at the same point.
- **Current sensing.** The input current reading is within about 2 % of the
  XPF from 3 to 8 A, then saturates at the ADC full scale (about 8.0 to 8.1 A).
- **Factory current ceiling.** `MAXDAC` 2023 limits input to about 7.5 A.

### 8 A test (`photon/bench/logs/overcurrent_*`, `soak_*`)

With the current ceiling raised to about 8.3 A and the firmware overcurrent
trip to 8.8 A (both settable over CAN, RAM only), at 24 V and an 8 A supply
limit:

- 24 minutes, no faults or trips. Temperature went from 29 to 37 °C and was
  still rising about 0.25 °C/min at the end.
- Average draw 7.5 A (92.9 % of V x I), with peaks to 8.4 A. The tracker hunts
  between about 7.3 and 8.2 A because the current reading saturates near 8 A.
- At a 7.5 A limit it holds 98 % of V x I.

A longer test at a steady 8 A is still to be done.

## Repository layout

```
tpee/                  TPEE sweep tools, raw sweeps and results
  sweep.py             automated sweep (XPF + TPEE serial + load camera)
  tpee.py              TPEE serial client (Open-SEC protocol)
  merge.py, reprocess.py, restrip.py, dashboard.py
  results/             final efficiency map and Photon comparison
  raw/                 every sweep run (CSV + console logs)
photon/
  bench/               Photon bench tools (CAN client, flasher, tests)
    README.md          how to run each tool
    logs/              all test logs (large raw logs gzipped)
  firmware/            buildable firmware tree (same layout as Dilithium's)
    photon_3/Photon_3_0/   application source with our changes
    patches/           our 12 commits on top of the Dilithium source
    can_bootloader/, nanopb/, dspic_include/, util/
  images/
    factory_backup_serial15_20261005/   read back from the unit, CRC-verified
    photon3_tracker_*.hex              built images
tools/                 XPF and Rigol supply drivers, load-display camera reader
```

The TPEE firmware and ReboostTool are upstream projects and are not copied
here: [OpenSmartEnergyConverter](https://github.com/TjitteS/OpenSmartEnergyConverter).

## Photon firmware: build and flash

Build with Microchip XC16 2.10 (free mode):

```
cd photon/firmware/photon_3/Photon_3_0
PATH=<xc16>/bin:$PATH make fw.hex
```

Do not run `make clean`: it deletes the checked-in generated `*.pb.*` files.
The legacy peripheral library is replaced by `periph_compat.c` (the bundled
one is COFF and does not link with XC16 2.x). The build reproduces the image
on the unit byte for byte.

Flash over CAN through the bootloader (the bootloader itself is never
written):

```
python3 photon/bench/flash.py photon/firmware/photon_3/Photon_3_0/fw.hex --yes
```

Restore the factory image the same way with
`photon/images/factory_backup_serial15_20261005/restore_app.hex`.

Turn the array input off before flashing. If a flash is interrupted, the
unit stays in its bootloader; rerun with `--resume`.

## Known issues

- At 5 A on one run, the channel 0 voltage and current readings latched
  below zero and the converter stopped (no damage; a reset cleared it). The
  suspected cause is I2C corruption of the DAC channels that hold the sense
  references. The refresh fix has run clean since, including the 24-minute
  soak, but has not been proven with a long soak at 5 to 7.5 A.
- At turn-on the output briefly exceeded the 130.2 V CV limit
  (CONSTANT_VOLTAGE state for a moment) while the main relay closed. It
  cleared on its own; not investigated.
- Tuning registers live in RAM and reset at power-off. Nothing writes FRAM.

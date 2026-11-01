# Photon 3 bench tools

Bench: Sorensen XPF 60-10DP (CH1 = 12 V logic for Photon + CAN bridge, CH2 = array
input to Photon channel 0), BK 8502 load in CV on the Photon output, Tritium
CAN-Ethernet bridge at 169.254.210.192 (bus 13, 500 kbit/s).

The Mac reaches the bridge only from a link-local alias, which does not survive a
reboot:

    sudo ifconfig en8 alias 169.254.1.10 255.255.0.0

Use `~/.rigol-venv/bin/python` for anything that talks to the XPF.

| Tool | What it does |
| --- | --- |
| `photon_can.py` | Bridge client, stock telemetry decode, bootloader commands |
| `backup.py` | Read-only backup: hw rev, FRAM, all program memory (`backups/`) |
| `flash.py HEX --yes` | Program the application through the CAN bootloader and verify |
| `svc.py` | Firmware service: `id`, `get`, `set`, `stop`, `start`, `retrack`, `dump` |
| `monitor.py` | Photon telemetry + XPF readback, 1 s summaries |
| `trace.py` | Tracker internals (debug stream) + XPF readback |
| `corners.py V:I ...` | Score power drawn vs the supply corner V*I, from XPF readback |
| `retrack_test.py` | Force retracks and measure the power lost |

Restore the factory firmware (verified against the FRAM CRC):

    python3 flash.py backups/20261005_214225/restore_app.hex --yes

Build the firmware (XC16 2.10 in `toolchain/xc16`):

    cd ~/Downloads/photon/software/photon_3/Photon_3_0
    PATH=$HOME/photon_bench/toolchain/xc16/bin:$PATH make fw.hex

Do not run `make clean`: it deletes the checked-in generated `*.pb.*` files.

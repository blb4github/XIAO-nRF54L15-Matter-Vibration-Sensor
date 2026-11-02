# Packaging a tested firmware snapshot and preparing a release

The project separates **reproducible source/configuration** from **flashable firmware artifacts**:

- The Git repository will contain the real app sources, `CMakeLists.txt`, `prj.conf`, `app.overlay`, `pm_static.yml`, ZAP template/generated files, necessary custom board support, and documentation.
- **GitHub Release assets** are the intended location for a tested and audited firmware file (`merged.hex`) and its SHA-256 checksum.
- Zephyr's `build/` directory is **not** part of source control.

**Current state (2 November 2026): documentation published; original firmware source and binary still on the development Mac, not yet audited or published.**

## 1. Prepare a private snapshot on the development Mac

Review [the exporter](../tools/export_tested_snapshot.py) first.

Run the following from a terminal on the development Mac:

```bash
cd ~/Documents/XIAO/matter_mailbox
curl -fLsS \
  https://raw.githubusercontent.com/blb4github/XIAO-nRF54L15-Matter-Vibration-Sensor/main/tools/export_tested_snapshot.py \
  -o /tmp/export_tested_snapshot.py
python3 /tmp/export_tested_snapshot.py
```

Default output:

```text
~/Downloads/XIAO-nRF54L15-Matter-Vibration-Sensor-review.zip
```

The ZIP is a **private review package** containing:

- Actual project source/configuration files (excluding build directories and typical backups).
- Custom board-support source files.
- **`firmware/merged.hex` copied from the local build**.
- A JSON manifest with SHA-256 hashes for each file.

Do **not** upload this ZIP directly to the public repository. This package needs a source, binary, licensing and secret review. Some device-specific Matter factory data or development keys may be embedded in binaries as well as source.

The exporter does not rebuild, flash or change the working project.

## 2. Check the snapshot against the tested installation

Verify the snapshot corresponds to the firmware running on the board:

- IMU GPIO wake-up interrupt with a retriggerable five-second Matter alarm.
- Green user LED gives a nonblocking **100 ms pulse** on every confirmed trigger.
- Power Source cluster reports ADC battery voltage and approximate percentage.
- `CONFIG_CHIP_ENABLE_ICD_SUPPORT=y`, Thread SED, and 1000 ms slow-poll interval.
- Current ZAP outputs, Power Source implementation, custom board-root, and required build files are complete.
- A clean build works in the documented NCS version. Do not claim byte-for-byte reproducibility until tested.

## 3. Public source publication

Publish only audited, nonsecret sources while preserving applicable upstream license notices. Do not publish Thread operational datasets, Matter setup passcodes, private keys, device-specific DAC private keys, Wi-Fi/network credentials, or logs showing local infrastructure.

## 4. Public firmware release (after audit)

Only after verifying the image is suitable for redistribution:

1. Create a GitHub Release (e.g. `v0.1.0`) from the audited source commit.
2. Attach the tested `merged.hex` and a SHA-256 checksum file.
3. Document the XIAO nRF54L15 Sense target, SDK version, firmware hash, memory layout, limitations and flashing steps.

**Do not publish a paired device's persistent flash dump.** On-board NVM can contain Matter fabrics, operational keys and Thread credentials. A compiled `merged.hex` is not a flash dump but can still embed factory data and secrets.

## 5. Flashing notes

See [build-flash.md](build-flash.md) for the tested pyOCD invocation. For the paired test device, `--erase sector` was used with an unchanged partition layout. It worked in local testing but does not guarantee preservation for arbitrary memory-layout changes.

Until a reviewed release exists, `build/merged.hex` on the development Mac is the only identified tested image.

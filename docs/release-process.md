# Packaging a tested firmware snapshot and preparing a release

The project separates **reproducible source/configuration** from **flashable firmware artifacts**:

- The Git repository will contain the real app sources, `CMakeLists.txt`, `prj.conf`, `app.overlay`, `pm_static.yml`, ZAP template/generated files, necessary custom board support, and documentation.
- **GitHub Release assets** are the intended location for a tested and audited firmware file (`merged.hex`) and its SHA-256 checksum.
- Zephyr's `build/` directory is **not** part of source control.

**Current state (2 November 2026): original source/configuration files have been published and a clean build from a fresh GitHub download has succeeded. The new image was not flashed or behavior-tested; no public binary is released because generated Matter factory data needs a safe per-device provisioning process.**

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

Do **not** upload this ZIP directly to the public repository. This package needs a source, binary, licensing and secret review. The uploaded tested HEX **does** contain a DAC private key, commissioning passcode, SPAKE2+ verifier and enable key in its factory-data partition. It must not be published unchanged.

The exporter does not rebuild, flash or change the working project.

The verified **source-only** importer used for the published snapshot is available at [tools/import_reviewed_snapshot.py](../tools/import_reviewed_snapshot.py). It uses the exact reviewed ZIP (checksum-pinned) to publish code/configuration without its firmware factory-data region. See [build-flash.md](build-flash.md#import-the-actual-tested-source-tree) for the Mac command.

## 2. Check the snapshot against the tested installation

Verify the snapshot corresponds to the firmware running on the board:

- IMU GPIO wake-up interrupt with a retriggerable five-second Matter alarm.
- Green user LED gives a nonblocking **100 ms pulse** on every confirmed trigger.
- Power Source cluster reports ADC battery voltage and approximate percentage.
- `CONFIG_CHIP_ENABLE_ICD_SUPPORT=y`, Thread SED, and 1000 ms slow-poll interval.
- Current ZAP outputs, Power Source implementation, custom board-root, and required build files are complete.
- **Clean build: verified on 2 November 2026** using NCS 3.0.2 and Zephyr SDK 0.17.0, from a fresh GitHub archive. Do not claim byte-for-byte reproducibility or successful device commissioning based on the compile alone.

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

The imported sources are available and **the clean-build procedure has been validated** on the development Mac; no public binary release exists. `build/merged.hex` on the development Mac remains the identified tested image and must not be publicly redistributed unchanged.

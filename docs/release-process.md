# Source-only release workflow and future firmware distribution

The project separates **reproducible source/configuration** from **flashable firmware artifacts**:

- The Git repository will contain the real app sources, `CMakeLists.txt`, `prj.conf`, `app.overlay`, `pm_static.yml`, ZAP template/generated files, necessary custom board support, and documentation.
- For now, **public releases consist only of tagged source revisions and documentation**. No compiled HEX is distributed.
- A future optional binary distribution could publish a reviewed application image separately from each device's Matter factory data, but that is **not part of the present release scope**.
- Zephyr's `build/` directory is **not** part of source control.

**Decision (2 November 2026): source-only development releases (option 1).** The public source compiles successfully from a clean checkout, but the newly compiled image has not been flashed or field-tested. The original image contains device-specific Matter secrets; it must not be distributed. A universal prebuilt image and automated per-device provisioning (option 2) are deferred pending demonstrated community demand.

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

## 4. Source-only development releases — current policy

For now the project follows **option 1**:

1. Publish the reviewed source and board/sysbuild configuration, including ZAP definitions.
2. Verify that a clean build succeeds for the documented NCS/toolchain version.
3. Optionally tag a known source revision (e.g. `v0.1.0`) with clear release notes, without attaching a compiled firmware HEX.
4. Explain that each builder is responsible for generating suitable **device-specific Matter development/test factory data**, flashing and commissioning their own board. A clean compilation does not establish that provisioning and commissioning were tested.
5. Keep the original prototype firmware and other device-specific credentials private.

**Option 2 is deferred:** a common, independently flashable application image plus a tested local tool for per-device factory data and onboarding. It can be reconsidered if there is sustained user demand. The provisioning workflow would not necessarily change for every application update when partitions and interfaces stay stable, but each distributed image and supported upgrade path would still need validation.

Do **not** publish the prototype `merged.hex`: its factory-data partition contains DAC private key, commissioning passcode, SPAKE2+ verifier and enable-key material. Do not publish a commissioned device's flash dump either.

## 5. Flashing notes

See [build-flash.md](build-flash.md) for the tested pyOCD invocation. For the paired test device, `--erase sector` was used with an unchanged partition layout. It worked in local testing but does not guarantee preservation for arbitrary memory-layout changes.

The imported sources are available and **the clean-build procedure has been validated** on the development Mac; no public binary release is planned for the current source-only scope. `build/merged.hex` on the development Mac remains the identified tested image and must not be publicly redistributed unchanged.

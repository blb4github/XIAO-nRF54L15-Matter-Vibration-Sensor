# Build, ZAP generation, flashing and persistent settings

## Verified development environment

- **Nordic Connect SDK 3.0.2**, Zephyr and Matter.
- Board target: `xiao_nrf54l15/nrf54l15/cpuapp`.
- Working local project: `~/Documents/XIAO/matter_mailbox`.
- Custom board root: `~/Documents/XIAO/board-root`.
- ZAP **v2024.08.14**.
- pyOCD with the `nrf54l` target and a supported debug probe.

The *exact clean-build invocation*, including board-root parameters and dependencies, must be extracted from the working project before a full reproducible release can be claimed. The [tested snapshot exporter](release-process.md) captures sources and the locally built binary for review, but does not make the project reproducible by itself.

Building from an already configured build directory was successfully performed with:

```bash
cd ~/Documents/XIAO/matter_mailbox
cmake --build build
```

The build creates `build/merged.hex` through Zephyr sysbuild/MCUboot.

## Import the actual tested source tree

The complete tested source and configuration files are included in the reviewed ZIP snapshot created on the development Mac. An importer now validates the exact ZIP SHA-256, checks per-file hashes and source paths, and copies only the source files into this public repository.

See [the source importer](../tools/import_reviewed_snapshot.py). Run it **on the development Mac** using the same ZIP that was reviewed:

```bash
cd ~/Documents/XIAO/matter_mailbox
curl -fLsS \
  https://raw.githubusercontent.com/blb4github/XIAO-nRF54L15-Matter-Vibration-Sensor/main/tools/import_reviewed_snapshot.py \
  -o /tmp/import_reviewed_snapshot.py
python3 /tmp/import_reviewed_snapshot.py --push
```

This creates a separate public Git working copy and commits the exact tested app configuration, generated ZAP files, sysbuild files and custom board root. It **never uploads firmware/merged.hex**. You must have Git credentials configured on the development Mac.

## Rebuilding from a clean checkout

The project uses a custom Zephyr board root. With **NCS v3.0.2 properly activated** in your terminal, the intended command from the repository root is:

```bash
west build --sysbuild \
  -b xiao_nrf54l15/nrf54l15/cpuapp \
  -d build \
  -p always \
  -- -DBOARD_ROOT="$PWD/board-root"
```

**Verification status:** The original project was built successfully from its existing configured build directory, but this exact clean-checkout command has **not yet been independently validated** after importing the source into GitHub. The generated firmware may differ between toolchain/build environments; compare configuration and hashes when validating a release.

## Regenerate Matter ZAP files

After editing and saving `src/default_zap/template.zap` in ZAP:

```bash
cd ~/Documents/XIAO/matter_mailbox
python3 /opt/nordic/ncs/v3.0.2/modules/lib/matter/scripts/tools/zap/generate.py \
  "$PWD/src/default_zap/template.zap" \
  -t /opt/nordic/ncs/v3.0.2/modules/lib/matter/src/app/zap-templates/app-templates.json \
  -o "$PWD/src/default_zap/zap-generated"
cmake --build build
```

The application supplies the correct paths for `DTC_OVERLAY_FILE` so the battery ADC channel appears in the generated devicetree.

## Flashing the working firmware

**At the moment there is no public release binary.** The only identified tested image is the development Mac's `build/merged.hex`; the [review/release workflow](release-process.md) explains how to export and audit it before publication.

When a verified release asset becomes available, users should first check that the file and board version match the documentation and verify its SHA-256. The example below uses the locally built file.

## Flash without deliberately erasing the Matter fabric

Example using pyOCD with a **placeholder** probe identifier:

```bash
cd ~/Documents/XIAO/matter_mailbox
~/.local/bin/pyocd flash \
  --target nrf54l \
  --uid YOUR_PROBE_UID \
  --erase sector \
  build/merged.hex
```

**SECURITY:** The original tested `merged.hex` has a populated Matter factory-data region containing a DAC private key, commissioning passcode, SPAKE2+ verifier and enable key. It is **not a public universal image** and must not be attached to a GitHub Release unchanged.

**Do not use `--erase chip` on a commissioned device** if you intend to preserve Matter credentials and Thread settings. Changing flash partition layout may still destroy data even when sector erase is selected; always compare the new and current memory layouts.

## Settings / commissioning investigation

Initial commissioning failed with:

```text
CHIP_ERROR_PERSISTED_STORAGE_FAILED (0x9f)
```

The initial settings partition was **8 KiB**, corresponding to two 4-KiB ZMS sectors. Enlarging it to **16 KiB** (four sectors) correlated with successful commissioning, although the exact low-level causal failure was not isolated.

Partition layout verified for the original working baseline:

| Partition | Start | End (exclusive) |
|---|---:|---:|
| `mcuboot_secondary` | `0x0D6000` | `0x160000` |
| `factory_data` | `0x160000` | `0x161000` |
| `settings_storage` | `0x161000` | `0x165000` |

One key configuration is `CONFIG_PM_PARTITION_SIZE_SETTINGS_STORAGE=0x4000`. The primary application partition was not changed by this fix.

Extensive ZMS/Settings debug logging correlated with a `BT RX WQ` stack overflow during tests. The project was returned to normal logging levels and retained the 16-KiB settings partition.

## Build pitfalls observed

- The application overlay must actually be passed to Zephyr: missing `app.overlay` caused the initial battery ADC devicetree compile problem.
- A previously built Matter `FactoryDataProvider` object could retain a stale device ordinal after a devicetree change; rebuilding that object resolved that local linker failure.
- Enabling the Power Source cluster adds a required server implementation; link the proper Power Source server code instead of a dummy init callback.
- Flash and RAM headroom were limited. Check actual memory usage after adding features; older memory numbers do not describe the latest ICD build.
- **Never upload factory-data secrets, private keys, pairing passcodes, Thread datasets, or device-specific credentials** to this public repository.

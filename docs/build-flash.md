# Build, ZAP generation, flashing and persistent settings

## Verified development environment

- **Nordic Connect SDK 3.0.2**, Zephyr and Matter.
- Board target: `xiao_nrf54l15/nrf54l15/cpuapp`.
- Working local project: `~/Documents/XIAO/matter_mailbox`.
- Custom board root: `~/Documents/XIAO/board-root`.
- ZAP **v2024.08.14**.
- pyOCD with the `nrf54l` target and a supported debug probe.

The *exact clean-build invocation*, including board-root parameters and dependencies, must be extracted from the working project before a full reproducible release can be claimed.

Building from an already configured build directory was successfully performed with:

```bash
cd ~/Documents/XIAO/matter_mailbox
cmake --build build
```

The build creates `build/merged.hex` through Zephyr sysbuild/MCUboot.

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

# Publishing the tested source tree

**The public repository is live, but its initial publication is documentation-only.** The real firmware lives in the developer's working directory on a separate Mac and has not yet been transferred. Publishing a reconstructed implementation would risk departing from the firmware that was actually tested.

## Export and release workflow

A source-and-binary snapshot exporter is available under [`tools/export_tested_snapshot.py`](../tools/export_tested_snapshot.py). It assembles a **private review ZIP** from the tested Mac project and includes `build/merged.hex`, without modifying the device. See the [full review/release workflow](release-process.md). The reviewed firmware **contains** DAC private key, Matter passcode, SPAKE2+ verifier and enable-key fields. Do not publish the ZIP or binary. Use [the checksum-pinned source-only importer](../tools/import_reviewed_snapshot.py) to publish the audited source/configuration files from the reviewed snapshot.

## Proposed source layout

```text
XIAO-nRF54L15-Matter-Vibration-Sensor/
├── README.md
├── LICENSE
├── .gitignore
├── CMakeLists.txt
├── prj.conf
├── app.overlay
├── pm_static.yml
├── src/
│   ├── app_task.cpp
│   ├── battery_monitor.cpp
│   └── default_zap/
│       ├── template.zap
│       └── zap-generated/
└── docs/
    ├── hardware.md
    ├── firmware.md
    ├── build-flash.md
    ├── validation.md
    └── publishing.md
```

**Illustrative only:** the tested project contains more code and configuration than the small file list above. Publish everything necessary for the actual successful build, including required files from the custom board root (subject to their licenses).

## Before committing the source

- Start from the current **tested** local source tree.
- Exclude generated build directories and binary artifacts.
- Review all files *and Git history* for credentials, Matter factory attestation keys, setup passcodes, Thread datasets, private key material, local IP addresses, local automation-specific names, provisioning logs and secrets.
- Verify licenses for Nordic/Zephyr/Matter and Seeed board assets. The project MIT license does not override upstream licenses.
- Confirm a completely clean build on a known configuration and document the exact `west build --sysbuild` invocation.
- Ensure any example configurations contain dummy values, not real network or Matter identity values.
- Consider tagging `v0.1.0` only after the real sources are committed and a clean build is repeatable.

## Separation of concerns

This public repository owns the **XIAO device**: hardware, IMU, firmware, Matter data model and power optimization. The actual user's home automation entity IDs, notifications, private Thread topology and rules for deciding that a vibration means delivered mail stay in a **private infrastructure repository**.

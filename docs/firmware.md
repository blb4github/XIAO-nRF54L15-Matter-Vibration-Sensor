# Firmware design and Matter model

This describes the **tested implementation**. The original source tree on the development Mac remains authoritative; the full source is not yet published here.

## IMU hardware wake-up

The onboard **LSM6DS3TR-C** accelerometer operates at approximately **52 Hz** and produces a wake-up interrupt on INT1, connected to the nRF54L15 **P0.02**.

The implementation uses these IMU registers:

| Register | Address | Value | Usage |
|---|---:|---:|---|
| `TAP_CFG` | `0x58` | `0x81` | Interrupt/wake-up configuration |
| `WAKE_UP_THS` | `0x5B` | `0x01` | Wake-up threshold |
| `WAKE_UP_DUR` | `0x5C` | `0x00` | Wake-up duration |
| `MD1_CFG` | `0x5E` | `0x20` | Route wake-up to INT1 |
| `WAKE_UP_SRC` | `0x1B` | read | Detect/acknowledge wake-up source |

The GPIO interrupt schedules a **Zephyr work item**. That work confirms the IMU event and schedules a Matter state change on the **Matter platform thread** through `chip::DeviceLayer::PlatformMgr().ScheduleWork(...)`. Matter attribute writes are not performed in the GPIO interrupt context.

### Event state machine

- First confirmed vibration: set Boolean State active; start a **five-second** timeout.
- Subsequent vibration while active: **reschedule** the timeout for five seconds after the latest event; avoid redundant Matter ON reports.
- Five seconds without another detection: report Boolean State inactive.
- At **every confirmed IMU detection**, pulse the onboard green LED for **100 ms**. A second trigger while the pulse is active does not extend it.

The pulse is driven by a separate Zephyr `k_work_delayable` and does not block the IMU work thread.

## Matter clusters

The Matter contact-sensor-style endpoint is used deliberately as a **time-limited event signal**, not a real physical door/contact status.

| Endpoint | Server cluster | Function |
|---|---|---|
| **1** | **Boolean State** | `StateValue`: active during a vibration detection window |
| **1** | **Power Source** (`0x002F`) | `BatVoltage` and `BatPercentRemaining` |

The Power Source cluster enables the **Battery (BAT)** feature (`FeatureMap = 2`). `BatVoltage` is reported in **millivolts**. `BatPercentRemaining` is encoded in **half-percent units**: e.g. 160 means 80%.

The optional `BatTimeRemaining` attribute was removed because the device cannot compute meaningful remaining runtime. Battery charging status is not claimed to be measured.

### Battery percentage interpolation

The implemented percentage is an **indicative voltage lookup**, with linear interpolation between these points:

| Voltage | Approx. percentage |
|---:|---:|
| 3.30 V | 0% |
| 3.60 V | 10% |
| 3.70 V | 20% |
| 3.75 V | 30% |
| 3.80 V | 40% |
| 3.85 V | 50% |
| 3.90 V | 60% |
| 3.95 V | 70% |
| 4.00 V | 80% |
| 4.10 V | 90% |
| 4.20 V | 100% |

This is **not** a coulomb-counting fuel gauge. Load, temperature and charging affect the voltage-to-SoC relationship.

## Thread SED and Matter ICD

The successful build reported the following Zephyr settings:

```ini
CONFIG_CHIP_ENABLE_ICD_SUPPORT=y
CONFIG_OPENTHREAD_MTD_SED=y
CONFIG_CHIP_ICD_SLOW_POLL_INTERVAL=1000
```

The slow poll interval is in milliseconds. Functionality was tested after enabling SED/ICD. **No calibrated current measurement has yet verified MCU deep sleep or battery lifetime.** UART, logging, charger circuitry and peripheral states may dominate standby draw.

## Existing local source tree

| Path in working project | Responsibility |
|---|---|
| `src/app_task.cpp` | IMU GPIO interrupt, Zephyr work, Boolean State updates |
| `src/battery_monitor.cpp` | ADC measurements, Power Source updates, green LED pulse |
| `src/default_zap/template.zap` | Matter endpoint/cluster definitions |
| `src/default_zap/zap-generated/` | Generated Matter endpoint configuration |
| `app.overlay` | ADC channel 7, battery measurement power control |
| `prj.conf` | Zephyr, Matter and OpenThread settings |
| `pm_static.yml` | MCUboot / settings partition layout |
| `CMakeLists.txt` | Application files and cluster source inclusion |
| Custom board-root directory | XIAO board support; license and provenance require review |

The dedicated Matter Power Source server implementation must be linked; merely supplying an empty `MatterPowerSourcePluginServerInitCallback()` stub is **not** a substitute for the server.

The real source files and generated files should be added to GitHub from the tested development tree, not rebuilt from this document.

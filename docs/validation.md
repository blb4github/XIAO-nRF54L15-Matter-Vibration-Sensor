# Verification and remaining measurements

**Field validation: 2 November 2026.**

## Verified

| Test | Observation |
|---|---|
| Battery-only operation | Device works and continues to communicate over Matter without USB-C. |
| Battery ADC | Multimeter 3.976 V vs device ADC 3.964 V, difference 12 mV. |
| Matter Power Source | Measured 4.006 V and estimated 80% available to the Matter controller. |
| IMU hardware wake-up | Mechanical shock produces a Boolean State detection. |
| Retriggerable window | State returns to inactive five seconds after the last vibration. |
| Local LED | Green LED off at rest; 100 ms pulse on each confirmed detection. |
| Thread SED / ICD | Configuration enabled and device remained functionally operational. |
| Real mounting position | An A5 envelope was detected landing in the mailbox. |
| Separate deliveries | Two letters at different times produced two events. |
| Mailbox emptying | An external controller combining a separate door contact suppressed false delivery notifications. |
| Ordinary firmware updates | Matter fabric preserved across sector-erase flashing and cold starts. |

**Important distinction:** detection, Boolean State, battery and LED behavior are firmware functions. Deciding whether a vibration represents *new mail* requires application-level interpretation, in the example involving an **external door contact**. This Home Assistant automation is not part of the public sensor firmware.

## Not yet verified

1. **Measured average current** in sleep/idle with the radio and all circuitry active.
2. Whether the MCU enters deep sleep under the current logging and peripheral configuration.
3. Radio transmit and SED parent-poll energy cost under real conditions.
4. Battery lifetime under temperature and self-discharge conditions.
5. False positives and sensitivity with additional mounting substrates, small/light objects and mechanical handling.
6. Long-term Thread reliability with enclosure/door closed.
7. Battery charging safety in the installation environment, especially near/below 0 °C.

For orientation only, a nominal **2000-mAh** cell would theoretically provide **83 days at 1 mA** average or **167 days at 0.5 mA**. These are ideal capacity/current calculations, **not** measured device lifetime.

The voltage-to-percentage mapping is approximate and must not be confused with a measured state of charge.

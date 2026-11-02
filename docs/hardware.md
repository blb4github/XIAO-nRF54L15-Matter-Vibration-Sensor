# Hardware

## Bill of materials

| Component | Function |
|---|---|
| **Seeed Studio XIAO nRF54L15 Sense** | Low-power MCU, IEEE 802.15.4 / Thread, ADC, USB |
| **Onboard ST LSM6DS3TR-C** | Three-axis accelerometer with hardware wake-up interrupt |
| **EEMB LP103454 1S LiPo, 3.7 V / 2000 mAh** | Battery power |
| **Onboard battery charge and monitoring circuitry** | USB-C charging, switched battery measurement |
| **Onboard green user LED** | Brief local detection indication |

Before attaching a LiPo battery, **verify its JST connector polarity**. Provide strain relief and insulation, and protect the battery from mechanical damage, moisture and abnormal temperatures. Do not assume LiPo charging below 0 °C is safe without cell-specific approval and suitable charger protection.

## Signals used by the implementation

| Connection | MCU pin / peripheral | Behavior |
|---|---|---|
| IMU INT1 | **P0.02** | Hardware GPIO interrupt on vibration event |
| IMU control | Onboard I²C | Configure accelerometer, read and clear wake-up source |
| Battery divider supply enable | **P1.15** | Turn on switched voltage divider for measurement |
| Battery divider ADC | **P1.14 / AIN7** | Read battery voltage through the divider |
| Green user LED | **P2.00** | Active-low; normally off; 100 ms indication |
| Battery | BAT+ / BAT− | Single-cell LiPo |
| USB-C | USB | Programming, UART/debug, battery charging |

## Battery measurement

The existing XIAO battery monitor path uses a switched divider with a nominal 1:2 ratio:

1. Enable the measurement regulator on P1.15.
2. Allow approximately 10 ms for settling.
3. Sample ADC channel 7 at P1.14.
4. Convert to mV and multiply by two.
5. Disable the divider to avoid continuous leakage.
6. Repeat after boot and **once per hour**.

### Real measurements

| Measurement | Observed value |
|---|---:|
| Multimeter at connected battery | **3.976 V** |
| XIAO ADC | **3.964 V** |
| Difference | **12 mV (0.30%)** |

A later sample logged **4.006 V**, corresponding to approximately **80%** according to the simple built-in voltage lookup. Matter displayed about 4.01 V in the controller.

**Battery voltage during USB-C charging does not provide a reliable state-of-charge estimate.**

## Mechanical installation

The first prototype was mounted in a corner of a mailbox using removable poster adhesive, with the LiPo secured separately against the wall. A wooden mailbox floor transmits the vibration of an arriving envelope. An A5 envelope was successfully detected.

Mechanical mounting is important: accelerometer thresholds valid on a bench may behave differently on another surface. Verify the trigger response with the lid/door fully closed, and test for vibrations caused by opening, closing, traffic and nearby handling.

The enclosure and battery temperature range have not yet been fully characterized.

# Problem Analysis: ESP32 Brownout When Using LilyGO T-A76XX with Battery Power

## Core Issue

The LilyGO T-A76XX boards (including T-A7608 and T-A7670) experience brownout issues when toggling GPIO12, which is used to control power to the cellular modem. This issue occurs specifically when:

1. The board is powered by a battery
2. GPIO12 is set HIGH to power on the modem

When this happens, the voltage drops significantly (below 3.0V as shown in oscilloscope measurements), triggering the ESP32's brownout detector. The device then resets, and sometimes enters download mode despite nothing being connected to GPIO0.

## Technical Details

1. The issue appears related to the power management design, where GPIO12 controls a DC boost circuit or MOSFET that powers the cellular modem
2. GPIO12 is problematic because it's also a strapping pin for the ESP32
3. The voltage drop occurs due to the sudden current draw when powering the modem
4. The problem happens regardless of battery quality - even with fully charged batteries (4.0V)
5. Users attempted adding various capacitors (100μF, 220μF, 1000μF, 2200μF) to the 3V3 rail, but these didn't resolve the issue

## Timing of the Issue

Several users reported the brownout happens specifically when GPIO12 is set HIGH after the ESP32 has started. Setting it HIGH immediately at boot seems to be less problematic for some users.

## Possible Solutions

1. Using a 5V power source. The connection must be made with wires to the same contacts that the 18650 battery (3.7V) connects to. This solution completely eliminates the problem, but requires organizing a power source in the case where the LilyGO module will be placed.
2. Software-based solution (as implemented now). We allow for the possibility of an unexpected module reboot during modem initialization. Before initializing the modem, we save the state of the running program, and in case of a reboot, we return the program to the same state in which the reboot occurred.
3. Disabling the voltage control system (brownout). With the control system disabled, there were also no reboots during a week of testing. However, this is the least suitable solution of all, since the control system protects the module's flash memory from damage when the battery charge is low.

## Impact on Discharge Rate

This problem does not affect the rate of battery energy consumption.

## Problems of Excessive Energy Consumption When Working on the Previous Iteration of the Project Software

The code is problematic to read, understand the logic, and maintain, so we cannot say exactly why there was such energy consumption. It was faster to create a new solution on the ESP-IDF framework, which provides more functions to control the operation of the module and its components.
In other words, we:
- Reduced the processor's operating frequency
- The Bluetooth module uses exclusively Low Energy mode. We also disabled device pairing capabilities and all other unnecessary options of the Bluetooth module
- Before the device's deep sleep mode, we deinitialize all modules
- Logging process only in code debugging mode

## Two-Week Testing Results

As a result of the module running on the new software using an 18650 battery (3.7V), it can be stated that the battery is consumed extremely economically. In other words, the voltage on the battery decreased by 0.032V and at the time of the end of measurement was 4.058V. It should be noted that data transmission via the mobile network occurred 4 times a day.
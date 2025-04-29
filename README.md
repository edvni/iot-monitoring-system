# IoT environmental monitoring system
This IoT project is a remote environmental monitoring system that:
1. **Collects data** from multimple Bluetooth sensors
2. **Stores measurements** locally
3. **Periodically sends measurements** data to Firebase over GSM
4. **Manages battery life** through optimized deep sleep cycles
5. **Provides error recovery** through a state machine system

# Getting started (21.3.2025)
1. Clone repository
2. Navigate to project directory `cd <project-directory>`
5. Pull the latest changes `git pull origin main`
6. Clone esp-protocols: Navigate to `C:\Users\USER_NAME\esp\v5.4\esp-idf\components` and execute command: `git clone --recursive https://github.com/espressif/esp-protocols.git`
7. Make sure ESP-IDF v5.4.0 is selected
8. Set environment variables on your system to satisfy .vscode/settings.json paths. See below.
9. To build: In ESP-IDF terminal: `idf.py build`
10. Connect ESP32 device via USB
11. To flash and monitor: In ESP-IDF terminal: `idf.py flash monitor`

## For changes:
1. Create own branch for your changes (if needed) `git checkout -b my-feature-branch`
2. Make changes and commit them: `git add .` `git commit -m "Description of changes"`
3. Push branch to remote repository `git push my-feature-branch:target-branch`
4. Create pull request!!

## Set environment variables to satisfy settings.json
1. Open Windows Powershell/Git Bash etc.
2. Type command: `setx IDF_PATH "C:\Users<username>\esp\v5.4\esp-idf"`
3. Type command: `setx IDF_TOOLS_PATH "C:\Users<username>.espressif"`
4. Type command: `setx IDF_PYTHON_PATH "C:\Users<username>.espressif\tools\idf-python\3.11.2\python.exe"`

# Project Overview

### Hardware

- **Development board**<br>
Lilygo T-A7670E Wireless module ESP32 Chip 4G LTE CAT1 MCU32<br>
![lilygo chip](images/hardware/lilygo_t-a7670e.png)

- **Sensor**<br>
RuuviTag Pro Bluetooth Sensor<br>
![ruuvitag sensor](images/hardware/ruuvitagpro_exploded.png)


### Main Components

- **Main Logic** (`main`): Central logic that manages the devices workflow.
- **Storage** (`storage`): Handles persistent storage operations for sensor data and log messages.
- **Sensors** (`sensors`): Manages Bluetooth sensors, their initialization, scanning, and data collection.
- **GSM Modem** (`gsm_modem`): Controls the GSM modem for cellular network connectivity.
- **Discord API** (`discord_api`): Provides integration with Discord for sending notifications and logs.
- **Firebase API** (`firebase_api`): Handles communication with Firebase for data storage retrieval.
- **Power Management** (`power_management`): Configures power management settings for the ESP32.
- **JSON Helper** (`json_helper`): Converts sensor data into JSON format for storage and transmission.
- **Reporter** (`reporter`): Used in logs reporting, including battery status.
- **System States** (`system_states`): Defines and manages the system state machine for recovery and normal operations.
- **Time Manager** (`time_manager`): Manages system time, synchronization, and timezone settings.

### Workflow

- **Initialization**: The system initializes power management, storage, sensors and the GSM modem.
- **Data collection**: The RuuviTag sensor data is collected via BLE and stored in SPIFFS.
- **Data transmission**: After a certain number of boot cycles (`SEND_DATA_CYCLE`), the accumulated data is sent to Firebase via the GSM module.
- **Error handling**: If an error occurs (e.g., GSM initialization fails), the system logs the error and restarts.
- **Sleep mode**: The ESP32 enters deep sleep mode after each cycle to save power.

# Working logic

1. **Boot process**:
- The system checks the boot count and system state from NVS.
- If it's the first boot, it initializes the GSM modem and sends a startup message to Discord
- If it's a normal boot, it proceeds with data collection and transmission

2. **Data collection**:
- The RuuviTag sensor data is collected via BLE and stored in a JSON file in SPIFFS
- The system waits up to 10 seconds to receive sensor data.

3. **Data transmission**:
- After `SEND_DATA_CYCLE` boot cycles, the system sends the accumulated data to Firebase Cloud Firestore.
- If the transmission fails, it retries up to 3 times.

4. **Sleep mode**:
- After completing the cycle, the ESP32 enter deep sleep for `TIME_TO_SLEEP` seconds.

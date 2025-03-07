# Project Overview

### Main Components

- **Main Logic** (`main.c`): Handles the overall workflow, including sensor data collection, storage and transmission.
- **Storage** (`storage.c`): Manages data storage using SPIFFS (SPI Flash File System) and NVS (Non-Volatile Storage) for boot count, error flags, and system state.
- **Sensors** (`sensor.c`): Interfaces with a RuuviTag sensor via BLE (Bluetooth Low Energy) to collect temperature and humidity data.
- **GSM Modem** (`gsm_modem.cpp`)
- **Discord API** (`discord_api.cpp`): Handles HTTP requests to send messages to a Discord channel.
- **Power Management** (`power_management.c`): Configures power management settings for the ESP32.
- **JSON Helper** (`json_helper.c`): Converts sensor data into JSON format for storage and transmission.

### Workflow

- **Initialization**: The system initializes power management, storage, sensors and the GSM modem.
- **Data collection**: The RuuviTag sensor data is collected via BLE and stored in SPIFFS.
- **Data transmission**: After a certain number of boot cycles (`SEND_DATA_CYCLE`), the accumulated data is sent to Discord via the GSM module.
- **Error handling**: If an error occurs (e.g., GSM initialization fails), the system logs the error and restarts.
- **Sleep mode**: The ESP32 enters deep sleep mode after each cycle to save power.

# Working logic

1. **Boot process**:

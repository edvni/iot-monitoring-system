#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

// Global logging definitions
#define DISCORD_LOGGING true
#define SYSTEM_LOGGING false

#define CONFIG_NIMBLE_CPP_LOG_LEVEL 0
#define SECONDS_IN_MICROS 1000000ULL
#define SEND_DATA_CYCLE 144  // For testing 3
#define TRIGGER_INTERVAL    (600 * SECONDS_IN_MICROS) // defined in seconds (600 seconds)
#define COMPENSATION_INTERVAL    (4.16666 * SECONDS_IN_MICROS)

#endif /* CONFIG_MANAGER_H */ 
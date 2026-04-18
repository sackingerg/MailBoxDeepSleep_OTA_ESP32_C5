#pragma once
#include <Arduino.h>

// ===============================
// Debug
// ===============================
#define DEBUG_MODE false

// ===============================
// ESP32-C6 pins (XIAO ESP32-C6 defaults)
// ===============================
#define TRIGGER_PIN        2
#define EXTERNAL_LED_PIN   16
#define ONBOARD_LED_PIN    15     // Yellow user LED "L" on XIAO ESP32-C6 (active-low)

// ===============================
// Session timing (user-configurable in seconds)
// ===============================
#define SESSION_DURATION_MIN   120UL           // Total flashing session length

#define BLINK_ON_SECONDS       1.0f            // LED on duration per flash (supports fractions)
#define BLINK_CYCLE_SECONDS    14.0f           // Full cycle time (start-to-start)

#define FLASH_ON_MS            (uint32_t)(BLINK_ON_SECONDS * 1000UL)
#define BLINK_INTERVAL_MS      (uint32_t)(BLINK_CYCLE_SECONDS * 1000UL)

// Debounce for mailbox close detection
#define MAILBOX_DEBOUNCE_MS    300UL

// If timer wakes while open, skip flash & count increment
#define PAUSE_FLASH_IF_OPEN_ON_TIMER_WAKE  1

// ===============================
// PWM Brightness Control for EXTERNAL_LED_PIN (MOSFET drive)
// ===============================
#define PWM_ENABLE          true          // Set false to use digital on/off (old behavior)
#define PWM_FREQUENCY       5000          // Hz (1-20000 recommended for MOSFET)
#define PWM_RESOLUTION      8             // Bits (8 = 0-255 duty cycle)
#define PWM_BRIGHTNESS      180           // 0-255 (180 ≈ 70% brightness, adjust to taste)

// ===============================
// OTA boot window (cold boot only)
// ===============================
#define BOOT_OTA_WINDOW_MS     (60UL * 1000UL)

#define OTA_AP_SSID            "Mailbox-OTA"
#define OTA_AP_PASSWORD        "Mailbox123"
#define OTA_HTTP_PORT          80
#define OTA_EXPECTED_BIN_NAME  "MailBoxDeepSleep_OTA_ESP32_C6.ino.bin"

#define FW_VERSION_STRING      "MailboxDeepSleep + OTA Upload (ESP32-C6) + PWM"
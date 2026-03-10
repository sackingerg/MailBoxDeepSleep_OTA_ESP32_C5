#pragma once
#include <Arduino.h>

// ===============================
// Debug
// ===============================
#define DEBUG_MODE false

// ===============================
// ESP32-C5 pins (XIAO ESP32-C5 defaults)
// ===============================
//
#define TRIGGER_PIN        1
#define EXTERNAL_LED_PIN   10
#define ONBOARD_LED_PIN    27 // Yellow user LED "L" on XIAO ESP32-C5 (LED_BUILTIN)

// ===============================
// Session timing (now user-configurable in seconds)
// ===============================
#define SESSION_DURATION_MIN   120UL           // Total flashing session length

#define BLINK_ON_SECONDS       1.0f           // ← Change this: LED on duration per flash - one decimal
#define BLINK_CYCLE_SECONDS    14.0f            // ← Change this: full cycle time (start-to-start)

#define FLASH_ON_MS            (uint32_t)(BLINK_ON_SECONDS * 1000UL)
#define BLINK_INTERVAL_MS      (uint32_t)(BLINK_CYCLE_SECONDS * 1000UL)

// Debounce for mailbox close detection
#define MAILBOX_DEBOUNCE_MS    300UL

// If timer wakes while open, skip flash & count increment
#define PAUSE_FLASH_IF_OPEN_ON_TIMER_WAKE  1

// ===============================
// OTA boot window (cold boot only)
// ===============================
#define BOOT_OTA_WINDOW_MS     (60UL * 1000UL)

#define OTA_AP_SSID            "Mailbox-OTA"
#define OTA_AP_PASSWORD        "Mailbox123"
#define OTA_HTTP_PORT          80
#define OTA_EXPECTED_BIN_NAME  "MailBoxDeepSleep_OTA_ESP32_C5.ino.bin"

#define FW_VERSION_STRING      "MailboxDeepSleep + OTA Upload (ESP32-C5)"
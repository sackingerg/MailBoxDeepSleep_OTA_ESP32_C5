#include <Arduino.h>
#include "esp_sleep.h"

#include "config.h"
#include "debug.h"
#include "OTA.h"

// RTC persisted session state
RTC_DATA_ATTR uint32_t gFlashCount = 0;
RTC_DATA_ATTR bool     gSessionActive = false;
RTC_DATA_ATTR uint32_t gTotalFlashes = 0;

OTAPortal gPortal;

/**
 * Calculate total session duration in milliseconds
 */
static uint32_t sessionDurationMs() {
  return SESSION_DURATION_MIN * 60UL * 1000UL;
}

/**
 * Check if mailbox switch is open (HIGH = open)
 */
static bool isMailboxOpen() {
  return digitalRead(TRIGGER_PIN) == HIGH;
}

/**
 * Set brightness on external LED (PWM) and mirror state on onboard LED
 * @param brightness 0-255 duty cycle (or 0/1 for digital mode)
 */
static void setBothLEDs(uint8_t brightness) {
  if (PWM_ENABLE) {
    ledcWrite(EXTERNAL_LED_PIN, brightness);           // PWM on external LED
  } else {
    digitalWrite(EXTERNAL_LED_PIN, brightness ? HIGH : LOW);
  }

  // Onboard yellow LED is active-low
  if (ONBOARD_LED_PIN >= 0) {
    digitalWrite(ONBOARD_LED_PIN, brightness ? LOW : HIGH);
  }
}

/**
 * Wait for mailbox to close with debounce
 */
static void waitForMailboxClose() {
  DBG_PRINTLN("Waiting for mailbox to CLOSE...");

  while (isMailboxOpen()) delay(10);

  uint32_t start = millis();
  while (millis() - start < MAILBOX_DEBOUNCE_MS) {
    if (isMailboxOpen()) start = millis();
    delay(5);
  }

  DBG_PRINTLN("Mailbox CLOSE confirmed (debounced).");
  DBG_FLUSH();
}

/**
 * Double-flash ACK to confirm mailbox closed
 */
static void ackDoubleFlash() {
  DBG_PRINTLN("Double flash ACK");
  for (int i = 0; i < 2; i++) {
    setBothLEDs(PWM_ENABLE ? PWM_BRIGHTNESS : 255);
    delay(1000);
    setBothLEDs(0);
    delay(1000);
  }
}

/**
 * Perform one flash cycle using PWM (or digital)
 */
static void doFlash() {
  DBG_PRINT("Flash #"); DBG_PRINT(gFlashCount);
  DBG_PRINT(" of ");    DBG_PRINTLN(gTotalFlashes);

  setBothLEDs(PWM_ENABLE ? PWM_BRIGHTNESS : 255);
  delay(FLASH_ON_MS);
  setBothLEDs(0);

  DBG_PRINTLN("Flash done.");
  DBG_FLUSH();
}

/**
 * Prepare for deep sleep with timer + GPIO wakeup
 */
static void deepSleepUntilNextFlash(uint32_t intervalMs) {
  setBothLEDs(0);  // Ensure LEDs are off before sleep

  DBG_PRINT("Deep sleep for "); DBG_PRINT(intervalMs / 1000);
  DBG_PRINTLN("s or GPIO(mailbox HIGH) or timer.");

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  uint64_t pinMask = 1ULL << TRIGGER_PIN;
  esp_deep_sleep_enable_gpio_wakeup(pinMask, ESP_GPIO_WAKEUP_GPIO_HIGH);

  esp_sleep_enable_timer_wakeup((uint64_t)intervalMs * 1000ULL);

  DBG_FLUSH();
  esp_deep_sleep_start();
}

/**
 * Print wakeup reason for debugging
 */
static void print_wakeup_reason() {
#if DEBUG_MODE
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  DBG_PRINT("Wakeup cause: ");
  switch (cause) {
    case ESP_SLEEP_WAKEUP_UNDEFINED:    DBG_PRINTLN("Power-on / reset / undefined"); break;
    case ESP_SLEEP_WAKEUP_GPIO:         DBG_PRINTLN("GPIO (deep sleep trigger)"); break;
    case ESP_SLEEP_WAKEUP_TIMER:        DBG_PRINTLN("Timer"); break;
    default:                            DBG_PRINT("Other: "); DBG_PRINTLN(cause); break;
  }
  DBG_FLUSH();
#endif
}

/**
 * Run OTA portal on cold boot (extended if client connected)
 */
static void runOtaBootWindow() {
  gPortal.begin();

  const unsigned long start = millis();
  while (true) {
    gPortal.handleClient();
    if (gPortal.quitRequested()) break;

    if ((millis() - start > BOOT_OTA_WINDOW_MS) && (WiFi.softAPgetStationNum() == 0)) {
      break;
    }
    delay(5);
  }

  gPortal.end();
}

void setup() {
#if DEBUG_MODE
  Serial.begin(115200);
  ensureSerialReady();
  DBG_PRINTLN("");
  DBG_PRINTLN("=== MailboxDeepSleep OTA Upload (ESP32-C6) ===");
  DBG_PRINT("FW: "); DBG_PRINTLN(FW_VERSION_STRING);
#endif

  // === PWM setup for external LED (MOSFET) ===
  if (PWM_ENABLE) {
    ledcAttach(EXTERNAL_LED_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
  } else {
    pinMode(EXTERNAL_LED_PIN, OUTPUT);
  }

  setBothLEDs(0);  // Ensure both LEDs off at start

  if (ONBOARD_LED_PIN >= 0) {
    pinMode(ONBOARD_LED_PIN, OUTPUT);
    digitalWrite(ONBOARD_LED_PIN, HIGH);  // active-low = off
  }

  pinMode(TRIGGER_PIN, INPUT_PULLDOWN);

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  print_wakeup_reason();

  // Cold boot: OTA portal
  if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    DBG_PRINTLN("Cold boot -> OTA window.");
    runOtaBootWindow();

    DBG_PRINTLN("OTA window ended -> idle deep sleep.");
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

    uint64_t pinMask = 1ULL << TRIGGER_PIN;
    esp_deep_sleep_enable_gpio_wakeup(pinMask, ESP_GPIO_WAKEUP_GPIO_HIGH);

    DBG_FLUSH();
    esp_deep_sleep_start();
  }

  // ... (rest of setup() is unchanged from your working version)
  // Mailbox open, timer wake, fallback paths remain exactly the same
  // (full code continues below for completeness)

  if (cause == ESP_SLEEP_WAKEUP_GPIO) {
    if (!gSessionActive) {
      DBG_PRINTLN("GPIO wakeup: mailbox OPEN from idle.");
      waitForMailboxClose();
      ackDoubleFlash();

      gSessionActive = true;
      gFlashCount = 1;
      gTotalFlashes = sessionDurationMs() / BLINK_INTERVAL_MS;

      DBG_PRINT("Session start, total flashes: ");
      DBG_PRINTLN(gTotalFlashes);

      deepSleepUntilNextFlash(BLINK_INTERVAL_MS);
    } else {
      DBG_PRINTLN("GPIO wakeup: mailbox OPEN during active session -> pause.");
      setBothLEDs(0);
      waitForMailboxClose();
      ackDoubleFlash();

      DBG_PRINT("Resume at flash #");
      DBG_PRINT(gFlashCount);
      DBG_PRINT(" of ");
      DBG_PRINTLN(gTotalFlashes);

      deepSleepUntilNextFlash(BLINK_INTERVAL_MS);
    }
  }

  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    if (!gSessionActive) {
      DBG_PRINTLN("TIMER wake but no session -> idle.");
      setBothLEDs(0);
      waitForOutputLow(EXTERNAL_LED_PIN, 100);

      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
      uint64_t pinMask = 1ULL << TRIGGER_PIN;
      esp_deep_sleep_enable_gpio_wakeup(pinMask, ESP_GPIO_WAKEUP_GPIO_HIGH);
      DBG_FLUSH();
      esp_deep_sleep_start();
    }

#if PAUSE_FLASH_IF_OPEN_ON_TIMER_WAKE
    if (isMailboxOpen()) {
      DBG_PRINTLN("TIMER wake but mailbox OPEN -> pause (no flash, no count).");
      setBothLEDs(0);
      waitForMailboxClose();
      ackDoubleFlash();
      deepSleepUntilNextFlash(BLINK_INTERVAL_MS);
    }
#endif

    doFlash();
    gFlashCount++;

    if (gFlashCount > gTotalFlashes) {
      DBG_PRINTLN("Session complete -> idle deep sleep.");
      setBothLEDs(0);
      waitForOutputLow(EXTERNAL_LED_PIN, 100);

      gSessionActive = false;
      gFlashCount = 0;

      esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
      uint64_t pinMask = 1ULL << TRIGGER_PIN;
      esp_deep_sleep_enable_gpio_wakeup(pinMask, ESP_GPIO_WAKEUP_GPIO_HIGH);
      DBG_FLUSH();
      esp_deep_sleep_start();
    }

    deepSleepUntilNextFlash(BLINK_INTERVAL_MS);
  }

  // Fallback
  DBG_PRINTLN("Unexpected wake cause path -> idle deep sleep.");
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  uint64_t pinMask = 1ULL << TRIGGER_PIN;
  esp_deep_sleep_enable_gpio_wakeup(pinMask, ESP_GPIO_WAKEUP_GPIO_HIGH);
  DBG_FLUSH();
  esp_deep_sleep_start();
}

void loop() {
  // never used
}
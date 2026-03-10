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

static uint32_t sessionDurationMs() {
  return SESSION_DURATION_MIN * 60UL * 1000UL;
}

static bool isMailboxOpen() {
  return digitalRead(TRIGGER_PIN) == HIGH;
}

// Helper to set BOTH LEDs at once (onboard mirrors external)
static void setBothLEDs(uint8_t externalState) {
  // external LED: HIGH = on (active-high)
  digitalWrite(EXTERNAL_LED_PIN, externalState);

  // onboard yellow LED: LOW = on (active-low)
  if (ONBOARD_LED_PIN >= 0) {
    digitalWrite(ONBOARD_LED_PIN, !externalState);  // invert logic
  }
}
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
//
static void ackDoubleFlash() {
  DBG_PRINTLN("Double flash ACK");
  for (int i = 0; i < 2; i++) {
    setBothLEDs(HIGH); delay(1000);
    setBothLEDs(LOW);  delay(1000);
  }
}

static void doFlash() {
  DBG_PRINT("Flash #"); DBG_PRINT(gFlashCount);
  DBG_PRINT(" of ");    DBG_PRINTLN(gTotalFlashes);

  setBothLEDs(HIGH);
  delay(FLASH_ON_MS);
  setBothLEDs(LOW);

  DBG_PRINTLN("Flash done.");
  DBG_FLUSH();
}

static void deepSleepUntilNextFlash(uint32_t intervalMs) {
  setBothLEDs(LOW);  // Ensure off before sleep

  DBG_PRINT("Deep sleep for "); DBG_PRINT(intervalMs / 1000);
  DBG_PRINTLN("s or GPIO(mailbox HIGH) or timer.");

  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  // Use modern GPIO wakeup for ESP32-C5 (any pin, level trigger)
  uint64_t pinMask = 1ULL << TRIGGER_PIN;
  esp_deep_sleep_enable_gpio_wakeup(pinMask, ESP_GPIO_WAKEUP_GPIO_HIGH);

  esp_sleep_enable_timer_wakeup((uint64_t)intervalMs * 1000ULL);

  DBG_FLUSH();
  esp_deep_sleep_start();
}

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

static void runOtaBootWindow() {
  gPortal.begin();

  const unsigned long start = millis();
  while (true) {  // Run indefinitely until quit or update success (which restarts)
    gPortal.handleClient();

    if (gPortal.quitRequested()) break;

    // Timeout only if no clients are connected (extends time if someone is using the portal)
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
  DBG_PRINTLN("=== MailboxDeepSleep OTA Upload (ESP32-C5) ===");
  DBG_PRINT("FW: "); DBG_PRINTLN(FW_VERSION_STRING);
#endif

  pinMode(EXTERNAL_LED_PIN, OUTPUT);
  setBothLEDs(LOW);  // Ensure both off at start

  if (ONBOARD_LED_PIN >= 0) {
    pinMode(ONBOARD_LED_PIN, OUTPUT);
    digitalWrite(ONBOARD_LED_PIN, HIGH);
  }

  pinMode(TRIGGER_PIN, INPUT_PULLDOWN);

  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  print_wakeup_reason();  // Log reason on every boot/wake

  // Cold boot: offer OTA portal (now extended if connected), then go idle deep sleep
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

  // Mailbox open (GPIO wakeup)
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
      digitalWrite(EXTERNAL_LED_PIN, LOW);

      waitForMailboxClose();
      ackDoubleFlash();

      DBG_PRINT("Resume at flash #");
      DBG_PRINT(gFlashCount);
      DBG_PRINT(" of ");
      DBG_PRINTLN(gTotalFlashes);

      deepSleepUntilNextFlash(BLINK_INTERVAL_MS);
    }
  }

  // Timer wake for scheduled flash
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    if (!gSessionActive) {
      DBG_PRINTLN("TIMER wake but no session -> idle.");
      digitalWrite(EXTERNAL_LED_PIN, LOW);
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
      digitalWrite(EXTERNAL_LED_PIN, LOW);
      waitForMailboxClose();
      ackDoubleFlash();
      deepSleepUntilNextFlash(BLINK_INTERVAL_MS);
    }
#endif

    doFlash();
    gFlashCount++;

    if (gFlashCount > gTotalFlashes) {
      DBG_PRINTLN("Session complete -> idle deep sleep.");
      digitalWrite(EXTERNAL_LED_PIN, LOW);
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

  // Fallback: go idle
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
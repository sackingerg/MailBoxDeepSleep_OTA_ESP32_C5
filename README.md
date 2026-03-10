Mailbox Notifier Flasher with Seeed Studio XIAO ESP32-C5

This project implements an ultra-low-power mailbox notifier/flasher using the Seeed Studio XIAO ESP32-C5 board.

The device remains in deep sleep most of the time (consuming only ~8–20 µA), wakes up on a GPIO trigger (mailbox door open) or timer, flashes an external LED (and mirrors it on the onboard yellow LED), and tracks a 2-hour flashing session. It includes a temporary Wi-Fi AP-based OTA update portal on cold boot for easy firmware updates without USB.

Features



Deep sleep with timer + GPIO wakeup (mailbox open detection)

Configurable flash ON time and cycle interval (in seconds)

Double-flash acknowledgment when mailbox opens/closes

Session persists across deep sleep using RTC memory

OTA firmware upload via browser (temporary AP on boot)

Both external LED (GPIO10) and onboard yellow LED (GPIO27, active-low) flash in sync

Debounced mailbox close detection

Debug logging (optional, disable for lower power)



Hardware Requirements



Board: Seeed Studio XIAO ESP32C5 (RISC-V, dual-band Wi-Fi 6 + BLE/Zigbee/Thread, 8 MB PSRAM, 384 KB SRAM)

External LED: Connected to GPIO10 (D10) with current-limiting resistor (e.g., 220–330 Ω to GND)

Mailbox trigger switch: Normally open switch between 3.3V and TRIGGER\_PIN (GPIO1 / D0 in your config) — pull-down enabled internally

Power: USB-C or 3.7V LiPo battery (via BAT+ / BAT- pads)

Optional: 10–100 nF capacitor across trigger pin for noise reduction



Pin Mapping (Your Config)



TRIGGER\_PIN: GPIO1 (D0) – Mailbox open = HIGH (switch to 3.3V)

EXTERNAL\_LED\_PIN: GPIO10 (D10) – Active-high LED

ONBOARD\_LED\_PIN: GPIO27 – Yellow "L" user LED (active-low: LOW = lit)



Circuit Diagram (Text Description)

text3.3V ───┬──────────────┬────────────── Switch (mailbox open → closed)

&nbsp;       │              │

&nbsp;       │         10–100 nF (optional noise filter)

&nbsp;       │              │

TRIGGER\_PIN (GPIO1) ───┴────────────── GND (via internal pull-down)



EXTERNAL\_LED\_PIN (GPIO10) ─── 220Ω Resistor ─── LED (+) ─── LED (-) ─── GND



Onboard LED (GPIO27): Internal to board, no external wiring needed



Switch: Wire one side to 3.3V, other to TRIGGER\_PIN (pull-down makes open = LOW, closed/open mailbox = HIGH depending on logic).

LED: Anode to GPIO10 via resistor, cathode to GND.



Software / Code Overview



Framework: Arduino-ESP32 core (v3.0+ recommended)

Main file: MailBoxDeepSleep\_OTA\_ESP32\_C5.ino

Helpers:

config.h: Pins, timings (SESSION\_DURATION\_MIN, BLINK\_ON\_SECONDS, BLINK\_CYCLE\_SECONDS), OTA settings

debug.h: DBG\_PRINT\* macros + helpers (ensureSerialReady, waitForOutputLow)

OTA.h: OTAPortal class (Wi-Fi AP + web server for .bin upload using Update.h)





Key Operation Flow



Cold boot (power-on or reset):

→ Starts OTA portal (AP: "Mailbox-OTA") for ~60 seconds (extended if client connected).

→ After timeout or quit/upload → enters idle deep sleep waiting for TRIGGER\_PIN HIGH.

Mailbox open (TRIGGER\_PIN → HIGH):

→ Wakes from deep sleep (GPIO wakeup).

→ Debounces close → double-flash ACK (both LEDs).

→ Starts 2-hour session (gSessionActive = true, calculates total flashes).

→ Sleeps until next cycle.

Timer wake (every BLINK\_CYCLE\_SECONDS):

→ If session active and mailbox closed → flash both LEDs for BLINK\_ON\_SECONDS.

→ Increment counter → if done → end session, idle sleep.

→ Else → sleep again.

Deep sleep: Wi-Fi off, radio powered down, ~10–20 µA current.



Installation \& Usage



Install Arduino IDE + esp32 board package (Boards Manager → "esp32" by Espressif ≥3.0).

Select board: Seeed XIAO ESP32C5 (or equivalent).

Set PSRAM: Enabled (Tools menu) if using later.

Upload via USB first.

For updates: Cold boot → join "Mailbox-OTA" Wi-Fi → http://192.168.4.1/ → upload new .ino.bin.



Exported .bin location: After Sketch → Export compiled Binary → in sketch folder (or temp build folder if issues).

Power Consumption



Deep sleep: ~8–20 µA (Wi-Fi/radio off, typical ESP32-C5 measurement with GPIO/timer wakeup).

Awake/flash: Brief ~50–100 mA spikes (LEDs on).

Average: <20 µA possible with sparse flashing → months/years on small LiPo/CR2032.



Sources \& References



Seeed Studio XIAO ESP32C5 Wiki: https://wiki.seeedstudio.com/xiao\_esp32c5\_getting\_started/

ESP-IDF Sleep Modes: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/system/sleep\_modes.html

Arduino-ESP32 OTA examples (WebServer + Update.h)

GPIO wakeup API for C-series chips (esp\_deep\_sleep\_enable\_gpio\_wakeup)



Future Improvements



Add low-battery monitoring (via GPIO6 BAT\_VOLT\_PIN).

Event logging to flash or PSRAM.

Configurable via OTA web form (not just upload).

Zigbee/Thread notification integration.


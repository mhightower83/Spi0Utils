/*
  Goal
  With NodeMCU module, ESP-12F, or similar operating in Flash DIO mode,
  enable the use of GPIO9 and GPIO10 as outputs or inputs without crashing.

  At power-on reset the ESP8266 may drive these pins. At reset, signals
  presented at these pins may affect the Flash Chip's ability to operate
  correctly. Some vendor parts may work others may not.

  Expect some wiggling of these GPIO pins at boot. GPIO15 has a pull down
  resistor and must be low at boot. This makes it a good pin to use for gateing
  the GPIO noise output pins 0, 1, 2, 3, 9, 10, and 16 and others that wiggle at
  boot.

  When driven, to avoid virtual shorts, GPIO pins should be driven through a
  current limiting resistor.

  In the case if the EON EN25Q32C (also A and B) Flash Chip, the HOLD# function
  is not connected. When not in QIO operations the DQ3 pin is ignored.

  DQ2/WP#   uses GPIO9
  DQ3/HOLD# uses GPIO10

  Keystroke setup for Flash
  * EON - at least the ones I have
  * Winbond
  * others similar to Winbond

files:
  * Reclaim_GPIO_9_10.ino*        // 'void setup() {...}', 'void loop() {...}'
  * utility_reclaim_gpio_9_10.ino // Makes it run and supports sharing 'preinit();'
  * ProcessKey.ino                // Hot keys to demo stuff
  * kStatusRegisterBitDef.h       // Some Status Registry bit definitions
  * FlashChipId.ino               // Try and report the Flash vendor name.
  * FlashChipId_D8.h              // Identify an obfuscated Flash memory
  * SFDP.*                        // dumps the SFDP table
  * SpiFlashUtil.*                // library
  * UartDownload.*                // Not required - just covenant for development

TODOs
Turn on WiFi and let the System update the System parameter sectors.
Maybe RF Calibration is enough.
Maybe a Flash write after the LEDs update

*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <BootROM_NONOS.h>
#include <BacktraceLog.h>
#include <SpiFlashUtils.h>
#include "kStatusRegisterBitDef.h"
#include "FlashChipId_D8.h"

#define ETS_PRINTF ets_uart_printf
#define NOINLINE __attribute__((noinline))

// #define DEBUG_FLASH_QE
// #define RECLAIM_GPIO_EARLY

////////////////////////////////////////////////////////////////////////////////
// Debug MACROS
#if defined(RECLAIM_GPIO_EARLY) && defined(DEBUG_FLASH_QE)
// Printing before "C++" runtime has initialized. Use lower level print functions.
#define DBG_PRINTF ets_uart_printf

#elif defined(DEBUG_FLASH_QE)
#define DBG_PRINTF(a, ...) Serial.printf_P(PSTR(a), ##__VA_ARGS__)

#else
#define DBG_PRINTF(...) do {} while (false)
#endif


extern bool gpio_9_10_available;
bool copy_gpio9_to_16 = false;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.printf_P(PSTR(
    "\r\n\r\n"
    "Demo: Use with Flash Mode DIO, reclaim GPIO pins 9 and 10.\r\n"
  ));
  printFlashChipID(Serial);

#if 0 //ndef RECLAIM_GPIO_EARLY
  gpio_9_10_available = reclaim_GPIO_9_10();
  if (gpio_9_10_available) {
    pinMode(9, INPUT);
    pinMode(10, INPUT);
  }
#endif
  // enableWiFiAtBootTime();
  pinMode(LED_BUILTIN, OUTPUT); // Arduino IDE, Tools->Builtin LED: "2"
  digitalWrite(LED_BUILTIN, HIGH);

  uint32_t status = 0;
  if (0 == spi0_flash_read_status_registers_3B(&status)) {
    ets_uart_printf("  spi0_flash_read_status_registers_3B(0x%06X)\n", status);
  } else {
    ets_uart_printf("  spi0_flash_read_status_registers_3B() failed!\n");
  }

  /*
    Thoughts need reorganizing.
  */
  if (is_spi0_quad()) {
    Serial.println("\n\n"
      "The ESP8266 SPI0 controller is configured for QUAD operations. In this mode,\n"
      "GPIO pins 9 and 10 are used for data pins. They cannot be reclaimed for GPIO\n"
      "use. Rebuild with SPI Flash Mode: \"DIO\".\n"
    );
  } else {
#if TEST_GPIO_INPUT
    if (is_QE()) {
      // This is handy for using jumpers on a plug board type testing.
  #if 1
      pinMode( 9, INPUT);
      pinMode(10, INPUT);
      Serial.println("\n\n"
        "For the NodeMCU, connect GPIO9(SD2) with a 1K resistor to either ground or\n"
        "the 3.3 Volt pin. Swap back and forth. Observe the red LED turn off and on\n"
        "with the resistor change. The resistor can be any value up to 12K\n"
        "Note, we are using the NodeMCU's red LED connected to GPIO16; not the\n"
        "blue LED at GPIO2 on the ESP-12 module.\n"
      );
  #else
      pinMode( 9, INPUT_PULLUP);
      pinMode(10, INPUT_PULLUP);
      // Effective Internal pullup resistance can be as low as 33K
      Serial.println("\n\n"
        "For the NodeMCU, connect GPIO9(SD2) with a 1K resistor to ground.\n"
        "Connect and disconnect while observing the red LED turn off and on\n"
        "with the resistor change.\n"
        "Note, we are using the NodeMCU's red LED connected to GPIO16; not the\n"
        "blue LED at GPIO2 on the ESP-12 module.\n"
      );
      // Add notes about selecting resistors to leave a noise resistance logic level.
  #endif
      pinMode(16, OUTPUT);
      digitalWrite(16, LOW);
      copy_gpio9_to_16 = true;
    }
#endif
  }
#if TEST_GPIO_OUTPUT
  if (gpio_9_10_available) {
    pinMode(9, OUTPUT);
    pinMode(10, OUTPUT);
    digitalWrite(9, HIGH);
    digitalWrite(10, HIGH);
  }

  if (gpio_9_10_available) {
    Serial.printf_P(PSTR(
      "Perform 1 second rotation with LOW on GPIO pins 9, 10, and %u\r\n\r\n"),
      LED_BUILTIN);
  } else {
    // If the Flash Chip has to use DOUT to work, then it may not
    // have the QE status register bit to support this example.
    Serial.printf_P(PSTR(
      "Reclaiming of GPIO pins 9 and 10 failed\r\n"
      "Check 'Flash Mode' used for build; it must be 'DIO'.\r\n"
    ));
  }
#endif
}

constexpr uint32_t kInterval = 1000u;

void loop() {
  // Copy GPIO9 to NodeMCU LED at GPIO16
  if (copy_gpio9_to_16) digitalWrite(16, digitalRead(9));

  // Need a blinking LEDs to show we are running
  static uint32_t lastInterval = millis();
  static uint32_t toggle = 0;

  uint32_t current = millis();
  if ((current - lastInterval) > kInterval) {
    lastInterval = current;
    toggle++;
    digitalWrite(LED_BUILTIN, (0 == (toggle % 3)) ? LOW : HIGH);
#if TEST_GPIO_OUTPUT
    if (gpio_9_10_available) {
      digitalWrite( 9, (1 == (toggle % 3)) ? LOW : HIGH);
      digitalWrite(10, (2 == (toggle % 3)) ? LOW : HIGH);
    }
#endif
  }

  serialClientLoop();
}


////////////////////////////////////////////////////////////////////////////////
// Shared preinit()
bool gpio_9_10_available __attribute__((section(".noinit")));

// If early initialization of GPIO pins is not needed, `reclaim_GPIO_9_10()`
// can be called from `setup()`
void preinit(void) {
  SHARE_PREINIT__DEBUG_ESP_BACKTRACELOG();

#ifdef RECLAIM_GPIO_EARLY
  gpio_9_10_available = reclaim_GPIO_9_10();
  if (gpio_9_10_available) {
    // Place GPIO pin initialization as needed
    pinMode(9, INPUT);
    pinMode(10, INPUT);
  }
#else
  // must init noinit variable
  gpio_9_10_available = false;
  // reclaim_GPIO_9_10() will be called and checked from the Sketch's setup() function.
#endif
}
////////////////////////////////////////////////////////////////////////////////
/*
  A simple example using the outline example to blink LEDs connected to GPIO9
  and GPIO10.

  Shows reclaiming GPIOs from preinit() or setup().
  See "Blinky.ino.globals.h" for build options.

  For urgent GPIO pin 9 and 10 initialization add `-DRECLAIM_GPIO_EARLY=1` to
  your "Blinky.ino.globals.h" file. For example when using GPIO10 as an
  INPUT, you may need to initialize early to resolve the issue of two output
  drivers fighting each other. As stated elsewhere, you must include a series
  resistor to limit the virtual short circuit current to the lesser component's
  operating limit. If the ESP8266's looses, the device may fail to boot.
  And, there is the risk of stressing devices and early failures.
*/
#include <ModeDIO_ReclaimGPIOs.h>

#if RECLAIM_GPIO_EARLY
// Variable is used before C++ runtime init has started.
bool gpio_9_10_available __attribute__((section(".noinit")));
#else
bool gpio_9_10_available = false;
#endif

uint32_t last_wink;
constexpr uint32_t kWinkInterval = 1000u; // 1 sec.
void blinky() {
  uint32_t current = millis();
  if (kWinkInterval < current - last_wink) {
    last_wink = current;
    uint8_t on9 = digitalRead(9);
    digitalWrite(10, on9);
    digitalWrite(9, (on9) ? LOW : HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n\nOutline Sketch using 'reclaim_GPIO_9_10()'");
#if ! RECLAIM_GPIO_EARLY
  gpio_9_10_available = reclaim_GPIO_9_10();
  if (gpio_9_10_available) {
    /*
      Add additional GPIO pin initialization here
    */
    digitalWrite(9, HIGH);    // Assumes LED is on when set LOW
    digitalWrite(10, HIGH);
    pinMode(9, OUTPUT);
    pinMode(10, OUTPUT);
  }
#endif
  last_wink = millis();
}

void loop() {
  if (gpio_9_10_available) {
    blinky();
  }
}

#if RECLAIM_GPIO_EARLY
void preinit() {
  /*
    If using `-DDEBUG_FLASH_QE=1`, reclaim_GPIO_9_10() printing will be at
    115200 bps.
  */
  gpio_9_10_available = reclaim_GPIO_9_10();
  if (gpio_9_10_available) {
    // urgent GPIO pin initialization
    digitalWrite(9, LOW);
    digitalWrite(10, LOW);
    pinMode(9, OUTPUT);
    pinMode(10, OUTPUT);
  }
}
#endif

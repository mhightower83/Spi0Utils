/*
  An outline example of using Reclaim GPIOs

  Shows reclaiming GPIOs from preinit() or setup().
  See "OutlineCustom.ino.globals.h" for build options.

  For urgent GPIO pin 9 and 10 initialization add `-DRECLAIM_GPIO_EARLY=1` to
  your "OutlineCustom.ino.globals.h" file. For example when using GPIO10 as an
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
  }
#endif
}

void loop() {

}

#if RECLAIM_GPIO_EARLY
void preinit() {
  /*
    If using `-DDEBUG_FLASH_QE=1`, reclaim_GPIO_9_10() printing will be at
    115200 bps.
  */
  gpio_9_10_available = reclaim_GPIO_9_10();
  if (gpio_9_10_available) {
    /*
      Add additional urgent GPIO pin initialization here
    */
  }
}
#endif

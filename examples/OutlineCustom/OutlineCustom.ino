/*
  An outline example of using Reclaim GPIOs

  Shows reclaiming GPIOs from preinit() or setup()

  Additionally provides a hypothetical custom handler for EN25QH128A by
  supplying replacement function: `spi_flash_vendor_cases()`

  For urgent GPIO pin 9 and 10 initialization add `-DRECLAIM_GPIO_EARLY=1` to
  your "Outline.ino.globals.h" file. For example when using GPIO10 as an INPUT,
  you may need to initialize early to resolve the issue of two output drivers
  fighting each other. As stated elsewhere, you must include a series resistor
  to limit the virtual short circuit current to the lesser component's operating
  limit. If the ESP8266's looses, the device may fail to boot.
*/
#include <ModeDIO_ReclaimGPIOs.h>


#if RECLAIM_GPIO_EARLY
extern bool gpio_9_10_available;
#else
bool gpio_9_10_available = false;
#endif

void setup() {

  Serial.begin(115200);
  delay(200);
  Serial.printf("\n\n\nOutline Sketch using 'reclaim_GPIO_9_10()'");
#if ! RECLAIM_GPIO_EARLY
  gpio_9_10_available = reclaim_GPIO_9_10();
  /*
    Add additional GPIO pin initialization here
  */
#endif

}

void loop() {

}

#if RECLAIM_GPIO_EARLY
bool gpio_9_10_available __attribute__((section(".noinit")));

void preinit() {
  /*
    If using `-DDEBUG_FLASH_QE=1`, printing will be at 115200 bps
  */
  gpio_9_10_available = reclaim_GPIO_9_10();
  if (gpio_9_10_available) {
    /*
      Add additional urgent GPIO pin initialization here
    */
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Expand Flash Vendor support for GPIO9 and GPIO10 reclaim
// Adds support for EN25QH128A
#include <SpiFlashUtils.h>

extern "C"
bool spi_flash_vendor_cases(uint32_t _id) {
  using namespace experimental;

  bool success = false;
  uint32_t vendor = 0xFFu & _id;

  if (SPI_FLASH_VENDOR_EON == vendor) { // 0x1C
    // EON SPI Flash parts have a WPDis S6 bit in status register-1 for
    // disabling /WP (and /HOLD). This is similar to QE/S9 on other vendors,
    // ISSI and Macronix.

    // Match on Device/MFG ignoreing bit capcacity
    if (0x701Cu == (_id & 0x0FFFFu)) {
      // EN25QH128A hypothetical example - never tested
      success = set_S6_QE_WPDis_bit(volatile_bit);
    } else
    if (0x301Cu == (_id & 0x0FFFFu)) {
      // EN25Q32A, EN25Q32B, EN25Q32C pin 4 NC (DQ3) no /HOLD function
      // tested with EN25Q32C
      success = set_S6_QE_WPDis_bit(volatile_bit);
      // Could refine to EN25Q32C only by using the presents of SFDP support.
    } else {
      // let all other EON parts fail.
      return false;
    }
  }

  if (! success) {
    success = __spi_flash_vendor_cases(_id);
  }
  return success;
}

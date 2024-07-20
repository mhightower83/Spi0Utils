# SPI0 Flash Utilities

## Status: WIP
* working
* code reorganizing
* comments always need updating

This library was created to support reclaiming the use of GPIO pins 9 and 10.
A lot of operations were generalized and placed in `ModeDIO_ReclaimGPIOs.h`,
`SpiFlashUtils.h`, and `SpiFlashUtilsQE.h`. This library makes extensive use of
`experimental::SPI0Command` in Arduino ESP8266 core which allows our
initialization code to stay out of IRAM.

Compatible SPI flash memory configured to ignore pins connected to GPIO9 and
GPIO10 allows them to be used as GPIO pins on ESP-12F modules and NodeMCU DEV
boards.
Boards that support SPI Mode: "QIO" should also work with SPI Mode: "DIO"
combined with `reclaim_GPIO_9_10()`.
Boards that support, at best, SPI Mode: "DIO" might work with
`reclaim_GPIO_9_10()` as well.

## Pin Name Clarifications and Terms:
* GPIO9, /HOLD, Flash SD3, Espressif SD_D2, and IO<sub>3</sub> all refer to the same connection, but from different perspectives.
* GPIO10, /WP, Flash SD2, Espressif SD_D3, and IO<sub>2</sub> all refer to the same connection, but from different perspectives.
* The ESP8266 mux decides if the SPI0 controller has the pin connection or if the GPIO has the pin. You alter this assignment when you call `pinMode`. Example `pinMode(9, SPECIAL)` assigns the connection to the SPI0 controller. Where, `pinMode(9, OUTPUT)` connects to a GPIO output function or `pinMode(9, INPUT)` connects to a GPIO input function.
* Quad Enable, QE, is a bit in the Flash Status Register. When QE is set, pin functions /WP[<sup>1</sup>](https://github.com/mhightower83/Arduino-ESP8266-misc/wiki/Pins-GPIO9-and-GPIO10/#1-nuance) and /HOLD are disabled. This allows the Chip to reuse those pins in SPI Modes QIO or QOUT without creating conflicts.
* SR1, SR2, and SR3 refer to 8-bit SPI Status Registers 1, 2, and 3.
* S6, S9, and S15 refer to bits in the SPI Flash Status Registers. SR1 bits 0 - 7 are S0 - S7. SR2 bits 0 - 7 are S8 - S17.

## A Guide to making GPIO9 and GPIO10 work

### What is the hardware and NONOS SDK expecting?
Espressif's [SPI Flash Modes](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/spi-flash-modes.html) provides good background knowledge of supported Flash Modes and the [FAQ](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/spi-flash-modes.html#frequently-asked-questions) covers when/why they don't work.

The ESP8266EX BootROM expects:
* The Flash to support 16-bit Writes to Flash Status Register-1 using Flash instruction 01h. For many newer Flash parts, the datasheets describe this as legacy support.
* The Flash has a Quad Enable bit in the Status Register at S9/BIT9. AKA BIT1 in Status Register-2.
* WEL Write Enable Latch at BIT1 of SR1 and WIP Write In Progress at BIT0 of SR1.

The NONOS SDK is Prepared to handle:
* The above items for BootROM.
* GigaDevice GD25Q32 and GD25Q128 support only 8-bit Write SR1 and SR2; however, it does have the QE bit at S9.
* ISSI/PMC Flash with the QE/S6 BIT6 in SR1
* Macronix Flash with the QE/S6 BIT6 in SR1

Based on the `SPI Flash Mode` found in the [Firmware Image Header](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/firmware-image-format.html#file-header), the BootROM tries to set or clear the non-volatile QE bit in the SPI Flash Status Register-2,
using QE=1 for QIO & QOUT and QE=0 for DIO and DOUT.
Some ESP8266 module providers do not pair the ESP8266EX with a 100% compatible SPI Flash Chip. And the BootROM's attempt to update the QE bit fails.
While some Flash Chips are QIO capable, they are incompatible because they do not support 16-bit status register-1 writes (or don't have a QE bit at S9).
If the Status Register `WEL` bit is still set after a Write Status Register, this is a good indicator that the 16-bit transfer failed.

This may explain why some ESP8266 Modules have silkscreen marks of DIO or QIO on the back of the antenna area. The ones marked with DIO may have Flash chips that technically support QIO but are incompatible with the ESP8266's BootROM code for QIO. However, they work fine using DIO.

The Winbond W25Q128JVSIQ and others support 8-bit and legacy 16-bit writes to Status Register-1. Note older parts with _only_ legacy 16-bit Write Status Register-1 are still in the supply chain and shipping.
Also, vendors like GigaDevice GD25Q32 only support discrete 8-bit Write Status Registers 1, 2 & 3 then, the BootROM initialization fails to change the current mode.

### Theory of Operation
<!-- ~We now look at the SPI Flash Status Register-2's QE bit.~  -->
On an 8-pin SPI Flash, two of the pins function as either /WP (Write Protect) and /HOLD or two data pins SD2 and SD3 when in a Quad-data mode. When QE is clear the pin functions /WP and /HOLD are enabled. If the /HOLD pin is held `LOW`, the Flash is put on hold and its output lines float. If /HOLD is `LOW` too long, a WDT Reset will occur. When QE is set, the pin functions /WP[<sup>1</sup>](https://github.com/mhightower83/Arduino-ESP8266-misc/wiki/Pins-GPIO9-and-GPIO10/#1-nuance) and /HOLD are disabled and those physical pins become SD2 and SD3 when Quad transfers are made, which only occurs with a quad Flash instruction. Otherwise, the pins float and are ignored by the Flash.

By selecting `SPI Flash Mode: "DIO"` for our Sketch build, we have the SPI0 controller setup for DIO operations thus no Quad Flash instructions will be used.
In this ideal case, we set the volatile copy of the QE bit, before calling `pinMode()` to configure GPIO9 and GPIO10 for the Sketch.
By setting the QE bit in the Flash Status Register, we disable the pin functions /WP[<sup>1</sup>](https://github.com/mhightower83/Arduino-ESP8266-misc/wiki/Pins-GPIO9-and-GPIO10/#1-nuance) and /HOLD so that no crash will occur from reassigning ESP8266's mux pin from /HOLD to GPIO9 or /WP to GPIO10.
The SPI Flash memory pins for /WP and /HOLD are still connected; however, they stay in the Float state because the SPI0 controller is _not_ configured to use Quad instructions.

This is for the ideal case where an ESP8266 Module works well with all SPI Flash Modes, QIO, QOUT, DIO, and DOUT. While we don't use QIO or QOUT mode, a device that functions with those options is more likely to support turning off pin functions /WP and /HOLD in a way we are familiar with.

#### <sup>1</sup> Nuance
Not all Flash devices that claim to disable pin function /WP with QE=1, fully do so. To fully ignore the /WP pin, some Flash require SRP1 and SRP0 set to 0. This was my case with BergMicro, Winbond, and XMC Flash; however, GigaDevice worked as I expected. I must be reading something, into the datasheet, that is not there. From what I have seen so far except for the QE bit, it would be best that all bits in SR1 and SR2 are zero.


### Two main steps for using this library
1. Run the "Analyze" example on your module or development board. It will
   evaluate if the SPI flash memory on your board supports turning off pin
   functions /WP and /HOLD. "Analyze" will also confirm if builtin support is
   already present in the library, and suggest copy/paste code to handle your
   flash memory. If needed, add the copy/paste code to a `CustomVendor.ino` file
   in your sketch folder.
   (How often does this work? - unknown, it works with my devices)

2. Call `reclaim_GPIO_9_10()` from your Sketch startup code, either `preinit()`
   or `setup()`. Perform additional setup as needed.

See [example Sketches](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples#readme)
for more details.

### Three common QE flash cases

A lot of the code in this library is for evaluating the flash memory once you
are finished with that, all you need to recover GPIO 9 and 10 is one of three main functions. Assuming your flash memory is not already supported by the builtin QE handlers.

1) For flash memory with QE or WPDis bit at BIT6:
```
bool set_S6_QE_bit_WPDis(bool non_volatile);
```
Example EON flash EN25Q32C, CustomeVendor.ino:
```
#include <ModeDIO_ReclaimGPIOs.h>

extern "C"
bool spi_flash_vendor_cases(uint32_t device) {
  using namespace experimental;
  bool success = false;

  if (0x301Cu == (device & 0xFFFFu)) {
    success = set_S6_QE_bit_WPDis(non_volatile_bit);
  }

  if (! success) {
    // then try builtin support
    success = __spi_flash_vendor_cases(device);
  }
  return success;
}
```
2) For flash memory with QE bit at BIT9 and support 16-bit Status Register-1 writes
which will also include boards that support the build option SPI Mode: "QIO":
```
bool set_S9_QE_bit__16_bit_sr1_write(bool non_volatile);
```
Example Winbond flash 25Q32FVSIG, CustomeVendor.ino:
```
#include <ModeDIO_ReclaimGPIOs.h>

extern "C"
bool spi_flash_vendor_cases(uint32_t device) {
  using namespace experimental;
  bool success = false;

  if (0x40EFu == (device & 0xFFFFu)) {
    success = set_S9_QE_bit__16_bit_sr1_write(volatile_bit);
  }

  if (! success) {
    // then try builtin support
    success = __spi_flash_vendor_cases(device);
  }
  return success;
}
```
3) For flash memory with QE bit at BIT9 and support only 8-bit Status Register writes:
```
bool set_S9_QE_bit__8_bit_sr2_write(bool non_volatile);
```
Example GigaDevice flash GD25Q32C, CustomeVendor.ino:
```
#include <ModeDIO_ReclaimGPIOs.h>

extern "C"
bool spi_flash_vendor_cases(uint32_t device) {
  using namespace experimental;
  bool success = false;

  if (0x40C8u == (device & 0xFFFFu)) {
    success = set_S9_QE_bit__8_bit_sr2_write(volatile_bit);
  }

  if (! success) {
    // then try builtin support
    success = __spi_flash_vendor_cases(device);
  }
  return success;
}
```

If the flash memory supports volatile Status Register bits, use `volatile_bit`
for the argument. Otherwise, use non_volatile_bit. A concern with using the
`non_volatile_bit` is wear on the Flash. In some cases, the BootROM cannot
change the QE bit leaving it as is. So once set it stays set. This is likely to
happen for QE/S6. The BootROM can only handle QE/S9 and 16-bit Status Register-1
writes.

<!-- If you want to look at how it works start with `reclaim_GPIO_9_10()` in
`ModeDIO_ReclaimGPIOs.cpp` -->

When using GPIO9 and GPIO10, be mindful these pins are driven high during boot.
And stay high until reclaimed then they are setup with `pinMode(, INPUT)` like
other Arduino GPIO pins.


TODO: delete from here down later
I have a fix PR pending, https://github.com/esp8266/Arduino/pull/9140#issue-2300765579,
which is required for this library to work.

I use BacktraceLog library in most of my projects and examples.
If you don't have it installed, just comment out the offending `#include` line.





<!--

What makes for a compatible flash memory?
1. The ability to turn off pin functions /WP and /HOLD.
  * QE/S6, quad enable bit at BIT6, bit in the flash status register-1.
    Flash memory that uses QE/S6 often does not have status register-2.
    Flash memory with the QE/S6 bit often use it to turn off both pin functions
    /WP and /HOLD.
  * QE/S9, quad enable bit at BIT9, setting this bit can disable the pin
    function /HOLD. Depending on the flash chip version and vendor, various
    permutations of QE and/or SRP1:SRP0 are needed to disable both pin function
    /WP and /HOLD. Some flash memory require status registers 1 and 2 writes to
    be a 16-bit write to status register-1. Others, require individual 8-bit
    status register writes.
2. Pin functions /WP and /HOLD are not present on the flash memory

TODO: Phase out this link.
See [Reclaiming GPIO9 and GPIO10](https://github.com/mhightower83/Arduino-ESP8266-misc/wiki/Pins-GPIO9-and-GPIO10)
for a more detailed explanation.
-->

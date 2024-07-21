# SPI0 Flash Utilities

## Status: WIP
* working
* code reorganizing
* comments always need updating

This library was created to support reclaiming the use of GPIO pins 9 and 10. A
lot of operations were generalized and placed in `ModeDIO_ReclaimGPIOs.h`,
`SpiFlashUtils.h`, and `SpiFlashUtilsQE.h`. This library makes extensive use of
`experimental::SPI0Command` in Arduino ESP8266 core which allows our
initialization code to stay out of IRAM.

Compatible SPI flash memory configured to ignore pins connected to GPIO9 and
GPIO10 allows them to be used as GPIO pins on ESP-12F modules and NodeMCU DEV
boards. Boards that support SPI Mode: "QIO" should also work with SPI Mode:
"DIO" combined with `reclaim_GPIO_9_10()`. Boards that support, at best, SPI
Mode: "DIO" might work with `reclaim_GPIO_9_10()` as well.

## Pin Name Clarifications and Terms:

* GPIO9, /HOLD, Flash SD3, Espressif SD_D2, and IO<sub>3</sub> all refer to the
same connection, but from different perspectives.
* GPIO10, /WP, Flash SD2, Espressif SD_D3, and IO<sub>2</sub> all refer to the
same connection, but from different perspectives.
* The ESP8266 mux decides if the SPI0 controller has the pin connection or if
the GPIO has the pin. You alter this assignment when you call `pinMode`. Example
`pinMode(9, SPECIAL)` assigns the connection to the SPI0 controller. Where,
`pinMode(9, OUTPUT)` connects to a GPIO output function or `pinMode(9, INPUT)`
connects to a GPIO input function.
* Quad Enable, QE, is a bit in the Flash Status Register. When QE is set, pin
functions /WP and /HOLD are disabled. This allows the Chip to reuse those pins
in SPI Modes QIO or QOUT without creating conflicts.
* SRP1 and SRP0 refer to Status Register Protect-1 and Status Register
Protect-0. Not all flash memory implement these bits; however, when they do,
SRP1:SRP0 may implement an ignore /WP function.
* SR1, SR2, and SR3 refer to 8-bit SPI Status Registers 1, 2, and 3.
* S6, S9, and S15 refer to bits in the SPI Flash Status Registers. SR1 bits
0 - 7 are S0 - S7. SR2 bits 0 - 7 are S8 - S17.

## A Guide to making GPIO9 and GPIO10 work

### What is the hardware and NONOS SDK expecting?

Espressif's [SPI Flash Modes](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/spi-flash-modes.html)
provides good background knowledge of supported Flash Modes and the
[FAQ](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/spi-flash-modes.html#frequently-asked-questions)
covers when/why they don't work.

The ESP8266EX BootROM expects:
* The Flash to support 16-bit Writes to Flash Status Register-1 using Flash
instruction 01h. For many newer Flash parts, the datasheets describe this as
legacy support.
* The Flash has a Quad Enable bit in the Status Register at S9/BIT9. AKA BIT1
in Status Register-2.
* WEL Write Enable Latch at BIT1 of SR1 and WIP Write In Progress at BIT0 of SR1.

The NONOS SDK is Prepared to handle:
* The above items for BootROM.
* GigaDevice GD25Q32 and GD25Q128 support only 8-bit Write SR1 and SR2; however,
it does have the QE bit at S9.
* ISSI/PMC Flash with the QE/S6 BIT6 in SR1
* Macronix Flash with the QE/S6 BIT6 in SR1

Based on the `SPI Flash Mode` found in the [Firmware Image Header](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/firmware-image-format.html#file-header),
the BootROM tries to set or clear the non-volatile QE bit in the SPI Flash
Status Register-2, using QE=1 for QIO & QOUT and QE=0 for DIO and DOUT. Some
ESP8266 module providers do not pair the ESP8266EX with a 100% compatible SPI
Flash Chip. And the BootROM's attempt to update the QE bit fails. While some
Flash Chips are QIO capable, they are incompatible because they do not support
16-bit status register-1 writes (or don't have a QE bit at S9). If the Status
Register `WEL` bit is still set after a Write Status Register, this is a good
indicator that the 16-bit transfer failed.

This may explain why some ESP8266 Modules have silkscreen marks of DIO or QIO on
the back of the antenna area. The ones marked with DIO may have Flash chips that
technically support QIO but are incompatible with the ESP8266's BootROM code for
QIO. However, they work fine using DIO.

The Winbond W25Q128JVSIQ and others support 8-bit and legacy 16-bit writes to
Status Register-1. Note older parts with _only_ legacy 16-bit Write Status
Register-1 are still in the supply chain and shipping. Also, vendors like
GigaDevice GD25Q32 only support discrete 8-bit Write Status Registers 1, 2 & 3
then, the BootROM initialization fails to change the current mode.

### Theory of Operation

On an 8-pin SPI Flash, two of the pins function as either /WP (Write Protect)
and /HOLD or two data pins SD2 and SD3 when in a Quad-data mode. When QE is
clear the pin functions /WP and /HOLD are enabled. If the /HOLD pin is held
`LOW`, the Flash is put on hold and its output lines float. If /HOLD is `LOW`
too long, a WDT Reset will occur. When QE is set, the pin functions /WP and
/HOLD are disabled and those physical pins become SD2 and SD3 when Quad
transfers are made, which only occurs with a quad Flash instruction. Otherwise,
the pins float and are ignored by the Flash.

By selecting `SPI Flash Mode: "DIO"` for our Sketch build, we have the SPI0
controller setup for DIO operations. Thus, the quad-flash instructions are not
used. In this ideal case, we set the volatile copy of the QE bit before calling
`pinMode()` to configure GPIO9 and GPIO10 for the Sketch. By setting the QE bit
in the Flash Status Register, we disable the pin functions /WP and /HOLD so that
no crash will occur from reassigning ESP8266's mux pin from /HOLD to GPIO9 or
/WP to GPIO10. The SPI Flash memory pins for /WP and /HOLD are still connected;
however, they stay in the Float state because the SPI0 controller is _not_
configured to use Quad instructions. For some flash devices to ignore the /WP
pin, both SRP1 and SRP0 must be set to 0. I find some datasheets to be confusing
on this point.

This is for the ideal case where an ESP8266 Module works well with all SPI Flash
Modes, QIO, QOUT, DIO, and DOUT. While we don't use QIO or QOUT mode, a device
that functions with those options is more likely to support turning off pin
functions /WP and /HOLD in a way we are familiar with.

## Two main steps for using this library

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

## Three common QE flash cases

A lot of the code in this library is for evaluating the flash memory once you
are finished with that, all you need to recover GPIO 9 and 10 is one of three
main functions. Assuming your flash memory is not already supported by the
builtin QE handlers.

The function `spi_flash_vendor_cases()` shown in the examples below are part of
the `reclaim_GPIO_9_10()` call chain. See [example Sketches](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Outline/Outline.ino)
for `reclaim_GPIO_9_10()` use and placement.


### 1. `set_S6_QE_bit_WPDis`
For flash memory with QE or WPDis bit at BIT6:
```cpp
bool set_S6_QE_bit_WPDis(bool non_volatile);
```
Example EON flash EN25Q32C, CustomeVendor.ino:

```cpp
#include <ModeDIO_ReclaimGPIOs.h>

extern  "C"
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

### 2. `set_S9_QE_bit__16_bit_sr1_write`
For flash memory with QE bit at BIT9 and support 16-bit Status Register-1 writes
which will also include boards that support the build option SPI Mode: "QIO":
```cpp
bool set_S9_QE_bit__16_bit_sr1_write(bool non_volatile);
```

Example Winbond flash 25Q32FVSIG, CustomeVendor.ino:
```cpp
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

### 3. `set_S9_QE_bit__8_bit_sr2_write`
For flash memory with QE bit at BIT9 and support only 8-bit Status Register writes:
```cpp
bool set_S9_QE_bit__8_bit_sr2_write(bool non_volatile);
```
Example GigaDevice flash GD25Q32C, CustomeVendor.ino:
```cpp
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

## Review and Considerations
* No generic solution for all modules. There are too many partially compatible
Flash memories in use.

* Select a non-quad SPI Flash Mode: DIO or DOUT. For the Arduino IDE selection
`Tools->Board: "NodeMCU v1.0"`, DIO is internally selected.

* After BootROM and NONOS SDK init, the state of the GPIO pins 9 and 10 is
equivalent to `pinMode( , SPECIAL)`. These pins are actively driven.

* Before taking control of these pins with `pinMode()`, the SPI Flash Chip
_must_ be configured to ignore the /WP and /HOLD signal lines.

* After reset or boot the /WP and /HOLD signals are actively driven. When used
as an input, insert a series resistor (300&Omega; preferably greater) to limit
the virtual short circuit current that occurs from connecting two opposing
drivers. When booting, the ESP8266 must be allowed to win this conflict or the
system will not run. The GPIO pins are rated for 12ma MAX. When calculating the
resistor values, between the ESP and the signal source, for computation use the
one with the smaller current limit.

* Generously adding a series resistor may conflict with the I<sup>2</sup>C
specification.

* When used as an output, take into consideration that the output will be driven
high at boot time until `pinMode()` is called. If this poses a problem,
additional logic may be needed to gate these GPIO pins. (A similar issue exists
with other GPIOs 0, 1, 2, 3, and 16). Since GPIO15 must be low for the system to
boot, this could be leveraged to create a master enable/disable gate signal.

* Not likely an issue; however, the capacitive load of the Flash Chip's /WP and
/HOLD pins may need to be considered on rare occasions. The datasheets have
parameters: COUT, CIN, and CL. Only CIN is of interest. CIN is the capacitance
contributed by a Flash pin for an input operation. The typical capacitive load
of a flash input pin is 6pF. This contribution to a circuit should be
negligible. However, you should include it when calculating the capacitive
loading for something like I<sup>2</sup>C.

TODO: When finished, delete from here down

Requires changes from pending PR, https://github.com/esp8266/Arduino/pull/9140#issue-2300765579,

I use BacktraceLog library in most of my projects and examples.
If you don't have it installed, just comment out the offending `#include` line.

Includes details from [Reclaiming GPIO9 and GPIO10](https://github.com/mhightower83/Arduino-ESP8266-misc/wiki/Pins-GPIO9-and-GPIO10)

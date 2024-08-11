# SPI0 Flash Utilities

An Arduino ESP8266 library that supports reclaiming the use of GPIO pins 9 and
10 on ESP8266 modules that expose access to those pins. The build requires SPI
Flash Mode "DIO" or "DOUT".

I expect this solution to work for the ESP-12F Module and the NodeMCU 1.0 DEV
board; however, it may depend on the SPI flash chip the module manufacturer
selected. The few I have are working. I assume board vendors that expose GPIO
pins 9 and 10 have made wise choices with their flash chip selection and allow
the pin functions /WP and /HOLD to be disabled. I am cautiously optimistic about
this solution. Confidence will only come with experience.

Various operations were generalized and placed in `ModeDIO_ReclaimGPIOs.h`,
`SpiFlashUtils.h`, and `SpiFlashUtilsQE.h`. This library extensively uses
`experimental::SPI0Command` in the Arduino ESP8266 core, which allows our
initialization code to stay out of IRAM.

This library requires the Arduino ESP8266 GIT commit
c2f136515a396be1101b261fe7b71e137aef0dce or the current GIT from Arduino
ESP8266. Use Release 3.2.3 or better when it becomes available.

Not all flash memory is compatible with this feature. When successful, the SPI
flash memory will ignore the flash memory pins connected to GPIO9 and GPIO10.
That leaves each circuit free to be reassigned as a GPIO.

Generally speaking, boards that support SPI Mode: "QIO" should also work with
SPI Mode: "DIO" combined with `reclaim_GPIO_9_10()`. Boards that support, at
best, SPI Mode: "DIO" may also work with `reclaim_GPIO_9_10()`.

## Quick Start

If this quick approach doesn't work, come back and continue reading.

First, I suggest running the [Analyze Sketch](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Analyze/)
on your board. While it is in the examples folder, you shouldn't need to make
any changes. It is a ready-to-go tool. It will indicate if the library has
built-in support for your board. If not, it may provide code to add to your
sketch as a new module. Copy/paste the code fragment to a new file, like
`CustomVendor.ino`. The Analyze Sketch may spew a lot of text. The Analyze
Sketch may spew a lot of text. To find areas of concern, focus on the lines
starting with `*`.


Next, look at the example function [Outline](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Outline/Outline.ino)
to see the basic things you need to add to your project to use GPIO9 and GPIO10.

For an optional test to build confidence, use the [Blinky](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Blinky/Blinky.ino)
example sketch to show that this method works for your ESP8266 board/flash.
Connect an LED in series with a current limiting resistor to the 3.3V rail and
GPIO10. The LED will light when GPIO10 is driven LOW. Repeat with another LED
and resistor to GPIO9. The Blinky sketch will alternately power each LED every 1
second.


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
Protect-0. Not all flash memories have these bits. When they do, the pair
SRP1:SRP0 (0:0) may implement an ignore /WP function.

* SR1, SR2, and SR3 refer to 8-bit SPI Status Registers 1, 2, and 3.

* Usage of S6, S9, and S15 refers to a QE bit placement in the SPI Flash Status Registers. SR1 bits 0 - 7 are S0 - S7. SR2 bits 0 - 7 are S8 - S15.

## A Guide to making GPIO9 and GPIO10 work

### What is the hardware and NONOS SDK expecting?

Espressif's [SPI Flash Modes](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/spi-flash-modes.html)
provides good background knowledge of supported Flash Modes and the
[FAQ](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/spi-flash-modes.html#frequently-asked-questions)
covers when/why they don't work.

The ESP8266EX BootROM expects:

* The Flash to support 16-bit Writes to Flash Status Register-1 using Flash
instruction 01h. For some newer Flash parts, the datasheets describe this as
legacy support.

* The Flash has a Quad Enable bit in the Status Register at S9/BIT9. AKA BIT1
in Status Register-2.

* WEL "Write Enable Latch" at BIT1 of SR1 and WIP "Write In Progress" at BIT0 of SR1.

The NONOS SDK is prepared to handle:

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
16-bit status register-1 writes (or don't have a QE bit at S9).
When the Status Register's `WEL` bit is left set after a Write Status Register
instruction, this is a good indicator that the 16-bit transfer failed.

It may be that ESP8266 Modules with silkscreen marks of DIO on the back of the
antenna area may have Flash chips that technically support QIO but are
incompatible with the ESP8266's BootROM code for QIO. However, they work fine
using DIO.

The Winbond W25Q128JVSIQ and others support 8-bit and legacy 16-bit writes to
Status Register-1. Note older parts with _only_ legacy 16-bit Write Status
Register-1 are still in the supply chain and shipping. Also, vendors like
GigaDevice GD25Q32 only support discrete 8-bit Write Status Registers 1, 2 & 3
then, the BootROM initialization fails to change the current mode.

The Winbond W25Q128JVSIQ (and others) supports the 8-bit and legacy 16-bit
writes to Status Register-1. Note older parts with _only_ legacy 16-bit Write
Status Register-1 are still in the supply chain and shipping. With GigaDevice
GD25Q32 (and other vendors) that only support discrete 8-bit Write Status
Registers (1 and 2), the BootROM initialization will fail to change the current
mode.


### Theory of Operation

For quad-data mode on an 8-pin SPI Flash, two pins will function as either /WP
(Write Protect) and /HOLD or as two data pins SD2 and SD3. When QE is clear, the
pin functions /WP and /HOLD are enabled. If the /HOLD pin is held `LOW,` the
flash is put on hold; its output lines float. If /HOLD pin is `LOW` for too
long, a WDT Reset will occur. With the QE set, the pin functions /WP and /HOLD
are disabled. Only with a quad flash instruction do those physical pins become
SD2 and SD3. Otherwise, those pins float, and the flash ignores them.

By selecting `SPI Flash Mode: "DIO"` for our Sketch build, we have the SPI0
controller setup for DIO operations. Thus, the quad flash instructions are never
issued. In this ideal case, we set the volatile copy of the QE bit before
calling `pinMode()` to configure GPIO9 and GPIO10 for the sketch. By setting the
QE bit in the Flash Status Register, we disable the pin functions /WP and /HOLD
so that no crash will occur from reassigning ESP8266's mux pin from /HOLD to
GPIO9 or /WP to GPIO10. The SPI Flash memory pins for /WP and /HOLD are still
connected; however, they stay in the Float state because the SPI0 controller is
_not_ configured to use Quad instructions.

The ideal case described above is typical for an ESP8266 module that works with
all available SPI Flash Modes, QIO, QOUT, DIO, and DOUT.  While we don't use QIO
or QOUT mode, a device that functions with those options is more likely to
support turning off pin functions /WP and /HOLD in a way we are familiar with.

For some flash devices to ignore the /WP pin, SRP1 and SRP0 must also be set
to 0. SRP1:SRP0 are often in the zero states after boot with `SPI Flash Mode:
"DIO"`, which may explain why some people experience GPIO10 working and GPIO9
crashing. I find some datasheets confusing on the requirements of QE, SRP1, and
SRP0 to meet our goal, and the requirements will also differ between
manufacturers.

## Two main steps for using this library

1. Run the "Analyze" example on your module or development board. It will
   evaluate if the SPI flash memory on your board supports turning off pin
   functions /WP and /HOLD. The "Analyze" sketch will also confirm if library
   built-in support is present and suggest copy/paste code to handle your flash
   memory. If needed, copy and paste the suggested code to the file
   `CustomVendor.ino` in your sketch folder.

2. Call `reclaim_GPIO_9_10()` from your Sketch startup code, either `preinit()`
   or `setup()`. Perform additional setup as needed.

See [example Sketches](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples#readme)
for more details.

## Four common QE flash cases

A lot of the code in this library is for evaluating the flash memory once you
are finished with that, all you need to recover GPIO 9 and 10 is one of three
main functions. Assuming your flash memory is not already supported by the
built-in QE handlers.

The function `spi_flash_vendor_cases()` shown in the examples below is part of
the `reclaim_GPIO_9_10()` call chain. See the [Outline](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Outline/Outline.ino)
example sketch for `reclaim_GPIO_9_10()` use and placement.


### 1. `set_S6_QE_bit__8_bit_sr1_write`
For flash memory with QE or WPDis bit at BIT6:
```cpp
bool set_S6_QE_bit__8_bit_sr1_write(bool non_volatile);
```
Example EON flash EN25Q32C, CustomVendor.ino:

```cpp
#include <ModeDIO_ReclaimGPIOs.h>

extern  "C"
bool spi_flash_vendor_cases(uint32_t device) {
  using namespace experimental;
  bool success = false;

  if (0x301Cu == (device & 0xFFFFu)) {
    success = set_S6_QE_bit__8_bit_sr1_write(non_volatile_bit);
  }

  if (! success) {
    // then try built-in support
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

Example Winbond flash 25Q32FVSIG, CustomVendor.ino:
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
    // then try built-in support
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
Example GigaDevice flash GD25Q32C, CustomVendor.ino:
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
    // then try built-in support
    success = __spi_flash_vendor_cases(device);
  }
  return success;
}
```

### 4. `success = true;`
Flash memory that does not implement either pin function WP or HOLD, no special
handling is required.

Example:
```cpp
#include <ModeDIO_ReclaimGPIOs.h>

extern "C"
bool spi_flash_vendor_cases(uint32_t device) {
  using namespace experimental;
  bool success = false;

  // TODO find a vendor that does this. EON omits /HOLD.
  if (0x????u == (device & 0xFFFFu)) {
    success = true;
  }

  if (! success) {
    // then try built-in support
    success = __spi_flash_vendor_cases(device);
  }
  return success;
}
```


If the flash memory supports volatile Status Register bits, use `volatile_bit`
for the argument. Otherwise, use non_volatile_bit. A concern with using the
`non_volatile_bit` is wear on the flash. For the cases where the BootROM cannot
change the QE bit, it remains set or clear. Often observed with QE/S6 or the
flash only supports 8-bit Status Register writes. The BootROM can only handle
QE/S9 and 16-bit Status Register-1 writes.

## Review and Considerations
* No generic solution for all modules. There are too many partially compatible
Flash memories in use.

* Select a non-quad SPI Flash Mode: DIO or DOUT. For the Arduino IDE selection
`Tools->Board: "NodeMCU v1.0"`, DIO is internally selected.

* After BootROM and NONOS SDK init, the state of the GPIO pins 9 and 10 is
equivalent to `pinMode( , SPECIAL)`. These pins are actively driven.

* Before taking control of these pins with `pinMode()`, the SPI Flash Chip
_must_ be configured to ignore the /WP and /HOLD signal lines.

* After reset (or boot), the /WP and /HOLD signals are actively driven. When
used as an input, insert a series resistor (300&Omega; preferably greater) to
limit the virtual short circuit current, which occurs when connecting two
opposing drivers. When booting, the ESP8266 must be allowed to win this
conflict. Otherwise, the system will not run. The output rating for an ESP8266
GPIO pin is 12ma MAX. When calculating the current limiting resistor between the
ESP and the signal source, use the source with the smaller current limit.

* Generously adding a series resistor may conflict with the I<sup>2</sup>C
specification.

* When used as an output, take into consideration that the output will be driven
high at boot time until `pinMode()` is called. If this poses a problem,
additional logic may be needed to gate these GPIO pins. (A similar issue exists
with other GPIOs 0, 1, 2, 3, and 16). A possible option lies with GPIO15. Since
GPIO15 must be LOW for the system to boot, use this to create a master gate
signal. At reset, GPIO15 will return to LOW.

* Not likely an issue; however, the capacitive load of the flash fhip's /WP and
/HOLD pins may need to be considered on rare occasions. Of the datasheet
parameters COUT, CIN, and CL, only CIN is of interest. CIN is the capacitance
contributed by a Flash pin for an input operation. The typical capacitive load
of a flash input pin is 6pF. This contribution to a circuit should be
negligible. However, you should include it when calculating the capacitive
loading for something like I<sup>2</sup>C.

## Notes, Concerns, and Possible Complications

* The BootROM expects a flash chip that supports 16-bit status register writes.
If the flash memory does not support 16-bit SR-1, this can work to our advantage.

* Not all Flash chips support 16-bit status register writes. exp. GD25Q32E

* Not all devices that support 16-bit register writes support 8-bit status
register writes to status register-2. e.g. BG25Q32A and Winbond W25Q32VS

* Not all devices support a volatile copy of the Status Register. e.g. EN25Q32C

* Not all Flash chips support a 2nd status register. For example, the EN25Q32C
does not have the QE bit as defined by other vendors. It does not have the /HOLD
signal. And /WP is disabled by 8-bit status register-1 BIT6.

* Excess flash wear is of concern. If the flash memory only supports a
non-volatile QE bit and supports 16-bit Flash Status Register-1, the
non-volatile QE bit will get set when `reclaim_GPIO_9_10()` is called and
cleared at boot by the BootROM. The bit flips back and forth at every boot
cycle. When a device does not support a 16-bit Flash Status Register-1 write,
the write fails, and the bit stays as is. Thus, there is no need to rewrite
QE=1.

* Caution: Another reason to prefer writing to volatile copies of status
register bits. Some bits in the Flash Status Register are OTP,
One-Time-Programming. Setting an OTP by accident can be non-recoverable. It is
strongly recommended that you identify your flash memory and review its
datasheet for issues.

* I often rely on RTOS_SDK for insight when the NONOS_SDK does not provide the
details needed. With info from [flash_qio_mode.c](https://github.com/espressif/esp-idf/blob/bdb9f972c6a1f1c5ca50b1be2e7211ec7c24e881/components/bootloader_support/bootloader_flash/src/flash_qio_mode.c#L37-L54),
I found an issue; it shows XM25QU64A as having QE/S6, while the datasheet I
downloaded says it is QE/S9, and my ESP12-F module with XMC flash is also QE/S9.
There is no substitute for testing on real hardware.

* Some flash chips may need individual initialization codes.

<!--
```
Convert to rst with:

pandoc README.md --from markdown --to rst -s -o Reclaim_GPIO_9_10.rst

This web site limits conversion per day:
https://cloudconvert.com/md-to-rst

Better - This one ask for donations:
https://www.vertopal.com/
```
-->

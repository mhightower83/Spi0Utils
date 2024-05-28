# WIP
# SPI0 Flash Utilities
## Status: working
## However, it is a WIP

This library was created during the process of working out how to reclaim GPIO pins 9 and 10.
For a how to, see examples/Reclaim_GPIO_9_10. A lot of operations were generalized and put into `SpiFlashUtils.h`.

This library relies on the use of `experimental::SPI0Command;` in Arduino ESP8266 core. I have a fix PR pending, https://github.com/esp8266/Arduino/pull/9140#issue-2300765579, which is required for this library to work.

I use BacktraceLog library in most of my projects and examples.
If you don't have it installed, just comment out the offending `#include` line.

###
###
# WIP
# For examples/Reclaim_GPIO_9_10
# Reclaim Use of GPIO09 and GPIO10
## Past Attempts
Over the years, attempts to reclaim GPIO9 and GPIO10 have centered around hardware modifications: cutting traces, lifting pads, strapping /WP, and /HOLD Flash pins to +V.
* [ESP8266 ESP-201 module - freeing up GPIO9 and GPIO10](https://smarpl.com/content/esp8266-esp-201-module-freeing-gpio9-and-gpio10)
* [GPIO9 and GPIO10 cause wdt reset](https://bbs.espressif.com/viewtopic.php?f=7&t=654&sid=c745ec068658b957a96ba4366ee514c6)](https://bbs.espressif.com/viewtopic.php?t=654)

With an unmodified EPS-12E or NodeMCU, some report that GPIO10 works; however, GPIO9 caused WDT Resets.
* [Another ESP8266 NodeMCU Development Board](https://www.sigmdel.ca/michel/ha/esp8266/doit_nodemcu_v3_en.html)
  * "However, simple experiments using an LED and a tactile switch showed that SD2 cannot be used as a input or output pin. Attempts to use SD2 would freeze the device. Indeed the schematic in the DOIT ESP-12F manual (page 8) shows SD2 and SD3 connected to the /HOLD and /WP pins of the flash memory chip."


> Reference clarification:
> * GPIO9, /HOLD, SD3, and IO<sub>3</sub> all refer to the same connection but from different perspectives.
> * GPIO10, /WP, SD2, and IO<sub>2</sub> all refer to the same connection but from different perspectives.
> * The ESP8266 mux decides if the SPI0 controller has the pin connection or if the GPIO has the pin. You alter this assignment when you call `pinMode`. Example `pinMode(9, SPECIAL)` assigns the connection to the SPI0 controller. Where `pinMode(9, OUTPUT)` connects to a GPIO output function.


## Why does GPIO10 work and not GPIO9?

GPIO10 is connected to the /WP pin on the SPI Flash; however, it works.
For the why look up the SPI Flash Status Registers BIT8 and BIT7 in the Flash memory datasheet.
When these bits are 0, pin /WP is ignored.
And, this tends to be the factory default.
Confirmed with datasheets for: W25Q32 bits SRL:SRP=0:0, GD25Q32C bits SRP1:SRP0=0:0, XM25Q32C SRP1:SRP0=0:0.
However, not all Flash memory is the same.
For example, EN25Q32C does not work this way.
It has only one status register and no QE bit. (more later on QE bit)
It _does_ have a Status Register BIT6, WPDis, to disable the /WP pin.
And, the /HOLD pin function does not exist on this part, only an SD3 pin.

It is easy to see why GPIO9 does not work.
For the case SPI Flash Mode: "DIO", when the Flash pin /HOLD is active, the Flash chip's SPI bus operation is put on hold (stopped) allowing for a higher priority bus activity.
When held on hold too long, as in the case of the GPIO9 set LOW, a WDT Reset occurs.


## A software solution to make GPIO9 and GPIO10 accessible.

We now look at the SPI Flash Status Register-2's Quad Enable (QE) bit. On an 8-pin SPI Flash, two of the pins function as either /WP (Write Protect) and /HOLD or two data pins SD2 and SD3 when in a Quad-data mode. When QE is clear the /WP and /HOLD pin functions are active. If /HOLD pin is held LOW, the Flash is put on hold and its output lines float. If /HOLD is LOW too long, a WDT Reset will occur. When QE is set, the /WP and /HOLD pin functions are disabled and those physical pins become SD2 and SD3 when Quad transfers are made, which only occurs with a quad opcode. Otherwise, the pins float and are ignored by the Flash. We will come back and use this to our advantage.

Now, we need some background information about the ESP8266 startup.
The BootROM configures the SPI0 controller for accessing the SPI Flash memory.
This involves reading, in standard mode, the SPI Flash Mode from the `.bin` header.
Then, initializing the SPI0 controller for the properties specified.
Additional changes will be done later at NONOS SDK init.

When `SPI Flash Mode: "DIO"` or `"DOUT"` are selected, the QE bit in the Flash Status Register is cleared by the BootROM.
This assumes the SPI Flash memory paired with the ESP8266 is 100% compatible. Key features needed: support for 16-bit Write Status Register-1 and support of the QE bit a BIT9.

By selecting `SPI Flash Mode: "DIO"` for our Sketch build, we have the SPI0 controller setup for DIO operations thus no Quad opcodes will be used.
In this ideal case, we could set the volatile copy of the QE bit from `preinit()` or `setup()`, before calling `pinMode()` to configure GPIO9 and GPIO10 for the Sketch.
With the QE bit set in the Flash Status Register, we have disabled the /WP and /HOLD functions so that no crash will occur from reassigning ESP8266's mux pin for SPI /HOLD to GPIO9 or SPI /WP to GPIO10.
The SPI Flash memory pins for /WP and /HOLD are still connected; however, they stay in the Float state because the SPI0 controller does not issue Quad opcodes.


## ESP8266EX BootROM Expectations
Based on the `SPI Flash Mode` found in the [Firmware Image Header](https://docs.espressif.com/projects/esptool/en/latest/esp8266/advanced-topics/firmware-image-format.html#file-header), the BootROM tries to set or clear the non-volatile QE bit in the SPI Flash Status Register-2. Some ESP8266 module providers do not pair the ESP8266EX with a compatible SPI Flash Chip. And, the BootROM's attempt to update the QE bit fails. While some Flash Chips are QIO capable, they are not compatible because they do not support 16-bit status register-1 writes (or don't have a QE bit at S9). If the Status Register `WEL` bit is still set after a Write Status Register, this is a good indicator that the 16-bit transfer failed.

This may explain why some ESP8266 Modules have silkscreen marks of DIO or QIO on the back of the antenna area. The ones marked with DIO had flash chips that appeared to support QIO. At least, according to the datasheets. However, they only worked when using DIO.

The Winbond W25Q128JVSIQ and others support both 8-bit and legacy 16-bit writes to Status Register-1. However, older parts with legacy 16-bit Write Status Register-1 only are in circulation and shipping. Other vendors like GigaDevice GD25Q32E only support the 8-bit Write Status Register-1 option and the BootROM initialization will fail to change it.


## Conclusion:
### With proper initialization, GPIO pins 9 and 10 are usable without crashing.
* Select a non-quad SPI Flash Mode: DIO or DOUT. For the Arduino IDE selection `Tools->Board: "NodeMCU v1.0"`, DIO is internally selected.
* After BootROM and NONOS SDK init, the state of the GPIO pins 9 and 10 is equivalent to `pinMode( , SPECIAL)`. These pins are actively driven.
* Before taking control of these pins with `pinMode()`, the SPI Flash Chip _must_ be configured to ignore the /WP and /HOLD signal lines.
  * This is the hard part. Most SPI Flash Chips I have seen, differ in how you set them up to ignore the /WP and /HOLD pins.
* After reset the /WP and /HOLD signals are actively driven. When used as an input, use a series resistor (maybe 1K) to limit the virtual short circuit current that occurs. When booting, the ESP8266 must be allowed to win this conflict or the system will not run. The GPIO pins are rated for 12ma MAX. When calculating the resistor values, between the ESP and the signal source, use the smaller current limit.
* When used as an output, take into consideration that the output will be driven high at boot time until pinMode is called.
If this poses a problem, additional logic may be needed to gate these GPIO pins. (A similar issue exists with other GPIOs 0, 1, 2, 3, and 16). Since GPIO15 must be low for the system to boot, this could be leveraged to create a master enable/disable gate signal.
* Not likely an issue; however, the capacitive load of the Flash Chip's /WP and /HOLD pins may need to be considered on rare occasions.

## Possible complications
Setting the non-volatile copy of QE=1 may not always work for every flash device. The ESP8266 BootROM reads the Flash Mode from the boot image and tries to reprogram the non-volatile QE bit. With a Flash Mode of DIO, the BootROM will try and set QE=0.
* The BootROM expects a flash chip that supports 16-bit status register writes.
* Not all Flash chips support a 2nd status register. For example, the EN25Q32C does not have the QE bit as defined by other vendors. It does not have the /HOLD signal. And /WP is disabled by status register-1 BIT6.
* Not all Flash chips support 16-bit status register writes. eg. QD25Q32E
* Not all devices that support 16-bit register writes support 8-bit status register writes to status register-2. eg. BG25Q32A
* Each Flash Chip vendor of interest, may need individual initialization code.

# WIP - Reorganizing Thoughts

##
# To Safely Use GPIO Pins 9 and 10
### Overview:
1. The ESP8266's SPI0 controller must be configured for DIO or DOUT.
2. The SPI Flash memory will need a setting that turns off the /WP and /HOLD pin functions.
This feature is often found when SPI Flash supports QIO. We are not going to use QIO transfers. We need the side effect of QE=1 turning off the /WP and /HOLD pin functions. The SPI Flash will only do a QUAD transfer if it receives a QUAD command. Provided that the SPI0 controller stays in DIO mode we should be safe.
3. Because of the large pool of SPI Flash memory vendors/devices, proper operation should be verified with each new SPI Flash device.

QE stands for Quad Enable, which refers to a bit often found in SPI Flash
Status Register-2 bit S9. When enabled, the SPI Flash no longer uses the /WP and
/HOLD signal pins. When a QUAD command is received, those pins become data pins SD2 and SD3.
While processing non-QUAD commands those pins are ignored.
If we enable QE and keep the system SPI0 controller in DIO mode, then any activity
on the GPIO pins will be ignored by the SPI Flash. In this state, the SPI0 controller
should never send QUAD commands to the SPI Flash. Thus, we have logically isolated
SPI Flash pins /WP and /HOLD from the GPIO INPUT/OUTPUT operations.
Now we can use `pinMode()` to change the GPIO pin to INPUT or OUTPUT.

With the QE=1 bit set, this disables the use of /HOLD and /WP signaling on
their respective pin. The ESP8266 BootROM expects the Flash Chip to support
16-bit writes to status register-1 wrapping into status register-2. It also
expects a QE bit in status register-2 at `BIT1` (or S9 from a 16-bit
perspective). When the Flash Chip does not support 16-bit writes, the write
operation does not occur and the WEL bit (Write-Enable-Latch) is left set.
The lack of 16-bit write status register-1 support may explain why some ESP-12 modules with QUAD supporting
Flash Chips do not work with SPI Flash Mode QIO.

There are some Flash Chips that do not conform to this behavior.

Examples:
* EON's EN25Q32C only has an 8-bit status register, /HOLD is not connected, and
BIT6 in status register-1 disables the /WP signal.
* GigaDevice's GD25Q32ET, does not support 16-bit writes to status register-1.
The QE bit (BIT1) must be set via an 8-bit write to status register-2.

While the EN25Q32C and GD25Q32ET may not work with SPI Flash Mode: "QIO", we can use them in DIO mode and with proper initialization access GPIO9 and GPIO10 within our Sketch.

# Directory Listing for Example

For compatible SPI Flash memory, the SPI Flash can be setup to ignore GPIO pins 9 and 10 allowing them to be used on ESP-12F modules, NodeMCU DEV boards, and other modules that expose those pins.


## [Analyze](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Analyze)

Evaluate the Flash memory for required attributes.
Report if the current code supports the device as is.
Prints a copy/paste block for creating `CustomVender.ino` file like that shown in [OutlineCustom](#OutlineCustom) example.


## [Outline](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Outline)

An outline example demonstrating the use of `reclaim_GPIO_9_10()` which can handle more of the less conforming Flash memories. Like GigaDevice GD25QxxC, GD25QxxE, EON EN25Q32C

Shows reclaiming GPIOs from `preinit()` or `setup()`.

The hope is this will handle a large percentage of the modules exposing GPIO9 and 10.


## [OutlineCustom](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/OutlineCustom)

Similar to "Outline" above. Illustrates adding support for an additional Flash part. Provides a hypothetical custom handler in `CustomeVendor.ino` for `spi_flash_vendor_cases()`.

## [Blinky](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Blinky)

A blinking LED example. An adaptation of the example (https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Outline) that does something with GPIO9 and GPIO10.
You can also add module `CustomVendor.ino` if needed.

## [SFDPHexDump](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/SFDPHexDump)

Probe the Flash for SFDP data

For information gathering, reads and dumps the SFDP.
Standalone version - It doesn't require the SpiFlashUtils library

An example using `experimental::SPI0Command` function to read SPI Flash Data Parameters from Flash and print a hex dump.

To understand the dump, you will need access to JEDEC documentation. In some cases, the SPI Flash vendor's datasheet will list an explanation of their SFDP data.

The "SFDP" may be used to discern between parts at different revision levels.

Reference:
```
  JEDEC STANDARD Serial Flash Discoverable Parameters
  https://www.jedec.org/standards-documents/docs/jesd216b
  Free Download - requires registration
```



## [d-a-v's EPS8266 pinout (Updated)](https://mhightower83.github.io/esp8266/pinout.html)


## [CompatibleQE](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/CompatibleQE)

**Plan to remove**

For ESP8266EX modules with QE-compatible Flash memory, this example should work.
Compatible Flash memory would be like BergMicro, Winbond, XMC, or others that have the QE bit at S9 in the Flash Status Register and support 16-bit Write Status Register-1.

Other flash memory may require more effort. However, this starts us off with a simple example to introduce the things to looking for.

TODO: I am not sure this example is needed? It is at a lower level.
Maybe the Outline example is enough?



## [ExploreQE](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/ExploreQE)

**Plan to remove**

This is a more detailed example intended for poking around.
It has a menu of options for analyzing and testing various SPI Flash memory.

I think this example needs more work? It still feels a bit complicated to drive.



## [Streaker](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Streaker)

**Plan to remove**

This probably doesn't belong here; however, It need a home for archiving purposes.

Uses a kludged together non-OS wrapper for running some code with the bare necessities.
This is not a true Arduino Sketch. It is closer to a low-level ESP8266 program running with some code fragments from the BootROM, NONOS SDK, and Arduino ESP8266.
Use build option `MMU:"16K Cache + 48K IRAM"`

This unorthodox Sketch generates low jitter signals for viewing on an oscilloscope.
Specifically for viewing ringing, rise, and fall times of GPIO pins.

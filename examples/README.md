# Directory Listing for Example

## [Outline](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Outline)

An outline example of using Reclaim GPIOs

Shows reclaiming GPIOs from preinit() or setup()


## [OutlineCustom](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/OutlineCustom)

Similar to "Outline" above. Illustrates adding support for an additional Flash part. Provides a hypothetical custom handler for EN25QH128A by supplying replacement function: `spi_flash_vendor_cases()`


## [CompatibleQE](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/CompatibleQE)

For compatible SPI Flash memory, the SPI Flash can be setup to ignore GPIO pins 9 and 10 allowing them to be used on ESP-12F modules and NodeMCU dev boards.

Assumes the Flash has compatible QE support

For ESP8266EX with QE-compatible Flash memory, this example should work.
Others will require more effort. However, this starts us off with a simple example.

Compatible Flash memory would be like BergMicro, Winbond, XMC, or others that have the QE bit at S9 in the Flash Status Register and support 16-bit Write Status Register-1.


## [Reclaim_GPIO_9_10](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Reclaim_GPIO_9_10)

For compatible SPI Flash memory, the SPI Flash can be setup to ignore GPIO pins 9 and 10 allowing them to be used on ESP-12F modules and NodeMCU dev boards.

This is a more comprehensive example. It should handle more of the less conforming Flash memories. Like GigaDevice GD25QxxC, GD25QxxE, EON EN25Q32C


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

## [Streaker](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Streaker)

Uses a kludged together non-OS wrapper for running some code with the bare necessities.
This is not a true Arduino Sketch. It is closer to a low-level ESP8266 program running with some code fragments from the BootROM, NONOS SDK, and Arduino ESP8266.
Use build option MMU:"16K Cache + 48K IRAM"

This unorthodox Sketch generates low jitter signals for viewing on an oscilloscope.
Specifically for viewing ringing, rise, and fall times of GPIO pins.
This probably doesn't belong here; however, It need a home for archiving purposes.

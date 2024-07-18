# Directory Listing for Examples

For compatible SPI Flash memory, the SPI Flash can be set up to ignore /WP and /HOLD (GPIO pins 9 and 10) allowing them to be used on ESP-12F modules, NodeMCU DEV boards, and other modules that expose those pins.


## [Analyze](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Analyze)

Evaluate the Flash memory for required attributes.
Reports if the current code supports the device as is.
Prints a copy/paste block for creating a `CustomVender.ino` file like that shown in the example [OutlineCustom](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/OutlineCustom). In OutlineCustom, `CustomVender.ino` is a merged collection of examples.


## [Outline](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Outline)

An outline example demonstrating the use of `reclaim_GPIO_9_10()` which can handle more of the less conforming Flash memories. Like GigaDevice GD25QxxC, GD25QxxE, EON EN25Q32C

Shows reclaiming GPIOs from `preinit()` or `setup()`.

The hope is this will handle a large percentage of the modules exposing GPIO9 and 10.


## [OutlineCustom](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/OutlineCustom)

Similar to "Outline" above. Illustrates adding support for an additional Flash part. Provides a hypothetical custom handler in `CustomVendor.ino` for `spi_flash_vendor_cases()`.

## [OutlineXMC](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/OutlineXMC)

Similar to "OutlineCustom" above. Illustrates the use of SFDP data for tailoring Flash initialization for each revision of the part. Provides a hypothetical custom handler in `CustomXMC.ino` for `spi_flash_vendor_cases()`.

## [Blinky](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Blinky)

A blinking LED example. An adaptation of the example [Outline](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/Outline) using GPIO9 and GPIO10.
You can also add the module `CustomVendor.ino` if needed.



## [SFDPHexDump](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples/SFDPHexDump)

Probe the Flash for SFDP data

For information gathering, reads and dumps the SFDP.
A standalone Sketch that doesn't require the SpiFlashUtils library.

An example using `experimental::SPI0Command` function to read SPI Flash Data Parameters from Flash and print a hex dump.

To understand the dump, you will need access to JEDEC documentation. Sometimes, the SPI Flash vendor's datasheet will explain their SFDP data.

Use "SFDP" to discern between parts at different revision levels.

Reference:
```
  JEDEC STANDARD Serial Flash Discoverable Parameters
  https://www.jedec.org/standards-documents/docs/jesd216b
  Free Download - requires registration
```


## [d-a-v's EPS8266 pinout (Updated)](https://mhightower83.github.io/esp8266/pinout.html)

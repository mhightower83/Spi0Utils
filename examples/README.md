# Example Directory Listing

## [CompatibleQE](https://github.com/mhightower83/SpiFlashUtils/examples/CompatibleQE)

Assumes the Flash has compatible QE support

For ESP8266EX with QE-compatible Flash memory, this example should work.
Others will require more effort. However, this starts us off with a simple example.

Compatible Flash memory would be like BergMicro, Winbond, XMC, or others that have the QE bit at S9 in the Flash Status Register and support 16-bit Write Status Register-1.

## [Reclaim_GPIO_9_10](https://github.com/mhightower83/SpiFlashUtils/examples/Reclaim_GPIO_9_10)

A more comprehensive example. It should handle more of the less conforming Flash memories. Like GigaDevice GD25QxxC, GD25QxxE, EON EN25Q32C

## [SFDPHexDump](https://github.com/mhightower83/SpiFlashUtils/examples/SFDPHexDump)

Standalone version - It doesn't require SpiFlashUtils library

An example using `experimental::SPI0Command` function to read SPI Flash Data Parameters from Flash and print a hex dump.

To understand the dump, you will need access to JEDEC documentation. In some cases, the SPI Flash vendor's datasheet will list an explanation of their SFDP data.

The "SFDP" may be useful for differentiating between some parts.

Reference:
```
  JEDEC STANDARD Serial Flash Discoverable Parameters
  https://www.jedec.org/standards-documents/docs/jesd216b
  Free Download - requires registration
```

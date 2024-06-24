# SPI0 Flash Utilities

## Status: WIP
* working
* code reorganizing, comments need updating

This library was created during the process of working out how to reclaim the use of GPIO pins 9 and 10.
A lot of operations were generalized and put into `ModeDIO_ReclaimGPIOs.h` and `SpiFlashUtils.h`.

For compatible SPI Flash memory, the SPI Flash can be setup to ignore GPIO pins 9 and 10 allowing them to be used on ESP-12F modules and NodeMCU DEV boards.
See [Reclaiming GPIO9 and GPIO10](https://github.com/mhightower83/Arduino-ESP8266-misc/wiki/Pins-GPIO9-and-GPIO10) for a more detailed explanation.

For how to use, see [examples](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples#readme).

This library relies on the use of `experimental::SPI0Command;` in Arduino ESP8266 core. I have a fixed PR pending, https://github.com/esp8266/Arduino/pull/9140#issue-2300765579, which is required for this library to work.

I use BacktraceLog library in most of my projects and examples.
If you don't have it installed, just comment out the offending `#include` line.

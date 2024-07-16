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

For compatible SPI flash memory, the Flash can be setup to ignore GPIO pins 9
and 10 allowing them to be used on ESP-12F modules and NodeMCU DEV boards.
Boards that support SPI Mode: "QIO" should also work with SPI Mode: "DIO"
combined with `reclaim_GPIO_9_10()`. Boards that support at best SPI Mode: "DIO"
might work with `reclaim_GPIO_9_10()`.

TODO: Phase out this link.
See [Reclaiming GPIO9 and GPIO10](https://github.com/mhightower83/Arduino-ESP8266-misc/wiki/Pins-GPIO9-and-GPIO10)
for a more detailed explanation.

Two main steps:
1. Run the "Analyze" example on your module or development board. It will
   evaluate if the SPI flash memory on your board supports turning off pin
   functions /WP and /HOLD. "Analyze" will also confirm if builtin support is
   already present in the library, and suggest copy/paste code to handle your
   flash memory. If needed, add the copy/paste code to a `CustomVendor.ino` file
   in your sketch folder. (How often does this work? - unknown, it works with my devices)

1. Call `reclaim_GPIO_9_10()` from your Sketch startup code, either `preinit()`
   or `setup()`. Perform additional setup as needed.

See [example Sketches](https://github.com/mhightower83/SpiFlashUtils/tree/master/examples#readme)
for more details.

When using GPIO9 and GPIO10, be mindful these pins are driven high during boot.
And stay high until reclaimed then they are setup with `pinMode(, INPUT)` like
other Arduino GPIO pins.

TODO: delete from here down later
I have a fix PR pending, https://github.com/esp8266/Arduino/pull/9140#issue-2300765579,
which is required for this library to work.

I use BacktraceLog library in most of my projects and examples.
If you don't have it installed, just comment out the offending `#include` line.

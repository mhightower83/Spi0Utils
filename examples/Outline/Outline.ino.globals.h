/*@create-file:build.opt@

// Use these to disable options, default to enable, 1.
// -DBUILTIN_SUPPORT_GIGADEVICE=0
// -DBUILTIN_SUPPORT_MYSTERY_VENDOR_D8=0
// -DBUILTIN_SUPPORT_SPI_FLASH__S6_QE_WPDIS=0
// -DBUILTIN_SUPPORT_SPI_FLASH_VENDOR_EON=0

// These options default to 0 or undefined.

// Enable when testing the Flash memory on you ESP module for compatibility
// with this method of reclaimming GPIO9 and GPIO10.
// "write once, debug everywhere"
-DDEBUG_FLASH_QE=1

// Alter library prints to work from preinit()
// -DRECLAIM_GPIO_EARLY=1

// For shareing preinit with BacktraceLog
-DSHARE_PREINIT__DEBUG_ESP_BACKTRACELOG="backtaceLog_preinit"
*/


/*@create-file:build.opt:debug@

// Use these to disable options, default to enable, 1.
// -DBUILTIN_SUPPORT_GIGADEVICE=0
// -DBUILTIN_SUPPORT_MYSTERY_VENDOR_D8=0
// -DBUILTIN_SUPPORT_SPI_FLASH__S6_QE_WPDIS=0
// -DBUILTIN_SUPPORT_SPI_FLASH_VENDOR_EON=0

// These options default to 0 or undefined.

// Enable when testing the Flash memory on you ESP module for compatibility
// with this method of reclaimming GPIO9 and GPIO10.
// "write once, debug everywhere"
-DDEBUG_FLASH_QE=1

// Alter library prints to work from preinit()
// -DRECLAIM_GPIO_EARLY=1

// For shareing preinit with BacktraceLog
-DSHARE_PREINIT__DEBUG_ESP_BACKTRACELOG="backtaceLog_preinit"
*/

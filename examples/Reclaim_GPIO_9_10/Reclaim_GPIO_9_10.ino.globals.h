/*@create-file:build.opt@

// Use these to disable options, default to enable, 1.
// -DSUPPORT_SPI_FLASH__S6_QE_WPDIS=0
// -DSUPPORT_MYSTERY_VENDOR_D8=0

// These options default to 0 or undefined.

// Enable when testing the Flash memory on you ESP module for compatibility
// with this method of reclaimming GPIO9 and GPIO10.
// "write once, debug everywhere"
-DDEBUG_FLASH_QE=1

// Early as in at preinit()
// -DRECLAIM_GPIO_EARLY=1

// For shareing preinit with BacktraceLog
-DSHARE_PREINIT__DEBUG_ESP_BACKTRACELOG="backtaceLog_preinit"
*/


/*@create-file:build.opt:debug@

-DSHARE_PREINIT__DEBUG_ESP_BACKTRACELOG="backtaceLog_preinit"
*/

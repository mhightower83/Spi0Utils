/*@create-file:build.opt@

// BUILD_OPTION_FLASH_SAFETY_OFF defaults to on. uncomment to disable
// -DBUILD_OPTION_FLASH_SAFETY_OFF=1

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

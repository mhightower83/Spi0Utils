/*@create-file:build.opt@

//
-DRUN_SCRIPT_AT_BOOT=1

// Enable when using Analyze for testing the Flash memory on you ESP module for
// compatibility with reclaimming GPIO9 and GPIO10 methods.
// Outside of the Analyze sketch, it can provide insite when things don't work
// as expected. When finished with debugging, remove define to reduce code and
// DRAM footprint.
//
-DDEBUG_FLASH_QE=1

// Build flag indicates that reclaim_GPIO_9_10() may be called before C++
// runtime has been called as in preinit(). Works with "-DDEBUG_FLASH_QE=1" to
// allow debug printing before C++ runtime.
//
// -DRECLAIM_GPIO_EARLY=1

// For shareing preinit with BacktraceLog
// -DSHARE_PREINIT__DEBUG_ESP_BACKTRACELOG="backtaceLog_preinit"
*/


/*@create-file:build.opt:debug@

//
-DRUN_SCRIPT_AT_BOOT=1

// Enable when using Analyze for testing the Flash memory on you ESP module for
// compatibility with reclaimming GPIO9 and GPIO10 methods.
// Outside of the Analyze sketch, it can provide insite when things don't work
// as expected. When finished with debugging, remove define to reduce code and
// DRAM footprint.
//
-DDEBUG_FLASH_QE=1

// Build flag indicates that reclaim_GPIO_9_10() may be called before C++
// runtime has been called as in preinit(). Works with "-DDEBUG_FLASH_QE=1" to
// allow debug printing before C++ runtime.
//
// -DRECLAIM_GPIO_EARLY=1

// For shareing preinit with BacktraceLog
-DSHARE_PREINIT__DEBUG_ESP_BACKTRACELOG="backtaceLog_preinit"
*/

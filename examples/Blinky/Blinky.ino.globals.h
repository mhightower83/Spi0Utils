/*@create-file:build.opt@

// Enable when testing the Flash memory on you ESP module for compatibility
// with reclaimming GPIO9 and GPIO10 methods. It can provide insite when things
// don't work as expected. When finished with debugging, remove define to reduce
// code and DRAM footprint.
//
-DDEBUG_FLASH_QE=1

// A debug build flag for reclaim_GPIO_9_10() when called from preinit().
// Allows the library to handle early printing before the C++ runtime has been
// completed. This is for use with the option "-DDEBUG_FLASH_QE=1".
//
-DRECLAIM_GPIO_EARLY=1

*/

/*@create-file:build.opt@

//
-DRUN_SCRIPT_AT_BOOT=1

// Enable when using Analyze for testing the flash memory on you ESP module for
// compatibility with reclaimming GPIO9 and GPIO10 methods.
// Outside of the Analyze sketch, it can provide insite when things don't work
// as expected. When finished with debugging, remove define to reduce code and
// DRAM footprint.
//
-DDEBUG_FLASH_QE=1

// A debug build flag for reclaim_GPIO_9_10() when called from preinit().
// Allows the library to handle early printing before the C++ runtime has been
// completed. This is for use with the option "-DDEBUG_FLASH_QE=1".
//
// -DRECLAIM_GPIO_EARLY=1

*/


/*@create-file:build.opt:debug@

//
-DRUN_SCRIPT_AT_BOOT=1

// Enable when using Analyze for testing the flash memory on you ESP module for
// compatibility with reclaimming GPIO9 and GPIO10 methods.
// Outside of the Analyze sketch, it can provide insite when things don't work
// as expected. When finished with debugging, remove define to reduce code and
// DRAM footprint.
//
-DDEBUG_FLASH_QE=1

// A debug build flag for reclaim_GPIO_9_10() when called from preinit().
// Allows the library to handle early printing before the C++ runtime has been
// completed. This is for use with the option "-DDEBUG_FLASH_QE=1".
//
// -DRECLAIM_GPIO_EARLY=1

*/

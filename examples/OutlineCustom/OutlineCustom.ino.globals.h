/*@create-file:build.opt@

// Enable when testing the Flash memory on you ESP module for compatibility
// with this method of reclaimming GPIO9 and GPIO10.
// "write once, debug everywhere"
-DDEBUG_FLASH_QE=1

// Alters library prints to work from preinit()
// -DRECLAIM_GPIO_EARLY=1

*/


/*@create-file:build.opt:debug@

// Enable when testing the Flash memory on you ESP module for compatibility
// with this method of reclaimming GPIO9 and GPIO10.
// "write once, debug everywhere"
-DDEBUG_FLASH_QE=1

// Alters library prints to work from preinit()
// -DRECLAIM_GPIO_EARLY=1

*/

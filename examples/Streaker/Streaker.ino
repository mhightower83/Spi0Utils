/*
  This example was thrown together to generate some low jitter signal for
  viewing on an oscilloscope. Specifically for viewing rise and fall times of
  GPIO pins connected to a SPI Flash pin.



  A kludged together non-OS wrapper for running some code with the bare necessities.
  Use build option MMU:"16K Cache + 48K IRAM"

  Intended use is for generating no-jitter simple periodic waveforms
  also suitable for examining rise times etc. No interruptions.

  The DRAM Heap is available
  No WiFi
  No WDT
  No SDK init - still taking up some room - there are a few APIs that work w/o init
  No debugger
  No postmortem - panic may work.


  Notes
  setup2() amd loop2() run without the benifit of "C" runtime initialization
  the NONOS SDK has not been initialized.

  Best to avoid "C++" functions and stick to "C"
  Don't expect structures to be initialized, etc.

  Update: I added the BSS zero and copied over the global_ctors init. I have no
  experience with this addition. I should probably stop with this. If I keep
  adding stuff, I will slowly build an SDK.

*/
// #include <mmu_iram.h>
#include <umm_malloc/umm_malloc.h>
#include <esp8266_undocumented.h>
// #include <BootROM_NONOS.h>
// #include <SpiFlashUtils.h>
// #include <FlashChipId_D8.h>
#include <ModeDIO_ReclaimGPIOs.h>


extern "C" void mmu_set_pll(void);

#define ETS_PRINTF ets_uart_printf
#if 1
#define DBG_PRINTF ets_uart_printf
#else
#define DBG_PRINTF(...) do {} while (false)
#endif

////////////////////////////////////////////////////////////////////////////////
// We still need these defined - I did say this was a kludge.
void setup() {}
void loop() {}



////////////////////////////////////////////////////////////////////////////////
// C and C++ runtime init borrowed from core_esp8266_main.cpp and eboot.c
// Not much testing with this
// Take extra care to avoid SDK APIs and libraries using SDK APIs.
extern "C" {
  struct object { long placeholder[ 10 ]; };
  void __register_frame_info (const void *begin, struct object *ob);
  extern char __eh_frame[];
};
extern void (*__init_array_end)(void);
extern void (*__init_array_start)(void);

static void do_global_ctors(void) {
  static struct object ob;
  __register_frame_info( __eh_frame, &ob );

  void (**p)(void) = &__init_array_end;
  while (p != &__init_array_start)
      (*--p)();
}

extern "C" {
  extern char _bss_start;
  extern char _bss_end;
  void ets_bzero (void*, size_t);
};

static void c_runtime_init(void) {
  ets_bzero(&_bss_start, &_bss_end - &_bss_start); // Clear BSS

  // I am not sure about this one. I really don't know enough. I am concerned
  // that this might do too much and call the NONOS SDK.
  do_global_ctors();
}

void IRAM_ATTR delay(unsigned long ms) {
  ets_delay_us(1000u * ms); // limit: 53,687 ms MAX
}



////////////////////////////////////////////////////////////////////////////////
// Bare essentials replacement for main loop
void run_now(void) {
  // For hard reset low clock rates, correct printing at 115200 bps by
  // setting PLL clock early.
  // Side effect, this updated rate corrects the periphereal clock rate for
  // a 26MHz crystal from 52 MHz to 80 MHz effect.
  // The BootROM starts the the Flash SPI0 at a 20 MHz clock rate.
  // Which is really 14 MHz for a 26 MHz crystal until NONOS SDK does PLL calibration.
  // We have no NONOS_SDK - do this now.
  mmu_set_pll();
  // Serial port speed now 115200 bps
  uart_buff_switch(0);
  c_runtime_init();

  // Run with no SDK support.
  setup2();
  while(true) {
    loop2();
  }
}



////////////////////////////////////////////////////////////////////////////////
// starter code copied from core_esp8266_main.cpp
extern "C" void call_user_start();

extern "C" void IRAM_ATTR app_entry_redefinable(void) {
    // /* Allocate continuation context on this SYS stack,
    //    and save pointer to it. */
    // cont_t s_cont __attribute__((aligned(16)));
    // g_pcont = &s_cont;

    // No need for a separate stack space.
    // We should have almost 8KB of stack.
    g_pcont = NULL;

    /* Doing umm_init just once before starting the SDK, allowed us to remove
    test and init calls at each malloc API entry point, saving IRAM. */
#ifdef UMM_INIT_USE_IRAM
    umm_init();
#else
    // umm_init() is in IROM
    mmu_wrap_irom_fn(umm_init);
#endif
    // Run IROM code with 16KB ICACHE
    mmu_wrap_irom_fn(run_now);  // Never returns

    // No NONOS SDK APIs!
    // call_user_start();
}

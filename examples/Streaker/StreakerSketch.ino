////////////////////////////////////////////////////////////////////////////////
//
#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE inline __attribute__ ((always_inline))
#endif

static void ALWAYS_INLINE _delayCycles(uint32_t delay_count) {
    // ~  3 -  2,  87.5ns,  7 cycles, +6 => +75ns
    // ~  9 -  8, 162.5ns, 13 cycles,
    // ~ 10 - 14, 237.5ns, 19 cycles, +75ns
    // ~ 16 - 20, 312.5ns, 25 cycles, +75ns
    // ~ 22 - 26, 387.5ns, 31 cycles, +75ns
    // ~ 28 - 32, 462.5ns, 37 cycles, +75ns
    uint32_t total, time;
    __asm__ __volatile__ (
        "rsr.ccount %[time]\n\t"
        "extw\n\t"      // 1 or more cycles, maybe 3
      "1:\n\t"          // This loop is 6 cycles long with unavoidable pipeline stalls.
        "rsr.ccount %[total]\n\t"
        "sub        %[total], %[total], %[time]\n\t"
        "blt        %[total], %[delay], 1b\n\t"
        : [total]"=&r"(total), [time]"=&r"(time)
        : [delay]"r"(delay_count)
    );
}

static void ALWAYS_INLINE _delay12D5nsNop() {
    __asm__ __volatile__ (
        "nop.n\n\t"
    );
}

static void ALWAYS_INLINE _delay25nsNop() {
    __asm__ __volatile__ (
        "nop.n\n\t"
        "nop.n\n\t"
    );
}

static void ALWAYS_INLINE _delay37D5nsNop() {
    __asm__ __volatile__ (
        "nop.n\n\t"
        "nop.n\n\t"
        "nop.n\n\t"
    );
}

static void ALWAYS_INLINE _delay50nsNop() {
    _delay25nsNop();
    _delay25nsNop();
}

static void ALWAYS_INLINE _delay100nsNop() {
    _delay50nsNop();
    _delay50nsNop();
}


///////////////////////////////////////////////////////////////////////////////
//
bool reclaimed_gpio = false;

void setup2() {
  ETS_PRINTF("\n\n\n\nAnd it begins ...\n\n");
  #if 0
  // Quick test to confirm that malloc works.
  ETS_PRINTF("%u = umm_free_heap_size()\n", umm_free_heap_size());
  uint8_t *buf = (uint8_t *)malloc(32 * 1024);
  ETS_PRINTF("uint8_t *buf(%p) = (uint8_t *)malloc(32 * 1024);\n", buf);
  ETS_PRINTF("%u = umm_free_heap_size()\n", umm_free_heap_size());
  if (buf) free(buf);
  ETS_PRINTF("%u = umm_free_heap_size()\n", umm_free_heap_size());
  #endif

  // Initialize GPIOs
  reclaimed_gpio = reclaim_GPIO_9_10();
  if (reclaimed_gpio) {
    ETS_PRINTF("\nreclaim_GPIO_9_10 success!\n");
    // Drive the pin with a square wave to evaluate rise time for a
    // capacitive loaded pin with pullup resistor.

    // The datasheets have COUT, CIN, and CL.
    //
    // COUT and CIN are the capacitance contributed by the Flash during these
    // operations. The capacitive load of a Flash input pin is ~6pF. The
    // contrabution should be negligible to the circuit the GPIO pin is
    // connected to. However, when calculating the capacitive loading for I2C it
    // should be entered.
    //
    // CL is the max capacitive load (typical 30pF) a SPI Flash memory output
    // can drive and meet the specification for rise/fall time.
    pinMode( 0, INPUT);
//  pinMode( 1, SPECIAL);   // TX0
    pinMode( 2, OUTPUT);    // TX1 and Blue LED
//  pinMode( 3, SPECIAL);   // RX0
    pinMode( 4, INPUT);
    pinMode( 5, OUTPUT);
    pinMode( 9, OUTPUT);
    pinMode(10, OUTPUT);
    pinMode(12, INPUT);
    pinMode(13, INPUT);
    pinMode(14, INPUT);
    pinMode(15, INPUT);
    pinMode(16, OUTPUT);
    digitalWrite( 2, HIGH);
    digitalWrite(16, HIGH);
    #if 0
    pinMode( 5, OUTPUT_OPEN_DRAIN);
    pinMode(10, OUTPUT_OPEN_DRAIN);
    #endif
    // digitalWrite( 2, LOW);  // Blue LED On
    digitalWrite( 9, LOW);
    digitalWrite(10, LOW);
  } else {
    ETS_PRINTF("\n\nsetup2() failed!\n\n");
  }
}


///////////////////////////////////////////////////////////////////////////////
//
void loop2() {
  // Flashing Blue light when 'reclaim_GPIO_9_10()' fails.
  digitalWrite( 2, digitalRead(2) ? LOW : HIGH);  // LED toggle
  if (reclaimed_gpio) {
    // Waveform generating coded
    // After one pass, everything should run from cache.
    // Syncronize GPIO5 and GPIO10
    uint32_t pinMask = (1u << 5u) | (1u << 10u);
    while (true) {
#if 0
      // 1MHz - 50/50 duty cycles 1MHz +/- 1
      // The delay times add up, the frequency is perfect, and the duty cycle;
      // however, I don't understand the lopsided distribution that is needed to
      // get 50/50.
      // TODO: go back and experiment with
      //     asm volatile("excw\n\t"); // ensure previous I/O has finished
      //     A note from other projects, back-to-back IO taking 87.5ns each,
      //     that is more than the 25ns I estamated for 'GPOC = pinMask' ??
      //
      //                     loop +12.5ns
      _delay12D5nsNop();
      _delay50nsNop();
      _delayCycles(24);   // 387ns   +/-6 >> +/-75ns
      GPOC = pinMask;     // Clear - LOW, +25ns
      _delay25nsNop();
      _delay25nsNop();
      _delay50nsNop();
      _delayCycles(24);   // 387ns
      GPOS = pinMask;     // Set - HIGH, +25ns
#elif 0
  // Check if this works as well as above
      GPOC = pinMask;     // Clear - LOW, +25ns
      _delayCycles(24);   // 387ns
      _delay25nsNop();
      _delay25nsNop();
      _delay50nsNop();
      GPOS = pinMask;     // Set - HIGH, +25ns
      _delayCycles(24);   // 387ns   +/-6 >> +/-75ns
      _delay12D5nsNop();
      _delay50nsNop();
#elif 0
      // 100KHz - needs testing
      //                     loop +12.5ns
      _delayCycles(390);  // 4,962.5ns   +/-6 >> +/-75ns
      GPOC = pinMask;     // +25ns
      _delay12D5nsNop();
      _delayCycles(390);  // 4,962.5ns   +/-6 >> +/-75ns
      GPOS = pinMask;     // +25ns
#elif 1
      // 100KHz - needs testing
      //                     loop +12.5ns
      _delay12D5nsNop();
      _delay50nsNop();
      _delayCycles(384);  // 4,887.5ns   +/-6 >> +/-75ns
      GPOC = pinMask;     // +25ns
      _delay25nsNop();
      _delay25nsNop();
      _delay50nsNop();
      _delayCycles(384);  // 4,887.5ns   +/-6 >> +/-75ns
      GPOS = pinMask;     // +25ns
#endif
// #elif 0
//       // 1MHz - confirmed - not 50/50
//       //                     loop +12.5ns
//       _delayCycles(30);   // 462.5ns   +/-6 >> +/-75ns
//       GPOC = pinMask;     // Clear - LOW, +25ns
//       _delay12D5nsNop();  //
//       _delayCycles(30);   // 462.5ns
//       GPOS = pinMask;     // Set - HIGH, +25ns
// #else
//       // 1MHz - locked
//       _delay25nsNop();
//       _delay50nsNop();
//       _delayCycles(24); // 462ns   +/-6 >> +/-75ns
//       GPOC = pinMask;   // Clear - LOW
//       _delay12D5nsNop();
//       _delay25nsNop();
//       _delay50nsNop();
//       _delayCycles(24); // 462ns
//       GPOS = pinMask;   // Set - HIGH
// #endif
    }
  }
  delay(250u);
}

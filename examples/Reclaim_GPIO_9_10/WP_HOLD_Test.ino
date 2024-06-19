/*
  Test that pin function /WP can be disabled
  Returns true on a successful write

  Uses last sector of code space for test writing.
  Currently uses byte 0 of sector to test

  Test that pin function /HOLD can be disabled
  If it can return it worked.

  Notes:
    GPIO9 may work because the SPI Flash chip does not implement a /HOLD. eg. EN25Q32C
    GPIO10 may work because other Status Register Bits indicate not to use /WP.
*/

////////////////////////////////////////////////////////////////////////////////
//
// For QE/S9 case set SR1, and SR2 such that SRP0=1 and SRP1=0
// preserve QE/S9 and clear all other bits.
//
// This causes the pin function /WP to be enabled on some devices for non-quad
// instructions regardless of QE=1 state.
//
bool test_set_SRP0(Print& oStream, uint32_t qe_bit, bool use_16_bit_sr1, bool non_volatile, [[maybe_unused]] bool was_preset) {
  bool ok = false;
  spi0_flash_write_disable(); // For some devices, EN25Q32C, this clears OTP mode.
  if (9u == qe_bit) {
    digitalWrite(10, HIGH);     // ensure /WP is not asserted
    pinMode(10, OUTPUT);
    uint32_t sr1 = 0;
    uint32_t sr2 = 0;
    spi0_flash_read_status_register_1(&sr1);
    spi0_flash_read_status_register_2(&sr2);
    if (BIT7 == (BIT7 & sr1)) {
      oStream.printf_P(PSTR("  SRP0 already set.\n"));
    }
    sr1 = BIT7;   // SRP1 must be zero to avoid permanently protected!
    sr2 &= BIT1;  // Keep QE bit, clear all others.
    if (use_16_bit_sr1) {
      sr1 |= sr2 << 8;
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 16);
    } else {
      spi0_flash_write_status_register(/* SR2 */ 1, sr2, non_volatile, 8);
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 8);
      // Just in case the order was wrong.
      spi0_flash_write_status_register(/* SR2 */ 1, sr2, non_volatile, 8);
    }
    // Verify
    sr1 = 0;
    spi0_flash_read_status_register_1(&sr1);
    ok = BIT7 == (BIT7 & sr1);
    digitalWrite(10, SPECIAL);
  }
  return ok;
}

////////////////////////////////////////////////////////////////////////////////
//
// For QE/S9 case clear SR1 and SR2, preserve QE/S9
//
// This is needed to completely disable pin function /WP on some devices for
// non-quad instructions. On some devices QE=1 was not enough. Winbond,
// BergMicro, XMC.
//
bool test_clear_SR1(Print& oStream, uint32_t qe_bit, bool use_16_bit_sr1,
  bool non_volatile, [[maybe_unused]] bool was_preset) {
  bool ok = false;
  spi0_flash_write_disable();
  if (9u == qe_bit) {
    digitalWrite(10, HIGH);
    pinMode(10, OUTPUT);      // was /WP
    uint32_t sr1 = 0;
    uint32_t sr2 = 0;
    spi0_flash_read_status_register_2(&sr2);
    sr2 &= BIT1; // Keep QE bit clear all others.
    if (use_16_bit_sr1) {
      sr1 |= sr2 << 8;
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 16);
    } else {
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 8);
      spi0_flash_write_status_register(/* SR2 */ 1, sr2, non_volatile, 8);
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 8);
    }
    // Verify
    spi0_flash_read_status_register_1(&sr1);
    ok = 0u == (sr1 & 0xFCu);
    digitalWrite(10, SPECIAL);
  }
  return ok;
}

////////////////////////////////////////////////////////////////////////////////
//
// was_preset == false, Sets the proposed QE bit and
// verifies that the bit is set returns true if set.
//
// was_preset == true, check current QE bit and
// returns true when QE is already set
//
bool test_set_QE(Print& oStream, uint32_t qe_bit, bool use_16_bit_sr1, bool non_volatile, bool was_preset) {
  spi0_flash_write_disable();
  bool qe = false;

  if (was_preset) {
    uint32_t sr1 = 0;
    uint32_t sr2 = 0;
    if (9u == qe_bit) {
      spi0_flash_read_status_register_2(&sr2);
      qe = BIT1 == (BIT1 & sr2);
    } else
    if (6u == qe_bit) {
      spi0_flash_read_status_register_1(&sr1);
      qe = BIT6 == (BIT6 & sr1);
    }
    oStream.printf_P(PSTR("  QE/S%u=%u used\n"), qe_bit, (qe) ? 1u : 0u);
    return qe;
  }

  if (9u == qe_bit) {
    uint32_t sr2 = 0;
    spi0_flash_read_status_register_2(&sr2); // flash_gd25q32c_read_status(/* SR2 */ 1);
    qe = BIT1 == (BIT1 & sr2);
    if (qe) {
      oStream.printf_P(PSTR("  QE/S%u already set.\n"), qe_bit);
    } else {
      sr2 |= BIT1;    // S9
      if (use_16_bit_sr1) {
        uint32_t sr1 = 0;
        spi0_flash_read_status_register_1(&sr1);
        sr1 |= sr2 << 8;
        spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 16);
      } else {
        spi0_flash_write_status_register(/* SR2 */ 1, sr2, non_volatile, 8);
      }
      // Verify
      sr2 = 0;
      spi0_flash_read_status_register_2(&sr2); // flash_gd25q32c_read_status(/* SR2 */ 1);
      qe =  BIT1 == (BIT1 & sr2);
    }
  } else
  if (6u == qe_bit) {
    uint32_t sr1 = 0;
    spi0_flash_read_status_register_1(&sr1);
    qe = BIT6 == (BIT6 & sr1);
    if (qe) {
      oStream.printf_P(PSTR("  QE/S%u already set.\n"), qe_bit);
    } else {
      sr1 |= BIT6;
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 8);
      // verify
      sr1 = 0;
      spi0_flash_read_status_register_1(&sr1);
      qe = BIT6 == (BIT6 & sr1);
    }
  }
  return qe;
}

////////////////////////////////////////////////////////////////////////////////
//
// Lets try and test /WP feature by setting and clearing a BP0 bit in SR1.
//
// For QE/S9 with SR1 and SR2, with 8 or 16-bit writes or
// For QE/S6 with SR1 only
//
// Only allow QE bit, SRP0, and PM0 to be set. All other are 0.
//
bool testFlashWrite([[maybe_unused]] Print& oStream, uint32_t qe_bit, bool use_16_bit_sr1, bool non_volatile) {
  bool test = false;
  spi0_flash_write_disable();

  if (9 == qe_bit) {
    // bool non_volatile = true;
    uint32_t sr1 = 0;
    uint32_t sr2 = 0;
    spi0_flash_read_status_register_1(&sr1);
    spi0_flash_read_status_register_2(&sr2);
    sr1 &= BIT7;  // Keep SRP0 asis
    sr1 |= BIT2;  // set PM0
    sr2 &= BIT1;  // Keep QE/S9
    if (use_16_bit_sr1) {
      sr1 |= sr2 << 8u;
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 16);
    } else {
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 8);
    }
    uint32_t verify_sr1 = 0;
    spi0_flash_read_status_register_1(&verify_sr1);
    test = BIT2 == (BIT2 & verify_sr1);
    sr1 &= ~BIT2;   // clear PM0
    if (use_16_bit_sr1) {
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 16);
    } else {
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 8);
    }
  } else
  if (6 == qe_bit) {
    // No SRP0 or SRP1
    uint32_t sr1 = 0;
    spi0_flash_read_status_register_1(&sr1);
    sr1 &= BIT6;  // QE/S6 or WPDis
    sr1 |= BIT2;  // set PM0
    spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 8);
    uint32_t verify_sr1 = 0;
    spi0_flash_read_status_register_1(&verify_sr1);
    test = BIT2 == (BIT2 & verify_sr1);
    sr1 &= ~BIT2;
    spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 8);
  }

  return test;
}

///////////////////////////////////////////////////////////////////////////////
//
// Verify that Flash pin function /WP (shared with GPIO10) can be disabled.
//
bool testOutputGPIO10(Print& oStream, uint32_t qe_bit, bool use_16_bit_sr1, bool non_volatile, bool was_preset) {
  oStream.printf_P(PSTR("\nRunning verification test for pin function /WP disable\n"));
  oStream.printf_P(PSTR("and confirm pin is ignored on flash writes\n"));
  if (9u != qe_bit && 6u != qe_bit) {
    oStream.printf_P(PSTR("* QE/S%u bit field specification undefined should be either S6 or S9\n"), qe_bit);
    return false;
  }
  bool has_QE = test_set_QE(oStream, qe_bit, use_16_bit_sr1, non_volatile, was_preset);
  uint32_t good_count = 0;

  oStream.printf_P(PSTR("  Test Write: QE/S%u=%c, GPIO10 as OUTPUT\n"), qe_bit, (has_QE) ? '1' : '0');
  pinMode(10, OUTPUT);      // was /WP
  digitalWrite(10, HIGH);
  bool success = testFlashWrite(oStream, qe_bit, use_16_bit_sr1, non_volatile);
  if (success) good_count++;
  // Expect success regardless of QE
  bool pass = success; //(success && has_QE) || (!success && !has_QE);
  oStream.printf_P(PSTR("%c Test Write: With /WP set HIGH write %s\n"), (pass) ? ' ' : '*', (success) ? "succeeded" : "failed.");

  digitalWrite(10, LOW);
  success = testFlashWrite(oStream, qe_bit, use_16_bit_sr1, non_volatile);
  if (success && has_QE) good_count++;
  // Expect success if QE=1, failure otherwise
  pass = (success && has_QE) || (!success && !has_QE);
  oStream.printf_P(PSTR("%c Test Write: With /WP set LOW write %s\n"), (pass) ? ' ' : '*', (success) ? "succeeded" : "failed.");
  //D if (!pass) printAllSRs(oStream);
  if (!success && has_QE) oStream.printf_P(PSTR("  Test Write: QE does not disable pin function /WP or QE/S%u is the wrong bit.\n"), qe_bit);

  if (2 == good_count) {
    oStream.printf_P(PSTR("  Test Write: Pass - confirmed QE disabled pin function /WP.\n"));
    return true;
  }
  oStream.printf_P(PSTR("  Test Write: Fail - unexpected results.\n"));
  return false;
}

void testOutputGPIO9(Print& oStream, uint32_t qe_bit, bool use_16_bit_sr1, bool non_volatile, bool was_preset) {
  oStream.printf_P(PSTR("\nRunning verification test for pin function /HOLD disable\n"));
  if (9u != qe_bit && 6u != qe_bit) {
    oStream.printf_P(PSTR("* QE/S%u bit field specification undefined should be either S6 or S9\n"), qe_bit);
    return;
  }

  bool has_QE = test_set_QE(oStream, qe_bit, use_16_bit_sr1, non_volatile, was_preset);
  oStream.printf_P(PSTR("  Verify /HOLD is disabled by Status Register QE/S%u=%u\n"), qe_bit, (has_QE) ? 1u : 0);
  oStream.printf_P(PSTR("  If changing GPIO9 to OUTPUT and setting LOW crashes, it failed.\n"));
  pinMode(9u, OUTPUT);
  digitalWrite(9u, LOW);
  if (has_QE) {
    oStream.printf_P(PSTR("  passed - QE/S%u bit setting worked.\n"), qe_bit); // No WDT Reset - then it passed.
  } else {
    oStream.printf_P(PSTR("* Ambiguous results. QE=0 and we did not crash. Flash may not support /HOLD.\n"));
  }
}

void testInput_GPIO9_GPIO10(Print& oStream, uint32_t qe_bit, bool use_16_bit_sr1, bool non_volatile, bool was_preset) {
  oStream.printf_P(PSTR("\nRunning GPIO9 and GPIO10 INPUT test\n"));
  oStream.printf_P(PSTR("Test GPIO9 and GPIO10 by reading from the pins as INPUT and print result.\n"));
  if (9u != qe_bit && 6u != qe_bit) {
    oStream.printf_P(PSTR("* QE/S%u bit field specification undefined should be either S6 or S9\n"), qe_bit);
    return;
  }
  bool has_QE = test_set_QE(oStream, qe_bit, use_16_bit_sr1, non_volatile, was_preset);
  oStream.printf_P(PSTR("  Test: QE/S%u=%u, GPIO pins 9 and 10 as INPUT\n"), qe_bit, (has_QE) ? 1u : 0);
  pinMode(9, INPUT);
  pinMode(10, INPUT);
  uint32_t pin9 = digitalRead(9);
  oStream.printf_P(PSTR("  digitalRead result: GPIO_9(%u) and GPIO_10(%u)\n"), pin9, digitalRead(10));

  if (0 == pin9){
    if (has_QE) {
      oStream.printf_P(PSTR("  Passed - no crash\n")); // No WDT Reset - then it passed.
    } else {
      oStream.printf_P(PSTR("* Ambiguous results. QE=0 and we did not crash. Flash may not support /HOLD.\n"));
    }
  }
}

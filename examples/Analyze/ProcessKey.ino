


////////////////////////////////////////////////////////////////////////////////
// Modal states
bool qe_was_preset = false;
bool write_status_register_16_bit = true;
uint32_t modal_qe_bit = 0xFFu;

static void resetModalValues() {
  qe_was_preset = false;
  modal_qe_bit = 0xFFu;
  write_status_register_16_bit = true;
}

////////////////////////////////////////////////////////////////////////////////
//
void printSR321(const char *indent) {
  using namespace experimental;

  uint32_t status = 0;
  SpiOpResult ok0 = spi0_flash_read_status_registers_3B(&status);
  if (SPI_RESULT_OK == ok0) {
    Serial.PRINTF_LN("%sSR321 0x%06X", indent, status);
    if (kWELBit  == (status & kWELBit))  Serial.PRINTF_LN("%s  WEL=1", indent);
    if (9u == modal_qe_bit) {
      if (kQES9Bit2B == (status & kQES9Bit2B)) Serial.PRINTF_LN("%s  QE/S9=1", indent);
      if (BIT7 == (BIT7 & status))  Serial.PRINTF_LN("%s  SRP0=1", indent);
      if (BIT8 == (BIT8 & status))  Serial.PRINTF_LN("%s  SRP1=1", indent);
    } else
    if (6u == modal_qe_bit) {
      if (kQES6Bit == (kQES6Bit & status)) Serial.PRINTF_LN("%s  QE/S6=1", indent);
    }
  } else {
    Serial.PRINTF_LN("* spi0_flash_read_status_registers_3B() failed!");
  }
}

////////////////////////////////////////////////////////////////////////////////
// WEL is an essential feature for our tests.
// Confirm that the SPI Flash Status Register supports a WEL (Write-Enable-Latch) bit
bool confirmWelBit() {
  using namespace experimental;

  spi0_flash_write_disable();
  if (! is_WEL()) {
    spi0_flash_write_enable();
    if (is_WEL()) {
      spi0_flash_write_disable();
      if (! is_WEL()) {
        return true;
      } else {
        Serial.PRINTF_LN("  WEL bit failed to clear");
      }
    } else {
      Serial.PRINTF_LN("  WEL bit failed to set");
    }
  } else {
    Serial.PRINTF_LN("  WEL bit stuck on");
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Analyze Status Registers available and write modes.
//
// WEL, Write Enable Latch, is turned on with the SPI instruction 06h. Most
// write instructions require the WEL bit set in the status register before a
// write operation. Once WEL is set, the next write instruction will clear it.
// If the write instruction is not valid, the WEL bit is still set. We rely on
// WEL bit left on when a status register is not supported. Or the bit length of
// the instruction is wrong for this Flash device.
//
// Thus we can detect if the register exist and the required length of the write.
//
bool test_sr_8_bit_write(uint32_t reg_0idx) {
  using namespace experimental;

  // No change write
  // Note NONOS_SDK function `flash_gd25q32c_read_status` has a bug where it
  // needlessly enables WEL This is not an issue for us because it is followed
  // by a flash_gd25q32c_write_status call.
  uint32_t status = flash_gd25q32c_read_status(reg_0idx);
  flash_gd25q32c_write_status(reg_0idx, status); // 8-bit status register write
  bool success = true;
  if (is_WEL()) {
    ETS_PRINTF("* WEL left set after %u-bit Status Register-%u write.\n", 8u, reg_0idx + 1u);
    success = false;
  } else {
    ETS_PRINTF("  Pass - %u-bit Write Status Register-%u.\n", 8u, reg_0idx + 1u);
  }

  uint32_t new_status = flash_gd25q32c_read_status(reg_0idx);
  if (new_status != status) {
    ETS_PRINTF("* Status Register changed!!! after %u-bit Status Register-%u write.\n", 8u, reg_0idx + 1u);
    success = false;
  }
  return success;
}

bool test_sr1_16_bit_write() {
  using namespace experimental;

  uint32_t status  = flash_gd25q32c_read_status(/* SR1 */ 0);
  status |= flash_gd25q32c_read_status(/* SR2 */ 1u) << 8u;
  spi_flash_write_status(status); // 16-bit status register write
  bool success = true;
  if (is_WEL()) {
    ETS_PRINTF("* WEL left set after %u-bit Status Register-%u write.\n", 16u, 1u);
    success = false;
  } else {
    ETS_PRINTF("  Pass - %u-bit Write Status Register-%u.\n", 16u, 1u);
  }

  uint32_t new_status  = flash_gd25q32c_read_status(/* SR1 */ 0);
  new_status |= flash_gd25q32c_read_status(/* SR2 */ 1u) << 8u;
  if (new_status != status) {
    ETS_PRINTF("* Status Register changed!!! after %u-bit Status Register-%u write.\n", 16u, 1u);
    success = false;
  }
  return success;
}

////////////////////////////////////////////////////////////////////////////////
// prerequisites
//   fd_state.has_8bw_sr2
//   fd_state.has_16bw_sr1
//
bool clearSR21(bool _non_volatile) {
  using namespace experimental;

  if (fd_state.has_16bw_sr1) {
    spi0_flash_write_status_register(/* SR1 */ 0, 0, _non_volatile, 16u);
  } else {
    spi0_flash_write_status_register(/* SR1 */ 0, 0, _non_volatile, 8u);
    if (fd_state.has_8bw_sr2) {
      spi0_flash_write_status_register(/* SR2 */ 1u, 0, _non_volatile, 8u);
      // I don't know that I need this. I sleep better with it. The concern is
      // all the Flash chips have different bits doing different things some
      // block register writes. If write block bits are split across register 1
      // and 2 then there may be a specific order needed. And, there are too
      // many data sheets for me to read.
      spi0_flash_write_status_register(/* SR1 */ 0, 0, _non_volatile, 8u);
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Generalized Flash Status Register modify bit n function
//   bit_pos       bit position, 0 - 15
//   val           bit value 0, 1
//   _non_volatile  non_volatile_bit use non-volatile Flash status register
//                 volatile_bit use volatile Flash status register
//
// prerequisites
//   fd_state.has_8bw_sr2
//   fd_state.has_16bw_sr1
//
// returns:
//  1 - value changed, updated
//  0 - value already up to date - nochange
// -1 - change failed - Status register write failed.
//
int modifyBitSR(const uint32_t bit_pos, uint32_t val, const bool _non_volatile ) {
  using namespace experimental;

  // BIT1 and BIT0 are WEL and WIP which cannot be directly changed.
  if (2 > bit_pos || 16 <= bit_pos) return false;

  uint32_t _bit = 1u << bit_pos;

  uint32_t sr2 = 0;
  uint32_t sr21 = 0;
  spi0_flash_read_status_register_1(&sr21);
  if (fd_state.has_8bw_sr2 || fd_state.has_16bw_sr1) {
    spi0_flash_read_status_register_2(&sr2);
    sr21 |= sr2 << 8u;
  }

  if (val) {
    if (0 != (sr21 & _bit)) {
      return 0;
    }
    sr21 |= _bit;
  } else {
    if (0 == (sr21 & _bit)) {
      return 0;
    }
    sr21 &= ~_bit;
  }

  if (fd_state.has_16bw_sr1) {
    spi0_flash_write_status_register(/* SR1 */ 0, sr21, _non_volatile, 16u);
  } else {
    if (8u > bit_pos) {
      spi0_flash_write_status_register(/* SR1 */ 0, sr21 & 0xFFu, _non_volatile, 8u);
    } else {
      spi0_flash_write_status_register(/* SR2 */ 1u, sr21 >> 8u, _non_volatile, 8u);
    }
  }

  uint32_t verify_sr2 = 0;
  uint32_t verify_sr21 = 0;
  spi0_flash_read_status_register_1(&verify_sr21);
  if (fd_state.has_8bw_sr2 || fd_state.has_16bw_sr1) {
    spi0_flash_read_status_register_2(&verify_sr2);
    verify_sr21 |= verify_sr2 << 8u;
  }

  return (verify_sr21 == sr21) ? 1 : -1;
}

////////////////////////////////////////////////////////////////////////////////
// Evaluate if Status Register writes are supported with QE bit S9/S6
//
// Test if proposed QE bit is writable
// If QE/S9 fails to write switch to QE/S6
//
bool analyze_write_QE(bool _non_volatile) {
  using namespace experimental;
  bool pass = false;
  uint32_t qe_num = (fd_state.S9) ? 9u : (fd_state.S6) ? 6u : 0xFFu;

  if (!fd_state.S9 && !fd_state.S6) {
    Serial.PRINTF_LN("\nanalyze_write_QE: Testing for QE/? - No QE bit selected, S9 or S6.");
    return pass;
  }

  Serial.PRINTF_LN("\nanalyze_write_QE: Testing for QE/%u Status Register write support", qe_num);
  // set and clear QE
  int res = modifyBitSR(qe_num, 1u, _non_volatile);
  if (0 <= res) {
    // It is okay if already set, we will learn if Status Register Writes are
    // allowed when we clear.
    res = modifyBitSR(qe_num, 0, _non_volatile);
    if (1 == res) {
      pass = true;
      Serial.PRINTF_LN("  supported");
    } else {
      Serial.PRINTF_LN("* QE/S%u bit stuck on cannot clear in %svolatile Status Register.",
        qe_num, (_non_volatile) ? "non-" : "");
    }
  } else {
    Serial.PRINTF_LN("* Unable to modify QE/S%u bit in %svolatile Status Register.",
      qe_num, (_non_volatile) ? "non-" : "");
  }
  return pass;
}


bool analyze_SR_BP0(const bool _non_volatile) {
  using namespace experimental;
  Serial.PRINTF_LN("\nanalyze_SR_BP0: Testing for writable %svolatile Status Register using BP0", (_non_volatile) ? "non-" : "");

  // set and clear BP0
  int res = modifyBitSR(/* BP0 */2u, 1u, _non_volatile);
  if (0 <= res) {
    // It is okay if already set, we will learn if Status Register Writes are
    // allowed when we clear.
    res = modifyBitSR(/* BP0 */2u, 0, _non_volatile);
    if (1 == res) {
      Serial.PRINTF_LN("  supported");
      return true;
    }

    Serial.PRINTF_LN("  PM0 bit stuck on cannot clear bit BP0 in %svolatile Status Register.", (_non_volatile) ? "non-" : "");
  } else {
    Serial.PRINTF_LN("  Unable to modify PM0 bit in %svolatile Status Register.", (_non_volatile) ? "non-" : "");
  }
  clearSR21(_non_volatile);
  return false;
}

/*
  Analyze Flash Status Registers for possible support for QE.
  Discover which status registers are available and how they need to be accessed.
  Best guess for QE S6 or S9
  Best guess for 8 and/or 16 bit support for status register write

  Assumptions made:
  * if 16-bit status register write works, then the QE bit is S9
  * if SR1 and SR2 exist then QE bit is S9
  * if SR2 does not exist and SR1 does exist then QE bit is S6
  * flash that use S15 for QE bit are not paired with the ESP8266.
    * These parts require a unique read and write instruction 3Fh/3Eh for SR2.
    * I don't find any support for these in esptool.py
*/
bool analyze_SR_QE(const uint32_t hint) {
  using namespace experimental;

  // Reset Flash Discovery states
  resetFlashDiscovery();

  // Take control of GPIO9/"/WP" and force high so SR writes are not protected.
  digitalWrite(9, HIGH);
  pinMode(9, OUTPUT);

  // This should not be necessary; however, our analysis depends on WEL working
  // a certain way. This confirms the Flash works and our thinking.
  Serial.PRINTF_LN("\nanalyze_SR_QE: Check Flash support for the Write-Enable-Latch instructions, Enable 06h and Disable 04h");
  if (confirmWelBit()) {
    Serial.PRINTF_LN("  supported");
  } else {
    Serial.PRINTF_LN("  not supported");
    return false;
  }

  Serial.PRINTF_LN("\nanalyze_SR_QE: Testing Status Register writes:");
  bool sr1_8  = test_sr_8_bit_write(0);
  bool sr2_8  = test_sr_8_bit_write(1u);
  bool sr3_8  = test_sr_8_bit_write(2u);
  bool sr1_16 = test_sr1_16_bit_write();
  spi0_flash_write_disable(); // WEL Cleanup

  fd_state.device = printFlashChipID("  ");

  if (sr1_8) fd_state.has_8bw_sr1 = true;
  if (sr2_8) fd_state.has_8bw_sr2 = true;
  if (sr1_16) fd_state.has_16bw_sr1 = true;

  // determine if the QE bit can be written.
  // determine if we have volatile bits
  // can we modify non-volatile BP0 in Status Register-1?
  // can we modify volatile BP0 in Status Register-1?
  // Clearing all bits should be safe. Setting is when we can get into trouble
  clearSR21(non_volatile_bit);
  fd_state.has_volatile = analyze_SR_BP0(volatile_bit);
  #if ! BUILD_OPTION_FLASH_SAFETY_OFF
  // With non-volatile, it is easy to brick a module vs with volatile it can be
  // restored by powering off/on.
  if (! fd_state.has_volatile) {
    Serial.PRINTF("\n"
    "* Built with safety on default.\n"
    "  To turn off use '-DBUILD_OPTION_FLASH_SAFETY_OFF=1'. Without volatile\n"
    "  Status Register bits, it is possible that proceeding with more test could\n"
    "  brick the Flash.\n");
    return false;
  }
  #endif
  clearSR21(non_volatile_bit);
  fd_state.write_QE = analyze_SR_BP0(non_volatile_bit);


  Serial.PRINTF_LN("\nanalyze_SR_QE: Flash Info Summary:");
  if (sr1_8 || sr2_8 || sr3_8) {
    Serial.PRINTF("  Support for 8-bit Status Registers writes:");
    if (sr1_8) Serial.PRINTF(" SR1");
    if (sr2_8) Serial.PRINTF(" SR2");
    if (sr3_8) Serial.PRINTF(" SR3");
    Serial.PRINTF_LN();
  } else {
    Serial.PRINTF_LN("  No Flash Status Registers discovered!??");
    return false;
  }

  if (!fd_state.write_QE) {
    // TODO S9 failed should we try with S6??
    // TODO check and report if possible OTP has been issued - might test at
    // the beginning and avoid confusion of when it happened.
    // Also may make sense to take control of pin 10 and force HIGH to enable
    // writes.
    Serial.PRINTF_LN("  Status Register-1 does not appear to support writes.");
    return false;
  }

  if (6u == hint) {
    Serial.PRINTF_LN("  Hint suggests Status Register-1 support QE/S6 (WPdis) BIT6");  //*
    fd_state.S6 = true;
  } else
  if (sr1_16) {
    fd_state.S9 = true; // maybe
    Serial.PRINTF_LN("  May support QE bit at BIT9/S9 with 16-bit Write Status Register-1"); //*
    if (sr2_8) {
      Serial.PRINTF_LN("    or as 8-bit Write Status Register-2 BIT1");
    }
  } else
  if (sr2_8) {
    Serial.PRINTF_LN("  May support QE bit at S9 with 8-bit Write Status Register-2 BIT1"); //*
    fd_state.S9 = true;
  } else {
    Serial.PRINTF_LN("  No Status Register-2 present");
    if (sr1_8) {
      Serial.PRINTF_LN("  Status Register-1 may support QE/S6 (WPdis) BIT6");  //*
      fd_state.S6 = true; // assumed when SR2 is not present
    }
  }
  if (fd_state.write_QE) {
    Serial.PRINTF_LN("  %svolatile Status Register bits available", "non-");
  }
  if (fd_state.has_volatile) {
    Serial.PRINTF_LN("  %svolatile Status Register bits available", "");
  }

  sfdpInfo = get_sfdp_revision();
  if (sfdpInfo.u32[0]) {
    Serial.PRINTF_LN(
      "  SFDP Revision: %u.%02u, 1ST Parameter Table Revision: %u.%02u\n"
      "  SFDP Table Ptr: 0x%02X, Size: %u Bytes",
      sfdpInfo.hdr_major,  sfdpInfo.hdr_minor, sfdpInfo.parm_major, sfdpInfo.parm_minor,
      sfdpInfo.tbl_ptr, sfdpInfo.sz_dw * 4u);
  } else {
    Serial.PRINTF_LN("  No SFDP");
  }

  // Check that the QE bit is writable
  fd_state.write_QE = analyze_write_QE((fd_state.has_volatile) ? volatile_bit : non_volatile_bit);
  if (!fd_state.write_QE) {
    Serial.PRINTF(
      "* The proposed QE/S%x bit was not writable.\n"
      "* We now assume no support for the QE bit.\n", (fd_state.S9) ? 9u : 6u);
    fd_state.S6 = false;
    fd_state.S9 = false;
  }

// TODO need more work on deciding if we have enough info to continue.
//
  if (fd_state.write_QE || fd_state.has_16bw_sr1 || fd_state.has_8bw_sr1 || fd_state.has_8bw_sr2) {
    // Safe to proceed
    return true;
  } else {
    // nothing conclusive
    resetFlashDiscovery();
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// check_QE_WP_xx - verify pin function /WP is not present or can be turned off
//
bool check_QE_WP_SxFF() {
  using namespace experimental;
  bool _non_volatile = (fd_state.has_volatile) ? volatile_bit : non_volatile_bit;

  // Clear Status Register-1 aka SR1
  test_clear_SRP1_SRP0_QE(0xFFu, write_status_register_16_bit, _non_volatile);
  bool pass1 = testOutputGPIO10(0xFFu, write_status_register_16_bit, _non_volatile, true /* use asis QE=0 */);

  if (pass1) {
    /* success with QE/S6= 0 or 1 */
    fd_state.WP = false;
    Serial.PRINTF_LN("  Feature pin function /WP not detected on this part.");
    if (fd_state.has_8bw_sr2) {
      Serial.PRINTF_LN("* However, check if the Flash supports SRP1 and SRP0 this would confuse the results\n"
                       "* when using QE/S6. Assuming these combinations exist.");
    }
  } else {
    Serial.PRINTF_LN("  Feature pin function /WP detected.");
    fd_state.WP = true;
  }

  // With success on the last test, we return with the Status Register ready for
  // the next test.
  return pass1;
}

bool check_QE_WP_S6() {
  using namespace experimental;
  bool _non_volatile = (fd_state.has_volatile) ? volatile_bit : non_volatile_bit;

  // Clear Status Register-1 aka SR1
  // spi0_flash_write_status_register(/* SR1 */ 0, /* value */ 0, _non_volatile, 8u);
  test_clear_SRP1_SRP0_QE(6u, write_status_register_16_bit, _non_volatile);
  bool pass1 = testOutputGPIO10(6u, write_status_register_16_bit, _non_volatile, true /* use asis QE=0 */);
  bool pass2 = testOutputGPIO10(6u, write_status_register_16_bit, _non_volatile, false /* set QE=1 */);

  if (pass1 && pass2) {
    /* success with QE/S6= 0 or 1 */
    fd_state.WP = false;
    Serial.PRINTF_LN("  Feature pin function /WP not detected on this part.");
    if (fd_state.has_8bw_sr2) {
      Serial.PRINTF_LN("* However, check if the Flash supports SRP1 and SRP0 this would confuse the results\n"
                       "* when using QE/S6. Assuming these combinations exist.");
    }
  } else
  if (!pass1 && pass2) {
    /* success, QE/S6=1 controls /WP feature enable/disable */
    Serial.PRINTF_LN("  Confirmed QE/S6=1 disables pin function /WP");
    fd_state.WP = true; // pin function /WP detected.
  } else {
    Serial.PRINTF_LN("  Unable to disable pin function /WP with QE/S6=1");
    fd_state.WP = true;
  }

  // With success on the last test, we return with the Status Register ready for
  // the next test.
  return pass2;
}

bool check_QE_WP_S9() {
  // Current working assumption - if not S9 then no SR2.
  using namespace experimental;
  // Okay we need 4 passes to be complete
  // QE=0/1 and SRP1:SRP0=0:0/0:1
  // Move this to a function to call
  bool _non_volatile = (fd_state.has_volatile) ? volatile_bit : non_volatile_bit;

  Serial.PRINTF_LN("  Set context SRP1:SRP0 0:1");
  test_set_SRP1_SRP0_clear_QE(9u, write_status_register_16_bit, _non_volatile);
  printSR321("  ");

  bool pass_SRP01_QE0 = testOutputGPIO10(9u, write_status_register_16_bit, _non_volatile, true  /* use QE=0 */);
  bool pass_SRP01_QE1 = testOutputGPIO10(9u, write_status_register_16_bit, _non_volatile, false /* set QE=1 */);

  Serial.PRINTF_LN("  Clear context SRP1:SRP0 0:0");
  test_clear_SRP1_SRP0_QE(9u, write_status_register_16_bit, _non_volatile);
  printSR321("  ");

  bool pass_SRP00_QE0 = testOutputGPIO10(9u, write_status_register_16_bit, _non_volatile, true  /* use QE=0 */);
  bool pass_SRP00_QE1 = testOutputGPIO10(9u, write_status_register_16_bit, _non_volatile, false /* set QE=1 */);

  if (pass_SRP00_QE1) {
    /*
      This looks like a lot of unnecessary testing; however, it helps confirm
      our understanding of what characteristics or features the Flash memory
      has or doesn't have.

      In the end we are going to use QE/S9 with SRP1:SRP0=0:0
    */
    if (pass_SRP01_QE0 && pass_SRP01_QE1 && pass_SRP00_QE0) {
      Serial.PRINTF_LN("  Feature pin function /WP not detected on this part.");
      fd_state.WP = false;
    } else
    if (!pass_SRP01_QE0 && !pass_SRP01_QE1 && pass_SRP00_QE0) {
      Serial.PRINTF_LN("  Requires only SRP1:SRP0=0:0 to disable pin function /WP");
      fd_state.WP = true; // pin function /WP detected.
    } else
    if (!pass_SRP01_QE0 && !pass_SRP01_QE1 && !pass_SRP00_QE0) {
      Serial.PRINTF_LN("  Requires QE/S9 plus SRP1:SRP0=0:0 to disable pin function /WP");
      fd_state.WP = true;
    } else
    if (!pass_SRP01_QE0 && pass_SRP01_QE1 && !pass_SRP00_QE0) {
      Serial.PRINTF_LN("  Requires QE/S9 only to disable pin function /WP");
      fd_state.WP = true;
    } else {
      Serial.PRINTF_LN("* Unexpected pass/fail pattern: %u=pass_SRP01_QE0, %u=pass_SRP01_QE1, %u=pass_SRP00_QE0, 1=pass_SRP00_QE1",
        pass_SRP01_QE0, pass_SRP01_QE1, pass_SRP00_QE0);
    }
  } else {
    /* no resolution found */
    Serial.PRINTF_LN("* No conclusion on how to disable pin function /WP using SRP1:SRP0 and QE/S9");
    Serial.PRINTF_LN("  Clear context SRP1:SRP0 0:0 and QE=0");
    test_clear_SRP1_SRP0_QE(9u, write_status_register_16_bit, volatile_bit);
    printSR321("  ");
  }

  // With success on the last test, we return with the Status Register ready for
  // the next test.
  return pass_SRP00_QE1;
}

////////////////////////////////////////////////////////////////////////////////
// Needed prerequisites from analyze_SR_QE()
// has_8bw_sr2
// has_16bw_sr1
// has_volatile
// fd_state.write_QE == 1
//
// return true if write protect can be turned off or was not detected.
//
// check fd_state.WP - it will be false if feature was not detected.
bool check_QE_WP() {
  // To complete we need
  //  4 passes for S9 - QE=0/1 and SRP1:SRP0=0:0/0:1   or
  //  2 passes for S6 - QE=0/1
  //
  bool pass = false;

  if (fd_state.S9) {
    pass = check_QE_WP_S9();
  } else
  if (fd_state.S6) {
    pass = check_QE_WP_S6();
  } else {
    // Maybe there is no pin function /WP
    pass = check_QE_WP_SxFF();
  }

  return pass;
}

bool analyze_this(const uint32_t hint) {
  bool pass = true;
  if (analyze_SR_QE(hint)) {
    if (fd_state.S6) {
      modal_qe_bit = 6u;
    } else
    if (fd_state.S9) {
      modal_qe_bit = 9u;
    } else {
      modal_qe_bit = 0xFFu;
    }
    write_status_register_16_bit = fd_state.has_16bw_sr1;
  } else {
    resetModalValues();
    pass = false;
  }
  return pass;
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool processKey(const int key) {
  using namespace experimental;
  bool pass = true;
  last_key = key;

  switch (key) {
    case 'a':
      pass = analyze_this(8u);  // hint QE/S9
      if (!scripting && pass) {
        Serial.PRINTF_LN("\nTo continue verification, run /WP test, menu 'w'");
      }
      break;

    case 'A':
      pass = analyze_this(6u);  // hint QE/S6
      if (!scripting && pass) {
        Serial.PRINTF_LN("\nTo continue verification, run /WP test, menu 'w'");
      }
      break;

    // Use settings found from analyze or those selected previously through the
    // menu.
    case 'w':
      pass = check_QE_WP();
      if (!scripting && pass) {
        Serial.PRINTF_LN("\nTo complete verification, run /HOLD test, menu 'h'");
      }
      fd_state.pass_WP = pass;
      break;
    //
    case 'h':
      if (fd_state.pass_WP) {
        // For this test, we use the Flash Status Register as left by 'w'
        pass = testOutputGPIO9(modal_qe_bit, write_status_register_16_bit,
          (fd_state.has_volatile) ? volatile_bit : non_volatile_bit, true);
      } else {
          Serial.PRINTF_LN("\nMissing prerequisite test, run /WP test, menu 'w'");
      }
      if (!scripting && pass) {
        Serial.PRINTF_LN("\nUse menu 'p' to print 'spi_flash_vendor_cases()' example function.");
      }
      fd_state.pass_HOLD = pass;
      break;

    case 'p':
      printReclaimFn();
      break;
    //
    case '8':
      write_status_register_16_bit = false;
      fd_state.has_8bw_sr2 = true;
      fd_state.has_16bw_sr1 = false;
      Serial.PRINTF_LN("%c 8 - Modal:  8-bits Write Status Register", (write_status_register_16_bit) ? ' ' : '>');
      break;
    case '7':
      write_status_register_16_bit = true;
      fd_state.has_16bw_sr1 = true;
      fd_state.has_8bw_sr2 = false;  // We really don't know
      Serial.PRINTF_LN("%c 7 - Modal: 16-bits Write Status Register", (write_status_register_16_bit) ? '>' : ' ');
      break;
    case '6':
      modal_qe_bit = 6u;
      fd_state.S9 = false;
      fd_state.S6 = true;
      Serial.PRINTF_LN("%c 6 - Modal: S6/QE Status Register", (6u == modal_qe_bit) ? '>' : ' ');
      break;
    case '9':
      modal_qe_bit = 9u;
      fd_state.S9 = true;
      fd_state.S6 = false;
      Serial.PRINTF_LN("%c 9 - Modal: S9/QE Status Register", (9u == modal_qe_bit) ? '>' : ' ');
      break;
    case '5':
    case 'M':
    case 'm':
      switch (key) {
        case 'M':
          qe_was_preset = true;
          break;
        case 'm':
          qe_was_preset = false;
          break;
        default:
        case '5':
          qe_was_preset = ! qe_was_preset;
          break;
      }
      Serial.PRINTF_LN("\nUpdated Modal selection");
      if (qe_was_preset) {
        Serial.PRINTF("  The current Status Register values will be used for options: 'h', 'w', 'v'\n"
                      "  Use options 'Q', 'q', 'E', or 'e' to change the Status Register QE bit.\n");
      } else {
        Serial.PRINTF_LN("  Modal settings will be used for options: 'h', 'w', 'v'");
      }
      break;

    case 'f':
      printSfdpReport();
      break;

    case 'v':
      testInput_GPIO9_GPIO10(modal_qe_bit, write_status_register_16_bit,
        (fd_state.has_volatile) ? volatile_bit : non_volatile_bit, qe_was_preset);
      break;



    // Evaluate S9 as
    //   hotkey 'Q' or 'E' => QE=1
    //   hotkey 'q' or 'e' => QE=0
    //   'Q' or 'q'  non-volatile
    //   'E' or 'e'  volatile
    case 'Q':
    case 'q':
    case 'E':
    case 'e':
      {
        Serial.PRINTF("\nUpdate QE=");
        uint32_t status = 0;
        SpiOpResult ok0 = spi0_flash_read_status_registers_3B(&status);
        if (SPI_RESULT_OK != ok0) {
          Serial.PRINTF_LN("* SR321 read operation failed!");
          break;
        }
        bool _non_volatile = ('Q' == key) || ('q' == key);
        bool set_QE = ('Q' == key) || ('E' == key);
        Serial.PRINTF_LN("%u bit in %svolatile flash status register, SR321 0x%06X", (set_QE) ? 1u : 0, (_non_volatile) ? "non-" : "", status);

        pass = false;
        if (set_QE) {
          if (9u == modal_qe_bit) {
            if (write_status_register_16_bit) {
              pass = set_S9_QE_bit__16_bit_sr1_write(_non_volatile);
            } else {
              pass = set_S9_QE_bit__8_bit_sr2_write(_non_volatile);
            }
          } else
          if (6u == modal_qe_bit) {
            pass = set_S6_QE_bit_WPDis(_non_volatile);
          }
        } else {
          if (9u == modal_qe_bit) {
            if (write_status_register_16_bit) {
              pass = clear_S9_QE_bit__16_bit_sr1_write(_non_volatile);
            } else {
              pass = clear_S9_QE_bit__8_bit_sr2_write(_non_volatile);
            }
          } else
          if (6u == modal_qe_bit) {
            pass = clear_S6_QE_bit_WPDis(_non_volatile);
          }
        }

        ok0 = spi0_flash_read_status_registers_3B(&status);
        if (SPI_RESULT_OK == ok0) {
          Serial.PRINTF_LN("  SR321 0x%06X", status);
        } else {
          Serial.PRINTF_LN("* SR321 read operation failed!");
        }
      }
      break;

    case 'r':
      if (qe_was_preset || gpio_9_10_available) {
        Serial.PRINTF_LN("reclaim_GPIO_9_10() has already been successfully called. Reboot to run again.");
      } else {
        Serial.PRINTF_LN("Test call to reclaim_GPIO_9_10()");
        gpio_9_10_available = reclaim_GPIO_9_10();
      }
      qe_was_preset = true;
      break;

    case '3':
      printSR321();
      break;

    case 'R':
      Serial.PRINTF_LN("Restart ...");
      test_clear_SRP1_SRP0_QE(modal_qe_bit, write_status_register_16_bit, volatile_bit);
      Serial.flush();
      ESP.restart();
      break;

    case 'd':
      {
        Serial.PRINTF_LN("\nFlash write disable:");
        SpiOpResult ok0 = spi0_flash_write_disable();
        Serial.PRINTF_LN("%c %s\n", (SPI_RESULT_OK == ok0) ? ' ' : '*', (SPI_RESULT_OK == ok0) ? "success" : "failed!");
        uint32_t status = 0;
        ok0 = spi0_flash_read_status_registers_3B(&status);
        if (SPI_RESULT_OK == ok0) {
          Serial.PRINTF_LN("%c SR321 0x%06X", (status & kWELBit) ? '*' : ' ', status);
          Serial.PRINTF_LN("* WEL bit stuck on");
        } else {
          Serial.PRINTF_LN("* SR321 read operation failed!");
        }
      }
      break;
    //
    case 'S':
      // Force clear /WP
      pinMode(10, OUTPUT);
      digitalWrite(10, HIGH);
      Serial.PRINTF_LN("\nSPI Flash Software Reset (66h, 99h):");
      if (SPI_RESULT_OK == spi0_flash_software_reset()) {
        auto ok0 = spi0_flash_write_status_register_2(0x00u, true);
        if (SPI_RESULT_OK == ok0) {
          Serial.PRINTF_LN("  Success");
        } else {
          Serial.PRINTF_LN("  Failed");
        }
      } else {
        Serial.PRINTF_LN("  Failed");
      }
      break;

    case 'z':
      runScript('a');
      break;

    case '?':
      Serial.PRINTF_LN("\r\nHot key help:");
      Serial.PRINTF_LN("\n  TODO: manual mode via Modal needs work to meld with FlashDiscovery table\n");
      Serial.PRINTF_LN("  3 - SPI Flash Read Status Registers, 3 Bytes");
      Serial.PRINTF_LN("  a - Analyze Status Register Writes and set selection (Modal) flags accordingly");
      Serial.PRINTF_LN("  A - Analyze Status Register Writes and set selection (Modal) flags accordingly, uses hint S6");
      Serial.PRINTF_LN("  f - Print SFDP Data");
      Serial.PRINTF_LN();
      Serial.PRINTF_LN("%c 8 - Modal:  8-bits Write Status Register", (write_status_register_16_bit) ? ' ' : '>');
      Serial.PRINTF_LN("%c 7 - Modal: 16-bits Write Status Register", (write_status_register_16_bit) ? '>' : ' ');
      Serial.PRINTF_LN("%c 6 - Modal: S6/QE Status Register", (6u == modal_qe_bit) ? '>' : ' ');
      Serial.PRINTF_LN("%c 9 - Modal: S9/QE Status Register", (9u == modal_qe_bit) ? '>' : ' ');
      Serial.PRINTF_LN("%c 5 - Modal: Use modal values defined above", (qe_was_preset) ? ' ' : '>');
      if (qe_was_preset) {
      Serial.PRINTF_LN("      The current Status Register values will be used for options: 'h', 'w', 'v'");
      Serial.PRINTF_LN("      Use options 'Q', 'q', 'E', or 'e' to change the Status Register QE bit.");
      } else {
      Serial.PRINTF_LN("      Modal settings will be used for options: 'h', 'w', 'v'");
      }
      Serial.PRINTF_LN();
      if (9u == modal_qe_bit || 6u == modal_qe_bit) {
      Serial.PRINTF_LN("  Q - Set SR, non-volatile WEL 06h, then set QE/S%u=1", modal_qe_bit);
      Serial.PRINTF_LN("  q - Set SR, non-volatile WEL 06h, then set QE/S%u=0", modal_qe_bit);
      Serial.PRINTF_LN("  E - Set SR, volatile 50h, then set QE/S%u=1", modal_qe_bit);
      Serial.PRINTF_LN("  e - Set SR, volatile 50h, then set QE/S%u=0", modal_qe_bit);
      } else {
      Serial.PRINTF_LN("  - - Select a QE Status Register bit to enable hotkeys 'Q', 'q', 'E', 'e'");
      }
      Serial.PRINTF_LN();
      Serial.PRINTF_LN("  h - Test /HOLD digitalWrite( 9, LOW)");
      Serial.PRINTF_LN("  w - Test /WP   digitalWrite(10, LOW) and write to Flash");
      Serial.PRINTF_LN("  p - Print 'spi_flash_vendor_cases()' example function");
      Serial.PRINTF_LN("  v - Test GPIO9 and GPIO10 INPUT");
      Serial.PRINTF_LN("%c r - Test call to 'reclaim_GPIO_9_10()'%s", (gpio_9_10_available) ? '>' : ' ', (gpio_9_10_available) ? " - already called" : "");
      Serial.PRINTF_LN();
      Serial.PRINTF_LN("  d - Clear WEL bit, write enable");
      Serial.PRINTF_LN("  S - SPI Flash - Software Reset 66h, 99h");
      Serial.PRINTF_LN("  R - Restart");
      Serial.PRINTF_LN("  ? - This help message");
      break;

    default:
      Serial.PRINTF_LN();
      break;
  }
  return pass;
}

void serialClientLoop(void) {
  if (0 < Serial.available()) {
    char hotKey = Serial.read();
    if (false == processKey(hotKey)) {
      // On error processing command, clear input buffer.
      while (0 <= Serial.read());
    }
  }
}





// Modal states
bool qe_was_preset = false;
bool write_status_register_16_bit = true;
uint32_t modal_qe_bit = 9u;

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
// SFDP
union SFDP_Hdr {
  struct {
    uint32_t signature:32;
    uint32_t rev_minor:8;
    uint32_t rev_major:8;
    uint32_t num_param_hdrs:8;
    uint32_t access_protocol:8;
  };
  uint32_t u32[2];
};

union SFDP_Param {
  struct {
    uint32_t id_lsb:8;
    uint32_t rev_minor:8;
    uint32_t rev_major:8;
    uint32_t num_dw:8;
    uint32_t tbl_ptr:24;
    uint32_t id_msb:8;
  };
  uint32_t u32[2];
};

union SFDP_REVISION {
  struct {
    uint32_t hdr_minor:8;
    uint32_t hdr_major:8;
    uint32_t parm_minor:8;
    uint32_t parm_major:8;
  };
  uint32_t u32;
};

union SFDP_REVISION get_sfdp_revision() {
  using namespace experimental;

  union SFDP_Hdr sfdp_hdr;
  union SFDP_Param sfdp_param;
  union SFDP_REVISION rev;
  rev.u32 = 0;

  size_t sz = sizeof(sfdp_hdr);
  size_t addr = 0u;
  SpiOpResult ok0 = spi0_flash_read_sfdp(addr, &sfdp_hdr.u32[0], sz);
  if (SPI_RESULT_OK == ok0 && 0x50444653 == sfdp_hdr.signature) {
    rev.hdr_major = sfdp_hdr.rev_major;
    rev.hdr_minor = sfdp_hdr.rev_minor;

    addr += sz;
    sz = sizeof(sfdp_param);
    ok0 = spi0_flash_read_sfdp(addr, &sfdp_param.u32[0], sz);
    if (SPI_RESULT_OK == ok0) {
      // return version of 1st parameter block
      rev.parm_major = sfdp_param.rev_major;
      rev.parm_minor = sfdp_param.rev_minor;
    }
  }
  return rev;
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
bool analyzeSR_QE() {
  using namespace experimental;

  // Reset Flash Discovery states
  fd_state.S9 = false;
  fd_state.S6 = false;
  fd_state.has_8b_sr2 = false;
  fd_state.has_16b_sr1 = false;
  fd_state.has_volatile = false;

  // Take control of GPIO9/"/WP" and force high so SR writes are not protected.
  digitalWrite(9, HIGH);
  pinMode(9, OUTPUT);

  // This should not be necessary; however, our analysis depends on WEL working
  // a certain way. This confirms the Flash works and our thinking.
  Serial.PRINTF_LN("\nCheck Flash support for the Write-Enable-Latch instructions, Enable 06h and Disable 04h");
  if (confirmWelBit()) {
    Serial.PRINTF_LN("  supported");
  } else {
    Serial.PRINTF_LN("  not supported");
    return false;
  }

  Serial.PRINTF_LN("\nTesting Status Register writes:");
  bool sr1_8  = test_sr_8_bit_write(0);
  bool sr2_8  = test_sr_8_bit_write(1u);
  bool sr3_8  = test_sr_8_bit_write(2u);
  bool sr1_16 = test_sr1_16_bit_write();
  spi0_flash_write_disable(); // WEL Cleanup

  fd_state.device = printFlashChipID();
  Serial.PRINTF_LN("\nFlash Info Summary:");
  if (sr1_8 || sr2_8 || sr3_8) {
    Serial.PRINTF("  Support for 8-bit Status Registers writes:");
    if (sr1_8) Serial.PRINTF(" SR1");
    if (sr2_8) {
      Serial.PRINTF(" SR2");
      fd_state.has_8b_sr2 = true;
    }
    if (sr3_8) Serial.PRINTF(" SR3");
    Serial.PRINTF_LN();
  }

  if (sr1_16) {
    Serial.PRINTF_LN("  Support for 16-bit Write Status Register-1");
    Serial.PRINTF_LN("  May support QE bit at BIT9/S9 with 16-bit Write Status Register-1");
    fd_state.has_16b_sr1 = true;
    fd_state.S9 = true;
    if (sr2_8) {
      Serial.PRINTF_LN("    or as 8-bit Write Status Register-2 BIT1");
    }
  } else
  if (sr2_8) {
    Serial.PRINTF_LN("  May support QE bit at S9 with 8-bit Write Status Register-2 BIT1");
    fd_state.S9 = true;
  } else {
    Serial.PRINTF_LN("  No Status Register-2 present");
    if (sr1_8) {
      Serial.PRINTF_LN("  Status Register-1 may support QE/WPdis at BIT6/S6");
      fd_state.S6 = true;
    }
  }

  union SFDP_REVISION rev = get_sfdp_revision();
  if (rev.u32) {
    Serial.PRINTF_LN("  SFDP Revision: %u.%02u, 1ST Parameter Table Revision: %u.%02u",
      rev.hdr_major,  rev.hdr_minor, rev.parm_major, rev.parm_minor);
  } else {
    Serial.PRINTF_LN("  No SFDP available");
  }


  if (fd_state.S9 || fd_state.S6) {
    return true;
  } else {
    // nothing conclusive - Reset Discovery
    fd_state.S9 = false;
    fd_state.S6 = false;
    fd_state.has_16b_sr1 = true;
    fd_state.has_volatile = false;
  }
  return false;
}

// Which would be better testing for volatile status register with S9/S6 or BP0 ??

// Evaluate if Status Register volatile writes are supported setting
// QE bit S9/S6

// Test if proposed QE bit is writable
bool analyze_write_QE(const bool non_volatile) {
  using namespace experimental;
  fd_state.write_QE = false;

  if (fd_state.S9 || fd_state.S6) {
    Serial.PRINTF_LN("\nTesting for QE/%u Status Register write support", fd_state.S9 ? 9u : 6u);
  } else {
    Serial.PRINTF_LN("\nNo Evidence of QE support detected.");
    return fd_state.write_QE;
  }

  if (fd_state.S9) {
    uint32_t sr2 = 0;
    spi0_flash_read_status_register_2(&sr2);
    if (BIT1 & sr2) {
      Serial.PRINTF_LN("  S9 bit already set attempting to clear.");
      // BootROM cannot always clear QE on strange flash
      sr2 &= ~BIT1;    // S9
      if (fd_state.has_16b_sr1) {
        uint32_t sr1 = 0;
        spi0_flash_read_status_register_1(&sr1);
        sr1 |= sr2 << 8u;
        spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 16u);
      } else {
        spi0_flash_write_status_register(/* SR2 */ 1u, sr2, non_volatile, 8u);
      }

      spi0_flash_read_status_register_2(&sr2);
    }

    if (BIT1 & sr2) {
      Serial.PRINTF_LN("  S9 bit stuck on cannot test for QE Status Register write support.");
    } else {
      sr2 |= BIT1;    // S9
      if (fd_state.has_16b_sr1) {
        uint32_t sr1 = 0;
        spi0_flash_read_status_register_1(&sr1);
        sr1 |= sr2 << 8u;
        spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 16u);
      } else {
        spi0_flash_write_status_register(/* SR2 */ 1u, sr2, non_volatile, 8u);
      }

      uint32_t new_sr2 = 0;
      spi0_flash_read_status_register_2(&new_sr2);
      if (BIT1 & new_sr2) {
        fd_state.write_QE = true;
        Serial.PRINTF_LN("  S9 Status Register write support confirmed.");
      }
    }

  } else
  if (fd_state.S6) {
    uint32_t sr1 = 0;
    spi0_flash_read_status_register_1(&sr1);
    if (BIT6 & sr1) {
      Serial.PRINTF_LN("  S6 bit already set attempting to clear.");
      sr1 &= ~BIT6;
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 8u);

      spi0_flash_read_status_register_1(&sr1);
    }

    if (BIT6 & sr1) {
      Serial.PRINTF_LN("  S6 bit stuck on cannot test for QE Status Register write support.");
    } else {
      sr1 |= BIT6;
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, non_volatile, 8u);

      uint32_t new_sr1 = 0;
      spi0_flash_read_status_register_1(&new_sr1);
      if (BIT6 & new_sr1) {
        fd_state.write_QE = true;
        Serial.PRINTF_LN("  S6 Status Register write support confirmed.");
      }
    }
  }

  if (! fd_state.write_QE) {
    // There is not a lot we can do if we cannot write to the Status bit possitions.
    Serial.PRINTF_LN("* QE/S%u Status Register writes not supported.", fd_state.S9 ? 9u : 6u);
  }

  return fd_state.write_QE;
}

bool analyzeSR_BP0_Volatile() {
  using namespace experimental;
  fd_state.has_volatile = false;

  if (fd_state.S9 || fd_state.S6) {
    Serial.PRINTF_LN("\nTesting for volatile Status Register write support using BP0");
  } else {
    Serial.PRINTF_LN("\nNo Evidence of QE support detected.");
    return fd_state.has_volatile;
  }

  uint32_t sr1 = 0;
  spi0_flash_read_status_register_1(&sr1);
  if (BIT2 & sr1) {
    Serial.PRINTF_LN("  PM0 bit already set attempting to clear.");
    sr1 &= ~BIT2;
    spi0_flash_write_status_register(/* SR1 */ 0, sr1, volatile_bit, 8u);

    spi0_flash_read_status_register_1(&sr1);
  }

  if (BIT2 & sr1) {
    Serial.PRINTF_LN("  PM0 bit stuck on cannot test for volatile Status Register write support.");
  } else {
    sr1 |= BIT2;
    spi0_flash_write_status_register(/* SR1 */ 0, sr1, volatile_bit, 8u);

    uint32_t new_sr1 = 0;
    spi0_flash_read_status_register_1(&new_sr1);
    if (BIT2 & new_sr1) {
      fd_state.has_volatile = true;
      Serial.PRINTF_LN("  PM0 volatile Status Register write support confirmed.");
    }
  }

  if (! fd_state.has_volatile)
    Serial.PRINTF_LN("* Volatile Status Register writes not supported.");

  return fd_state.has_volatile;
}


void printSR321(const char *indent) {
  using namespace experimental;

  uint32_t status = 0;
  SpiOpResult ok0 = spi0_flash_read_status_registers_3B(&status);
  if (SPI_RESULT_OK == ok0) {
    Serial.PRINTF_LN("%sSR321 0x%06X", indent, status);
    if (kQEBit2B == (status & kQEBit2B)) Serial.PRINTF_LN("%s  QE=1", indent);
    if (kWELBit  == (status & kWELBit))  Serial.PRINTF_LN("%s  WEL=1", indent);
    if (BIT7 == (status & BIT7))  Serial.PRINTF_LN("%s  SRP0=1", indent);
    if (BIT8 == (status & BIT8))  Serial.PRINTF_LN("%s  SRP1=1", indent);
  } else {
    Serial.PRINTF_LN("spi0_flash_read_status_registers_3B() failed!");
  }
}

bool check_QE_WP_S6() {
  using namespace experimental;

  OutputTestResult otr1;  // QE=0
  OutputTestResult otr2;  // QE=1
  otr1.qe_bit = 0xFFu;
  otr2.qe_bit = 0xFFu;

  bool pass = false;

  spi0_flash_write_status_register(/* SR1 */ 0, /* value */ 0,
    (fd_state.has_volatile) ? volatile_bit : non_volatile_bit, 8u);

  otr1 = testOutputGPIO10(6u, write_status_register_16_bit,
    (fd_state.has_volatile) ? volatile_bit : non_volatile_bit, true /* use asis QE=0 */);

  otr2 = testOutputGPIO10(6u, write_status_register_16_bit,
    (fd_state.has_volatile) ? volatile_bit : non_volatile_bit, false /* set QE=1 */);

  if (1u == otr1.low && 1u == otr2.low) {
    /* success with QE/S6=0 or 1 */
    pass = true;
    Serial.PRINTF_LN("\nFeature pin function /WP may not be present on this part.");
  } else
  if (0 == otr1.low && 1u == otr2.low) {
    /* success, QE/S6=1 controls /WP feature enable/disable */
    pass = true;
    Serial.PRINTF_LN("\nConfirmed QE/S6=1 disables pin function /WP");
  } else {
    Serial.PRINTF_LN("\nUnable to disable pin function /WP with QE/S6=1");
  }

  return pass;
}

bool check_QE_WP_S9() {
  using namespace experimental;
  // Okay we need 4 passes to be complete
  // QE=0/1 and SRP1:SRP0=0:0/0:1
  // Move this to a function to call
  OutputTestResult otr1;  // SRP1:SRP0 0:0, QE=0
  OutputTestResult otr2;  // SRP1:SRP0 0:0, QE=1
  OutputTestResult otr3;  // SRP1:SRP0 0:1, QE=0
  OutputTestResult otr4;  // SRP1:SRP0 0:1, QE=1
  otr1.qe_bit = 0xFFu;
  otr2.qe_bit = 0xFFu;
  otr3.qe_bit = 0xFFu;
  otr4.qe_bit = 0xFFu;
  bool non_volatile = (fd_state.has_volatile) ? volatile_bit : non_volatile_bit;
  bool pass = false;

  //?? if (fd_state.has_8b_sr2 && fd_state.S9) {

  // Current working assumption - if not S9 then no SR2.

  Serial.PRINTF_LN("\nSet context SRP1:SRP0 0:1");
  test_set_SRP1_SRP0_clear_QE(9u, write_status_register_16_bit, non_volatile);
  printSR321("  ");

  otr1 = testOutputGPIO10(9u, write_status_register_16_bit, non_volatile, true  /* use QE=0 */);
  otr2 = testOutputGPIO10(9u, write_status_register_16_bit, non_volatile, false /* set QE=1 */);

  Serial.PRINTF_LN("\nClear context SRP1:SRP0 0:0");
  test_clear_SRP1_SRP0_QE(9u, write_status_register_16_bit, non_volatile);
  printSR321("  ");

  otr3 = testOutputGPIO10(9u, write_status_register_16_bit, non_volatile, true  /* use QE=0 */);
  otr4 = testOutputGPIO10(9u, write_status_register_16_bit, non_volatile, false /* set QE=1 */);

  if (1u == otr4.low) {
    pass = true;
    /* success solution QE/S9=1 and/or SRP1:SRP0=0:1*/
    if (1u == otr1.low && 1u == otr3.low) {
      Serial.PRINTF_LN("\nFeature pin function /WP may not be present on this part.");
    } else
    if (0 == otr1.low && 1u == otr3.low && 1u == otr4.low) {
      Serial.PRINTF_LN("\nRequires only SRP1:SRP0=0:0 to disable pin function /WP");
    } else
    if (0 == otr1.low && 0 == otr2.low && 0 == otr3.low && 1u == otr4.low) {
      Serial.PRINTF_LN("\nRequires QE/S9 plus SRP1:SRP0=0:0 to disable pin function /WP");
    } else
    if (0 == otr1.low && 1u == otr2.low && 0 == otr3.low && 1u == otr4.low) {
      Serial.PRINTF_LN("\nRequires QE/S9 only to disable pin function /WP");
    } else {
      Serial.PRINTF_LN("\nUnexpected pass/fail pattern %u=otr1.low, %u=otr2.low, %u=otr3.low, %u=otr4.low", otr1.low, otr2.low, otr3.low, otr4.low);
    }
  } else {
    /* no resolution found */
    pass = false;
    Serial.PRINTF_LN("\nClear context SRP1:SRP0 0:0 and QE=0");
    test_clear_SRP1_SRP0_QE(9u, write_status_register_16_bit, volatile_bit);
    printSR321("  ");
    Serial.PRINTF_LN("\nNo conclusion on how to disable pin function /WP using SRP1:SRP0 and QE/S9");
  }

  return pass;
}

////////////////////////////////////////////////////////////////////////////////
// Needed prerequisites from analyzeSR_QE()/
// has_8b_sr2
// has_16b_sr1
// has_volatile
//
bool check_QE_WP() {
  // To complete we need
  //  4 passes for S9 - QE=0/1 and SRP1:SRP0=0:0/0:1   or
  //  2 passes for S6 - QE=0/1
  //
  bool pass = false;

  if (fd_state.S9) {
    pass = check_QE_WP_S9();
    if (!pass) {
      Serial.PRINTF_LN("\nTry using QE/S6");
      fd_state.S6 = true;
      fd_state.S9 = false;
      pass = check_QE_WP_S6();
      if (! pass) {
        fd_state.S6 = false;
        fd_state.S9 = true;
      }
    }
  } else
  if (fd_state.S6) {
    pass = check_QE_WP_S6();
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
      if (analyzeSR_QE()) {
        if (fd_state.S6) {
          modal_qe_bit = 6u;
        } else
        if (fd_state.S9) {
          modal_qe_bit = 9u;
        } else {
          // Reset modal values
          qe_was_preset = false;
          modal_qe_bit = 0xFFu;  // undefined
          write_status_register_16_bit = true;
          pass = false;
          break;
        }
        write_status_register_16_bit = fd_state.has_16b_sr1;
        analyzeSR_BP0_Volatile();
        // If the propossed QE bit cannot be written we are done.
        pass = analyze_write_QE((fd_state.has_volatile) ? volatile_bit : non_volatile_bit);
      } else {
        // Reset modal values
        qe_was_preset = false;
        modal_qe_bit = 0xFFu;
        write_status_register_16_bit = true;
        pass = false;
      }
      break;
    // Use settings found from analyze or those selected previously through the
    // menu.
    case 'w':
      pass = check_QE_WP();
      break;
    //
    case 'h':
      if (fd_state.S9 || fd_state.S6) {
        pass = testOutputGPIO9(modal_qe_bit, write_status_register_16_bit,
          (fd_state.has_volatile) ? volatile_bit : non_volatile_bit, qe_was_preset);
      }
      break;
    //
    case '8':
      write_status_register_16_bit = false;
      fd_state.has_8b_sr2 = true;
      fd_state.has_16b_sr1 = false;
      Serial.PRINTF_LN("%c 8 - Modal:  8-bits Write Status Register", (write_status_register_16_bit) ? ' ' : '>');
      break;
    case '7':
      write_status_register_16_bit = true;
      fd_state.has_16b_sr1 = true;
      fd_state.has_8b_sr2 = false;  // We really don't know
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
        bool non_volatile = ('Q' == key) || ('q' == key);
        bool set_QE = ('Q' == key) || ('E' == key);
        Serial.PRINTF_LN("%u bit in %svolatile flash status register, SR321 0x%06X", (set_QE) ? 1u : 0, (non_volatile) ? "non-" : "", status);

        pass = false;
        if (set_QE) {
          if (9u == modal_qe_bit) {
            if (write_status_register_16_bit) {
              pass = set_S9_QE_bit__16_bit_sr1_write(non_volatile);
            } else {
              pass = set_S9_QE_bit__8_bit_sr2_write(non_volatile);
            }
          } else {
            pass = set_S6_QE_bit_WPDis(non_volatile);
          }
        } else {
          if (9u == modal_qe_bit) {
            if (write_status_register_16_bit) {
              pass = clear_S9_QE_bit__16_bit_sr1_write(non_volatile);
            } else {
              pass = clear_S9_QE_bit__8_bit_sr2_write(non_volatile);
            }
          } else {
            pass = clear_S6_QE_bit_WPDis(non_volatile);
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
      printSR321("");
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
        Serial.PRINTF_LN("%c %s\n", ok0, (SPI_RESULT_OK == ok0) ? ' ' : '*', (SPI_RESULT_OK == ok0) ? "success" : "failed!");
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

    case '?':
      Serial.PRINTF_LN("\r\nHot key help:");
      Serial.PRINTF_LN("  3 - SPI Flash Read Status Registers, 3 Bytes");
      Serial.PRINTF_LN("  a - Analyze Status Register Writes and set selection (Modal) flags accordingly");
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
      Serial.PRINTF_LN("  Q - Set SR, non-volatile WEL 06h, then set QE/S%u=1", modal_qe_bit);
      Serial.PRINTF_LN("  q - Set SR, non-volatile WEL 06h, then set QE/S%u=0", modal_qe_bit);
      Serial.PRINTF_LN("  E - Set SR, volatile 50h, then set QE/S%u=1", modal_qe_bit);
      Serial.PRINTF_LN("  e - Set SR, volatile 50h, then set QE/S%u=0", modal_qe_bit);
      Serial.PRINTF_LN();
      Serial.PRINTF_LN("  h - Test /HOLD digitalWrite( 9, LOW)");
      Serial.PRINTF_LN("  w - Test /WP   digitalWrite(10, LOW) and write to Flash");
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

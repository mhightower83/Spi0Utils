#include "SFDP.h"

////////////////////////////////////////////////////////////////////////////////
// Discoveries
bool use_S9 = false;
bool use_S6 = false;
bool use_16_bit = false;
bool has_volatile = false;

// Modal
bool qe_was_preset = false;
bool write_status_register_16_bit = true;
uint32_t mode_qe_bit = 9u;

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
  // No change write
  // Note flash_gd25q32c_read_status has a bug where it needlessly enables WEL
  // This is not an issue for us because it is followed by a
  // flash_gd25q32c_write_status call.
  uint32_t status = flash_gd25q32c_read_status(reg_0idx);
  flash_gd25q32c_write_status(reg_0idx, status); // 8-bit status register write
  bool success = true;
  if (is_WEL()) {
    ETS_PRINTF("* WEL left set after %u-bit Status Register-%u write.\n", 8u, reg_0idx + 1);
    success = false;
  } else {
    ETS_PRINTF("  Pass - %u-bit Write Status Register-%u.\n", 8u, reg_0idx + 1);
  }

  uint32_t new_status = flash_gd25q32c_read_status(reg_0idx);
  if (new_status != status) {
    ETS_PRINTF("* Status Register changed!!! after %u-bit Status Register-%u write.\n", 8u, reg_0idx + 1);
    success = false;
  }
  return success;
}

bool test_sr1_16_bit_write() {
  uint32_t status  = flash_gd25q32c_read_status(/* SR1 */ 0);
  status |= flash_gd25q32c_read_status(/* SR2 */ 1) << 8;
  spi_flash_write_status(status); // 16-bit status register write
  bool success = true;
  if (is_WEL()) {
    ETS_PRINTF("* WEL left set after %u-bit Status Register-%u write.\n", 16u, 1u);
    success = false;
  } else {
    ETS_PRINTF("  Pass - %u-bit Write Status Register-%u.\n", 16u, 1u);
  }

  uint32_t new_status  = flash_gd25q32c_read_status(/* SR1 */ 0);
  new_status |= flash_gd25q32c_read_status(/* SR2 */ 1) << 8;
  if (new_status != status) {
    ETS_PRINTF("* Status Register changed!!! after %u-bit Status Register-%u write.\n", 16u, 1);
    success = false;
  }
  return success;
}



/*
  Analyze Flash Status Registers for possible support for QE.
  Discover which status registers are available and how they need to be accessed.

  Assumptions made:
  * if 16-bit status register write works, then the QE bit is S9
  * if SR1 and SR2 exist then QE bit is S9
  * if SR2 does not exist and SR1 does exist then QE bit is S6
  * flash that use S15 for QE bit are not paired with the ESP8266.
    * These parts require a unique read and write instruction 3Fh/3Eh for SR2.
    * I don't see any support for these in esptool.py
*/
bool analyzeSR_QE(Print& oStream) {
  oStream.println(F("\nTesting Status Register writes:"));
  bool sr1_8  = test_sr_8_bit_write(0);
  bool sr2_8  = test_sr_8_bit_write(1);
  bool sr3_8  = test_sr_8_bit_write(2);
  bool sr1_16 = test_sr1_16_bit_write();
  spi0_flash_write_disable(); // WEL Cleanup

  oStream.println(F("\nFlash Info Summary:"));
  printFlashChipID(Serial);
  if (sr1_8 || sr2_8 || sr3_8) {
    oStream.printf_P(PSTR("  Support for 8-bit Status Registers writes:"));
    if (sr1_8) oStream.printf_P(PSTR(" SR1"));
    if (sr2_8) oStream.printf_P(PSTR(" SR2"));
    if (sr3_8) oStream.printf_P(PSTR(" SR3"));
    oStream.println();
  }

  if (sr1_16) {
    oStream.printf_P(PSTR("  Support for 16-bit Write Status Register-1\n"));
    oStream.printf_P(PSTR("  May support QE bit at BIT9/S9 with 16-bit Write Status Register-1\n"));
    use_16_bit = true;
    use_S9 = true;
    if (sr2_8) {
      oStream.printf_P(PSTR("    or as 8-bit Write Status Register-2 BIT1\n"));
    }
  } else
  if (sr2_8) {
    oStream.printf_P(PSTR("  May support QE bit at S9 with 8-bit Write Status Register-2 BIT1\n"));
    use_S9 = true;
  } else {
    oStream.printf_P(PSTR("  No Status Register-2 present\n"));
    if (sr1_8) {
      oStream.printf_P(PSTR("  Status Register-1 may support QE/WPdis at BIT6/S6\n"));
      use_S6 = true;
    }
  }

  union SFDP_REVISION rev = get_sfdp_revision();
  if (rev.u32) {
    oStream.printf_P(PSTR("  SFDP Revision: %u.%02u, 1ST Parameter Table Revision: %u.%02u\n"),
      rev.hdr_major,  rev.hdr_minor, rev.parm_major, rev.parm_minor);
  } else {
    oStream.printf_P(PSTR("  No SFDP available\n"));
  }

  return use_S9 || use_S6;
}

// Evaluate if Status Register volatile writes are supported setting
// QE bit S9/S6
void analyzeSR_QEVolatile(Print& oStream) {
  if (use_S9 || use_S6) {
    oStream.printf_P(PSTR("\nTesting for volatile Status Register write support using QE/%s\n"), (use_S9) ? "S9" : "S6");
  } else {
    oStream.printf_P(PSTR("\nNo Evidence of QE support detected.\n"));
    return;
  }

  if (use_S9) {
    uint32_t sr2 = 0;
    spi0_flash_read_status_register_2(&sr2); // flash_gd25q32c_read_status(/* SR2 */ 1);
    if (BIT1 & sr2) {
      oStream.printf_P(PSTR("  S9 bit already set cannot test for volatile Status Register write support.\n"));
    } else {
      sr2 |= BIT1;    // S9
      if (use_16_bit) {
        uint32_t sr1 = 0;
        spi0_flash_read_status_register_1(&sr1);
        sr1 |= sr2 << 8;
        spi0_flash_write_status_register(/* SR1 */ 0, sr1, volatile_bit, 16);
      } else {
        spi0_flash_write_status_register(/* SR2 */ 1, sr2, volatile_bit, 8);
      }

      uint32_t new_sr2 = 0;
      spi0_flash_read_status_register_2(&new_sr2); // flash_gd25q32c_read_status(/* SR2 */ 1);
      if (BIT1 & new_sr2) {
        has_volatile = true;
        oStream.printf_P(PSTR("  S9 volatile Status Register write support confirmed.\n"));
      }
    }

  } else
  if (use_S6) {
    uint32_t sr1 = 0;
    spi0_flash_read_status_register_1(&sr1);
    if (BIT6 & sr1) {
      oStream.printf_P(PSTR("  S6 bit already set cannot test for volatile Status Register write support.\n"));
    } else {
      sr1 |= BIT6;
      spi0_flash_write_status_register(/* SR1 */ 0, sr1, volatile_bit, 8);

      uint32_t new_sr1 = 0;
      spi0_flash_read_status_register_1(&new_sr1);
      if (BIT6 & new_sr1) {
        has_volatile = true;
        oStream.printf_P(PSTR("  S6 volatile Status Register write support confirmed.\n"));
      }
    }
  }

  if (! has_volatile)
    oStream.printf_P(PSTR("* Volatile Status Register writes not supported.\n"));
}

void printAllSRs(Print& oStream) {
  uint32_t status = 0;
  SpiOpResult ok0 = spi0_flash_read_status_registers_3B(&status);
  if (SPI_RESULT_OK == ok0) {
    oStream.printf_P(PSTR("  spi0_flash_read_status_registers_3B(0x%06X)\n"), status);
    if (kQEBit2B == (status & kQEBit2B)) oStream.printf_P(PSTR("    QE=1\n"));
    if (kWELBit  == (status & kWELBit))  oStream.printf_P(PSTR("    WEL=1\n"));
  } else {
    oStream.printf_P(PSTR("  spi0_flash_read_status_registers_3B() failed!\n"));
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void cmdLoop(Print& oStream, int key) {
  switch (key) {
    case '8':
      write_status_register_16_bit = false;
      oStream.printf_P(PSTR("%s 8 - Modal:  8-bits Write Status Register\r\n"), (write_status_register_16_bit) ? " " : ">");
      break;
    case '7':
      write_status_register_16_bit = true;
      oStream.printf_P(PSTR("%s 7 - Modal: 16-bits Write Status Register\r\n"), (write_status_register_16_bit) ? ">" : " ");
      break;
    case '6':
      mode_qe_bit = 6u;
      oStream.printf_P(PSTR("%s 6 - Modal: S6/QE Status Register\r\n"), (6u == mode_qe_bit) ? ">" : " ");
      break;
    case '9':
      mode_qe_bit = 9u;
      oStream.printf_P(PSTR("%s 9 - Modal: S9/QE Status Register\r\n"), (9u == mode_qe_bit) ? ">" : " ");
      break;
    case '5':
      qe_was_preset = (qe_was_preset) ? false : true;
      oStream.printf_P(PSTR("%s 5 - Modal: Use values as set for tests\r\n"), (qe_was_preset) ? ">" : " ");
      break;

    case 'a':
      if (analyzeSR_QE(oStream)) {
        if (use_S6) {
          mode_qe_bit = 6u;
        } else
        if (use_S9) {
          mode_qe_bit = 9u;
        } else {
          mode_qe_bit = 0xFFu;  // undefined
        }
        write_status_register_16_bit = use_16_bit;
        analyzeSR_QEVolatile(oStream);
      }
      break;

    case 'f':
      printSfdpReport();
      break;

    case 'v':
      testInput_GPIO9_GPIO10(oStream, mode_qe_bit, write_status_register_16_bit, (has_volatile) ? volatile_bit : non_volatile_bit, qe_was_preset);
      break;

    // Use settings found from analyze or those selected previously through the
    // menu.
    case 'w':
      test_set_SRP0(oStream, mode_qe_bit, write_status_register_16_bit, volatile_bit, qe_was_preset);
      printAllSRs(oStream);
      testOutputGPIO10(oStream, mode_qe_bit, write_status_register_16_bit, (has_volatile) ? volatile_bit : non_volatile_bit, qe_was_preset);
      test_clear_SR1(oStream, mode_qe_bit, write_status_register_16_bit, volatile_bit, qe_was_preset);
      printAllSRs(oStream);
      break;
    // "
    case 'h':
      testOutputGPIO9(oStream, mode_qe_bit, write_status_register_16_bit, (has_volatile) ? volatile_bit : non_volatile_bit, qe_was_preset);
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
        uint32_t status = 0;
        SpiOpResult ok0 = spi0_flash_read_status_registers_3B(&status);
        oStream.printf("\n%d = spi0_flash_read_status_registers_3B(0x%06X)%s\n", ok0, status, (SPI_RESULT_OK == ok0) ? "" : " - failed!\n");
        bool non_volatile = ('Q' == key) || ('q' == key);

        uint32_t val = 0x00u;
        if (write_status_register_16_bit) {
          if ('Q' == key || 'E' == key) val = (9u == mode_qe_bit) ? BIT9 : BIT6;
          ok0 = spi0_flash_write_status_registers_2B(val, non_volatile);
          oStream.printf("\n%d = spi0_flash_write_status_registers_2B(0x%02X, %s)%s\n", ok0, val, (non_volatile) ? "non-volatile" : "volatile", (SPI_RESULT_OK == ok0) ? "" : " - failed!\n");
        } else {
          if ('Q' == key || 'E' == key) val = (9u == mode_qe_bit) ? BIT9 >> 8u : BIT6;
          if (9u == mode_qe_bit) {
            ok0 = spi0_flash_write_status_register_2(val, non_volatile);
          } else {
            ok0 = spi0_flash_write_status_register_1(val, non_volatile);
          }
          oStream.printf("\n%d = spi0_flash_write_status_register_2(0x%02X, %s)%s\n", ok0, val, (non_volatile) ? "non-volatile" : "volatile", (SPI_RESULT_OK == ok0) ? "" : " - failed!\n");
        }

        ok0 = spi0_flash_read_status_registers_3B(&status);
        oStream.printf("\n%d = spi0_flash_read_status_registers_3B(0x%06X)%s\n", ok0, status, (SPI_RESULT_OK == ok0) ? "" : " - failed!\n");
      }
      break;

    case 'r':
      if (qe_was_preset || gpio_9_10_available) {
        oStream.println(F("reclaim_GPIO_9_10() has already been called. Reboot to run again."));
      } else {
        oStream.println(F("Test call to reclaim_GPIO_9_10()"));
        gpio_9_10_available = reclaim_GPIO_9_10();
      }
      qe_was_preset = true;
      break;

    case '3':
      printAllSRs(oStream);
      break;

    case 'R':
      oStream.println(F("Restart ..."));
      test_clear_SR1(oStream, mode_qe_bit, write_status_register_16_bit, volatile_bit, qe_was_preset);
      oStream.flush();
      ESP.restart();
      break;

    case 'd':
      {
        SpiOpResult ok0 = spi0_flash_write_disable();
        oStream.printf("\n%d = spi0_flash_write_disable()%s\n", ok0, (SPI_RESULT_OK == ok0) ? "" : " - failed!\n");
        uint32_t status = 0;
        ok0 = spi0_flash_read_status_registers_3B(&status);
        oStream.printf("\n%d = spi0_flash_read_status_registers_3B(0x%06X)%s\n", ok0, status, (SPI_RESULT_OK == ok0) ? "" : " - failed!\n");
      }
      break;
    //
    case 'S':
      // Force clear /WP
      pinMode(10, OUTPUT);
      digitalWrite(10, HIGH);
      oStream.println(F("\nSPI Flash Software Reset (66h, 99h):"));
      if (SPI_RESULT_OK == spi0_flash_software_reset()) {
        auto ok0 = spi0_flash_write_status_register_2(0x00u, true);
        if (SPI_RESULT_OK == ok0) {
          oStream.println(F("  Success"));
        } else {
          oStream.println(F("  Failed"));
        }
      } else {
        oStream.println(F("  Failed"));
      }
      break;

    case '?':
      oStream.printf_P(PSTR("\r\nHot key help:\r\n"));
      oStream.printf_P(PSTR("  3 - SPI Flash Read Status Registers, 3 Bytes\r\n"));
      oStream.printf_P(PSTR("  a - Analyze Status Register Writes and set selection flags accordingly\r\n"));
      oStream.printf_P(PSTR("  f - Print SFDP Data\r\n"));
      oStream.println();
      oStream.printf_P(PSTR("%s 8 - Modal:  8-bits Write Status Register\r\n"), (write_status_register_16_bit) ? " " : ">");
      oStream.printf_P(PSTR("%s 7 - Modal: 16-bits Write Status Register\r\n"), (write_status_register_16_bit) ? ">" : " ");
      oStream.printf_P(PSTR("%s 6 - Modal: S6/QE Status Register\r\n"), (6u == mode_qe_bit) ? ">" : " ");
      oStream.printf_P(PSTR("%s 9 - Modal: S9/QE Status Register\r\n"), (9u == mode_qe_bit) ? ">" : " ");
      oStream.printf_P(PSTR("%s 5 - Modal: Use values as set for test preset values\r\n"), (qe_was_preset) ? ">" : " ");
      oStream.println();
      oStream.printf_P(PSTR("  Q - Set SR, non-volatile WEL 06h, then set QE/S%u=1\r\n"), mode_qe_bit);
      oStream.printf_P(PSTR("  q - Set SR, non-volatile WEL 06h, then set QE/S%u=0\r\n"), mode_qe_bit);
      oStream.printf_P(PSTR("  E - Set SR, volatile 50h, then set QE/S%u=1\r\n"), mode_qe_bit);
      oStream.printf_P(PSTR("  e - Set SR, volatile 50h, then set QE/S%u=0\r\n"), mode_qe_bit);
      oStream.println();
      oStream.printf_P(PSTR("  h - Test /HOLD digitalWrite( 9, LOW)\r\n"));
      oStream.printf_P(PSTR("  w - Test /WP   digitalWrite(10, LOW) and write to Flash\r\n"));
      oStream.printf_P(PSTR("  v - Test GPIO9 and GPIO10 INPUT\r\n"));
      oStream.printf_P(PSTR("%s r - Test call to 'reclaim_GPIO_9_10()'%s\r\n"), (gpio_9_10_available) ? ">" : " ", (gpio_9_10_available) ? " - already called" : "");
      oStream.println();
      oStream.printf_P(PSTR("  d - Clear WEL bit, write enable\r\n"));
      oStream.printf_P(PSTR("  S - SPI Flash - Software Reset 66h, 99h\r\n"));
      oStream.printf_P(PSTR("  R - Restart\r\n"));
      oStream.printf_P(PSTR("  ? - This help message\r\n"));
      break;
/*
  Test /WP
    * digitalWrite(10, LOW);
  Test /HOLD
    * digitalWrite(9, LOW);

  Mode:  8-bit Write SR
  Mode: 16-bit Write SR
  Mode: QE/S6
  Mode  QE/S9

  Set QE/S9
  Clear QE/S9

  Set QE/S6
  Clear QE/S6
*/
    default:
      break;
  }
  oStream.println();
}

void serialClientLoop(void) {
  if (0 < Serial.available()) {
    char hotKey = Serial.read();
    cmdLoop(Serial, hotKey);
  }
}

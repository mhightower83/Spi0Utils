#include "SFDP.h"

////////////////////////////////////////////////////////////////////////////////
// Discoveries
bool use_S9 = false;
bool use_S6 = false;
bool use_16_bit = false;
bool has_volatile = false;

void testGpio9Gpio10() {
  bool has_QE = false;
  char bit = '?';
  if (use_S9) {
    has_QE = is_QE();
    bit = '9';
  } else
  if (use_S6) {
    has_QE = is_S6_QE_WPDis();
    bit = '6';
  }

  ETS_PRINTF("  Test: QE/S%c=%c, GPIO pins 9 and 10 as INPUT\n", bit, (has_QE) ? '1' : '0');
  pinMode(9, INPUT);
  pinMode(10, INPUT);
  ETS_PRINTF("  digitalRead result: GPIO_9(%u) and GPIO_10(%u)\n", digitalRead(9), digitalRead(10));
  delay(10);

  ETS_PRINTF("  Test: QE/S%c=%c, GPIO pins 9 and 10 as OUTPUT set LOW\n", bit, (has_QE) ? '1' : '0');
  pinMode(9, OUTPUT);       // was /HOLD
  pinMode(10, OUTPUT);      // was /WP
  digitalWrite(9, LOW);     // Failure when part is put on hold and WDT Reset occurs or an exception
  digitalWrite(10, LOW);
  delay(10);

  if (has_QE) {
    ETS_PRINTF("  Passed - have not crashed\n"); // No WDT Reset - then it passed.
  } else {
    ETS_PRINTF("* Ambiguous results. QE=0 and we did not crash. Flash may not support /HOLD.\n");
  }
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
// Rely on WEL bit left on when a status register is not supported.
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
*/
bool analyzeSR_QE(Print& oStream) {
  oStream.println(F("\nTesting Status Register writes:"));
  bool sr1_8  = test_sr_8_bit_write(0);
  bool sr2_8  = test_sr_8_bit_write(1);
  bool sr3_8  = test_sr_8_bit_write(2);
  bool sr1_16 = test_sr1_16_bit_write();
  spi0_flash_write_disable(); // Cleanup

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
    oStream.printf_P(PSTR("\bNo Evidence of QE support detected.\n"));
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
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////



// Modal
bool write_status_register_16_bit = false;

void cmdLoop(Print& oStream, int key) {
  switch (key) {
    case 's':
      if (analyzeSR_QE(oStream)) {
        analyzeSR_QEVolatile(oStream);
      }
      break;

    case 'f':
      printSfdpReport();
      break;

    case 'v':
      oStream.println(F("\nRunning GPIO9 and GPIO10 verification test"));
      oStream.println(F("Test GPIO9 and GPIO10 with 'pinMode()' for INPUT and OUTPUT operation"));
      testGpio9Gpio10();
      break;

    // case 'w':
    //   oStream.println(F("\nRunning pin function /WP disabled verification test"));
    //   oStream.println(F("Set GPIO10 with 'pinMode() to OUTPUT and confirm writes work when LOW."));
    //   test_GPIO10_write();
    //   break;


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
          if ('Q' == key || 'E' == key) val = 0x0200u;
          ok0 = spi0_flash_write_status_registers_2B(val, non_volatile);
          oStream.printf("\n%d = spi0_flash_write_status_registers_2B(0x%02X, %s)%s\n", ok0, val, (non_volatile) ? "non-volatile" : "volatile", (SPI_RESULT_OK == ok0) ? "" : " - failed!\n");
        } else {
          if ('Q' == key || 'E' == key) val = 0x02u;
          ok0 = spi0_flash_write_status_register_2(val, non_volatile);
          oStream.printf("\n%d = spi0_flash_write_status_register_2(0x%02X, %s)%s\n", ok0, val, (non_volatile) ? "non-volatile" : "volatile", (SPI_RESULT_OK == ok0) ? "" : " - failed!\n");
        }
        ok0 = spi0_flash_read_status_registers_3B(&status);
        oStream.printf("\n%d = spi0_flash_read_status_registers_3B(0x%06X)%s\n", ok0, status, (SPI_RESULT_OK == ok0) ? "" : " - failed!\n");
      }
      break;

    case 'r':
      oStream.println(F("Test call to reclaim_GPIO_9_10()"));
      reclaim_GPIO_9_10();
      break;

    case '3':
      {
        uint32_t status = 0;
        SpiOpResult ok0 = spi0_flash_read_status_registers_3B(&status);
        if (SPI_RESULT_OK == ok0) {
          ETS_PRINTF("\nspi0_flash_read_status_registers_3B(0x%06X)\n", status);
          if (kQEBit2B == (status & kQEBit2B)) ETS_PRINTF("  QE=1\n");
          if (kWELBit  == (status & kWELBit))  ETS_PRINTF("  WEL=1\n");
        } else {
          ETS_PRINTF("\nspi0_flash_read_status_registers_3B() failed!\n");
        }
      }
      break;

    case 'R':
      oStream.println(F("Restart ..."));
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
      oStream.println(F("\r\nHot key help:"));
      oStream.println(F("  3 - SPI Flash Read Status Registers, 3 Bytes"));
      oStream.println(F("  s - Test Status Register Writes"));
      oStream.println(F("  f - Print SFDP Data"));
      oStream.println(F("  r - Test call to 'reclaim_GPIO_9_10()'"));
      oStream.println(F("  v - Test GPIO9 and GPIO10 with 'pinMode()' for INPUT and OUTPUT operation"));
      // oStream.println(F("  w - Test GPIO10 with 'pinMode() for OUTPUT and confirm writes work when LOW"));
      oStream.println();
      oStream.printf_P(PSTR("  8 - Set Write Status Register-2 Mode, 8-bits %s\r\n"), (write_status_register_16_bit) ? "" : "<");
      oStream.printf_P(PSTR("  6 - Set Write Status Register-1 Mode, 16-bits %s\r\n"), (write_status_register_16_bit) ? "<" : "");
      oStream.println(F("  Q - Test, non-volatile, set WEL then set QE=1"));
      oStream.println(F("  q - Test, non-volatile, set WEL then set QE=0"));
      oStream.println(F("  E - Test, volatile 50h, then set QE=1"));
      oStream.println(F("  e - Test, volatile 50h, then set QE=0"));
      oStream.println();
      oStream.println(F("  d - Clear WEL bit, write enable"));
      oStream.println(F("  S - SPI Flash - Software Reset 66h, 99h"));
      oStream.println(F("  R - Restart"));
      oStream.println(F("  ? - This help message\r\n"));
      break;

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

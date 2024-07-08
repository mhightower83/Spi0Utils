/*
  Test that pin function /WP can be disabled
  Returns true on a successful write

  Uses Status Register Protect bit to test writing.
  Currently uses P0 bit for test

  Test that pin function /HOLD can be disabled
  If it can return it worked.

  Notes:
    GPIO9 may work because the SPI Flash chip does not implement a /HOLD. eg. EN25Q32C
    GPIO10 may work because other Status Register Bits indicate not to use /WP.
    eg. SRP1:SRP0 = 0:0.  There are also parts that do no have pin function /WP.
*/

#include <Arduino.h>
#include <SpiFlashUtils.h>
#include "WP_HOLD_Test.h"
#define PRINTF(a, ...)        printf_P(PSTR(a), ##__VA_ARGS__)
#define PRINTF_LN(a, ...)     printf_P(PSTR(a "\r\n"), ##__VA_ARGS__)

#if 1
#define VERBOSE_PRINTF Serial.PRINTF
#define VERBOSE_PRINTF_LN Serial.PRINTF_LN
#else
#define VERBOSE_PRINTF(a, ...) do { (void)a; } while(false)
#define VERBOSE_PRINTF_LN(a, ...) do { (void)a; } while(false)
#endif

////////////////////////////////////////////////////////////////////////////////
//
// For QE/S9 case only, set SR1 and SR2 such that SRP0=1 and SRP1=0
// clear all other bits including QE.
//
// This causes the pin function /WP to be enabled on some devices for non-quad
// instructions regardless of QE=1 state.
//
// return 0 on succees, mask of bits stuck on, or all ones when BIT7 fails to set.
uint32_t test_set_SRP1_SRP0_clear_QE([[maybe_unused]] const uint32_t qe_bit, const bool use_16_bit_sr1, const bool _non_volatile) {
  using namespace experimental;

  // Only call for QE/S9 case
  if (9u != qe_bit) panic();

  spi0_flash_write_disable(); // For some devices, EN25Q32C, this clears OTP mode.
  digitalWrite(10u, HIGH);     // ensure /WP is not asserted
  pinMode(10u, OUTPUT);

  uint32_t sr1 = 0u;
  spi0_flash_read_status_register_1(&sr1);

  if (BIT7 == (BIT7 & sr1)) {
    Serial.PRINTF_LN("  SRP0 already set.");
  }
  sr1 = BIT7;   // SRP1 must be zero to avoid permanently protected!
  if (use_16_bit_sr1) {
    spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 16);
  } else {
    spi0_flash_write_status_register(/* SR2 */ 1u, 0u, _non_volatile, 8);
    spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 8);
    // Just in case the clearing order was wrong for allowing writes to SR2
    spi0_flash_write_status_register(/* SR2 */ 1u, 0u, _non_volatile, 8);
  }

  // Verify
  uint32_t sr21 = 0u, sr2 = 0u;
  spi0_flash_read_status_register_1(&sr21);
  sr21 &= 0xFCu;

  spi0_flash_read_status_register_2(&sr2);
  sr21 |= (sr2 << 8u);

  if (BIT7 == (BIT7 & sr1)) {
    sr21 &= ~BIT7;
  } else {
    sr21 = ~0u;
  }

  digitalWrite(10u, SPECIAL);
  return sr21;
}

////////////////////////////////////////////////////////////////////////////////
//
// For QE/S9 and QE/S6 cases, clears SR1 (and SR2)
//
// This is needed to completely disable pin function /WP on some devices for
// non-quad instructions. Contrary to datasheets QE=1 was not enough to disable
// /WP. ie. Winbond, BergMicro, XMC.
//
uint32_t test_clear_SRP1_SRP0_QE(const bool has_8bw_sr2, const bool use_16_bit_sr1, const bool _non_volatile) {
  using namespace experimental;

  uint32_t
  spi0_flash_write_disable();
  digitalWrite(10u, HIGH);     // ensure /WP is not asserted
  pinMode(10u, OUTPUT);

  if (use_16_bit_sr1) {
    spi0_flash_write_status_register(/* SR1 */ 0u, 0u, _non_volatile, 16);
  } else {
    spi0_flash_write_status_register(/* SR1 */ 0u, 0u, _non_volatile, 8);
    if (has_8bw_sr2) {
      spi0_flash_write_status_register(/* SR2 */ 1u, 0u, _non_volatile, 8);
      spi0_flash_write_status_register(/* SR1 */ 0u, 0u, _non_volatile, 8);
    }
  }

  // Verify
  uint32_t sr21 = 0u, sr2 = 0u;
  spi0_flash_read_status_register_1(&sr21);
  sr21 &= 0xFCu;
  if (has_8bw_sr2) {
    spi0_flash_read_status_register_2(&sr2);
    sr21 |= (sr2 << 8u);
  }

  digitalWrite(10u, SPECIAL);
  return sr21;
}
// bool test_clear_SRP1_SRP0_QE(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool _non_volatile)
// if (9u == qe_bit) {
//   if (use_16_bit_sr1) {
//     spi0_flash_write_status_register(/* SR1 */ 0u, 0u, _non_volatile, 16);
//   } else {
//     spi0_flash_write_status_register(/* SR1 */ 0u, 0u, _non_volatile, 8);
//     spi0_flash_write_status_register(/* SR2 */ 1, 0u, _non_volatile, 8);
//     spi0_flash_write_status_register(/* SR1 */ 0u, 0u, _non_volatile, 8);
//   }
// } else {
//   // 6u == qe_bit || 0xFFu == qe_bit
//   if (use_16_bit_sr1) {
//     spi0_flash_write_status_register(/* SR1 */ 0u, 0u, _non_volatile, 16);
//   } else {
//     spi0_flash_write_status_register(/* SR1 */ 0u, 0u, _non_volatile, 8);
//     // I haven't seen a QE/S6 device that has SR2
//   }
// }

////////////////////////////////////////////////////////////////////////////////
//
// use_preset == false, Sets the proposed QE bit and
// verifies that the bit is set returns true if set.
//
// use_preset == true, check current QE bit and
// returns true when QE is already set. Intended for using the current
// Status Register settings.
//
uint32_t test_set_QE(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool _non_volatile, const bool use_preset) {
  using namespace experimental;

  spi0_flash_write_disable();
  uint32_t qe = 0u;

  // check and report state of QE
  if (use_preset) {
    uint32_t sr1 = 0u, sr2 = 0u;
    if (9u == qe_bit) {
      spi0_flash_read_status_register_2(&sr2);
      qe = BIT1 == (BIT1 & sr2);
    } else
    if (6u == qe_bit) {
      spi0_flash_read_status_register_1(&sr1);
      qe = BIT6 == (BIT6 & sr1);
    }
    Serial.PRINTF_LN("  QE/S%x=%u used", qe_bit, (qe) ? 1u : 0u);
    return qe;
  }

  // Set QE and report result true if successful
  if (9u == qe_bit) {
    uint32_t sr2 = 0u;
    spi0_flash_read_status_register_2(&sr2); // flash_gd25q32c_read_status(/* SR2 */ 1);
    qe = BIT1 == (BIT1 & sr2);
    if (qe) {
      Serial.PRINTF_LN("  QE/S%u already set.", qe_bit);
    } else {
      sr2 |= BIT1;    // S9
      if (use_16_bit_sr1) {
        uint32_t sr1 = 0u;
        spi0_flash_read_status_register_1(&sr1);
        sr1 |= sr2 << 8u;
        spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 16u);
      } else {
        spi0_flash_write_status_register(/* SR2 */ 1u, sr2, _non_volatile, 8u);
      }
      // Verify
      sr2 = 0u;
      spi0_flash_read_status_register_2(&sr2); // flash_gd25q32c_read_status(/* SR2 */ 1);
      qe =  BIT1 == (BIT1 & sr2);
    }
  } else
  if (6u == qe_bit) {
    uint32_t sr1 = 0u;
    spi0_flash_read_status_register_1(&sr1);
    qe = BIT6 == (BIT6 & sr1);
    if (qe) {
      Serial.PRINTF_LN("  QE/S%u already set.", qe_bit);
    } else {
      sr1 |= BIT6;
      spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 8u);
      // verify
      sr1 = 0u;
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
bool testFlashWrite(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool _non_volatile) {
  using namespace experimental;

  bool test = false;
  spi0_flash_write_disable();

  if (9u == qe_bit) {
    // bool _non_volatile = true;
    uint32_t sr1 = 0u, sr2 = 0u;
    spi0_flash_read_status_register_1(&sr1);
    spi0_flash_read_status_register_2(&sr2);
    sr1 &= BIT7;  // Keep SRP0 asis
    sr1 |= BIT2;  // set PM0
    sr2 &= BIT1;  // Keep QE/S9
    if (use_16_bit_sr1) {
      sr1 |= sr2 << 8u;
      spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 16);
    } else {
      spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 8);
    }
    uint32_t verify_sr1 = 0u;
    spi0_flash_read_status_register_1(&verify_sr1);
    test = BIT2 == (BIT2 & verify_sr1);
    sr1 &= ~BIT2;   // clear PM0
    if (use_16_bit_sr1) {
      spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 16);
    } else {
      spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 8);
    }
  } else
  if (6 == qe_bit) {
    // No SRP0 or SRP1
    uint32_t sr1 = 0u;
    spi0_flash_read_status_register_1(&sr1);
    sr1 &= BIT6;  // QE/S6 or WPDis
    sr1 |= BIT2;  // set PM0
    spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 8);
    uint32_t verify_sr1 = 0u;
    spi0_flash_read_status_register_1(&verify_sr1);
    test = BIT2 == (BIT2 & verify_sr1);
    sr1 &= ~BIT2;
    spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 8);
  } else
  if (0xFFu == qe_bit) {
    // No QE, SRP0 or SRP1
    uint32_t sr1 = 0u, sr2 = 0u;
    spi0_flash_read_status_register_1(&sr1);
    sr1 |= BIT2;  // set PM0
    if (use_16_bit_sr1) {
      spi0_flash_read_status_register_2(&sr2);
      sr1 |= sr2 << 8u;
      spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 16);
    } else {
      spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 8);
    }
    uint32_t verify_sr1 = 0u;
    spi0_flash_read_status_register_1(&verify_sr1);
    test = BIT2 == (BIT2 & verify_sr1);
    sr1 &= ~BIT2;   // clear PM0
    if (use_16_bit_sr1) {
      spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 16);
    } else {
      spi0_flash_write_status_register(/* SR1 */ 0u, sr1, _non_volatile, 8);
    }
  }
  return test;
}


static int get_SRP10(const uint32_t qe_bit) {
  using namespace experimental;

  if (qe_bit != 9) return 0;

  uint32_t status = 0u;
  if (SPI_RESULT_OK == spi0_flash_read_status_registers_2B(&status)) return ((status >> 7u) & 3u); // return SRP1:SRP0
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
//
// Verify that Flash pin function /WP (shared with GPIO10) can be disabled.
//
// There are three situations:
//  1. Flash allows write with /WP LOW when QE=1 and fail when QE=0 and SRP1:SRP0=0:1
//  2. Flash doesn't care about QE. They allow writes with /WP LOW when SRP1:SRP0=0:0 and block when SRP1:SRP0=0:1.
//  3. Flash doesn't ever monitor /WP.
//
#if 0
// for reference in .h file
struct OutputTestResult {
  uint32_t qe_bit:8;          // 0xFF uninitialized, 7 for S7 and 9 for S9
  uint32_t srp0:1;            // BIT7 of SR321
  uint32_t srp1:1;            // BIT8 of SR321
  uint32_t low:1;             // pass/fail results for GPIO10 LOW
  uint32_t high:1;            // pass/fail results for GPIO10 HIGH
  uint32_t qe:1;              // QE bit
};
#endif

bool testOutputGPIO10(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool _non_volatile, const bool use_preset) {
  using namespace experimental;
  bool pass = false;

  VERBOSE_PRINTF_LN("\nRunning verification test for pin function /WP disable");
  if (9u != qe_bit && 6u != qe_bit && 0xFFu != qe_bit) {
    VERBOSE_PRINTF_LN("* QE/S%u bit field specification undefined should be either S6 or S9", qe_bit);
    return pass;
  }
  digitalWrite(10u, HIGH);     // ensure /WP is not asserted, otherwise test_set_QE may fail
  pinMode(10u, OUTPUT);

  uint32_t _QE = test_set_QE(qe_bit, use_16_bit_sr1, _non_volatile, use_preset);
  int srp10_WP = get_SRP10(qe_bit); // already masked with 3

  if (9u == qe_bit) {
    VERBOSE_PRINTF_LN("  Test Write: QE/S%x=%u SRP1:SRP0=%u:%u, GPIO10 as OUTPUT",
      qe_bit, _QE, (srp10_WP >> 1u) & 1u, srp10_WP & 1u);
  } else
  if (6u == qe_bit) {
    VERBOSE_PRINTF_LN("  Test Write: QE/S%x=%u GPIO10 as OUTPUT", qe_bit, _QE);
  } else {
    VERBOSE_PRINTF_LN("  Test Write: No QE bit GPIO10 as OUTPUT");
  }
  VERBOSE_PRINTF_LN("  Test Write: using %svolatile Status Register", (_non_volatile) ? "non-" : "");

  // With pin 10, HIGH, expect success regardless of QE and SRP1 and SRP0
  pass = testFlashWrite(qe_bit, use_16_bit_sr1, _non_volatile);
  VERBOSE_PRINTF_LN("  Test Write: With /WP set %s write %s", "HIGH", (pass) ? "succeeded" : "failed.");

  // Expect success if QE=1 and/or SRP1:0=0:0 are relavent, failure otherwise
  digitalWrite(10u, LOW);
  pass = testFlashWrite(qe_bit, use_16_bit_sr1, _non_volatile);
  VERBOSE_PRINTF_LN("  Test Write: With /WP set %s write %s", "LOW", (pass) ? "succeeded" : "failed.");

  pinMode(10u, SPECIAL);
  return pass;
}

//
//  Verify that Flash pin function /HOLD (shared with GPIO9) can be disabled.
//
bool testOutputGPIO9(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool _non_volatile, const bool use_preset) {
  using namespace experimental;
  bool pass = false;

  Serial.PRINTF_LN("\nRunning verification test for pin function /HOLD disable");
  if (9u != qe_bit && 6u != qe_bit && 0xFFu != qe_bit) {
    Serial.PRINTF_LN("* QE/S%x bit field specification undefined should be either S6 or S9", qe_bit);
    return pass;
  }

  uint32_t _qe = test_set_QE(qe_bit, use_16_bit_sr1, _non_volatile, use_preset);
  Serial.PRINTF_LN("  Verify /HOLD is disabled by Status Register QE/S%u=%u", qe_bit, _qe);
  Serial.PRINTF_LN("  If changing GPIO9 to OUTPUT and setting LOW crashes, it failed.");
  pinMode(9u, OUTPUT);
  digitalWrite(9u, LOW);
  if (_qe) {
    Serial.PRINTF_LN("  passed - QE/S%u bit setting worked.", qe_bit); // No WDT Reset - then it passed.
    pass = true;
  } else {
    Serial.PRINTF_LN("* Unexpected results. QE=0 and we did not crash. Flash may not support /HOLD.");
    pass = true;  // Uhh, it didn't crash - must be OK
  }

  pinMode(9u, SPECIAL);
  return pass; // ambiguous
}

////////////////////////////////////////////////////////////////////////////////
//
bool testInput_GPIO9_GPIO10(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool _non_volatile, const bool use_preset) {
  using namespace experimental;

  Serial.PRINTF_LN("\nRunning GPIO9 and GPIO10 INPUT test");
  Serial.PRINTF_LN("Test GPIO9 and GPIO10 by reading from the pins as INPUT and print result.");
  if (9u != qe_bit && 6u != qe_bit) {
    Serial.PRINTF_LN("* QE/S%u bit field specification undefined should be either S6 or S9", qe_bit);
    return false;
  }
  uint32_t _qe = test_set_QE(qe_bit, use_16_bit_sr1, _non_volatile, use_preset);
  Serial.PRINTF_LN("  Test: QE/S%u=%u, GPIO pins 9 and 10 as INPUT", qe_bit, _qe);
  pinMode(9u, INPUT);
  pinMode(10u, INPUT);
  uint32_t pin9 = digitalRead(9u);
  Serial.PRINTF_LN("  digitalRead result: GPIO_9(%u) and GPIO_10(%u)", pin9, digitalRead(10u));

  if (0u == pin9){
    if (_qe) {
      Serial.PRINTF_LN("  Passed - no crash"); // No WDT Reset - then it passed.
      return true;
    } else {
      Serial.PRINTF_LN("* Ambiguous results. QE=0 and we did not crash. Flash may not support /HOLD.");
    }
  }
  return false; // inconclusive
}

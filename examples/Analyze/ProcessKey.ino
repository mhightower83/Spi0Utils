/*
 *   Copyright 2024 M Hightower
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
/*
Hotkeys used for scripting "Analyze"

Analyze flash memory properties
  'a' with bias toward S9 for QE and  'A' risky option to force run when
      volatile Status Register is not available.
  'b' with strong bias toward S6 for QE and  'B' risky option to force run when
      volatile Status Register is not available

Test ability to disable pin function /WP
  'w'

Test ability to disable pin function /HOLD
  'h'

Test buildin/custom 'spi_flash_vendor_cases()' function for ability to disable
pin function /WP and /HOLD.
  't'

*/


#if 1
// https://stackoverflow.com/questions/8487986/file-macro-shows-full-path
// This value is resolved at compile time.
#define _FILENAME_ &(strrchr("/" __FILE__, '/')[1])
#define CHECK_OK0(a) \
  if ((a)) { \
    Serial.PRINTF_LN("* ERR: %s:%s:%d: ok0=%u", _FILENAME_, __func__, __LINE__, a); \
    return false; \
  }
#else
#define CHECK_OK0(a) do { (void)a; } while (false)
#endif

////////////////////////////////////////////////////////////////////////////////
//


////////////////////////////////////////////////////////////////////////////////
//
void printSR321(const char *indent, const bool always) {
  using namespace experimental;

  Serial.PRINTF("%sSR321 0x", indent);

  uint32_t sr3 = 0u;
  SpiOpResult ok0 = SPI_RESULT_ERR;
  if (always || fd_state.has_8bw_sr3) {
    ok0 = spi0_flash_read_status_register_3(&sr3);
  }
  if (SPI_RESULT_OK == ok0) {
    Serial.PRINTF("%02X:", sr3);
  } else {
    Serial.PRINTF("--:");
  }

  uint32_t sr2 = 0u;
  ok0 = SPI_RESULT_ERR;
  if (always || fd_state.has_8bw_sr2 || fd_state.has_16bw_sr1) {
    ok0 = spi0_flash_read_status_register_2(&sr2);
  }
  if (SPI_RESULT_OK == ok0) {
    Serial.PRINTF("%02X:", sr2);
  } else {
    Serial.PRINTF("--:");
  }

  uint32_t sr1 = 0u;
  ok0 = spi0_flash_read_status_register_1(&sr1);
  if (SPI_RESULT_OK == ok0) {
    Serial.PRINTF_LN("%02X", sr1);
  } else {
    Serial.PRINTF_LN("--");
  }

  if (kWELBit  == (kWELBit & sr1)) Serial.PRINTF_LN("%s  WEL=1", indent);
  if (fd_state.S9) {
    if (kQES9Bit1B == (kQES9Bit1B & sr1)) Serial.PRINTF_LN("%s  QE/S9=1", indent);
    if (BIT7 == (BIT7 & sr1)) Serial.PRINTF_LN("%s  SRP0=1", indent);
    if (BIT0 == (BIT0 & sr2)) Serial.PRINTF_LN("%s  SRP1=1", indent);
  } else
  if (fd_state.S6) {
    if (kQES6Bit == (kQES6Bit & sr1)) Serial.PRINTF_LN("%s  QE/S6=1", indent);
  }
}

////////////////////////////////////////////////////////////////////////////////
// WEL is essential for our tests. Confirm that the SPI Flash Status Register
// supports a WEL (Write-Enable-Latch) bit.
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
// the instruction is wrong for this flash memory.
//
// Thus we can detect if the register exist and the required length of the write.
//
bool test_sr_8_bit_write(uint32_t reg_0idx) {
  using namespace experimental;

  // No change write
  uint32_t status;
  SpiOpResult ok0 = spi0_flash_read_status_register(reg_0idx, &status);
  CHECK_OK0(ok0);
  if (0u == reg_0idx) {
    status &= 0xFCu;
  }
  ok0 = spi0_flash_write_status_register(reg_0idx, status, non_volatile_bit, 8u);
  CHECK_OK0(ok0);
  if (is_WEL()) {
    Serial.PRINTF("* WEL left set after %u-bit Status Register-%u write.\n", 8u, reg_0idx + 1u);
    return false;
  }

  bool success = true;
  uint32_t new_status;
  ok0 = spi0_flash_read_status_register(reg_0idx, &new_status);
  CHECK_OK0(ok0);
  if (0u == reg_0idx) {
    new_status &= 0xFCu;
  }
  if (new_status != status) {
    Serial.PRINTF(
      "* After a %u-bit Status Register-%u write, the Status Register changed\n"
      "  when it should not have .\n", 8u, reg_0idx + 1u);
    success = false;
  } else {
    Serial.PRINTF("  Pass - %u-bit Write Status Register-%u.\n", 8u, reg_0idx + 1u);
  }
  return success;
}


bool test_sr1_16_bit_write() {
  using namespace experimental;

  uint32_t sr1 = 0u, sr2 = 0u;
  SpiOpResult
  ok0 = spi0_flash_read_status_register(/* SR1 */ 0u, &sr1);
  CHECK_OK0(ok0);
  sr1 &= 0xFCu; // Avoid confusion ignore WEL and WIP
  ok0 = spi0_flash_read_status_register(/* SR2 */ 1u, &sr2);
  CHECK_OK0(ok0);
  sr1 |= (sr2 << 8u);
  ok0 = spi0_flash_write_status_registers_2B(sr1, non_volatile_bit);
  CHECK_OK0(ok0);

  if (is_WEL()) {
    Serial.PRINTF("* WEL left set after %u-bit Status Register-%u write.\n", 16u, 1u);
    return false;
  }

  bool success = true;
  uint32_t new_sr1 = 0u, new_sr2 = 0u;
  ok0 = spi0_flash_read_status_register(/* SR1 */ 0u, &new_sr1);
  CHECK_OK0(ok0);
  new_sr1 &= 0xFCu;
  ok0 = spi0_flash_read_status_register(/* SR2 */ 1u, &new_sr2);
  CHECK_OK0(ok0);
  new_sr1 |= (new_sr2 << 8u);
  if (new_sr1 != sr1) {
    Serial.PRINTF(
      "* After a %u-bit Status Register-%u write, the Status Register changed\n"
      "  when it should not have .\n", 16u, 1u);
    success = false;
  } else {
    Serial.PRINTF("  Pass - %u-bit Write Status Register-%u.\n", 16u, 1u);
  }
  return success;
}


////////////////////////////////////////////////////////////////////////////////
// Clear Status Register bits in register 1 and 2
// Assume both registers are present
//
// prerequisites
//   fd_state.has_8bw_sr2
//   fd_state.has_16bw_sr1
//
// returns true on success
static bool clearSR21(bool _non_volatile) {
  using namespace experimental;

  Serial.PRINTF_LN("  Clear Status Registers 1 and 2");
  uint32_t sr21 = test_clear_SRP1_SRP0_QE(fd_state.has_8bw_sr2, fd_state.has_16bw_sr1, _non_volatile);
  if (sr21) {
    Serial.PRINTF_LN("* %s: Stuck bits in Status Registers 2 and 1, 0x%04X", __func__, sr21);
  }

  return (0 == sr21);
}


////////////////////////////////////////////////////////////////////////////////
// Generalized Flash Status Register modify bit in function
//   bit_pos       bit position, 0 - 15
//   val           bit value 0, 1
//   _non_volatile  non_volatile_bit use non-volatile Flash Status Register
//                 volatile_bit use volatile Flash Status Register
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
int modifyBitSR(const uint32_t bit_pos, uint32_t val, const bool _non_volatile) {
  using namespace experimental;
  SpiOpResult ok0;

  // BIT1 and BIT0 are WEL and WIP which cannot be directly changed.
  if (2 > bit_pos || 16 <= bit_pos) return false;

  uint32_t _bit = 1u << bit_pos;

  uint32_t sr21 = 0u, sr2 = 0u;
  ok0 = spi0_flash_read_status_register_1(&sr21);
  CHECK_OK0(ok0);
  if (fd_state.has_8bw_sr2 || fd_state.has_16bw_sr1) {
    spi0_flash_read_status_register_2(&sr2);
    CHECK_OK0(ok0);
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
    ok0 = spi0_flash_write_status_register(/* SR1 */ 0u, sr21, _non_volatile, 16u);
    CHECK_OK0(ok0);
  } else {
    if (8u > bit_pos) {
      ok0 = spi0_flash_write_status_register(/* SR1 */ 0u, sr21 & 0xFFu, _non_volatile, 8u);
      CHECK_OK0(ok0);
    } else {
      ok0 = spi0_flash_write_status_register(/* SR2 */ 1u, sr21 >> 8u, _non_volatile, 8u);
      CHECK_OK0(ok0);
    }
  }

  uint32_t verify_sr21 = 0u, verify_sr2 = 0u;
  ok0 = spi0_flash_read_status_register_1(&verify_sr21);
  CHECK_OK0(ok0);
  if (fd_state.has_8bw_sr2 || fd_state.has_16bw_sr1) {
    ok0 = spi0_flash_read_status_register_2(&verify_sr2);
    CHECK_OK0(ok0);
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
// Here SFDP would have been useful if any of the ESP8266 modules I tested had
// support for JEDEC 1.06. Everything I saw was too old or broken (0xD8).
// DWORD 15 and 16 are the interesting ones; however, most of the flashes I see
// only have 9 DWORD. Leave the deeper dive into SFDP for a later revision.
bool analyze_write_QE(bool _non_volatile) {
  using namespace experimental;
  bool pass = false;
  uint32_t qe_pos = get_qe_pos();
  const char *str_kind = (_non_volatile) ? "non-" : "";

  if (0xFFu == qe_pos) {
    Serial.PRINTF_LN("\n%s: Test for QE/S? - No QE bit selected, S9 or S6.", __func__);
    return pass;
  }

  Serial.PRINTF_LN("\n%s: Test for QE/S%X Status Register write support", __func__, qe_pos);
  // set and clear QE
  int res = modifyBitSR(qe_pos, 1u, _non_volatile);
  if (0 <= res) {
    // It is okay if already set, we will learn if Status Register Writes are
    // allowed when we clear.
    res = modifyBitSR(qe_pos, 0u, _non_volatile);
    if (1 == res) {
      pass = true;
      Serial.PRINTF_LN("  %svolatile supported", str_kind);
    } else {
      Serial.PRINTF_LN("* QE/S%X bit stuck on cannot clear in %svolatile Status Register.", qe_pos, str_kind);
    }
  } else {
    Serial.PRINTF_LN("* Unable to modify QE/S%X bit in %svolatile Status Register.", qe_pos, str_kind);
  }
  return pass;
}


bool analyze_SR_BP0(const bool _non_volatile) {
  using namespace experimental;
  Serial.PRINTF_LN("\n%s: Test for writable %svolatile Status Register using BP0", __func__, (_non_volatile) ? "non-" : "");

  // set and clear BP0
  int res = modifyBitSR(/* BP0 */2u, 1u, _non_volatile);
  if (0 <= res) {
    // It is okay if already set, we will learn if Status Register Writes are
    // allowed when we clear.
    res = modifyBitSR(/* BP0 */2u, 0u, _non_volatile);
    if (1 == res) {
      Serial.PRINTF_LN("  %ssupported", "");
      return true;
    }

    Serial.PRINTF_LN("* PM0 bit stuck on cannot clear bit BP0 in %svolatile Status Register.", (_non_volatile) ? "non-" : "");
  } else {
    Serial.PRINTF_LN("* Unable to modify PM0 bit in %svolatile Status Register.", (_non_volatile) ? "non-" : "");
  }
  clearSR21(_non_volatile);
  return false;
}

bool test_short_circuit_9_10(const char *str) {
  // GPIO pins 9 and 10 short circuit test
  Serial.PRINTF_LN("\n"
    "%s: short circuit tests for GPIO pins 9 and 10.", str);
  bool pass = test_GPIO_pin_short(10u);
  if (pass) pass = test_GPIO_pin_short(9u);
  return pass;
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
bool analyze_SR_QE(const uint32_t hint, const bool saferMode) {
  using namespace experimental;

  // Reset Flash Discovery states
  resetFlashDiscovery();
  Serial.PRINTF_LN("\n%s: Analyze flash memoryand discover characteristics, hint: '%u', safety: %s.",
    __func__, hint, (saferMode) ? "on" : "off");

  fd_state.device = printFlashChipID("  ");
  printSR321("  ", true);

  // Validate that we have control over the output of the GPIO pins 9 and 10.
  // We cannot work with a flash chip that shorts /HOLD (or /WP) to +3.3V, etc.
  fd_state.pass_SC = test_short_circuit_9_10(__func__);
  if (! fd_state.pass_SC) {
    return false;
  }

  // Take control of GPIO10/"/WP" and force high so Status Register (SR) writes
  // are not blocked.
  digitalWrite(10u, HIGH);
  pinMode(10u, OUTPUT);

  // This should not be necessary; however, our analysis depends on WEL working
  // a certain way. This confirms that the flash memory works the way we think
  // it does.
  Serial.PRINTF_LN("\n"
    "%s: Check flash support for the Write-Enable-Latch instructions,\n"
    "  Enable 06h and Disable 04h", __func__);
  if (confirmWelBit()) {
    Serial.PRINTF_LN("  %ssupported", "");
  } else {
    Serial.PRINTF_LN("  %ssupported", "not ");
    return false;
  }

  // Discover which Status Registers and write methods are supported. For a safe
  // test, we read and write back the same value. For an indicator that the SR
  // write failed, we use finding the WEL bit in the set state after a write.
  Serial.PRINTF_LN("\n%s: Test Status Register writes:", __func__);
  bool sr1_8  = test_sr_8_bit_write(0u);
  bool sr2_8  = test_sr_8_bit_write(1u);
  bool sr3_8  = test_sr_8_bit_write(2u);
  bool sr1_16 = test_sr1_16_bit_write();
  spi0_flash_write_disable(); // WEL Cleanup


  if (sr1_8) fd_state.has_8bw_sr1 = true;
  if (sr2_8) fd_state.has_8bw_sr2 = true;
  if (sr3_8) fd_state.has_8bw_sr3 = true;
  if (sr1_16) fd_state.has_16bw_sr1 = true;
  printSR321("  ");

  // determine if the QE bit can be written.
  // determine if we have volatile bits
  // can we modify non-volatile BP0 in Status Register-1?
  // can we modify volatile BP0 in Status Register-1?

  // Clearing all bits should be safe. Setting is when we can get into trouble
  if (! clearSR21(non_volatile_bit)) {
    Serial.PRINTF_LN("* possible OTP issue - analyze aborted");
    return false;
  }
  printSR321("  ");

  fd_state.has_volatile = analyze_SR_BP0(volatile_bit);
  printSR321("  ");
  if (saferMode) {
    // With non-volatile, it is easy to brick a module vs with volatile it can be
    // restored by powering off/on.
    if (! fd_state.has_volatile) {
      Serial.PRINTF("\n"
        "* The safety is on.\n"
        "* No volatile Status Register bits were detected. Without volatile Status Register\n"
        "* bits, continuing with more tests could brick the Flash. To analyze without\n"
        "* safety, use the capitalized hotkeys 'A' or 'B'.\n");
      if (sr1_16 || sr2_8) {
        Serial.PRINTF("* For example to rerun with \"hint S%X\" enter hotkeys: %cwhp\n", 9u, 'A');
      } else {
        Serial.PRINTF("* For example to rerun with \"hint S%X\" enter hotkeys: %cwhp\n", 6u, 'B');
      }
      return false;
    }
  }
  clearSR21(non_volatile_bit);
  fd_state.write_QE = analyze_SR_BP0(non_volatile_bit);

  Serial.PRINTF_LN("\n%s: Flash Info Summary:", __func__);
  if (sr1_8 || sr2_8 || sr3_8) {
    Serial.PRINTF("  Support for 8-bit Status Registers writes:");
    if (sr1_8) Serial.PRINTF(" SR1");
    if (sr2_8) Serial.PRINTF(" SR2");
    if (sr3_8) Serial.PRINTF(" SR3");
    Serial.PRINTF_LN();
  } else {
    // This should never be
    Serial.PRINTF_LN("  No Flash Status Registers discovered!?");
    return false;
  }

  if (! fd_state.write_QE) {
    // If we cannot write BP0, then we assume we are not able to set QE.
    // Fail now and avoid later confusion.
    Serial.PRINTF_LN("* Status Register-1 does not appear to support writes.");
    Serial.PRINTF_LN("* possible OTP issue - analyze aborted");
    return false;
  }

  if (6u == hint) {
    Serial.PRINTF_LN("  Use hint, QE bit BIT6/S6 (WPdis) with 8-bit Write Status Register-1");
    fd_state.S6 = true;
  } else
  if (sr1_16) {
    Serial.PRINTF(
      "  Use the presence of '16-bit Write Status Register-1' as a hint for\n"
      "  the QE bit at BIT9/S9");
    fd_state.S9 = true; // maybe
    if (sr2_8) {
      Serial.PRINTF(" or at BIT1 with 8-bit Write Status Register-2");
    }
    Serial.PRINTF_LN(".");
  } else
  if (sr2_8) {
    Serial.PRINTF_LN("  May support QE bit at BIT9/S9 with 8-bit Write Status Register-2 BIT1");
    fd_state.S9 = true;
  } else {
    Serial.PRINTF_LN("  No Status Register-2 present");
    if (sr1_8) {
      Serial.PRINTF_LN("  Status Register-1 may support QE/S6 (WPdis) BIT6");
      fd_state.S6 = true; // assumed when SR2 is not present
    }
  }

  if (fd_state.write_QE) {
    Serial.PRINTF_LN("  %svolatile Status Register bits available", "non-");
    if (fd_state.has_volatile) {
      Serial.PRINTF_LN("  %svolatile Status Register bits available", "");
    }
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
  if (! fd_state.write_QE) {
    Serial.PRINTF(
      "* The proposed QE/S%x bit was not writable.\n"
      "* We now assume no support for the QE bit.\n", get_qe_pos());
    fd_state.S6 = false;
    fd_state.S9 = false;
  }

  if (! fd_state.write_QE) {
    Serial.PRINTF(
      "* Expectations for success are low; however if the flash memory does\n"
      "* not have the pin features /WP and /HOLD, it may work.\n");
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// check_QE_WP_SxFF
//
// Handle an odd case for no QE bit or Status Register is already in the state
// that needs to be tested.
//
// Use current context and confirm that Status Register is writable
bool check_QE_WP_SxFF() {
  using namespace experimental;
  const bool _non_volatile = (fd_state.has_volatile) ? volatile_bit : non_volatile_bit;

  // Clear Status Register-1 aka SR1
  if (! clearSR21(_non_volatile)) {
    return false;
  }

  bool pass1 = testOutputGPIO10(0xFFu, fd_state.has_16bw_sr1, _non_volatile, true /* use asis QE=0 */);

  Serial.PRINTF_LN("\nConclusion for pin function /WP");
  if (pass1) {
    /* success with QE/S6= 0 or 1 */
    fd_state.WP = false;
    Serial.PRINTF_LN("  Feature pin function /WP disabled or not present on this part.");
    if (fd_state.has_8bw_sr2) {
      Serial.PRINTF_LN("* However, check if the flash memory supports SRP1 and SRP0 this would confuse the results\n"
                       "* when using QE/S6. Assuming these combinations exist.");
    }
  } else {
    Serial.PRINTF_LN("* Feature pin function /WP detected and not disabled.");
    fd_state.WP = true;
  }

  // With success on the last test, we return with the Status Register ready for
  // the next test.
  return pass1;
}

bool check_QE_WP_S6() {
  using namespace experimental;
  const bool _non_volatile = (fd_state.has_volatile) ? volatile_bit : non_volatile_bit;

  // Clear Status Register-1 aka SR1
  if (! clearSR21(_non_volatile)) {
    return false;
  }

  bool pass1 = testOutputGPIO10(6u, fd_state.has_16bw_sr1, _non_volatile, true /* use asis QE=0 */);
  bool pass2 = testOutputGPIO10(6u, fd_state.has_16bw_sr1, _non_volatile, false /* set QE=1 */);

  Serial.PRINTF_LN("\nConclusion for pin function /WP");
  if (pass1 && pass2) {
    /* success with QE/S6= 0 or 1 */
    fd_state.WP = false;
    Serial.PRINTF_LN("  Feature pin function /WP not detected on this part.");
    if (fd_state.has_8bw_sr2) {
      Serial.PRINTF_LN("* However, check if the flash memory supports SRP1 and SRP0 this would confuse the results\n"
                       "* when using QE/S6. Assuming these combinations exist.");
    }
  } else
  if (!pass1 && pass2) {
    /* success, QE/S6=1 controls /WP feature enable/disable */
    Serial.PRINTF_LN("  Confirmed QE/S6=1 disables pin function /WP");
    fd_state.WP = true; // pin function /WP detected.
  } else {
    Serial.PRINTF_LN("* Unable to disable pin function /WP with QE/S6=1");
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
  const bool _non_volatile = (fd_state.has_volatile) ? volatile_bit : non_volatile_bit;

  Serial.PRINTF_LN("\nSet context SRP1:SRP0 0:1 and clear all others");
  uint32_t stuckMask = test_set_SRP1_SRP0_clear_QE(9u, fd_state.has_16bw_sr1, _non_volatile);
  if (stuckMask) {
      if (~0u == stuckMask) {
        Serial.PRINTF_LN("* failed, bit SRP0/BIT7 failed to set");
      } else {
        Serial.PRINTF_LN("* failed, stuck bit mask 0x%04X", stuckMask);
      }
      return false;
  } else {
      Serial.PRINTF_LN("  success");
  }

  bool pass_SRP01_QE0 = testOutputGPIO10(9u, fd_state.has_16bw_sr1, _non_volatile, true  /* use QE=0 */);
  bool pass_SRP01_QE1 = testOutputGPIO10(9u, fd_state.has_16bw_sr1, _non_volatile, false /* set QE=1 */);

  if (! clearSR21(_non_volatile)) {
    return false;
  }

  bool pass_SRP00_QE0 = testOutputGPIO10(9u, fd_state.has_16bw_sr1, _non_volatile, true  /* use QE=0 */);
  bool pass_SRP00_QE1 = testOutputGPIO10(9u, fd_state.has_16bw_sr1, _non_volatile, false /* set QE=1 */);

  Serial.PRINTF_LN("\nConclusion for pin function /WP");
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
    } else
    if (!pass_SRP01_QE0 &&  pass_SRP01_QE1 &&  pass_SRP00_QE0) {
      Serial.PRINTF_LN("  Requires either QE/S9=1 or SRP1:SRP0=0:0 to disable pin function /WP");
      fd_state.WP = true;
    } else {
      Serial.PRINTF_LN("* Unexpected pass/fail pattern: %u=pass_SRP01_QE0, %u=pass_SRP01_QE1, %u=pass_SRP00_QE0, 1=pass_SRP00_QE1",
        pass_SRP01_QE0, pass_SRP01_QE1, pass_SRP00_QE0);
    }
  } else {
    /* no resolution found */
    Serial.PRINTF_LN("* No conclusion on how to disable pin function /WP using SRP1:SRP0 and QE/S9");
    clearSR21(_non_volatile);
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
  //  1 pass for none (0xFFu)
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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool processKey(const int key) {
  using namespace experimental;
  bool pass = true;
  last_key = key;

  if ('\n' == key || '\r' == key) {
    Serial.PRINTF_LN();
    return pass;
  }
  Serial.PRINTF("\n>> '%c'", key);

  switch (key) {
    case '3':
      printSR321("\n", true);
      break;

    case 'r':
      runScript('a');
      break;

    case 'a':   // Safer
    case 'A':   // more risk uses non-volatile Status Register
      pass = analyze_SR_QE(9u, ('a' == key));  // hint QE/S9
      if (!scripting && pass) {
        Serial.PRINTF_LN("\nTo continue verification, run /WP test, menu 'w'");
      }
      fd_state.pass_analyze = pass;
      break;

    case 'b':   // Safer
    case 'B':   // more risk uses non-volatile Status Register
      pass = analyze_SR_QE(6u, ('b' == key));  // hint QE/S6
      if (!scripting && pass) {
        Serial.PRINTF_LN("\nTo continue verification, run /WP test, menu 'w'");
      }
      fd_state.pass_analyze = pass;
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
        pass = testOutputGPIO9(get_qe_pos(), fd_state.has_16bw_sr1,
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
      Serial.PRINTF_LN();
      if (! (fd_state.pass_HOLD && fd_state.pass_WP && fd_state.pass_SC)) {
        Serial.PRINTF_LN("\nThis may not work. Not all test passed.");
      }
      printReclaimFn();
      break;
    //
    case '8':
      // write_status_register_16_bit = false;
      fd_state.has_8bw_sr2 = true;
      fd_state.has_16bw_sr1 = false;
      fd_state.pass_WP = false;       // Reset test results
      fd_state.pass_HOLD = false;
      Serial.PRINTF_LN("\n%c 8 - Set discovery:  8-bits Write Status Register and clear 16-bit SR1", (fd_state.has_16bw_sr1) ? ' ' : '>');
      break;
    case '7':
      // write_status_register_16_bit = true;
      fd_state.has_16bw_sr1 = true;
      fd_state.has_8bw_sr2 = false;  // We really don't know
      fd_state.pass_WP = false;
      fd_state.pass_HOLD = false;
      Serial.PRINTF_LN("\n%c 7 - Set discovery: 16-bits Write Status Register and clear 8-bit SR2", (fd_state.has_16bw_sr1) ? '>' : ' ');
      break;
    case '6':
      // QE/S6;
      fd_state.S9 = false;
      fd_state.S6 = true;
      fd_state.has_8bw_sr2 = false;
      fd_state.has_16bw_sr1 = false;
      fd_state.pass_WP = false;
      fd_state.pass_HOLD = false;
      Serial.PRINTF_LN("\n%c 6 - Set discovery: QE/S6 Status Register and clear 16-bit SR1 & 8-bit SR2", (fd_state.S6) ? '>' : ' ');
      break;
    case '9':
      // QE/S9
      fd_state.S9 = true;
      fd_state.S6 = false;
      fd_state.pass_WP = false;
      fd_state.pass_HOLD = false;
      Serial.PRINTF_LN("\n%c 9 - Set discovery: QE/S9 Status Register", (fd_state.S9) ? '>' : ' ');
      break;
    //
    case '0':
      // QE - none
      fd_state.S9 = false;
      fd_state.S6 = false;
      fd_state.pass_WP = false;
      fd_state.pass_HOLD = false;
      Serial.PRINTF_LN("\n%c 0 - Set discovery: QE not available", (fd_state.S9 || fd_state.S6) ? ' ' : '>');
      break;

    case 'f':
      Serial.PRINTF_LN();
      printSfdpReport();
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
        Serial.PRINTF_LN();
        if (!fd_state.S9 && !fd_state.S6) {
          Serial.PRINTF_LN(
            "%s\n* to enable hotkeys 'Q', 'q', 'E', and 'e', first select a QE"
            "  Status Register bit", "Update QE");
          break;
        }
        bool _non_volatile = ('Q' == key) || ('q' == key);
        if (fd_state.has_volatile && !_non_volatile) {
          Serial.PRINTF_LN("%s\n* the volatile Status Register option is not available.", "Update QE");
          break;
        }
        uint32_t status = 0u;
        SpiOpResult ok0 = spi0_flash_read_status_registers_3B(&status);
        if (SPI_RESULT_OK != ok0) {
          Serial.PRINTF_LN("%s\n* SR321 read operation failed!", "Update QE");
          break;
        }
        bool set_QE = ('Q' == key) || ('E' == key);
        Serial.PRINTF_LN(
          "%s=%u bit in %svolatile Flash Status Register, SR321 0x%06X",
          "Update QE", (set_QE) ? 1u : 0u, (_non_volatile) ? "non-" : "", status);
        pass = false;
        if (set_QE) {
          if (fd_state.S9) {
            if (fd_state.has_16bw_sr1) {
              pass = set_S9_QE_bit__16_bit_sr1_write(_non_volatile);
            } else {
              pass = set_S9_QE_bit__8_bit_sr2_write(_non_volatile);
            }
          } else
          if (fd_state.S6) {
            pass = set_S6_QE_bit__8_bit_sr1_write(_non_volatile);
          }
        } else {
          if (fd_state.S9) {
            if (fd_state.has_16bw_sr1) {
              pass = clear_S9_QE_bit__16_bit_sr1_write(_non_volatile);
            } else {
              pass = clear_S9_QE_bit__8_bit_sr2_write(_non_volatile);
            }
          } else
          if (fd_state.S6) {
            pass = clear_S6_QE_bit__8_bit_sr1_write(_non_volatile);
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

    case 'R':
      Serial.PRINTF_LN("\nRestart ...");
      clearSR21(non_volatile_bit);
      Serial.flush();
      ESP.restart();
      break;

    case 'd':
      {
        Serial.PRINTF_LN("\nFlash write disable:");
        SpiOpResult ok0 = spi0_flash_write_disable();
        Serial.PRINTF_LN("%c %s\n", (SPI_RESULT_OK == ok0) ? ' ' : '*', (SPI_RESULT_OK == ok0) ? "success" : "failed!");
        uint32_t status = 0u;
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
      pinMode(10u, OUTPUT);
      digitalWrite(10u, HIGH);
      Serial.PRINTF_LN("\nStart SPI Flash Software Reset (66h, 99h):");
      spi0_flash_software_reset(25000u);
      printSR321("  ", true);
      break;

    case 's':
      // GPIO pins 9 and 10 short circuit test
      pass = test_short_circuit_9_10("menu 's'");
      fd_state.pass_SC = pass;
      break;

    case 't':
      {
        bool _non_volatile = (fd_state.has_volatile) ? volatile_bit : non_volatile_bit;
        Serial.PRINTF_LN("\n"
          "Checking 'spi_flash_vendor_cases()' for builtin QE bit handler\n");
        Serial.PRINTF_LN("Unroll Status Register changes");
        printSR321("  ", true);
        pinMode(9u, SPECIAL);
        pinMode(10u, SPECIAL);
        clearSR21(non_volatile_bit);
        printSR321("  ");

        Serial.PRINTF_LN("\nRun buildin/custom 'spi_flash_vendor_cases()' function");
        uint32_t device = fd_state.device & 0xFFFFFFu;
        pass = spi_flash_vendor_cases(device);
        Serial.PRINTF_LN("%c Device/Vendor: 0x%06X %ssupported", (pass) ? ' ' : '*', device, (pass) ? "" : "not ");
        printSR321("  ", true);

        if (pass) {
          // /WP
          pass = testOutputGPIO10(0xFFu, fd_state.has_16bw_sr1, _non_volatile, true /* use SR asis */);
          Serial.PRINTF_LN("%c Builtin/custom handler for pin function /WP %s.", (pass) ? ' ' : '*', (pass) ? "worked" : "failed");
          if (pass) {
            // /Hold
            pass = testOutputGPIO9(0xFFu, fd_state.has_16bw_sr1, _non_volatile, true);
          }
        }
      }
      break;

    case '?':
      Serial.PRINTF_LN("\nHot key help:");
      Serial.PRINTF_LN("\nAnalyze:");
      Serial.PRINTF_LN("  r - Run analyze script includes tests");
      Serial.PRINTF_LN("  a - Analyze Status Register Writes remembers discovered results. leans toward QE/S9");
      Serial.PRINTF_LN("  b - Analyze Status Register Writes remembers discovered results, uses hint QE/S6");
      Serial.PRINTF_LN("  A - Same as 'a' except less safe uses non-volatile Status Registers");
      Serial.PRINTF_LN("  B - Same as 'b' except less safe uses non-volatile Status Registers");
      Serial.PRINTF_LN("  f - Print SFDP Data");
      Serial.PRINTF_LN();
      Serial.PRINTF_LN("Isolated test sets:");
      Serial.PRINTF_LN("  s - GPIO pins 9 and 10 short circuit test, included in Analyze");
      Serial.PRINTF_LN("  w - Test /WP   digitalWrite(10, LOW) and write to Flash");
      Serial.PRINTF_LN("  h - Test /HOLD digitalWrite( 9, LOW)");
      Serial.PRINTF_LN("  p - Print 'spi_flash_vendor_cases()' example function");
      Serial.PRINTF_LN("  t - Test builtin/custom 'spi_flash_vendor_cases()'");
      Serial.PRINTF_LN();
      Serial.PRINTF_LN("Manual set/clear discovery flags:");
      Serial.PRINTF_LN("%c 8 - Set flags:  8-bits Write Status Register and clear 16-bit SR1", (fd_state.has_16bw_sr1) ? ' ' : '>');
      Serial.PRINTF_LN("%c 7 - Set flags: 16-bits Write Status Register and clear 8-bit SR2", (fd_state.has_16bw_sr1) ? '>' : ' ');
      Serial.PRINTF_LN("%c 6 - Set flags: QE/S6 Status Register and clear 16-bit SR1 & 8-bit SR2", (fd_state.S6) ? '>' : ' ');
      Serial.PRINTF_LN("%c 9 - Set flags: QE/S9 Status Register", (fd_state.S9) ? '>' : ' ');
      Serial.PRINTF_LN("%c 0 - Set flags: QE not available", (fd_state.S9 || fd_state.S6) ? ' ' : '>');
      Serial.PRINTF_LN();
      Serial.PRINTF_LN("Manual set/clear QE using discovery flags:");
      if (fd_state.S9 || fd_state.S6) {
        Serial.PRINTF_LN("  Q - Set SR, non-volatile WEL 06h, then set QE/S%X=1", get_qe_pos());
        Serial.PRINTF_LN("  q - Set SR, non-volatile WEL 06h, then set QE/S%X=0", get_qe_pos());
        if (fd_state.has_volatile) {
          Serial.PRINTF_LN("  E - Set SR, volatile 50h, then set QE/S%X=1", get_qe_pos());
          Serial.PRINTF_LN("  e - Set SR, volatile 50h, then set QE/S%X=0", get_qe_pos());
        }
      } else {
        Serial.PRINTF_LN("  - - Select a QE Status Register bit to enable hotkeys 'Q', 'q', 'E', 'e'");
      }

      Serial.PRINTF_LN();
      Serial.PRINTF_LN("Misc.");
      Serial.PRINTF_LN("  3 - SPI Flash Read Status Registers, 3 Bytes");
      Serial.PRINTF_LN("  d - Clear WEL bit, write enable");
      Serial.PRINTF_LN("  S - SPI Flash - Software Reset 66h, 99h");
      Serial.PRINTF_LN("  R - Restart");
      Serial.PRINTF_LN("  ? - This help message");
      break;

    default:
      Serial.PRINTF_LN(" ?");
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

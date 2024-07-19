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
  QE bit - SPI0 Flash Utilities
*/
#ifndef EXPERIMENTAL_SPIFLASHUTILSQE_H
#define EXPERIMENTAL_SPIFLASHUTILSQE_H
#ifdef __cplusplus
extern "C" {
#endif

// Defines DBG_SFU_PRINTF and DEBUG_FLASH_QE provided by
#include "SpiFlashUtils.h"  // handles DBG_SFU_PRINTF macro
#include "BootROM_NONOS.h"

namespace experimental {

////////////////////////////////////////////////////////////////////////////////
// Less general and more QE bit specific Flash operations
//


#ifndef PRESERVE_EXISTING_STATUS_BITS
// It may be best to not preserve existing Flash Status Register Bits.
// I am concerned about setting OTP on the part and being locked out.
// The BIT meanings vary too much between vendors and it is extra code.
//
// Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
// to zero without much care we assume they can all be zero at the startup.
//
// TODO factor out as always 0
#define PRESERVE_EXISTING_STATUS_BITS 0
#endif

constexpr uint32_t kWIPBit    = BIT0;  // Write In Progress
constexpr uint32_t kWELBit    = BIT1;  // Write Enable Latch
constexpr uint32_t kQES6Bit   = BIT6;  // QE/S6 Disable /WP pin
constexpr uint32_t kQES9Bit1B = BIT1;  // Enable QE=1, QE/S9
constexpr uint32_t kQES9Bit2B = BIT9;  // Enable QE=1, QE/S9

inline
bool verify_status_register_1(const uint32_t a_bit_mask) {
  uint32_t status = 0u;
  spi0_flash_read_status_register_1(&status);
  return ((a_bit_mask & status) == a_bit_mask);
}

inline
bool verify_status_register_2(const uint32_t a_bit_mask) {
  uint32_t status = 0u;
  spi0_flash_read_status_register_2(&status);
  return ((a_bit_mask & status) == a_bit_mask);
}

// WEL => Write Enable Latch
inline
bool is_WEL(void) {
  bool success = verify_status_register_1(kWELBit);
  // DBG_SFU_PRINTF("  %s bit %s set.\n", "WEL", (success) ? "confirmed" : "NOT");
  return success;
}

inline
bool is_WEL_dbg(void) {
  bool success = verify_status_register_1(kWELBit);
  ets_uart_printf("  %s bit %s set.\n", "WEL", (success) ? "confirmed" : "NOT");
  return success;
}

// WIP => Write In Progress aka Busy
inline
bool is_WIP(void) {
  return verify_status_register_1(kWIPBit);
}

// S6, WPDis => pin Write Protect Disable, maybe only on the EN25Q32C
// S6, on some devices (ISSI, ) is similar to QE at S9 on others.
inline
bool is_S6_QE_WPDis(void) {
  bool success = verify_status_register_1(kQES6Bit);
  DBG_SFU_PRINTF("  %s bit %s set.\n", "S6/QE/WPDis", (success) ? "confirmed" : "NOT");
  return success;
}

// QE => Quad Enable, S9
inline
bool is_QE(void) {
  bool success = verify_status_register_2(kQES9Bit1B);
  DBG_SFU_PRINTF("  %s bit %s set.\n", "QE", (success) ? "confirmed" : "NOT");
  return success;
}

// Note, there are other SPI registers with bits for other aspects of QUAD
// support. I chose these.
// Returns true when SPI0 controler is in either QOUT or QIO mode.
inline
bool is_spi0_quad(void) {
  return (0u != ((SPICQIO | SPICQOUT) & SPI0C));
}

// Only for EN25Q32C, earlier version may not work.
// Maybe add SFDP call to validate EN25Q32C part.
// see RTOS_SDK/components/spi_flash/src/spi_flash.c
// Don't rely on that Espressif sample too closely. The data shown for EN25Q16A
// is not correct. The EN25Q16A and EN25Q16B do no support SFDP.
[[maybe_unused]]
static bool set_S6_QE_bit_WPDis(const bool non_volatile) {
  uint32_t status = 0u;
  spi0_flash_read_status_register_1(&status);
  bool is_set = (0u != (status & kQES6Bit));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "S6/QE/WPDis", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status |= kQES6Bit;
#else
  // Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
  // to zero without much care we assume they can all be zero at the startup.
  status = kQES6Bit;
#endif
  // All changes made to the volatile copies of the Status Register-1.
  DBG_SFU_PRINTF("  Setting %svolatile %s bit.\n", (non_volatile) ? "non-" : "", "S6/QE/WPDis");
  spi0_flash_write_status_register_1(status, non_volatile);
  return is_S6_QE_WPDis();
}

[[maybe_unused]]
static bool clear_S6_QE_bit_WPDis(bool non_volatile = false) {
  uint32_t status = 0u;
  spi0_flash_read_status_register_1(&status);
  bool not_set = (0u == (status & kQES6Bit));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "S6/QE/WPDis", (not_set) ? "NOT" : "confirmed");
  if (not_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status &= ~kQES6Bit;
#else
  // Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
  // to zero without much care we assume they can all be zero at the startup.
  status = 0u;
#endif
  // All changes made to the volatile copies of the Status Register-1.
  DBG_SFU_PRINTF("  Clearing %svolatile S6/QE/WPDis bit - 8-bit write.\n", non_volatile ? "non-" : "");
  spi0_flash_write_status_register_1(status, non_volatile);
  return (false == is_S6_QE_WPDis());
}

[[maybe_unused]]
static bool set_S9_QE_bit__8_bit_sr2_write(const bool non_volatile) {
  uint32_t status2 = 0u;
  spi0_flash_read_status_register_2(&status2);
  bool is_set = (0u != (status2 & kQES9Bit1B));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "QE", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status2 |= kQES9Bit1B;
#else
  // Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
  // to zero without much care we assume they can all be zero at the startup.
  status2 = kQES9Bit1B;
#endif
  DBG_SFU_PRINTF("  Setting %svolatile %s bit - %u-bit write.\n", (non_volatile) ? "non-" : "", "QE", 8u);
  spi0_flash_write_status_register_2(status2, non_volatile);
  return is_QE();
}

[[maybe_unused]]
static bool set_S9_QE_bit__16_bit_sr1_write(const bool non_volatile) {
  uint32_t status = 0u;
  spi0_flash_read_status_registers_2B(&status);
  bool is_set = (0u != (status & kQES9Bit2B));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "QE", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status |= kQES9Bit2B;
#else
  // Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
  // to zero without much care we assume they can all be zero at the startup.
  status = kQES9Bit2B;
#endif
  DBG_SFU_PRINTF("  Setting %svolatile %s bit - %u-bit write.\n", (non_volatile) ? "non-" : "", "QE", 16u);
  spi0_flash_write_status_registers_2B(status, non_volatile);
  return is_QE();
}

#if 0
// I don't think these are needed anymore
[[maybe_unused]]
static void clear_sr_mask(uint32_t reg_0idx, const uint32_t pattern) {
  #if 0
  // BUGBUG sets WEL
  uint32_t status = flash_gd25q32c_read_status(reg_0idx);
  #else
  uint32_t status = 0u;
  spi0_flash_read_status_register(reg_0idx, &status);
  #endif
  if (pattern & status) {
    status &= ~pattern;
    flash_gd25q32c_write_status(reg_0idx, status); // 8-bit status register write
    // This message should not repeat at next boot
    DBG_SFU_PRINTF("** One time clear of Status Register-%u bits 0x%02X.\n", reg_0idx + 1, pattern);
  }
}
inline
void clear_sr1_mask(const uint32_t pattern) {
  clear_sr_mask(0u, pattern);
}
inline
void clear_sr2_mask(const uint32_t pattern) {
  clear_sr_mask(1u, pattern);
}
#endif

#if 1
[[maybe_unused]]
static bool clear_S9_QE_bit__8_bit_sr2_write(const bool non_volatile) {
  uint32_t status2 = 0u;
  spi0_flash_read_status_register_2(&status2);
  bool is_set = (0u != (status2 & kQES9Bit1B));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "QE", (is_set) ? "confirmed" : "NOT");
  if (! is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status2 &= kQES9Bit1B;
#else
  // Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
  // to zero without much care we assume they can all be zero at the startup.
  status2 = 0u;
#endif
  DBG_SFU_PRINTF("  Clear %svolatile %s bit - %u-bit write.\n", (non_volatile) ? "non-" : "", "QE", 8u);
  spi0_flash_write_status_register_2(status2, non_volatile);
  return (false == is_QE());
}

[[maybe_unused]]
static bool clear_S9_QE_bit__16_bit_sr1_write(const bool non_volatile) {
  uint32_t status = 0u;
  spi0_flash_read_status_registers_2B(&status);
  bool is_set = (0u != (status & kQES9Bit2B));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "QE", (is_set) ? "confirmed" : "NOT");
  if (! is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status &= kQES9Bit2B;
#else
  // Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
  // to zero without much care we assume they can all be zero at the startup.
  status = 0u;
#endif
  DBG_SFU_PRINTF("  Setting %svolatile %s bit - %u-bit write.\n", (non_volatile) ? "non-" : "", "QE", 16u);
  spi0_flash_write_status_registers_2B(status, non_volatile);
  return (false == is_QE());
}
#endif


};  // namespace experimental {

#ifdef __cplusplus
}
#endif
#endif // EXPERIMENTAL_SPIFLASHUTILSQE_H

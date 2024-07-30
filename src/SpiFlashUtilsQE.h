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
// Maybe need more thought - review spi_flash_check_wr_protect() in
// https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/components/spi_flash/src/spi_flash.c
// specificly SPI_*_WRITE_PROTECT_STATUS mask usage checking for write protect bits.
//
// Take away:
//    1. spi_flash_check_wr_protect() clears any set Block Protect bits
//       found for a specific flash vendor.
//    2. I don't see anyone setting Block Protect bits (??)
//    3. Block Protection and /WP are considered a protection against an
//       accidental write that might occur during a SPI bus data error.
//    4. However, if the bus is that unreliable, we would be freqently crashing
//       as the iCache hardware has no error detection/recovery process that I
//       am aware of unless you count the HWDT. Perhapse higher quality flash
//       memory should be used in conjunction with a quit board design.
//    5. I think we loose little by eliminating the use of /WP signal.
//       When the QIO option is selected, pin /WP is swapped out for a data pin.
//
// QE/S6
//   Write, block protect related bits at S5 - S2  AKA  BP3 - BP0
//
// QE/S9
//   Write, block protect related bits at S14, S6 - S2  AKA  CMP, BP4 - BP0
//   Status Register write protection S8:S7, SRP1:SRP0
//     OTP {1:1}
//     Vendor dependent, Power-Supply Lock-Down or hardware protected {1:0}
//       for hardware protected via pin /WP
//     Software protected {0:0}
//
// Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
// to zero without much care I assume they can all be zero at the startup.
//
// So far it looks like our optimum solution calls for SR1 and SR2 to be all
// zeros with the exception of the QE bit possition.
//
// I have not seen a case for not keeping zero.
// Keep macro for now, maybe later factor out the non-zero path.
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
bool is_S6_QE(void) {
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

////////////////////////////////////////////////////////////////////////////////
//
bool set_S6_QE_bit__8_bit_sr1_write(const bool non_volatile);
bool set_S9_QE_bit__8_bit_sr2_write(const bool non_volatile);
bool set_S9_QE_bit__16_bit_sr1_write(const bool non_volatile);
bool clear_S6_QE_bit__8_bit_sr1_write(const bool non_volatile);
bool clear_S9_QE_bit__8_bit_sr2_write(const bool non_volatile);
bool clear_S9_QE_bit__16_bit_sr1_write(const bool non_volatile);


#if 0
// I don't think these are needed anymore
void clear_sr_mask(uint32_t reg_0idx, const uint32_t pattern);
inline
void clear_sr1_mask(const uint32_t pattern) {
  clear_sr_mask(0u, pattern);
}
inline
void clear_sr2_mask(const uint32_t pattern) {
  clear_sr_mask(1u, pattern);
}
#endif

};  // namespace experimental {

#ifdef __cplusplus
}
#endif
#endif // EXPERIMENTAL_SPIFLASHUTILSQE_H

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
#include <Arduino.h>
#include <SpiFlashUtilsQE.h>

#ifdef __cplusplus
extern "C" {
#endif
namespace experimental {

////////////////////////////////////////////////////////////////////////////////
// Less general and more QE bit specific Flash operations
//
#if 0
// For the EON EN25Q32B flash only, the S6 bit is refered to as Write Protect Disable
// (WPDis); however, it gets complicated when setting volatile copy of the bit.
// All changes made to the volatile copies of the Status Register-1.
// Based on en25q16x_write_volatile_status() from
// https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/components/spi_flash/src/spi_flash.c
// This logic is expected to handle EN25QxxB part, untested - no hardware
//
// I don't trust the sample code. The datasheets I have for the EN25Q32B say,
// "In the OTP mode, WRSR command will ignore input data and program OTP_LOCK
// bit to 1." As I read it, this code will not set QE bit and will set OTP, and
// lock us out of any future changes? The datasheet also does not indicate
// support for instructions 50h or SFDP. Also volatile instructio 50h is not
// defined in the datasheet.
//
// Of concern in more recent code samples, the EON flash parts are not shown
// https://github.com/espressif/esp-idf/blob/bdb9f972c6a1f1c5ca50b1be2e7211ec7c24e881/components/bootloader_support/bootloader_flash/src/flash_qio_mode.c#L37-L54
//
// For the EON flash memory, I think it best to let the programmer verify what
// works for their flash memory and add a handler.
//
//C I am leaning toward removing this function.
//
bool set_S6_QE_bit__8_bit_sr1_write_EN25Q32B_volatile();
bool set_S6_QE_bit__8_bit_sr1_write_EN25Q32B_volatile(void) {
  const bool non_volatile = volatile_bit;
  uint32_t status = 0u;
  spi0_flash_read_status_register_1(&status);
  bool is_set = (0u != (status & kQES6Bit));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "S6/QE/WPDis", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

  DBG_SFU_PRINTF("  Setting %svolatile %s bit.\n", (non_volatile) ? "non-" : "", "S6/QE/WPDis");

  SPI0Command(kEonOtpCmd, NULL, 0u, 0); // Enter EON's OTP mode
  status = kQES6Bit;
  SPI0Command(kWriteStatusRegister1Cmd, &status, 8u, 0u, kVolatileWriteEnableCmd);
  bool pass = is_S6_QE();     // Do I really need to read before exiting OTP
  spi0_flash_write_disable(); // For the EN25Q32B, this exits OTP mode.
  return pass;
}
#endif

// For the EON EN25Q32C flash, the S6 bit is refered to as Write Protect Disable
// (WPDis)
//C renamed set_S6_QE_bit_WPDis to set_S6_QE_bit__8_bit_sr1_write
bool set_S6_QE_bit__8_bit_sr1_write(const bool non_volatile) {
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
  return is_S6_QE();
}

//C renamed clear_S6_QE_bit_WPDis to clear_S6_QE_bit__8_bit_sr1_write
bool clear_S6_QE_bit__8_bit_sr1_write(const bool non_volatile) {
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
  return (false == is_S6_QE());
}

bool set_S9_QE_bit__8_bit_sr2_write(const bool non_volatile) {
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

bool set_S9_QE_bit__16_bit_sr1_write(const bool non_volatile) {
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


bool clear_S9_QE_bit__8_bit_sr2_write(const bool non_volatile) {
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

bool clear_S9_QE_bit__16_bit_sr1_write(const bool non_volatile) {
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



#if 0
// I don't think these are needed anymore
void clear_sr_mask(uint32_t reg_0idx, const uint32_t pattern) {
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
#endif

};  // namespace experimental {
#ifdef __cplusplus
};
#endif

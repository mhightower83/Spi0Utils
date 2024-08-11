////////////////////////////////////////////////////////////////////////////////
/*
  Expand Flash Vendor support for GPIO9 and GPIO10 reclaim

  Modified example code generate by Analyze.ino uses SFDP to better detect the
  EN25Q32C.

*/

// #include <Arduino.h>
#include <ModeDIO_ReclaimGPIOs.h>
#include <SfdpRevInfo.h>

extern "C"
bool spi_flash_vendor_cases(uint32_t device) {
  using namespace experimental;
  bool success = false;

  SfdpRevInfo sfdpInfo;
  uint32_t* sfdp_dw = get_sfdp_basic(&sfdpInfo);  // uses malloc, must free

  if (0x301C == (device & 0xFFFFu) && sfdp_dw) {
    // Status: tested, sealed in ESP-12F module
    // EON EN25Q32C, identification based on datasheet and data matchup
    // SFDP Revision: 1.00, 1ST Parameter Table Revision: 1.00
    // SFDP Table Ptr: 0x30, Size: 9 DW
    if (1u == sfdpInfo.parm_major &&
      0u == sfdpInfo.parm_minor &&
      0xE5u == (sfdp_dw[0] & 0xFFu)) {
      // These will match EN25Q32C pin 4 NC (DQ3) no /HOLD function and no
      // volatile bits. EON SPI Flash parts have a WPDis S6 bit in status
      // register-1 for disabling /WP (and /HOLD). This is similar to QE/S9 on
      // other vendor parts. No support for volatile SR1.
      success = set_S6_QE_bit__8_bit_sr1_write(non_volatile_bit);
    }
    // 0x331Cu - EN25Q32, Not supported - no S6 bit
    // 0x701Cu - EN25QH128A might work
    // EN25Q32A, EN25Q32B - Not supported - no hardware to confirm operation
    // datasheets do not show SFDP support; however, RTOS_SDK sample code
    // implies they do.
  }
  if (sfdp_dw) free(sfdp_dw);

  if (! success) {
    // then try built-in support
    success = __spi_flash_vendor_cases(device);
  }
  return success;
}

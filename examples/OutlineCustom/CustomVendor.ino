////////////////////////////////////////////////////////////////////////////////
// Expand Flash Vendor support for GPIO9 and GPIO10 reclaim
// Adds support for EN25QH128A
#if RECLAIM_GPIO_EARLY || DEBUG_FLASH_QE
// Use lower level print functions when printing before "C++" runtime has initialized.
#define DBG_SFU_PRINTF(a, ...) ets_uart_printf(a, ##__VA_ARGS__)
#elif DEBUG_FLASH_QE
#define DBG_SFU_PRINTF(a, ...) Serial.PRINTF(a, ##__VA_ARGS__)
#else
#define DBG_SFU_PRINTF(...) do {} while (false)
#endif

#include <SpiFlashUtilsQE.h>

extern "C"
bool spi_flash_vendor_cases(uint32_t device) {
  using namespace experimental;
  bool success = false;

  if (0x6085 == (device & 0xFFFFu)) {
    // Puya logo marked as P25Q80H
    // SFDP Revision: 1.00, 1ST Parameter Table Revision: 1.00
    // SFDP Table Ptr: 0x30, Size: 36 Bytes
    success = set_S9_QE_bit__16_bit_sr1_write(volatile_bit);
  } else
  if (0x605E == (device & 0xFFFFu)) {
    // Zbit logo marked as 25VQ80AT
    // SFDP Revision: 1.06, 1ST Parameter Table Revision: 1.06
    // SFDP Table Ptr: 0x30, Size: 64 Bytes
    success = set_S9_QE_bit__16_bit_sr1_write(volatile_bit);
  } else
  if (0x325E == (device & 0xFFFFu)) {
    // No logo marked as T25S80 - no matching datasheet
    // Junk Flash works only with SPI Flash Mode: "DOUT"
    // No pin functions /WP or /HOLD
    // SFDP none
    success = true;
  }

#if 1
  // If you have no need for the additional boards supported in
  // __spi_flash_vendor_cases(device); this block can be omitted.
  // Then __spi_flash_vendor_cases will be omitted a link time.
  if (! success) {
    // Check builtin support
    success = __spi_flash_vendor_cases(device);
  }
#endif
  return success;
}

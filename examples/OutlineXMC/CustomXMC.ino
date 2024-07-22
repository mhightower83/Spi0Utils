////////////////////////////////////////////////////////////////////////////////
/*
  Expand Flash Vendor support for GPIO9 and GPIO10 reclaim

  Shows using the example code generate by Analyze.ino and handle flash devices
  with individual part revision differences.

  The datasheets for XMC XM25QH32B and XM25QH32C reveal a change in the Status
  Register-3 values for handling Drive Strength. This example shows how to
  match up 100% Drive Strength bit settings to the right part.

  Why. Someone found that changing the driver strength to 100% for some troubled
  boards running with flash at speeds above 26MHz improved reliability. So this
  example builds on logic from `core_esp8266_flash_quirks.cpp`

  The example does not address if the driver strength needs to be changed only
  how you could. I have never seen an issue with my boards.

  We use SFDP to aid in differentiating XM25QH32B, XM25QH32C, and XM25QH64C.

  We also take into account, some XMC parts clear SR3 on the first write to a
  volatile register.

*/
#include <ModeDIO_ReclaimGPIOs.h>
#include <SfdpRevInfo.h>
#include <spi_flash_defs.h>
#define SPI_FLASH_SR3_XM25QH32C_DRV_100 0

static uint32_t get_flash_mhz();
uint32_t get_flash_mhz() {
  // Load the first 4 byte from flash (magic byte + flash config)
  uint32_t data = *(uint32_t *)0x42000000u;
  // 32-bit reference to trick compiler out of optimizing 32-bit access to 8-bit
  asm volatile ( "" : "+ar"(data) ::);
  uint8_t * bytes = (uint8_t *) &data;

  switch (bytes[3] & 0x0Fu) {
    case 0x00:  // 40 MHz
      return 40u;
    case 0x01u: // 26 MHz
      return 26u;
    case 0x02u: // 20 MHz
      return 20u;
    case 0x0Fu: // 80 MHz
      return 80u;
    default:    // fail?
      return 0;
  }
  return 0;
}

extern "C"
bool spi_flash_vendor_cases(uint32_t device) {
  using namespace experimental;
  bool success = false;
  SfdpRevInfo sfdpInfo = get_sfdp_revision();

  if (0x4020 == (device & 0xFFFFu)) {  // XMC
    // Datasheet: Rev M issue date 2018/07/17
    // XMC XM25QH32B
    // SFDP Revision: 1.00, 1ST Parameter Table Revision: 1.00
    // SFDP Table Ptr: 0x30, Size: 9 DW
    //
    // Datasheet: Rev 2.1 issue date 2023/12/15
    // XMC XM25QH32C
    // SFDP Revision: 1.06, 1ST Parameter Table Revision: 1.06
    // SFDP Table Ptr: 0x30, Size: 9 DW
    //
    // Special handling for XMC XM25QH32B anomaly where driver strength value is
    // lost when switching from non-volatile to volatile requires backup/restore
    // of Status Register-3.
    uint32_t status3 = 0;
    SpiOpResult ok0 = spi0_flash_read_status_register_3(&status3);
    success = set_S9_QE_bit__16_bit_sr1_write(volatile_bit);
    /*
      Consider this example a hypothetical. While the data came from the
      vendor's datasheets, without real hardware to inspect, we cannot be sure,
      the data I used was not a typo.
    */
    if (SPI_RESULT_OK == ok0) {
      // Copy Driver Strength value from non-volatile to volatile
      uint32_t newSR3 = status3;;
      if (get_flash_mhz() > 26) { // >26Mhz?
        // Set the output drive to 100%
        if (1u == sfdpInfo.parm_major && 0 == sfdpInfo.parm_minor && 9u == sfdpInfo.sz_dw && 1u == sfdpInfo.num_parm_hdrs) {
          // Datasheet: Rev M issue date 2018/07/17
          // XMC XM25QH32B
          newSR3 &= ~(SPI_FLASH_SR3_XMC_DRV_MASK << SPI_FLASH_SR3_XMC_DRV_S);
          newSR3 |= (SPI_FLASH_SR3_XMC_DRV_100 << SPI_FLASH_SR3_XMC_DRV_S);
        } else
        if (1u == sfdpInfo.parm_major && 6u == sfdpInfo.parm_minor && 9u == sfdpInfo.sz_dw && 1u == sfdpInfo.num_parm_hdrs) {
          // Datasheet: Rev 2.1 issue date 2023/12/15
          // XMC XM25QH32C
          newSR3 &= ~(SPI_FLASH_SR3_XMC_DRV_MASK << SPI_FLASH_SR3_XMC_DRV_S);
          newSR3 |= (SPI_FLASH_SR3_XM25QH32C_DRV_100 << SPI_FLASH_SR3_XMC_DRV_S);
        } else
        if (1u == sfdpInfo.parm_major && 0 == sfdpInfo.parm_minor && 10u == sfdpInfo.sz_dw && 2u == sfdpInfo.num_parm_hdrs) {
          // Datasheet: Rev.R Issue Date: 2019/08/23
          // XMC XM25QH64C
          newSR3 &= ~(SPI_FLASH_SR3_XMC_DRV_MASK << SPI_FLASH_SR3_XMC_DRV_S);
          newSR3 |= (SPI_FLASH_SR3_XM25QH32C_DRV_100 << SPI_FLASH_SR3_XMC_DRV_S);
        }
        // For all others, no change
      }
      ok0 = spi0_flash_write_status_register_3(newSR3, volatile_bit);
      DBG_SFU_PRINTF("  XMC Anomaly: Copy Driver Strength values to volatile status register.\n");
      if (SPI_RESULT_OK != ok0) {
        DBG_SFU_PRINTF("* anomaly handling failed.\n");
      }
    }
  }

#if 1
  // If you do not need the additional boards supported in
  // __spi_flash_vendor_cases(device); this block can be omitted. When not
  // referenced, the function __spi_flash_vendor_cases is omitted by the linker
  // from the build.

  if (! success) {
    // Check builtin support
    success = __spi_flash_vendor_cases(device);
  }
#endif
  return success;
}

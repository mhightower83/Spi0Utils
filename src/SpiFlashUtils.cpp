/*
  SPI0 Flash Utilities
*/
#include <Arduino.h>
#include <user_interface.h> // system_soft_wdt_feed()
#include "BootROM_NONOS.h"

#if ((1 - DEBUG_FLASH_QE - 1) == 2)
#undef DEBUG_FLASH_QE
#define DEBUG_FLASH_QE 0
#endif

extern "C" {

#if DEBUG_FLASH_QE
#define DBG_SFU_PRINTF ets_uart_printf
#else
#define DBG_SFU_PRINTF(...) do {} while (false)
#endif
#include "SpiFlashUtils.h"
using experimental::SPI0Command;

namespace experimental {

////////////////////////////////////////////////////////////////////////////////
// base function see .h
// common logic, 24 bit address reads with one dummpy byte.
SpiOpResult _spi0_flash_read_common(const uint32_t offset, uint32_t *p, const size_t sz, const uint8_t cmd) {
  if (sz > 64 || (sz % sizeof(uint32_t))) return SPI_RESULT_ERR;
  FlashAddr24 addr24bit;

  // MSB goes on wire first. It comes out of the LSB of a 32-bit word.
  spi_set_addr(addr24bit.u8, offset);
  addr24bit.u8[3] = 0u; // Read has dummy byte after address.
  p[0] = addr24bit.u32;

#if DEBUG_FLASH_QE
  // Is there any reason to do this other than it looks pretty when debugging?
  // SPI0Command will clear to the bit length, sz * 8.
  size_t sz_dw = sz / sizeof(uint32_t);
  for (size_t i = 1; i < sz_dw; i++) p[i] = 0u;
#endif
  // SPI0Command sends a read opcode, like SFDP Read (5Ah) with 32 bits of data
  // containing the 24 bit address followed by a dummy byte of zeros. The
  // responce is copied back into pData starting at pData[0].
  return SPI0Command(cmd, p, 32u, sz * 8u);
}

////////////////////////////////////////////////////////////////////////////////
//// Some Flash Status Register functions
SpiOpResult spi0_flash_read_status_registers_2B(uint32_t *pStatus) {
  *pStatus = 0u;
  SpiOpResult ok0 = spi0_flash_read_status_register_1(pStatus);
  if (SPI_RESULT_OK == ok0) {
    uint32_t statusR2 = 0u;
    ok0 = spi0_flash_read_status_register_2(&statusR2);
    // SPI0Command clears unused bits in 32-bit word.
    if (SPI_RESULT_OK == ok0) *pStatus |= (statusR2 << 8u);
  }
  return ok0;
}

SpiOpResult spi0_flash_read_status_registers_3B(uint32_t *pStatus) {
  SpiOpResult ok0 = spi0_flash_read_status_registers_2B(pStatus);
  if (SPI_RESULT_OK == ok0) {
    uint32_t statusR3 = 0u;
    ok0 = spi0_flash_read_status_register_3(&statusR3);
    if (SPI_RESULT_OK == ok0) *pStatus |= (statusR3 << 16u);
  }
  return ok0;
}

SpiOpResult spi0_flash_write_status_registers_2B(uint32_t status16, bool non_volatile) {
  // Assume the flash supports 2B write if the SPIC2BSE bit is set.
  if (0u == (SPI0C & SPIC2BSE)) {
    DBG_SFU_PRINTF("\n* 2 Byte Status Write not enabled\n");
    // Let them try. If it fails, it will be discovered on verify.
    // All updates should/must be verified.
    // return SPI_RESULT_ERR;
  }

  // Winbond supports, some GD devices do not.
  return spi0_flash_write_status_register_1(status16, non_volatile, 16u);
}

//  spi0_flash_command_pair(kEnableResetCmd, kResetCmd);
void IRAM_ATTR spi0_flash_command_pair(const uint8_t cmd1, const uint8_t cmd2) {
  system_soft_wdt_feed();

  Cache_Read_Disable_2();
  Wait_SPI_Idle(flashchip);
  uint32_t saved_ps = xt_rsil(15); // Only needed for bad ISRs
  // preserve essential controller state such as incoming/outgoing
  // data lengths and IO mode.
  uint32_t oldSPI0C = SPI0C;
  uint32_t oldSPI0U = SPI0U;
  uint32_t oldSPI0U2= SPI0U2;

  // Select user defined command mode in the controller
  uint32_t spiu = SPIUCOMMAND | SPIUCSSETUP; //SPI_USR_COMMAND | SPI_CS_SETUP
  uint32_t spiu2 = ((7 & SPIMCOMMAND)<<SPILCOMMAND); // Set the command byte to send - minus the command
  // Select the most basic IO mode for maximum compatibility
  // Some flash commands are only available in this mode.
  uint32_t spic = oldSPI0C;
  spic &= ~(SPICQIO | SPICDIO | SPICQOUT | SPICDOUT | SPICAHB | SPICFASTRD);
  spic |= (SPICRESANDRES | SPICSHARE | SPICWPR | SPIC2BSE);

  // Send prefix cmd w/o data - sends 8 bits. eg. Volatile SR Write Enable, 0x50
  SPI0C  = spic;
  SPI0U  = spiu;
  SPI0U1 = 0u;

  SPI0U2 = spiu2 | cmd1;
  SPI0CMD = SPICMDUSR;   //Send cmd
  while ((SPI0CMD & SPICMDUSR));

  SPI0U2 = spiu2 | cmd2;
  SPI0CMD = SPICMDUSR;   //Send cmd
  while ((SPI0CMD & SPICMDUSR));

  // Restore saved registers
  SPI0U  = oldSPI0U;
  SPI0U2 = oldSPI0U2;
  SPI0C  = oldSPI0C;

  // needs 10us (tRST) or 40us for GigaDevice or 25ms if erase was running.
  ets_delay_us(25000u);
  // Before making lots of demands after a reset, say hello first with a read
  // status. Otherwise, the iCache stuff gets zeros (observed with XMC)
  // Yes, SPI_read_status() is also called within Wait_SPI_Idle(); however, that
  // by itself was not enough.
  uint32_t status;
  SPI_read_status(flashchip, &status);  // function will spin while WIP is set
  Wait_SPI_Idle(flashchip);
  WDT_FEED();
  xt_wsr_ps(saved_ps);
  Cache_Read_Enable_2();
}

};  // namespace experimental {

};

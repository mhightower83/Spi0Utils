/*
  SPI0 Flash Utilities
*/
#include <Arduino.h>

extern "C" {
  #include "SpiFlashUtils.h"
  using experimental::SPI0Command;
};

// #define DEV_DEBUG 1

#define ETS_PRINTF ets_uart_printf
#define NOINLINE __attribute__((noinline))

#ifdef DEV_DEBUG
#define DBG_PRINTF ets_uart_printf
#define DBG_NOINLINE __attribute__((noinline))
#else
#define DBG_PRINTF(...) do {} while (false)
#define DBG_NOINLINE
#endif

extern "C" {
////////////////////////////////////////////////////////////////////////////////
// base function see .h
// common logic, 24 bit address reads with one dummpy byte.
SpiFlashOpResult _spi0_flash_read_common(uint32_t offset, uint32_t *p, size_t sz, const uint8_t cmd) {
  if (sz > 64 || (sz % sizeof(uint32_t))) return SPI_FLASH_RESULT_ERR;
  size_t sz_dw = sz / sizeof(uint32_t);
  FlashAddr24 addr24bit;
  // MSB goes on wire first. It comes out of the LSB of a 32-bit word.
  spi_set_addr(addr24bit.u8, offset);
  addr24bit.u8[3] = 0u; // Read has dummy byte after address.
  p[0] = addr24bit.u32;
  for (size_t i = 1; i < sz_dw; i++) p[i] = 0;
  return (SpiFlashOpResult)SPI0Command(cmd, p, 32, sz * 8);
}

////////////////////////////////////////////////////////////////////////////////
//// Some Flash Status Register functions
SpiFlashOpResult spi0_flash_read_status_registers_2B(uint32_t *pStatus) {
  *pStatus = 0;
  auto ok0 = spi0_flash_read_status_register_1(pStatus);
  if (SPI_FLASH_RESULT_OK == ok0) {
    uint32_t statusR2 = 0;
    ok0 = spi0_flash_read_status_register_2(&statusR2);
    if (SPI_FLASH_RESULT_OK == ok0) *pStatus |= (statusR2 << 8);
  }
  return ok0;
}

SpiFlashOpResult spi0_flash_read_status_registers_3B(uint32_t *pStatus) {
  auto ok0 = spi0_flash_read_status_registers_2B(pStatus);
  if (SPI_FLASH_RESULT_OK == ok0) {
    uint32_t statusR3 = 0;
    ok0 = spi0_flash_read_status_register_3(&statusR3);
    if (SPI_FLASH_RESULT_OK == ok0) *pStatus |= (statusR3 << 16);
  }
  return ok0;
}

SpiFlashOpResult spi0_flash_write_status_registers_2B(uint32_t status16, bool non_volatile) {
  // Assume the flash supports 2B write if the SPIC2BSE bit is set.
  if (0 == (SPI0C & SPIC2BSE)) {
    DBG_PRINTF("\n2 Byte Status Write not enabled\n");
    // Let them try. If it fails, it will be discovered on verify.
    // All updates should/must be verified.
    // return SPI_FLASH_RESULT_ERR;
  } else {
    DBG_PRINTF("\n2 Byte Status Write enabled\n");
  }

  // Winbond supports, some GD devices do not.
  return spi0_flash_write_status_register_1(status16, non_volatile, 16u);
}

};

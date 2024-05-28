/*
  SPI0 Flash Utilities
*/
#ifndef SPIFLASHUTILS_H
#define SPIFLASHUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

// #include <BootROM_NONOS.h>
#include <spi_flash.h>    // SpiFlashOpResult
#include <spi_utils.h>
using experimental::SPI0Command;

constexpr uint8_t kVolatileWriteEnableCmd     = 0x50u;
constexpr uint8_t kWriteEnableCmd             = 0x06u;
constexpr uint8_t kWriteDisableCmd            = 0x04u;

constexpr uint8_t kEnableResetCmd             = 0x66u;
constexpr uint8_t kResetCmd                   = 0x99u;
constexpr uint8_t kChipEraseCmd               = 0x60u; // Chip Erase (CE) (C7h or 60h)

constexpr uint8_t kReadStatusRegister1Cmd     = 0x05u;
constexpr uint8_t kReadStatusRegister2Cmd     = 0x35u;
constexpr uint8_t kReadStatusRegister3Cmd     = 0x15u;

constexpr uint8_t kWriteStatusRegister1Cmd    = 0x01u;
constexpr uint8_t kWriteStatusRegister2Cmd    = 0x31u;
constexpr uint8_t kWriteStatusRegister3Cmd    = 0x11u;

constexpr uint8_t kReadUniqueIdCmd            = 0x4Bu;
constexpr uint8_t kReadSFDPCmd                = 0x5Au;

constexpr uint8_t kProgramSecurityRegisterCmd = 0x42u;
constexpr uint8_t kEraseSecurityRegisterCmd   = 0x44u;
constexpr uint8_t kReadSecurityRegisterCmd    = 0x48u;

// Note unlike the write_enable command 0x06, the WEL bit (BIT1) is not
// affected with the write_volatile_enable command 0x50.
// Note on some devices when the WEL bit is left set from a previous failed
// operation and is combined with spi0_flash_write_volatile_enable(), the subsequent
// write status register operation may save to the non-volatile register.

#if 0 // Use pre command
inline
SpiFlashOpResult spi0_flash_write_volatile_enable(void) {
  return (SpiFlashOpResult)SPI0Command(kVolatileWriteEnableCmd, NULL, 0, 0);
}
inline
SpiFlashOpResult spi0_flash_write_enable(void) {
  return (SpiFlashOpResult)SPI0Command(kWriteEnableCmd, NULL, 0, 0);
}
#endif


inline
SpiFlashOpResult spi0_flash_write_disable() {
  return (SpiFlashOpResult)SPI0Command(kWriteDisableCmd, NULL, 0, 0);
}

inline
SpiFlashOpResult spi0_flash_read_status_register_1(uint32_t *pStatus) {
  *pStatus = 0;
  return (SpiFlashOpResult)SPI0Command(kReadStatusRegister1Cmd, pStatus, 0, 8);
}
inline
SpiFlashOpResult spi0_flash_read_status_register_2(uint32_t *pStatus) {
  *pStatus = 0;
  return (SpiFlashOpResult)SPI0Command(kReadStatusRegister2Cmd, pStatus, 0, 8);
}
inline
SpiFlashOpResult spi0_flash_read_status_register_3(uint32_t *pStatus) {
  *pStatus = 0;
  return (SpiFlashOpResult)SPI0Command(kReadStatusRegister3Cmd, pStatus, 0, 8);
}

#if 0
inline
SpiFlashOpResult spi0_flash_write_status_register_1(uint32_t status, bool non_volatile, uint32_t numbits = 8) {
  SpiFlashOpResult ok;
  uint32_t saved_ps = xt_rsil(15);
  {
    if (non_volatile) {
      ok = spi0_flash_write_enable();
    } else {
      // Some SPI Flash parts will write to non-volatile if this was left set from a previous failled write
      spi0_flash_write_disable();
      ok = spi0_flash_write_volatile_enable();  // 50h cmd
    }
    if (SPI_FLASH_RESULT_OK == ok) ok = (SpiFlashOpResult)SPI0Command(kWriteStatusRegister1Cmd, &status, numbits, 0);
  }
  xt_wsr_ps(saved_ps);
  // Optimization - this is not a high use function.
  // Always call - save the code of testing for failure of Write
  spi0_flash_write_disable();
  return ok;
}

#else
inline
SpiFlashOpResult spi0_flash_write_status_register_1(uint32_t status, bool non_volatile, uint32_t numbits = 8) {
  SpiFlashOpResult ok;

  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }
  ok = (SpiFlashOpResult)SPI0Command(kWriteStatusRegister1Cmd, &status, numbits, 0, prefix);
  // Optimization - this is not a high use function.
  // Always call - save the code of testing for failure of Write
  spi0_flash_write_disable();
  return ok;
}
#endif

#if 0
inline
SpiFlashOpResult spi0_flash_write_status_register_2(uint32_t status, bool non_volatile) {
  SpiFlashOpResult ok;
  uint32_t saved_ps = xt_rsil(15);
  {
    if (non_volatile) {
      ok = spi0_flash_write_enable();
    } else {
      spi0_flash_write_disable();
      ok = spi0_flash_write_volatile_enable();  // 50h cmd
    }
    if (SPI_FLASH_RESULT_OK == ok) ok = (SpiFlashOpResult)SPI0Command(kWriteStatusRegister2Cmd, &status, 8, 0);
  }
  xt_wsr_ps(saved_ps);
  spi0_flash_write_disable();
  return ok;
}

inline
SpiFlashOpResult spi0_flash_write_status_register_3(uint32_t status, bool non_volatile) {
  SpiFlashOpResult ok;
  uint32_t saved_ps = xt_rsil(15);
  {
    if (non_volatile) {
      ok = spi0_flash_write_enable();
    } else {
      spi0_flash_write_disable();
      ok = spi0_flash_write_volatile_enable();  // 50h cmd
    }
    if (SPI_FLASH_RESULT_OK == ok) ok = (SpiFlashOpResult)SPI0Command(kWriteStatusRegister2Cmd, &status, 8, 0);
  }
  xt_wsr_ps(saved_ps);
  spi0_flash_write_disable();
  return ok;
}
#else
inline
SpiFlashOpResult spi0_flash_write_status_register_2(uint32_t status, bool non_volatile) {
  SpiFlashOpResult ok;
  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }
  ok = (SpiFlashOpResult)SPI0Command(kWriteStatusRegister2Cmd, &status, 8, 0, prefix);
  spi0_flash_write_disable();
  return ok;
}

inline
SpiFlashOpResult spi0_flash_write_status_register_3(uint32_t status, bool non_volatile) {
  SpiFlashOpResult ok;
  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }
  ok = (SpiFlashOpResult)SPI0Command(kWriteStatusRegister2Cmd, &status, 8, 0, prefix);
  spi0_flash_write_disable();
  return ok;
}
#endif


struct FlashAddr24 {
  union {
    uint8_t u8[4];
    uint32_t u32;
  };
};

inline
void spi_set_addr(uint8_t *buf, const uint32_t addr) {
  buf[0] = addr >> 16u;
  buf[1] = (addr >> 8u) & 0xFFu;
  buf[2] = addr & 0xFFu;
}


SpiFlashOpResult spi0_flash_read_status_registers_2B(uint32_t *pStatus);
SpiFlashOpResult spi0_flash_read_status_registers_3B(uint32_t *pStatus);

// Concerns:
// * This function requires the Flash Chip to support legacy 16-bit status register writes.
// * Some newer Flash Chips only support 8-bit status registry writes (1, 2, 3)
SpiFlashOpResult spi0_flash_write_status_registers_2B(uint32_t status, bool non_volatile);


#if 0
inline
SpiFlashOpResult spi0_flash_software_reset() {
  uint32_t save_ps = xt_rsil(15);
  SpiFlashOpResult ok0 = (SpiFlashOpResult)SPI0Command(kEnableResetCmd, NULL, 0, 0);
  if (SPI_FLASH_RESULT_OK == ok0) {
    ok0 = (SpiFlashOpResult)SPI0Command(kResetCmd, NULL, 0, 0);
    ets_delay_us(20u); // needs 10us (tRST)
  }
  xt_wsr_ps(save_ps);
  return ok0;
}
#else
inline
SpiFlashOpResult spi0_flash_software_reset() {
  SpiFlashOpResult ok0 = (SpiFlashOpResult)SPI0Command(kResetCmd, NULL, 0, 0, kEnableResetCmd);
  ets_delay_us(20u); // needs 10us (tRST)
  return ok0;
}
#endif

#if 0
inline
SpiFlashOpResult spi0_flash_chip_erase() {
  uint32_t save_ps = xt_rsil(15);
  SpiFlashOpResult ok0 = (SpiFlashOpResult)SPI0Command(kWriteEnableCmd, NULL, 0, 0);
  if (SPI_FLASH_RESULT_OK == ok0) {
    ok0 = (SpiFlashOpResult)SPI0Command(kChipEraseCmd, NULL, 0, 0);
    // On success - At return all is unstable. Running on code cached before the
    // Flash was erased. When that runs out we crash.
    if (SPI_FLASH_RESULT_OK == ok0) while(true);
  }
  xt_wsr_ps(save_ps);
  return ok0;
}
#else
inline
SpiFlashOpResult spi0_flash_chip_erase() {
  SpiFlashOpResult ok0 = (SpiFlashOpResult)SPI0Command(kChipEraseCmd, NULL, 0, 0, kWriteEnableCmd);
  // On success - At return, all is unstable. Running on code cached before the
  // Flash was erased. When that runs out we crash.
  if (SPI_FLASH_RESULT_OK == ok0) while(true);
  return ok0;
}
#endif


////////////////////////////////////////////////////////////////////////////////
// common 24 bit address reads with one dummpy byte.
SpiFlashOpResult _spi0_flash_read_common(uint32_t offset, uint32_t *p, size_t sz, const uint8_t cmd);

inline
SpiFlashOpResult spi0_flash_read_unique_id(uint32_t offset, uint32_t *pUnique16B, size_t sz) {
  return _spi0_flash_read_common(offset, pUnique16B, sz, kReadUniqueIdCmd);
}

inline
SpiFlashOpResult spi0_flash_read_unique_id_64(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0, pUnique16B, 8);
}

inline
SpiFlashOpResult spi0_flash_read_unique_id_128(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0, pUnique16B, 16);
}

inline
SpiFlashOpResult spi0_flash_read_sfdp(uint32_t addr, uint32_t *p, size_t sz) {
  return _spi0_flash_read_common(addr, p, sz, kReadSFDPCmd);
}

inline
SpiFlashOpResult spi0_flash_read_secure_register(uint32_t reg, uint32_t offset,  uint32_t *p, size_t sz) {
  // reg range {1, 2, 3}
  return _spi0_flash_read_common((reg << 12u) + offset, p, sz, kReadSecurityRegisterCmd);
}

#ifdef __cplusplus
}
#endif

#endif // SPIFLASHUTIL_H

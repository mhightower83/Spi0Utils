/*
  SPI0 Flash Utilities

  Consider PR to add to  .../spi_utils.h
*/
#ifndef SPIFLASHUTILS_H
#define SPIFLASHUTILS_H

#ifdef __cplusplus
extern "C" {
#endif

// #include <BootROM_NONOS.h>
extern SpiFlashOpResult spi_flash_read_status(uint32_t *status);
#include <spi_flash.h>    // SpiOpResult
#include <spi_utils.h>
// using experimental::SPI0Command;
using namespace experimental;

enum SpiFlashStatusRegister {
    non_volatile_bit = true,
    volatile_bit = false
};

//
// Note NOT all commands listed are universally available on all SPI Flash memory.
// EN25Q32C noted as an example
constexpr uint8_t kVolatileWriteEnableCmd     = 0x50u;
constexpr uint8_t kWriteEnableCmd             = 0x06u;
constexpr uint8_t kWriteDisableCmd            = 0x04u;

constexpr uint8_t kEnableResetCmd             = 0x66u;
constexpr uint8_t kResetCmd                   = 0x99u;
constexpr uint8_t kChipEraseCmd               = 0x60u; // Chip Erase (CE) (C7h or 60h)

constexpr uint8_t kReadStatusRegister1Cmd     = 0x05u;
// None of these on EN25Q32C
constexpr uint8_t kReadStatusRegister2Cmd     = 0x35u;
constexpr uint8_t kReadStatusRegister3Cmd     = 0x15u;

// None of these on EN25Q32C
constexpr uint8_t kWriteStatusRegister1Cmd    = 0x01u;
constexpr uint8_t kWriteStatusRegister2Cmd    = 0x31u;
constexpr uint8_t kWriteStatusRegister3Cmd    = 0x11u;

constexpr uint8_t kPageProgramCmd             = 0x02u;
constexpr uint8_t kReadDataCmd                = 0x03u;
constexpr uint8_t kSectorEraseCmd             = 0x20u;
constexpr uint8_t kJedecId                    = 0x9Fu;

// Conflict on EN25Q32C - (4-4-4) Fast Read Opcode
constexpr uint8_t kReadUniqueIdCmd            = 0x4Bu;

// The EN25Q32C has a 96-bit Unique ID in the SFDP response at bytes 80h:8Bh (95:00)
constexpr uint8_t kReadSFDPCmd                = 0x5Au;

// None of these on EN25Q32C
constexpr uint8_t kProgramSecurityRegisterCmd = 0x42u;
constexpr uint8_t kEraseSecurityRegisterCmd   = 0x44u;
constexpr uint8_t kReadSecurityRegisterCmd    = 0x48u;

// Note unlike the write_enable command 0x06, the WEL bit (BIT1) is not
// affected with the write_volatile_enable command 0x50.
// Note on some devices when the WEL bit is left set from a previous failed
// operation and is combined with spi0_flash_write_volatile_enable(), the subsequent
// write status register operation may save to the non-volatile register.

#if 0
// Not needed when using SPI0Command's pre_cmd argument
inline
SpiOpResult spi0_flash_write_volatile_enable(void) {
  return SPI0Command(kVolatileWriteEnableCmd, NULL, 0, 0);
}
inline
SpiOpResult spi0_flash_write_enable(void) {
  return SPI0Command(kWriteEnableCmd, NULL, 0, 0);
}
#endif


inline
SpiOpResult spi0_flash_write_disable() {
  return SPI0Command(kWriteDisableCmd, NULL, 0, 0);
}

#if 0
inline
SpiOpResult spi0_flash_read_status_register_1(uint32_t *pStatus) {
  *pStatus = 0;
  return SPI0Command(kReadStatusRegister1Cmd, pStatus, 0, 8);
}
#else
inline
SpiOpResult spi0_flash_read_status_register_1(uint32_t *pStatus) {
  *pStatus = 0;
  // Use the version provided by the SDK - return enums are the same
  return (SpiOpResult)spi_flash_read_status(pStatus);
}
#endif

inline
SpiOpResult spi0_flash_read_status_register_2(uint32_t *pStatus) {
  *pStatus = 0;
  return SPI0Command(kReadStatusRegister2Cmd, pStatus, 0, 8);
}
inline
SpiOpResult spi0_flash_read_status_register_3(uint32_t *pStatus) {
  *pStatus = 0;
  return SPI0Command(kReadStatusRegister3Cmd, pStatus, 0, 8);
}

inline
SpiOpResult spi0_flash_write_status_register(uint32_t idx0, uint32_t status, bool non_volatile, uint32_t numbits = 8) {
  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }

  uint8_t cmd = 0;
  if (0 == idx0) {
    cmd = kWriteStatusRegister1Cmd;
  } else if (1 == idx0) {
    cmd = kWriteStatusRegister2Cmd;
  } else if (2 == idx0) {
    cmd = kWriteStatusRegister3Cmd;
  }

  SpiOpResult ok0 = SPI0Command(cmd, &status, numbits, 0, prefix);
  // Optimization - this is not a high use function.
  // Always call - save the code of testing for failure of Write
  spi0_flash_write_disable();
  return ok0;
}

inline
SpiOpResult spi0_flash_write_status_register_1(uint32_t status, bool non_volatile, uint32_t numbits=8) {
  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }
  SpiOpResult ok0 = SPI0Command(kWriteStatusRegister1Cmd, &status, numbits, 0, prefix);
  // Optimization - this is not a high use function.
  // Always call - save the code of testing for failure of Write
  spi0_flash_write_disable();
  return ok0;
}

inline
SpiOpResult spi0_flash_write_status_register_2(uint32_t status, bool non_volatile) {
  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }
  SpiOpResult ok0 = SPI0Command(kWriteStatusRegister2Cmd, &status, 8, 0, prefix);
  spi0_flash_write_disable();
  return ok0;
}

inline
SpiOpResult spi0_flash_write_status_register_3(uint32_t status, bool non_volatile) {
  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }
  SpiOpResult ok0 = SPI0Command(kWriteStatusRegister3Cmd, &status, 8, 0, prefix);
  spi0_flash_write_disable();
  return ok0;
}

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


SpiOpResult spi0_flash_read_status_registers_2B(uint32_t *pStatus);
SpiOpResult spi0_flash_read_status_registers_3B(uint32_t *pStatus);

// Concerns:
// * This function requires the Flash Chip to support legacy 16-bit status register writes.
// * Some newer Flash Chips only support 8-bit status registry writes (1, 2, 3)
SpiOpResult spi0_flash_write_status_registers_2B(uint32_t status, bool non_volatile);

inline
SpiOpResult spi0_flash_software_reset() {
  // SPI0Command method is not safe. iCache reads could be attempted within
  // the 10us (tRST) required to reliable reset.
  // This should be done out of IRAM with a Cache_Read_Disable_2/
  // Cache_Read_Enable_2 wrapper.
  SpiOpResult ok0 = SPI0Command(kResetCmd, NULL, 0, 0, kEnableResetCmd);
  ets_delay_us(20u); // needs 10us (tRST)
  return ok0;
}

inline
SpiOpResult spi0_flash_chip_erase() {
  SpiOpResult ok0 = SPI0Command(kChipEraseCmd, NULL, 0, 0, kWriteEnableCmd);
  // On success - At return, all is unstable. Running on code cached before the
  // Flash was erased. When that runs out we crash.
  if (SPI_RESULT_OK == ok0) while(true);
  return ok0;
}

////////////////////////////////////////////////////////////////////////////////
// common 24 bit address reads with one dummpy byte.
SpiOpResult _spi0_flash_read_common(uint32_t offset, uint32_t *p, size_t sz, const uint8_t cmd);

inline
SpiOpResult spi0_flash_read_unique_id(uint32_t offset, uint32_t *pUnique16B, size_t sz) {
  return _spi0_flash_read_common(offset, pUnique16B, sz, kReadUniqueIdCmd);
}

inline
SpiOpResult spi0_flash_read_unique_id_64(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0, pUnique16B, 8);
}

inline
SpiOpResult spi0_flash_read_unique_id_96(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0, pUnique16B, 12);
}

inline
SpiOpResult spi0_flash_read_unique_id_128(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0, pUnique16B, 16);
}

inline
SpiOpResult spi0_flash_read_sfdp(uint32_t addr, uint32_t *p, size_t sz) {
  return _spi0_flash_read_common(addr, p, sz, kReadSFDPCmd);
}

inline
SpiOpResult spi0_flash_read_secure_register(uint32_t reg, uint32_t offset,  uint32_t *p, size_t sz) {
  // reg range {1, 2, 3}
  return _spi0_flash_read_common((reg << 12u) + offset, p, sz, kReadSecurityRegisterCmd);
}

// Useful when Flash ID is needed before the NONOS_SDK has initialized.
inline
uint32_t alt_spi_flash_get_id(void) {
  uint32_t _id = 0;
  SpiOpResult ok0 = SPI0Command(kJedecId, &_id, 0, 24u);
  return (SPI_RESULT_OK == ok0) ? _id : 0xFFFFFFFFu;
}

#ifdef __cplusplus
}
#endif

#endif // SPIFLASHUTIL_H

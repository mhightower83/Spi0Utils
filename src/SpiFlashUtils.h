/*
  SPI0 Flash Utilities
*/
#ifndef EXPERIMENTAL_SPIFLASHUTILS_H
#define EXPERIMENTAL_SPIFLASHUTILS_H

#if ((1 - DEBUG_FLASH_QE - 1) == 2)
#undef DEBUG_FLASH_QE
#define DEBUG_FLASH_QE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern SpiFlashOpResult spi_flash_read_status(uint32_t *status); // NONOS_SDK
#include <spi_flash.h>    // SpiOpResult
#include <spi_utils.h>

// Dropout unused debug printfs
#ifndef DBG_SFU_PRINTF
#define DBG_SFU_PRINTF(...) do {} while (false)
#endif

namespace experimental {

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

// These two are seldom needed when using SPI0Command's pre_cmd argument
inline
SpiOpResult spi0_flash_write_volatile_enable(void) {
  return SPI0Command(kVolatileWriteEnableCmd, NULL, 0u, 0);
}
inline
SpiOpResult spi0_flash_write_enable(void) {
  return SPI0Command(kWriteEnableCmd, NULL, 0u, 0);
}

inline
SpiOpResult spi0_flash_write_disable() {
  return SPI0Command(kWriteDisableCmd, NULL, 0u, 0);
}

#if 0
inline
SpiOpResult spi0_flash_read_status_register_1(uint32_t *pStatus) {
  *pStatus = 0u;
  return SPI0Command(kReadStatusRegister1Cmd, pStatus, 0u, 8u);
}
#else
inline
SpiOpResult spi0_flash_read_status_register_1(uint32_t *pStatus) {
  *pStatus = 0u;
  // Use the version provided by the SDK - return enums are the same
  return (SpiOpResult)spi_flash_read_status(pStatus);
}
#endif

inline
SpiOpResult spi0_flash_read_status_register(const size_t idx0, uint32_t *pStatus) {
  *pStatus = 0u;
  uint8_t cmd;
  if (0u == idx0) {
    cmd = kReadStatusRegister1Cmd;
  } else if (1u == idx0) {
    cmd = kReadStatusRegister2Cmd;
  } else if (2u == idx0) {
    cmd = kReadStatusRegister3Cmd;
  } else {
    // panic();
    return SPI_RESULT_ERR;
  }
  return SPI0Command(cmd, pStatus, 0u, 8u);
}

inline
SpiOpResult spi0_flash_read_status_register_2(uint32_t *pStatus) {
  return spi0_flash_read_status_register(1, pStatus);
}
inline
SpiOpResult spi0_flash_read_status_register_3(uint32_t *pStatus) {
  return spi0_flash_read_status_register(2, pStatus);
}

inline
SpiOpResult spi0_flash_write_status_register(const uint32_t idx0, uint32_t status, const bool non_volatile, const uint32_t numbits = 8) {
  uint8_t cmd = 0u;
  if (0u == idx0) {
    cmd = kWriteStatusRegister1Cmd;
  } else if (1u == idx0) {
    cmd = kWriteStatusRegister2Cmd;
  } else if (2u == idx0) {
    cmd = kWriteStatusRegister3Cmd;
  } else {
    // panic();
    return SPI_RESULT_ERR;
  }

  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }

  SpiOpResult ok0 = SPI0Command(cmd, &status, numbits, 0u, prefix);
  // Optimization - this is not a high use function.
  // Always call - save the code of testing for failure of Write
  spi0_flash_write_disable();
  return ok0;
}

inline
SpiOpResult spi0_flash_write_status_register_1(uint32_t status, const bool non_volatile, const uint32_t numbits=8) {
  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }
  SpiOpResult ok0 = SPI0Command(kWriteStatusRegister1Cmd, &status, numbits, 0u, prefix);
  // Optimization - this is not a high use function.
  // Always call - save the code of testing for failure of Write
  spi0_flash_write_disable();
  return ok0;
}

inline
SpiOpResult spi0_flash_write_status_register_2(uint32_t status, const bool non_volatile) {
  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }
  SpiOpResult ok0 = SPI0Command(kWriteStatusRegister2Cmd, &status, 8u, 0u, prefix);
  spi0_flash_write_disable();
  return ok0;
}

inline
SpiOpResult spi0_flash_write_status_register_3(uint32_t status, const bool non_volatile) {
  uint32_t prefix = kWriteEnableCmd;
  if (! non_volatile) {
    spi0_flash_write_disable();
    prefix = kVolatileWriteEnableCmd;
  }
  SpiOpResult ok0 = SPI0Command(kWriteStatusRegister3Cmd, &status, 8u, 0u, prefix);
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
// * Some Flash Chips only support 8-bit status registry writes (1, 2, 3)
//   * see spi0_flash_write_status_register(idx, status) for 8-bit writes.
SpiOpResult spi0_flash_write_status_registers_2B(uint32_t status, const bool non_volatile);

// Use for tightly sending two commands to the Flash. Like an enable instruction
// followed by the action instruction. No other flash instruction get inserted
// between them.
void spi0_flash_command_pair(const uint8_t cmd1, const uint8_t cmd2);

//D #if 0
//D inline
//D SpiOpResult spi0_flash_software_reset() {
//D   // SPI0Command method is not safe. iCache reads could be attempted within
//D   // the 10us (tRST) required to reliable reset.
//D   // This should be done out of IRAM with a Cache_Read_Disable_2/
//D   // Cache_Read_Enable_2 wrapper.
//D   // TODO move logic to IRAM and feed WDT for 100ms.
//D   SpiOpResult ok0 = SPI0Command(kResetCmd, NULL, 0u, 0u, kEnableResetCmd);
//D   ets_delay_us(20u); // needs 10us (tRST)
//D   // or 40us for GigaDevice or 25ms if erase was running.
//D   return ok0;
//D }
//D #else
inline
SpiOpResult spi0_flash_software_reset() {
  spi0_flash_command_pair(kEnableResetCmd, kResetCmd);
  return SPI_RESULT_OK;
}
//D #endif

inline
SpiOpResult spi0_flash_chip_erase() {
  SpiOpResult ok0 = SPI0Command(kChipEraseCmd, NULL, 0u, 0u, kWriteEnableCmd);
  // On success - At return, all is unstable. Running on code cached before the
  // Flash was erased. When that runs out we crash.
  if (SPI_RESULT_OK == ok0) while(true);
  return ok0;
}

////////////////////////////////////////////////////////////////////////////////
// common 24 bit address reads with one dummpy byte.
SpiOpResult _spi0_flash_read_common(const uint32_t offset, uint32_t *p, const size_t sz, const uint8_t cmd);

inline
SpiOpResult spi0_flash_read_unique_id(const uint32_t offset, uint32_t *pUnique16B, const size_t sz) {
  return _spi0_flash_read_common(offset, pUnique16B, sz, kReadUniqueIdCmd);
}

inline
SpiOpResult spi0_flash_read_unique_id_64(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0u, pUnique16B, 8u);
}

inline
SpiOpResult spi0_flash_read_unique_id_96(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0u, pUnique16B, 12u);
}

inline
SpiOpResult spi0_flash_read_unique_id_128(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0u, pUnique16B, 16u);
}

inline
SpiOpResult spi0_flash_read_sfdp(const uint32_t addr, uint32_t *p, const size_t sz) {
  return _spi0_flash_read_common(addr, p, sz, kReadSFDPCmd);
}

inline
SpiOpResult spi0_flash_read_secure_register(const uint32_t reg, const uint32_t offset,  uint32_t *p, const size_t sz) {
  // reg range {1, 2, 3}
  return _spi0_flash_read_common((reg << 12u) + offset, p, sz, kReadSecurityRegisterCmd);
}

// Useful when Flash ID is needed before the NONOS_SDK has initialized.
inline
uint32_t alt_spi_flash_get_id(void) {
  uint32_t _id = 0u;
  SpiOpResult ok0 = SPI0Command(kJedecId, &_id, 0u, 24u);
  return (SPI_RESULT_OK == ok0) ? _id : 0xFFFFFFFFu;
}

};  // namespace experimental {

#ifdef __cplusplus
}
#endif

#endif // EXPERIMENTAL_SPIFLASHUTILS_H

/*
  SPI0 Flash Utilities

  Consider PR to add to  .../spi_utils.h
*/
#ifndef SPIFLASHUTILS_H
#define SPIFLASHUTILS_H

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
SpiOpResult spi0_flash_write_status_register(const uint32_t idx0, uint32_t status, const bool non_volatile, const uint32_t numbits = 8) {
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
SpiOpResult spi0_flash_write_status_register_1(uint32_t status, const bool non_volatile, const uint32_t numbits=8) {
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
SpiOpResult spi0_flash_write_status_register_2(uint32_t status, const bool non_volatile) {
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
SpiOpResult spi0_flash_write_status_register_3(uint32_t status, const bool non_volatile) {
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
// * Some Flash Chips only support 8-bit status registry writes (1, 2, 3)
SpiOpResult spi0_flash_write_status_registers_2B(uint32_t status, const bool non_volatile);

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
SpiOpResult _spi0_flash_read_common(const uint32_t offset, uint32_t *p, const size_t sz, const uint8_t cmd);

inline
SpiOpResult spi0_flash_read_unique_id(const uint32_t offset, uint32_t *pUnique16B, const size_t sz) {
  return _spi0_flash_read_common(offset, pUnique16B, sz, kReadUniqueIdCmd);
}

inline
SpiOpResult spi0_flash_read_unique_id_64(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0, pUnique16B, 8u);
}

inline
SpiOpResult spi0_flash_read_unique_id_96(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0, pUnique16B, 12u);
}

inline
SpiOpResult spi0_flash_read_unique_id_128(uint32_t *pUnique16B) {
  return spi0_flash_read_unique_id(0, pUnique16B, 16u);
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
  uint32_t _id = 0;
  SpiOpResult ok0 = SPI0Command(kJedecId, &_id, 0, 24u);
  return (SPI_RESULT_OK == ok0) ? _id : 0xFFFFFFFFu;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// From here down
// Less general and more specific Flash operations
// Maybe this should be a separate .h
//
#include "BootROM_NONOS.h"

#ifndef PRESERVE_EXISTING_STATUS_BITS
// It may be best to not preserve existing Flash Status Register Bits.
// I am concerned about setting OTP on the part and being locked out.
// The BIT meanings vary too much between vendors and it is extra code.
//
// Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
// to zero without much care we assume they can all be zero at the startup.
//
// TODO factor out as always 0
#define PRESERVE_EXISTING_STATUS_BITS 0
#endif

#if DEBUG_FLASH_QE && ! defined(DBG_SFU_PRINTF)
#define DBG_SFU_PRINTF ets_uart_printf
#endif

#ifndef DBG_SFU_PRINTF
#define DBG_SFU_PRINTF(...) do {} while (false)
#endif

constexpr uint32_t kWIPBit   = BIT0;  // Write In Progress
constexpr uint32_t kWELBit   = BIT1;  // Write Enable Latch
constexpr uint32_t kWPDISBit = BIT6;  // Disable /WP pin
constexpr uint32_t kQEBit1B  = BIT1;  // Enable QE=1, disables WP# and HOLD#
constexpr uint32_t kQEBit2B  = BIT9;  // Enable QE=1, disables WP# and HOLD#

inline
bool verify_status_register_1(const uint32_t a_bit_mask) {
  uint32_t status = 0;
  spi0_flash_read_status_register_1(&status);
  return ((a_bit_mask & status) == a_bit_mask);
}

inline
bool verify_status_register_2(const uint32_t a_bit_mask) {
  uint32_t status = 0;
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
bool is_S6_QE_WPDis(void) {
  bool success = verify_status_register_1(kWPDISBit);
  DBG_SFU_PRINTF("  %s bit %s set.\n", "S6/QE/WPDis", (success) ? "confirmed" : "NOT");
  return success;
}

// QE => Quad Enable, S9
inline
bool is_QE(void) {
  bool success = verify_status_register_2(kQEBit1B);
  DBG_SFU_PRINTF("  %s bit %s set.\n", "QE", (success) ? "confirmed" : "NOT");
  return success;
}

// Note, there are other SPI registers with bits for other aspects of QUAD
// support. I chose these.
// Returns true when SPI0 controler is in either QOUT or QIO mode.
inline
bool is_spi0_quad(void) {
  return (0 != ((SPICQIO | SPICQOUT) & SPI0C));
}

// Only for EN25Q32C, earlier version may not work.
// Maybe add SFDP call to validate EN25Q32C part.
// see RTOS_SDK/components/spi_flash/src/spi_flash.c
// Don't rely on that Espressif sample too closely. The data shown for EN25Q16A
// is not correct. The EN25Q16A and EN25Q16B do no support SFDP.
[[maybe_unused]]
static bool set_S6_QE_WPDis_bit(const bool non_volatile) {
  uint32_t status = 0;
  spi0_flash_read_status_register_1(&status);
  bool is_set = (0 != (status & kWPDISBit));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "S6/QE/WPDis", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status |= kWPDISBit;
#else
  // Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
  // to zero without much care we assume they can all be zero at the startup.
  status = kWPDISBit;
#endif
  // All changes made to the volatile copies of the Status Register-1.
  DBG_SFU_PRINTF("  Setting %svolatile %s bit.\n", (non_volatile) ? "non-" : "", "S6/QE/WPDis");
  spi0_flash_write_status_register_1(status, non_volatile);
  return is_S6_QE_WPDis();
}

[[maybe_unused]]
static bool clear_S6_QE_WPDis_bit(void) {
  uint32_t status = 0;
  spi0_flash_read_status_register_1(&status);
  bool not_set = (0 == (status & kWPDISBit));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "S6/QE/WPDis", (not_set) ? "NOT" : "confirmed");
  if (not_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status &= ~kWPDISBit;
#else
  // Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
  // to zero without much care we assume they can all be zero at the startup.
  status = 0;
#endif
  // All changes made to the volatile copies of the Status Register-1.
  DBG_SFU_PRINTF("  Clearing volatile S6/QE/WPDis bit - 8-bit write.\n");
  spi0_flash_write_status_register_1(status, false);
  return (false == is_S6_QE_WPDis());
}

[[maybe_unused]]
static bool set_QE_bit__8_bit_sr2_write(const bool non_volatile) {
  uint32_t status2 = 0;
  spi0_flash_read_status_register_2(&status2);
  bool is_set = (0 != (status2 & kQEBit1B));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "QE", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status2 |= kQEBit1B;
#else
  // Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
  // to zero without much care we assume they can all be zero at the startup.
  status2 = kQEBit1B;
#endif
  DBG_SFU_PRINTF("  Setting %svolatile %s bit - %u-bit write.\n", (non_volatile) ? "non-" : "", "QE", 8u);
  spi0_flash_write_status_register_2(status2, non_volatile);
  return is_QE();
}

[[maybe_unused]]
static bool set_QE_bit__16_bit_sr1_write(const bool non_volatile) {
  uint32_t status = 0;
  spi0_flash_read_status_registers_2B(&status);
  bool is_set = (0 != (status & kQEBit2B));
  DBG_SFU_PRINTF("  %s bit %s set.\n", "QE", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status |= kQEBit2B;
#else
  // Since BootROM functions Enable_QMode and Disable_QMode are setting SR bits
  // to zero without much care we assume they can all be zero at the startup.
  status = kQEBit2B;
#endif
  DBG_SFU_PRINTF("  Setting %svolatile %s bit - %u-bit write.\n", (non_volatile) ? "non-" : "", "QE", 16u);
  spi0_flash_write_status_registers_2B(status, non_volatile);
  return is_QE();
}

[[maybe_unused]]
static void clear_sr_mask(uint32_t reg_0idx, const uint32_t pattern) {
  uint32_t status = flash_gd25q32c_read_status(reg_0idx);
  if (pattern & status) {
    status &= ~pattern;
    flash_gd25q32c_write_status(reg_0idx, status); // 8-bit status register write
    // This message should not repeat at next boot
    DBG_SFU_PRINTF("** One time clear of Status Register-%u bits 0x%02X.\n", reg_0idx + 1, pattern);
  }
}

inline
void clear_sr1_mask(const uint32_t pattern) {
  clear_sr_mask(0, pattern);
}

inline
void clear_sr2_mask(const uint32_t pattern) {
  clear_sr_mask(1, pattern);
}

};  // namespace experimental {

#ifdef __cplusplus
}
#endif

#endif // SPIFLASHUTIL_H

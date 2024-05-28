/*
  Reclaim the use of GPIO9 and GPIO10.

  After a successful call to `reclaim_GPIO_9_10()`, pinMode can be used on GPIO
  pins 9 and 10 to define their new function.

  If `reclaim_GPIO_9_10()` returns false, check for the following:

  * The Sketch must be built with SPI Flash Mode set to DIO or DOUT.

  * Does the Flash Chip support QIO?

    For example, the EN25Q32C does not have the QE bit as defined by other
    vendors. It does not have the /HOLD signal. And /WP is disabled by status
    register-1 BIT6. This case is already handled by default unless you supply
    "-DSUPPORT_SPI_FLASH_VENDOR_EON=0" to the build.

  * You may need to write a unique case for your device. We rely on setting
    Status Register QE bit (S9) to disable the use of /WP and /HOLD pin
    functions on the Flash Chip. Reconcile this with your SPI Flash Chip
    datasheet's information.


  Setting the non-volatile copy of QE=1 may not always work for every flash
  device. The ESP8266 BootROM reads the Flash Mode from the boot image and tries
  to reprogram the non-volatile QE bit. For a Flash Mode of DIO, the BootROM
  will try and set QE=0 with 16-bit Write Status Register-1. Some parts don't
  support this length.

////////////////////////////////////////////////////////////////////////////////
//
SPI Flash Notes and Anomalies:

XMC
 1. Clears status register-3 on volatile write to register-2. Restores on power-up.
 2. Accepts 8-bit write register-2 or 16-bit write register-2.
 3. XM25Q32B and XM25Q32C have different Driver strength tables.
    MFG/Device is not enough to differentiate. Need to use SFDP.

0xD8 (Obfuscated MFG ID?, GigaDevice ID in SFDP)
 1. Volatile status registers not working, must use non-volatile.
 2. This behavior does not agree with the GD25Q32E(T) datasheet.
 3. Only supports 8-bit Status Register writes
 4. The BootROM's 16-bit register-1 writes will fail. This works in our
    favor, no extra wear on the Flash.
 5. The last 64 bits of the 128-bit Unique ID are still in the erased state.

GigaDevice
 1. No legacy 16-bit, only 8-bit write status register commands are supported.
 2. Unkown, if the volatile status registers work on the non-obfuscated parts.

Winbond
 1. My new NodeMCU v1.0 board only works with 16-bit write status register-1.
    Appears to be very old inventory.

EON
 1. EN25Q32C found on an AI Thinker ESP-12F module marked as DIO near antenna.
 2. Only has 1 Status Register. The BootROM's 16-bit register-1 writes fail.
 3. NC, No /HOLD pin function.
 4. Has WPDis, Bit6, to disable the /WP pin function.



TODO: Remove this comment later-
I think I now appreciate why Igrr placed  `void IRAM_ATTR
user_spi_flash_dio_to_qio_pre_init() {}` in core_esp8266_phy.cpp. While it does
frees up memory it also blocks you from trying to add DIO to QIO  support for a
specific Flash. With so much variability in SPI Flash chips, it may be
best/wise to avoid the extra effort/frustration to make it work.

//
////////////////////////////////////////////////////////////////////////////////

bool reclaim_GPIO_9_10();
*/

// #define DEV_DEBUG 1 // set in *.ino.globals.h instead
#if 0
// Needed when module is a .cpp
#include <Arduino.h>
#include <SpiFlashUtils.h>
#include "kStatusRegisterBitDef.h"
#include "FlashChipId_D8.h"

// #define ETS_PRINTF ets_uart_printf
#ifdef DEV_DEBUG
#define DBG_PRINTF ets_uart_printf
#else
#define DBG_PRINTF(...) do {} while (false)
#endif
#endif

#ifndef SUPPORT_SPI_FLASH_VENDOR_EON
#define SUPPORT_SPI_FLASH_VENDOR_EON 1
#endif
#ifndef SUPPORT_MYSTERY_VENDOR_D8
#define SUPPORT_MYSTERY_VENDOR_D8 1
#endif
#ifndef PRESERVE_EXISTING_STATUS_BITS
// It may be best to not preserve existing Flash Status Register Bits.
// I am concerned about setting OTP on the part and being locked out.
// The BIT meanings vary too much between vendors and it is extra code.
#define PRESERVE_EXISTING_STATUS_BITS 0
#endif

////////////////////////////////////////////////////////////////////////////////
// Handle Freeing up GPIO pins 9 and 10 for various Flash memory chips.
//
bool reclaim_GPIO_9_10(void);
bool set_QE_bit__8_bit_sr2_write(void);
bool set_QE_bit__16_bit_sr1_write(void);

#if SUPPORT_SPI_FLASH_VENDOR_EON
bool set_WPDis_bit_EON(void);
bool clear_WPDis_bit_EON(void);
#endif

inline bool verify_status_register_1(uint32_t a_bit_mask) {
  uint32_t status = 0;
  spi0_flash_read_status_register_1(&status);
  return ((a_bit_mask & status) == a_bit_mask);
}

inline bool verify_status_register_2(uint32_t a_bit_mask) {
  uint32_t status = 0;
  spi0_flash_read_status_register_2(&status);
  return ((a_bit_mask & status) == a_bit_mask);
}

// WEL => Write Enable Latch
inline bool is_WEL(void) {
  return verify_status_register_1(kWELBit);
}

// WIP => Write In Progress aka Busy
inline bool is_WIP(void) {
  return verify_status_register_1(kWIPBit);
}

// WPDis => pin Write Protect Disable, maybe only on the EN25Q32C
inline bool is_WPDis_EON(void) {
  return verify_status_register_1(kWPDISBit);
}

// QE => Quad Enable
inline bool is_QE(void) {
  return verify_status_register_2(kQEBit8);
}

// Note, there are other SPI registers with bits for other aspects of QUAD
// support. I chose these.
// Returns true when SPI0 controler is in either QOUT or QIO mode.
inline bool is_spi0_quad(void) {
  return (0 != ((SPICQIO | SPICQOUT) & SPI0C));
}

#if SUPPORT_SPI_FLASH_VENDOR_EON
// Only for EN25Q32C, earlier version may not work.
// Maybe add SFDP call to validate EN25Q32C part.
// see RTOS_SDK/components/spi_flash/src/spi_flash.c
// Don't rely on that Espressif sample too closely. The data shown for EN25Q16A
// is not correct. The EN25Q16A and EN25Q16B do no support SFDP.
bool set_WPDis_bit_EON(void) {
  uint32_t status = kWPDISBit;
#if PRESERVE_EXISTING_STATUS_BITS
  spi0_flash_read_status_register_1(&status);
  status |= kWPDISBit;
#endif
  // All changes made to the volatile copies of the Status Register-1.
  spi0_flash_write_status_register_1(status, false);
  return is_WPDis_EON();
}

bool clear_WPDis_bit_EON(void) {
  uint32_t status = 0;
#if PRESERVE_EXISTING_STATUS_BITS
  spi0_flash_read_status_register_1(&status);
  status &= ~kWPDISBit;
#endif
  // All changes made to the volatile copies of the Status Register-1.
  spi0_flash_write_status_register_1(status, false);
  return (false == is_WPDis_EON());
}
#endif

bool set_QE_bit__8_bit_sr2_write(bool non_volatile) {
  uint32_t status = kQEBit8;
#if PRESERVE_EXISTING_STATUS_BITS
  spi0_flash_read_status_register_2(&status);
  status |= kQEBit8;
#endif
  spi0_flash_write_status_register_2(status, non_volatile);
  return is_QE();
}

bool set_QE_bit__16_bit_sr1_write(bool non_volatile) {
  uint32_t status = kQEBit16;
#if PRESERVE_EXISTING_STATUS_BITS
  spi0_flash_read_status_registers_2B(&status);
  status |= kQEBit16;
#endif
  spi0_flash_write_status_registers_2B(status, non_volatile);
  return is_QE();
}

////////////////////////////////////////////////////////////////////////////////
// returns:
// true  - on success
// false - on failure
bool reclaim_GPIO_9_10() {
  bool success = false;

#if defined(RECLAIM_GPIO_EARLY) && defined(DEV_DEBUG)
  pinMode(1, SPECIAL);
  uart_buff_switch(0);
#endif
  DBG_PRINTF("\n\n\nRun reclaim_GPIO_9_10()\n");

  uint32_t _id = spi_flash_get_id();
  DBG_PRINTF("  Flash Chip ID: 0x%06X\n", _id);

  if (is_WEL()) {
    // Most likely left over from BootROM's attempt to update the Flash Status Register.
    // Common event for SPI Flash that don't support 16-bit Write Status Register-1.
    // Seen with EON's EN25Q32C, GigaDevice and Mystery Vendor 0xD8. These
    // do not support 16-bit write status register-1.
    DBG_PRINTF("  Detected: a previous write failed. The WEL bit is still set.\n");
    spi0_flash_write_disable();
  }

  // Expand to read SFDP Parameter Version. Use result to differentiate parts.

  // SPI0 must be in DIO or DOUT mode to continue.
  if (is_spi0_quad()) {
    DBG_PRINTF("  GPIO pins 9 and 10 are not available when configured for SPI Flash Modes: \"QIO\" or \"QOUT\"\n");
    return false;
  }

  switch (0xFFu & _id) {

#if SUPPORT_MYSTERY_VENDOR_D8
    case SPI_FLASH_VENDOR_MYSTERY_D8: // 0xD8u: // Mystery Vendor
      success = is_QE();
      if (success) {
        DBG_PRINTF("  QE bit already set.\n");
      } else {
        // Write volatile status register not working, must use non-volatile
        // Once set, no extra wear on flash - stays set through reboots.
        // 16-bit register writes always fail.
        success = set_QE_bit__8_bit_sr2_write(true); // non-volatile
        if (success) DBG_PRINTF("  Non-volatile QE bit set with Write Status Register-2.\n");
      }
      break;
#endif
#if SUPPORT_SPI_FLASH_VENDOR_XMC
    case SPI_FLASH_VENDOR_XMC: // 0x20
      success = is_QE();
      if (success) {
        DBG_PRINTF("  QE bit already set.\n");
      } else {
        uint32_t status_reg3;
        SpiFlashOpResult ok0 = spi0_flash_read_status_register_3(&status_reg3);
        success = set_QE_bit__8_bit_sr2_write(false);
        if (SPI_FLASH_RESULT_OK == ok0) {
          // Copy Driver Strength value from non-volatile to volatile
          ok0 = spi0_flash_write_status_register_3(status_reg3, false); // volatile
          DBG_PRINTF("  XMC Anomaly: Copy Driver Strength values to volatile status register.\n");
          if (SPI_FLASH_RESULT_OK != ok0) {
            DBG_PRINTF("    anomaly handling failed.\n");
          }
        }
      }
      if (success) DBG_PRINTF("  Volatile QE bit set with 8-bit Write Status Register.\n");
      break;
#endif
#if SUPPORT_SPI_FLASH_VENDOR_EON
    case SPI_FLASH_VENDOR_EON: // 0x1C: // EON
      if (0x301Cu == (_id & 0x0FFFFu)) {
        success = is_WPDis_EON();
        if (success) {
          DBG_PRINTF("  EN25QxxC: WPDis bit already set.\n");
        } else {
          success = set_WPDis_bit_EON();
          if (success) DBG_PRINTF("  EN25QxxC: Volatile WPDis bit set.\n");
        }
      }
      break;
#endif

    default:
      // Try legacy method first.
      success = is_QE();
      if (success) {
        DBG_PRINTF("  QE bit already set.\n");
      } else {
        success = set_QE_bit__16_bit_sr1_write(false);
        if (success) {
          DBG_PRINTF("  Volatile QE bit set using legacy 16-bit Write Status Register-1.\n");
        } else {
          success = set_QE_bit__8_bit_sr2_write(false);
          if (success) {
            DBG_PRINTF("  Volatile QE bit set using Write Status Register-2.\n");
          }
        }
      }
      break;
  }
  spi0_flash_write_disable();
  DBG_PRINTF("  SPI0 signals '/WP' and '/HOLD' were%s disabled.\n", (success) ? "" : " NOT");
  return success;
}

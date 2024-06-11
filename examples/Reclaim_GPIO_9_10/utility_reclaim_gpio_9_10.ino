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
    "-DSUPPORT_SPI_FLASH__S6_QE_WPDIS=0" to the build.

  * You may need to write a unique case for your Flash device. We rely on
    setting Status Register QE bit (S9) to disable the use of /WP and /HOLD pin
    functions on the Flash. Reconcile this with your SPI Flash datasheet's
    information.


??  Setting the non-volatile copy of QE=1 may not always work for every flash
  device. The ESP8266 BootROM reads the Flash Mode from the boot image and tries
  to reprogram the non-volatile QE bit. For a Flash Mode of DIO, the BootROM
  will try and set QE=0 with 16-bit Write Status Register-1. Some parts don't
  support this length.

////////////////////////////////////////////////////////////////////////////////
//
SPI Flash Notes and Anomalies:

XMC - SFDP Revision matches up with XM25QH32B datasheet.
 1. Clears status register-3 on volatile write to register-2. Restores on
    power-up. But not on Flash software reset, opcodes 66h-99h. However, the QE
    bit did refresh to the non-volatile value.
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
 6. Flash Software Reset, opcodes 66h-99h, clears non-volatile QE bit!!!

GigaDevice
 1. No legacy 16-bit, only 8-bit write status register commands are supported.

Winbond
 1. My new NodeMCU v1.0 board only works with 16-bit write status register-1.
    Appears to be very old inventory.

EON
 1. EN25Q32C found on an AI Thinker ESP-12F module marked as DIO near antenna.
 2. Only has 1 Status Register. The BootROM's 16-bit register-1 writes fail.
 3. NC, No /HOLD pin function.
 4. Status Register has WPDis, Bit6, to disable the /WP pin function.



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

// #define DEBUG_FLASH_QE 1 // set in *.ino.globals.h instead
#if 0
// Needed when module is a .cpp
#include <Arduino.h>
#include <SpiFlashUtils.h>
#include <kStatusRegisterBitDef.h>
#include <FlashChipId_D8.h>

#define ETS_PRINTF ets_uart_printf
#ifdef DEBUG_FLASH_QE
#define DBG_PRINTF ets_uart_printf
#else
#define DBG_PRINTF(...) do {} while (false)
#endif
#endif

#ifndef SUPPORT_SPI_FLASH__S6_QE_WPDIS
#define SUPPORT_SPI_FLASH__S6_QE_WPDIS 1
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

inline
bool verify_status_register_1(uint32_t a_bit_mask) {
  uint32_t status = 0;
  spi0_flash_read_status_register_1(&status);
  return ((a_bit_mask & status) == a_bit_mask);
}

inline
bool verify_status_register_2(uint32_t a_bit_mask) {
  uint32_t status = 0;
  spi0_flash_read_status_register_2(&status);
  return ((a_bit_mask & status) == a_bit_mask);
}

// WEL => Write Enable Latch
inline
bool is_WEL(void) {
  bool success = verify_status_register_1(kWELBit);
  // DBG_PRINTF("  %s bit %s set.\n", "WEL", (success) ? "confirmed" : "NOT");
  return success;
}

inline
bool is_WEL_dbg(void) {
  bool success = verify_status_register_1(kWELBit);
  ETS_PRINTF("  %s bit %s set.\n", "WEL", (success) ? "confirmed" : "NOT");
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
  DBG_PRINTF("  %s bit %s set.\n", "S6/QE/WPDis", (success) ? "confirmed" : "NOT");
  return success;
}

// QE => Quad Enable, S9
inline
bool is_QE(void) {
  bool success = verify_status_register_2(kQEBit1B);
  DBG_PRINTF("  %s bit %s set.\n", "QE", (success) ? "confirmed" : "NOT");
  return success;
}

// Note, there are other SPI registers with bits for other aspects of QUAD
// support. I chose these.
// Returns true when SPI0 controler is in either QOUT or QIO mode.
inline
bool is_spi0_quad(void) {
  return (0 != ((SPICQIO | SPICQOUT) & SPI0C));
}

#if SUPPORT_SPI_FLASH__S6_QE_WPDIS
// Only for EN25Q32C, earlier version may not work.
// Maybe add SFDP call to validate EN25Q32C part.
// see RTOS_SDK/components/spi_flash/src/spi_flash.c
// Don't rely on that Espressif sample too closely. The data shown for EN25Q16A
// is not correct. The EN25Q16A and EN25Q16B do no support SFDP.
static bool set_S6_QE_WPDis_bit(bool non_volatile) {
  uint32_t status = 0;
  spi0_flash_read_status_register_1(&status);
  bool is_set = (0 != (status & kWPDISBit));
  DBG_PRINTF("  %s bit %s set.\n", "S6/QE/WPDis", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status |= kWPDISBit;
#else
  status = kWPDISBit;
#endif
  // All changes made to the volatile copies of the Status Register-1.
  DBG_PRINTF("  Setting %svolatile %s bit.\n", (non_volatile) ? "non-" : "", "S6/QE/WPDis");
  spi0_flash_write_status_register_1(status, non_volatile);
  return is_S6_QE_WPDis();
}

[[maybe_unused]]
static bool clear_S6_QE_WPDis_bit(void) {
  uint32_t status = 0;
  spi0_flash_read_status_register_1(&status);
  bool not_set = (0 == (status & kWPDISBit));
  DBG_PRINTF("  %s bit %s set.\n", "S6/QE/WPDis", (not_set) ? "NOT" : "confirmed");
  if (not_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status &= ~kWPDISBit;
#else
  status = 0;
#endif
  // All changes made to the volatile copies of the Status Register-1.
  DBG_PRINTF("  Clearing volatile S6/QE/WPDis bit - 8-bit write.\n");
  spi0_flash_write_status_register_1(status, false);
  return (false == is_S6_QE_WPDis());
}
#endif

static bool set_QE_bit__8_bit_sr2_write(bool non_volatile) {
  uint32_t status2 = 0;
  spi0_flash_read_status_register_2(&status2);
  bool is_set = (0 != (status2 & kQEBit1B));
  DBG_PRINTF("  %s bit %s set.\n", "QE", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status2 |= kQEBit1B;
#else
  status2 = kQEBit1B;
#endif
  DBG_PRINTF("  Setting %svolatile %s bit - %u-bit write.\n", (non_volatile) ? "non-" : "", "QE", 8u);
  spi0_flash_write_status_register_2(status2, non_volatile);
  return is_QE();
}

static bool set_QE_bit__16_bit_sr1_write(bool non_volatile) {
  uint32_t status = 0;
  spi0_flash_read_status_registers_2B(&status);
  bool is_set = (0 != (status & kQEBit2B));
  DBG_PRINTF("  %s bit %s set.\n", "QE", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

#if PRESERVE_EXISTING_STATUS_BITS
  status |= kQEBit2B;
#else
  status = kQEBit2B;
#endif
  DBG_PRINTF("  Setting %svolatile %s bit - %u-bit write.\n", (non_volatile) ? "non-" : "", "QE", 16u);
  spi0_flash_write_status_registers_2B(status, non_volatile);
  return is_QE();
}

static void clear_sr_mask(uint32_t reg_0idx, const uint32_t pattern) {
  uint32_t status = flash_gd25q32c_read_status(reg_0idx);
  if (pattern & status) {
    status &= ~pattern;
    flash_gd25q32c_write_status(reg_0idx, status); // 8-bit status register write
    // This message should not repeat at next boot
    DBG_PRINTF("** One time clear of Status Register-%u bits 0x%02X.\n", reg_0idx + 1, pattern);
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

////////////////////////////////////////////////////////////////////////////////
// To free up the GPIO pins, the SPI Flash device needs to support turning off
// pin functions /WP and /HOLD. This is often controlled through the Quad
// Enable (QE) bit. Depending on the vendor, it is either at S9 or S6 of the
// Status Register.
//
// Non-volatile Status Register values are loaded at powerup. When the volatile
// values are set and no power cycling, they stay set across ESP8266 reboots
// unless some part of the system is changing them.
// Flash that is fully compatible with the ESP8266 QIO bit handling will be
// reset back to DIO by the BootROM.
//
// How does that work. We are using volatile QE. Reboot and the BootROM rewrites
// Status Register QE back to non-volatile QE clear.
//
// returns:
// true  - on success
// false - on failure
bool reclaim_GPIO_9_10() {
  bool success = false;

#if defined(RECLAIM_GPIO_EARLY) && defined(DEBUG_FLASH_QE)
  pinMode(1, SPECIAL);
  uart_buff_switch(0);
#endif
  DBG_PRINTF("\n\n\nRun reclaim_GPIO_9_10()\n");

  //+ uint32_t _id = spi_flash_get_id();
  uint32_t _id = alt_spi_flash_get_id(); // works when SDK has not initialized
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

  /*
    The vendor id is an odd parity value. There are a possible 128 manufactures.
    Additionally there are 11 banks of 128 manufactures. Our extracted vendor
    value is one of 11 possible vendors. We do not have an exact match. I have
    not seen any way to ID the bank.

    A false ID is possible!
  */
  uint32_t vendor = 0xFFu & _id;
  switch (vendor) {

    case SPI_FLASH_VENDOR_GIGADEVICE:
      // I don't have matching hardware.  My read of the GigaDevice datasheet
      // says it should work. Why was the MFG ID is obfuscated.

      // Only supports 8-bit status register writes.
      success = set_QE_bit__8_bit_sr2_write(volatile_bit);

      // For this part, non-volatile could be used w/o concern of write fatgue.
      // Once non-volatile set, no attempts by the BootROM or SDK to change will
      // work. 16-bit Status Register-1 writes will always fail.
      // volatile_bit is safe and faster write time.
      break;

#if SUPPORT_MYSTERY_VENDOR_D8
    // Indicators are this is an obfuscated GigaDevice part.
    case SPI_FLASH_VENDOR_MYSTERY_D8: // 0xD8, Mystery Vendor
      // NONOS SDK does not handle MYSTERY_D8
      // clear Status Register protection bits
      clear_sr1_mask(0x7Cu);  // Clear BP4, BP3, BP2, BP1, and BP0
      clear_sr2_mask(0x40u);  // Clear CMP

      success = set_QE_bit__8_bit_sr2_write(volatile_bit);
      break;
#endif

#if SUPPORT_SPI_FLASH_VENDOR_XMC
    // Special handling for XMC anomaly where driver strength value is lost.
    case SPI_FLASH_VENDOR_XMC: // 0x20
      {
        // Backup Status Register-3
        uint32_t status3 = 0;
        SpiOpResult ok0 = spi0_flash_read_status_register_3(&status3);
        success = set_QE_bit__8_bit_sr2_write(volatile_bit);
        if (SPI_RESULT_OK == ok0) {
          // Copy Driver Strength value from non-volatile to volatile
          ok0 = spi0_flash_write_status_register_3(status3, volatile_bit);
          DBG_PRINTF("  XMC Anomaly: Copy Driver Strength values to volatile status register.\n");
          if (SPI_RESULT_OK != ok0) {
            DBG_PRINTF("** anomaly handling failed.\n");
          }
        }
      }
      break;
#endif

#if SUPPORT_SPI_FLASH__S6_QE_WPDIS
    // These use bit6 as a QE bit or WPDis
    case SPI_FLASH_VENDOR_PMC:        // 0x9D aka ISSI - Does not support volatile
    case SPI_FLASH_VENDOR_MACRONIX:   // 0xC2
      success = set_S6_QE_WPDis_bit(non_volatile_bit);
      break;

    case SPI_FLASH_VENDOR_EON:        // 0x1C
      // NONOS SDK does not handle EON as it does ISSI and Macronix S6 bit.
      // clear Status Register protection bits, preserves WPDis
      clear_sr1_mask(0x3Cu);

      // EON SPI Flash parts have a WPDis S6 bit in status register-1 for
      // disabling /WP (and /HOLD). This is similar to QE/S9 on other vendors,
      // ISSI and Macronix.
      // 0x331Cu - Not supported EN25Q32 no S6 bit.
      // 0x701Cu - EN25QH128A might work
      //
      // Match on Device/MFG ignoreing bit capcacity
      if (0x301Cu == (_id & 0x0FFFFu)) {
        // EN25Q32A, EN25Q32B, EN25Q32C pin 4 NC (DQ3) no /HOLD function
        // tested with EN25Q32C
        success = set_S6_QE_WPDis_bit(volatile_bit);
        // Could refine to EN25Q32C only by using the presents of SFDP support.
      }
      // let all others fail.
      break;
#endif

    default:
      // Assume QE bit at S9

      // Primary choice:
      // 16-bit status register writes is what the ESP8266 BootROM is
      // expecting the flash to support. "Legacy method" is what I often see
      // used to descibe the 16-bit status register-1 writes in newer SPI
      // Flash datasheets. I expect this to work with modules that are
      // compatibile with SPI Flash Mode: "QIO" or "QOUT".
      success = set_QE_bit__16_bit_sr1_write(volatile_bit);
      if (! success) {
        // Fallback for DIO only modules - some will work / some will not. If
        // not working, you will need to study the datasheet for the flash on
        // your module and write a module specific handler.
        success = set_QE_bit__8_bit_sr2_write(volatile_bit);
        if (! success) {
          DBG_PRINTF("** Unable to set volatile QE bit using default handler.\n");
        }
      }
      break;
  }
  spi0_flash_write_disable();
  DBG_PRINTF("%sSPI0 signals '/WP' and '/HOLD' were%s disabled.\n", (success) ? "  " : "** ", (success) ? "" : " NOT");
  return success;
}

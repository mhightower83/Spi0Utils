/*
  Reclaim the use of GPIO9 and GPIO10.

  To free up the GPIO pins, the SPI Flash device needs to support turning off
  pin functions /WP and /HOLD. This is often controlled through the Quad Enable
  (QE) bit. Depending on the vendor, it is either at S9 or S6 of the Status
  Register. Additionally, SRP0 and SRP1 may beed setting to 0.

  Non-volatile Status Register values are loaded at powerup. When the volatile
  values are set and no power cycling, they stay set across ESP8266 reboots
  unless some part of the system is changing them. Flash that is fully
  compatible with the ESP8266 QIO bit handling will be reset back to DIO by the
  BootROM.

  > How does that work? We are using volatile QE. Reboot and the BootROM
  > rewrites Status Register QE back to non-volatile QE clear.
  > Hmm, how does the flash handle switching back and forth setting of
  > volatile/non-volatile? I'll assume a read/modify/write non-volatile is
  > going to incorporate previous volatile bits writen.
  > I assume this is not a problem, after a few boot cycles, the non-volatile
  > bits are not changing. Only the volatile change at the call to
  > reclaim_GPIO_9_10().
  >
  > BootROM Enable_QMode and Disable_QMode sets and clears the QE bit.
  > Enable_QMode sets QE bit and clears all other 16-bits.
  > Diable_QMode clears the upper 8-bits and keeps the lower 8-bits in the Flash
  > status register. This operation is done at each boot. It is best to do
  > modifications at post boot as volatile leaving the non-volatile state
  > unchanged for boot.
  >

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
    setting Status Register QE bit (S9) to 1 and setting SRP0 and SRP1 set to 0
    to disable pin function /WP and /HOLD on the Flash. As well as SRP0 and SRP1
    set to 0. Reconcile this with your SPI Flash datasheet's information.

  * Setting the non-volatile copy of QE=1 may not always work for every flash
    device. The ESP8266 BootROM reads the Flash Mode from the boot image and
    tries to reprogram the non-volatile QE bit. For a Flash Mode of DIO, the
    BootROM will try and set QE=0 with 16-bit Write Status Register-1. Some
    parts don't support this length.

////////////////////////////////////////////////////////////////////////////////
//
SPI Flash Notes and Observed Anomalies:

XMC - SFDP Revision matches up with XM25QH32B datasheet.
 1. Clears status register-3 on volatile write to register-2. Restores on
    power-up. But not on Flash software reset, opcodes 66h-99h. However, the QE
    bit did refresh to the non-volatile value.
 2. Accepts 8-bit write register-2 or 16-bit write register-2.
 3. XM25Q32B and XM25Q32C have different Driver strength tables.
    MFG/Device is not enough to differentiate. Need to use SFDP.

0xD8 (Obfuscated MFG ID?, GigaDevice ID in SFDP)
 1. Only supports 8-bit Status Register writes
 2. The BootROM's 16-bit register-1 writes will fail. This works in our
    favor, no extra wear on the Flash.
 3. The last 64 bits of the 128-bit Unique ID are still in the erased state.
 4. Flash Software Reset, opcodes 66h-99h, clears non-volatile QE bit!!!
 5. Looks a lot like GigaDevice GD25Q32E

GigaDevice
 1. No legacy 16-bit, only 8-bit write status register commands are supported.

Winbond
 1. My new NodeMCU v1.0 board only works with 16-bit write status register-1.
    Appears to be built with very old inventory.

EON
 1. EN25Q32C found on an AI Thinker ESP-12F module marked as DIO near antenna.
 2. Only has 1 Status Register. The BootROM's 16-bit register-1 writes fail.
 3. NC, No /HOLD pin function.
 4. Status Register has WPDis, Bit6, to disable the /WP pin function.
//
////////////////////////////////////////////////////////////////////////////////
*/

// #define DEBUG_FLASH_QE 1 // set in *.ino.globals.h instead
#if 0
// Needed when module is a .cpp
#include <Arduino.h>

// These control informative messages from the library SpiFlashUtils.h
#ifdef DEBUG_FLASH_QE
#define DBG_SFU_PRINTF ets_uart_printf
#else
#define DBG_SFU_PRINTF(...) do {} while (false)
#endif
#endif
#include <SpiFlashUtils.h>

//D #include <BootROM_NONOS.h>
//D #include <kStatusRegisterBitDef.h>
//D #include <FlashChipId_D8.h>
#ifndef ETS_PRINTF
#define ETS_PRINTF ets_uart_printf
#endif

#ifndef SUPPORT_MYSTERY_VENDOR_D8
#define SUPPORT_MYSTERY_VENDOR_D8 1
#endif


////////////////////////////////////////////////////////////////////////////////
// Handle Freeing up GPIO pins 9 and 10 for various Flash memory chips.
//
// returns:
// true  - on success
// false - on failure
//
bool reclaim_GPIO_9_10() {
  bool success = false;

#if defined(RECLAIM_GPIO_EARLY) && defined(DEBUG_FLASH_QE)
  pinMode(1, SPECIAL);
  uart_buff_switch(0);
#endif
  DBG_SFU_PRINTF("\n\n\nRun reclaim_GPIO_9_10()\n");

  //+ uint32_t _id = spi_flash_get_id();
  uint32_t _id = alt_spi_flash_get_id(); // works when SDK has not initialized
  DBG_SFU_PRINTF("  Flash Chip ID: 0x%06X\n", _id);

  if (is_WEL()) {
    // Most likely left over from BootROM's attempt to update the Flash Status Register.
    // Common event for SPI Flash that don't support 16-bit Write Status Register-1.
    // Seen with EON's EN25Q32C, GigaDevice and Mystery Vendor 0xD8. These
    // do not support 16-bit write status register-1.
    DBG_SFU_PRINTF("  Detected: a previous write failed. The WEL bit is still set.\n");
    spi0_flash_write_disable();
  }

  // Expand to read SFDP Parameter Version. Use result to differentiate parts.

  // SPI0 must be in DIO or DOUT mode to continue.
  if (is_spi0_quad()) {
    DBG_SFU_PRINTF("  GPIO pins 9 and 10 are not available when configured for SPI Flash Modes: \"QIO\" or \"QOUT\"\n");
    return false;
  }

  /*
    A false ID is possible! Be aware of possible collisions. The vendor id is an
    odd parity value. There are a possible 128 manufactures. As of this writing,
    there are 11 banks of 128 manufactures. Our extracted vendor value is one of
    11 possible vendors. We do not have an exact match. I have not seen any way
    to ID the bank.
  */
  uint32_t vendor = 0xFFu & _id;
  switch (vendor) {

    case SPI_FLASH_VENDOR_GIGADEVICE:
      // I don't have matching hardware.  My read of the GigaDevice datasheet
      // says it should work.

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
      //D Always zero SR
      //D // NONOS SDK does not handle MYSTERY_D8. Clear Status Register protection
      //D // bits as spi_flash_check_wr_protect would have. This updates the
      //D // non-volatile copy. If alread clear, no changes made.
      //D clear_sr1_mask(0x7Cu);  // Clear BP4, BP3, BP2, BP1, and BP0
      //D clear_sr2_mask(0x40u);  // Clear CMP

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
          DBG_SFU_PRINTF("  XMC Anomaly: Copy Driver Strength values to volatile status register.\n");
          if (SPI_RESULT_OK != ok0) {
            DBG_SFU_PRINTF("** anomaly handling failed.\n");
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
      //D // NONOS SDK does not handle EON as it does ISSI and Macronix S6 bit.
      //D // clear Status Register protection bits, preserves WPDis
      //D clear_sr1_mask(0x3Cu);

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
          DBG_SFU_PRINTF("** Unable to set volatile QE bit using default handler.\n");
        }
      }
      break;
  }
  spi0_flash_write_disable();
  DBG_SFU_PRINTF("%sSPI0 signals '/WP' and '/HOLD' were%s disabled.\n", (success) ? "  " : "** ", (success) ? "" : " NOT");
  // Set GPIOs to Arduino defaults
  if (success) {
    pinMode(9, INPUT);
    pinMode(10, INPUT);
  }
#if defined(RECLAIM_GPIO_EARLY) && defined(DEBUG_FLASH_QE)
  pinMode(1, INPUT);      // restore back to default
#endif
  return success;
}

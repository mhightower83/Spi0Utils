/*
 *   Copyright 2024 M Hightower
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
/*
  Reclaim the use of GPIO9 and GPIO10.

  To free up the GPIO pins, the SPI Flash device needs to support turning off
  pin functions /WP and /HOLD. This is often controlled through the Quad Enable
  (QE) bit; however, some devices require Status Register bits SRP1:SPR0 set to
  0:0 to completely disable /WP. Depending on the vendor, the QE bit is either
  at S9 or S6 of the Flash Status Register.

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
 2. Accepts 8-bit write register-2 or 16-bit write register-1.
 3. XM25Q32B and XM25Q32C have different Driver strength tables.
    MFG/Device is not enough to differentiate. Need to use SFDP.

0xD8 (Obfuscated MFG ID?, GigaDevice ID in SFDP)
 1. Part marking 25Q32ET no logo
 2. Only supports 8-bit Status Register writes
 3. The BootROM's 16-bit register-1 writes will fail. This works in our
    favor, no extra wear on the Flash.
 4. The last 64 bits of the 128-bit Unique ID are still in the erased state.
 5. Flash Software Reset, opcodes 66h-99h, clears non-volatile QE bit!!!
 6. Looks a lot like the GigaDevice GD25Q32E.

GigaDevice
 1. No legacy 16-bit, only 8-bit write status register commands are supported.
 2. GD25B32E doesn't appear to have a /WP or /HOLD pin while GD25Q32C does!
    I have not seen a module with the GD25B32C part; I downloaded the wrong
    datasheet. From the datasheet: "The default value of QE bit is 1 and it
    cannot be changed, so that the IO2 and IO3 pins are enabled all the time."
    If the pins float for non-quad operations, it might work. If so, no special
    code is needed use pinMode to reclaim GPIO pin.
 3. Vendor confusing there is GigaDevice and ELM Technology with similar part
    numbers and same MFG ID. It looks like ELM Technology has GigaDevice NOR
    Flash in there product offering with PDF files rebadged as ELM.

Winbond
 1. Newer versions of the Winbond parts support both 16-bit Status Register-1
    writes and 8-bit SR-1 and SR-2 writes. My new NodeMCU v1.0 board only works
    with 16-bit write status register-1. It appears, very old inventory is still
    out there.

EON
 1. EN25Q32C found on an AI Thinker ESP-12F module marked as DIO near antenna.
 2. Only has 1 Status Register. The BootROM's 16-bit register-1 writes fail.
 3. NC, No /HOLD pin function.
 4. Status Register has WPDis, Bit6, to disable the /WP pin function.


//
////////////////////////////////////////////////////////////////////////////////
*/
#include <Arduino.h>
#include "ModeDIO_ReclaimGPIOs.h"

#if !defined(SPI_FLASH_VENDOR_MYSTERY_D8)
#include "FlashChipId_D8.h"
#endif

/*//////////////////////////////////////////////////////////////////////////////
  To have a larger pool of Flash vendors to test against, I tested with devices
  that did not expose GPIO9 and GPIO10. These did not work well. Let us hope
  that modules makers that expose GPIO9 and GPIO10 made an effort to pair the
  ESP8266EX with a SPI Flash that can disable /WP and /HOLD.
*/
bool __spi_flash_vendor_cases(const uint32_t _id) { // }, const uint32_t _sfdp_ver) {
  using namespace experimental;

  bool success = false;
  /*
  0xCCTTVV = _id = alt_spi_flash_get_id();
     | | |
     | | +--- Vendor   - manufacturer ID
     | +----- Type     - memory type - seems to add some uniqueness
     +------- Capacity - 2**(CC - 1), often correlates to byte capacity

  A false ID is possible! Be aware of possible collisions. The vendor id is an
  odd parity value. There are a possible 128 manufactures. As of this writing,
  there are 14 banks of 128 manufactures. Our extracted vendor value is one of
  14 possible vendors. We do not have an exact match. The only way I have seen
  to ID the Bank is with SFDP; however, it requires "optional parameter data", a
  "Parameter ID" that maps to "Bank Number":"Manufacturer ID". I do not have any
  devices that provide this.

  */
  uint32_t vendor = 0xFFu & _id;
  uint32_t type = (_id >> 8u) & 0xFFu;

  switch (vendor) {
    case SPI_FLASH_VENDOR_BERGMICRO:
      // Status: hardware tested
      // Logo XTX BN25F08
      // SFDP none

      // I have a Sonoff SV with flash part BN25F08 with an XTX logo mark
      // the JEDEC MFG ID matches BergMicro as does the part number.
    case SPI_FLASH_VENDOR_WINBOND_NEX:
      // Winbond 25Q32FVSIG
      // SFDP Revision: 1.00, 1ST Parameter Table Revision: 1.00
      // SFDP Table Ptr: 0x80, Size: 36 Bytes
      if (0x40u == type) {
        // 16-bit status register writes is what the ESP8266 BootROM is
        // expecting the flash to support. "Legacy method" is what I often see
        // used to descibe the 16-bit status register-1 writes in newer SPI
        // Flash datasheets. I expect this to work with modules that are
        // compatibile with SPI Flash Mode: "QIO" or "QOUT".
        success = set_S9_QE_bit__16_bit_sr1_write(volatile_bit);
      }
      break;

    case SPI_FLASH_VENDOR_PUYA:
      // Status: tested
      // Puya P25Q80H
      // SFDP Revision: 1.00, 1ST Parameter Table Revision: 1.00
      // SFDP Table Ptr: 0x30, Size: 36 Bytes
    case SPI_FLASH_VENDOR_ZBIT:
      // Status: tested
      // Zbit 25VQ80AT
      // SFDP Revision: 1.06, 1ST Parameter Table Revision: 1.06
      // SFDP Table Ptr: 0x30, Size: 64 Bytes
      if (0x60u == type) {
        success = set_S9_QE_bit__16_bit_sr1_write(volatile_bit);
      }
      // I have two parts with Zbit vendor ID that are not compatible
      // TODO - recheck datasheet for device values for 4MB part
      break;

    case SPI_FLASH_VENDOR_MYSTERY_D8: // 0xD8
      // "Mystery Vendor" - an obfuscated GigaDevice part?
      // The SFDP table says the MFG is 0xC8
      //
      // Status: tested, sealed in ESP-12F module
      // "Mystery Vendor" 25Q32ET, No logo
      // SFDP Revision: 1.06, 1ST Parameter Table Revision: 1.06
      // SFDP Table Ptr: 0x30, Size: 64 Bytes
    case SPI_FLASH_VENDOR_GIGADEVICE: // 0xC8
      // Based on my read of the GigaDevice datasheet
      // Only supports 8-bit status register writes just like "Mystery Vendor"
      // Status: no hardware testing
      if (0x40u == type) {
        success = set_S9_QE_bit__8_bit_sr2_write(volatile_bit);
      }
      // For this part, non-volatile could be used w/o concern of write fatgue.
      // Once non-volatile set, no attempts by the BootROM or SDK to change will
      // work. 16-bit Status Register-1 writes will always fail.
      // volatile_bit is safe and faster write time.
      break;

    case SPI_FLASH_VENDOR_XMC: // 0x20
      // Special handling for XMC anomaly where driver strength value is lost
      // when switching from non-volatile to volatile.
      // Status: tested, sealed in ESP-12F module
      // SFDP Revision: 1.00, 1ST Parameter Table Revision: 1.00
      // SFDP Table Ptr: 0x30, Size: 36 Bytes
      if (0x40u == type) {
        // Backup Status Register-3
        uint32_t status3 = 0u;
        SpiOpResult ok0 = spi0_flash_read_status_register_3(&status3);
        // success = set_S9_QE_bit__8_bit_sr2_write(volatile_bit);
        success = set_S9_QE_bit__16_bit_sr1_write(volatile_bit);
        if (SPI_RESULT_OK == ok0) {
          uint32_t newSR3 = 0u;
          spi0_flash_read_status_register_3(&newSR3);
          if (status3 != newSR3) {
            // Copy Driver Strength value from non-volatile to volatile
            ok0 = spi0_flash_write_status_register_3(status3, volatile_bit);
            DBG_SFU_PRINTF("  XMC Anomaly: Copy Driver Strength values to volatile status register.\n");
            if (SPI_RESULT_OK != ok0) {
              DBG_SFU_PRINTF("* anomaly handling failed.\n");
            }
          }
        }
      }
      break;

#if 0
    // Not tested, block for now
    // These are two QE/S6 Flash vendors the NONOS_SDK checks for.
    // Tests are based on RTOS_SDK code
    //   https://github.com/espressif/esp-idf/blob/bdb9f972c6a1f1c5ca50b1be2e7211ec7c24e881/components/bootloader_support/bootloader_flash/src/flash_qio_mode.c#L37-L54
    case SPI_FLASH_VENDOR_ISSI_2:     // 0x9D (confusable with PMC) - Does not support volatile
      if (0x40u == (0xCFu & type)) {
        // Status: no hardware testing
        success = set_S6_QE_bit_WPDis(non_volatile_bit);
      }
      break;

    case SPI_FLASH_VENDOR_MACRONIX:   // 0xC2
      if (0x20u == type) {
        // Status: no hardware testing
        success = set_S6_QE_bit_WPDis(non_volatile_bit);
      }
      break;
#endif

    case SPI_FLASH_VENDOR_EON:        // 0x1C
      // Status: tested, sealed in ESP-12F module
      // EON EN25Q32C, identification based on datasheet and data matchup
      // SFDP Revision: 1.00, 1ST Parameter Table Revision: 1.00
      // SFDP Table Ptr: 0x30, Size: 36 Bytes
      if (0x30u == type) {
        // These will match EN25Q32A, EN25Q32B, EN25Q32C pin 4 NC (DQ3) no /HOLD function
        // EON SPI Flash parts have a WPDis S6 bit in status register-1 for
        // disabling /WP (and /HOLD). This is similar to QE/S9 on other vendor parts.
        success = set_S6_QE_bit_WPDis(non_volatile_bit);
      }
      // 0x331Cu - Not supported, EN25Q32 no S6 bit
      // 0x701Cu - EN25QH128A might work
      break;

    default:
      success = false;
      break;
  }

  if (! success) {
    DBG_SFU_PRINTF("* No builtin flash QE bit handler.\n");
  }
  return success;
}

bool spi_flash_vendor_cases(uint32_t _id) __attribute__ ((weak, alias("__spi_flash_vendor_cases")));

////////////////////////////////////////////////////////////////////////////////
// Handle Freeing up GPIO pins 9 and 10 for various Flash memory chips.
//
// returns:
// true  - on success
// false - on failure
//
bool reclaim_GPIO_9_10() {
  using namespace experimental;
  bool success = false;

#if RECLAIM_GPIO_EARLY && DEBUG_FLASH_QE
  pinMode(1, SPECIAL);
  uart_buff_switch(0);
#endif
  DBG_SFU_PRINTF("\n\n\nRun reclaim_GPIO_9_10()\n");

  //+ uint32_t _id = spi_flash_get_id();
  uint32_t _id = alt_spi_flash_get_id(); // works when SDK has not initialized
  DBG_SFU_PRINTF("  Flash Chip ID: 0x%06X\n", _id);

#if DEBUG_FLASH_QE
  if (is_WEL()) {
    // Most likely left over from BootROM's attempt to update the Flash Status Register.
    // Common event for SPI Flash that don't support 16-bit Write Status Register-1.
    // Seen with EON's EN25Q32C, GigaDevice and Mystery Vendor 0xD8. These
    // do not support 16-bit write status register-1.
    DBG_SFU_PRINTF("  Detected: a previous write failed. The WEL bit is still set.\n");
    spi0_flash_write_disable();
  }
#else
  // No requirement for calling spi0_flash_write_disable. The library must guard
  // against WEL bit left on in order to prevent volatile writes from turning
  // into non-volatile.
#endif

  // Expand to read SFDP Parameter Version. Use result to differentiate parts.

  // SPI0 must be in DIO or DOUT mode to continue.
  if (is_spi0_quad()) {
    DBG_SFU_PRINTF("  GPIO pins 9 and 10 are not available when configured for SPI Flash Modes: \"QIO\" or \"QOUT\"\n");
    return false;
  }

  success = spi_flash_vendor_cases(_id);
  spi0_flash_write_disable();
  DBG_SFU_PRINTF("%sSPI0 signals '/WP' and '/HOLD' were%s disabled.\n", (success) ? "  " : "** ", (success) ? "" : " NOT");
  DBG_SFU_PRINTF("%sGPIO9 and GPIO10 are%s available.\n", (success) ? "  " : "** ", (success) ? "" : " NOT");

  // Set GPIOs to Arduino defaults
  if (success) {
    pinMode(9, INPUT);
    pinMode(10, INPUT);
  }
#if RECLAIM_GPIO_EARLY && DEBUG_FLASH_QE
  pinMode(1, INPUT);      // restore back to default
#endif
  return success;
}

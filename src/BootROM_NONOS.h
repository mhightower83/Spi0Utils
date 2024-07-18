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

TODO: move relavent parts to "esp8266_undocumented.h"

 */
//C #include "BootROM_NONOS.h"
// Placing here for now - it will move, this is not where it belongs

#ifndef BOOTROM_NONOS_H
#define BOOTROM_NONOS_H

#include <spi_flash.h>
#include <esp8266_undocumented.h>

#ifdef __cplusplus
extern "C" {
#endif

// BootROM
//D int SPI_write_enable(SpiFlashChip *chip);

/*
  Writes to "status register-1" or "status register-1 and status register-2"
  when
*/
int SPI_write_status(SpiFlashChip *chip, uint32_t status);
/*
  Uses builtin Status Register-1 read.
  Waits for cmd register to clear
  Reads data register and and with status mask provided via 'SpiFlashChip *chip'
  Returns result in status

  Default assumption is the Flash Chip will return a 16-bit value
  I am not sure the builtin 'Status Register-1' read will read more than 8 bits.
  And, if so not all vendors support such an option. The datasheets I have seen
  indicate that the 8-bit value would repeat.
*/
int SPI_read_status(SpiFlashChip *chip, uint32_t *status);

/*
  Wait_SPI_Idle is in esp8266/cores/esp8266/esp8266_undocumented.h
  This function checks for SPI read/idle
  Uses BIT9 in the SPI_READY register to determine SPI ready/idle status
  Once clear, check SPI_read_status result for zero
  return 0 - when Status Register result is zero
  return 1 - if any bits are set in the status register.

  ** BUGBUG - they should have limited the test to the WIP (busy), BIT0, bit.
  Other bits in the status could legitimately be on.
  IMO The Return result should be ignored it is meaningless. **

  uint32_t Wait_SPI_Idle(SpiFlashChip *fc);
*/

/*
To use Boot ROM's Enable_QMode or Disable_QMode functions, the Flash Chip has to
have a 2nd Status register with S9 bit for QE. At boot, for a Flash Mode byte
set to QIO and QOUT, Enable_QMode is called. For all other values, Disable_QMode
is called.

EON's EN25Q32B does not have a 2nd Status Register. IF it is possible for the
ESP8266 to use the EN25Q32B in QIO mode, Enable_QMode or Disable_QMode will not
be a part of that solution.

Enable_QMode
  * Sets QIO or QOUT in SPI CTRL register
  * Sets SPI_WRSR_2B in SPI_CTRL_REG changing Status Write to 16 bits long. This
    is a legacy support option in Winbond parts. ** It may not be present from
    other vendors. In this case, look for WEL left set after a Status Write. **
    Also, a legacy Winbond part would clear status register-2 when you do an
    8-bit write to status register-1.
    Note, EON (EN25Q32B) does not have an SR-2 or QE bit like other Flash Chips.
  * SPI_write_enable followed by
  * SPI_read_status (only 8-bit result), set QE bit
  * SPI_write_status 16-bit.
    ** Any previous bits set in Status Register-2 are lost. **
*/
int Enable_QMode(SpiFlashChip *chip);
int Disable_QMode(SpiFlashChip *chip);

// Disables iCache. Use Cache_Read_Enable_New to turn back on.
void Cache_Read_Disable();

// NONOS_SDK
bool spi_flash_issi_enable_QIO_mode();
bool flash_gd25q32c_enable_QIO_mode();
uint32_t spi_flash_enable_qmode();




enum GD25Q32C_status {  // From RTOS SDK
    GD25Q32C_STATUS1=0,
    GD25Q32C_STATUS2=1,
    GD25Q32C_STATUS3=2,
};

/*
  NONOS SDK function supports Read Status Registers 1, 2, and 3 at one byte per
  call. Uses SPI Flash instructions: 0x05, 0x35, and 0x15 for reading Status
  Registers 1, 2, and 3. reg_0idx is zero-based. eg. use 0 to access Status
  Register-1.

  Also, works with non-GD25Q32C Flash memory having matching SPI Instructions.
  The three Status registers are not aways present on SPI Flash memory. Advise
  using spi_flash_get_id to match use with supporting hardware.

  Returns Flash Status Register value.

  BUGBUG: Has an unnecessary SPI_write_enable call. On return, Status Register-1
  bit WEL set.
*/
extern uint32_t flash_gd25q32c_read_status(uint32_t reg_0idx);

/*
  NONOS SDK function supports Write Status Registers 1, 2, and 3 at one byte per
  call.   Uses SPI Flash instructions: 0x01, 0x31, and 0x11 for Write Status
  Registers 1, 2, and 3. reg_0idx is zero-based, eg use 0 to access Status
  Register-1. Always writes to non-volatile registers.

  Also, works with non-GD25Q32C Flash memory having matching SPI Instructions
  and accepting 8-bit writes. Some devices only accept 16-bit write transfers.
  For those use NONOS SDK's spi_flash_write_status.
  Advise using spi_flash_get_id to match use with supporting hardware.
*/
extern void flash_gd25q32c_write_status(uint32_t reg_0idx, uint32_t status);

/*
  NONOS SDK function: writes status16 to Status Register-1 and 2 - using a
  single 16-bit transfer. Always writes to non-volatile registers.

  Implemented with iCache disable/enable wrapper to call BootROM APIs
  SPI_write_enable and SPI_write_status.
  Returns values: SPI_FLASH_RESULT_OK or SPI_FLASH_RESULT_ERR

  Some SPI Flash devices only support 8-bit Status Register Writes. They will
  ignore the 16-bit transfer. Observe the WEL bit still set after a write.

  This API is not suitable for all SPI Flash memory.
  Advise using spi_flash_get_id to match use with supporting hardware.
*/
extern SpiFlashOpResult spi_flash_write_status(uint32_t status16);

/*
  NONOS SDK function: Reads Status Register-1 - 8-bits only

  Implemented with iCache disable/enable wrapper to call BootROM API
  SPI_read_status. Results stored at *status.
  Returns SPI_FLASH_RESULT_OK on success.
*/
extern SpiFlashOpResult spi_flash_read_status(uint32_t *status);







// Enables iCache.
// Note, Cache_Read_Disable/Cache_Read_Enable_New clears previously cached data.
// Not safe for calling from pre-SDK init
// Needs SDK init data for map parameter.
void Cache_Read_Enable_New();

// Places iCache in standby
void Cache_Read_Disable_2();

// Resume iCache operation. Previously cached data is available
void Cache_Read_Enable_2();

/*
  The NONOS SDK has a weak link to a default handler.
  Default handler has support for ISSI, Macronix, and GigaDevice.
  With out the default handler these SPI Flash memories cannot operate in QIO
  mode. It also requires a blank.bin bit to be set to run.

  The Arduino ESP8266 Core has an empty function to replace the default in
  order to save some IRAM.
*/
void user_spi_flash_dio_to_qio_pre_init(void);

#ifdef __cplusplus
};
#endif

#endif // BOOTROM_NONOS_H

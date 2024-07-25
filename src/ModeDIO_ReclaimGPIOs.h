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
#ifndef EXPERIMENTAL_MODE_DIO_RECLAIM_GPIOS_H
#define EXPERIMENTAL_MODE_DIO_RECLAIM_GPIOS_H

#if !defined(FLASHMODE_DIO) && !defined(FLASHMODE_DOUT)
#error Build with either 'Flash Mode: "DIO"' or 'Flash Mode: "DOUT"'
#endif

#include "SpiFlashUtilsQE.h"

#ifdef __cplusplus
extern "C" {
#endif

bool reclaim_GPIO_9_10();
bool spi_flash_vendor_cases(uint32_t _id);    // weak - replacement with custom
bool __spi_flash_vendor_cases(uint32_t _id);

#ifdef __cplusplus
}
#endif

// missing from spi_vendors.h
#ifndef SPI_FLASH_VENDOR_BERGMICRO
#define SPI_FLASH_VENDOR_BERGMICRO 0xE0u
#endif
#ifndef SPI_FLASH_VENDOR_ZBIT
#define SPI_FLASH_VENDOR_ZBIT      0x5Eu
#endif
#ifndef SPI_FLASH_VENDOR_ISSI_2
#define SPI_FLASH_VENDOR_ISSI_2    0x9Du
#endif

#endif // EXPERIMENTAL_MODE_DIO_RECLAIM_GPIOS_H

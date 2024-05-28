
#include "FlashChipId_D8.h"

////////////////////////////////////////////////////////////////////////////////
// Print popular Flash Chip IDs
#ifndef SPI_FLASH_VENDOR_BERGMICRO
// missing from spi_vendors.h
#define SPI_FLASH_VENDOR_BERGMICRO 0xE0u
#define SPI_FLASH_VENDOR_ZBIT      0x5Eu
#endif

struct FlashList {
  uint32_t id;
  const char *vendor;
} flashList[] = {
{SPI_FLASH_VENDOR_ALLIANCE,     /* 0x52 */    "Alliance Semiconductor"},
{SPI_FLASH_VENDOR_AMD,          /* 0x01 */    "AMD"},
{SPI_FLASH_VENDOR_AMIC,         /* 0x37 */    "AMIC"},
{SPI_FLASH_VENDOR_ATMEL,        /* 0x1F */    "Atmel (now used by Adesto)"},
{SPI_FLASH_VENDOR_BERGMICRO,    /* 0xE0 */    "BergMicro"},
{SPI_FLASH_VENDOR_BRIGHT,       /* 0xAD */    "Bright Microelectronics"},
{SPI_FLASH_VENDOR_CATALYST,     /* 0x31 */    "Catalyst"},
{SPI_FLASH_VENDOR_EON,          /* 0x1C */    "EON Silicon Devices, missing 0x7F prefix"},
{SPI_FLASH_VENDOR_ESMT,         /* 0x8C */    "Elite Semiconductor Memory Technology (ESMT) / EFST Elite Flash Storage"},
{SPI_FLASH_VENDOR_EXCEL,        /* 0x4A */    "ESI, missing 0x7F prefix"},
{SPI_FLASH_VENDOR_FIDELIX,      /* 0xF8 */    "Fidelix"},
{SPI_FLASH_VENDOR_FUJITSU,      /* 0x04 */    "Fujitsu"},
{SPI_FLASH_VENDOR_GIGADEVICE,   /* 0xC8 */    "GigaDevice"},
{SPI_FLASH_VENDOR_HYUNDAI,      /* 0xAD */    "Hyundai"},
{SPI_FLASH_VENDOR_INTEL,        /* 0x89 */    "Intel"},
{SPI_FLASH_VENDOR_ISSI,         /* 0xD5 */    "ISSI Integrated Silicon Solutions, see also PMC."}, // Bank1
{SPI_FLASH_VENDOR_MACRONIX,     /* 0xC2 */    "Macronix (MX)"},
{SPI_FLASH_VENDOR_NANTRONICS,   /* 0xD5 */    "Nantronics, missing prefix"},
{SPI_FLASH_VENDOR_PMC,          /* 0x9D */    "PMC, missing 0x7F prefix"},
// {SPI_FLASH_VENDOR_ISSI_2,       /* 0x9D */    "Integrated Silicon Solution (ISSI)"}, // Bank2
{SPI_FLASH_VENDOR_PUYA,         /* 0x85 */    "Puya semiconductor (shanghai) co. ltd"},
{SPI_FLASH_VENDOR_SANYO,        /* 0x62 */    "Sanyo"},
{SPI_FLASH_VENDOR_SHARP,        /* 0xB0 */    "Sharp"},
{SPI_FLASH_VENDOR_SPANSION,     /* 0x01 */    "Spansion, same ID as AMD"},
{SPI_FLASH_VENDOR_SST,          /* 0xBF */    "SST"},
{SPI_FLASH_VENDOR_XMC,          /* 0x20 */    "Wuhan Xinxin Semiconductor Manufacturing Corp, XMC"},
{SPI_FLASH_VENDOR_ST,           /* 0x20 */    "ST / SGS/Thomson / Numonyx (later acquired by Micron), XMC"},
{SPI_FLASH_VENDOR_SYNCMOS_MVC,  /* 0x40 */    "SyncMOS (SM) and Mosel Vitelic Corporation (MVC)"},
{SPI_FLASH_VENDOR_TENX,         /* 0x5E */    "Tenx Technologies"},
{SPI_FLASH_VENDOR_ZBIT,         /* 0x5E */    "Zbit Semiconductor, Inc. (ZB25VQ)"},
{SPI_FLASH_VENDOR_TI,           /* 0x97 */    "Texas Instruments"},
{SPI_FLASH_VENDOR_TI_OLD,       /* 0x01 */    "TI chips from last century"},
{SPI_FLASH_VENDOR_WINBOND,      /* 0xDA */    "Winbond"},
{SPI_FLASH_VENDOR_WINBOND_NEX,  /* 0xEF */    "Winbond (ex Nexcom) serial flashes"},
{SPI_FLASH_VENDOR_MYSTERY_D8,   /* 0xD8 */    "Mystery Vendor ID 0xD8 - value has Parity Error"},
{SPI_FLASH_VENDOR_UNKNOWN,      /* 0xFF */    "unknown"}
};

void printFlashChipID(Print& sio) {
 // These lookups matchup to vendors commonly thought to exist on ESP8266 Modules.
 // Unfortunatlly the information returned from spi_flash_get_id() does not
 // indicate a unique vendor. There can only be 128 vendors per bank. And, there
 // are arround 11 banks at this writing.  Also, I have not seen any devices
 // that impliment the repeating 0x7F to indicate bank number.
 uint32_t deviceId = spi_flash_get_id();
 const char *vendor = "unknown";
 for (size_t i = 0; SPI_FLASH_VENDOR_UNKNOWN != flashList[i].id; i++) {
   if (flashList[i].id == (deviceId & 0xFFu)) {
     vendor = flashList[i].vendor;
     sio.printf("  %-12s 0x%06x, '%s'\r\n", "Device ID:", deviceId, vendor);
   }
 }
}
//
////////////////////////////////////////////////////////////////////////////////

#ifndef MODE_DIO_RECLAIM_GPIOS_H
#define MODE_DIO_RECLAIM_GPIOS_H

// Default to exclude these from the build
#if ((1 - RECLAIM_GPIO_EARLY - 1) == 2)
#undef RECLAIM_GPIO_EARLY
#define RECLAIM_GPIO_EARLY 0
#endif
#if ((1 - DEBUG_FLASH_QE - 1) == 2)
#undef DEBUG_FLASH_QE
#define DEBUG_FLASH_QE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

bool reclaim_GPIO_9_10();
bool spi_flash_vendor_cases(uint32_t _id);
bool __spi_flash_vendor_cases(uint32_t _id);

#ifdef __cplusplus
}
#endif

//
// Default to include these in the build
//
#if ((1 - BUILTIN_SUPPORT_GIGADEVICE - 1) == 2)
#undef BUILTIN_SUPPORT_GIGADEVICE
#define BUILTIN_SUPPORT_GIGADEVICE 1
#endif

#if ((1 - BUILTIN_SUPPORT_MYSTERY_VENDOR_D8 - 1) == 2)
#undef BUILTIN_SUPPORT_MYSTERY_VENDOR_D8
#define BUILTIN_SUPPORT_MYSTERY_VENDOR_D8 1
#endif

#if ((1 - BUILTIN_SUPPORT_SPI_FLASH__S6_QE_WPDIS - 1) == 2)
#undef BUILTIN_SUPPORT_SPI_FLASH__S6_QE_WPDIS
#define BUILTIN_SUPPORT_SPI_FLASH__S6_QE_WPDIS 1
#endif

#if ((1 - BUILTIN_SUPPORT_SPI_FLASH_VENDOR_EON - 1) == 2)
#undef BUILTIN_SUPPORT_SPI_FLASH_VENDOR_EON
#define BUILTIN_SUPPORT_SPI_FLASH_VENDOR_EON 1
#endif


#endif // MODE_DIO_RECLAIM_GPIOS_H

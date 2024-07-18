/*
Probe the Flash for SFDP data

Standalone version - It doesn't require the addition of SpiFlashUtils library

An example using `experimental::SPI0Command` function to read SPI Flash Data
Parameters from flash and print a hex dump.

To understand the dump, you will need access to JEDEC documentation. In some
cases, the SPI Flash vendor's datasheet will list an explanation of their SFDP
data.

The "SFDP" may be useful for differentiating between some parts.

  Reference:
    JEDEC STANDARD Serial Flash Discoverable Parameters
    https://www.jedec.org/standards-documents/docs/jesd216b
    Free Download - requires registration

  This example code is in the public domain.
*/
#include <Arduino.h>
#include <spi_flash.h>
#include <spi_utils.h>
using namespace experimental;

constexpr uint8_t kReadSFDP = 0x5Au;

inline void spi_set_addr(uint8_t *buf, const uint32_t addr) {
  buf[0] = addr >> 16u;
  buf[1] = (addr >> 8u) & 0xFFu;
  buf[2] = addr & 0xFFu;
}

static SpiOpResult spi0_flash_read_sfdp(uint32_t offset, uint32_t *pData, size_t sz) {
  if (sz > 64u || (sz % sizeof(uint32_t)) || 0u == sz) {
    return SPI_RESULT_ERR;
  }

  // 24 bit address reads with one dummpy byte.
  struct FlashAddr24 {
    union {
      uint8_t u8[4];
      uint32_t u32;
    };
  } addr24bit;

  // MSB goes on wire first. It comes out of the LSB of a 32-bit word.
  spi_set_addr(addr24bit.u8, offset);
  addr24bit.u8[3] = 0u; // SFDP Read has dummy byte after address.
  pData[0] = addr24bit.u32;

  // SPI0Command sends the SFDP Read opcode, 5Ah with 32 bits of data containing
  // the  24 bit address followed by a dummy byte of zeros. The responce is
  // copied back into pData starting at pData[0].
  return SPI0Command(kReadSFDP, pData, 32u, sz * 8u);
}

constexpr uint32_t kSfdpSignature = 0x50444653u; //'SFDP'
//
// Returns true   if 'SFDP' signature was found.
// Returns false  when no signature is found.
//
// If an error occurs while reading SFDP data, after reading a good signature,
// true is still returned.
//
bool dumpSfdp() {
  Serial.printf("Raw dump of SFDP");
  uint32_t param[4];
  SpiOpResult ok0 = spi0_flash_read_sfdp(0, param, sizeof(param));

  if (SPI_RESULT_OK == ok0 && kSfdpSignature == param[0]) {
    for (size_t j = 0u; ; ) {
      Serial.printf("\n  0x%02X  ", 0x10u * j);
      for (size_t i = 0u; i < 4u; i++) {
        Serial.printf(" 0x%08X", param[i]);
      }
      j++;
      if (16 <= j) break;

      if (SPI_RESULT_OK != spi0_flash_read_sfdp((j * 16u), param, sizeof(param))) {
        Serial.printf("\n  error reading SFDP.");
        break;
      }
    }
  } else {
    Serial.printf("\n  SFDP not supported.");
    ok0 = SPI_RESULT_ERR;
  }

  Serial.printf("\n");
  return (SPI_RESULT_OK == ok0);
}

void setup() {
  Serial.begin(115200u);
  delay(200u);
  Serial.printf("\r\n\n\n");
  dumpSfdp();
}

void loop() {

}

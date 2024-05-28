/*
  Assumes the Flash has compatible QE support

  For ESP8266EX with QE compatible Flash memory this example should work.
  Others, will require more effort. However, this starts us off with a simple
  example.

  Compatible Flash memory would be like BergMicro, Winbond, XMC, or others that
  have the QE bit at S9 in the Flash Status Register and support 16-bit Write
  Status Register-1.
*/
#include <ESP8266WiFi.h>
#include <SpiFlashUtils.h>

#if defined(DEBUG_FLASH_QE)
#define DBG_PRINTF(a, ...) Serial.printf_P(PSTR(a), ##__VA_ARGS__)
#else
#define DBG_PRINTF(...) do {} while (false)
#endif

////////////////////////////////////////////////////////////////////////////////
// Some QE utilities
constexpr uint32_t kQEBit1B  = BIT1;  // Enable QE=1, disables WP# and HOLD#
constexpr uint32_t kQEBit2B  = BIT9;  // Enable QE=1, disables WP# and HOLD#

inline bool verify_status_register_2(uint32_t a_bit_mask) {
  uint32_t status = 0;
  SpiOpResult ok0 = spi0_flash_read_status_register_2(&status);
  if (SPI_RESULT_OK == ok0) {
    return ((a_bit_mask & status) == a_bit_mask);
  }
  return false;
}

// QE => Quad Enable, S9
inline bool is_QE(void) {
  bool success = verify_status_register_2(kQEBit1B);
  DBG_PRINTF("  %s bit %s set.\n", "QE", (success) ? "confirmed" : "NOT");
  return success;
}

inline bool is_spi0_quad(void) {
  return (0 != ((SPICQIO | SPICQOUT) & SPI0C));
}

static bool set_QE_bit__16_bit_sr1_write(bool non_volatile) {
  uint32_t status = 0;
  spi0_flash_read_status_registers_2B(&status);
  bool is_set = (0 != (status & kQEBit2B));
  DBG_PRINTF("  %s bit %s set.\n", "QE", (is_set) ? "confirmed" : "NOT");
  if (is_set) return true;

  // The argument for not preseving existing bits "we don't know what we don't know"
  // I am leaning toward preserving; however, early setup code tends not to.
#if PRESERVE_EXISTING_STATUS_BITS
  status |= kQEBit2B;
#else
  status = kQEBit2B;
#endif
  DBG_PRINTF("  Setting %svolatile %s bit - %u-bit write.\n", (non_volatile) ? "non-" : "", "QE", 16u);
  spi0_flash_write_status_registers_2B(status, non_volatile);
  return is_QE();
}
////////////////////////////////////////////////////////////////////////////////
// Print popular Flash Chip IDs
#ifndef SPI_FLASH_VENDOR_BERGMICRO
// missing from spi_vendors.h
#define SPI_FLASH_VENDOR_BERGMICRO  0xE0u
#define SPI_FLASH_VENDOR_ZBIT       0x5Eu
#define SPI_FLASH_VENDOR_MYSTERY_D8 0xD8u
#endif

struct FlashList {
  uint32_t id;
  const char *vendor;
} flashList[] = {
{SPI_FLASH_VENDOR_BERGMICRO,    /* 0xE0 */    "BergMicro"},
{SPI_FLASH_VENDOR_ZBIT,         /* 0x5E */    "Zbit Semiconductor, Inc. (ZB25VQ)"},
{SPI_FLASH_VENDOR_PMC,          /* 0x9D */    "PMC, missing 0x7F prefix"},
{SPI_FLASH_VENDOR_GIGADEVICE,   /* 0xC8 */    "GigaDevice"},
{SPI_FLASH_VENDOR_XMC,          /* 0x20 */    "Wuhan Xinxin Semiconductor Manufacturing Corp, XMC"},
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
     break;
   }
 }
 sio.printf("  %-12s 0x%06x, '%s'\r\n", "Device ID:", deviceId, vendor);
}
////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.printf(
    "\r\n\r\n"
    "Demo: Test reclaim GPIO pins 9 and 10 logic with QIO compliant target module.\r\n"
    "  Compatible Flash: BergMicro, Winbond, XMC, or ZBit"
  );

  printFlashChipID(Serial);

  if (is_spi0_quad()) {
    Serial.printf("  Demo cannot run. SPI0 Controler is configured for a QUAD mode\n");
    Serial.printf("  Reflash with build set for: SPI Flash Mode: \"DIO\"\n");
    return;
  }

  bool success = set_QE_bit__16_bit_sr1_write(volatile_bit);
  Serial.printf("  %s - Set Quad Enable bit with 16-bit Status Register-1 Write\n", (success) ? "Success" : "Failed");
  if (! success) Serial.printf("  Check Flash Chip compatibility\n");

  // This shows that after a Software Reset that the QE bit is cleared when the
  // BootROM runs again. BootROM Status Register Writes are always non-volatile.
  // Then, we turn volatile copy back on. The question is does this create wear
  // on the Flash? Since we didn't change to value of the non-volatile copy I
  // assume not. I would hope that writing a matching value back to non-volatile
  // space is a no-op.
}

void testGpio9Gpio10() {
  ETS_PRINTF("  Test: QE=%c, GPIO pins 9 and 10 as INPUT\n", is_QE() ? '1' : '0');
  pinMode(9, INPUT);
  pinMode(10, INPUT);
  ETS_PRINTF("  digitalRead result: GPIO_9(%u) and GPIO_10(%u)\n", digitalRead(9), digitalRead(10));
  delay(10);

  ETS_PRINTF("  Test: QE=%c, GPIO pins 9 and 10 as OUTPUT set LOW\n", is_QE() ? '1' : '0');
  pinMode(9, OUTPUT);       // was /HOLD
  pinMode(10, OUTPUT);      // was /WP
  digitalWrite(9, LOW);     // Failure when part is put on hold and WDT Reset occurs or an exception
  digitalWrite(10, LOW);
  // TODO: add write function to confirm digitalWrite(10, LOW) works OK
  delay(10);
  ETS_PRINTF("  Passed - have not crashed\n"); // No WDT Reset - then it passed.
}

void cmdLoop(Print& oStream, int key) {
  switch (key) {
    //
    case '3':
      {
        oStream.printf("\nRead Flash Status Registers: 1, 2, and 3\n")
        uint32_t status = 0;
        SpiOpResult ok0 = spi0_flash_read_status_registers_3B(&status);
        if (SPI_RESULT_OK == ok0) {
          oStream.printf("  spi0_flash_read_status_registers_3B(0x%06X)\n", status);
          if (kQEBit2B == (status & kQEBit2B)) ETS_PRINTF("  QE=1\n");
          if (kWELBit  == (status & kWELBit))  ETS_PRINTF("  WEL=1\n");
        } else {
          oStream.printf("  spi0_flash_read_status_registers_3B() failed!\n");
        }
      }
      break;
    //
    case 't':
      oStream.println(F("Run simple GPIO9 and GPIO10 test ..."));
      testGpio9Gpio10();
      break;
    //
    case 'R':
      oStream.println(F("Restart ..."));
      oStream.flush();
      ESP.restart();
      break;
    //
    case '?':
      oStream.println(F("\r\nHot key help:"));
      oStream.println(F("  3 - SPI Flash Read Status Registers, 3 Bytes"));
      oStream.println(F("  t - Run GPIO9 and GPIO10 test ..."));
      oStream.println(F("  R - Restart"));
      oStream.println(F("  ? - This help message\r\n"));
      break;
    //
    default:
      break;
  }
}

void loop() {
  if (0 < Serial.available()) {
    char hotKey = Serial.read();
    cmdLoop(Serial, hotKey);
  }
}

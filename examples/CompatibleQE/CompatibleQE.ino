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
#if ((1 - DEBUG_FLASH_QE - 1) == 2)
#undef DEBUG_FLASH_QE
#define DEBUG_FLASH_QE 0
#endif

#define PRINTF(a, ...)        printf_P(PSTR(a), ##__VA_ARGS__)
#define PRINTF_LN(a, ...)     printf_P(PSTR(a "\r\n"), ##__VA_ARGS__)

// These control informative messages from the library SpiFlashUtils
#if DEBUG_FLASH_QE
#define DBG_SFU_PRINTF(a, ...) Serial.PRINTF(a, ##__VA_ARGS__)
#else
#define DBG_SFU_PRINTF(...) do {} while (false)
#endif
#include <SpiFlashUtils.h>


////////////////////////////////////////////////////////////////////////////////
// Print popular Flash Chip IDs

// missing from spi_vendors.h
#ifndef SPI_FLASH_VENDOR_BERGMICRO
#define SPI_FLASH_VENDOR_BERGMICRO  0xE0u
#endif
#ifndef SPI_FLASH_VENDOR_ZBIT
#define SPI_FLASH_VENDOR_ZBIT       0x5Eu
#endif
#ifndef SPI_FLASH_VENDOR_MYSTERY_D8
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

void printFlashChipID() {
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
 Serial.PRINTF_LN("  %-12s 0x%06x, '%s'", "Device ID:", deviceId, vendor);
}
////////////////////////////////////////////////////////////////////////////////

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.PRINTF_LN(
    "\r\n\r\n"
    "Demo: Test reclaim GPIO pins 9 and 10 logic with QIO compliant target module.\r\n"
    "  Compatible Flash: BergMicro, Winbond, XMC, or ZBit"
  );

  printFlashChipID();

  if (is_spi0_quad()) {
    Serial.PRINTF_LN("  Demo cannot run. SPI0 Controler is configured for a QUAD mode");
    Serial.PRINTF_LN("  Reflash with build set for: SPI Flash Mode: \"DIO\"");
    return;
  }

  bool success = set_S9_QE_bit__16_bit_sr1_write(volatile_bit);
  Serial.PRINTF_LN("  %s - Set Quad Enable bit with 16-bit Status Register-1 Write", (success) ? "Success" : "Failed");
  if (! success) Serial.PRINTF_LN("  Unexpected results, Check Flash Chip compatibility");

  // This shows that after a Software Reset that the QE bit is cleared when the
  // BootROM runs again. BootROM Status Register Writes are always non-volatile.
  // Then, we turn volatile copy back on. The question is does this create wear
  // on the Flash? Since we didn't change the value of the non-volatile copy I
  // assume not. I would hope that writing a matching value back to non-volatile
  // space is a no-op.
}

void testGpio9Gpio10() {
  Serial.PRINTF_LN("  Test: QE=%c, GPIO pins 9 and 10 as INPUT", is_QE() ? '1' : '0');
  /*
    For this test you want to connect two pull down resistors to pins 9 and 10
    and observe the result when this test is run. If the Sketch crashes and
    reboots  GPIO9 and GPIO10 have not been properly freed.
    `set_S9_QE_bit__16_bit_sr1_write()` in `setup()` may not be correct for your
    Flash memory or you forgot to compile with SPI Mode: DIO.
  */
  pinMode(9, INPUT);
  pinMode(10, INPUT);
  Serial.PRINTF_LN("  digitalRead result: GPIO_9(%u) and GPIO_10(%u)", digitalRead(9), digitalRead(10));
  delay(10);

  Serial.PRINTF_LN("  Test: QE=%c, GPIO pins 9 and 10 as OUTPUT set LOW", is_QE() ? '1' : '0');
  pinMode(9, OUTPUT);       // was /HOLD
  pinMode(10, OUTPUT);      // was /WP
  digitalWrite(9, LOW);     // Failure when part is put on hold and WDT Reset occurs or an exception
  digitalWrite(10, LOW);
  // TODO: add write function to confirm digitalWrite(10, LOW) works OK
  delay(10);
  Serial.PRINTF_LN("  Passed - have not crashed"); // No WDT Reset - then it passed.
}

void cmdLoop(int key) {
  switch (key) {
    //
    case '3':
      {
        Serial.PRINTF_LN("\nRead Flash Status Registers: 1, 2, and 3");
        uint32_t status = 0;
        SpiOpResult ok0 = spi0_flash_read_status_registers_3B(&status);
        if (SPI_RESULT_OK == ok0) {
          Serial.PRINTF_LN("  spi0_flash_read_status_registers_3B(0x%06X)", status);
          if (kQES9Bit2B == (status & kQES9Bit2B)) Serial.PRINTF_LN("  QE=1");
          if (kWELBit  == (status & kWELBit))  Serial.PRINTF_LN("  WEL=1");
        } else {
          Serial.PRINTF_LN("  spi0_flash_read_status_registers_3B() failed!");
        }
      }
      break;
    //
    case 't':
      Serial.PRINTF_LN("Run simple GPIO9 and GPIO10 test ...");
      testGpio9Gpio10();
      break;
    //
    case 'R':
      Serial.PRINTF_LN("Restart ...");
      Serial.flush();
      ESP.restart();
      break;
    //
    case '?':
      Serial.PRINTF_LN("\r\nHot key help:");
      Serial.PRINTF_LN("  3 - SPI Flash Read Status Registers, 3 Bytes");
      Serial.PRINTF_LN("  t - Run GPIO9 and GPIO10 test ...");
      Serial.PRINTF_LN("  R - Restart");
      Serial.PRINTF_LN("  ? - This help message");
      break;
    //
    default:
      break;
  }
}

void loop() {
  if (0 < Serial.available()) {
    char hotKey = Serial.read();
    cmdLoop(hotKey);
  }
}

/*
  Goals
  We need to confirm the Flash memory is supported by our software.

  Evaluate if the current library supports the Flash for reclaiming
  GPIO9 and GPIO10.

  Write once test everywhere.


  Inspect MFG ID is it a known S6 QE flash

  case 'a': analyzeSR_QE() goes a long ways toward getting started.

  continue on with case 'h' and case 'w'

*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ModeDIO_ReclaimGPIOs.h>
#include <TestFlashQE/FlashChipId.h>
#include <TestFlashQE/SFDP.h>
#include <TestFlashQE/WP_HOLD_Test.h>

#define NOINLINE __attribute__((noinline))
#define ETS_PRINTF ets_uart_printf
#define PRINTF(a, ...)        printf_P(PSTR(a), ##__VA_ARGS__)
#define PRINTF_LN(a, ...)     printf_P(PSTR(a "\r\n"), ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////
// Debug MACROS
// These control informative messages from the library SpiFlashUtilsQE
// Also, used in utility_reclaim_gpio_9_10.ino
#if defined(RECLAIM_GPIO_EARLY) && defined(DEBUG_FLASH_QE)
// Use lower level print functions when printing before "C++" runtime has initialized.
#define DBG_SFU_PRINTF(a, ...) ets_uart_printf(a, ##__VA_ARGS__)
#elif defined(DEBUG_FLASH_QE)
#define DBG_SFU_PRINTF(a, ...) Serial.PRINTF(a, ##__VA_ARGS__)
#else
#define DBG_SFU_PRINTF(...) do {} while (false)
#endif
#include <SpiFlashUtilsQE.h>

////////////////////////////////////////////////////////////////////////////////
// Flash Discoveries
struct FlashDiscovery {
  uint32_t device = 0;
  bool S9 = false;
  bool S6 = false;
  bool has_8b_sr2 = false;
  bool has_16b_sr1 = false;      // Is valid only when either S6 or S9 is true
  bool has_volatile = false;
  bool write_QE = false;            // No writeable QE bit
} fd_state;

bool gpio_9_10_available = false;

int last_key __attribute__((section(".noinit")));

void suggestedReclaimFn() {
  // Check if device is already handled by __spi_flash_vendor_cases(device);
  bool builtin = __spi_flash_vendor_cases(fd_state.device & 0xFFFFFFu);

  if ((fd_state.S9 || fd_state.S6) && (fd_state.has_16b_sr1 || fd_state.has_8b_sr2)) {
    if (builtin) {
      Serial.PRINTF("\n"
        "No additional code is needed to support this Flash device; however,\n"
        "the pseudo code would look like this:\n");
    }

    Serial.PRINTF("\n"
      "//\n"
      "// Pseudo code:\n"
      "//\n"
      "#include <SpiFlashUtilsQE.h>\n"
      "\n"
      "extern \"C\" \n"
      "bool spi_flash_vendor_cases(uint32_t device) {\n"
      "  using namespace experimental;\n"
      "  bool success = false;\n"
      "\n"
      "  if (0x%04X == (device & 0xFFFFu)) {\n", fd_state.device & 0xFFFFu);
    if (fd_state.S9) {
      if (fd_state.has_16b_sr1) {
        Serial.PRINTF_LN("    success = set_S9_QE_bit__16_bit_sr1_write(%svolatile_bit);", (fd_state.has_volatile) ? "" : "non_");
      } else
      if (fd_state.has_8b_sr2) {
        Serial.PRINTF_LN("    success = set_S9_QE_bit__8_bit_sr2_write(%svolatile_bit);", (fd_state.has_volatile) ? "" : "non_");
      }
    } else
    if (fd_state.S6) {
      Serial.PRINTF_LN("    success = set_S6_QE_bit_WPDis(%svolatile_bit);", (fd_state.has_volatile) ? "" : "non_");
    }
    Serial.PRINTF(
      "  }\n"
      "\n"
      "  if (! success) {\n"
      "    // Check builin support\n"
      "    success = __spi_flash_vendor_cases(device);\n"
      "  }\n"
      "  return success;\n"
      "}\n");
  } else {
    if (builtin) {
      Serial.PRINTF("\n"
        "Caution: inconsistency detected. Analyze failed to find a configuration for the\n"
        "Flash, yet the manufacturer ID matches the one we support. There are 11 banks of\n"
        "128 manufacturers. It may be the result of an ID collision.\n");
    } else {
      Serial.PRINTF_LN("No apparent solution");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.PRINTF_LN(
    "\r\n\r\n"
    "Analyze Flash for reclaiming GPIO9 and GPIO10 support.");
  printFlashChipID();

  int next_key = '?';
  switch (ESP.getResetInfoPtr()->reason) {
    case REASON_WDT_RST:
    case REASON_SOFT_WDT_RST:
    case REASON_EXCEPTION_RST:
      // Last test crashed
      // preserve last_key for reporting
      break;

    default:
      next_key = 'a'; // Start Analyze
      last_key = '?';
      break;
  }
  switch (last_key) {
    case 'w':   // /WP test GPIO9
      Serial.PRINTF_LN("unexpected crash while testing `/WP`");
      break;

    case 'h':   // /HOLD test GPIO10
      Serial.PRINTF_LN("unexpected but anticipated crash while testing `/HOLD`");
      break;

    case 'a':   // Analyze
      Serial.PRINTF_LN("unexpected crash while running analyze");
      break;

    default:
      break;
  }

  /*
    Run a series of test
    a) Analyze - Find best guess selections S9, S6, 8-bit or 16-bit writes, etc.
      * A failure is NOT expected to cause crash/reboot
    w) Use proposed settings from analyze to test /WP
      * A failure is NOT expected to cause crash/reboot
    h) Use proposed settings from analyze to test /HOLD
      * expect a failure to cause a crash or reboot.
  */
  if ('a' == next_key) {
    if (processKey('a')) {
      // To be confident of the QE bit location S9 or S6 we need to fail and
      // succeed with /WP, enable and disable write protect with QE.

      // At this point, we have a guess for QE bit either S9 or S6
      processKey('M'); // not sure i need this

      if (processKey('w')) {
        // Test /HOLD while QE=1 - this is a final confirmation that we have free-ed GPIO9 and GPIO10
        // use settings suggested by 'w'
        if (processKey('h')) {
          suggestedReclaimFn();
        } else {
          Serial.PRINTF_LN("\nUnable to disable pin function /HOLD using QE/S%u", (fd_state.S9) ? 9 : 6);
        }
      } else {
        Serial.PRINTF_LN("\nUnable to disable pin function /WP using QE/S%u", (fd_state.S9) ? 9 : 6);
      }
    } else {
      Serial.PRINTF_LN("\nUnable to configure Flash Status Register to free GPIO9 and GPIO10");
    }
    last_key = '?';
  } else {
    // Serial.PRINTF_LN("Unable to configure Flash Status Register to free GPIO9 and GPIO10");
    processKey('?');
  }
}

void loop() {
  serialClientLoop();
}

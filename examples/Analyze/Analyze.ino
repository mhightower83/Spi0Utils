/*
  A SPI Flash test/analyzer that generates example/sample code for function
  `spi_flash_vendor_cases` using the results from the tests.

  Determine if the SPI Flash has support for disabling pin functions /WP
  write protect and /HOLD. Or confirm that those functions are not active.
  With these functions disabled, GPIO09 and GPIO10 can be reclaimed for general
  sketch use.

  At boot "Analyze" will attempt to discover the properties of your SPI Flash.
  It will generate a progress report as it goes. If your device is already
  supported, it will indicate so. Otherwise if the evaluation is successful, it
  will print the contents of a sample "CustomVender.ino" file that you can use
  with you sketch. See example "OutlineCustom" for more info on using it.

  It may be overly optomistic to think that all ESP modules that expose
  SD2/GPIO9 and SD3/GPIO10 can have those pins used as GPIO9 and GPIO10.
  However, if the module maker paired the ESP8266EX with a compatible SPI Flash
  that can disable /WP and /HOLD everything should work.



  Write once test everywhere.

  Discussion of what we are dealing with:

  There are different SPI Flash's used on ESP8266EX modules with many different
  characteristics. Some variations:
   * With or without support for QE, Quad Enable
   * QE bit possition at S6, S9, and S15. (S15 is not supported)
   * With or without support for pin function /WP
   * With or without support for pin function /HOLD
   * With or without support for volatile Status Register QE, PM0, .. bits
   * Support for 16-bit Status Register writes and/or 8-bit Status Register writes
   * Some devices only have Status Register-1 8-bits and no Status Register-2.
   * With or without support for Status Register Protect bits SRP0 and SRP1 may
     work to disable /WP feature.
   * There may even be those that support volatile and not non-volatile

  If the device has feature pin functions /WP and/or /HOLD we need a way to turn
  them off. Some combination of "QE" and/or "SRP0 and SRP1" may do this.

  This code expects/assumes the following
   * QE at either S9 or S6
   * If QE is S9, then SRP0 and SRP1 are available.
   * When QE is S6 there is often no Status Register-2.
     Assumes no SRP0 or SRP1 and write zero to those bits.
   * Flash supports non-volatile Status Register-1 with write enable cmd 06h
   * If volatile Status Register is available, then write enable CMD 50h
   * A Flash with only non-volatile Status Register bits is of concern due to
     the risk of bricking a device. Once detected, the automated analyze will
     fail/stop. To overide, use the hotkey menu to select the capitalized
     analyze options 'A' or 'B'.
   * Status Register does not have any OTP bits set.
   * SFDP is not required or used at this time.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <BacktraceLog.h>
#include <ModeDIO_ReclaimGPIOs.h>
#include <TestFlashQE/FlashChipId.h>
#include <TestFlashQE/SFDP.h>
#include <TestFlashQE/WP_HOLD_Test.h>
#include <SfdpRevInfo.h>

// #define NOINLINE __attribute__((noinline))
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
// FlashDiscovery - A list of discovered characteristics that may help guide
// configuring the SPI Flash to disable the pin functions /WP and /HOLD.
//
struct FlashDiscovery {
  uint32_t device = 0;
  bool S9 = false;
  bool S6 = false;
  bool WP = false;                // pin function /WP exist
//D bool has_QE = false;
  bool has_8bw_sr1 = false;       // When true, WEL write operation succeeded
  bool has_8bw_sr2 = false;       // "
  bool has_8bw_sr3 = false;       // "
  bool has_16bw_sr1 = false;      // "      //D-> TODO recheck i think this was an interpitation problem, false possitive - some devices w/o SR2 may clear WEL
  bool has_volatile = false;
  bool write_QE = false;          // No writable QE bit candidate
  bool pass_WP = false;
  bool pass_HOLD = false;
} fd_state;

static uint32_t get_qe_pos() {
  if (fd_state.S9) return 9u;
  if (fd_state.S6) return 6u;
  return 0xFFu;                   // QE not defined or does not exist.
}

static void resetFlashDiscovery() {
  fd_state.S9 = false;
  fd_state.S6 = false;
  fd_state.WP = false;
  fd_state.has_8bw_sr1 = false;
  fd_state.has_8bw_sr2 = false;
  fd_state.has_8bw_sr3 = false;
  fd_state.has_16bw_sr1 = false;
  fd_state.has_volatile = false;
  fd_state.write_QE = false;
  fd_state.pass_WP = false;
  fd_state.pass_HOLD = false;
}

experimental::SfdpRevInfo sfdpInfo;

////////////////////////////////////////////////////////////////////////////////

bool gpio_9_10_available = false;
bool scripting = true;
int last_key __attribute__((section(".noinit")));

void printSR321(const char *indent = "", const bool = false);

void printReclaimFn() {
    Serial.PRINTF("\n"
      "//\n"
      "// Sample CustomVendor.ino\n"
      "//\n"
      "////////////////////////////////////////////////////////////////////////////////\n"
      "// Expand Flash Vendor support for GPIO9 and GPIO10 reclaim\n"
      "#if RECLAIM_GPIO_EARLY || DEBUG_FLASH_QE\n"
      "// Use low level print functions when printing before \"C++\" runtime has initialized.\n"
      "#define DBG_SFU_PRINTF(a, ...) ets_uart_printf(a, ##__VA_ARGS__)\n"
      "#elif DEBUG_FLASH_QE\n"
      "#define DBG_SFU_PRINTF(a, ...) Serial.PRINTF(a, ##__VA_ARGS__)\n"
      "#else\n"
      "#define DBG_SFU_PRINTF(...) do {} while (false)\n"
      "#endif\n"
      "\n"
      "#include <SpiFlashUtilsQE.h>\n"
      "\n"
      "extern \"C\" \n"
      "bool spi_flash_vendor_cases(uint32_t device) {\n"
      "  using namespace experimental;\n"
      "  bool success = false;\n"
      "\n"
      "  if (0x%04X == (device & 0xFFFFu)) {\n"
      "    // <Replace this line with vendor name and part number>\n", fd_state.device & 0xFFFFu);
    //
    if (sfdpInfo.u32[0]) {
      Serial.PRINTF(
        "    // SFDP Revision: %u.%02u, 1ST Parameter Table Revision: %u.%02u\n"
        "    // SFDP Table Ptr: 0x%02X, Size: %u Bytes\n",
        sfdpInfo.hdr_major,  sfdpInfo.hdr_minor, sfdpInfo.parm_major, sfdpInfo.parm_minor,
        sfdpInfo.tbl_ptr, sfdpInfo.sz_dw * 4u);
    } else {
      Serial.PRINTF("    // SFDP none\n");
    }
    if (fd_state.S9) {
      if (fd_state.has_16bw_sr1) {
        Serial.PRINTF_LN("    success = set_S9_QE_bit__16_bit_sr1_write(%svolatile_bit);", (fd_state.has_volatile) ? "" : "non_");
      } else
      if (fd_state.has_8bw_sr2) {
        Serial.PRINTF_LN("    success = set_S9_QE_bit__8_bit_sr2_write(%svolatile_bit);", (fd_state.has_volatile) ? "" : "non_");
      }
    } else
    if (fd_state.S6) {
      Serial.PRINTF_LN("    success = set_S6_QE_bit_WPDis(%svolatile_bit);", (fd_state.has_volatile) ? "" : "non_");
    } else {
      Serial.PRINTF_LN("    success = true;");
    }
    Serial.PRINTF(
      "  }\n"
      "\n"
      "  if (! success) {\n"
      "    // then try builtin support\n"
      "    success = __spi_flash_vendor_cases(device);\n"
      "  }\n"
      "  return success;\n"
      "}\n");
}

void suggestedReclaimFn() {
  Serial.PRINTF("\n"
    "Checking __spi_flash_vendor_cases() for builtin QE bit handler\n");
  // Check if device is already handled by
  bool builtin = __spi_flash_vendor_cases(fd_state.device & 0xFFFFFFu);

  if (fd_state.pass_HOLD && fd_state.pass_WP) {
    if (builtin) {
      Serial.PRINTF("\n"
        "No additional code is needed to support this Flash device; however,\n"
        "the example would look like this:\n");
    }
    printReclaimFn();
  } else {
    if (builtin) {
      Serial.PRINTF("\n"
        "Caution: inconsistency detected. Analyze failed to find a configuration for the\n"
        "Flash, yet the manufacturer ID matches the one we support. There are 11 banks of\n"
        "128 manufacturers. It may be the result of an ID collision.\n");
    } else {
      Serial.PRINTF_LN("No clear solution");
    }
  }
}

void runScript(int next_key) {
  /*
    Run a series of test
    a) Analyze - Find best guess selections S9, S6, 8-bit or 16-bit writes, etc.
      * A failure is NOT expected to cause crash/reboot
    w) Use proposed settings from analyze to test /WP
      * A failure is NOT expected to cause crash/reboot
    h) Use proposed settings from analyze to test /HOLD
      * expect a failure to cause a crash or reboot.

    Print the custom example
  */
  scripting = true;
  if ('a' == next_key) {
    if (processKey('a')) {
      // To be confident of the QE bit location S9 or S6 we need to fail and
      // succeed with /WP, enable and disable write protect with QE.

      // At this point, we have a guess for QE bit either S9 or S6
      if (processKey('w')) {
        // Test /HOLD while QE=1 (or the final passing QE value from 'w' test) -
        // this is a final confirmation that we have free-ed GPIO9 and GPIO10
        // uses settings suggested by 'w'
        if (processKey('h')) {
          suggestedReclaimFn();
        } else {
          Serial.PRINTF_LN("\nUnable to disable pin function /HOLD using QE/S%u", (fd_state.S9) ? 9u : 6u);
        }
      } else {
        Serial.PRINTF_LN("\nUnable to disable pin function /WP using QE/S%u", (fd_state.S9) ? 9u : 6u);
        if (fd_state.S9) {
          Serial.PRINTF_LN("  Suggest trying QE/S6. Use menu items 'A', 'w', 'h', 'p'");
        }
      }
    } else {
      Serial.PRINTF_LN("\nUnable to configure Flash Status Register to free GPIO9 and GPIO10");
    }
    last_key = '?'; // clear value to avoid confusion from a later unrelated crash.
  } else {
    Serial.PRINTF_LN("\nUnable to configure Flash Status Register to free GPIO9 and GPIO10");
    processKey('?');
  }
  scripting = false;
}

extern "C" void patchEarlyCrashReason() {
  // An early HWDT crash is miss misdentified by the SDK. It reports the reason
  // as 0 and are often treated as REASON_DEFAULT_RST. The problem still exist
  // in SDK 3.05. The problem can be seen with crashes in preinit(), setup(),
  // and extend into the 1st pass through `loop()`.
  // A workaround is to set word offset 0x0 in system RTC to REASON_WDT_RST from
  // setup(), preinit() or first pass of loop().
  uint32_t save_ps = xt_rsil(15u);
  uint32_t rtc0;
  system_rtc_mem_read(0, &rtc0, sizeof(rtc0));
  if (0 == rtc0) {
    rtc0 = REASON_WDT_RST;
    system_rtc_mem_write(0, &rtc0, sizeof(rtc0));
  }
  xt_wsr_ps(save_ps);
}

bool run_once = false;

void setup() {
  patchEarlyCrashReason();

  Serial.begin(115200);
  delay(200);
  Serial.PRINTF_LN(
    "\r\n\r\n"
    "Analyze Flash for reclaiming GPIO9 and GPIO10 support");
  printSR321("  ", true);
  fd_state.device = printFlashChipID("  ");

  Serial.PRINTF_LN("  Reset reason: %s", ESP.getResetReason().c_str());
  uint32_t reason = ESP.getResetInfoPtr()->reason;
  switch (reason) {
    case REASON_WDT_RST:          // 1
    case REASON_EXCEPTION_RST:    // 2
    case REASON_SOFT_WDT_RST:     // 3
      switch (last_key) {
        case 'w':   // /WP test GPIO9
          Serial.PRINTF_LN("unexpected crash while testing `/WP`");
          break;

        case 'h':   // /HOLD test GPIO10
          Serial.PRINTF_LN("unwanted but anticipated crash while testing `/HOLD`");
          break;

        case 'a':   // Analyze
        case 'A':   // Analyze with hint S6
          Serial.PRINTF_LN("unexpected crash while running analyze");
          break;

        default:
          Serial.PRINTF_LN("unexpected crash with menu item '%c'", last_key);
          break;
      }
      Serial.PRINTF_LN("\nUnable to configure Flash Status Register to free GPIO9 and GPIO10");
      break;

    // REASON_DEFAULT_RST:      // 0
    // REASON_SOFT_RESTART:     // 4
    // REASON_DEEP_SLEEP_AWAKE: // 5
    // REASON_EXT_SYS_RST:      // 6
    default:
      run_once = true;
      last_key = '?';
      break;
  }
}

void loop() {
  if (run_once) {
    run_once = false;
    runScript('a');   // Start Analyze
  }
  serialClientLoop();
}

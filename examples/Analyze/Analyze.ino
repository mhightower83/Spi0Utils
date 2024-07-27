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
  A SPI flash memory test/analyzer that generates example/sample code for function
  `spi_flash_vendor_cases` using the results from the tests.

  Determine if the SPI flash memory has support for disabling pin functions /WP
  write protect and /HOLD. Or confirm that those functions are not active.
  With these functions disabled, GPIO09 and GPIO10 can be reclaimed for GPIO
  sketch use.

  At boot "Analyze" will attempt to discover the properties of your SPI flash
  memory. It will generate a progress report as it goes. If your device is
  already supported, it will indicate so. Otherwise if the evaluation is
  successful, it will print the contents of a sample "CustomVender.ino" file
  that you can use with you sketch. See example "OutlineCustom" for more info.

  It may be overly optomistic to think that all ESP modules that expose
  SD2/GPIO9 and SD3/GPIO10 can have those pins used as GPIO9 and GPIO10.
  However, if the module maker paired the ESP8266EX with a compatible SPI flash
  memory that can disable /WP and /HOLD everything should work.


  Summary of what we are dealing with:

  There are different SPI flash chips used on ESP8266EX modules with many
  different characteristics. Some variations:
   * With or without support for QE, Quad Enable
   * QE bit possition at S6, S9, and S15. (S15 is not supported)
   * With or without support for pin function /WP
   * With or without support for pin function /HOLD
   * With or without support for volatile Status Register QE, PM0, .. bits
   * Support for 16-bit Status Register writes and/or 8-bit Status Register writes
   * Some devices only have Status Register-1 8-bits and no Status Register-2.
   * Flash memory with Status Register protect bits SRP0 and SRP1 may use them
     to disable the pin feature /WP.
   * There may even be those that support volatile and not non-volatile

  If the device has feature pin functions /WP and/or /HOLD we need a way to turn
  them off. Some combination of "QE" and/or "SRP0 and SRP1" may do this.

  This code expects/assumes the following
   * QE at either S9 or S6
   * If QE is S9, then SRP0 and SRP1 are available.
   * When QE is S6 there is often no Status Register-2.
     Assumes no SRP0 or SRP1 and write zero to those bits.
   * Flash memory supports non-volatile Status Register-1 with write enable CMD 06h
   * If volatile Status Register is supported, then expects write enable CMD 50h
   * A flash memory with only non-volatile Status Register bits is of concern
     due to the risk of bricking a device. Once detected, the automated analyze
     will fail/stop. To overide, use the hotkey menu to select the capitalized
     analyze options 'A' or 'B'.
   * Status Register must not have any OTP bits set. This can break the tests.
   * SFDP is not required or used at this time.
*/

#include <Arduino.h>
#include <user_interface.h>
// #include <BacktraceLog.h>

#define NOINLINE __attribute__((noinline))
#define PRINTF(a, ...)        printf_P(PSTR(a), ##__VA_ARGS__)
#define PRINTF_LN(a, ...)     printf_P(PSTR(a "\n"), ##__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////
// Debug MACROS in SpiFlashUtils library for control printing also referenced
// by ModeDIO_ReclaimGPIOs. If required, place these defines in your
// Sketch.ino.globals.h file.
//
//  -DDEBUG_FLASH_QE=1 - use to debug library or provide insite when things
//     don't work as expected. When finished with debugging, remove define to
//     reduce code and DRAM footprint.
//
//  -DRECLAIM_GPIO_EARLY=1 - Build flag indicates that reclaim_GPIO_9_10() may
//     be called before C++ runtime has been called as in preinit().
//     Works with -DDEBUG_FLASH_QE=1 to allow debug printing before C++ runtime.
//
#include <ModeDIO_ReclaimGPIOs.h>
#include <SfdpRevInfo.h>
#include <TestFlashQE/FlashChipId.h>
#include <TestFlashQE/SFDP.h>
#include <TestFlashQE/WP_HOLD_Test.h>

////////////////////////////////////////////////////////////////////////////////
// FlashDiscovery - A list of discovered characteristics that may help guide
// configuring the SPI flash memory to disable the pin functions /WP and /HOLD.
//
struct FlashDiscovery {
  uint32_t device = 0;
  bool S9 = false;                // Proposed/discovered QE bit possition
  bool S6 = false;                // " S6 or S9 or neither
  bool WP = false;                // Pin function /WP exist
  bool has_8bw_sr1 = false;       // When true, WEL write operation succeeded
  bool has_8bw_sr2 = false;       // "
  bool has_8bw_sr3 = false;       // "
  bool has_16bw_sr1 = false;      // "
  bool has_volatile = false;      // Volatile Status Register bit detected
  bool write_QE = false;          // Writable QE bit candidate, S9 or S6 is set
  bool pass_analyze = false;
  bool pass_SC = false;           // Short Circuit tests on GPIO 9 and 10 passed
  bool pass_WP = false;           // /WP disabled - Test results
  bool pass_HOLD = false;         // /HOLD disabled - Test results
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
  fd_state.pass_analyze = false;
  fd_state.pass_SC = false;
  fd_state.pass_WP = false;
  fd_state.pass_HOLD = false;
}

experimental::SfdpRevInfo sfdpInfo;

////////////////////////////////////////////////////////////////////////////////

bool gpio_9_10_available = false;
bool scripting = true;
int last_key __attribute__((section(".noinit")));

void printSR321(const char *indent = "", const bool = false);

/*
  Print sample code for "CustomVender.ino"
  Should only be called when analyze, /WP, and /HOLD tests pass.
*/
void printReclaimFn() {
    Serial.PRINTF(
      "//\n"
      "// CustomVendor.ino\n"
      "//\n"
      "// Add new flash vendor support for GPIO9 and GPIO10 reclaim\n"
      "//\n"
      "#include <ModeDIO_ReclaimGPIOs.h>\n"
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
        "    // SFDP Table Ptr: 0x%02X, Size: %u DW\n",
        sfdpInfo.hdr_major,  sfdpInfo.hdr_minor, sfdpInfo.parm_major, sfdpInfo.parm_minor,
        sfdpInfo.tbl_ptr, sfdpInfo.sz_dw);
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
      Serial.PRINTF_LN("    success = %s;",
        (fd_state.pass_SC && fd_state.pass_WP && fd_state.pass_HOLD) ? "true" : "false");
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
  // Check if device is already handled by 'spi_flash_vendor_cases()' and run
  // test to confirm that it works.
  bool builtin = processKey('t');

  if (fd_state.pass_HOLD && fd_state.pass_WP && fd_state.pass_SC) {
    Serial.PRINTF("\n"
      "////////////////////////////////////////////////////////////////////////////////\n");
    if (builtin) {
      Serial.PRINTF(
        "// No additional code is needed to support this flash memory; however,\n"
        "// the example template would look like this:\n"
        "//\n");
    } else {
      Serial.PRINTF(
        "// To add support for your new flash memory, copy/paste the example code\n"
        "// below into a file in your sketch folder.\n"
        "//\n");
    }
    printReclaimFn();
  } else {
    if (builtin) {
      Serial.PRINTF("\n"
        "Caution: inconsistency detected. Analyze failed to find a configuration for\n"
        "the flash memory, yet the manufacturer ID matches one we support. There are 14\n"
        "banks of 128 manufacturers. It may be the result of an ID collision.\n");
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
  Serial.PRINTF_LN("\nRun Script at 1ST boot");
  if ('a' == next_key || 'b' == next_key || 'A' == next_key || 'B' == next_key) {
    if (processKey(next_key)) {
      // At this point, we have a guess for QE bit either S9 or S6
      // To be confident of the QE bit location S9 or S6 we need to fail and
      // succeed with /WP tests, enable and disable write protect with QE.
      if (processKey('w')) {
        // Test /HOLD while QE=1 (or the final passing QE value from 'w' test) -
        // this is a final confirmation that we have free-ed both GPIO9 and
        // GPIO10 uses settings suggested by 'w'
        if (processKey('h')) {
          suggestedReclaimFn();
        } else {
          Serial.PRINTF_LN("\nUnable to disable pin function /HOLD using QE/S%X", (fd_state.S9) ? 9u : 6u);
        }
      } else {
        Serial.PRINTF_LN("\nUnable to disable pin function /WP using QE/S%X", (fd_state.S9) ? 9u : 6u);
        if (fd_state.S9) {
          Serial.PRINTF_LN("  Suggest trying QE/S6. Use menu hotkeys 'b', 'w', 'h', 'p'");
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
  // An early HWDT crash is misdentified by the SDK. It reports the reason as 0
  // and are often treated as REASON_DEFAULT_RST. This problem persist in SDK
  // 3.05. The issue can be seen with crashes in preinit(), setup(), and
  // extend into the 1st pass through `loop()`.
  // The problem stops showing up after the first WDT tick/interrupt is
  // serviced. A workaround is to set word offset 0x0 in system RTC to
  // REASON_WDT_RST from setup(), preinit() or first pass of loop().
  uint32_t save_ps = xt_rsil(15u);
  uint32_t rtc0;
  system_rtc_mem_read(0u, &rtc0, sizeof(rtc0));
  if (0u == rtc0) {
    rtc0 = REASON_WDT_RST;
    system_rtc_mem_write(0u, &rtc0, sizeof(rtc0));
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
    "Analyze Flash Memory for reclaiming GPIO9 and GPIO10 support");
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
          Serial.PRINTF_LN("unexpected crash with menu hotkey '%c'", last_key);
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
#ifdef RUN_SCRIPT_AT_BOOT
    runScript('a');   // Start Analyze
#endif
  }
  serialClientLoop();
}

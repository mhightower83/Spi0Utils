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
  SPI Flash Data Parameters

  To understand the dump, you will need access to JEDEC documentation. In some
  cases, the SPI Flash vendor's datasheet will list an explanation of their
  SFDP data.

  Reference:
    JEDEC STANDARD Serial Flash Discoverable Parameters
    https://www.jedec.org/standards-documents/docs/jesd216b
    Free Download - requires registration

*/
#include <Arduino.h>
extern "C" {
#include <SpiFlashUtils.h>
// Rely on DBG_SFU_PRINTF from SpiFlashUtils.h
#define ETS_PRINTF(fmt, ...) DBG_SFU_PRINTF(fmt, ##__VA_ARGS__)
#include "SFDP.h"

#define NOINLINE __attribute__((noinline))

using namespace experimental;

void printSfdpReport() {
// #if 1
  union SFDP_Hdr {
    struct {
      uint32_t signature:32;
      uint32_t rev_minor:8;
      uint32_t rev_major:8;
      uint32_t num_parm_hdrs:8;     // zero based
      uint32_t access_protocol:8;
    };
    uint32_t u32[2];
  } sfdp_hdr;

  union SFDP_Param {
    struct {
      uint32_t id_lsb:8;
      uint32_t rev_minor:8;
      uint32_t rev_major:8;
      uint32_t num_dw:8;          // 1-based
      uint32_t tbl_ptr:24;
      uint32_t id_msb:8;
    };
    uint32_t u32[2];
  } sfdp_param;

  union SFDP_Basic_1 {
    struct {
      uint32_t erase_size:2;     // 00 & 10=res., 01=4KB erase, 11=64KB erase
      uint32_t wr_granularity:1; // 0=No, 1=Yes
      uint32_t wr_en_volatile:2; // 00=N/A, 01=use 50h opcode, 11=use 06h opcode
      uint32_t unused:3;
      uint32_t erase_4k_cmd:8;
      uint32_t fast_read_1_1_2:1; // opcode, address_input, output
      uint32_t address_bytes:2;
      uint32_t d_t_r:1; // 1 = device supports some type of double transfer rate clocking
      uint32_t fast_read_1_2_2:1;
      uint32_t fast_read_1_4_4:1;
      uint32_t fast_read_1_1_4:1;
      uint32_t unused2:9;

      uint32_t capacity:31;
      uint32_t giga:1;
    };
    uint32_t u32[2];
  } basic_param;

  [[maybe_unused]]
  union SFDP_Basic_15 {
    struct {
      // 15th DWORD - page 40
      uint32_t ignore:20;
      uint32_t qe:3;            // QE Quad Enable support - 0 = no QE support, 1-6 many variations
      uint32_t hold_disable:1;  // Hold/Reset function disable feature available in Extended Status Register
      uint32_t reserved2:8;
      // 16th DWORD - page 42
      uint32_t non_volatile:7;
      uint32_t reserved:1;
      uint32_t soft_reset:6;
      uint32_t exit_4b_addr:10;
      uint32_t enter_4b_addr:8;
    };
    uint32_t u32[2];
  } basic_param_15;

  constexpr uint32_t kSfdpSignature = 0x50444653u; //'SFDP'

  SpiOpResult ok0;

  size_t sz = sizeof(sfdp_hdr);
  size_t addr = 0u;

  ok0 = spi0_flash_read_sfdp(addr, &sfdp_hdr.u32[0], sz);
  if (SPI_RESULT_OK == ok0 && kSfdpSignature == sfdp_hdr.signature) {
    ETS_PRINTF("SFDP Header\n");
    ETS_PRINTF("  %-18s 0x%08X\n", "SFDP Signature", sfdp_hdr.signature);
    ETS_PRINTF("  %-18s %u.%u\n", "Revision", sfdp_hdr.rev_major, sfdp_hdr.rev_minor);
    ETS_PRINTF("  %-18s 0x%02X\n", "Access Protocol", sfdp_hdr.access_protocol);
    ETS_PRINTF("  %-18s %d\n", "Num Param hdrs", sfdp_hdr.num_parm_hdrs);

    // size_t addr = 0u;
    for (size_t i = 0u; i < (sfdp_hdr.num_parm_hdrs + 1u); i++) {
      addr += sz;
      sz = sizeof(sfdp_param);
      ok0 = spi0_flash_read_sfdp(addr, &sfdp_param.u32[0], sz);
      if (SPI_RESULT_OK == ok0) {
        ETS_PRINTF("\nParameter Header #%u\n", i + 1);
        ETS_PRINTF("  %-18s %d\n", "Num dwords", sfdp_param.num_dw);
        ETS_PRINTF("  %-18s %u.%u\n", "Revision", sfdp_param.rev_major, sfdp_param.rev_minor);
        ETS_PRINTF("  %-18s 0x%02X.%02X\n", "ID MSB.LSB", sfdp_param.id_msb, sfdp_param.id_lsb);
        ETS_PRINTF("  %-18s 0x%08X\n", "TBL PTR", sfdp_param.tbl_ptr);
      }
      if (0xFFu == sfdp_param.id_msb && 0x00u == sfdp_param.id_lsb) {
        ETS_PRINTF("\nTable #%u of Basic Parameters\n", i + 1);
        size_t flash_sz;
        size_t addr_tbl = sfdp_param.tbl_ptr;
        size_t sz_tbl = sizeof(basic_param);
        ok0 = spi0_flash_read_sfdp(addr_tbl, &basic_param.u32[0], sz_tbl);
        if (basic_param.giga) {
          flash_sz = 1u << (basic_param.capacity - 3u); // Convert bits to bytes
        } else {
          flash_sz = (basic_param.capacity + 1u) / 8u;
        }
        ETS_PRINTF("  %-18s 0x%08X, %u\n", "Capacity in Bytes", flash_sz, flash_sz);
        // TODO: Dump the descibed Parameter table
        ETS_PRINTF("  TODO: Described more of the Basic Parameter table\n");
      } else {
        ETS_PRINTF("\nTable #%u of Parameters\n", i + 1);
        ETS_PRINTF("  TODO: Descibed Parameter table\n");
      }
    }
  }

  ETS_PRINTF("\nRaw dump of SFDP");
  uint32_t param[4];
  ok0 = spi0_flash_read_sfdp(0, param, sizeof(param));
  if (SPI_RESULT_OK == ok0 && kSfdpSignature == param[0]) { //'SFDP' = 50444653h
    // const char *intro = "SFDP:";
    for (size_t j = 0; ; ) {
      // ETS_PRINTF("\n  %-9s  0x%02X  ", intro, 0x10 * j);
      ETS_PRINTF("\n  0x%02X  ", 0x10 * j);
      for (size_t i=0; i < 4; i++) {
        ETS_PRINTF(" 0x%08X", param[i]);
      }
      j++;
      if (j >= 16) break;
      // intro = "";
      if (SPI_RESULT_OK != spi0_flash_read_sfdp((j * 16u), param, sizeof(param))) {
        ETS_PRINTF(" error reading SFDP.");
        break;
      }
    }
  } else {
    ETS_PRINTF(" not supported.");
  }
  ETS_PRINTF("\n");
}

// SpiOpResult spi0_flash_read_secure_register(uint32_t reg, uint32_t offset,  uint32_t *p, size_t sz)
void printSecurityRegisters(uint32_t reg) {
  ETS_PRINTF("\nRaw dump of Security Register #%u", reg);
  uint32_t param[4];
  SpiOpResult ok0 = spi0_flash_read_secure_register(reg, 0, param, sizeof(param));
  if (SPI_RESULT_OK == ok0) {
    for (size_t j = 0; ; ) {
      ETS_PRINTF("\n  0x%02X  ", 0x10 * j);
      for (size_t i=0; i < 4; i++) {
        ETS_PRINTF(" 0x%08X", param[i]);
      }
      j++;
      if (j >= 16) break;
      if (SPI_RESULT_OK != spi0_flash_read_secure_register(reg, (j * 16u), param, sizeof(param))) {
        ETS_PRINTF("  error reading Security Register.");
        break;
      }
    }
  } else {
    ETS_PRINTF("Read secure register call failed.");
  }
  ETS_PRINTF("\n");
}

}; // extern "C" {...};

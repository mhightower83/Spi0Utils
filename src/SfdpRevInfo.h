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
#ifndef SFDPREVINFO_H
#define SFDPREVINFO_H

namespace experimental {

union SfdpHdr {
  struct {
    uint32_t signature:32;
    uint32_t rev_minor:8;
    uint32_t rev_major:8;
    uint32_t num_parm_hdrs:8;   // zero-based
    uint32_t access_protocol:8;
  };
  uint32_t u32[2];
};

union SfdpParam {
  struct {
    uint32_t id_lsb:8;
    uint32_t rev_minor:8;
    uint32_t rev_major:8;
    uint32_t sz_dw:8;           // 1-based
    uint32_t tbl_ptr:24;
    uint32_t id_msb:8;
  };
  uint32_t u32[2];
};

union SfdpRevInfo {
  struct {
    uint32_t hdr_minor:8;
    uint32_t hdr_major:8;
    uint32_t parm_minor:8;
    uint32_t parm_major:8;
    uint32_t sz_dw:8;           // Size of the first parameter table in double words
    uint32_t num_parm_hdrs:8;   // Number of parameter headers
    uint32_t tbl_ptr:16;        // JEDEC field is 24 bits, however, SFDP table is 256 bytes MAX
  };
  uint32_t u32[2];
};

union SFDP_Basic_1dw {
  struct {
    uint32_t erase_size:2;      // 00 & 10=res., 01=4KB erase, 11=64KB erase
    uint32_t wr_granularity:1;  // 0=No, 1=Yes
    uint32_t wr_en_volatile:2;  // 00=N/A, 01=use 50h opcode, 11=use 06h opcode
    uint32_t unused:3;
    uint32_t erase_4k_cmd:8;
    uint32_t fast_read_1_1_2:1; // opcode, address_input, output
    uint32_t address_bytes:2;
    uint32_t d_t_r:1;           // 1 = device supports some type of double transfer rate clocking
    uint32_t fast_read_1_2_2:1;
    uint32_t fast_read_1_4_4:1;
    uint32_t fast_read_1_1_4:1;
    uint32_t unused2:9;
  };
  uint32_t u32[2];
};

union SFDP_Basic_2dw {
  struct {
    uint32_t capacity:31;       // giga == 0, size in bits | giga == 1, size as 2^capacity
    uint32_t giga:1;
  };
  uint32_t u32[2];
};

extern "C" {
  SfdpRevInfo get_sfdp_revision();
  uint32_t* get_sfdp_basic(SfdpRevInfo *rev);
}
};
#endif // FLASH_CHIP_ID_H

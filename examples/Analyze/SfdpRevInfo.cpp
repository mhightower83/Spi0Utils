////////////////////////////////////////////////////////////////////////////////
// SFDP - JEDEC SPI Flash Data Parameters
//
// SFDP info may be useful, when dealing with two different SPI Flash chips,
// with identical `spi_flash_get_id()` results. Limited info returns version
// info and Parameter Table pointer and size.
#include <Arduino.h>
#include <SpiFlashUtils.h>

union SFDP_Hdr {
  struct {
    uint32_t signature:32;
    uint32_t rev_minor:8;
    uint32_t rev_major:8;
    uint32_t num_param_hdrs:8;
    uint32_t access_protocol:8;
  };
  uint32_t u32[2];
};

union SFDP_Param {
  struct {
    uint32_t id_lsb:8;
    uint32_t rev_minor:8;
    uint32_t rev_major:8;
    uint32_t sz_dw:8;
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
    uint32_t sz_dw:8;
    uint32_t tbl_ptr:24;
  };
  uint32_t u32[2];
};

union SfdpRevInfo get_sfdp_revision() {
  using namespace experimental;

  union SFDP_Hdr sfdp_hdr;
  union SFDP_Param sfdp_param;
  union SfdpRevInfo rev;
  rev.u32[0] = rev.u32[1] = 0;

  size_t sz = sizeof(sfdp_hdr);
  size_t addr = 0u;
  SpiOpResult ok0 = spi0_flash_read_sfdp(addr, &sfdp_hdr.u32[0], sz);
  if (SPI_RESULT_OK == ok0 && 0x50444653 == sfdp_hdr.signature) {
    rev.hdr_major = sfdp_hdr.rev_major;
    rev.hdr_minor = sfdp_hdr.rev_minor;

    addr += sz;
    sz = sizeof(sfdp_param);
    ok0 = spi0_flash_read_sfdp(addr, &sfdp_param.u32[0], sz);
    if (SPI_RESULT_OK == ok0) {
      // return version of 1st parameter block
      rev.parm_major = sfdp_param.rev_major;
      rev.parm_minor = sfdp_param.rev_minor;
      rev.sz_dw = sfdp_param.sz_dw;
      rev.tbl_ptr = sfdp_param.tbl_ptr;
    }
  }
  return rev;
}

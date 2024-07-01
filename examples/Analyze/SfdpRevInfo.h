#ifndef SFDPREVINFO_H
#define SFDPREVINFO_H


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

union SfdpRevInfo get_sfdp_revision();

#endif // FLASH_CHIP_ID_H

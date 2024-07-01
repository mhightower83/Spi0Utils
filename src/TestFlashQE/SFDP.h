/*
  SPI Flash Data Parameters
*/
#ifndef TESTFLASHQE_SFDP_CPP_H
#define TESTFLASHQE_SFDP_CPP_H

#ifdef __cplusplus
extern "C" {
#endif

void printSfdpReport();
void printSecurityRegisters(uint32_t reg);

#ifdef __cplusplus
};
#endif
#endif // TESTFLASHQE_SFDP_CPP_H

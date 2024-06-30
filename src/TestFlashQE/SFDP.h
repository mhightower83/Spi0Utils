/*
  SPI Flash Data Parameters
*/
#ifndef SFDP_CPP_H
#define SFDP_CPP_H

#ifdef __cplusplus
extern "C" {
#endif

void printSfdpReport();
void printSecurityRegisters(uint32_t reg);

#ifdef __cplusplus
};
#endif
#endif // SFDP_CPP_H

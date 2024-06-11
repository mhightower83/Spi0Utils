#ifndef K_STATUS_REGISTER_BIT_DEF_H
#define K_STATUS_REGISTER_BIT_DEF_H

// #include "kStatusRegisterBitDef.h"
constexpr uint32_t kWIPBit   = BIT0;  // Write In Progress
constexpr uint32_t kWELBit   = BIT1;  // Write Enable Latch
constexpr uint32_t kWPDISBit = BIT6;  // Disable /WP pin
constexpr uint32_t kQEBit1B  = BIT1;  // Enable QE=1, disables WP# and HOLD#
constexpr uint32_t kQEBit2B  = BIT9;  // Enable QE=1, disables WP# and HOLD#

#endif

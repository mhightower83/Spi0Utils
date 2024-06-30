#ifndef WP_HOLD_TEST_H
#define WP_HOLD_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

struct OutputTestResult {
  uint32_t qe_bit:8;          // 0xFF uninitialized, 7 for S7 and 9 for S9
  uint32_t srp0:1;            // BIT7 of SR321
  uint32_t srp1:1;            // BIT8 of SR321
  uint32_t low:1;             // pass/fail results for GPIO10 LOW
  uint32_t high:1;            // pass/fail results for GPIO10 HIGH
  uint32_t qe:1;              // QE bit
};

bool test_set_SRP1_SRP0_clear_QE(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool non_volatile);
bool test_clear_SRP1_SRP0_QE(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool non_volatile);
uint32_t test_set_QE(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool non_volatile, const bool was_preset);
bool testFlashWrite(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool non_volatile);
OutputTestResult testOutputGPIO10(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool non_volatile, const bool was_preset);
bool testOutputGPIO9(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool non_volatile, const bool was_preset);

bool testInput_GPIO9_GPIO10(const uint32_t qe_bit, const bool use_16_bit_sr1, const bool non_volatile, const bool was_preset);

#ifdef __cplusplus
};
#endif
#endif // WP_HOLD_TEST_H

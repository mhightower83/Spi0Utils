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
#ifndef TESTFLASHQE_WP_HOLD_TEST_H
#define TESTFLASHQE_WP_HOLD_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

// Used for QE/S9 only
uint32_t test_set_SRP1_SRP0_clear_QE(const uint32_t qe_pos, const bool use_16_bit_sr1, const bool non_volatile);

// General clear SR2 and SR1 with success verified.
uint32_t test_clear_SRP1_SRP0_QE(const bool has_8bw_sr2, const bool use_16_bit_sr1, const bool non_volatile);

// Test - turning off pin feature /HOLD
bool testOutputGPIO9(const uint32_t qe_pos, const bool use_16_bit_sr1, const bool non_volatile, const bool was_preset);

// Test - turning off pin feature /WP
bool testOutputGPIO10(const uint32_t qe_pos, const bool use_16_bit_sr1, const bool non_volatile, const bool was_preset);

#if 0
// Use to be called from an example; however, not now.
bool testInput_GPIO9_GPIO10(const uint32_t qe_pos, const bool use_16_bit_sr1, const bool non_volatile, const bool was_preset);
#endif

#if 0
// So far used locally - make static for now
int test_set_QE(const uint32_t qe_pos, const bool use_16_bit_sr1, const bool non_volatile, const bool was_preset);
bool testFlashWrite(const uint32_t qe_pos, const bool use_16_bit_sr1, const bool non_volatile);
#endif

#ifdef __cplusplus
};
#endif
#endif // TESTFLASHQE_WP_HOLD_TEST_H

/*
 *  Copyright (c) 2014 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "vp9/common/vp9_blockd.h"
#include "vp9/common/vp9_pred_common.h"
#include "vpx_mem/vpx_mem.h"

namespace {

using libvpx_test::ACMRandom;

const int count_test_block = 100000;

typedef void (*IntraPred)(uint16_t* dst, ptrdiff_t stride,
                          const uint16_t* above, const uint16_t* left,
                          int bps);

struct IntraPredFunc {
  IntraPredFunc(IntraPred pred = NULL, IntraPred ref = NULL,
                int block_size_value = 0, int bit_depth_value = 0)
      : pred_fn(pred), ref_fn(ref),
        block_size(block_size_value), bit_depth(bit_depth_value) {}

  IntraPred pred_fn;
  IntraPred ref_fn;
  int block_size;
  int bit_depth;
};

class VP9IntraPredTest : public ::testing::TestWithParam<IntraPredFunc> {
 public:
  void RunTest(uint16_t* left_col, uint16_t* above_data,
               uint16_t* dst, uint16_t* ref_dst) {
    ACMRandom rnd(ACMRandom::DeterministicSeed());
    const int block_size = params_.block_size;
    above_row_ = above_data + 16;
    left_col_ = left_col;
    dst_ = dst;
    ref_dst_ = ref_dst;
    int error_count = 0;
    for (int i = 0; i < count_test_block; ++i) {
      // Fill edges with random data, try first with saturated values.
      for (int x = -1; x <= block_size * 2; x++) {
        if (i == 0) {
          above_row_[x] = mask_;
        } else {
          above_row_[x] = rnd.Rand16() & mask_;
        }
      }
      for (int y = 0; y < block_size; y++) {
        if (i == 0) {
          left_col_[y] = mask_;
        } else {
          left_col_[y] = rnd.Rand16() & mask_;
        }
      }
      Predict();
      CheckPrediction(i, &error_count);
    }
    ASSERT_EQ(0, error_count);
  }

 protected:
  virtual void SetUp() {
    params_ = GetParam();
    stride_ = params_.block_size * 3;
    mask_   = (1 << params_.bit_depth) - 1;
  }

  void Predict() {
    const int bit_depth = params_.bit_depth;
    params_.ref_fn(ref_dst_, stride_, above_row_, left_col_, bit_depth);
    ASM_REGISTER_STATE_CHECK(params_.pred_fn(dst_, stride_,
                                             above_row_, left_col_, bit_depth));
  }

  void CheckPrediction(int test_case_number, int *error_count) const {
    // For each pixel ensure that the calculated value is the same as reference.
    const int block_size = params_.block_size;
    for (int y = 0; y < block_size; y++) {
      for (int x = 0; x < block_size; x++) {
        *error_count += ref_dst_[x + y * stride_] != dst_[x + y * stride_];
        if (*error_count == 1) {
          ASSERT_EQ(ref_dst_[x + y * stride_], dst_[x + y * stride_])
              << " Failed on Test Case Number "<< test_case_number;
        }
      }
    }
  }

  uint16_t *above_row_;
  uint16_t *left_col_;
  uint16_t *dst_;
  uint16_t *ref_dst_;
  ptrdiff_t stride_;
  int mask_;

  IntraPredFunc params_;
};

TEST_P(VP9IntraPredTest, IntraPredTests) {
  // max block size is 32
  DECLARE_ALIGNED(16, uint16_t, left_col[2*32]);
  DECLARE_ALIGNED(16, uint16_t, above_data[2*32+32]);
  DECLARE_ALIGNED(16, uint16_t, dst[3 * 32 * 32]);
  DECLARE_ALIGNED(16, uint16_t, ref_dst[3 * 32 * 32]);
  RunTest(left_col, above_data, dst, ref_dst);
}

#if HAVE_SSE2
#if CONFIG_VP9_HIGHBITDEPTH
INSTANTIATE_TEST_CASE_P(SSE2_TO_C_8, VP9IntraPredTest,
    ::testing::Values(
      IntraPredFunc(&vpx_highbd_dc_predictor_32x32_sse2,
                    &vpx_highbd_dc_predictor_32x32_c, 32, 8),
      IntraPredFunc(&vpx_highbd_tm_predictor_16x16_sse2,
                    &vpx_highbd_tm_predictor_16x16_c, 16, 8),
      IntraPredFunc(&vpx_highbd_tm_predictor_32x32_sse2,
                    &vpx_highbd_tm_predictor_32x32_c, 32, 8),
      IntraPredFunc(&vpx_highbd_dc_predictor_4x4_sse2,
                    &vpx_highbd_dc_predictor_4x4_c, 4, 8),
      IntraPredFunc(&vpx_highbd_dc_predictor_8x8_sse2,
                    &vpx_highbd_dc_predictor_8x8_c, 8, 8),
      IntraPredFunc(&vpx_highbd_dc_predictor_16x16_sse2,
                    &vpx_highbd_dc_predictor_16x16_c, 16, 8),
      IntraPredFunc(&vpx_highbd_v_predictor_4x4_sse2,
                    &vpx_highbd_v_predictor_4x4_c, 4, 8),
      IntraPredFunc(&vpx_highbd_v_predictor_8x8_sse2,
                    &vpx_highbd_v_predictor_8x8_c, 8, 8),
      IntraPredFunc(&vpx_highbd_v_predictor_16x16_sse2,
                    &vpx_highbd_v_predictor_16x16_c, 16, 8),
      IntraPredFunc(&vpx_highbd_v_predictor_32x32_sse2,
                    &vpx_highbd_v_predictor_32x32_c, 32, 8),
      IntraPredFunc(&vpx_highbd_tm_predictor_4x4_sse2,
                    &vpx_highbd_tm_predictor_4x4_c, 4, 8),
      IntraPredFunc(&vpx_highbd_tm_predictor_8x8_sse2,
                    &vpx_highbd_tm_predictor_8x8_c, 8, 8)));

INSTANTIATE_TEST_CASE_P(SSE2_TO_C_10, VP9IntraPredTest,
    ::testing::Values(
      IntraPredFunc(&vpx_highbd_dc_predictor_32x32_sse2,
                    &vpx_highbd_dc_predictor_32x32_c, 32, 10),
      IntraPredFunc(&vpx_highbd_tm_predictor_16x16_sse2,
                    &vpx_highbd_tm_predictor_16x16_c, 16, 10),
      IntraPredFunc(&vpx_highbd_tm_predictor_32x32_sse2,
                    &vpx_highbd_tm_predictor_32x32_c, 32, 10),
      IntraPredFunc(&vpx_highbd_dc_predictor_4x4_sse2,
                    &vpx_highbd_dc_predictor_4x4_c, 4, 10),
      IntraPredFunc(&vpx_highbd_dc_predictor_8x8_sse2,
                    &vpx_highbd_dc_predictor_8x8_c, 8, 10),
      IntraPredFunc(&vpx_highbd_dc_predictor_16x16_sse2,
                    &vpx_highbd_dc_predictor_16x16_c, 16, 10),
      IntraPredFunc(&vpx_highbd_v_predictor_4x4_sse2,
                    &vpx_highbd_v_predictor_4x4_c, 4, 10),
      IntraPredFunc(&vpx_highbd_v_predictor_8x8_sse2,
                    &vpx_highbd_v_predictor_8x8_c, 8, 10),
      IntraPredFunc(&vpx_highbd_v_predictor_16x16_sse2,
                    &vpx_highbd_v_predictor_16x16_c, 16, 10),
      IntraPredFunc(&vpx_highbd_v_predictor_32x32_sse2,
                    &vpx_highbd_v_predictor_32x32_c, 32, 10),
      IntraPredFunc(&vpx_highbd_tm_predictor_4x4_sse2,
                    &vpx_highbd_tm_predictor_4x4_c, 4, 10),
      IntraPredFunc(&vpx_highbd_tm_predictor_8x8_sse2,
                    &vpx_highbd_tm_predictor_8x8_c, 8, 10)));

INSTANTIATE_TEST_CASE_P(SSE2_TO_C_12, VP9IntraPredTest,
    ::testing::Values(
      IntraPredFunc(&vpx_highbd_dc_predictor_32x32_sse2,
                    &vpx_highbd_dc_predictor_32x32_c, 32, 12),
      IntraPredFunc(&vpx_highbd_tm_predictor_16x16_sse2,
                    &vpx_highbd_tm_predictor_16x16_c, 16, 12),
      IntraPredFunc(&vpx_highbd_tm_predictor_32x32_sse2,
                    &vpx_highbd_tm_predictor_32x32_c, 32, 12),
      IntraPredFunc(&vpx_highbd_dc_predictor_4x4_sse2,
                    &vpx_highbd_dc_predictor_4x4_c, 4, 12),
      IntraPredFunc(&vpx_highbd_dc_predictor_8x8_sse2,
                    &vpx_highbd_dc_predictor_8x8_c, 8, 12),
      IntraPredFunc(&vpx_highbd_dc_predictor_16x16_sse2,
                    &vpx_highbd_dc_predictor_16x16_c, 16, 12),
      IntraPredFunc(&vpx_highbd_v_predictor_4x4_sse2,
                    &vpx_highbd_v_predictor_4x4_c, 4, 12),
      IntraPredFunc(&vpx_highbd_v_predictor_8x8_sse2,
                    &vpx_highbd_v_predictor_8x8_c, 8, 12),
      IntraPredFunc(&vpx_highbd_v_predictor_16x16_sse2,
                    &vpx_highbd_v_predictor_16x16_c, 16, 12),
      IntraPredFunc(&vpx_highbd_v_predictor_32x32_sse2,
                    &vpx_highbd_v_predictor_32x32_c, 32, 12),
      IntraPredFunc(&vpx_highbd_tm_predictor_4x4_sse2,
                    &vpx_highbd_tm_predictor_4x4_c, 4, 12),
      IntraPredFunc(&vpx_highbd_tm_predictor_8x8_sse2,
                    &vpx_highbd_tm_predictor_8x8_c, 8, 12)));

#endif  // CONFIG_VP9_HIGHBITDEPTH
#endif  // HAVE_SSE2
}  // namespace

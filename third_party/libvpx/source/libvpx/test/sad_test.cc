/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <string.h>
#include <limits.h>
#include <stdio.h>

#include "third_party/googletest/src/include/gtest/gtest.h"

#include "./vpx_config.h"
#include "./vpx_dsp_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"
#include "vpx/vpx_codec.h"
#include "vpx_mem/vpx_mem.h"
#include "vpx_ports/mem.h"

template<typename Function>
struct TestParams {
  TestParams(int w, int h, Function f, int bd = -1)
      : width(w), height(h), bit_depth(bd), func(f) {}
  int width, height, bit_depth;
  Function func;
};

typedef unsigned int (*SadMxNFunc)(const uint8_t *src_ptr, int src_stride,
                                   const uint8_t *ref_ptr, int ref_stride);
typedef TestParams<SadMxNFunc> SadMxNParam;

typedef unsigned int (*SadMxNAvgFunc)(const uint8_t *src_ptr, int src_stride,
                                      const uint8_t *ref_ptr, int ref_stride,
                                      const uint8_t *second_pred);
typedef TestParams<SadMxNAvgFunc> SadMxNAvgParam;

typedef void (*SadMxNx4Func)(const uint8_t *src_ptr, int src_stride,
                             const uint8_t *const ref_ptr[], int ref_stride,
                             unsigned int *sad_array);
typedef TestParams<SadMxNx4Func> SadMxNx4Param;

using libvpx_test::ACMRandom;

namespace {
template<typename ParamType>
class SADTestBase : public ::testing::TestWithParam<ParamType> {
 public:
  explicit SADTestBase(const ParamType& params) : params_(params) {}

  virtual void SetUp() {
    source_data8_ = reinterpret_cast<uint8_t*>(
        vpx_memalign(kDataAlignment, kDataBlockSize));
    reference_data8_ = reinterpret_cast<uint8_t*>(
        vpx_memalign(kDataAlignment, kDataBufferSize));
    second_pred8_ = reinterpret_cast<uint8_t*>(
        vpx_memalign(kDataAlignment, 64*64));
    source_data16_ = reinterpret_cast<uint16_t*>(
        vpx_memalign(kDataAlignment, kDataBlockSize*sizeof(uint16_t)));
    reference_data16_ = reinterpret_cast<uint16_t*>(
        vpx_memalign(kDataAlignment, kDataBufferSize*sizeof(uint16_t)));
    second_pred16_ = reinterpret_cast<uint16_t*>(
        vpx_memalign(kDataAlignment, 64*64*sizeof(uint16_t)));

    if (params_.bit_depth == -1) {
      use_high_bit_depth_ = false;
      bit_depth_ = VPX_BITS_8;
      source_data_ = source_data8_;
      reference_data_ = reference_data8_;
      second_pred_ = second_pred8_;
#if CONFIG_VP9_HIGHBITDEPTH
    } else {
      use_high_bit_depth_ = true;
      bit_depth_ = static_cast<vpx_bit_depth_t>(params_.bit_depth);
      source_data_ = CONVERT_TO_BYTEPTR(source_data16_);
      reference_data_ = CONVERT_TO_BYTEPTR(reference_data16_);
      second_pred_ = CONVERT_TO_BYTEPTR(second_pred16_);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    }
    mask_ = (1 << bit_depth_) - 1;
    source_stride_ = (params_.width + 31) & ~31;
    reference_stride_ = params_.width * 2;
    rnd_.Reset(ACMRandom::DeterministicSeed());
  }

  virtual void TearDown() {
    vpx_free(source_data8_);
    source_data8_ = NULL;
    vpx_free(reference_data8_);
    reference_data8_ = NULL;
    vpx_free(second_pred8_);
    second_pred8_ = NULL;
    vpx_free(source_data16_);
    source_data16_ = NULL;
    vpx_free(reference_data16_);
    reference_data16_ = NULL;
    vpx_free(second_pred16_);
    second_pred16_ = NULL;

    libvpx_test::ClearSystemState();
  }

 protected:
  // Handle blocks up to 4 blocks 64x64 with stride up to 128
  static const int kDataAlignment = 16;
  static const int kDataBlockSize = 64 * 128;
  static const int kDataBufferSize = 4 * kDataBlockSize;

  uint8_t *GetReference(int block_idx) const {
#if CONFIG_VP9_HIGHBITDEPTH
    if (use_high_bit_depth_)
      return CONVERT_TO_BYTEPTR(CONVERT_TO_SHORTPTR(reference_data_) +
                                block_idx * kDataBlockSize);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    return reference_data_ + block_idx * kDataBlockSize;
  }

  // Sum of Absolute Differences. Given two blocks, calculate the absolute
  // difference between two pixels in the same relative location; accumulate.
  uint32_t ReferenceSAD(int block_idx) const {
    uint32_t sad = 0;
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const source8 = source_data_;
#if CONFIG_VP9_HIGHBITDEPTH
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const source16 = CONVERT_TO_SHORTPTR(source_data_);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    for (int h = 0; h < params_.height; ++h) {
      for (int w = 0; w < params_.width; ++w) {
        if (!use_high_bit_depth_) {
          sad += abs(source8[h * source_stride_ + w] -
                     reference8[h * reference_stride_ + w]);
#if CONFIG_VP9_HIGHBITDEPTH
        } else {
          sad += abs(source16[h * source_stride_ + w] -
                     reference16[h * reference_stride_ + w]);
#endif  // CONFIG_VP9_HIGHBITDEPTH
        }
      }
    }
    return sad;
  }

  // Sum of Absolute Differences Average. Given two blocks, and a prediction
  // calculate the absolute difference between one pixel and average of the
  // corresponding and predicted pixels; accumulate.
  unsigned int ReferenceSADavg(int block_idx) const {
    unsigned int sad = 0;
    const uint8_t *const reference8 = GetReference(block_idx);
    const uint8_t *const source8 = source_data_;
    const uint8_t *const second_pred8 = second_pred_;
#if CONFIG_VP9_HIGHBITDEPTH
    const uint16_t *const reference16 =
        CONVERT_TO_SHORTPTR(GetReference(block_idx));
    const uint16_t *const source16 = CONVERT_TO_SHORTPTR(source_data_);
    const uint16_t *const second_pred16 = CONVERT_TO_SHORTPTR(second_pred_);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    for (int h = 0; h < params_.height; ++h) {
      for (int w = 0; w < params_.width; ++w) {
        if (!use_high_bit_depth_) {
          const int tmp = second_pred8[h * params_.width + w] +
              reference8[h * reference_stride_ + w];
          const uint8_t comp_pred = ROUND_POWER_OF_TWO(tmp, 1);
          sad += abs(source8[h * source_stride_ + w] - comp_pred);
#if CONFIG_VP9_HIGHBITDEPTH
        } else {
          const int tmp = second_pred16[h * params_.width + w] +
              reference16[h * reference_stride_ + w];
          const uint16_t comp_pred = ROUND_POWER_OF_TWO(tmp, 1);
          sad += abs(source16[h * source_stride_ + w] - comp_pred);
#endif  // CONFIG_VP9_HIGHBITDEPTH
        }
      }
    }
    return sad;
  }

  void FillConstant(uint8_t *data, int stride, uint16_t fill_constant) const {
    uint8_t *data8 = data;
#if CONFIG_VP9_HIGHBITDEPTH
    uint16_t *data16 = CONVERT_TO_SHORTPTR(data);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    for (int h = 0; h < params_.height; ++h) {
      for (int w = 0; w < params_.width; ++w) {
        if (!use_high_bit_depth_) {
          data8[h * stride + w] = static_cast<uint8_t>(fill_constant);
#if CONFIG_VP9_HIGHBITDEPTH
        } else {
          data16[h * stride + w] = fill_constant;
#endif  // CONFIG_VP9_HIGHBITDEPTH
        }
      }
    }
  }

  void FillRandom(uint8_t *data, int stride) {
    uint8_t *data8 = data;
#if CONFIG_VP9_HIGHBITDEPTH
    uint16_t *data16 = CONVERT_TO_SHORTPTR(data);
#endif  // CONFIG_VP9_HIGHBITDEPTH
    for (int h = 0; h < params_.height; ++h) {
      for (int w = 0; w < params_.width; ++w) {
        if (!use_high_bit_depth_) {
          data8[h * stride + w] = rnd_.Rand8();
#if CONFIG_VP9_HIGHBITDEPTH
        } else {
          data16[h * stride + w] = rnd_.Rand16() & mask_;
#endif  // CONFIG_VP9_HIGHBITDEPTH
        }
      }
    }
  }

  uint32_t mask_;
  vpx_bit_depth_t bit_depth_;
  int source_stride_;
  int reference_stride_;
  bool use_high_bit_depth_;

  uint8_t *source_data_;
  uint8_t *reference_data_;
  uint8_t *second_pred_;
  uint8_t *source_data8_;
  uint8_t *reference_data8_;
  uint8_t *second_pred8_;
  uint16_t *source_data16_;
  uint16_t *reference_data16_;
  uint16_t *second_pred16_;

  ACMRandom rnd_;
  ParamType params_;
};

class SADx4Test : public SADTestBase<SadMxNx4Param> {
 public:
  SADx4Test() : SADTestBase(GetParam()) {}

 protected:
  void SADs(unsigned int *results) const {
    const uint8_t *references[] = { GetReference(0), GetReference(1),
                                    GetReference(2), GetReference(3) };

    ASM_REGISTER_STATE_CHECK(params_.func(source_data_, source_stride_,
                                          references, reference_stride_,
                                          results));
  }

  void CheckSADs() const {
    uint32_t reference_sad, exp_sad[4];

    SADs(exp_sad);
    for (int block = 0; block < 4; ++block) {
      reference_sad = ReferenceSAD(block);

      EXPECT_EQ(reference_sad, exp_sad[block]) << "block " << block;
    }
  }
};

class SADTest : public SADTestBase<SadMxNParam> {
 public:
  SADTest() : SADTestBase(GetParam()) {}

 protected:
  unsigned int SAD(int block_idx) const {
    unsigned int ret;
    const uint8_t *const reference = GetReference(block_idx);

    ASM_REGISTER_STATE_CHECK(ret = params_.func(source_data_, source_stride_,
                                                reference, reference_stride_));
    return ret;
  }

  void CheckSAD() const {
    const unsigned int reference_sad = ReferenceSAD(0);
    const unsigned int exp_sad = SAD(0);

    ASSERT_EQ(reference_sad, exp_sad);
  }
};

class SADavgTest : public SADTestBase<SadMxNAvgParam> {
 public:
  SADavgTest() : SADTestBase(GetParam()) {}

 protected:
  unsigned int SAD_avg(int block_idx) const {
    unsigned int ret;
    const uint8_t *const reference = GetReference(block_idx);

    ASM_REGISTER_STATE_CHECK(ret = params_.func(source_data_, source_stride_,
                                                reference, reference_stride_,
                                                second_pred_));
    return ret;
  }

  void CheckSAD() const {
    const unsigned int reference_sad = ReferenceSADavg(0);
    const unsigned int exp_sad = SAD_avg(0);

    ASSERT_EQ(reference_sad, exp_sad);
  }
};

TEST_P(SADTest, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(reference_data_, reference_stride_, mask_);
  CheckSAD();
}

TEST_P(SADTest, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(reference_data_, reference_stride_, 0);
  CheckSAD();
}

TEST_P(SADTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADTest, ShortSrc) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  CheckSAD();
  source_stride_ = tmp_stride;
}

TEST_P(SADavgTest, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(reference_data_, reference_stride_, mask_);
  FillConstant(second_pred_, params_.width, 0);
  CheckSAD();
}
TEST_P(SADavgTest, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(reference_data_, reference_stride_, 0);
  FillConstant(second_pred_, params_.width, 0);
  CheckSAD();
}

TEST_P(SADavgTest, ShortRef) {
  const int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, params_.width);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADavgTest, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  const int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, params_.width);
  CheckSAD();
  reference_stride_ = tmp_stride;
}

TEST_P(SADavgTest, ShortSrc) {
  const int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(reference_data_, reference_stride_);
  FillRandom(second_pred_, params_.width);
  CheckSAD();
  source_stride_ = tmp_stride;
}

TEST_P(SADx4Test, MaxRef) {
  FillConstant(source_data_, source_stride_, 0);
  FillConstant(GetReference(0), reference_stride_, mask_);
  FillConstant(GetReference(1), reference_stride_, mask_);
  FillConstant(GetReference(2), reference_stride_, mask_);
  FillConstant(GetReference(3), reference_stride_, mask_);
  CheckSADs();
}

TEST_P(SADx4Test, MaxSrc) {
  FillConstant(source_data_, source_stride_, mask_);
  FillConstant(GetReference(0), reference_stride_, 0);
  FillConstant(GetReference(1), reference_stride_, 0);
  FillConstant(GetReference(2), reference_stride_, 0);
  FillConstant(GetReference(3), reference_stride_, 0);
  CheckSADs();
}

TEST_P(SADx4Test, ShortRef) {
  int tmp_stride = reference_stride_;
  reference_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  reference_stride_ = tmp_stride;
}

TEST_P(SADx4Test, UnalignedRef) {
  // The reference frame, but not the source frame, may be unaligned for
  // certain types of searches.
  int tmp_stride = reference_stride_;
  reference_stride_ -= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  reference_stride_ = tmp_stride;
}

TEST_P(SADx4Test, ShortSrc) {
  int tmp_stride = source_stride_;
  source_stride_ >>= 1;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  source_stride_ = tmp_stride;
}

TEST_P(SADx4Test, SrcAlignedByWidth) {
  uint8_t * tmp_source_data = source_data_;
  source_data_ += params_.width;
  FillRandom(source_data_, source_stride_);
  FillRandom(GetReference(0), reference_stride_);
  FillRandom(GetReference(1), reference_stride_);
  FillRandom(GetReference(2), reference_stride_);
  FillRandom(GetReference(3), reference_stride_);
  CheckSADs();
  source_data_ = tmp_source_data;
}

//------------------------------------------------------------------------------
// C functions
const SadMxNParam c_tests[] = {
  SadMxNParam(64, 64, &vpx_sad64x64_c),
  SadMxNParam(64, 32, &vpx_sad64x32_c),
  SadMxNParam(32, 64, &vpx_sad32x64_c),
  SadMxNParam(32, 32, &vpx_sad32x32_c),
  SadMxNParam(32, 16, &vpx_sad32x16_c),
  SadMxNParam(16, 32, &vpx_sad16x32_c),
  SadMxNParam(16, 16, &vpx_sad16x16_c),
  SadMxNParam(16, 8, &vpx_sad16x8_c),
  SadMxNParam(8, 16, &vpx_sad8x16_c),
  SadMxNParam(8, 8, &vpx_sad8x8_c),
  SadMxNParam(8, 4, &vpx_sad8x4_c),
  SadMxNParam(4, 8, &vpx_sad4x8_c),
  SadMxNParam(4, 4, &vpx_sad4x4_c),
#if CONFIG_VP9_HIGHBITDEPTH
  SadMxNParam(64, 64, &vpx_highbd_sad64x64_c, 8),
  SadMxNParam(64, 32, &vpx_highbd_sad64x32_c, 8),
  SadMxNParam(32, 64, &vpx_highbd_sad32x64_c, 8),
  SadMxNParam(32, 32, &vpx_highbd_sad32x32_c, 8),
  SadMxNParam(32, 16, &vpx_highbd_sad32x16_c, 8),
  SadMxNParam(16, 32, &vpx_highbd_sad16x32_c, 8),
  SadMxNParam(16, 16, &vpx_highbd_sad16x16_c, 8),
  SadMxNParam(16, 8, &vpx_highbd_sad16x8_c, 8),
  SadMxNParam(8, 16, &vpx_highbd_sad8x16_c, 8),
  SadMxNParam(8, 8, &vpx_highbd_sad8x8_c, 8),
  SadMxNParam(8, 4, &vpx_highbd_sad8x4_c, 8),
  SadMxNParam(4, 8, &vpx_highbd_sad4x8_c, 8),
  SadMxNParam(4, 4, &vpx_highbd_sad4x4_c, 8),
  SadMxNParam(64, 64, &vpx_highbd_sad64x64_c, 10),
  SadMxNParam(64, 32, &vpx_highbd_sad64x32_c, 10),
  SadMxNParam(32, 64, &vpx_highbd_sad32x64_c, 10),
  SadMxNParam(32, 32, &vpx_highbd_sad32x32_c, 10),
  SadMxNParam(32, 16, &vpx_highbd_sad32x16_c, 10),
  SadMxNParam(16, 32, &vpx_highbd_sad16x32_c, 10),
  SadMxNParam(16, 16, &vpx_highbd_sad16x16_c, 10),
  SadMxNParam(16, 8, &vpx_highbd_sad16x8_c, 10),
  SadMxNParam(8, 16, &vpx_highbd_sad8x16_c, 10),
  SadMxNParam(8, 8, &vpx_highbd_sad8x8_c, 10),
  SadMxNParam(8, 4, &vpx_highbd_sad8x4_c, 10),
  SadMxNParam(4, 8, &vpx_highbd_sad4x8_c, 10),
  SadMxNParam(4, 4, &vpx_highbd_sad4x4_c, 10),
  SadMxNParam(64, 64, &vpx_highbd_sad64x64_c, 12),
  SadMxNParam(64, 32, &vpx_highbd_sad64x32_c, 12),
  SadMxNParam(32, 64, &vpx_highbd_sad32x64_c, 12),
  SadMxNParam(32, 32, &vpx_highbd_sad32x32_c, 12),
  SadMxNParam(32, 16, &vpx_highbd_sad32x16_c, 12),
  SadMxNParam(16, 32, &vpx_highbd_sad16x32_c, 12),
  SadMxNParam(16, 16, &vpx_highbd_sad16x16_c, 12),
  SadMxNParam(16, 8, &vpx_highbd_sad16x8_c, 12),
  SadMxNParam(8, 16, &vpx_highbd_sad8x16_c, 12),
  SadMxNParam(8, 8, &vpx_highbd_sad8x8_c, 12),
  SadMxNParam(8, 4, &vpx_highbd_sad8x4_c, 12),
  SadMxNParam(4, 8, &vpx_highbd_sad4x8_c, 12),
  SadMxNParam(4, 4, &vpx_highbd_sad4x4_c, 12),
#endif  // CONFIG_VP9_HIGHBITDEPTH
};
INSTANTIATE_TEST_CASE_P(C, SADTest, ::testing::ValuesIn(c_tests));

const SadMxNAvgParam avg_c_tests[] = {
  SadMxNAvgParam(64, 64, &vpx_sad64x64_avg_c),
  SadMxNAvgParam(64, 32, &vpx_sad64x32_avg_c),
  SadMxNAvgParam(32, 64, &vpx_sad32x64_avg_c),
  SadMxNAvgParam(32, 32, &vpx_sad32x32_avg_c),
  SadMxNAvgParam(32, 16, &vpx_sad32x16_avg_c),
  SadMxNAvgParam(16, 32, &vpx_sad16x32_avg_c),
  SadMxNAvgParam(16, 16, &vpx_sad16x16_avg_c),
  SadMxNAvgParam(16, 8, &vpx_sad16x8_avg_c),
  SadMxNAvgParam(8, 16, &vpx_sad8x16_avg_c),
  SadMxNAvgParam(8, 8, &vpx_sad8x8_avg_c),
  SadMxNAvgParam(8, 4, &vpx_sad8x4_avg_c),
  SadMxNAvgParam(4, 8, &vpx_sad4x8_avg_c),
  SadMxNAvgParam(4, 4, &vpx_sad4x4_avg_c),
#if CONFIG_VP9_HIGHBITDEPTH
  SadMxNAvgParam(64, 64, &vpx_highbd_sad64x64_avg_c, 8),
  SadMxNAvgParam(64, 32, &vpx_highbd_sad64x32_avg_c, 8),
  SadMxNAvgParam(32, 64, &vpx_highbd_sad32x64_avg_c, 8),
  SadMxNAvgParam(32, 32, &vpx_highbd_sad32x32_avg_c, 8),
  SadMxNAvgParam(32, 16, &vpx_highbd_sad32x16_avg_c, 8),
  SadMxNAvgParam(16, 32, &vpx_highbd_sad16x32_avg_c, 8),
  SadMxNAvgParam(16, 16, &vpx_highbd_sad16x16_avg_c, 8),
  SadMxNAvgParam(16, 8, &vpx_highbd_sad16x8_avg_c, 8),
  SadMxNAvgParam(8, 16, &vpx_highbd_sad8x16_avg_c, 8),
  SadMxNAvgParam(8, 8, &vpx_highbd_sad8x8_avg_c, 8),
  SadMxNAvgParam(8, 4, &vpx_highbd_sad8x4_avg_c, 8),
  SadMxNAvgParam(4, 8, &vpx_highbd_sad4x8_avg_c, 8),
  SadMxNAvgParam(4, 4, &vpx_highbd_sad4x4_avg_c, 8),
  SadMxNAvgParam(64, 64, &vpx_highbd_sad64x64_avg_c, 10),
  SadMxNAvgParam(64, 32, &vpx_highbd_sad64x32_avg_c, 10),
  SadMxNAvgParam(32, 64, &vpx_highbd_sad32x64_avg_c, 10),
  SadMxNAvgParam(32, 32, &vpx_highbd_sad32x32_avg_c, 10),
  SadMxNAvgParam(32, 16, &vpx_highbd_sad32x16_avg_c, 10),
  SadMxNAvgParam(16, 32, &vpx_highbd_sad16x32_avg_c, 10),
  SadMxNAvgParam(16, 16, &vpx_highbd_sad16x16_avg_c, 10),
  SadMxNAvgParam(16, 8, &vpx_highbd_sad16x8_avg_c, 10),
  SadMxNAvgParam(8, 16, &vpx_highbd_sad8x16_avg_c, 10),
  SadMxNAvgParam(8, 8, &vpx_highbd_sad8x8_avg_c, 10),
  SadMxNAvgParam(8, 4, &vpx_highbd_sad8x4_avg_c, 10),
  SadMxNAvgParam(4, 8, &vpx_highbd_sad4x8_avg_c, 10),
  SadMxNAvgParam(4, 4, &vpx_highbd_sad4x4_avg_c, 10),
  SadMxNAvgParam(64, 64, &vpx_highbd_sad64x64_avg_c, 12),
  SadMxNAvgParam(64, 32, &vpx_highbd_sad64x32_avg_c, 12),
  SadMxNAvgParam(32, 64, &vpx_highbd_sad32x64_avg_c, 12),
  SadMxNAvgParam(32, 32, &vpx_highbd_sad32x32_avg_c, 12),
  SadMxNAvgParam(32, 16, &vpx_highbd_sad32x16_avg_c, 12),
  SadMxNAvgParam(16, 32, &vpx_highbd_sad16x32_avg_c, 12),
  SadMxNAvgParam(16, 16, &vpx_highbd_sad16x16_avg_c, 12),
  SadMxNAvgParam(16, 8, &vpx_highbd_sad16x8_avg_c, 12),
  SadMxNAvgParam(8, 16, &vpx_highbd_sad8x16_avg_c, 12),
  SadMxNAvgParam(8, 8, &vpx_highbd_sad8x8_avg_c, 12),
  SadMxNAvgParam(8, 4, &vpx_highbd_sad8x4_avg_c, 12),
  SadMxNAvgParam(4, 8, &vpx_highbd_sad4x8_avg_c, 12),
  SadMxNAvgParam(4, 4, &vpx_highbd_sad4x4_avg_c, 12),
#endif  // CONFIG_VP9_HIGHBITDEPTH
};
INSTANTIATE_TEST_CASE_P(C, SADavgTest, ::testing::ValuesIn(avg_c_tests));

const SadMxNx4Param x4d_c_tests[] = {
  SadMxNx4Param(64, 64, &vpx_sad64x64x4d_c),
  SadMxNx4Param(64, 32, &vpx_sad64x32x4d_c),
  SadMxNx4Param(32, 64, &vpx_sad32x64x4d_c),
  SadMxNx4Param(32, 32, &vpx_sad32x32x4d_c),
  SadMxNx4Param(32, 16, &vpx_sad32x16x4d_c),
  SadMxNx4Param(16, 32, &vpx_sad16x32x4d_c),
  SadMxNx4Param(16, 16, &vpx_sad16x16x4d_c),
  SadMxNx4Param(16, 8, &vpx_sad16x8x4d_c),
  SadMxNx4Param(8, 16, &vpx_sad8x16x4d_c),
  SadMxNx4Param(8, 8, &vpx_sad8x8x4d_c),
  SadMxNx4Param(8, 4, &vpx_sad8x4x4d_c),
  SadMxNx4Param(4, 8, &vpx_sad4x8x4d_c),
  SadMxNx4Param(4, 4, &vpx_sad4x4x4d_c),
#if CONFIG_VP9_HIGHBITDEPTH
  SadMxNx4Param(64, 64, &vpx_highbd_sad64x64x4d_c, 8),
  SadMxNx4Param(64, 32, &vpx_highbd_sad64x32x4d_c, 8),
  SadMxNx4Param(32, 64, &vpx_highbd_sad32x64x4d_c, 8),
  SadMxNx4Param(32, 32, &vpx_highbd_sad32x32x4d_c, 8),
  SadMxNx4Param(32, 16, &vpx_highbd_sad32x16x4d_c, 8),
  SadMxNx4Param(16, 32, &vpx_highbd_sad16x32x4d_c, 8),
  SadMxNx4Param(16, 16, &vpx_highbd_sad16x16x4d_c, 8),
  SadMxNx4Param(16, 8, &vpx_highbd_sad16x8x4d_c, 8),
  SadMxNx4Param(8, 16, &vpx_highbd_sad8x16x4d_c, 8),
  SadMxNx4Param(8, 8, &vpx_highbd_sad8x8x4d_c, 8),
  SadMxNx4Param(8, 4, &vpx_highbd_sad8x4x4d_c, 8),
  SadMxNx4Param(4, 8, &vpx_highbd_sad4x8x4d_c, 8),
  SadMxNx4Param(4, 4, &vpx_highbd_sad4x4x4d_c, 8),
  SadMxNx4Param(64, 64, &vpx_highbd_sad64x64x4d_c, 10),
  SadMxNx4Param(64, 32, &vpx_highbd_sad64x32x4d_c, 10),
  SadMxNx4Param(32, 64, &vpx_highbd_sad32x64x4d_c, 10),
  SadMxNx4Param(32, 32, &vpx_highbd_sad32x32x4d_c, 10),
  SadMxNx4Param(32, 16, &vpx_highbd_sad32x16x4d_c, 10),
  SadMxNx4Param(16, 32, &vpx_highbd_sad16x32x4d_c, 10),
  SadMxNx4Param(16, 16, &vpx_highbd_sad16x16x4d_c, 10),
  SadMxNx4Param(16, 8, &vpx_highbd_sad16x8x4d_c, 10),
  SadMxNx4Param(8, 16, &vpx_highbd_sad8x16x4d_c, 10),
  SadMxNx4Param(8, 8, &vpx_highbd_sad8x8x4d_c, 10),
  SadMxNx4Param(8, 4, &vpx_highbd_sad8x4x4d_c, 10),
  SadMxNx4Param(4, 8, &vpx_highbd_sad4x8x4d_c, 10),
  SadMxNx4Param(4, 4, &vpx_highbd_sad4x4x4d_c, 10),
  SadMxNx4Param(64, 64, &vpx_highbd_sad64x64x4d_c, 12),
  SadMxNx4Param(64, 32, &vpx_highbd_sad64x32x4d_c, 12),
  SadMxNx4Param(32, 64, &vpx_highbd_sad32x64x4d_c, 12),
  SadMxNx4Param(32, 32, &vpx_highbd_sad32x32x4d_c, 12),
  SadMxNx4Param(32, 16, &vpx_highbd_sad32x16x4d_c, 12),
  SadMxNx4Param(16, 32, &vpx_highbd_sad16x32x4d_c, 12),
  SadMxNx4Param(16, 16, &vpx_highbd_sad16x16x4d_c, 12),
  SadMxNx4Param(16, 8, &vpx_highbd_sad16x8x4d_c, 12),
  SadMxNx4Param(8, 16, &vpx_highbd_sad8x16x4d_c, 12),
  SadMxNx4Param(8, 8, &vpx_highbd_sad8x8x4d_c, 12),
  SadMxNx4Param(8, 4, &vpx_highbd_sad8x4x4d_c, 12),
  SadMxNx4Param(4, 8, &vpx_highbd_sad4x8x4d_c, 12),
  SadMxNx4Param(4, 4, &vpx_highbd_sad4x4x4d_c, 12),
#endif  // CONFIG_VP9_HIGHBITDEPTH
};
INSTANTIATE_TEST_CASE_P(C, SADx4Test, ::testing::ValuesIn(x4d_c_tests));

//------------------------------------------------------------------------------
// ARM functions
#if HAVE_MEDIA
const SadMxNParam media_tests[] = {
  SadMxNParam(16, 16, &vpx_sad16x16_media),
};
INSTANTIATE_TEST_CASE_P(MEDIA, SADTest, ::testing::ValuesIn(media_tests));
#endif  // HAVE_MEDIA

#if HAVE_NEON
const SadMxNParam neon_tests[] = {
  SadMxNParam(64, 64, &vpx_sad64x64_neon),
  SadMxNParam(32, 32, &vpx_sad32x32_neon),
  SadMxNParam(16, 16, &vpx_sad16x16_neon),
  SadMxNParam(16, 8, &vpx_sad16x8_neon),
  SadMxNParam(8, 16, &vpx_sad8x16_neon),
  SadMxNParam(8, 8, &vpx_sad8x8_neon),
  SadMxNParam(4, 4, &vpx_sad4x4_neon),
};
INSTANTIATE_TEST_CASE_P(NEON, SADTest, ::testing::ValuesIn(neon_tests));

const SadMxNx4Param x4d_neon_tests[] = {
  SadMxNx4Param(64, 64, &vpx_sad64x64x4d_neon),
  SadMxNx4Param(32, 32, &vpx_sad32x32x4d_neon),
  SadMxNx4Param(16, 16, &vpx_sad16x16x4d_neon),
};
INSTANTIATE_TEST_CASE_P(NEON, SADx4Test, ::testing::ValuesIn(x4d_neon_tests));
#endif  // HAVE_NEON

//------------------------------------------------------------------------------
// x86 functions
#if HAVE_SSE2
const SadMxNParam sse2_tests[] = {
  SadMxNParam(64, 64, &vpx_sad64x64_sse2),
  SadMxNParam(64, 32, &vpx_sad64x32_sse2),
  SadMxNParam(32, 64, &vpx_sad32x64_sse2),
  SadMxNParam(32, 32, &vpx_sad32x32_sse2),
  SadMxNParam(32, 16, &vpx_sad32x16_sse2),
  SadMxNParam(16, 32, &vpx_sad16x32_sse2),
  SadMxNParam(16, 16, &vpx_sad16x16_sse2),
  SadMxNParam(16, 8, &vpx_sad16x8_sse2),
  SadMxNParam(8, 16, &vpx_sad8x16_sse2),
  SadMxNParam(8, 8, &vpx_sad8x8_sse2),
  SadMxNParam(8, 4, &vpx_sad8x4_sse2),
  SadMxNParam(4, 8, &vpx_sad4x8_sse2),
  SadMxNParam(4, 4, &vpx_sad4x4_sse2),
#if CONFIG_VP9_HIGHBITDEPTH
  SadMxNParam(64, 64, &vpx_highbd_sad64x64_sse2, 8),
  SadMxNParam(64, 32, &vpx_highbd_sad64x32_sse2, 8),
  SadMxNParam(32, 64, &vpx_highbd_sad32x64_sse2, 8),
  SadMxNParam(32, 32, &vpx_highbd_sad32x32_sse2, 8),
  SadMxNParam(32, 16, &vpx_highbd_sad32x16_sse2, 8),
  SadMxNParam(16, 32, &vpx_highbd_sad16x32_sse2, 8),
  SadMxNParam(16, 16, &vpx_highbd_sad16x16_sse2, 8),
  SadMxNParam(16, 8, &vpx_highbd_sad16x8_sse2, 8),
  SadMxNParam(8, 16, &vpx_highbd_sad8x16_sse2, 8),
  SadMxNParam(8, 8, &vpx_highbd_sad8x8_sse2, 8),
  SadMxNParam(8, 4, &vpx_highbd_sad8x4_sse2, 8),
  SadMxNParam(64, 64, &vpx_highbd_sad64x64_sse2, 10),
  SadMxNParam(64, 32, &vpx_highbd_sad64x32_sse2, 10),
  SadMxNParam(32, 64, &vpx_highbd_sad32x64_sse2, 10),
  SadMxNParam(32, 32, &vpx_highbd_sad32x32_sse2, 10),
  SadMxNParam(32, 16, &vpx_highbd_sad32x16_sse2, 10),
  SadMxNParam(16, 32, &vpx_highbd_sad16x32_sse2, 10),
  SadMxNParam(16, 16, &vpx_highbd_sad16x16_sse2, 10),
  SadMxNParam(16, 8, &vpx_highbd_sad16x8_sse2, 10),
  SadMxNParam(8, 16, &vpx_highbd_sad8x16_sse2, 10),
  SadMxNParam(8, 8, &vpx_highbd_sad8x8_sse2, 10),
  SadMxNParam(8, 4, &vpx_highbd_sad8x4_sse2, 10),
  SadMxNParam(64, 64, &vpx_highbd_sad64x64_sse2, 12),
  SadMxNParam(64, 32, &vpx_highbd_sad64x32_sse2, 12),
  SadMxNParam(32, 64, &vpx_highbd_sad32x64_sse2, 12),
  SadMxNParam(32, 32, &vpx_highbd_sad32x32_sse2, 12),
  SadMxNParam(32, 16, &vpx_highbd_sad32x16_sse2, 12),
  SadMxNParam(16, 32, &vpx_highbd_sad16x32_sse2, 12),
  SadMxNParam(16, 16, &vpx_highbd_sad16x16_sse2, 12),
  SadMxNParam(16, 8, &vpx_highbd_sad16x8_sse2, 12),
  SadMxNParam(8, 16, &vpx_highbd_sad8x16_sse2, 12),
  SadMxNParam(8, 8, &vpx_highbd_sad8x8_sse2, 12),
  SadMxNParam(8, 4, &vpx_highbd_sad8x4_sse2, 12),
#endif  // CONFIG_VP9_HIGHBITDEPTH
};
INSTANTIATE_TEST_CASE_P(SSE2, SADTest, ::testing::ValuesIn(sse2_tests));

const SadMxNAvgParam avg_sse2_tests[] = {
  SadMxNAvgParam(64, 64, &vpx_sad64x64_avg_sse2),
  SadMxNAvgParam(64, 32, &vpx_sad64x32_avg_sse2),
  SadMxNAvgParam(32, 64, &vpx_sad32x64_avg_sse2),
  SadMxNAvgParam(32, 32, &vpx_sad32x32_avg_sse2),
  SadMxNAvgParam(32, 16, &vpx_sad32x16_avg_sse2),
  SadMxNAvgParam(16, 32, &vpx_sad16x32_avg_sse2),
  SadMxNAvgParam(16, 16, &vpx_sad16x16_avg_sse2),
  SadMxNAvgParam(16, 8, &vpx_sad16x8_avg_sse2),
  SadMxNAvgParam(8, 16, &vpx_sad8x16_avg_sse2),
  SadMxNAvgParam(8, 8, &vpx_sad8x8_avg_sse2),
  SadMxNAvgParam(8, 4, &vpx_sad8x4_avg_sse2),
  SadMxNAvgParam(4, 8, &vpx_sad4x8_avg_sse2),
  SadMxNAvgParam(4, 4, &vpx_sad4x4_avg_sse2),
#if CONFIG_VP9_HIGHBITDEPTH
  SadMxNAvgParam(64, 64, &vpx_highbd_sad64x64_avg_sse2, 8),
  SadMxNAvgParam(64, 32, &vpx_highbd_sad64x32_avg_sse2, 8),
  SadMxNAvgParam(32, 64, &vpx_highbd_sad32x64_avg_sse2, 8),
  SadMxNAvgParam(32, 32, &vpx_highbd_sad32x32_avg_sse2, 8),
  SadMxNAvgParam(32, 16, &vpx_highbd_sad32x16_avg_sse2, 8),
  SadMxNAvgParam(16, 32, &vpx_highbd_sad16x32_avg_sse2, 8),
  SadMxNAvgParam(16, 16, &vpx_highbd_sad16x16_avg_sse2, 8),
  SadMxNAvgParam(16, 8, &vpx_highbd_sad16x8_avg_sse2, 8),
  SadMxNAvgParam(8, 16, &vpx_highbd_sad8x16_avg_sse2, 8),
  SadMxNAvgParam(8, 8, &vpx_highbd_sad8x8_avg_sse2, 8),
  SadMxNAvgParam(8, 4, &vpx_highbd_sad8x4_avg_sse2, 8),
  SadMxNAvgParam(64, 64, &vpx_highbd_sad64x64_avg_sse2, 10),
  SadMxNAvgParam(64, 32, &vpx_highbd_sad64x32_avg_sse2, 10),
  SadMxNAvgParam(32, 64, &vpx_highbd_sad32x64_avg_sse2, 10),
  SadMxNAvgParam(32, 32, &vpx_highbd_sad32x32_avg_sse2, 10),
  SadMxNAvgParam(32, 16, &vpx_highbd_sad32x16_avg_sse2, 10),
  SadMxNAvgParam(16, 32, &vpx_highbd_sad16x32_avg_sse2, 10),
  SadMxNAvgParam(16, 16, &vpx_highbd_sad16x16_avg_sse2, 10),
  SadMxNAvgParam(16, 8, &vpx_highbd_sad16x8_avg_sse2, 10),
  SadMxNAvgParam(8, 16, &vpx_highbd_sad8x16_avg_sse2, 10),
  SadMxNAvgParam(8, 8, &vpx_highbd_sad8x8_avg_sse2, 10),
  SadMxNAvgParam(8, 4, &vpx_highbd_sad8x4_avg_sse2, 10),
  SadMxNAvgParam(64, 64, &vpx_highbd_sad64x64_avg_sse2, 12),
  SadMxNAvgParam(64, 32, &vpx_highbd_sad64x32_avg_sse2, 12),
  SadMxNAvgParam(32, 64, &vpx_highbd_sad32x64_avg_sse2, 12),
  SadMxNAvgParam(32, 32, &vpx_highbd_sad32x32_avg_sse2, 12),
  SadMxNAvgParam(32, 16, &vpx_highbd_sad32x16_avg_sse2, 12),
  SadMxNAvgParam(16, 32, &vpx_highbd_sad16x32_avg_sse2, 12),
  SadMxNAvgParam(16, 16, &vpx_highbd_sad16x16_avg_sse2, 12),
  SadMxNAvgParam(16, 8, &vpx_highbd_sad16x8_avg_sse2, 12),
  SadMxNAvgParam(8, 16, &vpx_highbd_sad8x16_avg_sse2, 12),
  SadMxNAvgParam(8, 8, &vpx_highbd_sad8x8_avg_sse2, 12),
  SadMxNAvgParam(8, 4, &vpx_highbd_sad8x4_avg_sse2, 12),
#endif  // CONFIG_VP9_HIGHBITDEPTH
};
INSTANTIATE_TEST_CASE_P(SSE2, SADavgTest, ::testing::ValuesIn(avg_sse2_tests));

const SadMxNx4Param x4d_sse2_tests[] = {
  SadMxNx4Param(64, 64, &vpx_sad64x64x4d_sse2),
  SadMxNx4Param(64, 32, &vpx_sad64x32x4d_sse2),
  SadMxNx4Param(32, 64, &vpx_sad32x64x4d_sse2),
  SadMxNx4Param(32, 32, &vpx_sad32x32x4d_sse2),
  SadMxNx4Param(32, 16, &vpx_sad32x16x4d_sse2),
  SadMxNx4Param(16, 32, &vpx_sad16x32x4d_sse2),
  SadMxNx4Param(16, 16, &vpx_sad16x16x4d_sse2),
  SadMxNx4Param(16, 8, &vpx_sad16x8x4d_sse2),
  SadMxNx4Param(8, 16, &vpx_sad8x16x4d_sse2),
  SadMxNx4Param(8, 8, &vpx_sad8x8x4d_sse2),
  SadMxNx4Param(8, 4, &vpx_sad8x4x4d_sse2),
  SadMxNx4Param(4, 8, &vpx_sad4x8x4d_sse2),
  SadMxNx4Param(4, 4, &vpx_sad4x4x4d_sse2),
#if CONFIG_VP9_HIGHBITDEPTH
  SadMxNx4Param(64, 64, &vpx_highbd_sad64x64x4d_sse2, 8),
  SadMxNx4Param(64, 32, &vpx_highbd_sad64x32x4d_sse2, 8),
  SadMxNx4Param(32, 64, &vpx_highbd_sad32x64x4d_sse2, 8),
  SadMxNx4Param(32, 32, &vpx_highbd_sad32x32x4d_sse2, 8),
  SadMxNx4Param(32, 16, &vpx_highbd_sad32x16x4d_sse2, 8),
  SadMxNx4Param(16, 32, &vpx_highbd_sad16x32x4d_sse2, 8),
  SadMxNx4Param(16, 16, &vpx_highbd_sad16x16x4d_sse2, 8),
  SadMxNx4Param(16, 8, &vpx_highbd_sad16x8x4d_sse2, 8),
  SadMxNx4Param(8, 16, &vpx_highbd_sad8x16x4d_sse2, 8),
  SadMxNx4Param(8, 8, &vpx_highbd_sad8x8x4d_sse2, 8),
  SadMxNx4Param(8, 4, &vpx_highbd_sad8x4x4d_sse2, 8),
  SadMxNx4Param(4, 8, &vpx_highbd_sad4x8x4d_sse2, 8),
  SadMxNx4Param(4, 4, &vpx_highbd_sad4x4x4d_sse2, 8),
  SadMxNx4Param(64, 64, &vpx_highbd_sad64x64x4d_sse2, 10),
  SadMxNx4Param(64, 32, &vpx_highbd_sad64x32x4d_sse2, 10),
  SadMxNx4Param(32, 64, &vpx_highbd_sad32x64x4d_sse2, 10),
  SadMxNx4Param(32, 32, &vpx_highbd_sad32x32x4d_sse2, 10),
  SadMxNx4Param(32, 16, &vpx_highbd_sad32x16x4d_sse2, 10),
  SadMxNx4Param(16, 32, &vpx_highbd_sad16x32x4d_sse2, 10),
  SadMxNx4Param(16, 16, &vpx_highbd_sad16x16x4d_sse2, 10),
  SadMxNx4Param(16, 8, &vpx_highbd_sad16x8x4d_sse2, 10),
  SadMxNx4Param(8, 16, &vpx_highbd_sad8x16x4d_sse2, 10),
  SadMxNx4Param(8, 8, &vpx_highbd_sad8x8x4d_sse2, 10),
  SadMxNx4Param(8, 4, &vpx_highbd_sad8x4x4d_sse2, 10),
  SadMxNx4Param(4, 8, &vpx_highbd_sad4x8x4d_sse2, 10),
  SadMxNx4Param(4, 4, &vpx_highbd_sad4x4x4d_sse2, 10),
  SadMxNx4Param(64, 64, &vpx_highbd_sad64x64x4d_sse2, 12),
  SadMxNx4Param(64, 32, &vpx_highbd_sad64x32x4d_sse2, 12),
  SadMxNx4Param(32, 64, &vpx_highbd_sad32x64x4d_sse2, 12),
  SadMxNx4Param(32, 32, &vpx_highbd_sad32x32x4d_sse2, 12),
  SadMxNx4Param(32, 16, &vpx_highbd_sad32x16x4d_sse2, 12),
  SadMxNx4Param(16, 32, &vpx_highbd_sad16x32x4d_sse2, 12),
  SadMxNx4Param(16, 16, &vpx_highbd_sad16x16x4d_sse2, 12),
  SadMxNx4Param(16, 8, &vpx_highbd_sad16x8x4d_sse2, 12),
  SadMxNx4Param(8, 16, &vpx_highbd_sad8x16x4d_sse2, 12),
  SadMxNx4Param(8, 8, &vpx_highbd_sad8x8x4d_sse2, 12),
  SadMxNx4Param(8, 4, &vpx_highbd_sad8x4x4d_sse2, 12),
  SadMxNx4Param(4, 8, &vpx_highbd_sad4x8x4d_sse2, 12),
  SadMxNx4Param(4, 4, &vpx_highbd_sad4x4x4d_sse2, 12),
#endif  // CONFIG_VP9_HIGHBITDEPTH
};
INSTANTIATE_TEST_CASE_P(SSE2, SADx4Test, ::testing::ValuesIn(x4d_sse2_tests));
#endif  // HAVE_SSE2

#if HAVE_SSE3
// Only functions are x3, which do not have tests.
#endif  // HAVE_SSE3

#if HAVE_SSSE3
// Only functions are x3, which do not have tests.
#endif  // HAVE_SSSE3

#if HAVE_SSE4_1
// Only functions are x8, which do not have tests.
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
const SadMxNParam avx2_tests[] = {
  SadMxNParam(64, 64, &vpx_sad64x64_avx2),
  SadMxNParam(64, 32, &vpx_sad64x32_avx2),
  SadMxNParam(32, 64, &vpx_sad32x64_avx2),
  SadMxNParam(32, 32, &vpx_sad32x32_avx2),
  SadMxNParam(32, 16, &vpx_sad32x16_avx2),
};
INSTANTIATE_TEST_CASE_P(AVX2, SADTest, ::testing::ValuesIn(avx2_tests));

const SadMxNAvgParam avg_avx2_tests[] = {
  SadMxNAvgParam(64, 64, &vpx_sad64x64_avg_avx2),
  SadMxNAvgParam(64, 32, &vpx_sad64x32_avg_avx2),
  SadMxNAvgParam(32, 64, &vpx_sad32x64_avg_avx2),
  SadMxNAvgParam(32, 32, &vpx_sad32x32_avg_avx2),
  SadMxNAvgParam(32, 16, &vpx_sad32x16_avg_avx2),
};
INSTANTIATE_TEST_CASE_P(AVX2, SADavgTest, ::testing::ValuesIn(avg_avx2_tests));

const SadMxNx4Param x4d_avx2_tests[] = {
  SadMxNx4Param(64, 64, &vpx_sad64x64x4d_avx2),
  SadMxNx4Param(32, 32, &vpx_sad32x32x4d_avx2),
};
INSTANTIATE_TEST_CASE_P(AVX2, SADx4Test, ::testing::ValuesIn(x4d_avx2_tests));
#endif  // HAVE_AVX2

//------------------------------------------------------------------------------
// MIPS functions
#if HAVE_MSA
const SadMxNParam msa_tests[] = {
  SadMxNParam(64, 64, &vpx_sad64x64_msa),
  SadMxNParam(64, 32, &vpx_sad64x32_msa),
  SadMxNParam(32, 64, &vpx_sad32x64_msa),
  SadMxNParam(32, 32, &vpx_sad32x32_msa),
  SadMxNParam(32, 16, &vpx_sad32x16_msa),
  SadMxNParam(16, 32, &vpx_sad16x32_msa),
  SadMxNParam(16, 16, &vpx_sad16x16_msa),
  SadMxNParam(16, 8, &vpx_sad16x8_msa),
  SadMxNParam(8, 16, &vpx_sad8x16_msa),
  SadMxNParam(8, 8, &vpx_sad8x8_msa),
  SadMxNParam(8, 4, &vpx_sad8x4_msa),
  SadMxNParam(4, 8, &vpx_sad4x8_msa),
  SadMxNParam(4, 4, &vpx_sad4x4_msa),
};
INSTANTIATE_TEST_CASE_P(MSA, SADTest, ::testing::ValuesIn(msa_tests));

const SadMxNAvgParam avg_msa_tests[] = {
  SadMxNAvgParam(64, 64, &vpx_sad64x64_avg_msa),
  SadMxNAvgParam(64, 32, &vpx_sad64x32_avg_msa),
  SadMxNAvgParam(32, 64, &vpx_sad32x64_avg_msa),
  SadMxNAvgParam(32, 32, &vpx_sad32x32_avg_msa),
  SadMxNAvgParam(32, 16, &vpx_sad32x16_avg_msa),
  SadMxNAvgParam(16, 32, &vpx_sad16x32_avg_msa),
  SadMxNAvgParam(16, 16, &vpx_sad16x16_avg_msa),
  SadMxNAvgParam(16, 8, &vpx_sad16x8_avg_msa),
  SadMxNAvgParam(8, 16, &vpx_sad8x16_avg_msa),
  SadMxNAvgParam(8, 8, &vpx_sad8x8_avg_msa),
  SadMxNAvgParam(8, 4, &vpx_sad8x4_avg_msa),
  SadMxNAvgParam(4, 8, &vpx_sad4x8_avg_msa),
  SadMxNAvgParam(4, 4, &vpx_sad4x4_avg_msa),
};
INSTANTIATE_TEST_CASE_P(MSA, SADavgTest, ::testing::ValuesIn(avg_msa_tests));

const SadMxNx4Param x4d_msa_tests[] = {
  SadMxNx4Param(64, 64, &vpx_sad64x64x4d_msa),
  SadMxNx4Param(64, 32, &vpx_sad64x32x4d_msa),
  SadMxNx4Param(32, 64, &vpx_sad32x64x4d_msa),
  SadMxNx4Param(32, 32, &vpx_sad32x32x4d_msa),
  SadMxNx4Param(32, 16, &vpx_sad32x16x4d_msa),
  SadMxNx4Param(16, 32, &vpx_sad16x32x4d_msa),
  SadMxNx4Param(16, 16, &vpx_sad16x16x4d_msa),
  SadMxNx4Param(16, 8, &vpx_sad16x8x4d_msa),
  SadMxNx4Param(8, 16, &vpx_sad8x16x4d_msa),
  SadMxNx4Param(8, 8, &vpx_sad8x8x4d_msa),
  SadMxNx4Param(8, 4, &vpx_sad8x4x4d_msa),
  SadMxNx4Param(4, 8, &vpx_sad4x8x4d_msa),
  SadMxNx4Param(4, 4, &vpx_sad4x4x4d_msa),
};
INSTANTIATE_TEST_CASE_P(MSA, SADx4Test, ::testing::ValuesIn(x4d_msa_tests));
#endif  // HAVE_MSA

}  // namespace

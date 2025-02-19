// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/base/demuxer_stream_adapter.h"
#include "chromecast/media/cma/base/demuxer_stream_for_test.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {
// Maximum pts diff between frames
const int kMaxPtsDiffMs = 2000;
}  // namespace

// Test for multiple streams
class MultiDemuxerStreamAdaptersTest : public testing::Test {
 public:
  MultiDemuxerStreamAdaptersTest();
  ~MultiDemuxerStreamAdaptersTest() override;

  void Start();

 protected:
  void OnTestTimeout();
  void OnNewFrame(CodedFrameProvider* frame_provider,
                  const scoped_refptr<DecoderBufferBase>& buffer,
                  const ::media::AudioDecoderConfig& audio_config,
                  const ::media::VideoDecoderConfig& video_config);

  // Number of expected read frames.
  int total_expected_frames_;

  // Number of frames actually read so far.
  int frame_received_count_;

  // List of expected frame indices with decoder config changes.
  std::list<int> config_idx_;

  ScopedVector<DemuxerStreamForTest> demuxer_streams_;

  ScopedVector<CodedFrameProvider> coded_frame_providers_;

 private:
  // exit if all of the streams end
  void OnEos();

  // Number of reading-streams
  int running_stream_count_;

  scoped_refptr<BalancedMediaTaskRunnerFactory> media_task_runner_factory_;
  DISALLOW_COPY_AND_ASSIGN(MultiDemuxerStreamAdaptersTest);
};

MultiDemuxerStreamAdaptersTest::MultiDemuxerStreamAdaptersTest() {
}

MultiDemuxerStreamAdaptersTest::~MultiDemuxerStreamAdaptersTest() {
}

void MultiDemuxerStreamAdaptersTest::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MultiDemuxerStreamAdaptersTest::OnTestTimeout,
                 base::Unretained(this)),
      base::TimeDelta::FromSeconds(5));

  media_task_runner_factory_ = new BalancedMediaTaskRunnerFactory(
      base::TimeDelta::FromMilliseconds(kMaxPtsDiffMs));

  coded_frame_providers_.clear();
  frame_received_count_ = 0;

  for (auto* stream : demuxer_streams_) {
    coded_frame_providers_.push_back(base::WrapUnique(
        new DemuxerStreamAdapter(base::ThreadTaskRunnerHandle::Get(),
                                 media_task_runner_factory_, stream)));
  }
  running_stream_count_ = coded_frame_providers_.size();

  // read each stream
  for (auto* code_frame_provider : coded_frame_providers_) {
    auto read_cb = base::Bind(&MultiDemuxerStreamAdaptersTest::OnNewFrame,
                              base::Unretained(this),
                              code_frame_provider);

    base::Closure task = base::Bind(&CodedFrameProvider::Read,
                                    base::Unretained(code_frame_provider),
                                    read_cb);

    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, task);
  }
}

void MultiDemuxerStreamAdaptersTest::OnTestTimeout() {
  if (running_stream_count_ != 0) {
    ADD_FAILURE() << "Test timed out";
  }
}

void MultiDemuxerStreamAdaptersTest::OnNewFrame(
    CodedFrameProvider* frame_provider,
    const scoped_refptr<DecoderBufferBase>& buffer,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  if (buffer->end_of_stream()) {
    OnEos();
    return;
  }

  frame_received_count_++;
  auto read_cb = base::Bind(&MultiDemuxerStreamAdaptersTest::OnNewFrame,
                            base::Unretained(this),
                            frame_provider);
  frame_provider->Read(read_cb);
}

void MultiDemuxerStreamAdaptersTest::OnEos() {
  running_stream_count_--;
  ASSERT_GE(running_stream_count_, 0);
  if (running_stream_count_ == 0) {
    ASSERT_EQ(frame_received_count_, total_expected_frames_);
    base::MessageLoop::current()->QuitWhenIdle();
  }
}

TEST_F(MultiDemuxerStreamAdaptersTest, EarlyEos) {
  // We have more than one streams here. One of them is much shorter than the
  // others. When the shortest stream reaches EOS, other streams should still
  // run as usually. BalancedTaskRunner should not be blocked.
  int frame_count_short = 2;
  int frame_count_long =
      frame_count_short +
      kMaxPtsDiffMs / DemuxerStreamForTest::kDemuxerStreamForTestFrameDuration +
      100;
  demuxer_streams_.push_back(std::unique_ptr<DemuxerStreamForTest>(
      new DemuxerStreamForTest(frame_count_short, 2, 0, config_idx_)));
  demuxer_streams_.push_back(std::unique_ptr<DemuxerStreamForTest>(
      new DemuxerStreamForTest(frame_count_long, 10, 0, config_idx_)));

  total_expected_frames_ = frame_count_short + frame_count_long;

  std::unique_ptr<base::MessageLoop> message_loop(new base::MessageLoop());
  message_loop->PostTask(FROM_HERE,
                         base::Bind(&MultiDemuxerStreamAdaptersTest::Start,
                                    base::Unretained(this)));
  message_loop->Run();
}
}  // namespace media
}  // namespace chromecast

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_COMMON_PIPELINE_STATISTICS_STRUCT_TRAITS_H_
#define MEDIA_MOJO_COMMON_PIPELINE_STATISTICS_STRUCT_TRAITS_H_

#include "media/base/pipeline_status.h"
#include "media/mojo/interfaces/media_types.mojom.h"

namespace mojo {

template <>
struct StructTraits<media::mojom::PipelineStatistics,
                    media::PipelineStatistics> {
  static uint64_t audio_bytes_decoded(const media::PipelineStatistics& input) {
    return input.audio_bytes_decoded;
  }
  static uint64_t video_bytes_decoded(const media::PipelineStatistics& input) {
    return input.video_bytes_decoded;
  }
  static uint32_t video_frames_decoded(const media::PipelineStatistics& input) {
    return input.video_frames_decoded;
  }
  static uint32_t video_frames_dropped(const media::PipelineStatistics& input) {
    return input.video_frames_dropped;
  }
  static int64_t audio_memory_usage(const media::PipelineStatistics& input) {
    return input.audio_memory_usage;
  }
  static int64_t video_memory_usage(const media::PipelineStatistics& input) {
    return input.video_memory_usage;
  }

  static bool Read(media::mojom::PipelineStatisticsDataView data,
                   media::PipelineStatistics* output) {
    output->audio_bytes_decoded = data.audio_bytes_decoded();
    output->video_bytes_decoded = data.video_bytes_decoded();
    output->video_frames_decoded = data.video_frames_decoded();
    output->video_frames_dropped = data.video_frames_dropped();
    output->audio_memory_usage = data.audio_memory_usage();
    output->video_memory_usage = data.video_memory_usage();
    return true;
  }
};

}  // namespace mojo

#endif  // MEDIA_MOJO_COMMON_PIPELINE_STATISTICS_STRUCT_TRAITS_H_
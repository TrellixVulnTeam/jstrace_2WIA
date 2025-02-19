// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MEDIA_FOUNDATION_VIDEO_ENCODE_ACCELERATOR_WIN_H_
#define MEDIA_GPU_MEDIA_FOUNDATION_VIDEO_ENCODE_ACCELERATOR_WIN_H_

#include <mfapi.h>
#include <mfidl.h>
#include <stdint.h>
#include <strmif.h>

#include <deque>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/win/scoped_comptr.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

// Media Foundation implementation of the VideoEncodeAccelerator interface for
// Windows.
// This class saves the task runner on which it is constructed and returns
// encoded data to the client using that same task runner. This class has
// DCHECKs to makes sure that methods are called in sequence. It starts an
// internal encoder thread on which VideoEncodeAccelerator implementation tasks
// are posted.
class MEDIA_GPU_EXPORT MediaFoundationVideoEncodeAccelerator
    : public VideoEncodeAccelerator {
 public:
  MediaFoundationVideoEncodeAccelerator();

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(VideoPixelFormat input_format,
                  const gfx::Size& input_visible_size,
                  VideoCodecProfile output_profile,
                  uint32_t initial_bitrate,
                  Client* client) override;
  void Encode(const scoped_refptr<VideoFrame>& frame,
              bool force_keyframe) override;
  void UseOutputBitstreamBuffer(const BitstreamBuffer& buffer) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void Destroy() override;

  // Preload dlls required for encoding.
  static void PreSandboxInitialization();

 protected:
  ~MediaFoundationVideoEncodeAccelerator() override;

 private:
  // Holds output buffers coming from the client ready to be filled.
  struct BitstreamBufferRef;

  // Holds output buffers coming from the encoder.
  class EncodeOutput;

  // Creates an hardware encoder backed IMFTransform instance on |encoder_|.
  bool CreateHardwareEncoderMFT();

  // Initializes and allocates memory for input and output samples.
  bool InitializeInputOutputSamples();

  // Initializes encoder parameters for real-time use.
  bool SetEncoderModes();

  // Helper function to notify the client of an error on |client_task_runner_|.
  void NotifyError(VideoEncodeAccelerator::Error error);

  // Encoding tasks to be run on |encoder_thread_|.
  void EncodeTask(const scoped_refptr<VideoFrame>& frame, bool force_keyframe);

  // Checks for and copies encoded output on |encoder_thread_|.
  void ProcessOutput();

  // Inserts the output buffers for reuse on |encoder_thread_|.
  void UseOutputBitstreamBufferTask(
      std::unique_ptr<BitstreamBufferRef> buffer_ref);

  // Copies EncodeOutput into a BitstreamBuffer and returns it to the |client_|.
  void ReturnBitstreamBuffer(
      std::unique_ptr<EncodeOutput> encode_output,
      std::unique_ptr<MediaFoundationVideoEncodeAccelerator::BitstreamBufferRef>
          buffer_ref);

  // Changes encode parameters on |encoder_thread_|.
  void RequestEncodingParametersChangeTask(uint32_t bitrate,
                                           uint32_t framerate);

  // Destroys encode session on |encoder_thread_|.
  void DestroyTask();

  // Bitstream buffers ready to be used to return encoded output as a FIFO.
  std::deque<std::unique_ptr<BitstreamBufferRef>> bitstream_buffer_queue_;

  // EncodeOutput needs to be copied into a BitstreamBufferRef as a FIFO.
  std::deque<std::unique_ptr<EncodeOutput>> encoder_output_queue_;

  gfx::Size input_visible_size_;
  size_t bitstream_buffer_size_;
  int32_t frame_rate_;
  int32_t target_bitrate_;
  size_t u_plane_offset_;
  size_t v_plane_offset_;

  base::win::ScopedComPtr<IMFTransform> encoder_;
  base::win::ScopedComPtr<ICodecAPI> codec_api_;

  DWORD input_stream_count_min_;
  DWORD input_stream_count_max_;
  DWORD output_stream_count_min_;
  DWORD output_stream_count_max_;

  base::win::ScopedComPtr<IMFSample> input_sample_;
  base::win::ScopedComPtr<IMFSample> output_sample_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to this object *MUST* be executed on |client_task_runner_|.
  base::WeakPtr<Client> client_;
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;

  // Our original calling task runner for the child thread.
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  // Sequence checker to enforce that the methods of this object are called in
  // sequence.
  base::SequenceChecker sequence_checker_;

  // This thread services tasks posted from the VEA API entry points by the
  // GPU child thread and CompressionCallback() posted from device thread.
  base::Thread encoder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_thread_task_runner_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtrFactory<MediaFoundationVideoEncodeAccelerator>
      encoder_task_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaFoundationVideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_MEDIA_FOUNDATION_VIDEO_ENCODE_ACCELERATOR_WIN_H_
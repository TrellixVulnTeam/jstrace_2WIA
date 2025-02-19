// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/video_capture_device_android.h"

#include <stdint.h>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jni/VideoCapture_jni.h"
#include "media/capture/video/android/photo_capabilities.h"
#include "media/capture/video/android/video_capture_device_factory_android.h"
#include "third_party/libyuv/include/libyuv.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::GetClass;
using base::android::JavaParamRef;
using base::android::MethodID;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace media {

// static
bool VideoCaptureDeviceAndroid::RegisterVideoCaptureDevice(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

VideoCaptureDeviceAndroid::VideoCaptureDeviceAndroid(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      state_(kIdle),
      got_first_frame_(false),
      device_descriptor_(device_descriptor),
      weak_ptr_factory_(this) {}

VideoCaptureDeviceAndroid::~VideoCaptureDeviceAndroid() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  StopAndDeAllocate();
}

bool VideoCaptureDeviceAndroid::Init() {
  int id;
  if (!base::StringToInt(device_descriptor_.device_id, &id))
    return false;

  j_capture_.Reset(VideoCaptureDeviceFactoryAndroid::createVideoCaptureAndroid(
      id, reinterpret_cast<intptr_t>(this)));
  return true;
}

void VideoCaptureDeviceAndroid::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(lock_);
    if (state_ != kIdle)
      return;
    client_ = std::move(client);
    got_first_frame_ = false;
  }

  JNIEnv* env = AttachCurrentThread();

  jboolean ret = Java_VideoCapture_allocate(
      env, j_capture_.obj(), params.requested_format.frame_size.width(),
      params.requested_format.frame_size.height(),
      params.requested_format.frame_rate);
  if (!ret) {
    SetErrorState(FROM_HERE, "failed to allocate");
    return;
  }

  capture_format_.frame_size.SetSize(
      Java_VideoCapture_queryWidth(env, j_capture_.obj()),
      Java_VideoCapture_queryHeight(env, j_capture_.obj()));
  capture_format_.frame_rate =
      Java_VideoCapture_queryFrameRate(env, j_capture_.obj());
  capture_format_.pixel_format = GetColorspace();
  DCHECK_NE(capture_format_.pixel_format, media::PIXEL_FORMAT_UNKNOWN);
  CHECK(capture_format_.frame_size.GetArea() > 0);
  CHECK(!(capture_format_.frame_size.width() % 2));
  CHECK(!(capture_format_.frame_size.height() % 2));

  if (capture_format_.frame_rate > 0) {
    frame_interval_ = base::TimeDelta::FromMicroseconds(
        (base::Time::kMicrosecondsPerSecond + capture_format_.frame_rate - 1) /
        capture_format_.frame_rate);
  }

  DVLOG(1) << __FUNCTION__ << " requested ("
           << capture_format_.frame_size.ToString() << ")@ "
           << capture_format_.frame_rate << "fps";

  ret = Java_VideoCapture_startCapture(env, j_capture_.obj());
  if (!ret) {
    SetErrorState(FROM_HERE, "failed to start capture");
    return;
  }

  {
    base::AutoLock lock(lock_);
    state_ = kConfigured;
  }
}

void VideoCaptureDeviceAndroid::StopAndDeAllocate() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(lock_);
    if (state_ != kConfigured && state_ != kError)
      return;
  }

  JNIEnv* env = AttachCurrentThread();

  const jboolean ret = Java_VideoCapture_stopCapture(env, j_capture_.obj());
  if (!ret) {
    SetErrorState(FROM_HERE, "failed to stop capture");
    return;
  }

  {
    base::AutoLock lock(lock_);
    state_ = kIdle;
    client_.reset();
  }

  Java_VideoCapture_deallocate(env, j_capture_.obj());
}

void VideoCaptureDeviceAndroid::TakePhoto(TakePhotoCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(lock_);
    if (state_ != kConfigured)
      return;
    if (!got_first_frame_) {  // We have to wait until we get the first frame.
      photo_requests_queue_.push_back(
          base::Bind(&VideoCaptureDeviceAndroid::DoTakePhoto,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
      return;
    }
  }
  DoTakePhoto(std::move(callback));
}

void VideoCaptureDeviceAndroid::GetPhotoCapabilities(
    GetPhotoCapabilitiesCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(lock_);
    if (state_ != kConfigured)
      return;
    if (!got_first_frame_) {  // We have to wait until we get the first frame.
      photo_requests_queue_.push_back(
          base::Bind(&VideoCaptureDeviceAndroid::DoGetPhotoCapabilities,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback)));
      return;
    }
  }
  DoGetPhotoCapabilities(std::move(callback));
}

void VideoCaptureDeviceAndroid::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    base::AutoLock lock(lock_);
    if (state_ != kConfigured)
      return;
    if (!got_first_frame_) {  // We have to wait until we get the first frame.
      photo_requests_queue_.push_back(
          base::Bind(&VideoCaptureDeviceAndroid::DoSetPhotoOptions,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&settings),
                     base::Passed(&callback)));
      return;
    }
  }
  DoSetPhotoOptions(std::move(settings), std::move(callback));
}

void VideoCaptureDeviceAndroid::OnFrameAvailable(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jbyteArray>& data,
    jint length,
    jint rotation) {
  {
    base::AutoLock lock(lock_);
    if (state_ != kConfigured || !client_)
      return;
  }

  jbyte* buffer = env->GetByteArrayElements(data, NULL);
  if (!buffer) {
    LOG(ERROR) << "VideoCaptureDeviceAndroid::OnFrameAvailable: "
                  "failed to GetByteArrayElements";
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  {
    base::AutoLock lock(lock_);
    if (!got_first_frame_) {
      // Set aside one frame allowance for fluctuation.
      expected_next_frame_time_ = current_time - frame_interval_;
      first_ref_time_ = current_time;
      got_first_frame_ = true;

      for (const auto& request : photo_requests_queue_)
        main_task_runner_->PostTask(FROM_HERE, request);
      photo_requests_queue_.clear();
    }
  }

  // Deliver the frame when it doesn't arrive too early.
  if (expected_next_frame_time_ <= current_time) {
    expected_next_frame_time_ += frame_interval_;

    // TODO(qiangchen): Investigate how to get raw timestamp for Android,
    // rather than using reference time to calculate timestamp.
    base::AutoLock lock(lock_);
    if (!client_)
      return;
    client_->OnIncomingCapturedData(reinterpret_cast<uint8_t*>(buffer), length,
                                    capture_format_, rotation, current_time,
                                    current_time - first_ref_time_);
  }

  env->ReleaseByteArrayElements(data, buffer, JNI_ABORT);
}

void VideoCaptureDeviceAndroid::OnI420FrameAvailable(JNIEnv* env,
                                                     jobject obj,
                                                     jobject y_buffer,
                                                     jint y_stride,
                                                     jobject u_buffer,
                                                     jobject v_buffer,
                                                     jint uv_row_stride,
                                                     jint uv_pixel_stride,
                                                     jint width,
                                                     jint height,
                                                     jint rotation) {
  {
    base::AutoLock lock(lock_);
    if (state_ != kConfigured || !client_)
      return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  {
    base::AutoLock lock(lock_);
    if (!got_first_frame_) {
      // Set aside one frame allowance for fluctuation.
      expected_next_frame_time_ = current_time - frame_interval_;
      first_ref_time_ = current_time;
      got_first_frame_ = true;

      for (const auto& request : photo_requests_queue_)
        main_task_runner_->PostTask(FROM_HERE, request);
      photo_requests_queue_.clear();
    }
  }

  uint8_t* const y_src =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(y_buffer));
  CHECK(y_src);
  uint8_t* const u_src =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(u_buffer));
  CHECK(u_src);
  uint8_t* const v_src =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(v_buffer));
  CHECK(v_src);

  const int y_plane_length = width * height;
  const int uv_plane_length = y_plane_length / 4;
  const int buffer_length = y_plane_length + uv_plane_length * 2;
  std::unique_ptr<uint8_t> buffer(new uint8_t[buffer_length]);

  libyuv::Android420ToI420(y_src, y_stride, u_src, uv_row_stride, v_src,
                           uv_row_stride, uv_pixel_stride, buffer.get(), width,
                           buffer.get() + y_plane_length, width / 2,
                           buffer.get() + y_plane_length + uv_plane_length,
                           width / 2, width, height);

  // Deliver the frame when it doesn't arrive too early.
  if (expected_next_frame_time_ <= current_time) {
    expected_next_frame_time_ += frame_interval_;

    // TODO(qiangchen): Investigate how to get raw timestamp for Android,
    // rather than using reference time to calculate timestamp.
    base::AutoLock lock(lock_);
    if (!client_)
      return;
    client_->OnIncomingCapturedData(buffer.get(), buffer_length,
                                    capture_format_, rotation, current_time,
                                    current_time - first_ref_time_);
  }
}

void VideoCaptureDeviceAndroid::OnError(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        const JavaParamRef<jstring>& message) {
  SetErrorState(FROM_HERE,
                base::android::ConvertJavaStringToUTF8(env, message));
}

void VideoCaptureDeviceAndroid::OnPhotoTaken(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong callback_id,
    const base::android::JavaParamRef<jbyteArray>& data) {
  DCHECK(callback_id);

  base::AutoLock lock(photo_callbacks_lock_);

  TakePhotoCallback* const cb =
      reinterpret_cast<TakePhotoCallback*>(callback_id);
  // Search for the pointer |cb| in the list of |photo_callbacks_|.
  const auto reference_it =
      std::find_if(photo_callbacks_.begin(), photo_callbacks_.end(),
                   [cb](const std::unique_ptr<TakePhotoCallback>& callback) {
                     return callback.get() == cb;
                   });
  if (reference_it == photo_callbacks_.end()) {
    NOTREACHED() << "|callback_id| not found.";
    return;
  }

  mojom::BlobPtr blob = mojom::Blob::New();
  base::android::JavaByteArrayToByteVector(env, data.obj(), &blob->data);
  blob->mime_type = blob->data.empty() ? "" : "image/jpeg";
  cb->Run(std::move(blob));

  photo_callbacks_.erase(reference_it);
}

VideoPixelFormat VideoCaptureDeviceAndroid::GetColorspace() {
  JNIEnv* env = AttachCurrentThread();
  const int current_capture_colorspace =
      Java_VideoCapture_getColorspace(env, j_capture_.obj());
  switch (current_capture_colorspace) {
    case ANDROID_IMAGE_FORMAT_YV12:
      return media::PIXEL_FORMAT_YV12;
    case ANDROID_IMAGE_FORMAT_YUV_420_888:
      return media::PIXEL_FORMAT_I420;
    case ANDROID_IMAGE_FORMAT_NV21:
      return media::PIXEL_FORMAT_NV21;
    case ANDROID_IMAGE_FORMAT_UNKNOWN:
    default:
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

void VideoCaptureDeviceAndroid::SetErrorState(
    const tracked_objects::Location& from_here,
    const std::string& reason) {
  {
    base::AutoLock lock(lock_);
    state_ = kError;
    if (!client_)
      return;
    client_->OnError(from_here, reason);
  }
}

void VideoCaptureDeviceAndroid::DoTakePhoto(TakePhotoCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(lock_);
    DCHECK_EQ(kConfigured, state_);
    DCHECK(got_first_frame_);
  }
#endif
  JNIEnv* env = AttachCurrentThread();

  // Make copy on the heap so we can pass the pointer through JNI.
  std::unique_ptr<TakePhotoCallback> heap_callback(
      new TakePhotoCallback(std::move(callback)));
  const intptr_t callback_id = reinterpret_cast<intptr_t>(heap_callback.get());
  if (!Java_VideoCapture_takePhoto(env, j_capture_.obj(), callback_id,
                                   next_photo_resolution_.width(),
                                   next_photo_resolution_.height()))
    return;

  {
    base::AutoLock lock(photo_callbacks_lock_);
    photo_callbacks_.push_back(std::move(heap_callback));
  }
}

void VideoCaptureDeviceAndroid::DoGetPhotoCapabilities(
    GetPhotoCapabilitiesCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(lock_);
    DCHECK_EQ(kConfigured, state_);
    DCHECK(got_first_frame_);
  }
#endif
  JNIEnv* env = AttachCurrentThread();

  PhotoCapabilities caps(
      Java_VideoCapture_getPhotoCapabilities(env, j_capture_.obj()));

  // TODO(mcasas): Manual member copying sucks, consider adding typemapping from
  // PhotoCapabilities to mojom::PhotoCapabilitiesPtr, https://crbug.com/622002.
  mojom::PhotoCapabilitiesPtr photo_capabilities =
      mojom::PhotoCapabilities::New();
  photo_capabilities->iso = mojom::Range::New();
  photo_capabilities->iso->current = caps.getCurrentIso();
  photo_capabilities->iso->max = caps.getMaxIso();
  photo_capabilities->iso->min = caps.getMinIso();
  photo_capabilities->height = mojom::Range::New();
  photo_capabilities->height->current = caps.getCurrentHeight();
  photo_capabilities->height->max = caps.getMaxHeight();
  photo_capabilities->height->min = caps.getMinHeight();
  photo_capabilities->width = mojom::Range::New();
  photo_capabilities->width->current = caps.getCurrentWidth();
  photo_capabilities->width->max = caps.getMaxWidth();
  photo_capabilities->width->min = caps.getMinWidth();
  photo_capabilities->zoom = mojom::Range::New();
  photo_capabilities->zoom->current = caps.getCurrentZoom();
  photo_capabilities->zoom->max = caps.getMaxZoom();
  photo_capabilities->zoom->min = caps.getMinZoom();
  photo_capabilities->focus_mode = caps.getAutoFocusInUse()
                                       ? mojom::FocusMode::AUTO
                                       : mojom::FocusMode::MANUAL;
  callback.Run(std::move(photo_capabilities));
}

void VideoCaptureDeviceAndroid::DoSetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
#if DCHECK_IS_ON()
  {
    base::AutoLock lock(lock_);
    DCHECK_EQ(kConfigured, state_);
    DCHECK(got_first_frame_);
  }
#endif
  JNIEnv* env = AttachCurrentThread();

  // |width| and/or |height| are kept for the next TakePhoto()s.
  if (settings->has_width || settings->has_height)
    next_photo_resolution_.SetSize(0, 0);
  if (settings->has_width) {
    next_photo_resolution_.set_width(
        base::saturated_cast<int>(settings->width));
  }
  if (settings->has_height) {
    next_photo_resolution_.set_height(
        base::saturated_cast<int>(settings->height));
  }

  if (settings->has_zoom)
    Java_VideoCapture_setZoom(env, j_capture_.obj(), settings->zoom);

  callback.Run(true);
}

}  // namespace media

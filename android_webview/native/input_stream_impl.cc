// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/input_stream_impl.h"

#include "base/android/jni_android.h"
// Disable "Warnings treated as errors" for input_stream_jni as it's a Java
// system class and we have to generate C++ hooks for all methods in the class
// even if they're unused.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "jni/InputStreamUtil_jni.h"
#pragma GCC diagnostic pop
#include "net/base/io_buffer.h"

using base::android::AttachCurrentThread;
using base::android::ClearException;
using base::android::JavaRef;

namespace android_webview {

namespace {

// This should be the same as InputStramUtil.EXCEPTION_THROWN_STATUS.
const int kExceptionThrownStatusCode = -2;

}

// Maximum number of bytes to be read in a single read.
const int InputStreamImpl::kBufferSize = 4096;

// static
const InputStreamImpl* InputStreamImpl::FromInputStream(
        const InputStream* input_stream) {
    return static_cast<const InputStreamImpl*>(input_stream);
}

// TODO: Use unsafe version for all Java_InputStream methods in this file
// once BUG 157880 is fixed and implement graceful exception handling.

InputStreamImpl::InputStreamImpl() {
}

InputStreamImpl::InputStreamImpl(const JavaRef<jobject>& stream)
    : jobject_(stream) {
  DCHECK(!stream.is_null());
}

InputStreamImpl::~InputStreamImpl() {
  JNIEnv* env = AttachCurrentThread();
  Java_InputStreamUtil_close(env, jobject_.obj());
}

bool InputStreamImpl::BytesAvailable(int* bytes_available) const {
  JNIEnv* env = AttachCurrentThread();
  int bytes = Java_InputStreamUtil_available(env, jobject_.obj());
  if (bytes == kExceptionThrownStatusCode)
    return false;
  *bytes_available = bytes;
  return true;
}

bool InputStreamImpl::Skip(int64_t n, int64_t* bytes_skipped) {
  JNIEnv* env = AttachCurrentThread();
  int bytes = Java_InputStreamUtil_skip(env, jobject_.obj(), n);
  if (bytes < 0)
    return false;
  if (bytes > n)
    return false;
  *bytes_skipped = bytes;
  return true;
}

bool InputStreamImpl::Read(net::IOBuffer* dest, int length, int* bytes_read) {
  JNIEnv* env = AttachCurrentThread();
  if (!buffer_.obj()) {
    // Allocate transfer buffer.
    base::android::ScopedJavaLocalRef<jbyteArray> temp(
        env, env->NewByteArray(kBufferSize));
    buffer_.Reset(temp);
    if (ClearException(env))
      return false;
  }

  int remaining_length = length;
  char* dest_write_ptr = dest->data();
  jbyteArray buffer = buffer_.obj();
  *bytes_read = 0;

  while (remaining_length > 0) {
    const int max_transfer_length = std::min(remaining_length, kBufferSize);
    const int transfer_length = Java_InputStreamUtil_read(
        env, jobject_.obj(), buffer, 0, max_transfer_length);
    if (transfer_length == kExceptionThrownStatusCode)
      return false;

    if (transfer_length < 0)  // EOF
      break;

    // Note: it is possible, yet unlikely, that the Java InputStream returns
    // a transfer_length == 0 from time to time. In such cases we just continue
    // the read until we get either valid data or reach EOF.
    if (transfer_length == 0)
      continue;

    DCHECK_GE(max_transfer_length, transfer_length);
    DCHECK_GE(env->GetArrayLength(buffer), transfer_length);

    // This check is to prevent a malicious InputStream implementation from
    // overrunning the |dest| buffer.
    if (transfer_length > max_transfer_length)
      return false;

    // Copy the data over to the provided C++ IOBuffer.
    DCHECK_GE(remaining_length, transfer_length);
    env->GetByteArrayRegion(buffer, 0, transfer_length,
        reinterpret_cast<jbyte*>(dest_write_ptr));
    if (ClearException(env))
      return false;

    remaining_length -= transfer_length;
    dest_write_ptr += transfer_length;
  }
  // bytes_read can be strictly less than the req. length if EOF is encountered.
  DCHECK_GE(remaining_length, 0);
  DCHECK_LE(remaining_length, length);
  *bytes_read = length - remaining_length;
  return true;
}

} // namespace android_webview

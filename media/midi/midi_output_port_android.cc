// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_output_port_android.h"

#include "base/android/jni_array.h"
#include "jni/MidiOutputPortAndroid_jni.h"

using base::android::ScopedJavaLocalRef;

namespace media {
namespace midi {

MidiOutputPortAndroid::MidiOutputPortAndroid(JNIEnv* env, jobject raw)
    : raw_port_(env, raw) {}
MidiOutputPortAndroid::~MidiOutputPortAndroid() {
  Close();
}

bool MidiOutputPortAndroid::Open() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MidiOutputPortAndroid_open(env, raw_port_.obj());
}

void MidiOutputPortAndroid::Close() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MidiOutputPortAndroid_close(env, raw_port_.obj());
}

void MidiOutputPortAndroid::Send(const std::vector<uint8_t>& data) {
  if (data.size() == 0) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> data_to_pass =
      base::android::ToJavaByteArray(env, &data[0], data.size());

  Java_MidiOutputPortAndroid_send(env, raw_port_.obj(), data_to_pass.obj());
}

}  // namespace midi
}  // namespace media

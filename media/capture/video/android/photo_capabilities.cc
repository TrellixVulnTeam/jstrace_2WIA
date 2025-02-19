// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/photo_capabilities.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "jni/PhotoCapabilities_jni.h"

using base::android::AttachCurrentThread;

namespace media {

PhotoCapabilities::PhotoCapabilities(
    base::android::ScopedJavaLocalRef<jobject> object)
    : object_(object) {}

PhotoCapabilities::~PhotoCapabilities() {}

int PhotoCapabilities::getMinIso() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinIso(AttachCurrentThread(), object_.obj());
}

int PhotoCapabilities::getMaxIso() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxIso(AttachCurrentThread(), object_.obj());
}

int PhotoCapabilities::getCurrentIso() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentIso(AttachCurrentThread(),
                                              object_.obj());
}

int PhotoCapabilities::getMinHeight() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinHeight(AttachCurrentThread(),
                                             object_.obj());
}

int PhotoCapabilities::getMaxHeight() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxHeight(AttachCurrentThread(),
                                             object_.obj());
}

int PhotoCapabilities::getCurrentHeight() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentHeight(AttachCurrentThread(),
                                                 object_.obj());
}

int PhotoCapabilities::getMinWidth() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinWidth(AttachCurrentThread(),
                                            object_.obj());
}

int PhotoCapabilities::getMaxWidth() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxWidth(AttachCurrentThread(),
                                            object_.obj());
}

int PhotoCapabilities::getCurrentWidth() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentWidth(AttachCurrentThread(),
                                                object_.obj());
}

int PhotoCapabilities::getMinZoom() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinZoom(AttachCurrentThread(),
                                           object_.obj());
}

int PhotoCapabilities::getMaxZoom() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxZoom(AttachCurrentThread(),
                                           object_.obj());
}

int PhotoCapabilities::getCurrentZoom() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentZoom(AttachCurrentThread(),
                                               object_.obj());
}

bool PhotoCapabilities::getAutoFocusInUse() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getAutoFocusInUse(AttachCurrentThread(),
                                                  object_.obj());
}

}  // namespace media

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/account_management_screen_helper.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "jni/AccountManagementScreenHelper_jni.h"

using base::android::JavaParamRef;

// static
void AccountManagementScreenHelper::OpenAccountManagementScreen(
    Profile* profile,
    signin::GAIAServiceType service_type) {
  DCHECK(profile);
  DCHECK(ProfileAndroid::FromProfile(profile));

  Java_AccountManagementScreenHelper_openAccountManagementScreen(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject().obj(),
      static_cast<int>(service_type));
}

static void LogEvent(JNIEnv* env,
                     const JavaParamRef<jclass>& clazz,
                     jint metric,
                     jint gaiaServiceType) {
  ProfileMetrics::LogProfileAndroidAccountManagementMenu(
      static_cast<ProfileMetrics::ProfileAndroidAccountManagementMenu>(metric),
      static_cast<signin::GAIAServiceType>(gaiaServiceType));
}

// static
bool AccountManagementScreenHelper::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/uma_utils.h"

#include <stdint.h>

#include "chrome/browser/browser_process.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "jni/UmaUtils_jni.h"

using base::android::JavaParamRef;

class PrefService;

namespace chrome {
namespace android {

base::Time GetMainEntryPointTime() {
  JNIEnv* env = base::android::AttachCurrentThread();
  int64_t startTimeUnixMs = Java_UmaUtils_getMainEntryPointWallTime(env);
  return base::Time::UnixEpoch() +
         base::TimeDelta::FromMilliseconds(startTimeUnixMs);
}

static void RecordMetricsReportingDefaultOptIn(JNIEnv* env,
                                               const JavaParamRef<jclass>& obj,
                                               jboolean opt_in) {
  DCHECK(g_browser_process);
  PrefService* local_state = g_browser_process->local_state();
  metrics::RecordMetricsReportingDefaultState(
      local_state, opt_in ? metrics::EnableMetricsDefault::OPT_IN
                          : metrics::EnableMetricsDefault::OPT_OUT);
}

bool RegisterStartupMetricUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace chrome

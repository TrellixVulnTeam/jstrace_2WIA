// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/android/android_combined_policy_provider.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/policy/core/browser/android/policy_converter.h"
#include "jni/CombinedPolicyProvider_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace {

bool g_wait_for_policies = false;

}  // namespace

namespace policy {
namespace android {

AndroidCombinedPolicyProvider::AndroidCombinedPolicyProvider(
    SchemaRegistry* registry)
    : initialized_(!g_wait_for_policies) {
  PolicyNamespace ns(POLICY_DOMAIN_CHROME, std::string());
  const Schema* schema = registry->schema_map()->GetSchema(ns);
  policy_converter_.reset(new policy::android::PolicyConverter(schema));
  java_combined_policy_provider_.Reset(Java_CombinedPolicyProvider_linkNative(
      AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      policy_converter_->GetJavaObject().obj()));
}

AndroidCombinedPolicyProvider::~AndroidCombinedPolicyProvider() {
  Java_CombinedPolicyProvider_linkNative(AttachCurrentThread(), 0, jobject());
  java_combined_policy_provider_.Reset();
}

void AndroidCombinedPolicyProvider::RefreshPolicies() {
  JNIEnv* env = AttachCurrentThread();
  Java_CombinedPolicyProvider_refreshPolicies(
      env, java_combined_policy_provider_.obj());
}

void AndroidCombinedPolicyProvider::FlushPolicies(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  initialized_ = true;
  UpdatePolicy(policy_converter_->GetPolicyBundle());
}

// static
void AndroidCombinedPolicyProvider::SetShouldWaitForPolicy(
    bool should_wait_for_policy) {
  g_wait_for_policies = should_wait_for_policy;
}

bool AndroidCombinedPolicyProvider::IsInitializationComplete(
    PolicyDomain domain) const {
  return initialized_;
}

bool AndroidCombinedPolicyProvider::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace policy

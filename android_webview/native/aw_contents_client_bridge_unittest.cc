// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_client_bridge.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "jni/MockAwContentsClientBridge_jni.h"
#include "net/android/net_jni_registrar.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using net::SSLCertRequestInfo;
using net::SSLClientCertType;
using net::X509Certificate;
using testing::NotNull;
using testing::Test;

namespace android_webview {

namespace {

// Tests the android_webview contents client bridge.
class AwContentsClientBridgeTest : public Test {
 public:
  AwContentsClientBridgeTest() { }

  // Callback method called when a cert is selected.
  void CertSelected(X509Certificate* cert);
 protected:
  void SetUp() override;
  void TestCertType(SSLClientCertType type, const std::string& expected_name);
  // Create the TestBrowserThreads. Just instantiate the member variable.
  content::TestBrowserThreadBundle thread_bundle_;
  base::android::ScopedJavaGlobalRef<jobject> jbridge_;
  std::unique_ptr<AwContentsClientBridge> bridge_;
  scoped_refptr<SSLCertRequestInfo> cert_request_info_;
  X509Certificate* selected_cert_;
  int cert_selected_callbacks_;
  JNIEnv* env_;
};

class TestClientCertificateDelegate
    : public content::ClientCertificateDelegate {
 public:
  explicit TestClientCertificateDelegate(AwContentsClientBridgeTest* test)
      : test_(test) {}

  // content::ClientCertificateDelegate.
  void ContinueWithCertificate(net::X509Certificate* cert) override {
    test_->CertSelected(cert);
    test_ = nullptr;
  }

 private:
  AwContentsClientBridgeTest* test_;

  DISALLOW_COPY_AND_ASSIGN(TestClientCertificateDelegate);
};

}   // namespace

void AwContentsClientBridgeTest::SetUp() {
  env_ = AttachCurrentThread();
  ASSERT_THAT(env_, NotNull());
  ASSERT_TRUE(android_webview::RegisterAwContentsClientBridge(env_));
  ASSERT_TRUE(net::android::RegisterJni(env_));
  jbridge_.Reset(env_,
      Java_MockAwContentsClientBridge_getAwContentsClientBridge(env_).obj());
  bridge_.reset(new AwContentsClientBridge(env_, jbridge_.obj()));
  selected_cert_ = nullptr;
  cert_selected_callbacks_ = 0;
  cert_request_info_ = new net::SSLCertRequestInfo;
}

void AwContentsClientBridgeTest::CertSelected(X509Certificate* cert) {
  selected_cert_ = cert;
  cert_selected_callbacks_++;
}

TEST_F(AwContentsClientBridgeTest, TestClientCertKeyTypesCorrectlyEncoded) {
  SSLClientCertType cert_types[2] = {net::CLIENT_CERT_RSA_SIGN,
                                     net::CLIENT_CERT_ECDSA_SIGN};
  std::string expected_names[2] = {"RSA", "ECDSA"};

  for (int i = 0; i < 2; i++) {
    TestCertType(cert_types[i], expected_names[i]);
  }
}

void AwContentsClientBridgeTest::TestCertType(SSLClientCertType type,
      const std::string& expected_name) {
  cert_request_info_->cert_key_types.clear();
  cert_request_info_->cert_key_types.push_back(type);
  bridge_->SelectClientCertificate(
      cert_request_info_.get(),
      base::WrapUnique(new TestClientCertificateDelegate(this)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, cert_selected_callbacks_);
  ScopedJavaLocalRef<jobjectArray> key_types =
      Java_MockAwContentsClientBridge_getKeyTypes(env_, jbridge_.obj());
  std::vector<std::string> vec;
  base::android::AppendJavaStringArrayToStringVector(env_,
                                                     key_types.obj(),
                                                     &vec);
  EXPECT_EQ(1u, vec.size());
  EXPECT_EQ(expected_name, vec[0]);
}

// Verify that ProvideClientCertificateResponse works properly when the client
// responds with a null key.
TEST_F(AwContentsClientBridgeTest,
    TestProvideClientCertificateResponseCallsCallbackOnNullKey) {
  // Call SelectClientCertificate to create a callback id that mock java object
  // can call on.
  bridge_->SelectClientCertificate(
      cert_request_info_.get(),
      base::WrapUnique(new TestClientCertificateDelegate(this)));
  bridge_->ProvideClientCertificateResponse(
      env_, jbridge_,
      Java_MockAwContentsClientBridge_getRequestId(env_, jbridge_.obj()),
      Java_MockAwContentsClientBridge_createTestCertChain(env_, jbridge_.obj()),
      nullptr);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, selected_cert_);
  EXPECT_EQ(1, cert_selected_callbacks_);
}

// Verify that ProvideClientCertificateResponse calls the callback with
// null parameters when private key is not provided.
TEST_F(AwContentsClientBridgeTest,
    TestProvideClientCertificateResponseCallsCallbackOnNullChain) {
  // Call SelectClientCertificate to create a callback id that mock java object
  // can call on.
  bridge_->SelectClientCertificate(
      cert_request_info_.get(),
      base::WrapUnique(new TestClientCertificateDelegate(this)));
  int requestId =
    Java_MockAwContentsClientBridge_getRequestId(env_, jbridge_.obj());
  bridge_->ProvideClientCertificateResponse(env_, jbridge_, requestId, nullptr,
                                            nullptr);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(nullptr, selected_cert_);
  EXPECT_EQ(1, cert_selected_callbacks_);
}

}   // android_webview

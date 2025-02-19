// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/android_webview_jni_registrar.h"

#include "android_webview/native/android_protocol_handler.h"
#include "android_webview/native/aw_autofill_client.h"
#include "android_webview/native/aw_contents.h"
#include "android_webview/native/aw_contents_client_bridge.h"
#include "android_webview/native/aw_contents_statics.h"
#include "android_webview/native/aw_debug.h"
#include "android_webview/native/aw_dev_tools_server.h"
#include "android_webview/native/aw_form_database.h"
#include "android_webview/native/aw_gl_functor.h"
#include "android_webview/native/aw_http_auth_handler.h"
#include "android_webview/native/aw_message_port_service_impl.h"
#include "android_webview/native/aw_metrics_switch.h"
#include "android_webview/native/aw_pdf_exporter.h"
#include "android_webview/native/aw_picture.h"
#include "android_webview/native/aw_quota_manager_bridge_impl.h"
#include "android_webview/native/aw_settings.h"
#include "android_webview/native/aw_web_contents_delegate.h"
#include "android_webview/native/cookie_manager.h"
#include "android_webview/native/permission/aw_permission_request.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/trace_event/trace_event.h"

namespace android_webview {

static base::android::RegistrationMethod kWebViewRegisteredMethods[] = {
  // Register JNI for android_webview classes.
  { "AndroidProtocolHandler", RegisterAndroidProtocolHandler },
  { "AwAutofillClient", RegisterAwAutofillClient },
  { "AwContents", RegisterAwContents },
  { "AwContentsClientBridge", RegisterAwContentsClientBridge },
  { "AwContentsStatics", RegisterAwContentsStatics },
  { "AwDebug", RegisterAwDebug },
  { "AwDevToolsServer", RegisterAwDevToolsServer },
  { "AwFormDatabase", RegisterAwFormDatabase },
  { "AwPicture", RegisterAwPicture },
  { "AwSettings", RegisterAwSettings },
  { "AwHttpAuthHandler", RegisterAwHttpAuthHandler },
  { "AwMetricsServiceClient", RegisterAwMetricsServiceClient },
  { "AwPdfExporter", RegisterAwPdfExporter },
  { "AwPermissionRequest", RegisterAwPermissionRequest },
  { "AwQuotaManagerBridge", RegisterAwQuotaManagerBridge },
  { "AwWebContentsDelegate", RegisterAwWebContentsDelegate },
  { "CookieManager", RegisterCookieManager },
  { "AwMessagePortService", RegisterAwMessagePortService },
  { "AwGLFunctor", RegisterAwGLFunctor },
};

bool RegisterJni(JNIEnv* env) {
  TRACE_EVENT0("startup", "android_webview::RegisterJni");
  return RegisterNativeMethods(env,
      kWebViewRegisteredMethods, arraysize(kWebViewRegisteredMethods));
}

} // namespace android_webview

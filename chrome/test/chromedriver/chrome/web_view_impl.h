// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_IMPL_H_

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/test/chromedriver/chrome/web_view.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

struct BrowserInfo;
class DebuggerTracker;
struct DeviceMetrics;
class DevToolsClient;
class DomTracker;
class FrameTracker;
class GeolocationOverrideManager;
class MobileEmulationOverrideManager;
class NetworkConditionsOverrideManager;
class HeapSnapshotTaker;
struct KeyEvent;
struct MouseEvent;
class NavigationTracker;
class NonBlockingNavigationTracker;
class PageLoadStrategy;
class Status;

class WebViewImpl : public WebView {
 public:
  WebViewImpl(const std::string& id,
              const BrowserInfo* browser_info,
              std::unique_ptr<DevToolsClient> client);
  WebViewImpl(const std::string& id,
              const BrowserInfo* browser_info,
              std::unique_ptr<DevToolsClient> client,
              const DeviceMetrics* device_metrics,
              std::string page_load_strategy);
  ~WebViewImpl() override;

  // Overridden from WebView:
  std::string GetId() override;
  bool WasCrashed() override;
  Status ConnectIfNecessary() override;
  Status HandleReceivedEvents() override;
  Status GetUrl(std::string* url) override;
  Status Load(const std::string& url, const Timeout* timeout) override;
  Status Reload(const Timeout* timeout) override;
  Status TraverseHistory(int delta, const Timeout* timeout) override;
  Status EvaluateScript(const std::string& frame,
                        const std::string& expression,
                        std::unique_ptr<base::Value>* result) override;
  Status CallFunction(const std::string& frame,
                      const std::string& function,
                      const base::ListValue& args,
                      std::unique_ptr<base::Value>* result) override;
  Status CallAsyncFunction(const std::string& frame,
                           const std::string& function,
                           const base::ListValue& args,
                           const base::TimeDelta& timeout,
                           std::unique_ptr<base::Value>* result) override;
  Status CallUserAsyncFunction(const std::string& frame,
                               const std::string& function,
                               const base::ListValue& args,
                               const base::TimeDelta& timeout,
                               std::unique_ptr<base::Value>* result) override;
  Status GetFrameByFunction(const std::string& frame,
                            const std::string& function,
                            const base::ListValue& args,
                            std::string* out_frame) override;
  Status DispatchMouseEvents(const std::list<MouseEvent>& events,
                             const std::string& frame) override;
  Status DispatchTouchEvent(const TouchEvent& event) override;
  Status DispatchTouchEvents(const std::list<TouchEvent>& events) override;
  Status DispatchKeyEvents(const std::list<KeyEvent>& events) override;
  Status GetCookies(std::unique_ptr<base::ListValue>* cookies) override;
  Status DeleteCookie(const std::string& name, const std::string& url) override;
  Status WaitForPendingNavigations(const std::string& frame_id,
                                   const Timeout& timeout,
                                   bool stop_load_on_timeout) override;
  Status IsPendingNavigation(const std::string& frame_id,
                             const Timeout* timeout,
                             bool* is_pending) override;
  JavaScriptDialogManager* GetJavaScriptDialogManager() override;
  Status OverrideGeolocation(const Geoposition& geoposition) override;
  Status OverrideNetworkConditions(
      const NetworkConditions& network_conditions) override;
  Status CaptureScreenshot(std::string* screenshot) override;
  Status SetFileInputFiles(const std::string& frame,
                           const base::DictionaryValue& element,
                           const std::vector<base::FilePath>& files) override;
  Status TakeHeapSnapshot(std::unique_ptr<base::Value>* snapshot) override;
  Status StartProfile() override;
  Status EndProfile(std::unique_ptr<base::Value>* profile_data) override;
  Status SynthesizeTapGesture(int x,
                              int y,
                              int tap_count,
                              bool is_long_press) override;
  Status SynthesizeScrollGesture(int x,
                                 int y,
                                 int xoffset,
                                 int yoffset) override;
  Status SynthesizePinchGesture(int x, int y, double scale_factor) override;

 private:
  Status TraverseHistoryWithJavaScript(int delta);
  Status CallAsyncFunctionInternal(const std::string& frame,
                                   const std::string& function,
                                   const base::ListValue& args,
                                   bool is_user_supplied,
                                   const base::TimeDelta& timeout,
                                   std::unique_ptr<base::Value>* result);
  Status IsNotPendingNavigation(const std::string& frame_id,
                                const Timeout* timeout,
                                bool* is_not_pending);

  Status InitProfileInternal();
  Status StopProfileInternal();

  std::string id_;
  const BrowserInfo* browser_info_;
  std::unique_ptr<DomTracker> dom_tracker_;
  std::unique_ptr<FrameTracker> frame_tracker_;
  std::unique_ptr<JavaScriptDialogManager> dialog_manager_;
  std::unique_ptr<PageLoadStrategy> navigation_tracker_;
  std::unique_ptr<MobileEmulationOverrideManager>
      mobile_emulation_override_manager_;
  std::unique_ptr<GeolocationOverrideManager> geolocation_override_manager_;
  std::unique_ptr<NetworkConditionsOverrideManager>
      network_conditions_override_manager_;
  std::unique_ptr<HeapSnapshotTaker> heap_snapshot_taker_;
  std::unique_ptr<DebuggerTracker> debugger_;
  std::unique_ptr<DevToolsClient> client_;
};

namespace internal {

enum EvaluateScriptReturnType {
  ReturnByValue,
  ReturnByObject
};
Status EvaluateScript(DevToolsClient* client,
                      int context_id,
                      const std::string& expression,
                      EvaluateScriptReturnType return_type,
                      std::unique_ptr<base::DictionaryValue>* result);
Status EvaluateScriptAndGetObject(DevToolsClient* client,
                                  int context_id,
                                  const std::string& expression,
                                  bool* got_object,
                                  std::string* object_id);
Status EvaluateScriptAndGetValue(DevToolsClient* client,
                                 int context_id,
                                 const std::string& expression,
                                 std::unique_ptr<base::Value>* result);
Status ParseCallFunctionResult(const base::Value& temp_result,
                               std::unique_ptr<base::Value>* result);
Status GetNodeIdFromFunction(DevToolsClient* client,
                             int context_id,
                             const std::string& function,
                             const base::ListValue& args,
                             bool* found_node,
                             int* node_id);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_IMPL_H_

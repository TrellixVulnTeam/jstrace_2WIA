// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/event_switches.h"
#include "ui/events/latency_info.h"

using blink::WebInputEvent;

namespace {

const char kJankyPageURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<meta name='viewport' content='width=device-width'/>"
    "<style>"
    "html, body {"
    "  margin: 0;"
    "}"
    ".spacer { height: 10000px; }"
    "</style>"
    "<div class=spacer></div>"
    "<script>"
    "  function jank(millis)"
    "  {"
    "    var end = performance.now() + millis;"
    "    while(performance.now() < end) {};"
    "    window.mouseMoveCount = 0;"
    "    window.touchMoveCount = 0;"
    "  }"
    "  window.mouseMoveCount = 0;"
    "  window.touchMoveCount = 0;"
    "  document.addEventListener('mousemove', function(e) {"
    "    window.mouseMoveCount++;"
    "    window.lastMouseMoveEvent = e;"
    "  });"
    "  document.addEventListener('touchmove', function (e) {"
    "    window.touchMoveCount++;"
    "    window.lastTouchMoveEvent = e;"
    "  }, {passive: true});"
    "  document.addEventListener('click', function(e) { jank(500); });"
    "  document.title='ready';"
    "</script>";

}  // namespace

namespace content {

class MainThreadEventQueueBrowserTest : public ContentBrowserTest {
 public:
  MainThreadEventQueueBrowserTest() {}
  ~MainThreadEventQueueBrowserTest() override {}

  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
  }

 protected:
  void LoadURL(const char* page_data) {
    const GURL data_url(page_data);
    NavigateToURL(shell(), data_url);

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    base::string16 ready_title(base::ASCIIToUTF16("ready"));
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    ignore_result(watcher.WaitAndGetTitle());

    MainThreadFrameObserver main_thread_sync(host);
    main_thread_sync.Wait();
  }

  int ExecuteScriptAndExtractInt(const std::string& script) {
    int value = 0;
    EXPECT_TRUE(content::ExecuteScriptAndExtractInt(
        shell(), "domAutomationController.send(" + script + ")", &value));
    return value;
  }

  void DoMouseMove() {
    // Send a click event to cause some jankiness. This is done via a click
    // event as ExecuteScript is synchronous.
    SimulateMouseClick(shell()->web_contents(), 0,
                       blink::WebPointerProperties::ButtonLeft);
    scoped_refptr<InputMsgWatcher> input_msg_watcher(
        new InputMsgWatcher(GetWidgetHost(), blink::WebInputEvent::MouseMove));
    GetWidgetHost()->ForwardMouseEvent(SyntheticWebMouseEventBuilder::Build(
        blink::WebInputEvent::MouseMove, 10, 10, 0));
    GetWidgetHost()->ForwardMouseEvent(SyntheticWebMouseEventBuilder::Build(
        blink::WebInputEvent::MouseMove, 15, 15, 0));
    GetWidgetHost()->ForwardMouseEvent(SyntheticWebMouseEventBuilder::Build(
        blink::WebInputEvent::MouseMove, 20, 25, 0));

    // Runs until we get the InputMsgAck callback.
    EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, input_msg_watcher->WaitForAck());

    int mouse_move_count = 0;
    while (mouse_move_count <= 0)
      mouse_move_count = ExecuteScriptAndExtractInt("window.mouseMoveCount");
    EXPECT_EQ(1, mouse_move_count);

    int last_mouse_x =
        ExecuteScriptAndExtractInt("window.lastMouseMoveEvent.x");
    int last_mouse_y =
        ExecuteScriptAndExtractInt("window.lastMouseMoveEvent.y");
    EXPECT_EQ(20, last_mouse_x);
    EXPECT_EQ(25, last_mouse_y);
  }

  void DoTouchMove() {
    SyntheticWebTouchEvent kEvents[4];
    kEvents[0].PressPoint(10, 10);
    kEvents[1].PressPoint(10, 10);
    kEvents[1].MovePoint(0, 20, 20);
    kEvents[2].PressPoint(10, 10);
    kEvents[2].MovePoint(0, 30, 30);
    kEvents[3].PressPoint(10, 10);
    kEvents[3].MovePoint(0, 35, 40);

    // Send a click event to cause some jankiness. This is done via a click
    // event as ExecuteScript is synchronous.
    SimulateMouseClick(shell()->web_contents(), 0,
                       blink::WebPointerProperties::ButtonLeft);
    scoped_refptr<InputMsgWatcher> input_msg_watcher(
        new InputMsgWatcher(GetWidgetHost(), blink::WebInputEvent::TouchMove));

    for (const auto& event : kEvents)
      GetWidgetHost()->ForwardEmulatedTouchEvent(event);

    // Runs until we get the InputMsgAck callback.
    EXPECT_EQ(INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING,
              input_msg_watcher->WaitForAck());

    int touch_move_count = 0;
    while (touch_move_count <= 0)
      touch_move_count = ExecuteScriptAndExtractInt("window.touchMoveCount");
    EXPECT_EQ(1, touch_move_count);

    int last_touch_x = ExecuteScriptAndExtractInt(
        "window.lastTouchMoveEvent.touches[0].pageX");
    int last_touch_y = ExecuteScriptAndExtractInt(
        "window.lastTouchMoveEvent.touches[0].pageY");
    EXPECT_EQ(35, last_touch_x);
    EXPECT_EQ(40, last_touch_y);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MainThreadEventQueueBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MainThreadEventQueueBrowserTest, MouseMove) {
  LoadURL(kJankyPageURL);
  DoMouseMove();
}

// Disabled on MacOS because it doesn't support touch input.
#if defined(OS_MACOSX)
#define MAYBE_TouchMove DISABLED_TouchMove
#else
#define MAYBE_TouchMove TouchMove
#endif
IN_PROC_BROWSER_TEST_F(MainThreadEventQueueBrowserTest, MAYBE_TouchMove) {
  LoadURL(kJankyPageURL);
  DoTouchMove();
}

}  // namespace content

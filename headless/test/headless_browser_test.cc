// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_browser_test.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "headless/public/domains/runtime.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace headless {
namespace {

class SynchronousLoadObserver {
 public:
  SynchronousLoadObserver(HeadlessBrowserTest* browser_test,
                          HeadlessWebContents* web_contents)
      : web_contents_(web_contents),
        devtools_client_(HeadlessDevToolsClient::Create()) {
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    load_observer_.reset(new LoadObserver(
        devtools_client_.get(),
        base::Bind(&HeadlessBrowserTest::FinishAsynchronousTest,
                   base::Unretained(browser_test))));
  }

  ~SynchronousLoadObserver() {
    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  }

  bool navigation_succeeded() const {
    return load_observer_->navigation_succeeded();
  }

 private:
  HeadlessWebContents* web_contents_;  // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
  std::unique_ptr<LoadObserver> load_observer_;
};

class EvaluateHelper {
 public:
  EvaluateHelper(HeadlessBrowserTest* browser_test,
                 HeadlessWebContents* web_contents,
                 const std::string& script_to_eval)
      : browser_test_(browser_test),
        web_contents_(web_contents),
        devtools_client_(HeadlessDevToolsClient::Create()) {
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    devtools_client_->GetRuntime()->Evaluate(
        script_to_eval,
        base::Bind(&EvaluateHelper::OnEvaluateResult, base::Unretained(this)));
  }

  ~EvaluateHelper() {
    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  }

  void OnEvaluateResult(std::unique_ptr<runtime::EvaluateResult> result) {
    result_ = std::move(result);
    browser_test_->FinishAsynchronousTest();
  }

  std::unique_ptr<runtime::EvaluateResult> TakeResult() {
    return std::move(result_);
  }

 private:
  HeadlessBrowserTest* browser_test_;  // Not owned.
  HeadlessWebContents* web_contents_;  // Not owned.
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;

  std::unique_ptr<runtime::EvaluateResult> result_;

  DISALLOW_COPY_AND_ASSIGN(EvaluateHelper);
};

}  // namespace

LoadObserver::LoadObserver(HeadlessDevToolsClient* devtools_client,
                           base::Closure callback)
    : callback_(std::move(callback)),
      devtools_client_(devtools_client),
      navigation_succeeded_(true) {
  devtools_client_->GetNetwork()->AddObserver(this);
  devtools_client_->GetNetwork()->Enable();
  devtools_client_->GetPage()->AddObserver(this);
  devtools_client_->GetPage()->Enable();
}

LoadObserver::~LoadObserver() {
  devtools_client_->GetNetwork()->RemoveObserver(this);
  devtools_client_->GetPage()->RemoveObserver(this);
}

void LoadObserver::OnLoadEventFired(const page::LoadEventFiredParams& params) {
  callback_.Run();
}

void LoadObserver::OnResponseReceived(
    const network::ResponseReceivedParams& params) {
  if (params.GetResponse()->GetStatus() != 200 ||
      params.GetResponse()->GetUrl() == content::kUnreachableWebDataURL) {
    navigation_succeeded_ = false;
  }
}

HeadlessBrowserTest::HeadlessBrowserTest() {
  base::FilePath headless_test_data(FILE_PATH_LITERAL("headless/test/data"));
  CreateTestServer(headless_test_data);
}

HeadlessBrowserTest::~HeadlessBrowserTest() {}

void HeadlessBrowserTest::SetUpOnMainThread() {}

void HeadlessBrowserTest::TearDownOnMainThread() {
  browser()->Shutdown();
}

void HeadlessBrowserTest::RunTestOnMainThreadLoop() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Pump startup related events.
  base::RunLoop().RunUntilIdle();

  SetUpOnMainThread();
  RunTestOnMainThread();
  TearDownOnMainThread();

  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->FastShutdownIfPossible();
  }
}

HeadlessBrowser* HeadlessBrowserTest::browser() const {
  return HeadlessContentMainDelegate::GetInstance()->browser();
}

bool HeadlessBrowserTest::WaitForLoad(HeadlessWebContents* web_contents) {
  SynchronousLoadObserver load_observer(this, web_contents);
  RunAsynchronousTest();
  return load_observer.navigation_succeeded();
}

std::unique_ptr<runtime::EvaluateResult> HeadlessBrowserTest::EvaluateScript(
    HeadlessWebContents* web_contents,
    const std::string& script) {
  EvaluateHelper helper(this, web_contents, script);
  RunAsynchronousTest();
  return helper.TakeResult();
}

void HeadlessBrowserTest::RunAsynchronousTest() {
  base::MessageLoop::ScopedNestableTaskAllower nestable_allower(
      base::MessageLoop::current());
  EXPECT_FALSE(run_loop_);
  run_loop_ = base::WrapUnique(new base::RunLoop());
  run_loop_->Run();
  run_loop_ = nullptr;
}

void HeadlessBrowserTest::FinishAsynchronousTest() {
  run_loop_->Quit();
}

HeadlessAsyncDevTooledBrowserTest::HeadlessAsyncDevTooledBrowserTest()
    : web_contents_(nullptr),
      devtools_client_(HeadlessDevToolsClient::Create()) {}

HeadlessAsyncDevTooledBrowserTest::~HeadlessAsyncDevTooledBrowserTest() {}

void HeadlessAsyncDevTooledBrowserTest::DevToolsTargetReady() {
  EXPECT_TRUE(web_contents_->GetDevToolsTarget());
  web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
  RunDevTooledTest();
}

void HeadlessAsyncDevTooledBrowserTest::RunTest() {
  browser_context_ = browser()->CreateBrowserContextBuilder().Build();

  web_contents_ = browser_context_->CreateWebContentsBuilder().Build();
  web_contents_->AddObserver(this);

  RunAsynchronousTest();

  web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
  web_contents_->RemoveObserver(this);
  web_contents_->Close();
  web_contents_ = nullptr;
  browser_context_->Close();
  browser_context_ = nullptr;
}

}  // namespace headless

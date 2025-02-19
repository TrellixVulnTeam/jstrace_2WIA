// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/data_reduction_proxy_resource_throttle_android.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::ResourceThrottle;
using safe_browsing::SafeBrowsingService;
using safe_browsing::SafeBrowsingUIManager;
using safe_browsing::SBThreatType;

// TODO(eroman): Downgrade these CHECK()s to DCHECKs once there is more
//               unit test coverage.
// TODO(sgurun) following the comment above, also provide tests for
// checking whether the headers are injected correctly and the SPDY proxy
// origin is tested properly.

const char* DataReductionProxyResourceThrottle::kUnsafeUrlProceedHeader =
      "X-Unsafe-Url-Proceed";

// static
DataReductionProxyResourceThrottle*
DataReductionProxyResourceThrottle::MaybeCreate(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceType resource_type,
    SafeBrowsingService* sb_service) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  // Don't create the throttle if we can't handle the request.
  if (io_data->IsOffTheRecord() || !io_data->IsDataReductionProxyEnabled() ||
      request->url().SchemeIsCryptographic()) {
    return NULL;
  }

  return new DataReductionProxyResourceThrottle(request, resource_type,
                                                sb_service);
}

DataReductionProxyResourceThrottle::DataReductionProxyResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    SafeBrowsingService* safe_browsing)
    : state_(STATE_NONE),
      safe_browsing_(safe_browsing),
      request_(request),
      is_subresource_(resource_type != content::RESOURCE_TYPE_MAIN_FRAME),
      is_subframe_(resource_type == content::RESOURCE_TYPE_SUB_FRAME) {
}

DataReductionProxyResourceThrottle::~DataReductionProxyResourceThrottle() { }

void DataReductionProxyResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  CHECK(state_ == STATE_NONE);

  // Save the redirect urls for possible malware detail reporting later.
  redirect_urls_.push_back(redirect_info.new_url);

  // We need to check the new URL before following the redirect.
  SBThreatType threat_type = CheckUrl();
  if (threat_type == safe_browsing::SB_THREAT_TYPE_SAFE)
    return;

  if (request_->load_flags() & net::LOAD_PREFETCH) {
    controller()->Cancel();
    return;
  }
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);

  state_ = STATE_DISPLAYING_BLOCKING_PAGE;
  SafeBrowsingUIManager::UnsafeResource unsafe_resource;
  unsafe_resource.url = redirect_info.new_url;
  unsafe_resource.original_url = request_->original_url();
  unsafe_resource.redirect_urls = redirect_urls_;
  unsafe_resource.is_subresource = is_subresource_;
  unsafe_resource.is_subframe = is_subframe_;
  unsafe_resource.threat_type = threat_type;
  unsafe_resource.callback = base::Bind(
      &DataReductionProxyResourceThrottle::OnBlockingPageComplete, AsWeakPtr());
  unsafe_resource.callback_thread =
      content::BrowserThread::GetTaskRunnerForThread(
          content::BrowserThread::IO);
  unsafe_resource.render_process_host_id = info->GetChildID();
  unsafe_resource.render_frame_id = info->GetRenderFrameID();
  unsafe_resource.threat_source = safe_browsing::ThreatSource::DATA_SAVER;

  *defer = true;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(
          &DataReductionProxyResourceThrottle::StartDisplayingBlockingPage,
          AsWeakPtr(), safe_browsing_->ui_manager(), unsafe_resource));
}

const char* DataReductionProxyResourceThrottle::GetNameForLogging() const {
    return "DataReductionProxyResourceThrottle";
}

// static
void DataReductionProxyResourceThrottle::StartDisplayingBlockingPage(
    const base::WeakPtr<DataReductionProxyResourceThrottle>& throttle,
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    const SafeBrowsingUIManager::UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      resource.render_process_host_id, resource.render_frame_id);
  if (rfh) {
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(web_contents);
    if (prerender_contents) {
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(resource.callback, false));
      return;
    }
  }
  ui_manager->DisplayBlockingPage(resource);
}

// SafeBrowsingService::UrlCheckCallback implementation, called on the IO
// thread when the user has decided to proceed with the current request, or
// go back.
void DataReductionProxyResourceThrottle::OnBlockingPageComplete(bool proceed) {
  CHECK(state_ == STATE_DISPLAYING_BLOCKING_PAGE);
  state_ = STATE_NONE;

  if (proceed)
    ResumeRequest();
  else
    controller()->Cancel();
}

SBThreatType DataReductionProxyResourceThrottle::CheckUrl() {
  SBThreatType result = safe_browsing::SB_THREAT_TYPE_SAFE;

  // TODO(sgurun) Check for spdy proxy origin.
  if (request_->response_headers() == NULL)
    return result;

  if (request_->response_headers()->HasHeader("X-Phishing-Url"))
    result = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  else if (request_->response_headers()->HasHeader("X-Malware-Url"))
    result = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;

  // If safe browsing is disabled and the request is sent to the DRP server,
  // we need to break the redirect loop by setting the extra header.
  if (result != safe_browsing::SB_THREAT_TYPE_SAFE &&
      !safe_browsing_->enabled()) {
    request_->SetExtraRequestHeaderByName(kUnsafeUrlProceedHeader, "1", true);
    result = safe_browsing::SB_THREAT_TYPE_SAFE;
  }

  return result;
}

void DataReductionProxyResourceThrottle::ResumeRequest() {
  CHECK(state_ == STATE_NONE);

  // Inject the header before resuming the request.
  request_->SetExtraRequestHeaderByName(kUnsafeUrlProceedHeader, "1", true);
  controller()->Resume();
}

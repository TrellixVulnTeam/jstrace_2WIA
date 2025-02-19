// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_
#define IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/dom_distiller/core/dom_distiller_request_view_base.h"
#include "components/dom_distiller/core/task_tracker.h"

class GURL;

namespace dom_distiller {

class DistilledPagePrefs;

// A very simple and naive implementation of the dom_distiller
// ViewRequestDelegate: From an URL it builds an HTML string and notifies when
// finished.
class DistillerViewer : public DomDistillerRequestViewBase {
 public:
  typedef struct {
    // The url of the image.
    GURL url;
    // The image data as a string.
    std::string data;
  } ImageInfo;
  typedef base::Callback<void(const GURL&,
                              const std::string&,
                              const std::vector<ImageInfo>& images)>
      DistillationFinishedCallback;

  DistillerViewer(dom_distiller::DomDistillerService* distillerService,
                  PrefService* prefs,
                  const GURL& url,
                  const DistillationFinishedCallback& callback);
  ~DistillerViewer() override;

  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override;

  void SendJavaScript(const std::string& buffer) override;

 private:
  // The url of the distilled page.
  const GURL url_;
  // JavaScript buffer.
  std::string js_buffer_;
  // Callback to run once distillation is complete.
  const DistillationFinishedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DistillerViewer);
};

}  // namespace dom_distiller

#endif  // IOS_CHROME_BROWSER_DOM_DISTILLER_DISTILLER_VIEWER_H_

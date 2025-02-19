// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_OBSERVER_H_
#define BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_OBSERVER_H_

#include "base/macros.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

// An observer API implemented by classes which are interested in various events
// related to BlimpContents.
class BlimpContentsObserver {
 public:
  virtual ~BlimpContentsObserver() = default;

  // Invoked when the navigation state of the BlimpContents has changed.
  virtual void OnNavigationStateChanged() {}

 protected:
  BlimpContentsObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpContentsObserver);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_OBSERVER_H_

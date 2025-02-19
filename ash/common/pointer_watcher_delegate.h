// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_POINTER_WATCHER_DELEGATE_H_
#define ASH_COMMON_POINTER_WATCHER_DELEGATE_H_

#include "ash/ash_export.h"

namespace views {
class PointerWatcher;
}

namespace ash {

// Allows different implementations of PointerWatcher in mus and non-mus ash.
class ASH_EXPORT PointerWatcherDelegate {
 public:
  virtual ~PointerWatcherDelegate() {}

  virtual void AddPointerWatcher(views::PointerWatcher* watcher) = 0;
  virtual void RemovePointerWatcher(views::PointerWatcher* watcher) = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_POINTER_WATCHER_DELEGATE_H_

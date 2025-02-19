// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_SHELF_DELEGATE_IMPL_H_
#define ASH_SHELL_SHELF_DELEGATE_IMPL_H_

#include "ash/common/shelf/shelf_delegate.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace ash {
namespace shell {

// TODO(jamescook): Replace with TestShelfDelegate so we don't have to maintain
// two stub implementations.
class ShelfDelegateImpl : public ShelfDelegate {
 public:
  ShelfDelegateImpl();
  ~ShelfDelegateImpl() override;

  // ShelfDelegate overrides:
  void OnShelfCreated(Shelf* shelf) override;
  void OnShelfDestroyed(Shelf* shelf) override;
  void OnShelfAlignmentChanged(Shelf* shelf) override;
  void OnShelfAutoHideBehaviorChanged(Shelf* shelf) override;
  void OnShelfAutoHideStateChanged(Shelf* shelf) override;
  void OnShelfVisibilityStateChanged(Shelf* shelf) override;
  ShelfID GetShelfIDForAppID(const std::string& app_id) override;
  bool HasShelfIDToAppIDMapping(ShelfID id) const override;
  const std::string& GetAppIDForShelfID(ShelfID id) override;
  void PinAppWithID(const std::string& app_id) override;
  bool IsAppPinned(const std::string& app_id) override;
  void UnpinAppWithID(const std::string& app_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_SHELF_DELEGATE_IMPL_H_

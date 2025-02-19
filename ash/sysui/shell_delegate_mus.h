// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSUI_SHELL_DELEGATE_MUS_H_
#define ASH_SYSUI_SHELL_DELEGATE_MUS_H_

#include <memory>

#include "ash/common/shell_delegate.h"
#include "base/macros.h"

namespace keyboard {
class KeyboardUI;
}

namespace ash {

class AppListPresenterMus;

namespace sysui {

class ShellDelegateMus : public ShellDelegate {
 public:
  explicit ShellDelegateMus(std::unique_ptr<AppListPresenterMus>);
  ~ShellDelegateMus() override;

  bool IsFirstRunAfterBoot() const override;
  bool IsIncognitoAllowed() const override;
  bool IsMultiProfilesEnabled() const override;
  bool IsRunningInForcedAppMode() const override;
  bool CanShowWindowForUser(WmWindow* window) const override;
  bool IsForceMaximizeOnFirstRun() const override;
  void PreInit() override;
  void PreShutdown() override;
  void Exit() override;
  keyboard::KeyboardUI* CreateKeyboardUI() override;
  void OpenUrlFromArc(const GURL& url) override;
  app_list::AppListPresenter* GetAppListPresenter() override;
  ShelfDelegate* CreateShelfDelegate(ShelfModel* model) override;
  SystemTrayDelegate* CreateSystemTrayDelegate() override;
  UserWallpaperDelegate* CreateUserWallpaperDelegate() override;
  SessionStateDelegate* CreateSessionStateDelegate() override;
  AccessibilityDelegate* CreateAccessibilityDelegate() override;
  NewWindowDelegate* CreateNewWindowDelegate() override;
  MediaDelegate* CreateMediaDelegate() override;
  std::unique_ptr<PaletteDelegate> CreatePaletteDelegate() override;
  std::unique_ptr<PointerWatcherDelegate> CreatePointerWatcherDelegate()
      override;
  ui::MenuModel* CreateContextMenu(WmShelf* wm_shelf,
                                   const ShelfItem* item) override;
  GPUSupport* CreateGPUSupport() override;
  base::string16 GetProductName() const override;
  gfx::Image GetDeprecatedAcceleratorImage() const override;

 private:
  std::unique_ptr<app_list::AppListPresenter> app_list_presenter_;

  DISALLOW_COPY_AND_ASSIGN(ShellDelegateMus);
};

}  // namespace sysui
}  // namespace ash

#endif  // ASH_SYSUI_SHELL_DELEGATE_MUS_H_

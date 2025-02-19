// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_cocoa.h"
#import "chrome/browser/ui/cocoa/website_settings/permission_bubble_controller.h"
#include "chrome/browser/ui/views/website_settings/permission_prompt_impl.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// Implementation of PermissionPromptImpl's anchor methods for Cocoa
// browsers. In Cocoa browsers there is no parent views::View for the permission
// bubble, so these methods supply an anchor point instead.

views::View* PermissionPromptImpl::GetAnchorView() {
  return nullptr;
}

gfx::Point PermissionPromptImpl::GetAnchorPoint() {
  return gfx::ScreenPointFromNSPoint(
      [PermissionBubbleController getAnchorPointForBrowser:browser_]);
}

views::BubbleBorder::Arrow PermissionPromptImpl::GetAnchorArrow() {
  return [PermissionBubbleController hasVisibleLocationBarForBrowser:browser_]
             ? views::BubbleBorder::TOP_LEFT
             : views::BubbleBorder::NONE;
}

// static
std::unique_ptr<PermissionPrompt> PermissionPrompt::Create(Browser* browser) {
  if (chrome::ToolkitViewsWebUIDialogsEnabled())
    return base::WrapUnique(new PermissionPromptImpl(browser));
  return base::WrapUnique(new PermissionBubbleCocoa(browser));
}

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_popup_header_button.h"

#include "ash/common/ash_constants.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/background.h"
#include "ui/views/painter.h"

namespace ash {

// static
const char TrayPopupHeaderButton::kViewClassName[] =
    "tray/TrayPopupHeaderButton";

TrayPopupHeaderButton::TrayPopupHeaderButton(views::ButtonListener* listener,
                                             int enabled_resource_id,
                                             int disabled_resource_id,
                                             int enabled_resource_id_hover,
                                             int disabled_resource_id_hover,
                                             int accessible_name_id)
    : views::ToggleImageButton(listener) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  SetImage(views::Button::STATE_NORMAL,
           bundle.GetImageNamed(enabled_resource_id).ToImageSkia());
  SetToggledImage(views::Button::STATE_NORMAL,
                  bundle.GetImageNamed(disabled_resource_id).ToImageSkia());
  SetImage(views::Button::STATE_HOVERED,
           bundle.GetImageNamed(enabled_resource_id_hover).ToImageSkia());
  SetToggledImage(
      views::Button::STATE_HOVERED,
      bundle.GetImageNamed(disabled_resource_id_hover).ToImageSkia());
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  SetAccessibleName(bundle.GetLocalizedString(accessible_name_id));
  SetFocusForPlatform();

  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(1, 2, 2, 3)));
}

TrayPopupHeaderButton::~TrayPopupHeaderButton() {}

const char* TrayPopupHeaderButton::GetClassName() const {
  return kViewClassName;
}

gfx::Size TrayPopupHeaderButton::GetPreferredSize() const {
  int size = GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT);
  return gfx::Size(size, size);
}

void TrayPopupHeaderButton::StateChanged() {
  if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    set_background(views::Background::CreateSolidBackground(
        kTrayPopupHoverBackgroundColor));
  } else {
    set_background(nullptr);
  }
  SchedulePaint();
}

}  // namespace ash

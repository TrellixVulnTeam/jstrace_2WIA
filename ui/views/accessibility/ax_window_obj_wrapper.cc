// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_window_obj_wrapper.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/aura/window.h"
#include "ui/views/accessibility/ax_aura_obj_cache.h"
#include "ui/views/widget/widget.h"

namespace views {

AXWindowObjWrapper::AXWindowObjWrapper(aura::Window* window)
    : window_(window), is_alert_(false) {
  window->AddObserver(this);
}

AXWindowObjWrapper::~AXWindowObjWrapper() {
  window_->RemoveObserver(this);
  window_ = NULL;
}

AXAuraObjWrapper* AXWindowObjWrapper::GetParent() {
  if (!window_->parent())
    return NULL;

  return AXAuraObjCache::GetInstance()->GetOrCreate(window_->parent());
}

void AXWindowObjWrapper::GetChildren(
    std::vector<AXAuraObjWrapper*>* out_children) {
  aura::Window::Windows children = window_->children();
  for (size_t i = 0; i < children.size(); ++i) {
    if (!children[i]->IsVisible())
      continue;
    out_children->push_back(
        AXAuraObjCache::GetInstance()->GetOrCreate(children[i]));
  }

  // Also consider any associated widgets as children.
  Widget* widget = Widget::GetWidgetForNativeView(window_);
  if (widget && widget->IsVisible())
    out_children->push_back(AXAuraObjCache::GetInstance()->GetOrCreate(widget));
}

void AXWindowObjWrapper::Serialize(ui::AXNodeData* out_node_data) {
  out_node_data->id = GetID();
  out_node_data->role = is_alert_ ? ui::AX_ROLE_ALERT : ui::AX_ROLE_WINDOW;
  out_node_data->AddStringAttribute(ui::AX_ATTR_NAME,
                                    base::UTF16ToUTF8(window_->title()));
  out_node_data->state = 0;
  out_node_data->location = gfx::RectF(window_->bounds());
}

int32_t AXWindowObjWrapper::GetID() {
  return AXAuraObjCache::GetInstance()->GetID(window_);
}

void AXWindowObjWrapper::OnWindowDestroyed(aura::Window* window) {
  AXAuraObjCache::GetInstance()->Remove(window);
}

}  // namespace views

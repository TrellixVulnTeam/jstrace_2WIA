// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_tooltip_manager.h"

#include "ash/common/shelf/shelf_view.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const int kTooltipAppearanceDelay = 1000;  // msec

// Tooltip layout constants.

// Shelf item tooltip height.
const int kTooltipHeight = 24;

// The maximum width of the tooltip bubble.  Borrowed the value from
// ash/tooltip/tooltip_controller.cc
const int kTooltipMaxWidth = 250;

// Shelf item tooltip internal text margins.
const int kTooltipTopBottomMargin = 4;
const int kTooltipLeftRightMargin = 8;

// The offset for the tooltip bubble - making sure that the bubble is spaced
// with a fixed gap. The gap is accounted for by the transparent arrow in the
// bubble and an additional 1px padding for the shelf item views.
const int kArrowTopBottomOffset = 1;
const int kArrowLeftRightOffset = 1;

// Tooltip's border interior thickness that defines its minimum height.
const int kBorderInteriorThickness = kTooltipHeight / 2;

}  // namespace

// The implementation of tooltip of the launcher.
class ShelfTooltipManager::ShelfTooltipBubble
    : public views::BubbleDialogDelegateView {
 public:
  ShelfTooltipBubble(views::View* anchor,
                     views::BubbleBorder::Arrow arrow,
                     const base::string16& text)
      : views::BubbleDialogDelegateView(anchor, arrow) {
    gfx::Insets insets(kArrowTopBottomOffset, kArrowLeftRightOffset);
    // Adjust the anchor location for asymmetrical borders of shelf item.
    if (anchor->border())
      insets += anchor->border()->GetInsets();

    set_anchor_view_insets(insets);
    set_close_on_deactivate(false);
    set_can_activate(false);
    set_accept_events(false);
    set_margins(gfx::Insets(kTooltipTopBottomMargin, kTooltipLeftRightMargin));
    set_shadow(views::BubbleBorder::NO_ASSETS);
    SetLayoutManager(new views::FillLayout());
    views::Label* label = new views::Label(text);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    ui::NativeTheme* theme = anchor->GetWidget()->GetNativeTheme();
    label->SetEnabledColor(
        theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipText));
    SkColor background_color =
        theme->GetSystemColor(ui::NativeTheme::kColorId_TooltipBackground);
    set_color(background_color);
    label->SetBackgroundColor(background_color);
    AddChildView(label);
    views::BubbleDialogDelegateView::CreateBubble(this);
    // The bubble border has its own background so the background created by the
    // BubbleDialogDelegateView is redundant and would cause extra opacity.
    set_background(nullptr);
    SetArrowPaintType(views::BubbleBorder::PAINT_TRANSPARENT);
    SetBorderInteriorThickness(kBorderInteriorThickness);
  }

 private:
  // BubbleDialogDelegateView overrides:
  gfx::Size GetPreferredSize() const override {
    const gfx::Size size = BubbleDialogDelegateView::GetPreferredSize();
    const int kTooltipMinHeight = kTooltipHeight - 2 * kTooltipTopBottomMargin;
    return gfx::Size(std::min(size.width(), kTooltipMaxWidth),
                     std::max(size.height(), kTooltipMinHeight));
  }

  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* bubble_widget) const override {
    // Place the bubble in the same display as the anchor.
    WmLookup::Get()
        ->GetWindowForWidget(anchor_widget())
        ->GetRootWindowController()
        ->ConfigureWidgetInitParamsForContainer(
            bubble_widget, kShellWindowId_SettingBubbleContainer, params);
  }

  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_NONE; }

  DISALLOW_COPY_AND_ASSIGN(ShelfTooltipBubble);
};

ShelfTooltipManager::ShelfTooltipManager(ShelfView* shelf_view)
    : timer_delay_(kTooltipAppearanceDelay),
      shelf_view_(shelf_view),
      bubble_(nullptr),
      weak_factory_(this) {
  shelf_view_->wm_shelf()->AddObserver(this);
  WmShell::Get()->AddPointerWatcher(this);
}

ShelfTooltipManager::~ShelfTooltipManager() {
  WmShell::Get()->RemovePointerWatcher(this);
  shelf_view_->wm_shelf()->RemoveObserver(this);
  WmWindow* window = nullptr;
  if (shelf_view_->GetWidget())
    window = WmLookup::Get()->GetWindowForWidget(shelf_view_->GetWidget());
  if (window)
    window->RemoveLimitedPreTargetHandler(this);
}

void ShelfTooltipManager::Init() {
  WmWindow* window =
      WmLookup::Get()->GetWindowForWidget(shelf_view_->GetWidget());
  window->AddLimitedPreTargetHandler(this);
}

void ShelfTooltipManager::Close() {
  timer_.Stop();
  if (bubble_)
    bubble_->GetWidget()->Close();
  bubble_ = nullptr;
}

bool ShelfTooltipManager::IsVisible() const {
  return bubble_ && bubble_->GetWidget()->IsVisible();
}

views::View* ShelfTooltipManager::GetCurrentAnchorView() const {
  return bubble_ ? bubble_->GetAnchorView() : nullptr;
}

void ShelfTooltipManager::ShowTooltip(views::View* view) {
  timer_.Stop();
  if (bubble_) {
    // Cancel the hiding animation to hide the old bubble immediately.
    WmLookup::Get()
        ->GetWindowForWidget(bubble_->GetWidget())
        ->SetVisibilityAnimationTransition(::wm::ANIMATE_NONE);
    Close();
  }

  if (!ShouldShowTooltipForView(view))
    return;

  views::BubbleBorder::Arrow arrow = views::BubbleBorder::Arrow::NONE;
  switch (shelf_view_->wm_shelf()->GetAlignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      arrow = views::BubbleBorder::BOTTOM_CENTER;
      break;
    case SHELF_ALIGNMENT_LEFT:
      arrow = views::BubbleBorder::LEFT_CENTER;
      break;
    case SHELF_ALIGNMENT_RIGHT:
      arrow = views::BubbleBorder::RIGHT_CENTER;
      break;
  }

  base::string16 text = shelf_view_->GetTitleForView(view);
  bubble_ = new ShelfTooltipBubble(view, arrow, text);
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(bubble_->GetWidget());
  window->SetVisibilityAnimationType(
      ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
  window->SetVisibilityAnimationTransition(::wm::ANIMATE_HIDE);
  bubble_->GetWidget()->Show();
}

void ShelfTooltipManager::ShowTooltipWithDelay(views::View* view) {
  if (ShouldShowTooltipForView(view)) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(timer_delay_),
                 base::Bind(&ShelfTooltipManager::ShowTooltip,
                            weak_factory_.GetWeakPtr(), view));
  }
}

void ShelfTooltipManager::OnMousePressed(const ui::MouseEvent& event,
                                         const gfx::Point& location_in_screen,
                                         views::Widget* target) {
  // Close on any mouse press events inside or outside the tooltip.
  Close();
}

void ShelfTooltipManager::OnTouchPressed(const ui::TouchEvent& event,
                                         const gfx::Point& location_in_screen,
                                         views::Widget* target) {
  // Close on any touch press events inside or outside the tooltip.
  Close();
}

void ShelfTooltipManager::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_EXITED) {
    Close();
    return;
  }

  if (event->type() != ui::ET_MOUSE_MOVED)
    return;

  gfx::Point point = event->location();
  views::View::ConvertPointFromWidget(shelf_view_, &point);
  views::View* view = shelf_view_->GetTooltipHandlerForPoint(point);
  const bool should_show = ShouldShowTooltipForView(view);

  timer_.Stop();
  if (IsVisible() && should_show && bubble_->GetAnchorView() != view)
    ShowTooltip(view);
  else if (!IsVisible() && should_show)
    ShowTooltipWithDelay(view);
  else if (IsVisible() && shelf_view_->ShouldHideTooltip(point))
    Close();
}

void ShelfTooltipManager::WillChangeVisibilityState(
    ShelfVisibilityState new_state) {
  if (new_state == SHELF_HIDDEN)
    Close();
}

void ShelfTooltipManager::OnAutoHideStateChanged(ShelfAutoHideState new_state) {
  if (new_state == SHELF_AUTO_HIDE_HIDDEN) {
    timer_.Stop();
    // AutoHide state change happens during an event filter, so immediate close
    // may cause a crash in the HandleMouseEvent() after the filter.  So we just
    // schedule the Close here.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ShelfTooltipManager::Close, weak_factory_.GetWeakPtr()));
  }
}

bool ShelfTooltipManager::ShouldShowTooltipForView(views::View* view) {
  return shelf_view_ && shelf_view_->ShouldShowTooltipForView(view) &&
         shelf_view_->wm_shelf()->GetVisibilityState() == SHELF_VISIBLE;
}

}  // namespace ash

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/frame/custom_frame_view_ash.h"

#include <algorithm>
#include <vector>

#include "ash/common/ash_switches.h"
#include "ash/common/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/common/frame/default_header_painter.h"
#include "ash/common/frame/frame_border_hit_test.h"
#include "ash/common/frame/header_painter.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_observer.h"
#include "ash/common/wm/immersive/wm_immersive_fullscreen_controller.h"
#include "ash/common/wm/immersive/wm_immersive_fullscreen_controller_delegate.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/window_state_delegate.h"
#include "ash/common/wm/window_state_observer.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_observer.h"
#include "ash/common/wm_window_property.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAshWindowStateDelegate

// Handles a user's fullscreen request (Shift+F4/F4). Puts the window into
// immersive fullscreen if immersive fullscreen is enabled for non-browser
// windows.
class CustomFrameViewAshWindowStateDelegate : public wm::WindowStateDelegate,
                                              public wm::WindowStateObserver,
                                              public WmWindowObserver {
 public:
  CustomFrameViewAshWindowStateDelegate(wm::WindowState* window_state,
                                        CustomFrameViewAsh* custom_frame_view)
      : window_state_(nullptr) {
    immersive_fullscreen_controller_ =
        WmShell::Get()->CreateImmersiveFullscreenController();
    if (immersive_fullscreen_controller_) {
      custom_frame_view->InitImmersiveFullscreenControllerForView(
          immersive_fullscreen_controller_.get());
    }

    // Add a window state observer to exit fullscreen properly in case
    // fullscreen is exited without going through
    // WindowState::ToggleFullscreen(). This is the case when exiting
    // immersive fullscreen via the "Restore" window control.
    // TODO(pkotwicz): This is a hack. Remove ASAP. http://crbug.com/319048
    window_state_ = window_state;
    window_state_->AddObserver(this);
    window_state_->window()->AddObserver(this);
  }
  ~CustomFrameViewAshWindowStateDelegate() override {
    if (window_state_) {
      window_state_->RemoveObserver(this);
      window_state_->window()->RemoveObserver(this);
    }
  }

 private:
  // Overridden from wm::WindowStateDelegate:
  bool ToggleFullscreen(wm::WindowState* window_state) override {
    bool enter_fullscreen = !window_state->IsFullscreen();
    if (enter_fullscreen)
      window_state_->window()->SetShowState(ui::SHOW_STATE_FULLSCREEN);
    else
      window_state->Restore();
    if (immersive_fullscreen_controller_) {
      immersive_fullscreen_controller_->SetEnabled(
          WmImmersiveFullscreenController::WINDOW_TYPE_OTHER, enter_fullscreen);
    }
    return true;
  }
  // Overridden from WmWindowObserver:
  void OnWindowDestroying(WmWindow* window) override {
    window_state_->RemoveObserver(this);
    window->RemoveObserver(this);
    window_state_ = nullptr;
  }
  // Overridden from wm::WindowStateObserver:
  void OnPostWindowStateTypeChange(wm::WindowState* window_state,
                                   wm::WindowStateType old_type) override {
    if (!window_state->IsFullscreen() && !window_state->IsMinimized() &&
        immersive_fullscreen_controller_ &&
        immersive_fullscreen_controller_->IsEnabled()) {
      immersive_fullscreen_controller_->SetEnabled(
          WmImmersiveFullscreenController::WINDOW_TYPE_OTHER, false);
    }
  }

  wm::WindowState* window_state_;
  std::unique_ptr<WmImmersiveFullscreenController>
      immersive_fullscreen_controller_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewAshWindowStateDelegate);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh::HeaderView

// View which paints the header. It slides off and on screen in immersive
// fullscreen.
class CustomFrameViewAsh::HeaderView
    : public views::View,
      public WmImmersiveFullscreenControllerDelegate,
      public ShellObserver {
 public:
  // |frame| is the widget that the caption buttons act on.
  explicit HeaderView(views::Widget* frame);
  ~HeaderView() override;

  // Schedules a repaint for the entire title.
  void SchedulePaintForTitle();

  // Tells the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Returns the amount of the view's pixels which should be on screen.
  int GetPreferredOnScreenHeight() const;

  // Returns the view's preferred height.
  int GetPreferredHeight() const;

  // Returns the view's minimum width.
  int GetMinimumWidth() const;

  void UpdateAvatarIcon();

  void SizeConstraintsChanged();

  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);

  // views::View:
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // ShellObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

  FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

  views::View* avatar_icon() const { return avatar_icon_; }

 private:
  // WmImmersiveFullscreenControllerDelegate:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenExited() override;
  void SetVisibleFraction(double visible_fraction) override;
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override;

  // The widget that the caption buttons act on.
  views::Widget* frame_;

  // Helper for painting the header.
  std::unique_ptr<DefaultHeaderPainter> header_painter_;

  views::ImageView* avatar_icon_;

  // View which contains the window caption buttons.
  FrameCaptionButtonContainerView* caption_button_container_;

  // The fraction of the header's height which is visible while in fullscreen.
  // This value is meaningless when not in fullscreen.
  double fullscreen_visible_fraction_;

  DISALLOW_COPY_AND_ASSIGN(HeaderView);
};

CustomFrameViewAsh::HeaderView::HeaderView(views::Widget* frame)
    : frame_(frame),
      header_painter_(new DefaultHeaderPainter),
      avatar_icon_(NULL),
      caption_button_container_(NULL),
      fullscreen_visible_fraction_(0) {
  caption_button_container_ = new FrameCaptionButtonContainerView(frame_);
  caption_button_container_->UpdateSizeButtonVisibility();
  AddChildView(caption_button_container_);

  header_painter_->Init(frame_, this, caption_button_container_);
  UpdateAvatarIcon();

  WmShell::Get()->AddShellObserver(this);
}

CustomFrameViewAsh::HeaderView::~HeaderView() {
  WmShell::Get()->RemoveShellObserver(this);
}

void CustomFrameViewAsh::HeaderView::SchedulePaintForTitle() {
  header_painter_->SchedulePaintForTitle();
}

void CustomFrameViewAsh::HeaderView::ResetWindowControls() {
  caption_button_container_->ResetWindowControls();
}

int CustomFrameViewAsh::HeaderView::GetPreferredOnScreenHeight() const {
  if (frame_->IsFullscreen()) {
    return static_cast<int>(GetPreferredHeight() *
                            fullscreen_visible_fraction_);
  }
  return GetPreferredHeight();
}

int CustomFrameViewAsh::HeaderView::GetPreferredHeight() const {
  return header_painter_->GetHeaderHeightForPainting();
}

int CustomFrameViewAsh::HeaderView::GetMinimumWidth() const {
  return header_painter_->GetMinimumHeaderWidth();
}

void CustomFrameViewAsh::HeaderView::UpdateAvatarIcon() {
  SessionStateDelegate* delegate = WmShell::Get()->GetSessionStateDelegate();
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(frame_);
  bool show = delegate->ShouldShowAvatar(window);
  if (!show) {
    if (!avatar_icon_)
      return;
    delete avatar_icon_;
    avatar_icon_ = NULL;
  } else {
    gfx::ImageSkia image = delegate->GetAvatarImageForWindow(window);
    DCHECK_EQ(image.width(), image.height());
    if (!avatar_icon_) {
      avatar_icon_ = new views::ImageView();
      AddChildView(avatar_icon_);
    }
    avatar_icon_->SetImage(image);
  }
  header_painter_->UpdateLeftHeaderView(avatar_icon_);
  Layout();
}

void CustomFrameViewAsh::HeaderView::SizeConstraintsChanged() {
  caption_button_container_->ResetWindowControls();
  caption_button_container_->UpdateSizeButtonVisibility();
  Layout();
}

void CustomFrameViewAsh::HeaderView::SetFrameColors(
    SkColor active_frame_color,
    SkColor inactive_frame_color) {
  header_painter_->SetFrameColors(active_frame_color, inactive_frame_color);
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh::HeaderView, views::View overrides:

void CustomFrameViewAsh::HeaderView::Layout() {
  header_painter_->LayoutHeader();
}

void CustomFrameViewAsh::HeaderView::OnPaint(gfx::Canvas* canvas) {
  bool paint_as_active =
      frame_->non_client_view()->frame_view()->ShouldPaintAsActive();
  caption_button_container_->SetPaintAsActive(paint_as_active);

  HeaderPainter::Mode header_mode = paint_as_active
                                        ? HeaderPainter::MODE_ACTIVE
                                        : HeaderPainter::MODE_INACTIVE;
  header_painter_->PaintHeader(canvas, header_mode);
}

void CustomFrameViewAsh::HeaderView::ChildPreferredSizeChanged(
    views::View* child) {
  // FrameCaptionButtonContainerView animates the visibility changes in
  // UpdateSizeButtonVisibility(false). Due to this a new size is not available
  // until the completion of the animation. Layout in response to the preferred
  // size changes.
  if (child != caption_button_container_)
    return;
  parent()->Layout();
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh::HeaderView, ShellObserver overrides:

void CustomFrameViewAsh::HeaderView::OnOverviewModeStarting() {
  if (MaterialDesignController::IsOverviewMaterial())
    caption_button_container_->SetVisible(false);
}

void CustomFrameViewAsh::HeaderView::OnOverviewModeEnded() {
  if (MaterialDesignController::IsOverviewMaterial())
    caption_button_container_->SetVisible(true);
}

void CustomFrameViewAsh::HeaderView::OnMaximizeModeStarted() {
  caption_button_container_->UpdateSizeButtonVisibility();
  parent()->Layout();
}

void CustomFrameViewAsh::HeaderView::OnMaximizeModeEnded() {
  caption_button_container_->UpdateSizeButtonVisibility();
  parent()->Layout();
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh::HeaderView,
//   WmImmersiveFullscreenControllerDelegate overrides:

void CustomFrameViewAsh::HeaderView::OnImmersiveRevealStarted() {
  fullscreen_visible_fraction_ = 0;
  SetPaintToLayer(true);
  layer()->SetFillsBoundsOpaquely(false);
  parent()->Layout();
}

void CustomFrameViewAsh::HeaderView::OnImmersiveRevealEnded() {
  fullscreen_visible_fraction_ = 0;
  SetPaintToLayer(false);
  parent()->Layout();
}

void CustomFrameViewAsh::HeaderView::OnImmersiveFullscreenExited() {
  fullscreen_visible_fraction_ = 0;
  SetPaintToLayer(false);
  parent()->Layout();
}

void CustomFrameViewAsh::HeaderView::SetVisibleFraction(
    double visible_fraction) {
  if (fullscreen_visible_fraction_ != visible_fraction) {
    fullscreen_visible_fraction_ = visible_fraction;
    parent()->Layout();
  }
}

std::vector<gfx::Rect>
CustomFrameViewAsh::HeaderView::GetVisibleBoundsInScreen() const {
  // TODO(pkotwicz): Implement views::View::ConvertRectToScreen().
  gfx::Rect visible_bounds(GetVisibleBounds());
  gfx::Point visible_origin_in_screen(visible_bounds.origin());
  views::View::ConvertPointToScreen(this, &visible_origin_in_screen);
  std::vector<gfx::Rect> bounds_in_screen;
  bounds_in_screen.push_back(
      gfx::Rect(visible_origin_in_screen, visible_bounds.size()));
  return bounds_in_screen;
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh::OverlayView

// View which takes up the entire widget and contains the HeaderView. HeaderView
// is a child of OverlayView to avoid creating a larger texture than necessary
// when painting the HeaderView to its own layer.
class CustomFrameViewAsh::OverlayView : public views::View,
                                        public views::ViewTargeterDelegate {
 public:
  explicit OverlayView(HeaderView* header_view);
  ~OverlayView() override;

  // views::View:
  void Layout() override;

 private:
  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  HeaderView* header_view_;

  DISALLOW_COPY_AND_ASSIGN(OverlayView);
};

CustomFrameViewAsh::OverlayView::OverlayView(HeaderView* header_view)
    : header_view_(header_view) {
  AddChildView(header_view);
  SetEventTargeter(
      std::unique_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
}

CustomFrameViewAsh::OverlayView::~OverlayView() {}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh::OverlayView, views::View overrides:

void CustomFrameViewAsh::OverlayView::Layout() {
  // Layout |header_view_| because layout affects the result of
  // GetPreferredOnScreenHeight().
  header_view_->Layout();

  int onscreen_height = header_view_->GetPreferredOnScreenHeight();
  if (onscreen_height == 0) {
    header_view_->SetVisible(false);
  } else {
    int height = header_view_->GetPreferredHeight();
    header_view_->SetBounds(0, onscreen_height - height, width(), height);
    header_view_->SetVisible(true);
  }
}

///////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh::OverlayView, views::ViewTargeterDelegate overrides:

bool CustomFrameViewAsh::OverlayView::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  CHECK_EQ(target, this);
  // Grab events in the header view. Return false for other events so that they
  // can be handled by the client view.
  return header_view_->HitTestRect(rect);
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, public:

// static
const char CustomFrameViewAsh::kViewClassName[] = "CustomFrameViewAsh";

CustomFrameViewAsh::CustomFrameViewAsh(views::Widget* frame)
    : frame_(frame), header_view_(new HeaderView(frame)) {
  WmWindow* frame_window = WmLookup::Get()->GetWindowForWidget(frame);
  frame_window->InstallResizeHandleWindowTargeter(nullptr);
  // |header_view_| is set as the non client view's overlay view so that it can
  // overlay the web contents in immersive fullscreen.
  frame->non_client_view()->SetOverlayView(new OverlayView(header_view_));

  // A delegate for a more complex way of fullscreening the window may already
  // be set. This is the case for packaged apps.
  wm::WindowState* window_state = frame_window->GetWindowState();
  if (!window_state->HasDelegate()) {
    window_state->SetDelegate(std::unique_ptr<wm::WindowStateDelegate>(
        new CustomFrameViewAshWindowStateDelegate(window_state, this)));
  }
}

CustomFrameViewAsh::~CustomFrameViewAsh() {}

void CustomFrameViewAsh::InitImmersiveFullscreenControllerForView(
    WmImmersiveFullscreenController* immersive_fullscreen_controller) {
  immersive_fullscreen_controller->Init(header_view_, frame_, header_view_);
}

void CustomFrameViewAsh::SetFrameColors(SkColor active_frame_color,
                                        SkColor inactive_frame_color) {
  header_view_->SetFrameColors(active_frame_color, inactive_frame_color);
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, views::NonClientFrameView overrides:

gfx::Rect CustomFrameViewAsh::GetBoundsForClientView() const {
  gfx::Rect client_bounds = bounds();
  client_bounds.Inset(0, NonClientTopBorderHeight(), 0, 0);
  return client_bounds;
}

gfx::Rect CustomFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(0, -NonClientTopBorderHeight(), 0, 0);
  return window_bounds;
}

int CustomFrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  return FrameBorderNonClientHitTest(
      this, header_view_->caption_button_container(), point);
}

void CustomFrameViewAsh::GetWindowMask(const gfx::Size& size,
                                       gfx::Path* window_mask) {
  // No window masks in Aura.
}

void CustomFrameViewAsh::ResetWindowControls() {
  header_view_->ResetWindowControls();
}

void CustomFrameViewAsh::UpdateWindowIcon() {}

void CustomFrameViewAsh::UpdateWindowTitle() {
  header_view_->SchedulePaintForTitle();
}

void CustomFrameViewAsh::SizeConstraintsChanged() {
  header_view_->SizeConstraintsChanged();
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, views::View overrides:

gfx::Size CustomFrameViewAsh::GetPreferredSize() const {
  gfx::Size pref = frame_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

void CustomFrameViewAsh::Layout() {
  views::NonClientFrameView::Layout();
  WmWindow* frame_window = WmLookup::Get()->GetWindowForWidget(frame_);
  frame_window->SetIntProperty(WmWindowProperty::TOP_VIEW_INSET,
                               NonClientTopBorderHeight());
}

const char* CustomFrameViewAsh::GetClassName() const {
  return kViewClassName;
}

gfx::Size CustomFrameViewAsh::GetMinimumSize() const {
  gfx::Size min_client_view_size(frame_->client_view()->GetMinimumSize());
  return gfx::Size(
      std::max(header_view_->GetMinimumWidth(), min_client_view_size.width()),
      NonClientTopBorderHeight() + min_client_view_size.height());
}

gfx::Size CustomFrameViewAsh::GetMaximumSize() const {
  gfx::Size max_client_size(frame_->client_view()->GetMaximumSize());
  int width = 0;
  int height = 0;

  if (max_client_size.width() > 0)
    width = std::max(header_view_->GetMinimumWidth(), max_client_size.width());
  if (max_client_size.height() > 0)
    height = NonClientTopBorderHeight() + max_client_size.height();

  return gfx::Size(width, height);
}

void CustomFrameViewAsh::SchedulePaintInRect(const gfx::Rect& r) {
  // We may end up here before |header_view_| has been added to the Widget.
  if (header_view_->GetWidget()) {
    // The HeaderView is not a child of CustomFrameViewAsh. Redirect the paint
    // to HeaderView instead.
    gfx::RectF to_paint(r);
    views::View::ConvertRectToTarget(this, header_view_, &to_paint);
    header_view_->SchedulePaintInRect(gfx::ToEnclosingRect(to_paint));
  } else {
    views::NonClientFrameView::SchedulePaintInRect(r);
  }
}

void CustomFrameViewAsh::VisibilityChanged(views::View* starting_from,
                                           bool is_visible) {
  if (is_visible)
    header_view_->UpdateAvatarIcon();
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, views::ViewTargeterDelegate overrides:

views::View* CustomFrameViewAsh::GetHeaderView() {
  return header_view_;
}

const views::View* CustomFrameViewAsh::GetAvatarIconViewForTest() const {
  return header_view_->avatar_icon();
}

////////////////////////////////////////////////////////////////////////////////
// CustomFrameViewAsh, private:

// views::NonClientFrameView:
bool CustomFrameViewAsh::DoesIntersectRect(const views::View* target,
                                           const gfx::Rect& rect) const {
  CHECK_EQ(target, this);
  // NonClientView hit tests the NonClientFrameView first instead of going in
  // z-order. Return false so that events get to the OverlayView.
  return false;
}

FrameCaptionButtonContainerView*
CustomFrameViewAsh::GetFrameCaptionButtonContainerViewForTest() {
  return header_view_->caption_button_container();
}

int CustomFrameViewAsh::NonClientTopBorderHeight() const {
  return frame_->IsFullscreen() ? 0 : header_view_->GetPreferredHeight();
}

}  // namespace ash

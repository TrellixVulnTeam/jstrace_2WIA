// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/window_cycle_controller.h"

#include "ash/common/metrics/task_switch_source.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/window_cycle_event_filter.h"
#include "ash/common/wm/window_cycle_list.h"
#include "ash/common/wm_shell.h"
#include "base/metrics/histogram.h"

namespace ash {

namespace {

// Returns the most recently active window from the |window_list| or nullptr
// if the list is empty.
WmWindow* GetActiveWindow(const MruWindowTracker::WindowList& window_list) {
  return window_list.empty() ? nullptr : window_list[0];
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, public:

WindowCycleController::WindowCycleController() {}

WindowCycleController::~WindowCycleController() {}

// static
bool WindowCycleController::CanCycle() {
  // Prevent window cycling if the screen is locked or a modal dialog is open.
  WmShell* wm_shell = WmShell::Get();
  return !wm_shell->GetSessionStateDelegate()->IsScreenLocked() &&
         !wm_shell->IsSystemModalWindowOpen() && !wm_shell->IsPinned();
}

void WindowCycleController::HandleCycleWindow(Direction direction) {
  if (!CanCycle())
    return;

  if (!IsCycling())
    StartCycling();

  Step(direction);
}

void WindowCycleController::StartCycling() {
  MruWindowTracker::WindowList window_list =
      WmShell::Get()->mru_window_tracker()->BuildMruWindowList();

  active_window_before_window_cycle_ = GetActiveWindow(window_list);

  window_cycle_list_.reset(new WindowCycleList(window_list));
  event_filter_ = WmShell::Get()->CreateWindowCycleEventFilter();
  cycle_start_time_ = base::Time::Now();
  WmShell::Get()->RecordUserMetricsAction(UMA_WINDOW_CYCLE);
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowCycleController.Items",
                           window_list.size());
}

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, private:

void WindowCycleController::Step(Direction direction) {
  DCHECK(window_cycle_list_.get());
  window_cycle_list_->Step(direction);
}

void WindowCycleController::StopCycling() {
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowCycleController.SelectionDepth",
                           window_cycle_list_->current_index() + 1);
  window_cycle_list_.reset();

  WmWindow* active_window_after_window_cycle = GetActiveWindow(
      WmShell::Get()->mru_window_tracker()->BuildMruWindowList());

  // Remove our key event filter.
  event_filter_.reset();
  UMA_HISTOGRAM_MEDIUM_TIMES("Ash.WindowCycleController.CycleTime",
                             base::Time::Now() - cycle_start_time_);

  if (active_window_after_window_cycle != nullptr &&
      active_window_before_window_cycle_ != active_window_after_window_cycle) {
    WmShell::Get()->RecordTaskSwitchMetric(
        TaskSwitchSource::WINDOW_CYCLE_CONTROLLER);
  }
  active_window_before_window_cycle_ = nullptr;
}

}  // namespace ash

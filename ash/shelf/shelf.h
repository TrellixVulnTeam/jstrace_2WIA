// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_H_
#define ASH_SHELF_SHELF_H_

#include <stdint.h>

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_locking_manager.h"
#include "ash/common/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget_observer.h"

namespace app_list {
class ApplicationDragAndDropHost;
}

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace views {
class View;
}

namespace ash {
class AppListButton;
class FocusCycler;
class ShelfDelegate;
class ShelfIconObserver;
class ShelfModel;
class ShelfView;
class WmShelf;

namespace test {
class ShelfTestAPI;
}

// Controller for shelf state. All access to state (visibility, auto-hide, etc.)
// should occur via this class.
class ASH_EXPORT Shelf {
 public:
  static const char kNativeViewName[];

  Shelf(ShelfModel* model,
        WmShelf* wm_shelf,
        ShelfWidget* widget);
  ~Shelf();

  // Return the shelf for the primary display. NULL if no user is logged in yet.
  // Useful for tests. For production code use ForWindow() because the user may
  // have multiple displays.
  static Shelf* ForPrimaryDisplay();

  // Return the shelf for the display that |window| is currently on, or a shelf
  // on primary display if the shelf per display feature is disabled. NULL if no
  // user is logged in yet.
  static Shelf* ForWindow(const aura::Window* window);

  void SetAlignment(ShelfAlignment alignment);
  ShelfAlignment alignment() const { return alignment_; }

  // Sets the ShelfAutoHideBehavior. See enum description for details.
  void SetAutoHideBehavior(ShelfAutoHideBehavior auto_hide_behavior);
  ShelfAutoHideBehavior auto_hide_behavior() const {
    return auto_hide_behavior_;
  }

  ShelfAutoHideState GetAutoHideState() const;

  ShelfVisibilityState GetVisibilityState() const;

  // Returns the screen bounds of the item for the specified window. If there is
  // no item for the specified window an empty rect is returned.
  gfx::Rect GetScreenBoundsOfItemIconForWindow(const aura::Window* window);

  // Updates the icon position given the current window bounds. This is used
  // when dragging panels to reposition them with respect to the other panels.
  void UpdateIconPositionForWindow(aura::Window* window);

  // Activates the the shelf item specified by the index in the list of shelf
  // items.
  void ActivateShelfItem(int index);

  // Cycles the window focus linearly over the current shelf items.
  void CycleWindowLinear(CycleDirection direction);

  void AddIconObserver(ShelfIconObserver* observer);
  void RemoveIconObserver(ShelfIconObserver* observer);

  // Returns true if the shelf is showing a context menu.
  bool IsShowingMenu() const;

  bool IsShowingOverflowBubble() const;

  void SetVisible(bool visible) const;
  bool IsVisible() const;

  void SchedulePaint();

  AppListButton* GetAppListButton() const;

  // Launch a 0-indexed shelf item in the shelf.
  // A negative index launches the last shelf item in the shelf.
  void LaunchAppIndexAt(int item_index);

  ShelfWidget* shelf_widget() { return shelf_widget_; }

  // TODO(msw): ShelfLayoutManager should not be accessed externally.
  ShelfLayoutManager* shelf_layout_manager() {
    return shelf_widget_->shelf_layout_manager();
  }

  // Returns rectangle bounding all visible shelf items. Used screen coordinate
  // system.
  gfx::Rect GetVisibleItemsBoundsInScreen() const;

  // Returns ApplicationDragAndDropHost for this shelf.
  app_list::ApplicationDragAndDropHost* GetDragAndDropHostForAppList();

  // Updates the background for the shelf items.
  void UpdateShelfItemBackground(int alpha);

  ShelfLockingManager* shelf_locking_manager_for_testing() {
    return &shelf_locking_manager_;
  }

  ShelfView* shelf_view_for_testing() { return shelf_view_; }

 private:
  friend class test::ShelfTestAPI;

  // The shelf controller. Owned by the root window controller.
  WmShelf* wm_shelf_;
  ShelfWidget* shelf_widget_;
  ShelfView* shelf_view_;
  ShelfLockingManager shelf_locking_manager_;

  ShelfAlignment alignment_ = SHELF_ALIGNMENT_BOTTOM;
  ShelfAutoHideBehavior auto_hide_behavior_ = SHELF_AUTO_HIDE_BEHAVIOR_NEVER;

  DISALLOW_COPY_AND_ASSIGN(Shelf);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_H_

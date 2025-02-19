// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_WINDOW_SERVER_DELEGATE_H_
#define SERVICES_UI_WS_WINDOW_SERVER_DELEGATE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/ui/common/types.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"

namespace ui {

namespace mojom {
class WindowManagerFactory;
class WindowTree;
}

namespace ws {

class Display;
class ServerWindow;
class WindowServer;
class WindowTree;
class WindowTreeBinding;

class WindowServerDelegate {
 public:
  enum BindingType {
    EMBED,
    WINDOW_MANAGER,
  };

  // Called if no Displays have been created, but a WindowManagerFactory has
  // been set.
  virtual void CreateDefaultDisplays() = 0;

  // Called once when the AcceleratedWidget of a Display is available.
  virtual void OnFirstDisplayReady();

  virtual void OnNoMoreDisplays() = 0;

  virtual bool IsTestConfig() const = 0;

  // Creates a WindowTreeBinding. Default implementation returns null, which
  // creates DefaultBinding.
  virtual std::unique_ptr<WindowTreeBinding> CreateWindowTreeBinding(
      BindingType type,
      ws::WindowServer* window_server,
      ws::WindowTree* tree,
      mojom::WindowTreeRequest* tree_request,
      mojom::WindowTreeClientPtr* client);

 protected:
  virtual ~WindowServerDelegate() {}
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_WINDOW_SERVER_DELEGATE_H_

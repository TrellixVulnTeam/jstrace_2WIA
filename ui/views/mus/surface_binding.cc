// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/surface_binding.h"

#include <stdint.h>

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_local.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/cpp/context_provider.h"
#include "services/ui/public/cpp/output_surface.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "ui/views/mus/window_tree_host_mus.h"

namespace views {

// PerClientState --------------------------------------------------------------

// State needed per WindowTreeClient. Provides the real implementation of
// CreateOutputSurface. SurfaceBinding obtains a pointer to the
// PerClientState appropriate for the WindowTreeClient. PerClientState is
// stored in a thread local map. When no more refereces to a PerClientState
// remain the PerClientState is deleted and the underlying map cleaned up.
class SurfaceBinding::PerClientState : public base::RefCounted<PerClientState> {
 public:
  static PerClientState* Get(ui::WindowTreeClient* client);

  std::unique_ptr<cc::OutputSurface> CreateOutputSurface(
      ui::Window* window,
      ui::mojom::SurfaceType type);

 private:
  typedef std::map<ui::WindowTreeClient*, PerClientState*> ClientToStateMap;

  friend class base::RefCounted<PerClientState>;

  explicit PerClientState(ui::WindowTreeClient* client);
  ~PerClientState();

  static base::LazyInstance<
      base::ThreadLocalPointer<ClientToStateMap>>::Leaky window_states;

  ui::WindowTreeClient* client_;

  DISALLOW_COPY_AND_ASSIGN(PerClientState);
};

// static
base::LazyInstance<base::ThreadLocalPointer<
    SurfaceBinding::PerClientState::ClientToStateMap>>::Leaky
    SurfaceBinding::PerClientState::window_states;

// static
SurfaceBinding::PerClientState* SurfaceBinding::PerClientState::Get(
    ui::WindowTreeClient* client) {
  ClientToStateMap* window_map = window_states.Pointer()->Get();
  if (!window_map) {
    window_map = new ClientToStateMap;
    window_states.Pointer()->Set(window_map);
  }
  if (!(*window_map)[client])
    (*window_map)[client] = new PerClientState(client);
  return (*window_map)[client];
}

std::unique_ptr<cc::OutputSurface>
SurfaceBinding::PerClientState::CreateOutputSurface(
    ui::Window* window,
    ui::mojom::SurfaceType surface_type) {
  scoped_refptr<cc::ContextProvider> context_provider(new ui::ContextProvider);
  return base::WrapUnique(new ui::OutputSurface(
      context_provider, window->RequestSurface(surface_type)));
}

SurfaceBinding::PerClientState::PerClientState(ui::WindowTreeClient* client)
    : client_(client) {}

SurfaceBinding::PerClientState::~PerClientState() {
  ClientToStateMap* window_map = window_states.Pointer()->Get();
  DCHECK(window_map);
  DCHECK_EQ(this, (*window_map)[client_]);
  window_map->erase(client_);
  if (window_map->empty()) {
    delete window_map;
    window_states.Pointer()->Set(nullptr);
  }
}

// SurfaceBinding --------------------------------------------------------------

SurfaceBinding::SurfaceBinding(ui::Window* window,
                               ui::mojom::SurfaceType surface_type)
    : window_(window),
      surface_type_(surface_type),
      state_(PerClientState::Get(window->window_tree())) {}

SurfaceBinding::~SurfaceBinding() {}

std::unique_ptr<cc::OutputSurface> SurfaceBinding::CreateOutputSurface() {
  return state_ ? state_->CreateOutputSurface(window_, surface_type_) : nullptr;
}

}  // namespace views

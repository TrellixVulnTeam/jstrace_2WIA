// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_CONTEXT_PROVIDER_FACTORY_H_
#define UI_ANDROID_CONTEXT_PROVIDER_FACTORY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "ui/android/ui_android_export.h"

namespace cc {
class ContextProvider;
class GpuMemoryBufferManager;
class VulkanContextProvider;
class SharedBitmapManager;
class SurfaceManager;
}

namespace gpu {
namespace gles2 {
struct ContextCreationAttribHelper;
}  // namespace gles

struct SharedMemoryLimits;
class GpuMemoryBufferManager;
}  // namespace gpu

namespace ui {

// This class is not thread-safe and should only be accessed from the UI thread.
class UI_ANDROID_EXPORT ContextProviderFactory {
 public:
  using ContextProviderCallback =
      base::Callback<void(const scoped_refptr<cc::ContextProvider>&)>;

  enum class ContextType {
    BLIMP_RENDER_COMPOSITOR_CONTEXT,
    BLIMP_RENDER_WORKER_CONTEXT,
  };

  static ContextProviderFactory* GetInstance();

  // This should only be called once, on startup. Ownership remains with the
  // caller.
  static void SetInstance(ContextProviderFactory* context_provider_factory);

  virtual ~ContextProviderFactory(){};

  virtual scoped_refptr<cc::VulkanContextProvider>
  GetSharedVulkanContextProvider() = 0;

  // Creates an offscreen ContextProvider for the compositor. Any shared
  // contexts passed here *must* have been created using this factory.
  // The callback may be triggered synchronously if possible, and will always
  // have the context provider.
  virtual void CreateOffscreenContextProvider(
      ContextType context_type,
      gpu::SharedMemoryLimits shared_memory_limits,
      gpu::gles2::ContextCreationAttribHelper attributes,
      bool support_locking,
      bool automatic_flushes,
      cc::ContextProvider* shared_context_provider,
      ContextProviderCallback result_callback) = 0;

  virtual cc::SurfaceManager* GetSurfaceManager() = 0;

  virtual uint32_t AllocateSurfaceClientId() = 0;

  virtual cc::SharedBitmapManager* GetSharedBitmapManager() = 0;

  virtual gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;
};

}  // namespace ui

#endif  // UI_ANDROID_CONTEXT_PROVIDER_FACTORY_H_

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_
#define UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/native_library.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/ozone_base_export.h"
#include "ui/ozone/public/native_pixmap.h"

namespace ui {

class NativePixmap;
class SurfaceOzoneCanvas;

// The Ozone interface allows external implementations to hook into Chromium to
// provide a system specific implementation. The Ozone interface supports two
// drawing modes: 1) accelerated drawing using GL and 2) software drawing
// through Skia.
//
// If you want to paint on a window with ozone, you need to create a GLSurface
// or SurfaceOzoneCanvas for that window. The platform can support software, GL,
// or both for painting on the window. The following functionality is specific
// to the drawing mode and may not have any meaningful implementation in the
// other mode. An implementation must provide functionality for at least one
// mode.
//
// 1) Accelerated Drawing (GL path):
//
// The following functions are specific to GL:
//  - GetNativeDisplay (EGL only)
//  - LoadEGLGLES2Bindings (EGL only)
//  - CreateViewGLSurface (all GL implementations)
//  - CreateSurfacelessViewGLSurface (EGL only)
//  - CreateOffscreenGLSurface (all GL implementations)
//
// 2) Software Drawing (Skia):
//
// The following function is specific to the software path:
//  - CreateCanvasForWidget
//
// The accelerated path can optionally provide support for the software drawing
// path.
//
// The remaining functions are not covered since they are needed in both drawing
// modes (See comments bellow for descriptions).
class OZONE_BASE_EXPORT SurfaceFactoryOzone {
 public:
  typedef void* (*GLGetProcAddressProc)(const char* name);
  typedef base::Callback<void(base::NativeLibrary)> AddGLLibraryCallback;
  typedef base::Callback<void(GLGetProcAddressProc)>
      SetGLGetProcAddressProcCallback;

  // Returns native platform display handle. This is used to obtain the EGL
  // display connection for the native display.
  virtual intptr_t GetNativeDisplay();

  // Creates a GL surface that renders directly to a view for the specified GL
  // implementation.
  virtual scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLImplementation implementation,
      gfx::AcceleratedWidget widget);

  // Creates a GL surface that renders directly into a window with surfaceless
  // semantics for the specified GL implementation. The surface is not backed
  // by any buffers and is used for overlay-only displays. This will return
  // nullptr if surfaceless mode unsupported.
  virtual scoped_refptr<gl::GLSurface> CreateSurfacelessViewGLSurface(
      gl::GLImplementation implementation,
      gfx::AcceleratedWidget widget);

  // Creates a GL surface used for offscreen rendering for the specified GL
  // implementation.
  virtual scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLImplementation implementation,
      const gfx::Size& size);

  // Create SurfaceOzoneCanvas for the specified gfx::AcceleratedWidget.
  //
  // Note: The platform must support creation of SurfaceOzoneCanvas from the
  // Browser Process using only the handle contained in gfx::AcceleratedWidget.
  virtual std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget);

  // Sets up GL bindings for the native surface. Takes two callback parameters
  // that allow Ozone to register the GL bindings.
  virtual bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) = 0;

  // Returns all scanout formats for |widget| representing a particular display
  // controller or default display controller for kNullAcceleratedWidget.
  virtual std::vector<gfx::BufferFormat> GetScanoutFormats(
      gfx::AcceleratedWidget widget);

  // Create a single native buffer to be used for overlay planes or zero copy
  // for |widget| representing a particular display controller or default
  // display controller for kNullAcceleratedWidget.
  // It can be called on any thread.
  virtual scoped_refptr<NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage);

  // Create a single native buffer from an existing handle. Takes ownership of
  // |handle| and can be called on any thread.
  virtual scoped_refptr<NativePixmap> CreateNativePixmapFromHandle(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      const gfx::NativePixmapHandle& handle);

 protected:
  SurfaceFactoryOzone();
  virtual ~SurfaceFactoryOzone();

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceFactoryOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_SURFACE_FACTORY_OZONE_H_

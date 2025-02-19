// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_MEDIA_MEDIA_CAPS_OBSERVER_IMPL_H_
#define CHROMECAST_RENDERER_MEDIA_MEDIA_CAPS_OBSERVER_IMPL_H_

#include "base/macros.h"
#include "chromecast/common/media/media_caps.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace chromecast {
namespace media {

class MediaCapsObserverImpl : public mojom::MediaCapsObserver {
 public:
  explicit MediaCapsObserverImpl(mojom::MediaCapsObserverPtr* proxy);
  ~MediaCapsObserverImpl() override;

 private:
  void SupportedHdmiSinkCodecsChanged(
      uint32_t supported_codec_bitmask) override;

  mojo::Binding<mojom::MediaCapsObserver> binding_;

  DISALLOW_COPY_AND_ASSIGN(MediaCapsObserverImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_MEDIA_MEDIA_CAPS_OBSERVER_IMPL_H_

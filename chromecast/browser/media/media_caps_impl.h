// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_MEDIA_CAPS_IMPL_H_
#define CHROMECAST_BROWSER_MEDIA_MEDIA_CAPS_IMPL_H_

#include "base/macros.h"
#include "chromecast/common/media/media_caps.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace chromecast {
namespace media {

class MediaCapsImpl : public mojom::MediaCaps {
 public:
  MediaCapsImpl();
  ~MediaCapsImpl() override;

  void AddBinding(mojom::MediaCapsRequest request);

  void SetSupportedHdmiSinkCodecs(unsigned int supported_codecs_bitmask);

 private:
  // chromecast::mojom::MediaCaps implementation.
  void AddObserver(mojom::MediaCapsObserverPtr observer) override;

  unsigned int supported_codecs_bitmask_;
  mojo::InterfacePtrSet<mojom::MediaCapsObserver> observers_;
  mojo::BindingSet<mojom::MediaCaps> bindings_;

  DISALLOW_COPY_AND_ASSIGN(MediaCapsImpl);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_MEDIA_CAPS_IMPL_H_

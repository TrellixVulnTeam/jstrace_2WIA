// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_MANIFEST_FEATURE_H_
#define EXTENSIONS_COMMON_FEATURES_MANIFEST_FEATURE_H_

#include <string>

#include "extensions/common/features/simple_feature.h"

namespace extensions {

class ManifestFeature : public SimpleFeature {
 public:
  ManifestFeature();
  ~ManifestFeature() override;

  Feature::Availability IsAvailableToContext(
      const Extension* extension,
      Feature::Context context,
      const GURL& url,
      Feature::Platform platform) const override;

  bool Validate(std::string* error) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_MANIFEST_FEATURE_H_

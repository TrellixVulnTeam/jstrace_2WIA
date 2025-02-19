// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBlockLayoutAlgorithm_h
#define NGBlockLayoutAlgorithm_h

namespace blink {

class LayoutBox;
class NGConstraintSpace;

class NGBlockLayoutAlgorithm {
public:
    NGBlockLayoutAlgorithm();

    NGConstraintSpace createConstraintSpaceFromLayoutObject(const LayoutBox&);
};

} // namespace blink

#endif // NGBlockLayoutAlgorithm_h

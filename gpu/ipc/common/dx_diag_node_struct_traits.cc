// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/dx_diag_node_struct_traits.h"

namespace mojo {

// static
bool StructTraits<gpu::mojom::DxDiagNode, gpu::DxDiagNode>::Read(
    gpu::mojom::DxDiagNodeDataView data,
    gpu::DxDiagNode* out) {
  return data.ReadValues(&out->values) && data.ReadChildren(&out->children);
}

}  // namespace mojo

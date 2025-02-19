# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../build/common_untrusted.gypi',
    'ppapi_shared.gypi',
  ],
  'conditions': [
    ['disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'ppapi_shared_nacl',
          'type': 'none',
          'variables': {
            'ppapi_shared_target': 1,
            'nacl_win64_target': 0,
            'nacl_untrusted_build': 1,
            'nlib_target': 'libppapi_shared_nacl.a',
            'build_glibc': 0,
            'build_newlib': 0,
            'build_irt': 1,
            'build_pnacl_newlib': 0,
            'build_nonsfi_helper': 1,
          },
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base_nacl.gyp:base_nacl',
            '../base/base_nacl.gyp:base_nacl_nonsfi',
            '../gpu/command_buffer/command_buffer_nacl.gyp:gles2_utils_nacl',
            '../gpu/gpu_nacl.gyp:command_buffer_client_nacl',
            '../gpu/gpu_nacl.gyp:gles2_implementation_nacl',
            '../media/media_nacl.gyp:shared_memory_support_nacl',
            '../third_party/khronos/khronos.gyp:khronos_headers',
            '../ui/gfx/gfx_nacl.gyp:gfx_geometry_nacl',
          ],
        },
      ],
    }],
  ],
}

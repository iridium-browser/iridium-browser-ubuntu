#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to generate chromium.gpu.json and chromium.gpu.fyi.json in
the src/testing/buildbot directory. Maintaining these files by hand is
too unwieldy.
"""

import copy
import json
import os
import string
import sys

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(os.path.dirname(os.path.dirname(THIS_DIR)))

WATERFALL = {
  'builders': {
    'GPU Win Builder' : {},
    'GPU Win Builder (dbg)' : {},
    'GPU Mac Builder' : {},
    'GPU Mac Builder (dbg)' : {},
    'GPU Linux Builder' : {},
    'GPU Linux Builder (dbg)' : {},
   },

  'testers': {
    'Win7 Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Mac 10.10 Release (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.12'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Debug (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.12'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Debug (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Linux Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'linux',
    },
    'Linux Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'linux',
    },
  }
}

FYI_WATERFALL = {
  'builders': {
    'GPU Win Builder' : {},
    'GPU Win Builder (dbg)' : {},
    'GPU Win x64 Builder' : {},
    'GPU Win x64 Builder (dbg)' : {},
    'GPU Mac Builder' : {},
    'GPU Mac Builder (dbg)' : {},
    'GPU Linux Builder' : {},
    'GPU Linux Builder (dbg)' : {},
    'Linux ChromiumOS Builder' : {
      'additional_compile_targets' : [ "All" ]
    },
    'Linux ChromiumOS Ozone Builder' : {
      'additional_compile_targets' : [ "All" ]
    },
  },

  'testers': {
    'Win7 Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Win10 Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-10'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win10 Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-10'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6613',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Debug (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6613',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 Release (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:041a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win7 Release (NVIDIA GeForce 730)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0f02',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win10 Release (Intel HD 530)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:1912',
          'os': 'Windows-10',
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win10 Debug (Intel HD 530)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:1912',
          'os': 'Windows-10',
        },
      ],
      'build_config': 'Debug',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win7 Release (AMD R5 230)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6779',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'win',
    },
    'Win7 x64 Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release_x64',
      'swarming': True,
      'os_type': 'win',
    },
    'Win7 x64 Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Debug_x64',
      'swarming': True,
      'os_type': 'win',
    },
    'Mac 10.10 Release (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.12'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Debug (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.12'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:679e',
          'os': 'Mac-10.10'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'mac',
    },
    'Mac 10.10 Debug (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:679e',
          'os': 'Mac-10.10'
        },
      ],
      'build_config': 'Debug',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'mac',
    },
    'Mac Retina Release': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0fe9',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac Retina Debug': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0fe9',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.10 Retina Debug (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'mac',
    },
    'Mac 10.11 Retina Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off for testing purposes.
      'swarming': False,
      'os_type': 'mac',
    },
    'Mac Experimental Retina Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0fe9',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off for testing purposes.
      'swarming': False,
      'os_type': 'mac',
    },
    'Mac GPU ASAN Release': {
      # This bot spawns jobs on multiple GPU types.
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.12'
        },
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
      'is_asan': True,
    },
    'Linux Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'linux',
    },
    'Linux Release (Intel Graphics Stack)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:041a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Release (AMD R5 230)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6779',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Release (NVIDIA GeForce 730)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0f02',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Debug (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Debug',
      'swarming': True,
      'os_type': 'linux',
    },
    'Linux Release (Intel HD 530)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:1912',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Debug (Intel HD 530)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:1912',
          'os': 'Linux'
        },
      ],
      'build_config': 'Debug',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Linux Release (AMD R7 240)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6613',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'linux',
    },
    'Android Release (Nexus 5)': {
      'swarming_dimensions': [
        {
          # There are no PCI IDs on Android.
          # This is a hack to get the script working.
          'gpu': '0000:0000',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'android',
    },
    'Android Release (Nexus 5X)': {
      'swarming_dimensions': [
        {
          'device_type': 'bullhead',
          'device_os': 'M',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      'swarming': True,
      'os_type': 'android',
    },
    'Android Release (Nexus 6)': {
      'swarming_dimensions': [
        {
          # There are no PCI IDs on Android.
          # This is a hack to get the script working.
          'gpu': '0000:0000',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'android',
    },
    'Android Release (Nexus 6P)': {
      'swarming_dimensions': [
        {
          # There are no PCI IDs on Android.
          # This is a hack to get the script working.
          'gpu': '0000:0000',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'android',
    },
    'Android Release (Nexus 9)': {
      'swarming_dimensions': [
        {
          # There are no PCI IDs on Android.
          # This is a hack to get the script working.
          'gpu': '0000:0000',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'android',
    },
    'Android Release (Pixel C)': {
      'swarming_dimensions': [
        {
          # There are no PCI IDs on Android.
          # This is a hack to get the script working.
          'gpu': '0000:0000',
          'os': 'Android'
        },
      ],
      'build_config': 'android-chromium',
      # This bot is a one-off and doesn't have similar slaves in the
      # swarming pool.
      'swarming': False,
      'os_type': 'android',
    },

    # The following "optional" testers don't actually exist on the
    # waterfall. They are present here merely to specify additional
    # tests which aren't on the main tryservers. Unfortunately we need
    # a completely different (redundant) bot specification to handle
    # this.
    'Optional Win7 Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Optional Win7 Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6613',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Optional Mac 10.10 Release (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.12'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Optional Mac Retina Release': {
      'swarming_dimensions': [
        {
          'gpu': '10de:0fe9',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Optional Mac 10.10 Retina Release (AMD)': {
      'swarming_dimensions': [
        {
          'gpu': '1002:6821',
          'hidpi': '1',
          'os': 'Mac'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Optional Linux Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'linux',
    },
  }
}

V8_FYI_WATERFALL = {
  'prologue': {
    "V8 Android GN (dbg)": {
      "additional_compile_targets": [
        "chrome_public_apk"
      ],
      "gtest_tests": []
    },
    "V8 Linux GN": {
      "additional_compile_targets": [
        "accessibility_unittests",
        "aura_unittests",
        "browser_tests",
        "cacheinvalidation_unittests",
        "capture_unittests",
        "cast_unittests",
        "cc_unittests",
        "chromedriver_unittests",
        "components_browsertests",
        "components_unittests",
        "content_browsertests",
        "content_unittests",
        "crypto_unittests",
        "dbus_unittests",
        "device_unittests",
        "display_unittests",
        "events_unittests",
        "extensions_browsertests",
        "extensions_unittests",
        "gcm_unit_tests",
        "gfx_unittests",
        "gn_unittests",
        "google_apis_unittests",
        "gpu_ipc_service_unittests",
        "gpu_unittests",
        "interactive_ui_tests",
        "ipc_tests",
        "jingle_unittests",
        "media_unittests",
        "media_blink_unittests",
        "mojo_common_unittests",
        "mojo_public_bindings_unittests",
        "mojo_public_system_unittests",
        "mojo_system_unittests",
        "nacl_loader_unittests",
        "net_unittests",
        "pdf_unittests",
        "ppapi_unittests",
        "printing_unittests",
        "remoting_unittests",
        "sandbox_linux_unittests",
        "skia_unittests",
        "sql_unittests",
        "storage_unittests",
        "sync_integration_tests",
        "ui_base_unittests",
        "ui_touch_selection_unittests",
        "unit_tests",
        "url_unittests",
        "views_unittests",
        "wm_unittests"
      ]
    }
  },
  'testers': {
    'Win Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Windows-2008ServerR2-SP1'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'win',
    },
    'Mac Release (Intel)': {
      'swarming_dimensions': [
        {
          'gpu': '8086:0a2e',
          'os': 'Mac-10.12'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'mac',
    },
    'Linux Release (NVIDIA)': {
      'swarming_dimensions': [
        {
          'gpu': '10de:104a',
          'os': 'Linux'
        },
      ],
      'build_config': 'Release',
      'swarming': True,
      'os_type': 'linux',
    },
  }
}

COMMON_GTESTS = {
  'angle_deqp_egl_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
        # Run only on the Win7 Release NVIDIA 32- and 64-bit bots
        # (and trybots) for the time being, at least until more capacity is
        # added.
        # TODO(jmadill): Run on the Linux Release NVIDIA bots.
        'build_configs': ['Release', 'Release_x64'],
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Windows-2008ServerR2-SP1'
          }
        ],
      },
    ],
    'args': [
      '--test-launcher-batch-limit=400'
    ]
  },

  'angle_deqp_gles2_d3d11_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
        # Run only on the Win7 NVIDIA/AMD R7 240 32- and 64-bit bots (and
        # trybots) for the time being, at least until more capacity is
        # added.
        'build_configs': ['Release', 'Release_x64'],
        'swarming_dimension_sets': [
          # NVIDIA Win 7
          {
            'gpu': '10de:104a',
            'os': 'Windows-2008ServerR2-SP1'
          },
          # AMD Win 7
          {
            'gpu': '1002:6613',
            'os': 'Windows-2008ServerR2-SP1'
          },
        ],
      },
    ],
    'desktop_swarming': {
      'shards': 4,
    },
    'test': 'angle_deqp_gles2_tests',
    'args': [
      '--test-launcher-batch-limit=400',
      '--deqp-egl-display-type=angle-d3d11'
    ]
  },

  'angle_deqp_gles2_gl_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
        # Run only on the Linux Release NVIDIA 32- and 64-bit bots (and
        # trybots) for the time being, at least until more capacity is added.
        'build_configs': ['Release', 'Release_x64'],
        'swarming_dimension_sets': [
          # NVIDIA Linux
          {
            'gpu': '10de:104a',
            'os': 'Linux'
          },
        ],
      },
    ],
    'desktop_swarming': {
      'shards': 4,
    },
    'test': 'angle_deqp_gles2_tests',
    'args': [
      '--test-launcher-batch-limit=400',
      '--deqp-egl-display-type=angle-gl'
    ]
  },

  'angle_deqp_gles2_gles_tests': {
    'tester_configs': [
      {
        'allow_on_android': True,
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
        # Run on Nexus 5X swarmed bots.
        'build_configs': ['android-chromium'],
        'swarming_dimension_sets': [
          # Nexus 5X
          {
            'device_type': 'bullhead',
            'device_os': 'M',
            'os': 'Android'
          }
        ],
      },
    ],
    'test': 'angle_deqp_gles2_tests',
    # Only pass the display type to desktop. The Android runner doesn't support
    # passing args to the executable but only one display type is supported on
    # Android anyways.
    'desktop_args': [
      '--test-launcher-batch-limit=400',
      '--deqp-egl-display-type=angle-gles'
    ],
    'android_args': ['--enable-xml-result-parsing']
  },

  'angle_deqp_gles3_d3d11_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # TODO(jmadill): Run this on ANGLE roll tryservers.
        'run_on_optional': False,
        # Run only on the NVIDIA and AMD Win7 bots (and trybots) for the time
        # being, at least until more capacity is added.
        'build_configs': ['Release'],
        'swarming_dimension_sets': [
          # NVIDIA Win 7
          {
            'gpu': '10de:104a',
            'os': 'Windows-2008ServerR2-SP1'
          },
          # AMD Win 7
          {
            'gpu': '1002:6613',
            'os': 'Windows-2008ServerR2-SP1'
          }
        ],
      }
    ],
    'swarming': {
      'shards': 12,
    },
    'test': 'angle_deqp_gles3_tests',
    'args': [
      '--test-launcher-batch-limit=400',
      '--deqp-egl-display-type=angle-d3d11'
    ]
  },

  'angle_deqp_gles3_gl_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # TODO(jmadill): Run this on ANGLE roll tryservers.
        'run_on_optional': False,
        # Run only on the Linux Release NVIDIA 32-bit bots (and trybots) for
        # the time being, at least until more capacity is added.
        'build_configs': ['Release'],
        'swarming_dimension_sets': [
          # NVIDIA Linux
          {
            'gpu': '10de:104a',
            'os': 'Linux'
          }
        ],
      }
    ],
    'swarming': {
      'shards': 12,
    },
    'test': 'angle_deqp_gles3_tests',
    'args': [
      '--test-launcher-batch-limit=400',
      '--deqp-egl-display-type=angle-gl'
    ]
  },

  'angle_deqp_gles31_d3d11_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'run_on_optional': False,
        # Run on the Win Release NVIDIA bots.
        'build_configs': ['Release'],
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Windows-2008ServerR2-SP1'
          }
        ],
      }
    ],
    'swarming': {
      # TODO(geofflang): Increase the number of shards as more tests start to
      # pass and runtime increases.
      'shards': 4,
    },
    'test': 'angle_deqp_gles31_tests',
    'args': [
      '--test-launcher-batch-limit=400',
      '--deqp-egl-display-type=angle-d3d11'
    ]
  },

  'angle_deqp_gles31_gl_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'run_on_optional': False,
        # Run on the Win/Linux Release NVIDIA bots.
        'build_configs': ['Release'],
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Windows-2008ServerR2-SP1'
          },
          {
            'gpu': '10de:104a',
            'os': 'Linux'
          }
        ],
      }
    ],
    'swarming': {
      # TODO(geofflang): Increase the number of shards as more tests start to
      # pass and runtime increases.
      'shards': 4,
    },
    'test': 'angle_deqp_gles31_tests',
    'args': [
      '--test-launcher-batch-limit=400',
      '--deqp-egl-display-type=angle-gl'
    ]
  },

  # Until we have more capacity, run angle_end2end_tests only on the
  # FYI waterfall, the ANGLE trybots (which mirror the FYI waterfall),
  # and the optional trybots (mainly used during ANGLE rolls).
  'angle_end2end_tests': {
    'tester_configs': [
      {
        'allow_on_android': True,
        'fyi_only': True,
        'run_on_optional': True,
      },
    ],
    'disabled_tester_configs': [
      {
        'names': [
          # TODO(ynovikov) Investigate why the test breaks on older devices.
          'Android Release (Nexus 5)',
          'Android Release (Nexus 6)',
          'Android Release (Nexus 9)',

          # These tests are flaky on old AMD.
          # TODO(jmadill): Enably flaky test retries only on this config.
          'Win7 Release (AMD R5 230)',
        ],
      },
    ],
    'desktop_args': [
      '--use-gpu-in-tests',
      # ANGLE test retries deliberately disabled to prevent flakiness.
      # http://crbug.com/669196
      '--test-launcher-retry-limit=0'
    ]
  },
  'angle_unittests': {
    'tester_configs': [
      {
        'allow_on_android': True,
      }
    ],
    'desktop_args': [
      '--use-gpu-in-tests',
      # ANGLE test retries deliberately disabled to prevent flakiness.
      # http://crbug.com/669196
      '--test-launcher-retry-limit=0'
    ]
  },
  # Until the media-only tests are extracted from content_unittests,
  # and audio_unittests and content_unittests can be run on the commit
  # queue with --require-audio-hardware-for-testing, run them only on
  # the FYI waterfall.
  #
  # Note that the transition to the Chromium recipe has forced the
  # removal of the --require-audio-hardware-for-testing flag for the
  # time being. See crbug.com/574942.
  'audio_unittests': {
    'tester_configs': [
      {
        'fyi_only': True,
      }
    ],
    'args': ['--use-gpu-in-tests']
  },
  # TODO(kbr): content_unittests is killing the Linux GPU swarming
  # bots. crbug.com/582094 . It's not useful now anyway until audio
  # hardware is deployed on the swarming bots, so stop running it
  # everywhere.
  # 'content_unittests': {},
  'gl_tests': {
    'tester_configs': [
      {
        'allow_on_android': True,
      }
    ],
    'disabled_tester_configs': [
      {
        'names': [
          # TODO(kbr): investigate inability to recognize this
          # configuration in the various tests. crbug.com/624621
          'Android Release (Pixel C)',
        ],
      },
    ],
    'desktop_args': ['--use-gpu-in-tests']
  },
  'gl_unittests': {
    'tester_configs': [
      {
        'allow_on_android': True,
      }
    ],
    'desktop_args': ['--use-gpu-in-tests']
  },
  # The gles2_conform_tests are closed-source and deliberately only run
  # on the FYI waterfall and the optional tryservers.
  'gles2_conform_test': {
    'tester_configs': [
      {
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
      }
    ],
    'args': ['--use-gpu-in-tests']
  },
  'gles2_conform_d3d9_test': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win'],
        # Run this on the optional tryservers.
        'run_on_optional': True,
      }
    ],
    'args': [
      '--use-gpu-in-tests',
      '--use-angle=d3d9',
    ],
    'test': 'gles2_conform_test',
  },
  'gles2_conform_gl_test': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win'],
        # Run this on the optional tryservers.
        'run_on_optional': True,
      }
    ],
    'args': [
      '--use-gpu-in-tests',
      '--use-angle=gl',
      '--disable-gpu-sandbox',
    ],
    'test': 'gles2_conform_test',
  },
  'swiftshader_unittests': {
    'tester_configs': [
      {
        'fyi_only': True,
        # Run this on the optional tryservers.
        'run_on_optional': True,
        'os_types': ['win', 'linux'],
      },
    ],
  },
  'tab_capture_end2end_tests': {
    'tester_configs': [
      {
        'build_configs': ['Release', 'Release_x64'],
      }
    ],
    'override_compile_targets': [
      'tab_capture_end2end_tests_run',
    ],
  },
  'video_decode_accelerator_unittest': {
    'tester_configs': [
      {
        'os_types': ['win']
      },
    ],
    'args': [
      '--use-test-data-path',
    ],
  },
}

# This requires a hack because the isolate's name is different than
# the executable's name. On the few non-swarmed testers, this causes
# the executable to not be found. It would be better if the Chromium
# recipe supported running isolates locally. crbug.com/581953

NON_SWARMED_GTESTS = {
  'tab_capture_end2end_tests': {
     'swarming': {
       'can_use_on_swarming_builders': False
     },
     'test': 'browser_tests',
     'args': [
       '--enable-gpu',
       '--test-launcher-jobs=1',
       '--gtest_filter=CastStreamingApiTestWithPixelOutput.EndToEnd*:' + \
           'TabCaptureApiPixelTest.EndToEnd*'
     ],
     'swarming': {
       'can_use_on_swarming_builders': False,
     },
  }
}

# These tests use Telemetry's new browser_test_runner, which is a much
# simpler harness for correctness testing.
TELEMETRY_GPU_INTEGRATION_TESTS = {
  'context_lost': {
    'tester_configs': [
      {
        'allow_on_android': True,
        'run_on_v8': True,
      },
    ]
  },
  'depth_capture': {
    'tester_configs': [
      {
        'allow_on_android': True,
        'run_on_v8': True,
      },
    ]
  },
  'gpu_process_launch_tests': {
    'target_name': 'gpu_process',
    'tester_configs': [
      {
        'allow_on_android': True,
        'run_on_v8': True,
      }
    ],
  },
  'hardware_accelerated_feature': {
    'tester_configs': [
      {
        'allow_on_android': True,
        'run_on_v8': True,
      },
    ],
  },
  'maps_pixel_test': {
    'target_name': 'maps',
    'args': [
      '--os-type',
      '${os_type}',
      '--build-revision',
      '${got_revision}',
      '--test-machine-name',
      '${buildername}',
    ],
    'tester_configs': [
      {
        'allow_on_android': True,
        'run_on_v8': True,
      },
    ],
  },
  'pixel_test': {
    'target_name': 'pixel',
    'args': [
      '--refimg-cloud-storage-bucket',
      'chromium-gpu-archive/reference-images',
      '--os-type',
      '${os_type}',
      '--build-revision',
      '${got_revision}',
      '--test-machine-name',
      '${buildername}',
    ],
    'non_precommit_args': [
      '--upload-refimg-to-cloud-storage',
    ],
    'precommit_args': [
      '--download-refimg-from-cloud-storage',
    ],
    'tester_configs': [
      {
        'allow_on_android': True,
        'run_on_v8': True,
      },
    ],
  },
  'screenshot_sync': {
    'tester_configs': [
      {
        'allow_on_android': True,
        'run_on_v8': True,
      },
    ],
  },
  'trace_test': {
    'tester_configs': [
      {
        'allow_on_android': True,
        'run_on_v8': True,
      },
    ],
  },
  'webgl_conformance': {
    'tester_configs': [
      {
        'allow_on_android': True,
        'run_on_v8': True,
      },
    ],
    'asan_args': ['--is-asan'],
  },
  'webgl_conformance_d3d9_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win'],
        'run_on_optional': True,
      }
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-angle=d3d9',
    ],
    'asan_args': ['--is-asan'],
  },
  'webgl_conformance_gl_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win'],
        'run_on_optional': True,
      }
    ],
    'disabled_tester_configs': [
      {
        'swarming_dimension_sets': [
          # crbug.com/555545 and crbug.com/649824:
          # Disable webgl_conformance_gl_tests on some Win/AMD cards.
          # Always fails on older cards, flaky on newer cards.
          # Note that these must match the GPUs exactly; wildcard
          # matches (i.e., only device ID) aren't supported!
          {
            'gpu': '1002:6779',
            'os': 'Windows-2008ServerR2-SP1'
          },
          {
            'gpu': '1002:6613',
            'os': 'Windows-2008ServerR2-SP1'
          },
          # BUG 590951: Disable webgl_conformance_gl_tests on Win/Intel
          {
            'gpu': '8086:041a',
            'os': 'Windows-2008ServerR2-SP1'
          },
          {
            'gpu': '8086:0412',
            'os': 'Windows-2008ServerR2-SP1'
          },
        ],
      },
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-angle=gl',
    ],
    'asan_args': ['--is-asan'],
  },
  'webgl_conformance_angle_tests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['linux'],
        'run_on_optional': True,
      }
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-gl=angle',
    ],
    'asan_args': ['--is-asan'],
  },
  'webgl_conformance_d3d11_passthrough': {
    'tester_configs': [
      {
        'fyi_only': True,
        'os_types': ['win'],
        'run_on_optional': True,
      }
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-angle=d3d11',
      '--use-passthrough-cmd-decoder',
      # TODO(geofflang): Remove --disable-es3-apis once crbug.com/671217 is
      # complete.
      '--disable-es3-apis',
      # TODO(geofflang): --disable-es3-gl-context is required because of
      # crbug.com/680522
      '--disable-es3-gl-context',
    ],
    'asan_args': ['--is-asan'],
  },
  'webgl2_conformance_tests': {
    'tester_configs': [
      {
         # The WebGL 2.0 conformance tests take over an hour to run on
         # the Debug bots, which is too long.
        'build_configs': ['Release', 'Release_x64'],
        'fyi_only': True,
        'run_on_optional': True,
        'run_on_v8': True,
      },
    ],
    'disabled_tester_configs': [
      {
        'names': [
          # http://crbug.com/599451: this test is currently too slow
          # to run on x64 in Debug mode. Need to shard the tests.
          'Win7 x64 Debug (NVIDIA)',
        ],
      },
    ],
    'target_name': 'webgl_conformance',
    'args': [
      '--webgl-conformance-version=2.0.1',
      # The current working directory when run via isolate is
      # out/Debug or out/Release. Reference this file relatively to
      # it.
      '--read-abbreviated-json-results-from=' + \
      '../../content/test/data/gpu/webgl2_conformance_tests_output.json',
    ],
    'asan_args': ['--is-asan'],
    'swarming': {
      # These tests currently take about an hour and fifteen minutes
      # to run. Split them into roughly 5-minute shards.
      'shards': 15,
    },
  },
  'webgl2_conformance_angle_tests': {
    'tester_configs': [
      {
         # The WebGL 2.0 conformance tests take over an hour to run on
         # the Debug bots, which is too long.
        'build_configs': ['Release'],
        'fyi_only': True,
        'run_on_optional': False,
        # Only run on the NVIDIA Release and Intel Release Linux bots
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Linux'
          },
          {
            'gpu': '8086:0412',
            'os': 'Linux'
          },
          {
            'gpu': '8086:1912',
            'os': 'Linux'
          },
        ],
      },
    ],
    'target_name': 'webgl_conformance',
    'extra_browser_args': [
      '--use-gl=angle',
    ],
    'args': [
      '--webgl-conformance-version=2.0.1',
      # The current working directory when run via isolate is
      # out/Debug or out/Release. Reference this file relatively to
      # it.
      '--read-abbreviated-json-results-from=' + \
      '../../content/test/data/gpu/webgl2_conformance_tests_output.json',
    ],
    'asan_args': ['--is-asan'],
    'swarming': {
      # These tests currently take about an hour and fifteen minutes
      # to run. Split them into roughly 5-minute shards.
      'shards': 15,
    },
  },
}

# These isolated tests don't use telemetry. They need to be placed in the
# isolated_scripts section of the generated json.
NON_TELEMETRY_ISOLATED_SCRIPT_TESTS = {
  # We run angle_perftests on the ANGLE CQ to ensure the tests don't crash.
  'angle_perftests': {
    'tester_configs': [
      {
        'fyi_only': True,
        'run_on_optional': True,
        # Run on the Win/Linux Release NVIDIA bots.
        'build_configs': ['Release'],
        'swarming_dimension_sets': [
          {
            'gpu': '10de:104a',
            'os': 'Windows-2008ServerR2-SP1'
          },
          {
            'gpu': '10de:104a',
            'os': 'Linux'
          }
        ],
      },
    ],
  },
}

def substitute_args(tester_config, args):
  """Substitutes the ${os_type} variable in |args| from the
     tester_config's "os_type" property.
  """
  substitutions = {
    'os_type': tester_config['os_type']
  }
  return [string.Template(arg).safe_substitute(substitutions) for arg in args]

def matches_swarming_dimensions(tester_config, dimension_sets):
  for dimensions in dimension_sets:
    for cur_dims in tester_config['swarming_dimensions']:
      if set(dimensions.items()).issubset(cur_dims.items()):
        return True
  return False

def is_android(tester_config):
  return tester_config['os_type'] == 'android'

def is_asan(tester_config):
  return tester_config.get('is_asan', False)

def tester_config_matches_tester(tester_name, tester_config, tc, is_fyi, is_v8,
                                 check_waterfall):
  if check_waterfall:
    if tc.get('fyi_only', False) and not is_fyi:
      return False

    # Handle the optional tryservers with the 'run_on_optional' flag.
    # Only a subset of the tests run on these tryservers.
    if tester_name.startswith('Optional') and not tc.get(
        'run_on_optional', False):
      return False

    # Handle the client.v8.fyi GPU bots with the 'run_on_v8' flag.
    if (is_v8 and not tc.get('run_on_v8', False)):
      return False

  if 'names' in tc:
    # Give priority to matching the tester_name.
    if tester_name in tc['names']:
      return True
    if not tester_name in tc['names']:
      return False
  if 'os_types' in tc:
    if not tester_config['os_type'] in tc['os_types']:
      return False
  if 'build_configs' in tc:
    if not tester_config['build_config'] in tc['build_configs']:
      return False
  if 'swarming_dimension_sets' in tc:
    if not matches_swarming_dimensions(tester_config,
                                       tc['swarming_dimension_sets']):
      return False
  if is_android(tester_config):
    if not tc.get('allow_on_android', False):
      return False
  return True

def should_run_on_tester(tester_name, tester_config, test_config,
                         is_fyi, is_v8):
  # Check if this config is disabled on this tester
  if 'disabled_tester_configs' in test_config:
    for dtc in test_config['disabled_tester_configs']:
      if tester_config_matches_tester(tester_name, tester_config, dtc,
                                      is_fyi, is_v8, False):
        return False
  if 'tester_configs' in test_config:
    for tc in test_config['tester_configs']:
      if tester_config_matches_tester(tester_name, tester_config, tc,
                                      is_fyi, is_v8, True):
        return True
    return False
  else:
    # If tester_configs is unspecified, run nearly all tests by default,
    # but let tester_config_matches_tester filter out any undesired
    # tests, such as ones that should only run on the Optional bots.
    return tester_config_matches_tester(tester_name, tester_config, {},
                                        is_fyi, is_v8, True)

def generate_gtest(tester_name, tester_config, test, test_config,
                   is_fyi, is_v8):
  if not should_run_on_tester(tester_name, tester_config, test_config,
                              is_fyi, is_v8):
    return None
  result = copy.deepcopy(test_config)
  if 'tester_configs' in result:
    # Don't print the tester_configs in the JSON.
    result.pop('tester_configs')
  if 'disabled_tester_configs' in result:
    # Don't print the disabled_tester_configs in the JSON.
    result.pop('disabled_tester_configs')
  if 'test' in result:
    result['name'] = test
  else:
    result['test'] = test
  if (not tester_config['swarming']) and test in NON_SWARMED_GTESTS:
    # Need to override this result.
    result = copy.deepcopy(NON_SWARMED_GTESTS[test])
    result['name'] = test
  else:
    # Put the swarming dimensions in anyway. If the tester is later
    # swarmed, they will come in handy.
    if not 'swarming' in result:
      result['swarming'] = {}
    result['swarming'].update({
      'can_use_on_swarming_builders': True,
      'dimension_sets': tester_config['swarming_dimensions']
    })
    if is_android(tester_config):
      # Override the isolate target to get rid of any "_apk" suffix
      # that would be added by the recipes.
      if 'test' in result:
        result['override_isolate_target'] = result['test']
      else:
        result['override_isolate_target'] = result['name']
      # Integrate with the unified logcat system.
      result['swarming'].update({
        'cipd_packages': [
          {
            'cipd_package': 'infra/tools/luci/logdog/butler/${platform}',
            'location': 'bin',
            'revision': 'git_revision:25755a2c316937ee44a6432163dc5e2f9c85cf58'
          }
        ],
        'output_links': [
          {
            'link': [
              'https://luci-logdog.appspot.com/v/?s',
              '=android%2Fswarming%2Flogcats%2F',
              '${TASK_ID}%2F%2B%2Funified_logcats'
            ],
            'name': 'shard #${SHARD_INDEX} logcats'
          }
        ]
      })
  if 'desktop_args' in result:
    if not is_android(tester_config):
      if not 'args' in result:
        result['args'] = []
      result['args'] += result['desktop_args']
    # Don't put the desktop args in the JSON.
    result.pop('desktop_args')
  if 'android_args' in result:
    if is_android(tester_config):
      if not 'args' in result:
        result['args'] = []
      result['args'] += result['android_args']
    # Don't put the android args in the JSON.
    result.pop('android_args')
  if 'desktop_swarming' in result:
    if not is_android(tester_config):
      result['swarming'].update(result['desktop_swarming'])
    # Don't put the desktop_swarming in the JSON.
    result.pop('desktop_swarming')

  # This flag only has an effect on the Linux bots that run tests
  # locally (as opposed to via Swarming), which are only those couple
  # on the chromium.gpu.fyi waterfall. Still, there is no harm in
  # specifying it everywhere.
  result['use_xvfb'] = False
  return result

def generate_gtests(tester_name, tester_config, test_dictionary, is_fyi, is_v8):
  # The relative ordering of some of the tests is important to
  # minimize differences compared to the handwritten JSON files, since
  # Python's sorts are stable and there are some tests with the same
  # key (see gles2_conform_d3d9_test and similar variants). Avoid
  # losing the order by avoiding coalescing the dictionaries into one.
  gtests = []
  for test_name, test_config in sorted(test_dictionary.iteritems()):
    test = generate_gtest(tester_name, tester_config,
                          test_name, test_config, is_fyi, is_v8)
    if test:
      # generate_gtest may veto the test generation on this platform.
      gtests.append(test)
  return gtests

def generate_isolated_test(tester_name, tester_config, test, test_config,
                           is_fyi, is_v8, extra_browser_args, isolate_name,
                           override_compile_targets, prefix_args):
  if not should_run_on_tester(tester_name, tester_config, test_config,
                              is_fyi, is_v8):
    return None
  test_args = ['-v']
  extra_browser_args_string = ""
  if extra_browser_args != None:
    extra_browser_args_string += ' '.join(extra_browser_args)
  if 'extra_browser_args' in test_config:
    extra_browser_args_string += ' ' + ' '.join(
        test_config['extra_browser_args'])
  if extra_browser_args_string != "":
    test_args.append('--extra-browser-args=' + extra_browser_args_string)
  if 'args' in test_config:
    test_args.extend(substitute_args(tester_config, test_config['args']))
  if 'desktop_args' in test_config and not is_android(tester_config):
    test_args.extend(substitute_args(tester_config,
                                     test_config['desktop_args']))
  if 'android_args' in test_config and is_android(tester_config):
    test_args.extend(substitute_args(tester_config,
                                     test_config['android_args']))
  if 'asan_args' in test_config and is_asan(tester_config):
    test_args.extend(substitute_args(tester_config,
                                     test_config['asan_args']))
  # The step name must end in 'test' or 'tests' in order for the
  # results to automatically show up on the flakiness dashboard.
  # (At least, this was true some time ago.) Continue to use this
  # naming convention for the time being to minimize changes.
  step_name = test
  if not (step_name.endswith('test') or step_name.endswith('tests')):
    step_name = '%s_tests' % step_name
  # Prepend GPU-specific flags.
  swarming = {
    # Always say this is true regardless of whether the tester
    # supports swarming. It doesn't hurt.
    'can_use_on_swarming_builders': True,
    'dimension_sets': tester_config['swarming_dimensions']
  }
  if 'swarming' in test_config:
    swarming.update(test_config['swarming'])
  result = {
    'args': prefix_args + test_args,
    'isolate_name': isolate_name,
    'name': step_name,
    'swarming': swarming,
  }
  if override_compile_targets != None:
    result['override_compile_targets'] = override_compile_targets
  if 'non_precommit_args' in test_config:
    result['non_precommit_args'] = test_config['non_precommit_args']
  if 'precommit_args' in test_config:
    result['precommit_args'] = test_config['precommit_args']
  return result

def generate_telemetry_test(tester_name, tester_config,
                            test, test_config, is_fyi, is_v8):
  extra_browser_args = ['--enable-logging=stderr', '--js-flags=--expose-gc']
  benchmark_name = test_config.get('target_name') or test
  prefix_args = [
    benchmark_name,
    '--show-stdout',
    '--browser=%s' % tester_config['build_config'].lower()
  ]
  return generate_isolated_test(tester_name, tester_config, test,
                                test_config, is_fyi, is_v8, extra_browser_args,
                                'telemetry_gpu_integration_test',
                                ['telemetry_gpu_integration_test_run'],
                                prefix_args)

def generate_telemetry_tests(tester_name, tester_config,
                             test_dictionary, is_fyi, is_v8):
  isolated_scripts = []
  for test_name, test_config in sorted(test_dictionary.iteritems()):
    test = generate_telemetry_test(
      tester_name, tester_config, test_name, test_config, is_fyi, is_v8)
    if test:
      isolated_scripts.append(test)
  return isolated_scripts

def generate_non_telemetry_isolated_test(tester_name, tester_config,
                                         test, test_config, is_fyi, is_v8):
  return generate_isolated_test(tester_name, tester_config, test,
                                test_config, is_fyi, is_v8,
                                None, test, None, [])

def generate_non_telemetry_isolated_tests(tester_name, tester_config,
                                          test_dictionary, is_fyi, is_v8):
  isolated_scripts = []
  for test_name, test_config in sorted(test_dictionary.iteritems()):
    test = generate_non_telemetry_isolated_test(
      tester_name, tester_config, test_name, test_config, is_fyi, is_v8)
    if test:
      isolated_scripts.append(test)
  return isolated_scripts

def generate_all_tests(waterfall, filename, is_fyi, is_v8):
  tests = {}
  for builder, config in waterfall.get('prologue', {}).iteritems():
    tests[builder] = config
  for builder, config in waterfall.get('builders', {}).iteritems():
    tests[builder] = config
  for name, config in waterfall['testers'].iteritems():
    gtests = generate_gtests(name, config, COMMON_GTESTS, is_fyi, is_v8)
    isolated_scripts = \
      generate_telemetry_tests(
        name, config, TELEMETRY_GPU_INTEGRATION_TESTS, is_fyi, is_v8) + \
      generate_non_telemetry_isolated_tests(name, config,
        NON_TELEMETRY_ISOLATED_SCRIPT_TESTS, is_fyi, is_v8)
    tests[name] = {
      'gtest_tests': sorted(gtests, key=lambda x: x['test']),
      'isolated_scripts': sorted(isolated_scripts, key=lambda x: x['name'])
    }
  tests['AAAAA1 AUTOGENERATED FILE DO NOT EDIT'] = {}
  tests['AAAAA2 See generate_buildbot_json.py to make changes'] = {}
  with open(os.path.join(SRC_DIR, 'testing', 'buildbot', filename), 'wb') as fp:
    json.dump(tests, fp, indent=2, separators=(',', ': '), sort_keys=True)
    fp.write('\n')

def main():
  generate_all_tests(FYI_WATERFALL, 'chromium.gpu.fyi.json', True, False)
  generate_all_tests(WATERFALL, 'chromium.gpu.json', False, False)
  generate_all_tests(V8_FYI_WATERFALL, 'client.v8.fyi.json', True, True)
  return 0

if __name__ == "__main__":
  sys.exit(main())

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import sys

# Add the third_party/ dir to our search path so that we can find the
# modules in there automatically.  This isn't normal, so don't replicate
# this pattern elsewhere.
_chromite_dir = os.path.normpath(os.path.dirname(os.path.realpath(__file__)))
_venv_dir = os.path.join(_chromite_dir, 'venv', '.venv')
_containing_dir = os.path.dirname(_chromite_dir)
_third_party_dirs = [os.path.join(_chromite_dir, 'third_party')]
# If chromite is living inside the Chrome checkout under
# <chrome_root>/src/third_party/chromite, its dependencies will be checked out
# to <chrome_root>/src/third_party instead of the normal chromite/third_party
# location due to git-submodule limitations (a submodule cannot be contained
# inside another submodule's workspace), so we want to add that to the
# search path.
if os.path.basename(_containing_dir) == 'third_party':
  _third_party_dirs.append(_containing_dir)

# List of third_party packages that might need subpaths added to search.
_paths = [
    'dpkt',
    os.path.join('gdata', 'src'),
    'google',
    'pyelftools',
]

for _path in _paths:
  for _third_party in _third_party_dirs[:]:
    _component = os.path.join(_third_party, _path)
    if os.path.isdir(_component):
      _third_party_dirs.append(_component)

# Some chromite scripts make use of the chromite virtualenv located at
# $CHROMITE_DIR/.venv. If we are in such a virtualenv, we want it to take
# precedence over third_party.
# Therefore, we leave sys.path unaltered up to the final element from the
# virtualenv, and only insert third_party items after that.
for i, path in reversed(list(enumerate(sys.path))):
  if _venv_dir in path:
    _insert_at = i + 1
    break
else:
  _insert_at = 0
sys.path[_insert_at:_insert_at] = _third_party_dirs

# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

def _AddToPathIfNeeded(path):
  if path not in sys.path:
    sys.path.insert(0, path)

def _UpdateSysPathIfNeeded():
  catapult_path = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
  catapult_third_party_path = os.path.abspath(os.path.join(
      catapult_path, 'third_party'))
  _AddToPathIfNeeded(os.path.join(catapult_third_party_path, 'six'))
  _AddToPathIfNeeded(os.path.join(catapult_third_party_path, 'Paste'))
  _AddToPathIfNeeded(os.path.join(catapult_third_party_path, 'webapp2'))
  _AddToPathIfNeeded(os.path.join(catapult_third_party_path, 'WebOb'))
  _AddToPathIfNeeded(os.path.join(catapult_path, 'tracing'))
  _AddToPathIfNeeded(os.path.join(catapult_path, 'perf_insights'))

_UpdateSysPathIfNeeded()

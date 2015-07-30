# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys


def CheckChange(input_api, output_api):
  original_sys_path = sys.path
  try:
    sys.path += [input_api.PresubmitLocalPath()]
    from hooks import pre_commit
    results = pre_commit.GetResults('@{u}')
    return map(output_api.PresubmitError, results)
  finally:
    sys.path = original_sys_path


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)

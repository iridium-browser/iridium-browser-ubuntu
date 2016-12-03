# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys
import unittest

from catapult_base import util


class PathTest(unittest.TestCase):

  def testIsExecutable(self):
    self.assertFalse(util.IsExecutable('nonexistent_file'))
    # We use actual files on disk instead of pyfakefs because the executable is
    # set different on win that posix platforms and pyfakefs doesn't support
    # win platform well.
    self.assertFalse(util.IsExecutable(_GetFileInTestDir('foo.txt')))
    self.assertTrue(util.IsExecutable(sys.executable))


def _GetFileInTestDir(file_name):
  return os.path.join(os.path.dirname(__file__), 'test_data', file_name)

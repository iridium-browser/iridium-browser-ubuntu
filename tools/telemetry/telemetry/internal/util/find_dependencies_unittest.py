# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from telemetry.core import util
from telemetry.internal.util import find_dependencies


class FindDependenciesTest(unittest.TestCase):
  def testFindPythonDependencies(self):
    dog_object_path = os.path.join(
        util.GetUnittestDataDir(),
        'dependency_test_dir', 'dog', 'dog', 'dog_object.py')
    cat_module_path = os.path.join(
        util.GetUnittestDataDir(),
        'dependency_test_dir', 'other_animals', 'cat', 'cat')
    cat_module_init_path = os.path.join(cat_module_path, '__init__.py')
    cat_object_path = os.path.join(cat_module_path, 'cat_object.py')
    self.assertEquals(
      set(p for p in find_dependencies.FindPythonDependencies(dog_object_path)),
      {dog_object_path, cat_module_path, cat_module_init_path, cat_object_path})

# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from tvcm import generate
from tvcm import fake_fs
from tvcm import project as project_module


class GenerateTests(unittest.TestCase):

  def setUp(self):
    self.fs = fake_fs.FakeFS()
    self.fs.AddFile('/x/foo/my_module.html', """
<!DOCTYPE html>
<link rel="import" href="/foo/other_module.html">
""")
    self.fs.AddFile('/x/foo/other_module.html', """
<!DOCTYPE html>
<script src="/foo/raw/raw_script.js"></script>
<script>
    'use strict';
    HelloWorld();
</script>
""")
    self.fs.AddFile('/x/foo/raw/raw_script.js', """
/* raw script */
""")
    self.fs.AddFile('/x/components/polymer/polymer.min.js', """
""")

    self.project = project_module.Project(
        [os.path.normpath('/x')])

  def testJSGeneration(self):
    with self.fs:
      load_sequence = self.project.CalcLoadSequenceForModuleNames(
          [os.path.normpath('foo.my_module')])
      generate.GenerateJS(load_sequence)

  def testHTMLGeneration(self):
    with self.fs:
      load_sequence = self.project.CalcLoadSequenceForModuleNames(
          [os.path.normpath('foo.my_module')])
      res = generate.GenerateStandaloneHTMLAsString(load_sequence, 'Title')
      assert 'HelloWorld();' in res

  def testExtraScriptWithWriteContentsFunc(self):
    with self.fs:
      load_sequence = self.project.CalcLoadSequenceForModuleNames(
          [os.path.normpath('foo.my_module')])

      class ExtraScript(generate.ExtraScript):
        def WriteToFile(self, f):
          f.write('<script>ExtraScript!</script>')

      res = generate.GenerateStandaloneHTMLAsString(
          load_sequence, 'Title', extra_scripts=[ExtraScript()])
      assert 'ExtraScript' in res

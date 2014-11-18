#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Unit tests for the 'grit build' tool.
'''

import os
import sys
import tempfile
if __name__ == '__main__':
  sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))

import unittest

from grit import util
from grit.tool import build


class BuildUnittest(unittest.TestCase):

  def testFindTranslationsWithSubstitutions(self):
    # This is a regression test; we had a bug where GRIT would fail to find
    # messages with substitutions e.g. "Hello [IDS_USER]" where IDS_USER is
    # another <message>.
    output_dir = tempfile.mkdtemp()
    builder = build.RcBuilder()
    class DummyOpts(object):
      def __init__(self):
        self.input = util.PathFromRoot('grit/testdata/substitute.grd')
        self.verbose = False
        self.extra_verbose = False
    builder.Run(DummyOpts(), ['-o', output_dir])

  def testGenerateDepFile(self):
    output_dir = tempfile.mkdtemp()
    builder = build.RcBuilder()
    class DummyOpts(object):
      def __init__(self):
        self.input = util.PathFromRoot('grit/testdata/substitute.grd')
        self.verbose = False
        self.extra_verbose = False
    expected_dep_file = os.path.join(output_dir, 'substitute.grd.d')
    builder.Run(DummyOpts(), ['-o', output_dir,
                              '--depdir', output_dir,
                              '--depfile', expected_dep_file])

    self.failUnless(os.path.isfile(expected_dep_file))
    with open(expected_dep_file) as f:
      line = f.readline()
      (dep_output_file, deps_string) = line.split(': ')
      deps = deps_string.split(' ')

      self.failUnlessEqual("resource.h", dep_output_file)
      self.failUnlessEqual(1, len(deps))
      self.failUnlessEqual(deps[0],
          util.PathFromRoot('grit/testdata/substitute.xmb'))

  def testAssertOutputs(self):
    output_dir = tempfile.mkdtemp()
    class DummyOpts(object):
      def __init__(self):
        self.input = util.PathFromRoot('grit/testdata/substitute.grd')
        self.verbose = False
        self.extra_verbose = False

    # Incomplete output file list should fail.
    builder_fail = build.RcBuilder()
    self.failUnlessEqual(2,
        builder_fail.Run(DummyOpts(), [
            '-o', output_dir,
            '-a', os.path.abspath(
                os.path.join(output_dir, 'en_generated_resources.rc'))]))

    # Complete output file list should succeed.
    builder_ok = build.RcBuilder()
    self.failUnlessEqual(0,
        builder_ok.Run(DummyOpts(), [
            '-o', output_dir,
            '-a', os.path.abspath(
                os.path.join(output_dir, 'en_generated_resources.rc')),
            '-a', os.path.abspath(
                os.path.join(output_dir, 'sv_generated_resources.rc')),
            '-a', os.path.abspath(
                os.path.join(output_dir, 'resource.h'))]))

if __name__ == '__main__':
  unittest.main()

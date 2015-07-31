# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for mb.py."""

import json
import sys
import unittest

import mb


class FakeMBW(mb.MetaBuildWrapper):
  def __init__(self):
    super(FakeMBW, self).__init__()
    self.files = {}
    self.calls = []
    self.out = ''
    self.err = ''
    self.platform = 'linux2'
    self.chromium_src_dir = '/fake_src'
    self.default_config = '/fake_src/tools/mb/mb_config.pyl'

  def ExpandUser(self, path):
    return '$HOME/%s' % path

  def Exists(self, path):
    return self.files.get(path) is not None

  def ReadFile(self, path):
    return self.files[path]

  def WriteFile(self, path, contents):
    self.files[path] = contents

  def Call(self, cmd):
    self.calls.append(cmd)
    return 0, '', ''

  def Print(self, *args, **kwargs):
    sep = kwargs.get('sep', ' ')
    end = kwargs.get('end', '\n')
    f = kwargs.get('file', sys.stdout)
    if f == sys.stderr:
      self.err += sep.join(args) + end
    else:
      self.out += sep.join(args) + end

  def TempFile(self):
    return FakeFile(self.files)

  def RemoveFile(self, path):
    del self.files[path]


class FakeFile(object):
  def __init__(self, files):
    self.name = '/tmp/file'
    self.buf = ''
    self.files = files

  def write(self, contents):
    self.buf += contents

  def close(self):
     self.files[self.name] = self.buf


class IntegrationTest(unittest.TestCase):
  def test_validate(self):
    # Note that this validates that the actual mb_config.pyl is valid.
    ret = mb.main(['validate', '--quiet'])
    self.assertEqual(ret, 0)


TEST_CONFIG = """\
{
  'common_dev_configs': ['gn_debug'],
  'configs': {
    'gyp_rel_bot': ['gyp', 'rel', 'goma'],
    'gn_debug': ['gn', 'debug'],
    'gn_rel_bot': ['gn', 'rel', 'goma'],
    'private': ['gyp', 'fake_feature1'],
    'unsupported': ['gn', 'fake_feature2'],
  },
  'masters': {
    'fake_master': {
      'fake_builder': 'gyp_rel_bot',
      'fake_gn_builder': 'gn_rel_bot',
    },
  },
  'mixins': {
    'fake_feature1': {
      'gn_args': 'enable_doom_melon=true',
      'gyp_defines': 'doom_melon=1',
    },
    'fake_feature2': {
      'gn_args': 'enable_doom_melon=false',
      'gyp_defaults': 'doom_melon=0',
    },
    'gyp': {'type': 'gyp'},
    'gn': {'type': 'gn'},
    'goma': {
      'gn_args': 'use_goma=true goma_dir="$(goma_dir)"',
      'gyp_defines': 'goma=1 gomadir="$(goma_dir)"',
    },
    'rel': {
      'gn_args': 'is_debug=false',
      'gyp_config': 'Release',
    },
    'debug': {
      'gn_args': 'is_debug=true',
    },
  },
  'private_configs': ['private'],
  'unsupported_configs': ['unsupported'],
}
"""


class UnitTest(unittest.TestCase):
  def fake_mbw(self, files=None):
    mbw = FakeMBW()
    mbw.files.setdefault(mbw.default_config, TEST_CONFIG)
    if files:
      for path, contents in files.items():
        mbw.files[path] = contents
    return mbw

  def check(self, args, mbw=None, files=None, out=None, err=None, ret=None):
    if not mbw:
      mbw = self.fake_mbw(files)
    mbw.ParseArgs(args)
    actual_ret = mbw.args.func()
    if ret is not None:
      self.assertEqual(actual_ret, ret)
    if out is not None:
      self.assertEqual(mbw.out, out)
    if err is not None:
      self.assertEqual(mbw.err, err)
    return mbw

  def test_gn_analyze(self):
    files = {'/tmp/in.json': """{\
               "files": ["foo/foo_unittest.cc"],
               "targets": ["foo_unittests", "bar_unittests"]
             }"""}

    mbw = self.fake_mbw(files)
    mbw.Call = lambda cmd: (0, 'out/Default/foo_unittests\n', '')

    self.check(['analyze', '-c', 'gn_debug', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])
    self.assertEqual(out, {
      'status': 'Found dependency',
      'targets': ['foo_unittests'],
      'build_targets': ['foo_unittests']
    })

  def test_gn_analyze_all(self):
    files = {'/tmp/in.json': """{\
               "files": ["foo/foo_unittest.cc"],
               "targets": ["all", "bar_unittests"]
             }"""}
    mbw = self.fake_mbw(files)
    mbw.Call = lambda cmd: (0, 'out/Default/foo_unittests\n', '')

    self.check(['analyze', '-c', 'gn_debug', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])
    self.assertEqual(out, {
      'status': 'Found dependency (all)',
    })

  def test_gn_analyze_missing_file(self):
    files = {'/tmp/in.json': """{\
               "files": ["foo/foo_unittest.cc"],
               "targets": ["bar_unittests"]
             }"""}
    mbw = self.fake_mbw(files)
    mbw.Call = lambda cmd: (
        1, 'The input matches no targets, configs, or files\n', '')

    self.check(['analyze', '-c', 'gn_debug', '//out/Default',
                '/tmp/in.json', '/tmp/out.json'], mbw=mbw, ret=0)
    out = json.loads(mbw.files['/tmp/out.json'])
    self.assertEqual(out, {
      'build_targets': [],
      'targets': [],
      'status': 'No dependency',
    })

  def test_gyp_analyze(self):
    self.check(['analyze', '-c', 'gyp_rel_bot', '//out/Release',
                '/tmp/in.json', '/tmp/out.json'],
               ret=0)

  def test_gen(self):
    self.check(['gen', '-c', 'gn_debug', '//out/Default'], ret=0)
    self.check(['gen', '-c', 'gyp_rel_bot', '//out/Release'], ret=0)

  def test_gen_fails(self):
    mbw = self.fake_mbw()
    mbw.Call = lambda cmd: (1, '', '')
    self.check(['gen', '-c', 'gn_debug', '//out/Default'], mbw=mbw, ret=1)
    self.check(['gen', '-c', 'gyp_rel_bot', '//out/Release'], mbw=mbw, ret=1)

  def test_goma_dir_expansion(self):
    self.check(['lookup', '-c', 'gyp_rel_bot', '-g', '/foo'], ret=0,
               out=("python build/gyp_chromium -G 'output_dir=<path>' "
                    "-G config=Release -D goma=1 -D gomadir=/foo\n"))
    self.check(['lookup', '-c', 'gn_rel_bot', '-g', '/foo'], ret=0,
               out=("/fake_src/buildtools/linux64/gn gen '<path>' "
                    "'--args=is_debug=false use_goma=true "
                    "goma_dir=\"/foo\"'\n" ))

  def test_help(self):
    self.assertRaises(SystemExit, self.check, ['-h'])
    self.assertRaises(SystemExit, self.check, ['help'])
    self.assertRaises(SystemExit, self.check, ['help', 'gen'])

  def test_lookup(self):
    self.check(['lookup', '-c', 'gn_debug'], ret=0)

  def test_validate(self):
    self.check(['validate'], ret=0)


if __name__ == '__main__':
  unittest.main()

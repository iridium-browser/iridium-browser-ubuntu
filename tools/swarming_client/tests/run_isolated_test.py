#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

# pylint: disable=R0201

import StringIO
import functools
import json
import logging
import os
import shutil
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

import isolated_format
import isolateserver
import run_isolated
from depot_tools import auto_stub
from utils import file_path
from utils import on_error
from utils import subprocess42
from utils import tools

import isolateserver_mock


def write_content(filepath, content):
  with open(filepath, 'wb') as f:
    f.write(content)


def json_dumps(data):
  return json.dumps(data, sort_keys=True, separators=(',', ':'))


class StorageFake(object):
  def __init__(self, files):
    self._files = files.copy()

  def __enter__(self, *_):
    return self

  def __exit__(self, *_):
    pass

  @property
  def hash_algo(self):
    return isolateserver_mock.ALGO

  def async_fetch(self, channel, _priority, digest, _size, sink):
    sink([self._files[digest]])
    channel.send_result(digest)


class RunIsolatedTestBase(auto_stub.TestCase):
  def setUp(self):
    super(RunIsolatedTestBase, self).setUp()
    self.tempdir = tempfile.mkdtemp(prefix=u'run_isolated_test')
    logging.debug(self.tempdir)
    self.mock(run_isolated, 'make_temp_dir', self.fake_make_temp_dir)
    self.mock(run_isolated.auth, 'ensure_logged_in', lambda _: None)

  def tearDown(self):
    for dirpath, dirnames, filenames in os.walk(self.tempdir, topdown=True):
      for filename in filenames:
        file_path.set_read_only(os.path.join(dirpath, filename), False)
      for dirname in dirnames:
        file_path.set_read_only(os.path.join(dirpath, dirname), False)
    shutil.rmtree(self.tempdir)
    super(RunIsolatedTestBase, self).tearDown()

  @property
  def run_test_temp_dir(self):
    """Where to map all files in run_isolated.run_tha_test."""
    return os.path.join(self.tempdir, 'run_tha_test')

  def fake_make_temp_dir(self, prefix, _root_dir=None):
    """Predictably returns directory for run_tha_test (one per test case)."""
    self.assertIn(prefix, ('run_tha_test', 'isolated_out'))
    temp_dir = os.path.join(self.tempdir, prefix)
    self.assertFalse(os.path.isdir(temp_dir))
    os.makedirs(temp_dir)
    return temp_dir

  def temp_join(self, *args):
    """Shortcut for joining path with self.run_test_temp_dir."""
    return os.path.join(self.run_test_temp_dir, *args)


class RunIsolatedTest(RunIsolatedTestBase):
  def setUp(self):
    super(RunIsolatedTest, self).setUp()
    self.popen_calls = []
    # pylint: disable=no-self-argument
    class Popen(object):
      def __init__(self2, args, **kwargs):
        kwargs.pop('cwd', None)
        kwargs.pop('env', None)
        self.popen_calls.append((args, kwargs))
        self2.returncode = None

      def communicate(self):
        self.returncode = 0
        return ('', None)

      def kill(self):
        pass

    self.mock(subprocess42, 'Popen', Popen)

  def test_main(self):
    self.mock(tools, 'disable_buffering', lambda: None)
    isolated = json_dumps(
        {
          'command': ['foo.exe', 'cmd with space'],
        })
    isolated_hash = isolateserver_mock.hash_content(isolated)
    def get_storage(_isolate_server, _namespace):
      return StorageFake({isolated_hash:isolated})
    self.mock(isolateserver, 'get_storage', get_storage)

    cmd = [
        '--no-log',
        '--isolated', isolated_hash,
        '--cache', self.tempdir,
        '--isolate-server', 'https://localhost',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)
    self.assertEqual(
        [([self.temp_join(u'foo.exe'), u'cmd with space'], {'detached': True})],
        self.popen_calls)

  def test_main_args(self):
    self.mock(tools, 'disable_buffering', lambda: None)
    isolated = json_dumps({'command': ['foo.exe', 'cmd w/ space']})
    isolated_hash = isolateserver_mock.hash_content(isolated)
    def get_storage(_isolate_server, _namespace):
      return StorageFake({isolated_hash:isolated})
    self.mock(isolateserver, 'get_storage', get_storage)

    cmd = [
        '--no-log',
        '--isolated', isolated_hash,
        '--cache', self.tempdir,
        '--isolate-server', 'https://localhost',
        '--',
        '--extraargs',
        'bar',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(0, ret)
    self.assertEqual(
        [
          ([self.temp_join(u'foo.exe'), u'cmd w/ space', '--extraargs', 'bar'],
            {'detached': True}),
          ],
        self.popen_calls)

  def _run_tha_test(self, isolated_hash, files):
    make_tree_call = []
    def add(i, _):
      make_tree_call.append(i)
    for i in ('make_tree_read_only', 'make_tree_files_read_only',
              'make_tree_deleteable', 'make_tree_writeable'):
      self.mock(file_path, i, functools.partial(add, i))

    ret = run_isolated.run_tha_test(
        isolated_hash,
        StorageFake(files),
        isolateserver.MemoryCache(),
        False,
        [])
    self.assertEqual(0, ret)
    return make_tree_call

  def test_run_tha_test_naked(self):
    isolated = json_dumps({'command': ['invalid', 'command']})
    isolated_hash = isolateserver_mock.hash_content(isolated)
    files = {isolated_hash:isolated}
    make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual(
        ['make_tree_writeable', 'make_tree_deleteable', 'make_tree_deleteable'],
        make_tree_call)
    self.assertEqual(1, len(self.popen_calls))
    self.assertEqual(
        [([self.temp_join(u'invalid'), u'command'], {'detached': True})],
        self.popen_calls)

  def test_run_tha_test_naked_read_only_0(self):
    isolated = json_dumps(
        {
          'command': ['invalid', 'command'],
          'read_only': 0,
        })
    isolated_hash = isolateserver_mock.hash_content(isolated)
    files = {isolated_hash:isolated}
    make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual(
        ['make_tree_writeable', 'make_tree_deleteable', 'make_tree_deleteable'],
        make_tree_call)
    self.assertEqual(1, len(self.popen_calls))
    self.assertEqual(
        [([self.temp_join(u'invalid'), u'command'], {'detached': True})],
        self.popen_calls)

  def test_run_tha_test_naked_read_only_1(self):
    isolated = json_dumps(
        {
          'command': ['invalid', 'command'],
          'read_only': 1,
        })
    isolated_hash = isolateserver_mock.hash_content(isolated)
    files = {isolated_hash:isolated}
    make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual(
        [
          'make_tree_files_read_only', 'make_tree_deleteable',
          'make_tree_deleteable',
        ],
        make_tree_call)
    self.assertEqual(1, len(self.popen_calls))
    self.assertEqual(
        [([self.temp_join(u'invalid'), u'command'], {'detached': True})],
        self.popen_calls)

  def test_run_tha_test_naked_read_only_2(self):
    isolated = json_dumps(
        {
          'command': ['invalid', 'command'],
          'read_only': 2,
        })
    isolated_hash = isolateserver_mock.hash_content(isolated)
    files = {isolated_hash:isolated}
    make_tree_call = self._run_tha_test(isolated_hash, files)
    self.assertEqual(
        ['make_tree_read_only', 'make_tree_deleteable', 'make_tree_deleteable'],
        make_tree_call)
    self.assertEqual(1, len(self.popen_calls))
    self.assertEqual(
        [([self.temp_join(u'invalid'), u'command'], {'detached': True})],
        self.popen_calls)

  def test_main_naked(self):
    self.mock(on_error, 'report', lambda _: None)
    # The most naked .isolated file that can exist.
    self.mock(tools, 'disable_buffering', lambda: None)
    isolated = json_dumps({'command': ['invalid', 'command']})
    isolated_hash = isolateserver_mock.hash_content(isolated)
    def get_storage(_isolate_server, _namespace):
      return StorageFake({isolated_hash:isolated})
    self.mock(isolateserver, 'get_storage', get_storage)

    def r(self, args, **kwargs):
      old_init(self, args, **kwargs)
      raise OSError('Unknown')
    old_init = self.mock(subprocess42.Popen, '__init__', r)

    cmd = [
        '--no-log',
        '--isolated', isolated_hash,
        '--cache', self.tempdir,
        '--isolate-server', 'https://localhost',
    ]
    ret = run_isolated.main(cmd)
    self.assertEqual(1, ret)
    self.assertEqual(1, len(self.popen_calls))
    self.assertEqual(
        [([self.temp_join(u'invalid'), u'command'], {'detached': True})],
        self.popen_calls)

  def test_modified_cwd(self):
    isolated = json_dumps({
        'command': ['../out/some.exe', 'arg'],
        'relative_cwd': 'some',
    })
    isolated_hash = isolateserver_mock.hash_content(isolated)
    files = {isolated_hash:isolated}
    _ = self._run_tha_test(isolated_hash, files)
    self.assertEqual(1, len(self.popen_calls))
    self.assertEqual(
        [([self.temp_join(u'out', u'some.exe'), 'arg'], {'detached': True})],
        self.popen_calls)

  def test_python_cmd(self):
    isolated = json_dumps({
        'command': ['../out/cmd.py', 'arg'],
        'relative_cwd': 'some',
    })
    isolated_hash = isolateserver_mock.hash_content(isolated)
    files = {isolated_hash:isolated}
    _ = self._run_tha_test(isolated_hash, files)
    self.assertEqual(1, len(self.popen_calls))
    # Injects sys.executable.
    self.assertEqual(
        [
          ([sys.executable, os.path.join(u'..', 'out', 'cmd.py'), u'arg'],
            {'detached': True}),
        ],
        self.popen_calls)


class RunIsolatedTestRun(RunIsolatedTestBase):
  def test_output(self):
    # Starts a full isolate server mock and have run_tha_test() uploads results
    # back after the task completed.
    server = isolateserver_mock.MockIsolateServer()
    try:
      script = (
        'import sys\n'
        'open(sys.argv[1], "w").write("bar")\n')
      script_hash = isolateserver_mock.hash_content(script)
      isolated = {
        'algo': 'sha-1',
        'command': ['cmd.py', '${ISOLATED_OUTDIR}/foo'],
        'files': {
          'cmd.py': {
            'h': script_hash,
            'm': 0700,
            's': len(script),
          },
        },
        'version': isolated_format.ISOLATED_FILE_VERSION,
      }
      if sys.platform == 'win32':
        isolated['files']['cmd.py'].pop('m')
      isolated_data = json_dumps(isolated)
      isolated_hash = isolateserver_mock.hash_content(isolated_data)
      server.add_content('default-store', script)
      server.add_content('default-store', isolated_data)
      store = isolateserver.get_storage(server.url, 'default-store')

      self.mock(sys, 'stdout', StringIO.StringIO())
      ret = run_isolated.run_tha_test(
          isolated_hash,
          store,
          isolateserver.MemoryCache(),
          False,
          [])
      self.assertEqual(0, ret)

      # It uploaded back. Assert the store has a new item containing foo.
      hashes = {isolated_hash, script_hash}
      output_hash = isolateserver_mock.hash_content('bar')
      hashes.add(output_hash)
      isolated =  {
        'algo': 'sha-1',
        'files': {
          'foo': {
            'h': output_hash,
            # TODO(maruel): Handle umask.
            'm': 0640,
            's': 3,
          },
        },
        'version': isolated_format.ISOLATED_FILE_VERSION,
      }
      if sys.platform == 'win32':
        isolated['files']['foo'].pop('m')
      uploaded = json_dumps(isolated)
      uploaded_hash = isolateserver_mock.hash_content(uploaded)
      hashes.add(uploaded_hash)
      self.assertEqual(hashes, set(server.contents['default-store']))

      expected = ''.join([
        '[run_isolated_out_hack]',
        '{"hash":"%s","namespace":"default-store","storage":%s}' % (
            uploaded_hash, json.dumps(server.url)),
        '[/run_isolated_out_hack]'
      ]) + '\n'
      self.assertEqual(expected, sys.stdout.getvalue())
    finally:
      server.close()


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  unittest.main()

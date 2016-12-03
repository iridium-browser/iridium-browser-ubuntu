# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import glob
import imp
import inspect
import logging
import os
import socket
import sys
import time

from catapult_base import util as catapult_util  # pylint: disable=import-error

from telemetry.core import exceptions


IsRunningOnCrosDevice = catapult_util.IsRunningOnCrosDevice
GetCatapultDir = catapult_util.GetCatapultDir


def GetBaseDir():
  main_module = sys.modules['__main__']
  if hasattr(main_module, '__file__'):
    return os.path.dirname(os.path.abspath(main_module.__file__))
  else:
    return os.getcwd()


def GetCatapultThirdPartyDir():
  return os.path.normpath(os.path.join(GetCatapultDir(), 'third_party'))


def GetTelemetryDir():
  return os.path.normpath(os.path.join(
      os.path.abspath(__file__), '..', '..', '..'))


def GetTelemetryThirdPartyDir():
  return os.path.join(GetTelemetryDir(), 'third_party')


def GetUnittestDataDir():
  return os.path.join(GetTelemetryDir(), 'telemetry', 'internal', 'testing')


def GetChromiumSrcDir():
  return os.path.normpath(os.path.join(GetTelemetryDir(), '..', '..', '..'))


_counter = [0]


def _GetUniqueModuleName():
  _counter[0] += 1
  return "page_set_module_" + str(_counter[0])


def GetPythonPageSetModule(file_path):
  return imp.load_source(_GetUniqueModuleName(), file_path)


def WaitFor(condition, timeout):
  """Waits for up to |timeout| secs for the function |condition| to return True.

  Polling frequency is (elapsed_time / 10), with a min of .1s and max of 5s.

  Returns:
    Result of |condition| function (if present).
  """
  min_poll_interval = 0.1
  max_poll_interval = 5
  output_interval = 300

  def GetConditionString():
    if condition.__name__ == '<lambda>':
      try:
        return inspect.getsource(condition).strip()
      except IOError:
        pass
    return condition.__name__

  start_time = time.time()
  last_output_time = start_time
  while True:
    res = condition()
    if res:
      return res
    now = time.time()
    elapsed_time = now - start_time
    last_output_elapsed_time = now - last_output_time
    if elapsed_time > timeout:
      raise exceptions.TimeoutException('Timed out while waiting %ds for %s.' %
                                        (timeout, GetConditionString()))
    if last_output_elapsed_time > output_interval:
      logging.info('Continuing to wait %ds for %s. Elapsed: %ds.', timeout,
                   GetConditionString(), elapsed_time)
      last_output_time = time.time()
    poll_interval = min(
        max(elapsed_time / 10., min_poll_interval), max_poll_interval)
    time.sleep(poll_interval)


class PortKeeper(object):
  """Port keeper hold an available port on the system.

  Before actually use the port, you must call Release().
  """

  def __init__(self):
    self._temp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    self._temp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    self._temp_socket.bind(('', 0))
    self._port = self._temp_socket.getsockname()[1]

  @property
  def port(self):
    return self._port

  def Release(self):
    assert self._temp_socket, 'Already released'
    self._temp_socket.close()
    self._temp_socket = None


def GetUnreservedAvailableLocalPort():
  """Returns an available port on the system.

  WARNING: This method does not reserve the port it returns, so it may be used
  by something else before you get to use it. This can lead to flake.
  """
  tmp = socket.socket()
  tmp.bind(('', 0))
  port = tmp.getsockname()[1]
  tmp.close()

  return port


def GetBuildDirectories(chrome_root=None):
  """Yields all combination of Chromium build output directories."""
  # chrome_root can be set to something else via --chrome-root.
  if not chrome_root:
    chrome_root = GetChromiumSrcDir()

  # CHROMIUM_OUTPUT_DIR can be set by --chromium-output-directory.
  output_dir = os.environ.get('CHROMIUM_OUTPUT_DIR')
  if output_dir:
    yield os.path.join(chrome_root, output_dir)
  elif os.path.exists('build.ninja'):
    yield os.getcwd()
  else:
    out_dir = os.environ.get('CHROMIUM_OUT_DIR')
    if out_dir:
      build_dirs = [out_dir]
    else:
      build_dirs = ['build',
                    'out',
                    'xcodebuild']

    build_types = ['Debug', 'Debug_x64', 'Release', 'Release_x64', 'Default']

    for build_dir in build_dirs:
      for build_type in build_types:
        yield os.path.join(chrome_root, build_dir, build_type)


def GetSequentialFileName(base_name):
  """Returns the next sequential file name based on |base_name| and the
  existing files. base_name should not contain extension.
  e.g: if base_name is /tmp/test, and /tmp/test_000.json,
  /tmp/test_001.mp3 exist, this returns /tmp/test_002. In case no
  other sequential file name exist, this will return /tmp/test_000
  """
  name, ext = os.path.splitext(base_name)
  assert ext == '', 'base_name cannot contain file extension.'
  index = 0
  while True:
    output_name = '%s_%03d' % (name, index)
    if not glob.glob(output_name + '.*'):
      break
    index = index + 1
  return output_name

# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import psutil
import signal

from pylib.device import device_errors
from pylib.device import device_utils


def _KillWebServers():
  for s in [signal.SIGTERM, signal.SIGINT, signal.SIGQUIT, signal.SIGKILL]:
    signalled = []
    for server in ['lighttpd', 'webpagereplay']:
      for p in psutil.process_iter():
        try:
          if not server in ' '.join(p.cmdline):
            continue
          logging.info('Killing %s %s %s', s, server, p.pid)
          p.send_signal(s)
          signalled.append(p)
        except Exception:
          logging.exception('Failed killing %s %s', server, p.pid)
    for p in signalled:
      try:
        p.wait(1)
      except Exception:
        logging.exception('Failed waiting for %s to die.', p.pid)


def CleanupLeftoverProcesses(devices):
  """Clean up the test environment, restarting fresh adb and HTTP daemons.

  Args:
    devices: The devices to clean.
  """
  _KillWebServers()
  device_utils.RestartServer()

  def cleanup_device(d):
    d.RestartAdbd()
    try:
      d.EnableRoot()
    except device_errors.CommandFailedError:
      logging.exception('Failed to enable root')
    d.WaitUntilFullyBooted()

  device_utils.DeviceUtils.parallel(devices).pMap(cleanup_device)


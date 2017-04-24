# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Send system monitoring data to the timeseries monitoring API."""

from __future__ import print_function

import random
import time

import psutil

from chromite.lib import commandline
from chromite.lib import cros_logging as logging
from chromite.lib import metrics
from chromite.lib import ts_mon_config
from chromite.scripts.sysmon import loop
from chromite.scripts.sysmon import osinfo_metrics
from chromite.scripts.sysmon import prod_metrics
from chromite.scripts.sysmon import puppet_metrics
from chromite.scripts.sysmon import system_metrics
from infra_libs.ts_mon.common import interface

logger = logging.getLogger(__name__)


class _MetricCollector(object):
  """Metric collector class."""

  def __init__(self, collect_prod_hosts=False):
    self._get_osinfo = _TimedCallback(
        callback=osinfo_metrics.get_os_info,
        interval=60 * 60)
    if collect_prod_hosts:
      logger.info('Enabling prod host metric collection.')
      self._get_prod_hosts = _TimedCallback(
          callback=prod_metrics.get_prod_hosts,
          interval=10 * 60)
    else:
      self._get_prod_hosts = lambda: None

  def __call__(self):
    """Collect metrics."""
    system_metrics.get_uptime()
    system_metrics.get_cpu_info()
    system_metrics.get_disk_info()
    system_metrics.get_mem_info()
    system_metrics.get_net_info()
    system_metrics.get_proc_info()
    system_metrics.get_load_avg()
    puppet_metrics.get_puppet_summary()
    self._get_prod_hosts()
    self._get_osinfo()
    system_metrics.get_unix_time()  # must be just before flush
    metrics.Flush()

  @property
  def _next_osinfo_collection(self):
    return self._last_osinfo_collection + (60 * 60)


class _TimedCallback(object):
  """Limits callback to one call in a given interval."""

  def __init__(self, callback, interval):
    """Initialize instance.

    Args:
      callback: function to call
      interval: Number of seconds between allowed calls
    """
    self._callback = callback
    self._interval = interval
    self._last_called = time.time() - interval

  def __call__(self):
    if time.time() > self._next_call:
      self._callback()
      self._last_called = time.time()

  @property
  def _next_call(self):
    return self._last_called + self._interval


def main():
  parser = commandline.ArgumentParser(
      description=__doc__,
      default_log_level='DEBUG')
  parser.add_argument(
      '--interval',
      default=60,
      type=int,
      help='time (in seconds) between sampling system metrics')
  parser.add_argument(
      '--collect-prod-hosts',
      action='store_true',
      help='Enable collection of prod host metrics, like roles')
  opts = parser.parse_args()
  opts.Freeze()

  # This returns a 0 value the first time it's called.  Call it now and
  # discard the return value.
  psutil.cpu_times_percent()

  # Wait a random amount of time before starting the loop in case sysmon
  # is started at exactly the same time on all machines.
  time.sleep(random.uniform(0, opts.interval))

  # This call returns a context manager that doesn't do anything, so we
  # ignore the return value.
  ts_mon_config.SetupTsMonGlobalState('sysmon', auto_flush=False)
  # The default prefix is '/chrome/infra/'.
  interface.state.metric_name_prefix = (interface.state.metric_name_prefix
                                        + 'chromeos/sysmon/')

  collector = _MetricCollector(collect_prod_hosts=opts.collect_prod_hosts)
  loop.SleepLoop(callback=collector,
                 interval=opts.interval).loop_forever()


if __name__ == '__main__':
  main()

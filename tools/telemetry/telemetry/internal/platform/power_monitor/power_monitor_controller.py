# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import atexit
import logging

import telemetry.internal.platform.power_monitor as power_monitor


def _ReenableChargingIfNeeded(battery):
  if not battery.GetCharging():
    battery.TieredSetCharging(True)

class PowerMonitorController(power_monitor.PowerMonitor):
  """
  PowerMonitor that acts as facade for a list of PowerMonitor objects and uses
  the first available one.
  """
  def __init__(self, power_monitors, battery):
    super(PowerMonitorController, self).__init__()
    self._candidate_power_monitors = power_monitors
    self._active_monitors = []
    self._battery = battery
    atexit.register(_ReenableChargingIfNeeded, self._battery)

  def CanMonitorPower(self):
    return any(m.CanMonitorPower() for m in self._candidate_power_monitors)

  def StartMonitoringPower(self, browser):
    assert not self._active_monitors, 'Must call StopMonitoringPower().'
    self._active_monitors = (
        [m for m in self._candidate_power_monitors if m.CanMonitorPower()])
    assert self._active_monitors, 'No available monitor.'
    for monitor in self._active_monitors:
      monitor.StartMonitoringPower(browser)

  @staticmethod
  def _MergePowerResults(combined_results, monitor_results):
    """
    Merges monitor_results into combined_results and leaves monitor_results
    values if there are merge conflicts.
    """
    def _CheckDuplicateKeys(dict_one, dict_two, ignore_list=None):
      for key in dict_one:
        if key in dict_two and key not in ignore_list:
          logging.warning('Found multiple instances of %s in power monitor '
                          'enteries. Using newest one.', key)
    # Sub level power enteries.
    for part in ['platform_info', 'component_utilization']:
      if part in monitor_results:
        _CheckDuplicateKeys(combined_results[part], monitor_results[part])
        combined_results[part].update(monitor_results[part])

    # Top level power enteries.
    platform_info = combined_results['platform_info'].copy()
    comp_utilization = combined_results['component_utilization'].copy()
    _CheckDuplicateKeys(
        combined_results, monitor_results,
        ['identifier', 'platform_info', 'component_utilization'])
    combined_results.update(monitor_results)
    combined_results['platform_info'] = platform_info
    combined_results['component_utilization'] = comp_utilization

  def StopMonitoringPower(self):
    assert self._active_monitors, 'StartMonitoringPower() not called.'
    try:
      results = {'platform_info': {}, 'component_utilization': {}}
      for monitor in self._active_monitors:
        self._MergePowerResults(results, monitor.StopMonitoringPower())
      return results
    finally:
      self._active_monitors = []

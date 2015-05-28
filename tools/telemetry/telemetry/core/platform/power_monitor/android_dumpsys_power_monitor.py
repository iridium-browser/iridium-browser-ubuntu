# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import csv
import logging

from telemetry.core.platform.power_monitor import sysfs_power_monitor


class DumpsysPowerMonitor(sysfs_power_monitor.SysfsPowerMonitor):
  """PowerMonitor that relies on the dumpsys batterystats to monitor the power
  consumption of a single android application. This measure uses a heuristic
  and is the same information end-users see with the battery application.
  Available on Android L and higher releases.
  """
  def __init__(self, device, platform_backend):
    """Constructor.

    Args:
        device: A DeviceUtil instance.
        platform_backend: A LinuxBasedPlatformBackend instance.
    """
    super(DumpsysPowerMonitor, self).__init__(platform_backend)
    self._browser = None
    self._device = device

  def CanMonitorPower(self):
    return self._device.old_interface.CanControlUsbCharging()

  def StartMonitoringPower(self, browser):
    super(DumpsysPowerMonitor, self).StartMonitoringPower(browser)
    self._browser = browser
    # Disable the charging of the device over USB. This is necessary because the
    # device only collects information about power usage when the device is not
    # charging.
    self._device.DisableBatteryUpdates()

  def StopMonitoringPower(self):
    if self._browser:
      package = self._browser._browser_backend.package
      self._browser = None
    cpu_stats = super(DumpsysPowerMonitor, self).StopMonitoringPower()
    self._device.EnableBatteryUpdates()
    # By default, 'dumpsys batterystats' measures power consumption during the
    # last unplugged period.
    result = self._platform.RunCommand('dumpsys batterystats -c %s' % package)
    assert result, 'Dumpsys produced no output'
    return super(DumpsysPowerMonitor, self).CombineResults(
        cpu_stats, DumpsysPowerMonitor.ParseSamplingOutput(package,
                                                           result.split('\n')))

  @staticmethod
  def ParseSamplingOutput(package, dumpsys_output):
    """Parse output of 'dumpsys batterystats -c'

    See:
    https://android.googlesource.com/platform/frameworks/base/+/master/core/java/android/os/BatteryStats.java

    Returns:
        Dictionary in the format returned by StopMonitoringPower().
    """
    # Raw power usage samples.
    out_dict = {}
    out_dict['identifier'] = 'dumpsys'
    out_dict['power_samples_mw'] = []

    # The list of useful CSV columns.
    # Index of the column containing the format version.
    DUMP_VERSION_INDEX = 0
    # Index of the column containing the type of the row.
    ROW_TYPE_INDEX = 3

    # Index for columns of type unique identifier ('uid')
    # Index of the column containing the uid.
    PACKAGE_UID_INDEX = 4
    # Index of the column containing the application package.
    PACKAGE_NAME_INDEX = 5

    # Index for columns of type power use ('pwi')
    # The column containing the uid of the item.
    PWI_UID_INDEX = 1
    # The column containing the type of consumption. Only consumtion since last
    # charge are of interest here.
    PWI_AGGREGATION_INDEX = 2
    # The column containing the amount of power used, in mah.
    PWI_POWER_COMSUMPTION_INDEX = 5
    csvreader = csv.reader(dumpsys_output)
    uid_entries = {}
    pwi_entries = defaultdict(list)
    for entry in csvreader:
      if entry[DUMP_VERSION_INDEX] not in ['8', '9']:
        # Wrong file version.
        break
      if ROW_TYPE_INDEX >= len(entry):
        continue
      if entry[ROW_TYPE_INDEX] == 'uid':
        current_package = entry[PACKAGE_NAME_INDEX]
        assert current_package not in uid_entries
        uid_entries[current_package] = entry[PACKAGE_UID_INDEX]
      elif (PWI_POWER_COMSUMPTION_INDEX < len(entry) and
            entry[ROW_TYPE_INDEX] == 'pwi' and
            entry[PWI_AGGREGATION_INDEX] == 'l'):
        pwi_entries[entry[PWI_UID_INDEX]].append(
            float(entry[PWI_POWER_COMSUMPTION_INDEX]))

    # Find the uid of for the given package.
    if not package in uid_entries:
      logging.warning('Unable to parse dumpsys output. '
                      'Please upgrade the OS version of the device.'
                      'package=%s, uid_entries=%s' % (package, uid_entries))
      out_dict['energy_consumption_mwh'] = 0
      return out_dict
    uid = uid_entries[package]
    consumptions_mah = pwi_entries[uid]
    consumption_mah = sum(consumptions_mah)
    # Converting at a nominal voltage of 4.0V, as those values are obtained by a
    # heuristic, and 4.0V is the voltage we set when using a monsoon device.
    consumption_mwh = consumption_mah * 4.0
    out_dict['energy_consumption_mwh'] = consumption_mwh
    return out_dict

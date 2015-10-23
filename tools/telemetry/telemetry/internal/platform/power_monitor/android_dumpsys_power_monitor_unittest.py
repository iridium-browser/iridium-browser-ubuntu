# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from telemetry.internal.platform.power_monitor import android_dumpsys_power_monitor


class DumpsysPowerMonitorMonitorTest(unittest.TestCase):

  def testEnergyComsumption(self):
    package = 'com.google.android.apps.chrome'
    power_data = {'data': [23.9], 'uid': '12345'}
    results = (
        android_dumpsys_power_monitor.DumpsysPowerMonitor.ProcessPowerData(
            power_data, 4.0, package))
    self.assertEqual(results['identifier'], 'dumpsys')
    self.assertAlmostEqual(results['energy_consumption_mwh'], 95.6)

  # Older version of the OS do not have the data.
  def testNoData(self):
    package = 'com.android.chrome'
    power_data = None
    results = (
        android_dumpsys_power_monitor.DumpsysPowerMonitor.ProcessPowerData(
            power_data, 4.0, package))
    self.assertEqual(results['identifier'], 'dumpsys')
    self.assertEqual(results['energy_consumption_mwh'], 0)


if __name__ == '__main__':
  unittest.main()

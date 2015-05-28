# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import wpr_modes
from telemetry import decorators
from telemetry.unittest_util import options_for_unittests
from telemetry.unittest_util import page_test_test_case

from measurements import record_per_area


class RecordPerAreaUnitTest(page_test_test_case.PageTestTestCase):
  """Smoke test for record_per_area measurement

     Runs record_per_area measurement on a simple page and verifies
     that all metrics were added to the results. The test is purely functional,
     i.e. it only checks if the metrics are present and non-zero.
  """

  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  @decorators.Disabled('android')
  def testRecordPerArea(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')
    measurement = record_per_area.RecordPerArea()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(results.failures))

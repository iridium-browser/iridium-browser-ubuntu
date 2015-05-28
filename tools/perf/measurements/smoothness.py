# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_test

from measurements import smoothness_controller
from metrics import power


class Smoothness(page_test.PageTest):
  def __init__(self):
    super(Smoothness, self).__init__()
    self._power_metric = None
    self._smoothness_controller = None

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')
    options.AppendExtraBrowserArgs('--touch-events=enabled')
    options.AppendExtraBrowserArgs('--running-performance-benchmark')
    power.PowerMetric.CustomizeBrowserOptions(options)

  def WillStartBrowser(self, platform):
    self._power_metric = power.PowerMetric(platform)

  def WillNavigateToPage(self, page, tab):
    self._power_metric.Start(page, tab)
    self._smoothness_controller = smoothness_controller.SmoothnessController()
    self._smoothness_controller.SetUp(page, tab)

  def WillRunActions(self, page, tab):
    self._smoothness_controller.Start(tab)

  def DidRunActions(self, page, tab):
    self._power_metric.Stop(page, tab)
    self._smoothness_controller.Stop(tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    self._power_metric.AddResults(tab, results)
    self._smoothness_controller.AddResults(tab, results)

  def CleanUpAfterPage(self, page, tab):
    if self._power_metric:
      self._power_metric.Stop(page, tab)
    if self._smoothness_controller:
      self._smoothness_controller.CleanUp(tab)

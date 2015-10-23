# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Chromium's IndexedDB performance test. These test:

Databases:
  create/delete
Keys:
  create/delete
Indexes:
  create/delete
Data access:
  Random read/write
  Read cache
Cursors:
  Read & random writes
  Walking multiple
  Seeking.
"""

import json
import os

from core import perf_benchmark

from telemetry.core import util
from telemetry import page as page_module
from telemetry.page import page_test
from telemetry import story
from telemetry.value import scalar

from metrics import memory
from metrics import power


class _IndexedDbMeasurement(page_test.PageTest):
  def __init__(self):
    super(_IndexedDbMeasurement, self).__init__()
    self._memory_metric = None
    self._power_metric = None

  def WillStartBrowser(self, platform):
    """Initialize metrics once right before the browser has been launched."""
    self._power_metric = power.PowerMetric(platform)

  def DidStartBrowser(self, browser):
    """Initialize metrics once right after the browser has been launched."""
    self._memory_metric = memory.MemoryMetric(browser)

  def DidNavigateToPage(self, page, tab):
    self._memory_metric.Start(page, tab)
    self._power_metric.Start(page, tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForDocumentReadyStateToBeComplete()
    tab.WaitForJavaScriptExpression(
        'window.document.cookie.indexOf("__done=1") >= 0', 600)

    self._power_metric.Stop(page, tab)
    self._memory_metric.Stop(page, tab)

    self._memory_metric.AddResults(tab, results)
    self._power_metric.AddResults(tab, results)

    js_get_results = "JSON.stringify(automation.getResults());"
    result_dict = json.loads(tab.EvaluateJavaScript(js_get_results))
    total = 0.0
    for key in result_dict:
      if key == 'OverallTestDuration':
        continue
      msec = float(result_dict[key])
      results.AddValue(scalar.ScalarValue(
          results.current_page, key, 'ms', msec, important=False))

      total += msec
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'Total Perf', 'ms', total))


  def CustomizeBrowserOptions(self, options):
    memory.MemoryMetric.CustomizeBrowserOptions(options)
    power.PowerMetric.CustomizeBrowserOptions(options)

class IndexedDb(perf_benchmark.PerfBenchmark):
  """Chromium's IndexedDB Performance tests."""
  test = _IndexedDbMeasurement

  @classmethod
  def Name(cls):
    return 'indexeddb_perf'

  def CreateStorySet(self, options):
    indexeddb_dir = os.path.join(util.GetChromiumSrcDir(), 'chrome', 'test',
                                 'data', 'indexeddb')
    ps = story.StorySet(base_dir=indexeddb_dir)
    ps.AddStory(page_module.Page('file://perf_test.html', ps, ps.base_dir))
    return ps

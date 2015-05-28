# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import cloud_storage_test_base
import gpu_rasterization_expectations
import optparse
import page_sets

from telemetry.image_processing import image_util


test_harness_script = r"""
  var domAutomationController = {};
  domAutomationController._succeeded = false;
  domAutomationController._finished = false;

  domAutomationController.setAutomationId = function(id) {}
  domAutomationController.send = function(msg) {
    domAutomationController._finished = true;
    if (msg.toLowerCase() == "success")
      domAutomationController._succeeded = true;
    else
      domAutomationController._succeeded = false;
  }

  window.domAutomationController = domAutomationController;
"""

def _DidTestSucceed(tab):
  return tab.EvaluateJavaScript('domAutomationController._succeeded')

class _GpuRasterizationValidator(cloud_storage_test_base.ValidatorBase):
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--enable-threaded-compositing',
                                    '--enable-impl-side-painting',
                                    '--force-gpu-rasterization',
                                    '--enable-gpu-benchmarking'])

  def ValidateAndMeasurePage(self, page, tab, results):
    if not _DidTestSucceed(tab):
      raise page_test.Failure('Page indicated a failure')

    if not hasattr(page, 'expectations') or not page.expectations:
      raise page_test.Failure('Expectations not specified')

    if not tab.screenshot_supported:
      raise page_test.Failure('Browser does not support screenshot capture')

    screenshot = tab.Screenshot()
    if screenshot is None:
      raise page_test.Failure('Could not capture screenshot')

    device_pixel_ratio = tab.EvaluateJavaScript('window.devicePixelRatio')
    if hasattr(page, 'test_rect'):
      test_rect = [int(x * device_pixel_ratio) for x in page.test_rect]
      screenshot = image_util.Crop(screenshot, test_rect[0], test_rect[1],
                                   test_rect[2], test_rect[3])

    self._ValidateScreenshotSamples(
        page.display_name,
        screenshot,
        page.expectations,
        device_pixel_ratio)


class GpuRasterization(cloud_storage_test_base.TestBase):
  """Tests that GPU rasterization produces valid content"""
  test = _GpuRasterizationValidator

  @classmethod
  def Name(cls):
    return 'gpu_rasterization'

  def CreatePageSet(self, options):
    page_set = page_sets.GpuRasterizationTestsPageSet()
    for page in page_set.pages:
      page.script_to_evaluate_on_commit = test_harness_script
    return page_set

  def CreateExpectations(self):
    return gpu_rasterization_expectations.GpuRasterizationExpectations()

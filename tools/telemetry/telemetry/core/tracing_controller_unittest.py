# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import decorators
from telemetry.testing import tab_test_case
from telemetry.timeline import tracing_category_filter
from telemetry.timeline import tracing_options

class TracingControllerTest(tab_test_case.TabTestCase):

  @decorators.Isolated
  def testModifiedConsoleTime(self):
    tracing_controller = self._tab.browser.platform.tracing_controller
    category_filter = tracing_category_filter.TracingCategoryFilter()
    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True
    tracing_controller.Start(options, category_filter)
    self.Navigate('blank.html')
    self.assertEquals(
        self._tab.EvaluateJavaScript('document.location.pathname;'),
        '/blank.html')

    self._tab.EvaluateJavaScript("""
        window.__console_time = console.time;
        console.time = function() { };
        """)
    with self.assertRaisesRegexp(Exception, 'Page stomped on console.time'):
      tracing_controller.Stop()

    # Restore console.time and stop tracing so that any subsequent tests will
    # be able to use tracing normally. See crbug.com/520663.
    self._tab.EvaluateJavaScript("""
        console.time = window.__console_time;
        delete window.__console_time;
        """)
    tracing_controller.Stop()

    # Check that subsequent tests will be able to use tracing normally.
    self.assertFalse(tracing_controller.is_tracing_running)
    tracing_controller.Start(options, category_filter)
    self.assertTrue(tracing_controller.is_tracing_running)
    tracing_controller.Stop()
    self.assertFalse(tracing_controller.is_tracing_running)

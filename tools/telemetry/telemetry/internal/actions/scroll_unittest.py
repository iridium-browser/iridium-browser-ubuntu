# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.internal.actions import scroll
from telemetry.testing import tab_test_case

class ScrollActionTest(tab_test_case.TabTestCase):
  def testScrollAction(self):
    self.Navigate('blank.html')

    # Make page bigger than window so it's scrollable.
    self._tab.ExecuteJavaScript("""document.body.style.height =
                              (2 * window.innerHeight + 1) + 'px';""")

    self.assertEquals(
        self._tab.EvaluateJavaScript("""document.documentElement.scrollTop
                                   || document.body.scrollTop"""), 0)

    i = scroll.ScrollAction()
    i.WillRunAction(self._tab)

    self._tab.ExecuteJavaScript("""
        window.__scrollAction.beginMeasuringHook = function() {
            window.__didBeginMeasuring = true;
        };
        window.__scrollAction.endMeasuringHook = function() {
            window.__didEndMeasuring = true;
        };""")
    i.RunAction(self._tab)

    self.assertTrue(self._tab.EvaluateJavaScript('window.__didBeginMeasuring'))
    self.assertTrue(self._tab.EvaluateJavaScript('window.__didEndMeasuring'))

    scroll_position = self._tab.EvaluateJavaScript(
        '(document.documentElement.scrollTop || document.body.scrollTop)')
    self.assertTrue(scroll_position != 0,
                    msg='scroll_position=%d;' % (scroll_position))

  def testDiagonalScrollAction(self):
    # Diagonal scrolling was not supported in the ScrollAction until Chrome
    # branch number 2332
    branch_num = self._tab.browser._browser_backend.devtools_client \
        .GetChromeBranchNumber()
    if branch_num < 2332:
      return

    self.Navigate('blank.html')

    # Make page bigger than window so it's scrollable.
    self._tab.ExecuteJavaScript("""document.body.style.height =
                              (2 * window.innerHeight + 1) + 'px';""")
    self._tab.ExecuteJavaScript("""document.body.style.width =
                              (2 * window.innerWidth + 1) + 'px';""")

    self.assertEquals(
        self._tab.EvaluateJavaScript("""document.documentElement.scrollTop
                                   || document.body.scrollTop"""), 0)
    self.assertEquals(
        self._tab.EvaluateJavaScript("""document.documentElement.scrollLeft
                                   || document.body.scrollLeft"""), 0)

    i = scroll.ScrollAction(direction='downright')
    i.WillRunAction(self._tab)

    i.RunAction(self._tab)

    viewport_top = self._tab.EvaluateJavaScript(
        '(document.documentElement.scrollTop || document.body.scrollTop)')
    self.assertTrue(viewport_top != 0, msg='viewport_top=%d;' % viewport_top)

    viewport_left = self._tab.EvaluateJavaScript(
        '(document.documentElement.scrollLeft || document.body.scrollLeft)')
    self.assertTrue(viewport_left != 0, msg='viewport_left=%d;' % viewport_left)

  def testBoundingClientRect(self):
    self.Navigate('blank.html')

    with open(os.path.join(os.path.dirname(__file__),
                           'gesture_common.js')) as f:
      js = f.read()
      self._tab.ExecuteJavaScript(js)

    # Verify that the rect returned by getBoundingVisibleRect() in scroll.js is
    # completely contained within the viewport. Scroll events dispatched by the
    # scrolling API use the center of this rect as their location, and this
    # location needs to be within the viewport bounds to correctly decide
    # between main-thread and impl-thread scroll. If the scrollable area were
    # not clipped to the viewport bounds, then the instance used here (the
    # scrollable area being more than twice as tall as the viewport) would
    # result in a scroll location outside of the viewport bounds.
    self._tab.ExecuteJavaScript("""document.body.style.height =
                           (3 * window.innerHeight + 1) + 'px';""")
    self._tab.ExecuteJavaScript("""document.body.style.width =
                           (3 * window.innerWidth + 1) + 'px';""")
    self._tab.ExecuteJavaScript(
        "window.scrollTo(window.innerWidth, window.innerHeight);")

    rect_top = int(self._tab.EvaluateJavaScript(
        '__GestureCommon_GetBoundingVisibleRect(document.body).top'))
    rect_height = int(self._tab.EvaluateJavaScript(
        '__GestureCommon_GetBoundingVisibleRect(document.body).height'))
    rect_bottom = rect_top + rect_height

    rect_left = int(self._tab.EvaluateJavaScript(
        '__GestureCommon_GetBoundingVisibleRect(document.body).left'))
    rect_width = int(self._tab.EvaluateJavaScript(
        '__GestureCommon_GetBoundingVisibleRect(document.body).width'))
    rect_right = rect_left + rect_width

    viewport_height = int(self._tab.EvaluateJavaScript('window.innerHeight'))
    viewport_width = int(self._tab.EvaluateJavaScript('window.innerWidth'))

    self.assertTrue(rect_top >= 0,
        msg='%s >= %s' % (rect_top, 0))
    self.assertTrue(rect_left >= 0,
        msg='%s >= %s' % (rect_left, 0))
    self.assertTrue(rect_bottom <= viewport_height,
        msg='%s + %s <= %s' % (rect_top, rect_height, viewport_height))
    self.assertTrue(rect_right <= viewport_width,
        msg='%s + %s <= %s' % (rect_left, rect_width, viewport_width))

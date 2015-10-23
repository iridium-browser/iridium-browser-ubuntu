# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for quick_logger module."""

import unittest

from dashboard import quick_logger
from dashboard import testing_common


class QuickLoggerTest(testing_common.TestCase):

  def testQuickLogger(self):
    """Tests basic logging."""
    template = '{message}{extra}'
    formatter = quick_logger.Formatter(template, extra='!')
    logger = quick_logger.QuickLogger('a_namespace', 'a_log_name', formatter)
    logger.Log('Hello %s', 'world')
    logger.Save()
    logs = quick_logger.Get('a_namespace', 'a_log_name')
    self.assertEqual(len(logs), 1)
    self.assertEqual(logs[0].message, 'Hello world!')

  def testQuickLogger_LogSizeAndNumberAtSizeLimit(self):
    """Tests quick_logger limits."""
    logger = quick_logger.QuickLogger('a_namespace', 'a_log_name')
    for i in xrange(quick_logger._MAX_NUM_RECORD):
      logger.Log(str(i%2) * quick_logger._MAX_MSG_SIZE)
    logger.Save()
    logs = quick_logger.Get('a_namespace', 'a_log_name')
    self.assertEqual(len(logs), quick_logger._MAX_NUM_RECORD)

  def testQuickLogger_MultipleLogs_UsesCorrectOrder(self):
    """Logger should keep most recent logs."""
    logger = quick_logger.QuickLogger('a_namespace', 'a_log_name')
    for i in xrange(quick_logger._MAX_NUM_RECORD + 10):
      logger.Log(i)
    logger.Save()
    logs = quick_logger.Get('a_namespace', 'a_log_name')
    self.assertEqual(len(logs), quick_logger._MAX_NUM_RECORD)
    # First record is the last log added.
    self.assertEqual(logs[0].message, str(quick_logger._MAX_NUM_RECORD + 9))


if __name__ == '__main__':
  unittest.main()

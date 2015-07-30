# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from telemetry import page as page_module
from telemetry.page import page_set
from telemetry.results import page_test_results
from telemetry.value import failure
from telemetry.value import histogram
from telemetry.value import list_of_scalar_values
from telemetry.value import scalar
from telemetry.value import summary as summary_module

class TestBase(unittest.TestCase):
  def setUp(self):
    ps = page_set.PageSet(file_path=os.path.dirname(__file__))
    ps.AddUserStory(page_module.Page('http://www.bar.com/', ps, ps.base_dir))
    ps.AddUserStory(page_module.Page('http://www.baz.com/', ps, ps.base_dir))
    ps.AddUserStory(page_module.Page('http://www.foo.com/', ps, ps.base_dir))
    self.page_set = ps

  @property
  def pages(self):
    return self.page_set.pages


class SummaryTest(TestBase):
  def testBasicSummary(self):
    page0 = self.pages[0]
    page1 = self.pages[1]

    results = page_test_results.PageTestResults()

    results.WillRunPage(page0)
    v0 = scalar.ScalarValue(page0, 'a', 'seconds', 3)
    results.AddValue(v0)
    results.DidRunPage(page0)

    results.WillRunPage(page1)
    v1 = scalar.ScalarValue(page1, 'a', 'seconds', 7)
    results.AddValue(v1)
    results.DidRunPage(page1)

    summary = summary_module.Summary(results.all_page_specific_values)
    values = summary.interleaved_computed_per_page_values_and_summaries

    v0_list = list_of_scalar_values.ListOfScalarValues(
        page0, 'a', 'seconds', [3])
    v1_list = list_of_scalar_values.ListOfScalarValues(
        page1, 'a', 'seconds', [7])
    merged_value = list_of_scalar_values.ListOfScalarValues(
      None, 'a', 'seconds', [3, 7])

    self.assertEquals(3, len(values))
    self.assertIn(v0_list, values)
    self.assertIn(v1_list, values)
    self.assertIn(merged_value, values)

  def testBasicSummaryWithOnlyOnePage(self):
    page0 = self.pages[0]

    results = page_test_results.PageTestResults()

    results.WillRunPage(page0)
    v0 = scalar.ScalarValue(page0, 'a', 'seconds', 3)
    results.AddValue(v0)
    results.DidRunPage(page0)

    summary = summary_module.Summary(results.all_page_specific_values)
    values = summary.interleaved_computed_per_page_values_and_summaries

    v0_list = list_of_scalar_values.ListOfScalarValues(
        None, 'a', 'seconds', [3])

    self.assertEquals([v0_list], values)

  def testBasicSummaryNonuniformResults(self):
    page0 = self.pages[0]
    page1 = self.pages[1]
    page2 = self.pages[2]

    results = page_test_results.PageTestResults()
    results.WillRunPage(page0)
    v0 = scalar.ScalarValue(page0, 'a', 'seconds', 3)
    results.AddValue(v0)
    v1 = scalar.ScalarValue(page0, 'b', 'seconds', 10)
    results.AddValue(v1)
    results.DidRunPage(page0)

    results.WillRunPage(page1)
    v2 = scalar.ScalarValue(page1, 'a', 'seconds', 3)
    results.AddValue(v2)
    v3 = scalar.ScalarValue(page1, 'b', 'seconds', 10)
    results.AddValue(v3)
    results.DidRunPage(page1)

    results.WillRunPage(page2)
    v4 = scalar.ScalarValue(page2, 'a', 'seconds', 7)
    results.AddValue(v4)
    # Note, page[2] does not report a 'b' metric.
    results.DidRunPage(page2)

    summary = summary_module.Summary(results.all_page_specific_values)
    values = summary.interleaved_computed_per_page_values_and_summaries

    v0_list = list_of_scalar_values.ListOfScalarValues(
        page0, 'a', 'seconds', [3])
    v1_list = list_of_scalar_values.ListOfScalarValues(
        page0, 'b', 'seconds', [10])
    v2_list = list_of_scalar_values.ListOfScalarValues(
        page1, 'a', 'seconds', [3])
    v3_list = list_of_scalar_values.ListOfScalarValues(
        page1, 'b', 'seconds', [10])
    v4_list = list_of_scalar_values.ListOfScalarValues(
        page2, 'a', 'seconds', [7])

    a_summary = list_of_scalar_values.ListOfScalarValues(
        None, 'a', 'seconds', [3, 3, 7])
    b_summary = list_of_scalar_values.ListOfScalarValues(
        None, 'b', 'seconds', [10, 10])

    self.assertEquals(7, len(values))
    self.assertIn(v0_list, values)
    self.assertIn(v1_list, values)
    self.assertIn(v2_list, values)
    self.assertIn(v3_list, values)
    self.assertIn(v4_list, values)
    self.assertIn(a_summary, values)
    self.assertIn(b_summary, values)

  def testBasicSummaryPassAndFailPage(self):
    """If a page failed, only print summary for individual pages."""
    page0 = self.pages[0]
    page1 = self.pages[1]

    results = page_test_results.PageTestResults()
    results.WillRunPage(page0)
    v0 = scalar.ScalarValue(page0, 'a', 'seconds', 3)
    results.AddValue(v0)
    v1 = failure.FailureValue.FromMessage(page0, 'message')
    results.AddValue(v1)
    results.DidRunPage(page0)

    results.WillRunPage(page1)
    v2 = scalar.ScalarValue(page1, 'a', 'seconds', 7)
    results.AddValue(v2)
    results.DidRunPage(page1)

    summary = summary_module.Summary(results.all_page_specific_values)
    values = summary.interleaved_computed_per_page_values_and_summaries

    v0_list = list_of_scalar_values.ListOfScalarValues(
        page0, 'a', 'seconds', [3])
    v2_list = list_of_scalar_values.ListOfScalarValues(
        page1, 'a', 'seconds', [7])

    self.assertEquals(2, len(values))
    self.assertIn(v0_list, values)
    self.assertIn(v2_list, values)

  def testRepeatedPagesetOneIterationOnePageFails(self):
    """Page fails on one iteration, no averaged results should print."""
    page0 = self.pages[0]
    page1 = self.pages[1]

    results = page_test_results.PageTestResults()
    results.WillRunPage(page0)
    v0 = scalar.ScalarValue(page0, 'a', 'seconds', 3)
    results.AddValue(v0)
    results.DidRunPage(page0)

    results.WillRunPage(page1)
    v1 = scalar.ScalarValue(page1, 'a', 'seconds', 7)
    results.AddValue(v1)
    v2 = failure.FailureValue.FromMessage(page1, 'message')
    results.AddValue(v2)
    results.DidRunPage(page1)

    results.WillRunPage(page0)
    v3 = scalar.ScalarValue(page0, 'a', 'seconds', 4)
    results.AddValue(v3)
    results.DidRunPage(page0)

    results.WillRunPage(page1)
    v4 = scalar.ScalarValue(page1, 'a', 'seconds', 8)
    results.AddValue(v4)
    results.DidRunPage(page1)

    summary = summary_module.Summary(results.all_page_specific_values)
    values = summary.interleaved_computed_per_page_values_and_summaries

    page0_aggregated = list_of_scalar_values.ListOfScalarValues(
        page0, 'a', 'seconds', [3, 4])
    page1_aggregated = list_of_scalar_values.ListOfScalarValues(
        page1, 'a', 'seconds', [7, 8])

    self.assertEquals(2, len(values))
    self.assertIn(page0_aggregated, values)
    self.assertIn(page1_aggregated, values)

  def testRepeatedPages(self):
    page0 = self.pages[0]
    page1 = self.pages[1]

    results = page_test_results.PageTestResults()
    results.WillRunPage(page0)
    v0 = scalar.ScalarValue(page0, 'a', 'seconds', 3)
    results.AddValue(v0)
    results.DidRunPage(page0)

    results.WillRunPage(page0)
    v2 = scalar.ScalarValue(page0, 'a', 'seconds', 4)
    results.AddValue(v2)
    results.DidRunPage(page0)

    results.WillRunPage(page1)
    v1 = scalar.ScalarValue(page1, 'a', 'seconds', 7)
    results.AddValue(v1)
    results.DidRunPage(page1)

    results.WillRunPage(page1)
    v3 = scalar.ScalarValue(page1, 'a', 'seconds', 8)
    results.AddValue(v3)
    results.DidRunPage(page1)

    summary = summary_module.Summary(results.all_page_specific_values)
    values = summary.interleaved_computed_per_page_values_and_summaries

    page0_aggregated = list_of_scalar_values.ListOfScalarValues(
        page0, 'a', 'seconds', [3, 4])
    page1_aggregated = list_of_scalar_values.ListOfScalarValues(
        page1, 'a', 'seconds', [7, 8])
    a_summary = list_of_scalar_values.ListOfScalarValues(
        None, 'a', 'seconds', [3, 4, 7, 8])

    self.assertEquals(3, len(values))
    self.assertIn(page0_aggregated, values)
    self.assertIn(page1_aggregated, values)
    self.assertIn(a_summary, values)

  def testPageRunsTwice(self):
    page0 = self.pages[0]

    results = page_test_results.PageTestResults()

    results.WillRunPage(page0)
    v0 = scalar.ScalarValue(page0, 'b', 'seconds', 2)
    results.AddValue(v0)
    results.DidRunPage(page0)

    results.WillRunPage(page0)
    v1 = scalar.ScalarValue(page0, 'b', 'seconds', 3)
    results.AddValue(v1)
    results.DidRunPage(page0)

    summary = summary_module.Summary(results.all_page_specific_values)
    values = summary.interleaved_computed_per_page_values_and_summaries

    page0_aggregated = list_of_scalar_values.ListOfScalarValues(
        page0, 'b', 'seconds', [2, 3])
    b_summary = list_of_scalar_values.ListOfScalarValues(
        None, 'b', 'seconds', [2, 3])

    self.assertEquals(2, len(values))
    self.assertIn(page0_aggregated, values)
    self.assertIn(b_summary, values)

  def testListValue(self):
    page0 = self.pages[0]
    page1 = self.pages[1]

    results = page_test_results.PageTestResults()

    results.WillRunPage(page0)
    v0 = list_of_scalar_values.ListOfScalarValues(page0, 'b', 'seconds', [2, 2])
    results.AddValue(v0)
    results.DidRunPage(page0)

    results.WillRunPage(page1)
    v1 = list_of_scalar_values.ListOfScalarValues(page1, 'b', 'seconds', [3, 3])
    results.AddValue(v1)
    results.DidRunPage(page1)

    summary = summary_module.Summary(results.all_page_specific_values)
    values = summary.interleaved_computed_per_page_values_and_summaries

    b_summary = list_of_scalar_values.ListOfScalarValues(
        None, 'b', 'seconds', [2, 2, 3, 3])

    self.assertEquals(3, len(values))
    self.assertIn(v0, values)
    self.assertIn(v1, values)
    self.assertIn(b_summary, values)

  def testHistogram(self):
    page0 = self.pages[0]
    page1 = self.pages[1]

    results = page_test_results.PageTestResults()
    results.WillRunPage(page0)
    v0 = histogram.HistogramValue(
        page0, 'a', 'units',
        raw_value_json='{"buckets": [{"low": 1, "high": 2, "count": 1}]}',
        important=False)
    results.AddValue(v0)
    results.DidRunPage(page0)

    results.WillRunPage(page1)
    v1 = histogram.HistogramValue(
        page1, 'a', 'units',
        raw_value_json='{"buckets": [{"low": 2, "high": 3, "count": 1}]}',
        important=False)
    results.AddValue(v1)
    results.DidRunPage(page1)

    summary = summary_module.Summary(results.all_page_specific_values)
    values = summary.interleaved_computed_per_page_values_and_summaries

    self.assertEquals(2, len(values))
    self.assertIn(v0, values)
    self.assertIn(v1, values)

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import json
import os
import StringIO
import unittest

from telemetry import story
from telemetry.internal.results import json_3_output_formatter
from telemetry.internal.results import page_test_results
from telemetry import page as page_module
from telemetry.value import failure
from telemetry.value import improvement_direction
from telemetry.value import scalar
from telemetry.value import skip


def _MakeStorySet():
  story_set = story.StorySet(base_dir=os.path.dirname(__file__))
  story_set.AddStory(
      page_module.Page('http://www.foo.com/', story_set, story_set.base_dir,
                       name='Foo'))
  story_set.AddStory(
      page_module.Page('http://www.bar.com/', story_set, story_set.base_dir,
                       name='Bar'))
  story_set.AddStory(
      page_module.Page('http://www.baz.com/', story_set, story_set.base_dir,
                       name='Baz',
                       grouping_keys={'case': 'test', 'type': 'key'}))
  return story_set


def _HasBenchmark(tests_dict, benchmark_name):
  return tests_dict.get(benchmark_name, None) != None


def _HasStory(benchmark_dict, story_name):
  return benchmark_dict.get(story_name) != None


class Json3OutputFormatterTest(unittest.TestCase):
  def setUp(self):
    self._output = StringIO.StringIO()
    self._story_set = _MakeStorySet()
    self._formatter = json_3_output_formatter.JsonOutputFormatter(
        self._output)

  def testOutputAndParse(self):
    results = page_test_results.PageTestResults()
    self._output.truncate(0)

    results.WillRunPage(self._story_set[0])
    v0 = scalar.ScalarValue(results.current_page, 'foo', 'seconds', 3,
                            improvement_direction=improvement_direction.DOWN)
    results.AddValue(v0)
    results.DidRunPage(self._story_set[0])

    self._formatter.Format(results)
    json.loads(self._output.getvalue())

  def testAsDictBaseKeys(self):
    results = page_test_results.PageTestResults()
    d = json_3_output_formatter.ResultsAsDict(results)

    self.assertEquals(d['interrupted'], False)
    self.assertEquals(d['num_failures_by_type'], {})
    self.assertEquals(d['path_delimiter'], '/')
    self.assertEquals(d['seconds_since_epoch'], None)
    self.assertEquals(d['tests'], {})
    self.assertEquals(d['version'], 3)

  def testAsDictWithOnePage(self):
    results = page_test_results.PageTestResults()
    results.telemetry_info.benchmark_name = 'benchmark_name'
    results.WillRunPage(self._story_set[0])
    v0 = scalar.ScalarValue(results.current_page, 'foo', 'seconds', 3,
                            improvement_direction=improvement_direction.DOWN)
    results.AddValue(v0)
    results.DidRunPage(self._story_set[0])

    d = json_3_output_formatter.ResultsAsDict(results)

    self.assertTrue(_HasBenchmark(d['tests'], 'benchmark_name'))
    self.assertTrue(_HasStory(d['tests']['benchmark_name'], 'Foo'))
    story_result = d['tests']['benchmark_name']['Foo']
    self.assertEquals(story_result['actual'], 'PASS')
    self.assertEquals(story_result['expected'], 'PASS')
    self.assertEquals(d['num_failures_by_type'], {'PASS': 1})

  def testAsDictWithTwoPages(self):
    results = page_test_results.PageTestResults()
    results.telemetry_info.benchmark_name = 'benchmark_name'
    results.WillRunPage(self._story_set[0])
    v0 = scalar.ScalarValue(results.current_page, 'foo', 'seconds', 3,
                            improvement_direction=improvement_direction.DOWN)
    results.AddValue(v0)
    results.DidRunPage(self._story_set[0])

    results.WillRunPage(self._story_set[1])
    v1 = scalar.ScalarValue(results.current_page, 'bar', 'seconds', 4,
                            improvement_direction=improvement_direction.DOWN)
    results.AddValue(v1)
    results.DidRunPage(self._story_set[1])

    d = json_3_output_formatter.ResultsAsDict(results)

    self.assertTrue(_HasBenchmark(d['tests'], 'benchmark_name'))
    self.assertTrue(_HasStory(d['tests']['benchmark_name'], 'Foo'))
    story_result = d['tests']['benchmark_name']['Foo']
    self.assertEquals(story_result['actual'], 'PASS')
    self.assertEquals(story_result['expected'], 'PASS')

    self.assertTrue(_HasBenchmark(d['tests'], 'benchmark_name'))
    self.assertTrue(_HasStory(d['tests']['benchmark_name'], 'Bar'))
    story_result = d['tests']['benchmark_name']['Bar']
    self.assertEquals(story_result['actual'], 'PASS')
    self.assertEquals(story_result['expected'], 'PASS')

    self.assertEquals(d['num_failures_by_type'], {'PASS': 2})

  def testAsDictWithRepeatedTests(self):
    results = page_test_results.PageTestResults()
    results.telemetry_info.benchmark_name = 'benchmark_name'

    results.WillRunPage(self._story_set[0])
    v0 = scalar.ScalarValue(results.current_page, 'foo', 'seconds', 3,
                            improvement_direction=improvement_direction.DOWN)
    results.AddValue(v0)
    results.DidRunPage(self._story_set[0])

    results.WillRunPage(self._story_set[1])
    v1 = scalar.ScalarValue(results.current_page, 'bar', 'seconds', 4,
                            improvement_direction=improvement_direction.DOWN)
    results.AddValue(v1)
    results.DidRunPage(self._story_set[1])

    results.WillRunPage(self._story_set[0])
    v0 = scalar.ScalarValue(results.current_page, 'foo', 'seconds', 3,
                            improvement_direction=improvement_direction.DOWN)
    results.AddValue(v0)
    results.DidRunPage(self._story_set[0])

    results.WillRunPage(self._story_set[1])
    v1 = scalar.ScalarValue(results.current_page, 'bar', 'seconds', 4,
                            improvement_direction=improvement_direction.DOWN)
    results.AddValue(v1)
    results.DidRunPage(self._story_set[1])

    d = json_3_output_formatter.ResultsAsDict(results)
    foo_story_result = d['tests']['benchmark_name']['Foo']
    self.assertEquals(foo_story_result['actual'], 'PASS PASS')
    self.assertEquals(foo_story_result['expected'], 'PASS')

    bar_story_result = d['tests']['benchmark_name']['Bar']
    self.assertEquals(bar_story_result['actual'], 'PASS PASS')
    self.assertEquals(bar_story_result['expected'], 'PASS')

    self.assertEquals(d['num_failures_by_type'], {'PASS': 4})

  def testAsDictWithSkippedAndFailedTests(self):
    results = page_test_results.PageTestResults()
    results.telemetry_info.benchmark_name = 'benchmark_name'

    results.WillRunPage(self._story_set[0])
    v0 = scalar.ScalarValue(results.current_page, 'foo', 'seconds', 3,
                            improvement_direction=improvement_direction.DOWN)
    results.AddValue(v0)
    results.DidRunPage(self._story_set[0])

    results.WillRunPage(self._story_set[1])
    v1 = scalar.ScalarValue(results.current_page, 'bar', 'seconds', 4,
                            improvement_direction=improvement_direction.DOWN)
    results.AddValue(v1)
    results.DidRunPage(self._story_set[1])

    results.WillRunPage(self._story_set[0])
    v0 = skip.SkipValue(results.current_page, 'fake_skip')
    results.AddValue(v0)
    results.DidRunPage(self._story_set[0])

    results.WillRunPage(self._story_set[1])
    v1 = failure.FailureValue.FromMessage(results.current_page, 'fake_failure')
    results.AddValue(v1)
    results.DidRunPage(self._story_set[1])

    d = json_3_output_formatter.ResultsAsDict(results)

    foo_story_result = d['tests']['benchmark_name']['Foo']
    self.assertEquals(foo_story_result['actual'], 'PASS SKIP')
    self.assertEquals(foo_story_result['expected'], 'PASS SKIP')

    bar_story_result = d['tests']['benchmark_name']['Bar']
    self.assertEquals(bar_story_result['actual'], 'PASS FAIL')
    self.assertEquals(bar_story_result['expected'], 'PASS')

    self.assertEquals(
        d['num_failures_by_type'], {'PASS': 2, 'FAIL': 1, 'SKIP': 1})

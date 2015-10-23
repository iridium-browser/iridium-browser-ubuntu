# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import StringIO
import sys
import unittest

from catapult_base import cloud_storage
from telemetry import benchmark
from telemetry.core import exceptions
from telemetry.core import util
from telemetry import decorators
from telemetry.internal.results import results_options
from telemetry.internal import story_runner
from telemetry.internal.util import exception_formatter as ex_formatter_module
from telemetry.page import page as page_module
from telemetry.page import page_test
from telemetry import story as story_module
from telemetry.testing import options_for_unittests
from telemetry.testing import system_stub
import mock
from telemetry.value import list_of_scalar_values
from telemetry.value import scalar
from telemetry.value import summary as summary_module
from telemetry.web_perf import story_test
from telemetry.web_perf import timeline_based_measurement
from telemetry.wpr import archive_info

# This linter complains if we define classes nested inside functions.
# pylint: disable=bad-super-call


class FakePlatform(object):
  def CanMonitorThermalThrottling(self):
    return False


class TestSharedState(story_module.SharedState):

  _platform = FakePlatform()

  @classmethod
  def SetTestPlatform(cls, platform):
    cls._platform = platform

  def __init__(self, test, options, story_set):
    super(TestSharedState, self).__init__(
        test, options, story_set)
    self._test = test
    self._current_story = None

  @property
  def platform(self):
    return self._platform

  def WillRunStory(self, story):
    self._current_story = story

  def CanRunStory(self, story):
    return True

  def RunStory(self, results):
    raise NotImplementedError

  def DidRunStory(self, results):
    pass

  def TearDownState(self):
    pass


class TestSharedPageState(TestSharedState):
  def RunStory(self, results):
    self._test.RunPage(self._current_story, None, results)


class FooStoryState(TestSharedPageState):
  pass


class BarStoryState(TestSharedPageState):
  pass


class DummyTest(page_test.PageTest):
  def RunPage(self, *_):
    pass

  def ValidateAndMeasurePage(self, page, tab, results):
    pass


class EmptyMetadataForTest(benchmark.BenchmarkMetadata):
  def __init__(self):
    super(EmptyMetadataForTest, self).__init__('')


class DummyLocalStory(story_module.Story):
  def __init__(self, shared_state_class, name=''):
    super(DummyLocalStory, self).__init__(
        shared_state_class, name=name)

  @property
  def is_local(self):
    return True

class MixedStateStorySet(story_module.StorySet):
  @property
  def allow_mixed_story_states(self):
    return True

def SetupStorySet(allow_multiple_story_states, story_state_list):
  if allow_multiple_story_states:
    story_set = MixedStateStorySet()
  else:
    story_set = story_module.StorySet()
  for story_state in story_state_list:
    story_set.AddStory(DummyLocalStory(story_state))
  return story_set

def _GetOptionForUnittest():
  options = options_for_unittests.GetCopy()
  options.output_formats = ['none']
  options.suppress_gtest_report = False
  parser = options.CreateParser()
  story_runner.AddCommandLineArgs(parser)
  options.MergeDefaultValues(parser.get_default_values())
  story_runner.ProcessCommandLineArgs(parser, options)
  return options


class FakeExceptionFormatterModule(object):
  @staticmethod
  def PrintFormattedException(
      exception_class=None, exception=None, tb=None, msg=None):
    pass


def GetNumberOfSuccessfulPageRuns(results):
  return len([run for run in results.all_page_runs if run.ok or run.skipped])


class StoryRunnerTest(unittest.TestCase):

  def setUp(self):
    self.fake_stdout = StringIO.StringIO()
    self.actual_stdout = sys.stdout
    sys.stdout = self.fake_stdout
    self.options = _GetOptionForUnittest()
    self.results = results_options.CreateResults(
        EmptyMetadataForTest(), self.options)
    self._story_runner_logging_stub = None

  def SuppressExceptionFormatting(self):
    """Fake out exception formatter to avoid spamming the unittest stdout."""
    story_runner.exception_formatter = FakeExceptionFormatterModule
    self._story_runner_logging_stub = system_stub.Override(
      story_runner, ['logging'])

  def RestoreExceptionFormatter(self):
    story_runner.exception_formatter = ex_formatter_module
    if self._story_runner_logging_stub:
      self._story_runner_logging_stub.Restore()
      self._story_runner_logging_stub = None

  def tearDown(self):
    sys.stdout = self.actual_stdout
    self.RestoreExceptionFormatter()

  def testStoriesGroupedByStateClass(self):
    foo_states = [FooStoryState, FooStoryState, FooStoryState,
                  FooStoryState, FooStoryState]
    mixed_states = [FooStoryState, FooStoryState, FooStoryState,
                    BarStoryState, FooStoryState]
    # StorySet's are only allowed to have one SharedState.
    story_set = SetupStorySet(False, foo_states)
    story_groups = (
        story_runner.StoriesGroupedByStateClass(
            story_set, False))
    self.assertEqual(len(story_groups), 1)
    story_set = SetupStorySet(False, mixed_states)
    self.assertRaises(
        ValueError,
        story_runner.StoriesGroupedByStateClass,
        story_set, False)
    # BaseStorySets are allowed to have multiple SharedStates.
    mixed_story_set = SetupStorySet(True, mixed_states)
    story_groups = (
        story_runner.StoriesGroupedByStateClass(
            mixed_story_set, True))
    self.assertEqual(len(story_groups), 3)
    self.assertEqual(story_groups[0].shared_state_class,
                     FooStoryState)
    self.assertEqual(story_groups[1].shared_state_class,
                     BarStoryState)
    self.assertEqual(story_groups[2].shared_state_class,
                     FooStoryState)

  def RunStoryTest(self, s, expected_successes):
    test = DummyTest()
    story_runner.Run(
        test, s, self.options, self.results)
    self.assertEquals(0, len(self.results.failures))
    self.assertEquals(expected_successes,
                      GetNumberOfSuccessfulPageRuns(self.results))

  def testStoryTest(self):
    all_foo = [FooStoryState, FooStoryState, FooStoryState]
    one_bar = [FooStoryState, FooStoryState, BarStoryState]
    story_set = SetupStorySet(True, one_bar)
    self.RunStoryTest(story_set, 3)
    story_set = SetupStorySet(True, all_foo)
    self.RunStoryTest(story_set, 6)
    story_set = SetupStorySet(False, all_foo)
    self.RunStoryTest(story_set, 9)
    story_set = SetupStorySet(False, one_bar)
    test = DummyTest()
    self.assertRaises(ValueError, story_runner.Run, test, story_set,
                      self.options, self.results)

  def testSuccessfulTimelineBasedMeasurementTest(self):
    """Check that PageTest is not required for story_runner.Run.

    Any PageTest related calls or attributes need to only be called
    for PageTest tests.
    """
    class TestSharedTbmState(TestSharedState):
      def RunStory(self, results):
        pass

    test = timeline_based_measurement.TimelineBasedMeasurement(
        timeline_based_measurement.Options())
    story_set = story_module.StorySet()
    story_set.AddStory(DummyLocalStory(TestSharedTbmState))
    story_set.AddStory(DummyLocalStory(TestSharedTbmState))
    story_set.AddStory(DummyLocalStory(TestSharedTbmState))
    story_runner.Run(
        test, story_set, self.options, self.results)
    self.assertEquals(0, len(self.results.failures))
    self.assertEquals(3, GetNumberOfSuccessfulPageRuns(self.results))

  def testCallOrderBetweenStoryTestAndSharedState(self):
    """Check that the call order between StoryTest and SharedState is correct.
    """
    TEST_WILL_RUN_STORY = 'test.WillRunStory'
    TEST_MEASURE = 'test.Measure'
    TEST_DID_RUN_STORY = 'test.DidRunStory'
    STATE_WILL_RUN_STORY = 'state.WillRunStory'
    STATE_RUN_STORY = 'state.RunStory'
    STATE_DID_RUN_STORY = 'state.DidRunStory'

    EXPECTED_CALLS_IN_ORDER = [TEST_WILL_RUN_STORY,
                               STATE_WILL_RUN_STORY,
                               STATE_RUN_STORY,
                               TEST_MEASURE,
                               STATE_DID_RUN_STORY,
                               TEST_DID_RUN_STORY]

    class TestStoryTest(story_test.StoryTest):
      def WillRunStory(self, platform):
        pass

      def Measure(self, platform, results):
        pass

      def DidRunStory(self, platform):
        pass

    class TestSharedStateForStoryTest(TestSharedState):
      def RunStory(self, results):
        pass

    @mock.patch.object(TestStoryTest, 'WillRunStory')
    @mock.patch.object(TestStoryTest, 'Measure')
    @mock.patch.object(TestStoryTest, 'DidRunStory')
    @mock.patch.object(TestSharedStateForStoryTest, 'WillRunStory')
    @mock.patch.object(TestSharedStateForStoryTest, 'RunStory')
    @mock.patch.object(TestSharedStateForStoryTest, 'DidRunStory')
    def GetCallsInOrder(state_DidRunStory, state_RunStory, state_WillRunStory,
                        test_DidRunStory, test_Measure, test_WillRunStory):
      manager = mock.MagicMock()
      manager.attach_mock(test_WillRunStory, TEST_WILL_RUN_STORY)
      manager.attach_mock(test_Measure, TEST_MEASURE)
      manager.attach_mock(test_DidRunStory, TEST_DID_RUN_STORY)
      manager.attach_mock(state_WillRunStory, STATE_WILL_RUN_STORY)
      manager.attach_mock(state_RunStory, STATE_RUN_STORY)
      manager.attach_mock(state_DidRunStory, STATE_DID_RUN_STORY)

      test = TestStoryTest()
      story_set = story_module.StorySet()
      story_set.AddStory(DummyLocalStory(TestSharedStateForStoryTest))
      story_runner.Run(test, story_set, self.options, self.results)
      return [call[0] for call in manager.mock_calls]

    calls_in_order = GetCallsInOrder() # pylint: disable=no-value-for-parameter
    self.assertEquals(EXPECTED_CALLS_IN_ORDER, calls_in_order)

  def testTearDownIsCalledOnceForEachStoryGroupWithPageSetRepeat(self):
    self.options.pageset_repeat = 3
    fooz_init_call_counter = [0]
    fooz_tear_down_call_counter = [0]
    barz_init_call_counter = [0]
    barz_tear_down_call_counter = [0]
    class FoozStoryState(FooStoryState):
      def __init__(self, test, options, storyz):
        super(FoozStoryState, self).__init__(
          test, options, storyz)
        fooz_init_call_counter[0] += 1
      def TearDownState(self):
        fooz_tear_down_call_counter[0] += 1

    class BarzStoryState(BarStoryState):
      def __init__(self, test, options, storyz):
        super(BarzStoryState, self).__init__(
          test, options, storyz)
        barz_init_call_counter[0] += 1
      def TearDownState(self):
        barz_tear_down_call_counter[0] += 1
    def AssertAndCleanUpFoo():
      self.assertEquals(1, fooz_init_call_counter[0])
      self.assertEquals(1, fooz_tear_down_call_counter[0])
      fooz_init_call_counter[0] = 0
      fooz_tear_down_call_counter[0] = 0

    story_set1_list = [FoozStoryState, FoozStoryState, FoozStoryState,
                       BarzStoryState, BarzStoryState]
    story_set1 = SetupStorySet(True, story_set1_list)
    self.RunStoryTest(story_set1, 15)
    AssertAndCleanUpFoo()
    self.assertEquals(1, barz_init_call_counter[0])
    self.assertEquals(1, barz_tear_down_call_counter[0])
    barz_init_call_counter[0] = 0
    barz_tear_down_call_counter[0] = 0

    story_set2_list = [FoozStoryState, FoozStoryState, FoozStoryState,
                       FoozStoryState]
    story_set2 = SetupStorySet(False, story_set2_list)
    self.RunStoryTest(story_set2, 27)
    AssertAndCleanUpFoo()
    self.assertEquals(0, barz_init_call_counter[0])
    self.assertEquals(0, barz_tear_down_call_counter[0])

  def testAppCrashExceptionCausesFailureValue(self):
    self.SuppressExceptionFormatting()
    story_set = story_module.StorySet()
    class SharedStoryThatCausesAppCrash(TestSharedPageState):
      def WillRunStory(self, story):
        raise exceptions.AppCrashException('App Foo crashes')

    story_set.AddStory(DummyLocalStory(
          SharedStoryThatCausesAppCrash))
    story_runner.Run(
        DummyTest(), story_set, self.options, self.results)
    self.assertEquals(1, len(self.results.failures))
    self.assertEquals(0, GetNumberOfSuccessfulPageRuns(self.results))
    self.assertIn('App Foo crashes', self.fake_stdout.getvalue())

  def testUnknownExceptionIsFatal(self):
    self.SuppressExceptionFormatting()
    story_set = story_module.StorySet()

    class UnknownException(Exception):
      pass

    # This erroneous test is set up to raise exception for the 2nd story
    # run.
    class Test(page_test.PageTest):
      def __init__(self, *args):
        super(Test, self).__init__(*args)
        self.run_count = 0

      def RunPage(self, *_):
        old_run_count = self.run_count
        self.run_count += 1
        if old_run_count == 1:
          raise UnknownException('FooBarzException')

      def ValidateAndMeasurePage(self, page, tab, results):
        pass

    s1 = DummyLocalStory(TestSharedPageState)
    s2 = DummyLocalStory(TestSharedPageState)
    story_set.AddStory(s1)
    story_set.AddStory(s2)
    test = Test()
    with self.assertRaises(UnknownException):
      story_runner.Run(
          test, story_set, self.options, self.results)
    self.assertEqual(set([s2]), self.results.pages_that_failed)
    self.assertEqual(set([s1]), self.results.pages_that_succeeded)
    self.assertIn('FooBarzException', self.fake_stdout.getvalue())

  def testRaiseBrowserGoneExceptionFromRunPage(self):
    self.SuppressExceptionFormatting()
    story_set = story_module.StorySet()

    class Test(page_test.PageTest):
      def __init__(self, *args):
        super(Test, self).__init__(*args)
        self.run_count = 0

      def RunPage(self, *_):
        old_run_count = self.run_count
        self.run_count += 1
        if old_run_count == 0:
          raise exceptions.BrowserGoneException('i am a browser instance')

      def ValidateAndMeasurePage(self, page, tab, results):
        pass

    story_set.AddStory(DummyLocalStory(TestSharedPageState))
    story_set.AddStory(DummyLocalStory(TestSharedPageState))
    test = Test()
    story_runner.Run(
        test, story_set, self.options, self.results)
    self.assertEquals(2, test.run_count)
    self.assertEquals(1, len(self.results.failures))
    self.assertEquals(1, GetNumberOfSuccessfulPageRuns(self.results))

  def testAppCrashThenRaiseInTearDownFatal(self):
    self.SuppressExceptionFormatting()
    story_set = story_module.StorySet()

    unit_test_events = []  # track what was called when
    class DidRunTestError(Exception):
      pass

    class TestTearDownSharedState(TestSharedPageState):
      def TearDownState(self):
        unit_test_events.append('tear-down-state')
        raise DidRunTestError


    class Test(page_test.PageTest):
      def __init__(self, *args):
        super(Test, self).__init__(*args)
        self.run_count = 0

      def RunPage(self, *_):
        old_run_count = self.run_count
        self.run_count += 1
        if old_run_count == 0:
          unit_test_events.append('app-crash')
          raise exceptions.AppCrashException

      def ValidateAndMeasurePage(self, page, tab, results):
        pass

    story_set.AddStory(DummyLocalStory(TestTearDownSharedState))
    story_set.AddStory(DummyLocalStory(TestTearDownSharedState))
    test = Test()

    with self.assertRaises(DidRunTestError):
      story_runner.Run(
          test, story_set, self.options, self.results)
    self.assertEqual(['app-crash', 'tear-down-state'], unit_test_events)
    # The AppCrashException gets added as a failure.
    self.assertEquals(1, len(self.results.failures))

  def testPagesetRepeat(self):
    story_set = story_module.StorySet()

    # TODO(eakuefner): Factor this out after flattening page ref in Value
    blank_story = DummyLocalStory(TestSharedPageState, name='blank')
    green_story = DummyLocalStory(TestSharedPageState, name='green')
    story_set.AddStory(blank_story)
    story_set.AddStory(green_story)

    class Measurement(page_test.PageTest):
      i = 0
      def RunPage(self, page, _, results):
        self.i += 1
        results.AddValue(scalar.ScalarValue(
            page, 'metric', 'unit', self.i))

      def ValidateAndMeasurePage(self, page, tab, results):
        pass

    self.options.page_repeat = 1
    self.options.pageset_repeat = 2
    self.options.output_formats = []
    results = results_options.CreateResults(
      EmptyMetadataForTest(), self.options)
    story_runner.Run(
        Measurement(), story_set, self.options, results)
    summary = summary_module.Summary(results.all_page_specific_values)
    values = summary.interleaved_computed_per_page_values_and_summaries

    blank_value = list_of_scalar_values.ListOfScalarValues(
        blank_story, 'metric', 'unit', [1, 3])
    green_value = list_of_scalar_values.ListOfScalarValues(
        green_story, 'metric', 'unit', [2, 4])
    merged_value = list_of_scalar_values.ListOfScalarValues(
        None, 'metric', 'unit', [1, 2, 3, 4])

    self.assertEquals(4, GetNumberOfSuccessfulPageRuns(results))
    self.assertEquals(0, len(results.failures))
    self.assertEquals(3, len(values))
    self.assertIn(blank_value, values)
    self.assertIn(green_value, values)
    self.assertIn(merged_value, values)

  @decorators.Disabled('chromeos')  # crbug.com/483212
  def testUpdateAndCheckArchives(self):
    usr_stub = system_stub.Override(story_runner, ['cloud_storage'])
    wpr_stub = system_stub.Override(archive_info, ['cloud_storage'])
    archive_data_dir = os.path.join(
        util.GetTelemetryDir(),
        'telemetry', 'internal', 'testing', 'archive_files')
    try:
      story_set = story_module.StorySet()
      story_set.AddStory(page_module.Page(
          'http://www.testurl.com', story_set, story_set.base_dir))
      # Page set missing archive_data_file.
      self.assertRaises(
          story_runner.ArchiveError,
          story_runner._UpdateAndCheckArchives,
          story_set.archive_data_file,
          story_set.wpr_archive_info,
          story_set.stories)

      story_set = story_module.StorySet(
          archive_data_file='missing_archive_data_file.json')
      story_set.AddStory(page_module.Page(
          'http://www.testurl.com', story_set, story_set.base_dir))
      # Page set missing json file specified in archive_data_file.
      self.assertRaises(
          story_runner.ArchiveError,
          story_runner._UpdateAndCheckArchives,
          story_set.archive_data_file,
          story_set.wpr_archive_info,
          story_set.stories)

      story_set = story_module.StorySet(
          archive_data_file=os.path.join(archive_data_dir, 'test.json'),
          cloud_storage_bucket=cloud_storage.PUBLIC_BUCKET)
      story_set.AddStory(page_module.Page(
          'http://www.testurl.com', story_set, story_set.base_dir))
      # Page set with valid archive_data_file.
      self.assertTrue(story_runner._UpdateAndCheckArchives(
            story_set.archive_data_file, story_set.wpr_archive_info,
            story_set.stories))
      story_set.AddStory(page_module.Page(
          'http://www.google.com', story_set, story_set.base_dir))
      # Page set with an archive_data_file which exists but is missing a page.
      self.assertRaises(
          story_runner.ArchiveError,
          story_runner._UpdateAndCheckArchives,
          story_set.archive_data_file,
          story_set.wpr_archive_info,
          story_set.stories)

      story_set = story_module.StorySet(
          archive_data_file=
              os.path.join(archive_data_dir, 'test_missing_wpr_file.json'),
          cloud_storage_bucket=cloud_storage.PUBLIC_BUCKET)
      story_set.AddStory(page_module.Page(
          'http://www.testurl.com', story_set, story_set.base_dir))
      story_set.AddStory(page_module.Page(
          'http://www.google.com', story_set, story_set.base_dir))
      # Page set with an archive_data_file which exists and contains all pages
      # but fails to find a wpr file.
      self.assertRaises(
          story_runner.ArchiveError,
          story_runner._UpdateAndCheckArchives,
          story_set.archive_data_file,
          story_set.wpr_archive_info,
          story_set.stories)
    finally:
      usr_stub.Restore()
      wpr_stub.Restore()


  def _testMaxFailuresOptionIsRespectedAndOverridable(
      self, num_failing_stories, runner_max_failures, options_max_failures,
      expected_num_failures):
    class SimpleSharedState(story_module.SharedState):
      _fake_platform = FakePlatform()
      _current_story = None

      @property
      def platform(self):
        return self._fake_platform

      def WillRunStory(self, story):
        self._current_story = story

      def RunStory(self, results):
        self._current_story.Run()

      def DidRunStory(self, results):
        pass

      def CanRunStory(self, story):
        return True

      def TearDownState(self):
        pass

    class FailingStory(story_module.Story):
      def __init__(self):
        super(FailingStory, self).__init__(
            shared_state_class=SimpleSharedState,
            is_local=True)
        self.was_run = False

      def Run(self):
        self.was_run = True
        raise page_test.Failure

    self.SuppressExceptionFormatting()

    story_set = story_module.StorySet()
    for _ in range(num_failing_stories):
      story_set.AddStory(FailingStory())

    options = _GetOptionForUnittest()
    options.output_formats = ['none']
    options.suppress_gtest_report = True
    if options_max_failures:
      options.max_failures = options_max_failures

    results = results_options.CreateResults(EmptyMetadataForTest(), options)
    story_runner.Run(
        DummyTest(), story_set, options,
        results, max_failures=runner_max_failures)
    self.assertEquals(0, GetNumberOfSuccessfulPageRuns(results))
    self.assertEquals(expected_num_failures, len(results.failures))
    for ii, story in enumerate(story_set.stories):
      self.assertEqual(story.was_run, ii < expected_num_failures)

  def testMaxFailuresNotSpecified(self):
    self._testMaxFailuresOptionIsRespectedAndOverridable(
        num_failing_stories=5, runner_max_failures=None,
        options_max_failures=None, expected_num_failures=5)

  def testMaxFailuresSpecifiedToRun(self):
    # Runs up to max_failures+1 failing tests before stopping, since
    # every tests after max_failures failures have been encountered
    # may all be passing.
    self._testMaxFailuresOptionIsRespectedAndOverridable(
        num_failing_stories=5, runner_max_failures=3,
        options_max_failures=None, expected_num_failures=4)

  def testMaxFailuresOption(self):
    # Runs up to max_failures+1 failing tests before stopping, since
    # every tests after max_failures failures have been encountered
    # may all be passing.
    self._testMaxFailuresOptionIsRespectedAndOverridable(
        num_failing_stories=5, runner_max_failures=3,
        options_max_failures=1, expected_num_failures=2)

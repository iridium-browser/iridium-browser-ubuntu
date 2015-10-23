# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from pylib import valgrind_tools
from pylib.base import base_test_result
from pylib.base import test_run
from pylib.base import test_collection
from pylib.device import device_errors


class LocalDeviceTestRun(test_run.TestRun):

  def __init__(self, env, test_instance):
    super(LocalDeviceTestRun, self).__init__(env, test_instance)
    self._tools = {}

  #override
  def RunTests(self):
    tests = self._GetTests()

    def run_tests_on_device(dev, tests, results):
      for test in tests:
        try:
          result = self._RunTest(dev, test)
          if isinstance(result, base_test_result.BaseTestResult):
            results.AddResult(result)
          elif isinstance(result, list):
            results.AddResults(result)
          else:
            raise Exception(
                'Unexpected result type: %s' % type(result).__name__)
        except:
          if isinstance(tests, test_collection.TestCollection):
            tests.add(test)
          raise
        finally:
          if isinstance(tests, test_collection.TestCollection):
            tests.test_completed()
      logging.info('Finished running tests on this device.')

    tries = 0
    results = base_test_result.TestRunResults()
    all_fail_results = {}
    while tries < self._env.max_tries and tests:
      logging.info('STARTING TRY #%d/%d', tries + 1, self._env.max_tries)
      logging.info('Will run %d tests on %d devices: %s',
                   len(tests), len(self._env.devices),
                   ', '.join(str(d) for d in self._env.devices))
      for t in tests:
        logging.debug('  %s', t)

      try:
        try_results = base_test_result.TestRunResults()
        if self._ShouldShard():
          tc = test_collection.TestCollection(self._CreateShards(tests))
          self._env.parallel_devices.pMap(
              run_tests_on_device, tc, try_results).pGet(None)
        else:
          self._env.parallel_devices.pMap(
              run_tests_on_device, tests, try_results).pGet(None)
      except device_errors.CommandFailedError:
        logging.exception('Shard terminated: command failed')
      except device_errors.CommandTimeoutError:
        logging.exception('Shard terminated: command timed out')
      except device_errors.DeviceUnreachableError:
        logging.exception('Shard terminated: device became unreachable')

      for result in try_results.GetAll():
        if result.GetType() in (base_test_result.ResultType.PASS,
                                base_test_result.ResultType.SKIP):
          results.AddResult(result)
        else:
          all_fail_results[result.GetName()] = result

      results_names = set(r.GetName() for r in results.GetAll())
      tests = [t for t in tests if self._GetTestName(t) not in results_names]
      tries += 1
      logging.info('FINISHED TRY #%d/%d', tries, self._env.max_tries)
      if tests:
        logging.info('%d failed tests remain.', len(tests))
      else:
        logging.info('All tests completed.')

    all_unknown_test_names = set(self._GetTestName(t) for t in tests)
    all_failed_test_names = set(all_fail_results.iterkeys())

    unknown_tests = all_unknown_test_names.difference(all_failed_test_names)
    failed_tests = all_failed_test_names.intersection(all_unknown_test_names)

    if unknown_tests:
      results.AddResults(
          base_test_result.BaseTestResult(
              u, base_test_result.ResultType.UNKNOWN)
          for u in unknown_tests)
    if failed_tests:
      results.AddResults(all_fail_results[f] for f in failed_tests)

    return results

  def GetTool(self, device):
    if not str(device) in self._tools:
      self._tools[str(device)] = valgrind_tools.CreateTool(
          self._env.tool, device)
    return self._tools[str(device)]

  def _CreateShards(self, tests):
    raise NotImplementedError

  def _GetTestName(self, test):
    return test

  def _GetTests(self):
    raise NotImplementedError

  def _RunTest(self, device, test):
    raise NotImplementedError

  def _ShouldShard(self):
    raise NotImplementedError

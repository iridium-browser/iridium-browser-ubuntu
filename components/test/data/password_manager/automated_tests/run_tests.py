# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Encapsulates running tests defined in tests.py.

Running this script requires passing --config-path with a path to a config file
of the following structure:

  [data_files]
  passwords_path=<path to a file with passwords>
  [binaries]
  chrome-path=<chrome binary path>
  chromedriver-path=<chrome driver path>
  [run_options]
  # |tests_in_parallel| is optional, the default value is 1.
  tests_in_parallel=<number of parallel tests>
  # |tests_to_runs| field is optional, if it is absent all tests will be run.
  tests_to_run=<test names to run, comma delimited>

The script uses the Python's logging library to report the test results,
as well as debugging information. It emits three levels of logs (in
descending order of severity):
  logging.INFO: Summary of the tests.
  logging.DEBUG: Details about tests failures.
  SCRIPT_DEBUG (see below): Debug info of this script.
You have to set up appropriate logging handlers to have the logs appear.
"""
import argparse
import ConfigParser
import logging
import os
import shutil
import subprocess
import tempfile
import time

import tests


# Just below logging.DEBUG, use for this script's debug messages instead
# of logging.DEBUG, which is already used for detailed test debug messages.
SCRIPT_DEBUG = 9


class TestRunner(object):
  """Runs tests for a single website."""

  def __init__(self, test_cmd, test_name):
    """Initialize the TestRunner.

    Args:
      test_cmd: List of command line arguments to be supplied to
        every test run.
      test_name: Test name (e.g., facebook).
    """
    self.logger = logging.getLogger("run_tests")

    self.profile_path = tempfile.mkdtemp()
    results = tempfile.NamedTemporaryFile(delete=False)
    self.results_path = results.name
    results.close()
    self.test_cmd = test_cmd + ["--profile-path", self.profile_path,
                                "--save-path", self.results_path]
    self.test_name = test_name
    # TODO(vabr): Ideally we would replace timeout with something allowing
    # calling tests directly inside Python, and working on other platforms.
    #
    # The website test runs multiple scenarios, each one has an internal
    # timeout of 200s for waiting (see |remaining_time_to_wait| and
    # Wait() in websitetest.py). Expecting that not every scenario should
    # take 200s, the maximum time allocated for all of them is 300s.
    self.test_cmd = ["timeout", "300"] + self.test_cmd

    self.logger.log(SCRIPT_DEBUG,
                    "TestRunner set up for test %s, command '%s', "
                    "profile path %s, results file %s",
                    self.test_name, self.test_cmd, self.profile_path,
                    self.results_path)

    self.runner_process = None
    # The tests can be flaky. This is why we try to rerun up to 3 times.
    self.max_test_runs_left = 3
    self.failures = []
    self._run_test()

  def get_test_result(self):
    """Return the test results.

    Returns:
      (True, []) if the test passed.
      (False, list_of_failures) if the test failed.
      None if the test is still running.
    """

    test_running = self.runner_process and self.runner_process.poll() is None
    if test_running:
      return None
    # Test is not running, now we have to check if we want to start it again.
    if self._check_if_test_passed():
      self.logger.log(SCRIPT_DEBUG, "Test %s passed", self.test_name)
      return True, []
    if self.max_test_runs_left == 0:
      self.logger.log(SCRIPT_DEBUG, "Test %s failed", self.test_name)
      return False, self.failures
    self._run_test()
    return None

  def _check_if_test_passed(self):
    """Returns True if and only if the test passed."""

    success = False
    if os.path.isfile(self.results_path):
      with open(self.results_path, "r") as results:
        # TODO(vabr): Parse the results to make sure all scenarios succeeded
        # instead of hard-coding here the number of tests scenarios from
        # test.py:main.
        NUMBER_OF_TEST_SCENARIOS = 3
        passed_scenarios = 0
        for line in results:
          self.failures.append(line)
          passed_scenarios += line.count("successful='True'")
          success = passed_scenarios == NUMBER_OF_TEST_SCENARIOS
          if success:
            break

    self.logger.log(
        SCRIPT_DEBUG,
        "Test run of {0} succeded: {1}".format(self.test_name, success))
    return success

  def _run_test(self):
    """Executes the command to run the test."""
    with open(self.results_path, "w"):
      pass  # Just clear the results file.
    shutil.rmtree(path=self.profile_path, ignore_errors=True)
    self.max_test_runs_left -= 1
    self.logger.log(SCRIPT_DEBUG, "Run of test %s started", self.test_name)
    self.runner_process = subprocess.Popen(self.test_cmd)


def _apply_defaults(config, defaults):
  """Adds default values from |defaults| to |config|.

  Note: This differs from ConfigParser's mechanism for providing defaults in
  two aspects:
    * The "defaults" here become explicit, and are associated with sections.
    * Sections get created for the added defaults where needed, that is, if
      they do not exist before.

  Args:
    config: A ConfigParser instance to be updated
    defaults: A dictionary mapping (section_string, option_string) pairs
      to string values. For every section/option combination not already
      contained in |config|, the value from |defaults| is stored in |config|.
  """
  for (section, option) in defaults:
    if not config.has_section(section):
      config.add_section(section)
    if not config.has_option(section, option):
      config.set(section, option, defaults[(section, option)])


def run_tests(config_path):
  """Runs automated tests.

  Runs the tests and returns the results through logging:
  On logging.INFO logging level, it returns the summary of how many tests
  passed and failed.
  On logging.DEBUG logging level, it returns the failure logs, if any.
  (On SCRIPT_DEBUG it returns diagnostics for this script.)

  Args:
    config_path: The path to the config INI file. See the top of the file
      for format description.
  """

  defaults = {("run_options", "tests_in_parallel"): "1"}
  config = ConfigParser.ConfigParser()
  _apply_defaults(config, defaults)
  config.read(config_path)
  max_tests_in_parallel = config.getint("run_options", "tests_in_parallel")
  full_path = os.path.realpath(__file__)
  tests_dir = os.path.dirname(full_path)
  tests_path = os.path.join(tests_dir, "tests.py")
  test_name_idx = 2  # Index of "test_name_placeholder" below.
  general_test_cmd = ["python", tests_path, "test_name_placeholder",
                      "--chrome-path", config.get("binaries", "chrome-path"),
                      "--chromedriver-path",
                      config.get("binaries", "chromedriver-path"),
                      "--passwords-path",
                      config.get("data_files", "passwords_path")]
  runners = []
  if config.has_option("run_options", "tests_to_run"):
    user_selected_tests = config.get("run_options", "tests_to_run").split(",")
    tests_to_run = user_selected_tests
  else:
    tests_to_run = tests.all_tests.keys()

  logger = logging.getLogger("run_tests")
  logger.log(SCRIPT_DEBUG, "%d tests to run: %s", len(tests_to_run),
             tests_to_run)
  results = []  # List of (name, bool_passed, failure_log).
  while len(runners) + len(tests_to_run) > 0:
    i = 0
    # TODO(melandory): Rewrite with list comprehension to increase readability.
    while i < len(runners):
      result = runners[i].get_test_result()
      if result:  # This test run is finished.
        status, log = result
        results.append((runners[i].test_name, status, log))
        del runners[i]
      else:
        i += 1
    while len(runners) < max_tests_in_parallel and len(tests_to_run):
      test_name = tests_to_run.pop()
      specific_test_cmd = list(general_test_cmd)
      specific_test_cmd[test_name_idx] = test_name
      runners.append(TestRunner(specific_test_cmd, test_name))
    time.sleep(1)
  failed_tests = [(name, log) for (name, passed, log) in results if not passed]
  logger.info("%d failed tests out of %d, failing tests: %s",
              len(failed_tests), len(results),
              [name for (name, _) in failed_tests])
  logger.debug("Logs of failing tests: %s", failed_tests)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("config_path", metavar="N",
                      help="Path to the config.ini file.")
  args = parser.parse_args()
  run_tests(args.config_path)


if __name__ == "__main__":
  main()

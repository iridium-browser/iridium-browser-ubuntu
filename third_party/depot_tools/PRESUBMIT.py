# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Top-level presubmit script for depot tools.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into depot_tools.
"""

import fnmatch
import os


def DepotToolsPylint(input_api, output_api):
  """Gather all the pylint logic into one place to make it self-contained."""
  white_list = [
    r'^[^/]*\.py$',
    r'^testing_support/[^/]*\.py$',
    r'^tests/[^/]*\.py$',
    r'^recipe_modules/.*\.py$',  # Allow recursive search in recipe modules.
  ]
  black_list = list(input_api.DEFAULT_BLACK_LIST)
  if os.path.exists('.gitignore'):
    with open('.gitignore') as fh:
      lines = [l.strip() for l in fh.readlines()]
      black_list.extend([fnmatch.translate(l) for l in lines if
                         l and not l.startswith('#')])
  if os.path.exists('.git/info/exclude'):
    with open('.git/info/exclude') as fh:
      lines = [l.strip() for l in fh.readlines()]
      black_list.extend([fnmatch.translate(l) for l in lines if
                         l and not l.startswith('#')])
  disabled_warnings = [
    'R0401',  # Cyclic import
    'W0613',  # Unused argument
  ]
  return input_api.canned_checks.GetPylint(
      input_api,
      output_api,
      white_list=white_list,
      black_list=black_list,
      disabled_warnings=disabled_warnings)


def CommonChecks(input_api, output_api, tests_to_black_list):
  results = []
  results.extend(input_api.canned_checks.CheckOwners(input_api, output_api))
  # TODO(maruel): Make sure at least one file is modified first.
  # TODO(maruel): If only tests are modified, only run them.
  tests = DepotToolsPylint(input_api, output_api)
  unit_tests = input_api.canned_checks.GetUnitTestsInDirectory(
      input_api,
      output_api,
      'tests',
      whitelist=[r'.*test\.py$'],
      blacklist=tests_to_black_list)
  if not input_api.platform.startswith(('cygwin', 'win32')):
    tests.extend(unit_tests)
  else:
    print('Warning: not running unit tests on Windows')
  results.extend(input_api.RunTests(tests))
  return results


def RunGitClTests(input_api, output_api):
  """Run all the shells scripts in the directory test.
  """
  if input_api.platform == 'win32':
    # Skip for now as long as the test scripts are bash scripts.
    return []

  # First loads a local Rietveld instance.
  import sys
  old_sys_path = sys.path
  try:
    sys.path = [input_api.PresubmitLocalPath()] + sys.path
    from testing_support import local_rietveld
    server = local_rietveld.LocalRietveld()
  finally:
    sys.path = old_sys_path

  results = []
  try:
    # Start a local rietveld instance to test against.
    server.start_server()
    test_path = input_api.os_path.abspath(
        input_api.os_path.join(input_api.PresubmitLocalPath(), 'tests'))

    # test-lib.sh is not an actual test so it should not be run.
    NON_TEST_FILES = ('test-lib.sh')
    for test in input_api.os_listdir(test_path):
      if test in NON_TEST_FILES or not test.endswith('.sh'):
        continue

      print('Running %s' % test)
      try:
        if input_api.verbose:
          input_api.subprocess.check_call(
              [input_api.os_path.join(test_path, test)], cwd=test_path)
        else:
          input_api.subprocess.check_output(
              [input_api.os_path.join(test_path, test)], cwd=test_path,
              stderr=input_api.subprocess.STDOUT)
      except (OSError, input_api.subprocess.CalledProcessError), e:
        results.append(output_api.PresubmitError('%s failed\n%s' % (test, e)))
  except local_rietveld.Failure, e:
    results.append(output_api.PresubmitError('\n'.join(str(i) for i in e.args)))
  finally:
    server.stop_server()
  return results


def CheckChangeOnUpload(input_api, output_api):
  # Do not run integration tests on upload since they are way too slow.
  tests_to_black_list = [
      r'^checkout_test\.py$',
      r'^gclient_smoketest\.py$',
      r'^scm_unittest\.py$',
      r'^subprocess2_test\.py$',
    ]
  return CommonChecks(input_api, output_api, tests_to_black_list)


def CheckChangeOnCommit(input_api, output_api):
  output = []
  output.extend(CommonChecks(input_api, output_api, []))
  output.extend(input_api.canned_checks.CheckDoNotSubmit(
      input_api,
      output_api))
  output.extend(RunGitClTests(input_api, output_api))
  return output

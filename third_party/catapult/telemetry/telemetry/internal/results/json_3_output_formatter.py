# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import json

from telemetry.internal.results import output_formatter


def _mk_dict(d, *args):
  for key in args:
    if key not in d:
      d[key] = {}
    d = d[key]
  return d


def ResultsAsDict(page_test_results):
  """Takes PageTestResults to a dict in the JSON test results format.

  To serialize results as JSON we first convert them to a dict that can be
  serialized by the json module.

  See: https://www.chromium.org/developers/the-json-test-results-format

  Args:
    page_test_results: a PageTestResults object
  """
  telemetry_info = page_test_results.telemetry_info
  result_dict = {
    'interrupted': telemetry_info.benchmark_interrupted,
    'path_delimiter': '/',
    'version': 3,
    'seconds_since_epoch': telemetry_info.benchmark_start_ms,
    'tests': {},
  }
  status_counter = collections.Counter()
  for run in page_test_results.all_page_runs:
    expected = 'PASS'
    if run.skipped:
      status = expected = 'SKIP'
    elif run.failed:
      status = 'FAIL'
    else:
      status = 'PASS'
    status_counter[status] += 1

    test = _mk_dict(
        result_dict, 'tests', telemetry_info.benchmark_name,
        run.story.name)
    if 'actual' not in test:
      test['actual'] = status
    else:
      test['actual'] += (' ' + status)

    if 'expected' not in test:
      test['expected'] = expected
    else:
      if expected not in test['expected']:
        test['expected'] += (' ' + expected)

  result_dict['num_failures_by_type'] = dict(status_counter)
  return result_dict


class JsonOutputFormatter(output_formatter.OutputFormatter):
  def __init__(self, output_stream):
    super(JsonOutputFormatter, self).__init__(output_stream)

  def Format(self, page_test_results):
    """Serialize page test results in JSON Test Results format.

    See: https://www.chromium.org/developers/the-json-test-results-format
    """
    json.dump(ResultsAsDict(page_test_results),
        self.output_stream, indent=2, sort_keys=True, separators=(',', ': '))
    self.output_stream.write('\n')

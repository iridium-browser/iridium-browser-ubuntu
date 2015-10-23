# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions for making lists of tests, and an AJAX endpoint to list tests.

This module contains functions for listing:
 - Sub-tests for a given test suite (in a tree structure).
 - Tests which match a given test path pattern.
"""

import json

from google.appengine.ext import ndb

from dashboard import layered_cache
from dashboard import request_handler
from dashboard import utils
from dashboard.models import graph_data


class ListTestsHandler(request_handler.RequestHandler):
  """URL endpoint for AJAX requests to list masters, bots, and tests."""

  def post(self):
    """Outputs a JSON string of the requested list.

    Request parameters:
      type: Type of list to make, one of "suite", "sub_tests" or "pattern".
      suite: Test suite name (applies only if type is "sub_tests").
      bots: Comma-separated bots name (applies only if type is "sub_tests").
      p: Test path pattern (applies only if type is "pattern").
      has_rows: "1" if the requester wants to list only list tests that
          have points (applies only if type is "pattern").

    Outputs:
      A data structure with test names in JSON format, or nothing.
    """
    self.response.headers.add_header('Access-Control-Allow-Origin', '*')
    list_type = self.request.get('type')
    # TODO(qyearsley): Separate these into two different handlers.

    if list_type == 'sub_tests':
      suite_name = self.request.get('suite')
      bot_names = self.request.get('bots').split(',')
      test_list = GetSubTests(suite_name, bot_names)
      self.response.out.write(json.dumps(test_list))

    if list_type == 'pattern':
      pattern = self.request.get('p')
      only_with_rows = self.request.get('has_rows') == '1'
      test_list = GetTestsMatchingPattern(
          pattern, only_with_rows=only_with_rows)
      self.response.out.write(json.dumps(test_list))


def GetSubTests(suite_name, bot_names):
  """Gets the entire tree of subtests for the suite with the given name.

  Each bot may have different sub-tests available, but there is one combined
  sub-tests dict returned for all the bots specified.

  This method is used by the test-picker select menus to display what tests
  are available; only tests that are not deprecated should be listed.

  Args:
    suite_name: Top level test name.
    bot_names: List of master/bot names in the form "<master>/<platform>".

  Returns:
    A dict mapping test names to dicts to entries which have the keys
    "has_rows" (boolean) and "sub_tests", which is another sub-tests dict.
    This forms a tree structure which matches the tree structure of the
    Test entities in the datastore.
  """
  # For some bots, there may be cached data; First collect and combine this.
  combined = {}
  for bot_name in bot_names:
    master, bot = bot_name.split('/')
    suite_key = ndb.Key('Master', master, 'Bot', bot, 'Test', suite_name)
    cached = layered_cache.Get(_ListSubTestCacheKey(suite_key))
    if cached:
      combined = _MergeSubTestsDict(combined, cached)
    else:
      sub_test_paths = _FetchSubTestPaths(suite_key)
      sub_tests = _SubTestsDict(sub_test_paths)
      layered_cache.Set(_ListSubTestCacheKey(suite_key), sub_tests)
      combined = _MergeSubTestsDict(combined, sub_tests)
  return combined


def _FetchSubTestPaths(test_key):
  """Makes a list of partial test paths for descendants of a test suite.

  Args:
    test_key: A ndb.Key object for a Test entity.

  Returns:
    A list of test paths for all descendant Test entities that have associated
    Row entities. These test paths omit the Master/bot/suite part.
  """
  query = graph_data.Test.query(ancestor=test_key)
  query = query.filter(
      graph_data.Test.has_rows == True,
      graph_data.Test.deprecated == False)
  keys = query.fetch(keys_only=True)
  return map(_SubTestPath, keys)


def _SubTestPath(test_key):
  """Returns the part of a test path starting from after the test suite."""
  full_test_path = utils.TestPath(test_key)
  parts = full_test_path.split('/')
  assert len(parts) > 3
  return '/'.join(parts[3:])


def _SubTestsDict(paths):
  """Constructs a sub-test dict from a list of test paths.

  Args:
    paths: An iterable of test paths for which there are points. Each test
        path is of the form "Master/bot/benchmark/chart/...". Each test path
        corresponds to a Test entity for which has_rows is set to True.

  Returns:
    A recursively nested dict of sub-tests, as returned by GetSubTests.
  """
  sub_tests = {}
  top_level = set(p.split('/')[0] for p in paths if p)
  for name in top_level:
    sub_test_paths = _SubPaths(paths, name)
    has_rows = name in paths
    sub_tests[name] = _SubTestsDictEntry(sub_test_paths, has_rows)
  return sub_tests


def _SubPaths(paths, first_part):
  """Returns paths of sub-tests that start with some name."""
  assert first_part
  return ['/'.join(p.split('/')[1:]) for p in paths
          if '/' in p and p.split('/')[0] == first_part]


def _SubTestsDictEntry(sub_test_paths, has_rows):
  """Recursively gets an entry in a sub-tests dict."""
  return {
      'has_rows': has_rows,
      'sub_tests': _SubTestsDict(sub_test_paths)
  }


def _ListSubTestCacheKey(test_key):
  """Returns the sub-tests list cache key for a test suite."""
  parts = utils.TestPath(test_key).split('/')
  master, bot, suite = parts[0:3]
  return graph_data.LIST_TESTS_SUBTEST_CACHE_KEY % (master, bot, suite)


def _MergeSubTestsDict(a, b):
  """Merges two sub-tests dicts together."""
  sub_tests = {}
  a_names, b_names = set(a), set(b)
  for name in a_names & b_names:
    sub_tests[name] = _MergeSubTestsDictEntry(a[name], b[name])
  for name in a_names - b_names:
    sub_tests[name] = a[name]
  for name in b_names - a_names:
    sub_tests[name] = b[name]
  return sub_tests


def _MergeSubTestsDictEntry(a, b):
  """Merges two corresponding sub-tests dict entries together."""
  assert a and b
  return {
      'has_rows': a['has_rows'] or b['has_rows'],
      'sub_tests': _MergeSubTestsDict(a['sub_tests'], b['sub_tests'])
  }


def GetTestsMatchingPattern(pattern, only_with_rows=False, list_entities=False):
  """Given a pattern, get the Test entities or keys which match.

  For this function, it's assumed that a test path should only have up to seven
  parts. In theory, tests can be arbitrarily nested, but in practice, tests
  are usually structured as master/bot/suite/graph/trace, and only a few have
  seven parts.

  Args:
    pattern: /-separated string of '*' wildcard and Test string_ids.
    only_with_rows: If True, only return Test entities which have data points.
    list_entities: If True, return entities. If false, return keys (faster).

  Returns:
    A list of test paths, or test entities if list_entities is True.
  """
  property_names = [
      'master_name', 'bot_name', 'suite_name', 'test_part1_name',
      'test_part2_name', 'test_part3_name', 'test_part4_name']
  pattern_parts = pattern.split('/')
  if len(pattern_parts) > 7:
    return []

  # Below, we first build a list of (property_name, value) pairs to filter on.
  query_filters = []
  for index, part in enumerate(pattern_parts):
    if '*' not in part:
      query_filters.append((property_names[index], part))
  for index in range(len(pattern_parts), 7):
    # Tests longer than the desired pattern will have non-empty property names,
    # so they can be filtered out by matching against an empty string.
    query_filters.append((property_names[index], ''))

  # Query tests based on the above filters. Pattern parts with * won't be
  # filtered here; the set of tests queried is a superset of the matching tests.
  query = graph_data.Test.query()
  for f in query_filters:
    query = query.filter(
        graph_data.Test._properties[f[0]] == f[1])  # pylint: disable=protected-access
  query = query.order(graph_data.Test.key)
  if only_with_rows:
    query = query.filter(
        graph_data.Test.has_rows == True)
  test_keys = query.fetch(keys_only=True)

  # Filter to include only tests that match the pattern.
  test_keys = [k for k in test_keys if utils.TestMatchesPattern(k, pattern)]

  if list_entities:
    return ndb.get_multi(test_keys)
  return [utils.TestPath(k) for k in test_keys]

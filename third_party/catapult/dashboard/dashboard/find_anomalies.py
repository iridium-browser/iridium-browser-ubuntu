# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Processes tests and creates new Anomaly entities.

This module contains the ProcessTest function, which searches the recent
points in a Test for potential regressions or improvements, and creates
new Anomaly entities.
"""

import logging

from google.appengine.ext import ndb

from dashboard import email_sheriff
from dashboard import find_change_points
from dashboard import utils
from dashboard.models import alert_group
from dashboard.models import anomaly
from dashboard.models import anomaly_config
from dashboard.models import graph_data

# Number of points to fetch and pass to FindChangePoints. A different number
# may be used if a test has a "max_window_size" anomaly config parameter.
DEFAULT_NUM_POINTS = 50


def ProcessTest(test_key):
  """Processes a test to find new anomalies.

  Args:
    test_key: The ndb.Key for a Test.
  """
  test = test_key.get()
  config = anomaly_config.GetAnomalyConfigDict(test)
  max_num_rows = config.get('max_window_size', DEFAULT_NUM_POINTS)
  rows = GetRowsToAnalyze(test, max_num_rows)
  # If there were no rows fetched, then there's nothing to analyze.
  if not rows:
    # In some cases (e.g. if some points are deleted) it might be possible
    # that last_alerted_revision is incorrect. In this case, reset it.
    highest_rev = _HighestRevision(test_key)
    if test.last_alerted_revision > highest_rev:
      logging.error('last_alerted_revision %d is higher than highest rev %d '
                    'for test %s; setting last_alerted_revision to None.',
                    test.last_alerted_revision, highest_rev, test.test_path)
      test.last_alerted_revision = None
      test.put()
    logging.error('No rows fetched for %s', test.test_path)
    return

  test = test_key.get()
  sheriff = _GetSheriffForTest(test)
  if not sheriff:
    logging.error('No sheriff for %s', test_key)
    return

  # Get anomalies and check if they happen in ref build also.
  change_points = _FindChangePointsForTest(rows, config)
  change_points = _FilterAnomaliesFoundInRef(change_points, test_key, len(rows))

  anomalies = [_MakeAnomalyEntity(c, test, rows) for c in change_points]

  # If no new anomalies were found, then we're done.
  if not anomalies:
    return

  logging.info('Found at least one anomaly in: %s', test.test_path)

  # Update the last_alerted_revision property of the test.
  test.last_alerted_revision = anomalies[-1].end_revision
  test.put()

  alert_group.GroupAlerts(
      anomalies, utils.TestSuiteName(test.key), 'Anomaly')

  # Email sheriff about any new regressions.
  for anomaly_entity in anomalies:
    if (anomaly_entity.bug_id is None and
        not anomaly_entity.is_improvement and
        not sheriff.summarize):
      email_sheriff.EmailSheriff(sheriff, test, anomaly_entity)

  ndb.put_multi(anomalies)


def GetRowsToAnalyze(test, max_num_rows):
  """Gets the Row entities that we want to analyze.

  Args:
    test: The Test entity to get data for.
    max_num_rows: The maximum number of points to get.

  Returns:
    A list of the latest Rows after the last alerted revision, ordered by
    revision. These rows are fetched with t a projection query so they only
    have the revision and value properties.
  """
  query = graph_data.Row.query(projection=['revision', 'value'])
  query = query.filter(graph_data.Row.parent_test == test.key)

  # The query is ordered in descending order by revision because we want
  # to get the newest points.
  query = query.filter(graph_data.Row.revision > test.last_alerted_revision)
  query = query.order(-graph_data.Row.revision)

  # However, we want to analyze them in ascending order.
  return list(reversed(query.fetch(limit=max_num_rows)))


def _HighestRevision(test_key):
  """Gets the revision number of the Row with the highest ID for a Test."""
  query = graph_data.Row.query(graph_data.Row.parent_test == test_key)
  query = query.order(-graph_data.Row.revision)
  highest_row_key = query.get(keys_only=True)
  if highest_row_key:
    return highest_row_key.id()
  return None


def _FilterAnomaliesFoundInRef(change_points, test_key, num_rows):
  """Filters out the anomalies that match the anomalies in ref build.

  Background about ref build tests: Variation in test results can be caused
  by changes in Chrome or changes in the test-running environment. The ref
  build results are results from a reference (stable) version of Chrome run
  in the same environment. If an anomaly happens in the ref build results at
  the same time as an anomaly happened in the test build, that suggests that
  the variation was caused by a change in the test-running environment, and
  can be ignored.

  Args:
    change_points: ChangePoint objects returned by FindChangePoints.
    test_key: ndb.Key of monitored Test.
    num_rows: Number of Rows that were analyzed from the Test. When fetching
        the ref build Rows, we need not fetch more than |num_rows| rows.

  Returns:
    A copy of |change_points| possibly with some entries filtered out.
    Any entries in |change_points| whose end revision matches that of
    an anomaly found in the corresponding ref test will be filtered out.
  """
  # Get anomalies for ref build.
  ref_test = _CorrespondingRefTest(test_key)
  if not ref_test:
    return change_points[:]

  ref_config = anomaly_config.GetAnomalyConfigDict(ref_test)
  ref_rows = GetRowsToAnalyze(ref_test, num_rows)
  ref_change_points = _FindChangePointsForTest(ref_rows, ref_config)
  if not ref_change_points:
    return change_points[:]

  change_points_filtered = []
  test_path = utils.TestPath(test_key)
  for c in change_points:
    # Log information about what anomaly got filtered and what did not.
    if not _IsAnomalyInRef(c, ref_change_points):
      # TODO(qyearsley): Add test coverage. See http://crbug.com/447432
      logging.info('Nothing was filtered out for test %s, and revision %s',
                   test_path, c.x_value)
      change_points_filtered.append(c)
    else:
      logging.info('Filtering out anomaly for test %s, and revision %s',
                   test_path, c.x_value)
  return change_points_filtered


def _CorrespondingRefTest(test_key):
  """Returns the Test for the corresponding ref build trace, or None."""
  test_path = utils.TestPath(test_key)
  possible_ref_test_paths = [test_path + '_ref', test_path + '/ref']
  for path in possible_ref_test_paths:
    ref_test = utils.TestKey(path).get()
    if ref_test:
      return ref_test
  return None


def _IsAnomalyInRef(change_point, ref_change_points):
  """Checks if anomalies are detected in both ref and non ref build.

  Args:
    change_point: A find_change_points.ChangePoint object to check.
    ref_change_points: List of find_change_points.ChangePoint objects
        found for a ref build series.

  Returns:
    True if there is a match found among the ref build series change points.
  """
  for ref_change_point in ref_change_points:
    if change_point.x_value == ref_change_point.x_value:
      return True
  # TODO(qyearsley): Add test coverage. See http://crbug.com/447432
  return False


def _GetSheriffForTest(test):
  """Gets the Sheriff for a test, or None if no sheriff."""
  if test.sheriff:
    return test.sheriff.get()
  return None


def _GetImmediatelyPreviousRevisionNumber(later_revision, rows):
  """Gets the revision number of the Row immediately before the given one.

  Args:
    later_revision: A revision number.
    rows: List of Row entities in ascending order by revision.

  Returns:
    The revision number just before the given one.
  """
  for row in reversed(rows):
    if row.revision < later_revision:
      return row.revision
  # TODO(qyearsley): Add test coverage. See http://crbug.com/447432
  assert False, 'No matching revision found in |rows|.'


def _MakeAnomalyEntity(change_point, test, rows):
  """Creates an Anomaly entity.

  Args:
    change_point: A find_change_points.ChangePoint object.
    test: The Test entity that the anomalies were found on.
    rows: List of Row entities that the anomalies were found on.

  Returns:
    An Anomaly entity, which is not yet put in the datastore.
  """
  end_rev = change_point.x_value
  start_rev = _GetImmediatelyPreviousRevisionNumber(end_rev, rows) + 1
  median_before = change_point.median_before
  median_after = change_point.median_after
  return anomaly.Anomaly(
      start_revision=start_rev,
      end_revision=end_rev,
      median_before_anomaly=median_before,
      median_after_anomaly=median_after,
      segment_size_before=change_point.size_before,
      segment_size_after=change_point.size_after,
      window_end_revision=change_point.window_end,
      std_dev_before_anomaly=change_point.std_dev_before,
      t_statistic=change_point.t_statistic,
      degrees_of_freedom=change_point.degrees_of_freedom,
      p_value=change_point.p_value,
      is_improvement=_IsImprovement(test, median_before, median_after),
      test=test.key,
      sheriff=test.sheriff,
      internal_only=test.internal_only)


def _FindChangePointsForTest(rows, config_dict):
  """Gets the anomaly data from the anomaly detection module.

  Args:
    rows: The Row entities to find anomalies for, sorted backwards by revision.
    config_dict: Anomaly threshold parameters as a dictionary.

  Returns:
    A list of find_change_points.ChangePoint objects.
  """
  data_series = [(row.revision, row.value) for row in rows]
  return find_change_points.FindChangePoints(data_series, **config_dict)


def _IsImprovement(test, median_before, median_after):
  """Returns whether the alert is an improvement for the given test.

  Args:
    test: Test to get the improvement direction for.
    median_before: The median of the segment immediately before the anomaly.
    median_after: The median of the segment immediately after the anomaly.

  Returns:
    True if it is improvement anomaly, otherwise False.
  """
  if (median_before < median_after and
      test.improvement_direction == anomaly.UP):
    return True
  if (median_before >= median_after and
      test.improvement_direction == anomaly.DOWN):
    return True
  return False

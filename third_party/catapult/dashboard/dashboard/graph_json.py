# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Serves JSON for a graph.

This serves the JSON in the format consumed by Flot:
https://github.com/flot/flot/blob/master/API.md
"""

import copy
import datetime
import json
import logging
import re
import urllib

from google.appengine.ext import ndb

from dashboard import alerts
from dashboard import list_tests
from dashboard import request_handler
from dashboard import utils
from dashboard.models import anomaly
from dashboard.models import graph_data

# Default number of points to fetch per test.
# This can be overridden by specifying num_points or start_rev and end_rev.
_DEFAULT_NUM_POINTS = 150

# Dictionary mapping improvement directions constants to strings.
_BETTER_DICT = {
    anomaly.UP: 'Higher',
    anomaly.DOWN: 'Lower',
    anomaly.UNKNOWN: '?',
}

# Amount of time before a warning is shown for tests with no new data.
_STALE_DATA_DELTA = datetime.timedelta(days=7)


class GraphJsonHandler(request_handler.RequestHandler):
  """Request handler for requests for graph data."""

  def post(self):
    """Fetches and prepares data for a graph.

    Request parameters:
      graphs: A JSON serialization of a dict that contains the arguments
          for GetGraphJson.

    Outputs:
      JSON serialization of data to be used for plotting a graph.
    """
    self.response.headers.add_header('Access-Control-Allow-Origin', '*')
    arguments = self._ParseRequestArguments()
    if not arguments:
      self.ReportError('Bad Graph JSON Request')
      return
    self.response.out.write(GetGraphJson(**arguments))

  def _ParseRequestArguments(self):
    """Parses parameters from a request and checks for errors.

    The post request is expected to pass one parameter, called 'graphs',
    whose value is a JSON serialization of a dict of parameters.

    Returns:
      A dict of arguments that can be given to GetGraphJson, or None if
      no valid dict of arguments can be constructed.
    """
    graphs = self.request.get('graphs')
    if graphs is None:
      logging.error('No graph names specified')
      return None
    try:
      graphs = json.loads(graphs)
    except ValueError:
      logging.error('Invalid JSON string for graphs')
      return None

    if not graphs.get('test_path_dict'):
      logging.error('No test_path_dict specified')
      return None

    arguments = {
        'test_path_dict': graphs['test_path_dict'],
        'rev': PositiveIntOrNone(graphs.get('rev')),
        'num_points': (PositiveIntOrNone(graphs.get('num_points'))
                       or _DEFAULT_NUM_POINTS),
        'is_selected': graphs.get('is_selected'),
        'start_rev': PositiveIntOrNone(graphs.get('start_rev')),
        'end_rev': PositiveIntOrNone(graphs.get('end_rev')),
    }
    return arguments


def PositiveIntOrNone(input_str):
  """Parses a string as a positive int if possible, otherwise returns None."""
  if not input_str:
    return None
  try:
    parsed = int(input_str)
  except ValueError:
    return None
  if parsed < 0:
    return None
  return parsed


def _GetAnomalyAnnotationMap(test):
  """Gets a map of revision numbers to Anomaly entities."""
  anomalies = anomaly.Anomaly.query().filter(
      anomaly.Anomaly.test == test).fetch()
  return dict((a.end_revision, a) for a in anomalies)


def _UpdateRevisionMap(revision_map, parent_test, rev, num_points,
                       start_rev=None, end_rev=None):
  """Updates a dict of revisions to data point information for one Test.

  Depending on which arguments are given, there are several ways that
  this function can update the dict of revisions:
    1. If start_rev and end_rev are given, then revisions in this range
       are used. The num_points argument is ignored.
    2. Otherwise, if rev is given, then revisions before and after the
       specified revision are used.
    3. Otherwise, the latest revisions are used.

  Args:
    revision_map: A dict mapping revision numbers to dicts of point info.
        Each point info dict contains information from a Row entity.
    parent_test: A Test entity with Row children.
    rev: The middle revision in the revision map (could be None).
    num_points: The number of points to include in the revision map.
    start_rev: Start revision number (optional).
    end_rev: End revision number (optional).
  """
  anomaly_annotation_map = _GetAnomalyAnnotationMap(parent_test.key)

  if start_rev and end_rev:
    rows = _GetRowsForTestInRange(parent_test.key, start_rev, end_rev)
  elif rev:
    assert num_points
    rows = _GetRowsForTestAroundRev(parent_test.key, rev, num_points)
  else:
    assert num_points
    rows = _GetLatestRowsForTest(parent_test.key, num_points)

  parent_test_key = parent_test.key.urlsafe()
  for row in rows:
    if row.revision not in revision_map:
      revision_map[row.revision] = {}
    revision_map[row.revision][parent_test_key] = _PointInfoDict(
        row, parent_test, anomaly_annotation_map)


def _PointInfoDict(row, parent_test, anomaly_annotation_map):
  """Makes a dict of properties of one Row."""
  point_info = {
      'value': row.value,
      'a_trace_rerun_options': _GetTracingRerunOptions(row),
  }

  tracing_uri = _GetTracingUri(row)
  if tracing_uri:
    point_info['a_tracing_uri'] = tracing_uri

  old_stdio_uri = _GetOldStdioUri(row, parent_test)
  if old_stdio_uri:
    point_info.update(
        _CreateLinkProperty('stdio_uri', 'Buildbot stdio', old_stdio_uri))

  if row.error is not None:
    point_info['error'] = row.error
  if anomaly_annotation_map.get(row.revision):
    anomaly_entity = anomaly_annotation_map.get(row.revision)
    point_info['g_anomaly'] = alerts.GetAnomalyDict(anomaly_entity)
  for name, val in row.to_dict().iteritems():
    if name.startswith('r_'):
      point_info[name] = val
    elif name == 'a_default_rev':
      point_info['a_default_rev'] = val
    elif name == 'timestamp':
      point_info['timestamp'] = val
    elif name.startswith('a_') and _IsMarkdownLink(val):
      point_info[name] = val
  return point_info


def _IsMarkdownLink(value):
  """Checks whether |value| is a markdown link."""
  if not isinstance(value, str):
    return False
  return re.match(r'\[.+?\]\(.+?\)', value)


def _CreateLinkProperty(name, label, url):
  """Returns a dict containing markdown link to show on dashboard."""
  return {'a_' + name: '[%s](%s)' % (label, url)}


def _GetOldStdioUri(row, test):
  """Gets or makes the URI string for the buildbot stdio link.

  This is here to support the deprecated method way of creating
  Buildbot stdio URI.

  TODO(chrisphan): Remove this after sometime.

  Args:
    row: A Row entity.
    test: The Test entity for the given Row.

  Returns:
    An URI string, or None if none can be made.
  """
  # A masterid and buildname are required to construct a valid URI.
  if (not hasattr(test, 'masterid') or not hasattr(test, 'buildername')
      or not hasattr(row, 'buildnumber')):
    return None

  buildbot_uri_prefix = _GetBuildbotUriPrefix(test, row=row)
  if not buildbot_uri_prefix:
    return None
  return '%s/%s/builders/%s/builds/%s/steps/%s/logs/stdio' % (
      buildbot_uri_prefix,
      urllib.quote(test.masterid),
      urllib.quote(test.buildername),
      urllib.quote(str(getattr(row, 'buildnumber'))),
      urllib.quote(test.suite_name))


def _GetBuildbotUriPrefix(test, row=None):
  """Gets the start of the buildbot stdio or builder status URI.

  Gets the uri prefix from 'a_stdio_uri_prefix' property if exist or
  the public uri prefix if test is not internal.

  Args:
    test: A Test entity.
    row: A Row entity, optional.

  Returns:
    The protocol, hostname and start of the pathname for Buildbot builder
    status or stdio links.
  """
  if row and hasattr(row, 'a_stdio_uri_prefix'):
    return row.a_stdio_uri_prefix

  if test.internal_only:
    return None
  return 'http://build.chromium.org/p'


def _GetRowsForTestInRange(test_key, start_rev, end_rev):
  """Gets all the Row entities for a Test between a given start and end."""
  query = graph_data.Row.query(
      graph_data.Row.parent_test == test_key,
      graph_data.Row.revision >= start_rev,
      graph_data.Row.revision <= end_rev)
  return query.fetch(batch_size=100)


def _GetRowsForTestAroundRev(test_key, rev, num_points):
  """Gets up to num_points Row entities for a Test centered on a revision."""
  num_rows_before = int(num_points / 2) + 1
  num_rows_after = int(num_points / 2)

  query_up_to_rev = graph_data.Row.query(
      graph_data.Row.parent_test == test_key,
      graph_data.Row.revision <= rev)
  query_up_to_rev = query_up_to_rev.order(-graph_data.Row.revision)
  rows_up_to_rev = query_up_to_rev.fetch(limit=num_rows_before, batch_size=100)

  query_after_rev = graph_data.Row.query(
      graph_data.Row.parent_test == test_key,
      graph_data.Row.revision > rev)
  query_after_rev = query_after_rev.order(graph_data.Row.revision)
  rows_after_rev = query_after_rev.fetch(limit=num_rows_after, batch_size=100)

  return rows_up_to_rev + rows_after_rev


def _GetLatestRowsForTest(test_key, num_points):
  """Gets the latest num_points Row entities for a Test."""
  query = graph_data.Row.query(graph_data.Row.parent_test == test_key)
  query = query.order(-graph_data.Row.revision)
  return query.fetch(limit=num_points, batch_size=100)


def _GetSeriesAnnotations(tests):
  """Makes a list of metadata about each series (i.e. each test).

  Args:
    tests: List of Test entities.

  Returns:
    A list of dicts of metadata about each series. One dict for each test.
  """
  series_annotations = {}
  for i, test in enumerate(tests):
    series_annotations[i] = {
        'name': test.key.string_id(),
        'path': test.test_path,
        'units': test.units,
        'better': _BETTER_DICT[test.improvement_direction],
        'description': test.description
    }
  return series_annotations


def _ClampRevisionMap(revision_map, rev, num_points):
  """Clamp the results down to the requested number of points before/after rev.

  Not all of the Tests have Rows for the exact same revisions. If one test has
  gaps in the requested range, the query for points before/after rev will
  extend outside the range, but the other tests with complete data will not
  extend their query range. We only want the num_points/2 rows nearest rev
  because the extended range didn't query all of the tests. See crbug.com/236718

  Args:
    revision_map: The dict with all found revisions. This will be modified.
    rev: The requested revision.
    num_points: The requested number of points to plot.
  """
  revisions = sorted(revision_map.keys())
  if len(revisions) <= num_points:
    return

  # Default to clamping to the last revision, then try to fill in better.
  index = len(revisions) - 1
  if rev is not None:
    for i, r in enumerate(revisions):
      if r >= rev:
        index = i
        break

  rows_before = int(num_points / 2) if rev is not None else num_points
  clamp_before = max(index - rows_before, 0)
  rows_after = int(num_points / 2) if rev is not None else 0
  clamp_after = index + rows_after + 1
  for rev_to_delete in (
      revisions[:clamp_before] + revisions[clamp_after:]):
    del revision_map[rev_to_delete]


def _GetTracingUri(point):
  """Gets the URI string for tracing in cloud storage, if available.

  Args:
    point: A Row entitiy.

  Returns:
    An URI string, or None if there is no trace available.
  """
  if not hasattr(point, 'a_tracing_uri'):
    return None
  return point.a_tracing_uri


def _GetTracingRerunOptions(point):
  """Gets the trace rerun options, if available.

  Args:
    point: A Row entitiy.

  Returns:
    A dict of {description: params} strings, or None.
  """
  if not hasattr(point, 'a_trace_rerun_options'):
    return None
  return point.a_trace_rerun_options.to_dict()


def _GetFlotJson(revision_map, tests, show_old_data_warning):
  """Constructs JSON in the format expected by Flot.

  Args:
    revision_map: A dict which maps revision numbers to data point info.
    tests: A list of Test entities.
    show_old_data_warning: Whether to a show a warning to the user that
        the graph data is out of date.

  Returns:
    JSON serialization of a dict with line data, annotations, error range data,
    and possibly warning information. (This data may not be passed exactly
    as-is to the Flot plot funciton, but it will all be used when plotting.)
  """
  # TODO(qyearsley): Break this function into smaller functions.

  # Each entry in the following dict is one Flot series object. The actual
  # x-y values will be put into the 'data' properties for each object.
  cols = {i: _FlotSeries(i) for i in  range(len(tests))}

  flot_annotations = {}
  flot_annotations['series'] = _GetSeriesAnnotations(tests)

  # For each Test (which corresponds to a trace line), the shaded error
  # region is specified by two series objects. For a demo, see:
  # http://www.flotcharts.org/flot/examples/percentiles/index.html
  error_bars = {x: [
      {
          'id': 'bottom_%d' % x,
          'data': [],
          'color': x,
          'clickable': False,
          'hoverable': False,
          'lines': {
              'show': True,
              'lineWidth': 0,
              'fill': 0.2,
          },
          'fillBetween': 'line_%d' % x,
      },
      {
          'id': 'top_%d' % x,
          'data': [],
          'color': x,
          'clickable': False,
          'hoverable': False,
          'lines': {
              'show': True,
              'lineWidth': 0,
              'fill': 0.2,
          },
          'fillBetween': 'line_%d' % x,
      }
  ] for x, _ in enumerate(tests)}
  test_keys = [t.key.urlsafe() for t in tests]
  last_timestamp = None
  has_points = False
  for revision in sorted(revision_map.keys()):
    for series_index, key in enumerate(test_keys):
      point_info = revision_map[revision].get(key, None)
      if not point_info:
        continue
      has_points = True
      timestamp = point_info.get('timestamp')
      if timestamp:
        if type(timestamp) is datetime.datetime:
          point_info['timestamp'] = utils.TimestampMilliseconds(timestamp)
        if not last_timestamp or point_info['timestamp'] > last_timestamp:
          last_timestamp = point_info['timestamp']

      point_list = [revision, point_info['value']]
      if 'error' in point_info:
        error = point_info['error']
        error_bars[series_index][0]['data'].append(
            [revision, point_info['value'] - error])
        error_bars[series_index][1]['data'].append(
            [revision, point_info['value'] + error])
      cols[series_index]['data'].append(point_list)
      data_index = len(cols[series_index]['data']) - 1
      series_dict = flot_annotations.setdefault(series_index, {})
      data_dict = copy.deepcopy(point_info)
      del data_dict['value']
      series_dict.setdefault(data_index, data_dict)
  warning = None

  if show_old_data_warning and last_timestamp:
    last_timestamp = datetime.datetime.fromtimestamp(last_timestamp / 1000)
    if last_timestamp < datetime.datetime.now() - _STALE_DATA_DELTA:
      warning = ('Graph out of date! Last data received: %s' %
                 last_timestamp.strftime('%Y/%m/%d %H:%M'))
  elif not has_points:
    warning = 'No data available.'
    if not utils.IsInternalUser():
      warning += ' Note that some data is only available when logged in.'
  return json.dumps(
      {
          'data': cols,
          'annotations': flot_annotations,
          'error_bars': error_bars,
          'warning': warning
      },
      allow_nan=False)


def _FlotSeries(index):
  return {
      'data': [],
      'color': index,
      'id': 'line_%d' % index
  }


def _GetTestPathFromDict(test_path_dict):
  """Gets a list of test paths from a test path dictionary.

  This function looks up series and the corresponding list of selected
  series and returns test paths of those that contain rows.

  Args:
    test_path_dict: Dictionary of test path to list of selected series.

  Returns:
    List of test paths with rows.
  """
  test_paths_with_rows = []
  for test_path in test_path_dict:
    parent_test_name = test_path.split('/')[-1]
    selected_traces = test_path_dict[test_path]
    if not selected_traces:
      sub_test_dict = _GetSubTestDict([test_path])
      selected_traces = _GetSubTestTraces(test_path, sub_test_dict)
    for trace in selected_traces:
      if trace == parent_test_name:
        test_paths_with_rows.append(test_path)
      else:
        test_paths_with_rows.append(test_path + '/' + trace)
  return test_paths_with_rows


def _GetUnselectedTestPathFromDict(test_path_dict):
  """Gets a list of test paths for unselected series for a test path dictionary.

  This function looks up series that are directly under provided test path
  that are also not in the list of selected series.

  Args:
    test_path_dict: Dictionary of test path to list of selected sub-series.

  Returns:
    List of test paths of Tests that have rows.
  """
  test_paths = []
  sub_test_dict = _GetSubTestDict(t for t in test_path_dict)
  for test_path in test_path_dict:
    parent_test_name = test_path.split('/')[-1]
    selected_traces = test_path_dict[test_path]
    # Add sub-tests not in selected traces.
    unselected_traces = _GetSubTestTraces(test_path, sub_test_dict)
    for trace in unselected_traces:
      if trace not in selected_traces:
        if trace == parent_test_name:
          test_paths.append(test_path)
        else:
          test_paths.append(test_path + '/' + trace)
  return test_paths


def _GetSubTestDict(test_paths):
  """Gets a dict of test suite path to sub test dict.

  Args:
    test_paths: List of test paths.

  Returns:
    Dictionary of test suite path to sub-test tree (see
    list_tests.GetSubTests).
  """
  subtests = {}
  for test_path in test_paths:
    path_parts = test_path.split('/')
    bot_path = '/'.join(path_parts[0:2])
    test_suite_path = '/'.join(path_parts[0:3])
    test_suite = path_parts[2]
    if test_suite_path not in subtests:
      subtests[test_suite_path] = {}
    subtests[test_suite_path] = list_tests.GetSubTests(test_suite, [bot_path])
  return subtests


def _GetSubTestTraces(test_path, sub_test_dict):
  """Gets summary and sub-test traces directly underneath test_path.

  Args:
    test_path: A test path.
    sub_test_dict: Dictionary of test suite path to sub-test tree.

  Returns:
    List of trace names.
  """
  traces = []
  test_parts = test_path.split('/')
  test_suite_path = '/'.join(test_parts[0:3])
  target_trace = test_parts[-1]

  if test_suite_path not in sub_test_dict:
    return []
  sub_test_tree = sub_test_dict[test_suite_path]

  for part in test_parts[3:-1]:
    if part in sub_test_tree:
      sub_test_tree = sub_test_tree[part]['sub_tests']
    else:
      return []
  if target_trace not in sub_test_tree:
    return []

  # Add target trace.
  target_sub_test_tree = sub_test_tree[target_trace]
  if target_sub_test_tree['has_rows']:
    traces.append(target_trace)

  # Add direct sub-tests.
  for key, value in target_sub_test_tree['sub_tests'].iteritems():
    if value['has_rows']:
      traces.append(key)
  return traces


def GetGraphJson(
    test_path_dict, rev=None, num_points=None,
    is_selected=True, start_rev=None, end_rev=None):
  """Makes a JSON serialization of data for one chart with multiple series.

  This function can return data for one chart (with multiple data series
  plotted on it) with revisions on the x-axis, for a certain range of
  revisions. The particular set of revisions to get data for can be specified
  with the arguments rev, num_points, start_rev, and end_rev.

  Args:
    test_path_dict: Dictionary of test path to list of selected series.
    rev: A revision number that the chart may be clamped relative to.
    num_points: Number of points to plot.
    is_selected: Whether this request is for selected or un-selected series.
    start_rev: The lowest revision to get trace data for.
    end_rev: The highest revision to get trace data for.

  Returns:
    JSON serialization of a dict with info that will be used to plot a chart.
  """
  # TODO(qyearsley): Parallelize queries if possible.

  # Get a list of Test entities.
  if is_selected:
    test_paths = _GetTestPathFromDict(test_path_dict)
  else:
    test_paths = _GetUnselectedTestPathFromDict(test_path_dict)

  test_keys = map(utils.TestKey, test_paths)
  test_entities = ndb.get_multi(test_keys)
  test_entities = [t for t in test_entities if t is not None]

  # Filter out deprecated tests, but only if not all the tests are deprecated.
  all_deprecated = all(t.deprecated for t in test_entities)
  if not all_deprecated:
    test_entities = [t for t in test_entities if not t.deprecated]

  test_entities = [t for t in test_entities if t.has_rows]
  revision_map = {}
  num_points = num_points or _DEFAULT_NUM_POINTS
  for test in test_entities:
    _UpdateRevisionMap(revision_map, test, rev, num_points, start_rev, end_rev)
  if not (start_rev and end_rev):
    _ClampRevisionMap(revision_map, rev, num_points)
  show_old_data_warning = not (rev or start_rev or end_rev)
  return _GetFlotJson(revision_map, test_entities, show_old_data_warning)

# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""URL endpoint containing server-side functionality for pinpoint jobs."""

import json

from google.appengine.api import users
from google.appengine.ext import ndb

from dashboard import start_try_job
from dashboard.common import namespaced_stored_object
from dashboard.common import request_handler
from dashboard.common import utils
from dashboard.services import crrev_service
from dashboard.services import pinpoint_service

_PINPOINT_REPOSITORIES = 'repositories'
_ISOLATE_TARGETS = [
    'angle_perftests', 'cc_perftests', 'gpu_perftests',
    'load_library_perf_tests', 'media_perftests', 'net_perftests',
    'performance_browser_tests', 'telemetry_perf_tests',
    'telemetry_perf_webview_tests', 'tracing_perftests']


class InvalidParamsError(Exception):
  pass


class PinpointNewPrefillRequestHandler(request_handler.RequestHandler):
  def post(self):
    story_filter = start_try_job.GuessStoryFilter(self.request.get('test_path'))
    self.response.write(json.dumps({'story_filter': story_filter}))


class PinpointNewBisectRequestHandler(request_handler.RequestHandler):
  def post(self):
    job_params = dict(
        (a, self.request.get(a)) for a in self.request.arguments())

    try:
      pinpoint_params = PinpointParamsFromBisectParams(job_params)
    except InvalidParamsError as e:
      self.response.write(json.dumps({'error': e.message}))
      return

    results = pinpoint_service.NewJob(pinpoint_params)

    alert_keys = job_params.get('alerts')
    if 'jobId' in results and alert_keys:
      alerts = json.loads(alert_keys)
      for alert_urlsafe_key in alerts:
        alert = ndb.Key(urlsafe=alert_urlsafe_key).get()
        alert.pinpoint_bisects.append(results['jobId'])
        alert.put()

    self.response.write(json.dumps(results))


class PinpointNewPerfTryRequestHandler(request_handler.RequestHandler):
  def post(self):
    job_params = dict(
        (a, self.request.get(a)) for a in self.request.arguments())

    try:
      pinpoint_params = PinpointParamsFromPerfTryParams(job_params)
    except InvalidParamsError as e:
      self.response.write(json.dumps({'error': e.message}))
      return

    self.response.write(json.dumps(pinpoint_service.NewJob(pinpoint_params)))


def ParseMetricParts(test_path_parts):
  metric_parts = test_path_parts[3:]

  # Normal test path structure, ie. M/B/S/foo/bar.html
  if len(metric_parts) == 2:
    return '', metric_parts[0], metric_parts[1]

  # 3 part structure, so there's a TIR label in there.
  # ie. M/B/S/timeToFirstMeaningfulPaint_avg/load_tools/load_tools_weather
  if len(metric_parts) == 3:
    return metric_parts[1], metric_parts[0], metric_parts[2]

  # Should be something like M/B/S/EventsDispatching where the trace_name is
  # left empty and implied to be summary.
  assert len(metric_parts) == 1
  return '', metric_parts[0], ''


def ResolveToGitHash(commit_position, repository):
  try:
    int(commit_position)
    if repository != 'chromium':
      raise InvalidParamsError(
          'Repository %s commit positions not supported.' % repository)
    result = crrev_service.GetNumbering(
        number=commit_position,
        numbering_identifier='refs/heads/master',
        numbering_type='COMMIT_POSITION',
        project='chromium',
        repo='chromium/src')
    if 'error' in result:
      raise InvalidParamsError(
          'Error retrieving commit info: %s' % result['error'].get('message'))
    return result['git_sha']
  except ValueError:
    pass

  # It was probably a git hash, so just return as is
  return commit_position


def ParseTIRLabelChartNameAndTraceName(test_path_parts):
  """Returns tir_label, chart_name, trace_name from a test path."""
  test = ndb.Key('TestMetadata', '/'.join(test_path_parts)).get()

  tir_label, chart_name, trace_name = ParseMetricParts(test_path_parts)
  if trace_name and test.unescaped_story_name:
    trace_name = test.unescaped_story_name
  return tir_label, chart_name, trace_name


def ParseStatisticNameFromChart(chart_name):
  statistic_types = [
      'avg', 'min', 'max', 'sum', 'std', 'count'
  ]

  chart_name_parts = chart_name.split('_')
  statistic_name = ''

  if chart_name_parts[-1] in statistic_types:
    chart_name = '_'.join(chart_name_parts[:-1])
    statistic_name = chart_name_parts[-1]
  return chart_name, statistic_name


def PinpointParamsFromPerfTryParams(params):
  """Takes parameters from Dashboard's pinpoint-perf-job-dialog and returns
  a dict with parameters for a new Pinpoint job.

  Args:
    params: A dict in the following format:
    {
        'test_path': Test path for the metric being bisected.
        'start_commit': Git hash or commit position of earlier revision.
        'end_commit': Git hash or commit position of later revision.
        'start_repository': Repository for earlier revision.
        'end_repository': Repository for later revision.
        'extra_test_args': Extra args for the swarming job.
    }

  Returns:
    A dict of params for passing to Pinpoint to start a job, or a dict with an
    'error' field.
  """
  if not utils.IsValidSheriffUser():
    user = users.get_current_user()
    raise InvalidParamsError('User "%s" not authorized.' % user)

  test_path = params['test_path']
  test_path_parts = test_path.split('/')
  bot_name = test_path_parts[1]
  suite = test_path_parts[2]

  # Pinpoint also requires you specify which isolate target to run the
  # test, so we derive that from the suite name. Eventually, this would
  # ideally be stored in a SparesDiagnostic but for now we can guess.
  target = 'telemetry_perf_tests'
  if suite in _ISOLATE_TARGETS:
    raise InvalidParamsError('Only telemetry is supported at the moment.')
  elif 'webview' in bot_name:
    target = 'telemetry_perf_webview_tests'

  start_repository = params['start_repository']
  end_repository = params['end_repository']
  start_commit = params['start_commit']
  end_commit = params['end_commit']

  start_git_hash = ResolveToGitHash(start_commit, start_repository)
  end_git_hash = ResolveToGitHash(end_commit, end_repository)

  supported_repositories = namespaced_stored_object.Get(_PINPOINT_REPOSITORIES)

  # Bail if it's not a supported repository to bisect on
  if not start_repository in supported_repositories:
    raise InvalidParamsError('Invalid repository: %s' % start_repository)
  if not end_repository in supported_repositories:
    raise InvalidParamsError('Invalid repository: %s' % end_repository)

  # Pinpoint only supports chromium at the moment, so just throw up a
  # different error for now.
  if start_repository != 'chromium' or end_repository != 'chromium':
    raise InvalidParamsError('Only chromium perf try jobs supported currently.')

  extra_test_args = params['extra_test_args']

  email = users.get_current_user().email()
  job_name = 'Job on [%s/%s] for [%s]' % (bot_name, suite, email)

  return {
      'configuration': bot_name,
      'benchmark': suite,
      'trace': '',
      'chart': '',
      'tir_label': '',
      'story': '',
      'start_repository': start_repository,
      'end_repository': end_repository,
      'start_git_hash': start_git_hash,
      'end_git_hash': end_git_hash,
      'extra_test_args': extra_test_args,
      'bug_id': '',
      'auto_explore': '0',
      'target': target,
      'email': email,
      'name': job_name
  }


def PinpointParamsFromBisectParams(params):
  """Takes parameters from Dashboard's pinpoint-job-dialog and returns
  a dict with parameters for a new Pinpoint job.

  Args:
    params: A dict in the following format:
    {
        'test_path': Test path for the metric being bisected.
        'start_git_hash': Git hash of earlier revision.
        'end_git_hash': Git hash of later revision.
        'start_repository': Repository for earlier revision.
        'end_repository': Repository for later revision.
        'bug_id': Associated bug.
    }

  Returns:
    A dict of params for passing to Pinpoint to start a job, or a dict with an
    'error' field.
  """
  if not utils.IsValidSheriffUser():
    user = users.get_current_user()
    raise InvalidParamsError('User "%s" not authorized.' % user)

  test_path = params['test_path']
  test_path_parts = test_path.split('/')
  bot_name = test_path_parts[1]
  suite = test_path_parts[2]
  story_filter = params['story_filter']

  # If functional bisects are speciied, Pinpoint expects these parameters to be
  # empty.
  bisect_mode = params['bisect_mode']
  if bisect_mode != 'performance' and bisect_mode != 'functional':
    raise InvalidParamsError('Invalid bisect mode %s specified.' % bisect_mode)

  tir_label = ''
  chart_name = ''
  trace_name = ''
  if bisect_mode == 'performance':
    tir_label, chart_name, trace_name = ParseTIRLabelChartNameAndTraceName(
        test_path_parts)

  # Pinpoint also requires you specify which isolate target to run the
  # test, so we derive that from the suite name. Eventually, this would
  # ideally be stored in a SparesDiagnostic but for now we can guess.
  target = 'telemetry_perf_tests'
  if suite in _ISOLATE_TARGETS:
    target = suite
  elif 'webview' in bot_name:
    target = 'telemetry_perf_webview_tests'

  start_repository = params['start_repository']
  end_repository = params['end_repository']
  start_commit = params['start_commit']
  end_commit = params['end_commit']

  start_git_hash = ResolveToGitHash(start_commit, start_repository)
  end_git_hash = ResolveToGitHash(end_commit, end_repository)

  supported_repositories = namespaced_stored_object.Get(_PINPOINT_REPOSITORIES)

  # Bail if it's not a supported repository to bisect on
  if not start_repository in supported_repositories:
    raise InvalidParamsError('Invalid repository: %s' % start_repository)
  if not end_repository in supported_repositories:
    raise InvalidParamsError('Invalid repository: %s' % end_repository)

  # Pinpoint only supports chromium at the moment, so just throw up a
  # different error for now.
  if start_repository != 'chromium' or end_repository != 'chromium':
    raise InvalidParamsError('Only chromium bisects supported currently.')

  email = users.get_current_user().email()
  job_name = 'Job on [%s/%s/%s] for [%s]' % (bot_name, suite, chart_name, email)

  # Histogram names don't include the statistic, so split these
  chart_name, statistic_name = ParseStatisticNameFromChart(chart_name)

  alert_key = ''
  if params.get('alerts'):
    alert_keys = json.loads(params.get('alerts'))
    if alert_keys:
      alert_key = alert_keys[0]

  return {
      'configuration': bot_name,
      'benchmark': suite,
      'trace': trace_name,
      'chart': chart_name,
      'statistic': statistic_name,
      'tir_label': tir_label,
      'story': story_filter,
      'start_repository': start_repository,
      'end_repository': end_repository,
      'start_git_hash': start_git_hash,
      'end_git_hash': end_git_hash,
      'bug_id': params['bug_id'],
      'auto_explore': '1',
      'target': target,
      'email': email,
      'name': job_name,
      'tags': json.dumps({
          'test_path': test_path,
          'alert': alert_key
      }),
  }

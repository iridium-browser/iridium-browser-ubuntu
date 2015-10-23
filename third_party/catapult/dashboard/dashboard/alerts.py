# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides the web interface for displaying an overview of alerts."""

import json

from google.appengine.ext import ndb

from dashboard import email_template
from dashboard import request_handler
from dashboard import utils
from dashboard.models import anomaly
from dashboard.models import sheriff
from dashboard.models import stoppage_alert

_MAX_ANOMALIES_TO_COUNT = 5000
_MAX_ANOMALIES_TO_SHOW = 500
_MAX_STOPPAGE_ALERTS = 500


class AlertsHandler(request_handler.RequestHandler):
  """Shows an overview of recent anomalies for perf sheriffing."""

  def get(self):
    """Renders the UI for listing alerts.

    Request parameters:
      sheriff: The name of a sheriff (optional).
      triaged: Whether to include triaged alerts (i.e. with a bug ID).
      improvements: Whether to include improvement anomalies.

    Outputs:
      A page displaying an overview table of all alerts.
    """
    sheriff_name = self.request.get('sheriff', 'Chromium Perf Sheriff')
    sheriff_key = ndb.Key('Sheriff', sheriff_name)
    include_improvements = bool(self.request.get('improvements'))
    include_triaged = bool(self.request.get('triaged'))

    anomaly_keys = _FetchAnomalyKeys(
        sheriff_key, include_improvements, include_triaged)
    anomalies = ndb.get_multi(anomaly_keys[:_MAX_ANOMALIES_TO_SHOW])
    stoppage_alerts = _FetchStoppageAlerts(sheriff_key, include_triaged)

    self.RenderHtml('alerts.html', {
        'anomaly_list': json.dumps(AnomalyDicts(anomalies)),
        'stoppage_alert_list': json.dumps(StoppageAlertDicts(stoppage_alerts)),
        'have_anomalies': bool(anomalies),
        'have_stoppage_alerts': bool(stoppage_alerts),
        'sheriff_list': json.dumps(_GetSheriffList()),
        'num_anomalies': len(anomaly_keys),
    })


def _FetchAnomalyKeys(sheriff_key, include_improvements, include_triaged):
  """Fetches the list of Anomaly keys that may be shown.

  Args:
    sheriff_key: The ndb.Key for the Sheriff to fetch alerts for.
    include_improvements: Whether to include improvement Anomalies.
    include_triaged: Whether to include Anomalies with a bug ID already set.

  Returns:
    A list of Anomaly keys, in reverse-chronological order.
  """
  query = anomaly.Anomaly.query(
      anomaly.Anomaly.sheriff == sheriff_key)

  if not include_improvements:
    query = query.filter(
        anomaly.Anomaly.is_improvement == False)

  if not include_triaged:
    query = query.filter(
        anomaly.Anomaly.bug_id == None)
    query = query.filter(
        anomaly.Anomaly.recovered == False)

  query = query.order(-anomaly.Anomaly.timestamp)
  return query.fetch(limit=_MAX_ANOMALIES_TO_COUNT, keys_only=True)


def _FetchStoppageAlerts(sheriff_key, include_triaged):
  """Fetches the list of Anomaly keys that may be shown.

  Args:
    sheriff_key: The ndb.Key for the Sheriff to fetch alerts for.
    include_triaged: Whether to include alerts with a bug ID.

  Returns:
    A list of StoppageAlert entities, in reverse-chronological order.
  """
  query = stoppage_alert.StoppageAlert.query(
      stoppage_alert.StoppageAlert.sheriff == sheriff_key)

  if not include_triaged:
    query = query.filter(
        stoppage_alert.StoppageAlert.bug_id == None)
    query = query.filter(
        stoppage_alert.StoppageAlert.recovered == False)

  query = query.order(-stoppage_alert.StoppageAlert.timestamp)
  return query.fetch(limit=_MAX_STOPPAGE_ALERTS)


def _GetSheriffList():
  """Returns a list of sheriff names for all sheriffs in the datastore."""
  sheriff_keys = sheriff.Sheriff.query().fetch(keys_only=True)
  return [key.string_id() for key in sheriff_keys]


def AnomalyDicts(anomalies):
  """Makes a list of dicts with properties of Anomaly entities."""
  bisect_statuses = _GetBisectStatusDict(anomalies)
  return [GetAnomalyDict(a, bisect_statuses.get(a.bug_id)) for a in anomalies]


def StoppageAlertDicts(stoppage_alerts):
  """Makes a list of dicts with properties of StoppageAlert entities."""
  return [_GetStoppageAlertDict(a) for a in stoppage_alerts]


def GetAnomalyDict(anomaly_entity, bisect_status=None):
  """Returns a dictionary for an Anomaly which can be encoded as JSON.

  Args:
    anomaly_entity: An Anomaly entity.
    bisect_status: String status of bisect run.

  Returns:
    A dictionary which is safe to be encoded as JSON.
  """
  alert_dict = _AlertDict(anomaly_entity)
  alert_dict.update({
      'median_after_anomaly': anomaly_entity.median_after_anomaly,
      'median_before_anomaly': anomaly_entity.median_before_anomaly,
      'percent_changed': '%s' % anomaly_entity.GetDisplayPercentChanged(),
      'improvement': anomaly_entity.is_improvement,
      'bisect_status': bisect_status,
      'recovered': anomaly_entity.recovered,
  })
  return alert_dict


def _GetStoppageAlertDict(stoppage_alert_entity):
  """Returns a dictionary of properties of a stoppage alert."""
  alert_dict = _AlertDict(stoppage_alert_entity)
  alert_dict.update({
      'mail_sent': stoppage_alert_entity.mail_sent,
      'last_row_date': str(stoppage_alert_entity.last_row_date.date()),
      'recovered': stoppage_alert_entity.recovered,
  })
  return alert_dict


def _AlertDict(alert_entity):
  """Returns a base dictionary with properties common to all alerts."""
  test_path = utils.TestPath(alert_entity.test)
  test_path_parts = test_path.split('/')
  dashboard_link = email_template.GetReportPageLink(
      test_path, rev=alert_entity.end_revision, add_protocol_and_host=False)
  return {
      'key': alert_entity.key.urlsafe(),
      'group': alert_entity.group.urlsafe() if alert_entity.group else None,
      'start_revision': alert_entity.start_revision,
      'end_revision': alert_entity.end_revision,
      'date': str(alert_entity.timestamp.date()),
      'master': test_path_parts[0],
      'bot': test_path_parts[1],
      'testsuite': test_path_parts[2],
      'test': '/'.join(test_path_parts[3:]),
      'bug_id': alert_entity.bug_id,
      'dashboard_link': dashboard_link,
  }


def _GetBisectStatusDict(anomalies):
  """Returns a dictionary of bug ID to bisect status string."""
  bug_id_list = {a.bug_id for a in anomalies if a.bug_id > 0}
  bugs = ndb.get_multi(ndb.Key('Bug', b) for b in bug_id_list)
  return {b.key.id(): b.latest_bisect_status for b in bugs if b}

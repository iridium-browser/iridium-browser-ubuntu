# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import calendar
import collections
import datetime


_Alert = collections.namedtuple('Alert', [
    'key', 'timestamp', 'test_suite', 'measurement', 'bot', 'test_case',
    'start_revision', 'end_revision',
    'median_before_anomaly', 'median_after_anomaly', 'units', 'improvement',
    'bug_id', 'bisect_status'])


def _IsoFormatStrToTimestamp(value):
  # Parse a string produced by datetime.isoformat(), assuming UTC time zone,
  # and convert to a Unix timestamp.
  d = datetime.datetime.strptime(value, '%Y-%m-%dT%H:%M:%S.%f')
  return calendar.timegm(d.timetuple())


class Alert(_Alert):
  __slots__ = ()
  name = 'alerts'
  columns = _Alert._fields

  @classmethod
  def FromJson(cls, data):
    kwargs = {k: data[k] for k in cls.columns if k in data}
    kwargs['timestamp'] = _IsoFormatStrToTimestamp(data['timestamp'])
    kwargs['test_suite'] = data['testsuite']
    kwargs['measurement'], kwargs['test_case'] = data['test'].split('/', 1)
    kwargs['bot'] = '/'.join([data['master'], data['bot']])
    return cls(**kwargs)

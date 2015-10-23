# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The database models for anomaly alerting threshold configs."""

import logging

from google.appengine.ext import ndb

# The parameters to use from anomaly threshold config dict.
# Any parameters in such a dict that aren't in this list will be ignored.
_VALID_ANOMALY_CONFIG_PARAMETERS = {
    'max_window_size',
    'multiple_of_std_dev',
    'min_relative_change',
    'min_absolute_change',
    'min_segment_size',
}


class AnomalyConfig(ndb.Model):
  """Represents a set of parameters for the anomaly detection function.

  The anomaly detection module uses set of parameters to determine the
  thresholds for what is considered an anomaly.
  """
  # A dictionary mapping parameter names to values.
  config = ndb.JsonProperty(required=True, indexed=False)

  # A list of test path patterns. Each pattern is a string which can match parts
  # of the test path either exactly, or use * as a wildcard.
  # Note: Test entities contain a key property called overridden_anomaly_config,
  # which is set in the pre-put hook for Test in graph_data.py.
  patterns = ndb.StringProperty(repeated=True, indexed=False)


def GetAnomalyConfigDict(test):
  """Gets the anomaly threshold config for the given test.

  Args:
    test: Test entity to get the config for.

  Returns:
    A dictionary with threshold parameters for the given test.
  """
  if not test.overridden_anomaly_config:
    return {}
  anomaly_config = test.overridden_anomaly_config.get()
  if not anomaly_config:
    logging.warning('No AnomalyConfig fetched from key %s for test %s',
                    test.overridden_anomaly_config, test.test_path)
    # The the overridden_anomaly_config property should be reset
    # in the pre-put hook of the Test entity.
    test.put()
    return {}
  config_dict = anomaly_config.config
  # In the config dict there may be extra "comment" parameters which
  # should be ignored.
  return {key: value for key, value in config_dict.iteritems()
          if key in _VALID_ANOMALY_CONFIG_PARAMETERS}

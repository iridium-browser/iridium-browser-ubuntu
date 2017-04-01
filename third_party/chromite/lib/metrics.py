# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper library around ts_mon.

This library provides some wrapper functionality around ts_mon, to make it more
friendly to developers. It also provides import safety, in case ts_mon is not
deployed with your code.
"""

from __future__ import print_function

import collections
import contextlib
import datetime
import ssl

from functools import wraps
from collections import namedtuple

from chromite.lib import cros_logging as logging

try:
  from infra_libs import ts_mon
# TODO(akeshet): AttributeError only needs to be caught while landing
# https://chromium-review.googlesource.com/#/c/359447/ , due to some issues in
# cbuildbot bootstrapping and backwards compatibility of ts_mon. I believe that
# after it lands, we no longer need to catch AttributeError here.
except (ImportError, RuntimeError, AttributeError):
  ts_mon = None


# This number is chosen because 1.16^100 seconds is about
# 32 days. This is a good compromise between bucket size
# and dynamic range.
_SECONDS_BUCKET_FACTOR = 1.16

# If none, we create metrics in this process. Otherwise, we send metrics via
# this Queue to a dedicated flushing processes.
MESSAGE_QUEUE = None

MetricCall = namedtuple(
    'MetricCall',
    'metric_name metric_args metric_kwargs '
    'method method_args method_kwargs '
    'reset_after')


class ProxyMetric(object):
  """Redirects any method calls to the message queue."""
  def __init__(self, metric, metric_args, metric_kwargs):
    self.metric = metric
    self.metric_args = metric_args
    self.reset_after = metric_kwargs.pop('reset_after', False)
    self.metric_kwargs = metric_kwargs

  def __getattr__(self, method_name):
    """Redirects all method calls to the MESSAGE_QUEUE."""
    def enqueue(*args, **kwargs):
      MESSAGE_QUEUE.put(MetricCall(
          metric_name=self.metric,
          metric_args=self.metric_args,
          metric_kwargs=self.metric_kwargs,
          method=method_name,
          method_args=args,
          method_kwargs=kwargs,
          reset_after=self.reset_after))
    return enqueue


def _Indirect(fn):
  """Decorates a function to be indirect If MESSAGE_QUEUE is set.

  If MESSAGE_QUEUE is set, the indirect function will return a proxy metrics
  object; otherwise, it behaves normally.
  """
  @wraps(fn)
  def AddToQueueIfPresent(*args, **kwargs):
    if MESSAGE_QUEUE:
      return ProxyMetric(fn.__name__, args, kwargs)
    else:
      # Whether to reset the metric after the flush; this is only used by
      # |ProxyMetric|, so remove this from the kwargs.
      kwargs.pop('reset_after', None)
      return fn(*args, **kwargs)
  return AddToQueueIfPresent


class MockMetric(object):
  """Mock metric object, to be returned if ts_mon is not set up."""

  def _mock_method(self, *args, **kwargs):
    pass

  def __getattr__(self, _):
    return self._mock_method


def _ImportSafe(fn):
  """Decorator which causes |fn| to return MockMetric if ts_mon not imported."""
  @wraps(fn)
  def wrapper(*args, **kwargs):
    if ts_mon:
      return fn(*args, **kwargs)
    else:
      return MockMetric()

  return wrapper


def _Metric(fn):
  """A pipeline of decorators to apply to our metric constructors."""
  return _ImportSafe(_Indirect(fn))

# This is needed for the reset_after flag used by @Indirect.
# pylint: disable=unused-argument

@_Metric
def Counter(name, reset_after=False):
  """Returns a metric handle for a counter named |name|."""
  return ts_mon.CounterMetric(name)


@_Metric
def Gauge(name, reset_after=False):
  """Returns a metric handle for a gauge named |name|."""
  return ts_mon.GaugeMetric(name)


@_Metric
def CumulativeMetric(name, reset_after=False):
  """Returns a metric handle for a cumulative float named |name|."""
  return ts_mon.CumulativeMetric(name)


@_Metric
def String(name, reset_after=False):
  """Returns a metric handle for a string named |name|."""
  return ts_mon.StringMetric(name)


@_Metric
def Boolean(name, reset_after=False):
  """Returns a metric handle for a boolean named |name|."""
  return ts_mon.BooleanMetric(name)


@_Metric
def Float(name, reset_after=False):
  """Returns a metric handle for a float named |name|."""
  return ts_mon.FloatMetric(name)


@_Metric
def CumulativeDistribution(name, reset_after=False):
  """Returns a metric handle for a cumulative distribution named |name|."""
  return ts_mon.CumulativeDistributionMetric(name)


@_Metric
def CumulativeSmallIntegerDistribution(name, reset_after=False):
  """Returns a metric handle for a cumulative distribution named |name|.

  This differs slightly from CumulativeDistribution, in that the underlying
  metric uses a uniform bucketer rather than a geometric one.

  This metric type is suitable for holding a distribution of numbers that are
  nonnegative integers in the range of 0 to 100.
  """
  return ts_mon.CumulativeDistributionMetric(
      name,
      bucketer=ts_mon.FixedWidthBucketer(1))

@_Metric
def SecondsDistribution(name, reset_after=False):
  """Returns a metric handle for a cumulative distribution named |name|.

  The distribution handle returned by this method is better suited than the
  default one for recording handling times, in seconds.

  This metric handle has bucketing that is optimized for time intervals
  (in seconds) in the range of 1 second to 32 days.
  """
  b = ts_mon.GeometricBucketer(growth_factor=_SECONDS_BUCKET_FACTOR)
  return ts_mon.CumulativeDistributionMetric(
      name, bucketer=b, units=ts_mon.MetricsDataUnits.SECONDS)

@_Metric
def PercentageDistribution(name, num_buckets=1000, reset_after=False):
  """Returns a metric handle for a cumulative distribution for percentage.

  The distribution handle returned by this method is better suited for reporting
  percentage values than the default one. The bucketing is optimized for values
  in [0,100].

  Args:
    name: The name of this metric.
    num_buckets: This metric buckets the percentage values before
        reporting. This argument controls the number of the bucket the range
        [0,100] is divided in. The default gives you 0.1% resolution.
    reset_after: Should the metric be reset after reporting.
  """
  # The last bucket actually covers [100, 100 + 1.0/num_buckets), so it
  # corresponds to values that exactly match 100%.
  bucket_width = 100.0 / num_buckets
  b = ts_mon.FixedWidthBucketer(bucket_width, num_buckets)
  return ts_mon.CumulativeDistributionMetric(name, bucketer=b)

@contextlib.contextmanager
def SecondsTimer(name, fields=None):
  """Record the time of an operation to a SecondsDistributionMetric.

  Records the time taken inside of the context block, to the
  SecondsDistribution named |name|, with the given fields.

  Usage:

  # Time the doSomething() call, with field values that are independent of the
  # results of the operation.
  with SecondsTimer('timer/name', fields={'foo': 'bar'}):
    doSomething()

  # Time the doSomethingElse call, with field values that depend on the results
  # of that operation. Note that it is important that a default value is
  # specified for these fields, in case an exception is thrown by
  # doSomethingElse()
  f = {'success': False, 'foo': 'bar'}
  with SecondsTimer('timer/name', fields=f) as c:
    doSomethingElse()
    c['success'] = True

  # Incorrect Usage!
  with SecondsTimer('timer/name') as c:
    doSomething()
    c['foo'] = bar # 'foo' is not a valid field, because no default
                   # value for it was specified in the context constructor.
                   # It will be silently ignored.
  """
  m = SecondsDistribution(name)
  f = fields or {}
  f = dict(f)
  keys = f.keys()
  t0 = datetime.datetime.now()
  yield f
  dt = (datetime.datetime.now() - t0).total_seconds()
  # Filter out keys that were not part of the initial key set. This is to avoid
  # inconsistent fields.
  # TODO(akeshet): Doing this filtering isn't super efficient. Would be better
  # to implement some key-restricted subclass or wrapper around dict, and just
  # yield that above rather than yielding a regular dict.
  f = {k: f[k] for k in keys}
  m.add(dt, fields=f)


def SecondsTimerDecorator(name, fields=None):
  """Decorator to time the duration of function calls.

  Usage:
    @SecondsTimerDecorator('timer/name', fields={'foo': 'bar'})
    def Foo(bar):
      return doStuff()

    is equivalent to

    def Foo(bar):
      with SecondsTimer('timer/name', fields={'foo': 'bar'})
        return doStuff()
  """
  def decorator(fn):
    @wraps(fn)
    def wrapper(*args, **kwargs):
      with SecondsTimer(name, fields):
        return fn(*args, **kwargs)

    return wrapper

  return decorator


class RuntimeBreakdownTimer(object):
  """Record the time of an operation and the breakdown into sub-steps.

  Usage:
    with RuntimeBreakdownTimer('timer/name', fields={'foo':'bar'}) as timer:
      with timer.Step('first_step'):
        doFirstStep()
      with timer.Step('second_step'):
        doSecondStep()
      # The time spent next will show up under .../timer/name/breakdown_no_step
      doSomeNonStepWork()

  This will emit the following metrics:
  - .../timer/name/total_duration - A SecondsDistribution metric for the time
        spent inside the outer with block.
  - .../timer/name/breakdown/first_step and
    .../timer/name/breakdown/second_step - PercentageDistribution metrics for
        the fraction of time devoted to each substep.
  - .../timer/name/breakdown_unaccounted - PercentageDistribution metric for the
        fraction of time that is not accounted for in any of the substeps.
  - .../timer/name/bucketing_loss - PercentageDistribution metric buckets values
        before reporting them as distributions. This causes small errors in the
        reported values because they are rounded to the reported buckets lower
        bound. This is a CumulativeMetric measuring the total rounding error
        accrued in reporting all the percentages. The worst case bucketing loss
        for x steps is (x+1)/10. So, if you time across 9 steps, you should
        expect no more than 1% rounding error.

  NB: This helper can only be used if the field values are known at the
  beginning of the outer context and do not change as a result of any of the
  operations timed.
  """

  PERCENT_BUCKET_COUNT = 1000

  _StepMetrics = collections.namedtuple('_StepMetrics', ['name', 'time_s'])

  def __init__(self, name, fields=None):
    self._name = name
    self._fields = fields
    self._outer_t0 = None
    self._total_time_s = 0
    self._inside_step = False
    self._step_metrics = []

  def __enter__(self):
    self._outer_t0 = datetime.datetime.now()
    return self

  def __exit__(self, _type, _value, _traceback):
    self._RecordTotalTime()

    outer_timer = SecondsDistribution('%s/total_duration' % (self._name,))
    outer_timer.add(self._total_time_s, fields=self._fields)

    for name, percent in self._GetStepBreakdowns().iteritems():
      step_metric = PercentageDistribution(
          '%s/breakdown/%s' % (self._name, name),
          num_buckets=self.PERCENT_BUCKET_COUNT)
      step_metric.add(percent, fields=self._fields)

    unaccounted_metric = PercentageDistribution(
        '%s/breakdown_unaccounted' % self._name,
        num_buckets=self.PERCENT_BUCKET_COUNT)
    unaccounted_metric.add(self._GetUnaccountedBreakdown(), fields=self._fields)

    bucketing_loss_metric = CumulativeMetric('%s/bucketing_loss' % self._name)
    bucketing_loss_metric.increment_by(self._GetBucketingLoss(),
                                       fields=self._fields)

  @contextlib.contextmanager
  def Step(self, step_name):
    """Start a new step named step_name in the timed operation.

    Note that it is not possible to start a step inside a step. i.e.,

    with RuntimeBreakdownTimer('timer') as timer:
      with timer.Step('outer_step'):
        with timer.Step('inner_step'):
          # will by design raise an exception.

    Args:
      step_name: The name of the step being timed.
    """
    if self._inside_step:
      logging.error('RuntimeBreakdownTimer.Step is not reentrant. '
                    'Dropping step: %s', step_name)
      yield
      return

    self._inside_step = True
    t0 = datetime.datetime.now()
    try:
      yield
    finally:
      self._inside_step = False
      step_time_s = (datetime.datetime.now() - t0).total_seconds()
      self._step_metrics.append(self._StepMetrics(step_name, step_time_s))

  def _GetStepBreakdowns(self):
    """Returns percentage of time spent in each step.

    Must be called after |_RecordTotalTime|.
    """
    if not self._total_time_s:
      return {}
    return {x.name: (x.time_s * 100.0) / self._total_time_s
            for x in self._step_metrics}

  def _GetUnaccountedBreakdown(self):
    """Returns the percentage time spent outside of all steps.

    Must be called after |_RecordTotalTime|.
    """
    breakdown_percentages = sum(self._GetStepBreakdowns().itervalues())
    return max(0, 100 - breakdown_percentages)

  def _GetBucketingLoss(self):
    """Compute the actual loss in reported percentages due to bucketing.

    Must be called after |_RecordTotalTime|.
    """
    reported = self._GetStepBreakdowns().values()
    reported.append(self._GetUnaccountedBreakdown())
    bucket_width = 100.0 / self.PERCENT_BUCKET_COUNT
    return sum(x % bucket_width for x in reported)

  def _RecordTotalTime(self):
    self._total_time_s = (
        datetime.datetime.now() - self._outer_t0).total_seconds()


def Flush(reset_after=()):
  """Flushes metrics, but warns on transient errors.

  Args:
    reset_after: A list of metrics to reset after flushing.
  """
  if not ts_mon:
    return

  try:
    ts_mon.flush()
    while reset_after:
      reset_after.pop().reset()
  except ssl.SSLError as e:
    logging.warning('Caught transient network error while flushing: %s', e)
  except Exception as e:
    logging.error('Caught exception while flushing: %s', e)

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_test
from telemetry.timeline.model import TimelineModel
from telemetry.timeline import tracing_category_filter
from telemetry.timeline import tracing_options
from telemetry.util import statistics
from telemetry.value import scalar


class V8GCTimes(page_test.PageTest):

  _TIME_OUT_IN_SECONDS = 60
  _CATEGORIES = ['blink.console',
                 'renderer.scheduler',
                 'toplevel',
                 'v8',
                 'webkit.console']
  _RENDERER_MAIN_THREAD = 'CrRendererMain'
  _IDLE_TASK_PARENT = 'SingleThreadIdleTaskRunner::RunTask'

  def __init__(self):
    super(V8GCTimes, self).__init__()

  def WillNavigateToPage(self, page, tab):
    category_filter = tracing_category_filter.TracingCategoryFilter()

    for category in self._CATEGORIES:
      category_filter.AddIncludedCategory(category)

    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True

    tab.browser.platform.tracing_controller.Start(
        options, category_filter, self._TIME_OUT_IN_SECONDS)

  def ValidateAndMeasurePage(self, page, tab, results):
    trace_data = tab.browser.platform.tracing_controller.Stop()
    timeline_model = TimelineModel(trace_data)
    renderer_process = timeline_model.GetRendererProcessFromTabId(tab.id)
    self._AddV8MetricsToResults(renderer_process, results)

  def DidRunPage(self, platform):
    if platform.tracing_controller.is_tracing_running:
      platform.tracing_controller.Stop()

  def _AddV8MetricsToResults(self, process, results):
    if process is None:
      return

    for thread in process.threads.values():
      if thread.name != self._RENDERER_MAIN_THREAD:
        continue

      self._AddV8EventStatsToResults(thread, results)
      self._AddCpuTimeStatsToResults(thread, results)

  def _AddV8EventStatsToResults(self, thread, results):
    v8_event_stats = [
        V8EventStat('V8.GCIncrementalMarking',
                    'v8_gc_incremental_marking',
                    'incremental marking steps'),
        V8EventStat('V8.GCScavenger',
                    'v8_gc_scavenger',
                    'scavenges'),
        V8EventStat('V8.GCCompactor',
                    'v8_gc_mark_compactor',
                    'mark-sweep-compactor')]
    # Find all V8 GC events in the trace.
    for event in thread.IterAllSlices():
      event_stat = _FindV8EventStatForEvent(v8_event_stats, event.name)
      if not event_stat:
        continue

      event_stat.thread_duration += event.thread_duration

      parent_idle_task = _ParentIdleTask(event)
      if parent_idle_task:
        allotted_idle_time = parent_idle_task.args['allotted_time_ms']
        idle_task_wall_overrun = 0
        if event.duration > allotted_idle_time:
          idle_task_wall_overrun = event.duration - allotted_idle_time
        # Don't count time over the deadline as being inside idle time.
        # Since the deadline should be relative to wall clock we compare
        # allotted_time_ms with wall duration instead of thread duration, and
        # then assume the thread duration was inside idle for the same
        # percentage of time.
        inside_idle = event.thread_duration * statistics.DivideIfPossibleOrZero(
            event.duration - idle_task_wall_overrun, event.duration)
        event_stat.thread_duration_inside_idle += inside_idle
        event_stat.idle_task_overrun_duration += idle_task_wall_overrun

    for v8_event_stat in v8_event_stats:
      results.AddValue(scalar.ScalarValue(
          results.current_page, v8_event_stat.result_name, 'ms',
          v8_event_stat.thread_duration,
          description=('Total thread duration spent in %s' %
                       v8_event_stat.result_description)))
      results.AddValue(scalar.ScalarValue(results.current_page,
          '%s_outside_idle' % v8_event_stat.result_name, 'ms',
          v8_event_stat.thread_duration_outside_idle,
          description=(
              'Total thread duration spent in %s outside of idle tasks' %
              v8_event_stat.result_description)))
      results.AddValue(scalar.ScalarValue(results.current_page,
          '%s_idle_deadline_overrun' % v8_event_stat.result_name, 'ms',
          v8_event_stat.idle_task_overrun_duration,
          description=('Total idle task deadline overrun for %s idle tasks'
                       % v8_event_stat.result_description)))
      results.AddValue(scalar.ScalarValue(results.current_page,
          '%s_percentage_idle' % v8_event_stat.result_name, 'idle%',
          v8_event_stat.percentage_thread_duration_during_idle,
          description=('Percentage of %s spent in idle time' %
                       v8_event_stat.result_description)))

    # Add total metrics.
    gc_total = sum(x.thread_duration for x in v8_event_stats)
    gc_total_outside_idle = sum(
        x.thread_duration_outside_idle for x in v8_event_stats)
    gc_total_idle_deadline_overrun = sum(
        x.idle_task_overrun_duration for x in v8_event_stats)
    gc_total_percentage_idle = statistics.DivideIfPossibleOrZero(
        100 * (gc_total - gc_total_outside_idle), gc_total)

    results.AddValue(scalar.ScalarValue(results.current_page,
        'v8_gc_total', 'ms', gc_total,
        description='Total thread duration of all garbage collection events'))
    results.AddValue(scalar.ScalarValue(results.current_page,
        'v8_gc_total_outside_idle', 'ms', gc_total_outside_idle,
        description=(
            'Total thread duration of all garbage collection events outside of '
            'idle tasks')))
    results.AddValue(scalar.ScalarValue(results.current_page,
        'v8_gc_total_idle_deadline_overrun', 'ms',
        gc_total_idle_deadline_overrun,
        description=(
            'Total idle task deadline overrun for all idle tasks garbage '
            'collection events')))
    results.AddValue(scalar.ScalarValue(results.current_page,
        'v8_gc_total_percentage_idle', 'idle%', gc_total_percentage_idle,
        description=(
            'Percentage of the thread duration of all garbage collection '
            'events spent inside of idle tasks')))

  def _AddCpuTimeStatsToResults(self, thread, results):
    if thread.toplevel_slices:
      start_time = min(s.start for s in thread.toplevel_slices)
      end_time = max(s.end for s in thread.toplevel_slices)
      duration = end_time - start_time
      cpu_time = sum(s.thread_duration for s in thread.toplevel_slices)
    else:
      duration = cpu_time = 0

    results.AddValue(scalar.ScalarValue(
        results.current_page, 'duration', 'ms', duration))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'cpu_time', 'ms', cpu_time))


def _FindV8EventStatForEvent(v8_event_stats_list, event_name):
  for v8_event_stat in v8_event_stats_list:
    if v8_event_stat.src_event_name == event_name:
      return v8_event_stat
  return None


def _ParentIdleTask(event):
  parent = event.parent_slice
  while parent:
    # pylint: disable=protected-access
    if parent.name == V8GCTimes._IDLE_TASK_PARENT:
      return parent
    parent = parent.parent_slice
  return None


class V8EventStat(object):

  def __init__(self, src_event_name, result_name, result_description):
    self.src_event_name = src_event_name
    self.result_name = result_name
    self.result_description = result_description
    self.thread_duration = 0.0
    self.thread_duration_inside_idle = 0.0
    self.idle_task_overrun_duration = 0.0

  @property
  def thread_duration_outside_idle(self):
    return self.thread_duration - self.thread_duration_inside_idle

  @property
  def percentage_thread_duration_during_idle(self):
    return statistics.DivideIfPossibleOrZero(
        100 * self.thread_duration_inside_idle, self.thread_duration)

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import v8_detached_context_age_in_gc
from measurements import v8_gc_times
import page_sets
from telemetry import benchmark


# Disabled on Win due to crbug.com/416502.
# Disabled on reference due to crbug.com/507836.
@benchmark.Disabled('win', 'reference')
class V8Top25(perf_benchmark.PerfBenchmark):
  """Measures V8 GC metrics on the while scrolling down the top 25 web pages.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = v8_gc_times.V8GCTimes
  page_set = page_sets.Top25SmoothPageSet

  @classmethod
  def Name(cls):
    return 'v8.top_25_smooth'

@benchmark.Enabled('android')
class V8KeyMobileSites(perf_benchmark.PerfBenchmark):
  """Measures V8 GC metrics on the while scrolling down key mobile sites.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = v8_gc_times.V8GCTimes
  page_set = page_sets.KeyMobileSitesSmoothPageSet

  @classmethod
  def Name(cls):
    return 'v8.key_mobile_sites_smooth'

@benchmark.Disabled('mac', 'win')  # crbug.com/514198
class V8DetachedContextAgeInGC(perf_benchmark.PerfBenchmark):
  """Measures the number of GCs needed to collect a detached context.

  http://www.chromium.org/developers/design-documents/rendering-benchmarks"""
  test = v8_detached_context_age_in_gc.V8DetachedContextAgeInGC
  page_set = page_sets.PageReloadCasesPageSet

  @classmethod
  def Name(cls):
    return 'v8.detached_context_age_in_gc'

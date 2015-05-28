# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import benchmark

from measurements import startup
import page_sets


class _StartupCold(benchmark.Benchmark):
  """Measures cold startup time with a clean profile."""
  options = {'pageset_repeat': 5}

  @classmethod
  def Name(cls):
    return 'startup'

  def CreatePageTest(self, options):
    return startup.Startup(cold=True)


class _StartupWarm(benchmark.Benchmark):
  """Measures warm startup time with a clean profile."""
  options = {'pageset_repeat': 20}

  @classmethod
  def Name(cls):
    return 'startup'

  @classmethod
  def ValueCanBeAddedPredicate(cls, _, is_first_result):
    return not is_first_result

  def CreatePageTest(self, options):
    return startup.Startup(cold=False)


@benchmark.Enabled('has tabs')
@benchmark.Disabled('snowleopard') # crbug.com/336913
class StartupColdBlankPage(_StartupCold):
  """Measures cold startup time with a clean profile."""
  tag = 'cold'
  page_set = page_sets.BlankPageSet

  @classmethod
  def Name(cls):
    return 'startup.cold.blank_page'


@benchmark.Enabled('has tabs')
class StartupWarmBlankPage(_StartupWarm):
  """Measures warm startup time with a clean profile."""
  tag = 'warm'
  page_set = page_sets.BlankPageSet

  @classmethod
  def Name(cls):
    return 'startup.warm.blank_page'

@benchmark.Enabled('has tabs')
@benchmark.Enabled('win', 'linux', 'mac')
class StartupLargeProfileColdBlankPage(_StartupCold):
  """Measures cold startup time with a large profile."""
  tag = 'cold'
  page_set = page_sets.BlankPageSet
  options = {'pageset_repeat': 1}

  def __init__(self, max_failures=None):
    super(StartupLargeProfileColdBlankPage, self).__init__(max_failures)
    self.generated_profile_archive = "large_profile.zip"

  @classmethod
  def Name(cls):
    return 'startup.large_profile.cold.blank_page'

@benchmark.Enabled('has tabs')
@benchmark.Enabled('win', 'linux', 'mac')
class StartupLargeProfileWarmBlankPage(_StartupWarm):
  """Measures warm startup time with a large profile."""
  tag = 'warm'
  page_set = page_sets.BlankPageSet
  options = {'pageset_repeat': 1}

  def __init__(self, max_failures=None):
    super(StartupLargeProfileWarmBlankPage, self).__init__(max_failures)
    self.generated_profile_archive = "large_profile.zip"

  @classmethod
  def Name(cls):
    return 'startup.large_profile.warm.blank_page'

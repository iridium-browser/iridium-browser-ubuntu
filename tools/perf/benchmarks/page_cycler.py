# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from measurements import page_cycler
import page_sets
from telemetry import benchmark


class _PageCycler(perf_benchmark.PerfBenchmark):
  options = {'pageset_repeat': 6}
  cold_load_percent = 50  # % of page visits for which a cold load is forced

  @classmethod
  def Name(cls):
    return 'page_cycler'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_option('--report-speed-index',
        action='store_true',
        help='Enable the speed index metric.')

  @classmethod
  def ValueCanBeAddedPredicate(cls, _, is_first_result):
    return cls.cold_load_percent > 0 or not is_first_result

  def CreatePageTest(self, options):
    return page_cycler.PageCycler(
        page_repeat = options.page_repeat,
        pageset_repeat = options.pageset_repeat,
        cold_load_percent = self.cold_load_percent,
        report_speed_index = options.report_speed_index)


# This is an old page set, we intend to remove it after more modern benchmarks
# work on CrOS.
@benchmark.Enabled('chromeos')
class PageCyclerDhtml(_PageCycler):
  """Benchmarks for various DHTML operations like simple animations."""
  page_set = page_sets.DhtmlPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.dhtml'


class PageCyclerIntlArFaHe(_PageCycler):
  """Page load time for a variety of pages in Arabic, Farsi and Hebrew.

  Runs against pages recorded in April, 2013.
  """
  page_set = page_sets.IntlArFaHePageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.intl_ar_fa_he'


@benchmark.Disabled('win')  # crbug.com/366715
class PageCyclerIntlEsFrPtBr(_PageCycler):
  """Page load time for a pages in Spanish, French and Brazilian Portuguese.

  Runs against pages recorded in April, 2013.
  """
  page_set = page_sets.IntlEsFrPtBrPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.intl_es_fr_pt-BR'


class PageCyclerIntlHiRu(_PageCycler):
  """Page load time benchmark for a variety of pages in Hindi and Russian.

  Runs against pages recorded in April, 2013.
  """
  page_set = page_sets.IntlHiRuPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.intl_hi_ru'


@benchmark.Disabled('android', 'win')  # crbug.com/379564, crbug.com/434366
class PageCyclerIntlJaZh(_PageCycler):
  """Page load time benchmark for a variety of pages in Japanese and Chinese.

  Runs against pages recorded in April, 2013.
  """
  page_set = page_sets.IntlJaZhPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.intl_ja_zh'


@benchmark.Disabled('xp', 'android')  # crbug.com/434366, crbug.com/506903
class PageCyclerIntlKoThVi(_PageCycler):
  """Page load time for a variety of pages in Korean, Thai and Vietnamese.

  Runs against pages recorded in April, 2013.
  """
  page_set = page_sets.IntlKoThViPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.intl_ko_th_vi'


class PageCyclerMorejs(_PageCycler):
  """Page load for a variety of pages that were JavaScript heavy in 2009."""
  page_set = page_sets.MorejsPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.morejs'


# This is an old page set, we intend to remove it after more modern benchmarks
# work on CrOS.
@benchmark.Enabled('chromeos')
class PageCyclerMoz(_PageCycler):
  """Page load for mozilla's original page set. Recorded in December 2000."""
  page_set = page_sets.MozPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.moz'


# Win, mac, linux: crbug.com/353260
# Android: crbug.com/473161
@benchmark.Disabled('linux', 'win', 'mac', 'android')
class PageCyclerNetsimTop10(_PageCycler):
  """Measures load time of the top 10 sites under simulated cable network.

  Recorded in June, 2013.  Pages are loaded under the simplisticly simulated
  bandwidth and RTT constraints of a cable modem (5Mbit/s down, 1Mbit/s up,
  28ms RTT). Contention is realistically simulated, but slow start is not.
  DNS lookups are 'free'.
  """
  tag = 'netsim'
  page_set = page_sets.Top10PageSet
  options = {
      'extra_wpr_args_as_string': '--shaping_type=proxy --net=cable',
      'pageset_repeat': 6,
  }
  cold_load_percent = 100

  @classmethod
  def Name(cls):
    return 'page_cycler.netsim.top_10'

  def CreatePageTest(self, options):
    return page_cycler.PageCycler(
        page_repeat = options.page_repeat,
        pageset_repeat = options.pageset_repeat,
        cold_load_percent = self.cold_load_percent,
        report_speed_index = options.report_speed_index,
        clear_cache_before_each_run = True)

@benchmark.Enabled('android')
class PageCyclerTop10Mobile(_PageCycler):
  """Page load time benchmark for the top 10 mobile web pages.

  Runs against pages recorded in November, 2013.
  """

  @classmethod
  def Name(cls):
    return 'page_cycler.top_10_mobile'

  def CreateStorySet(self, options):
    # Disable the taobao.com page since it's crashing. crbug.com/509690
    stories = page_sets.Top10MobilePageSet(run_no_page_interactions=True)
    found = next((x for x in stories if 'taobao.com' in x.url), None)
    if found:
      stories.RemoveStory(found)
    return stories

@benchmark.Disabled
class PageCyclerKeyMobileSites(_PageCycler):
  """Page load time benchmark for key mobile sites."""
  page_set = page_sets.KeyMobileSitesPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.key_mobile_sites_smooth'


@benchmark.Disabled('android')  # crbug.com/357326
class PageCyclerToughLayoutCases(_PageCycler):
  """Page loading for the slowest layouts observed in the Alexa top 1 million.

  Recorded in July 2013.
  """
  page_set = page_sets.ToughLayoutCasesPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.tough_layout_cases'


# crbug.com/273986: This test is flakey on Windows Chrome.
@benchmark.Enabled('android', 'chromeos', 'linux', 'ios', 'mac',
                   'mandoline-release', 'mandoline-debug')
class PageCyclerTypical25(_PageCycler):
  """Page load time benchmark for a 25 typical web pages.

  Designed to represent typical, not highly optimized or highly popular web
  sites. Runs against pages recorded in June, 2014.
  """

  @classmethod
  def Name(cls):
    return 'page_cycler.typical_25'

  def CreateStorySet(self, options):
    return page_sets.Typical25PageSet(run_no_page_interactions=True)


@benchmark.Disabled('reference', 'android')
class PageCyclerBasicOopifIsolated(_PageCycler):
  """ A benchmark measuring performance of out-of-process iframes. """
  page_set = page_sets.OopifBasicPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler_site_isolation.basic_oopif'

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(['--site-per-process'])


@benchmark.Disabled('reference')  # crbug.com/523346
class PageCyclerBasicOopif(_PageCycler):
  """ A benchmark measuring performance of the out-of-process iframes page
  set, without running in out-of-process iframes mode.. """
  page_set = page_sets.OopifBasicPageSet

  @classmethod
  def Name(cls):
    return 'page_cycler.basic_oopif'


@benchmark.Disabled  # crbug.com/443730
class PageCyclerBigJs(_PageCycler):
  page_set = page_sets.BigJsPageSet
  @classmethod
  def Name(cls):
    return 'page_cycler.big_js'

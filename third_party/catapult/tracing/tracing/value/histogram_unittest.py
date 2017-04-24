# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import math
import unittest

from tracing.value import histogram


class PercentToStringUnittest(unittest.TestCase):
  def testPercentToString(self):
    with self.assertRaises(Exception) as ex:
      histogram.PercentToString(-1)
    self.assertEqual(ex.exception.message, 'percent must be in [0,1]')

    with self.assertRaises(Exception) as ex:
      histogram.PercentToString(2)
    self.assertEqual(ex.exception.message, 'percent must be in [0,1]')

    self.assertEqual(histogram.PercentToString(0), '000')
    self.assertEqual(histogram.PercentToString(1), '100')

    with self.assertRaises(Exception) as ex:
      histogram.PercentToString(float('nan'))
    self.assertEqual(ex.exception.message, 'Unexpected percent')

    self.assertEqual(histogram.PercentToString(0.50), '050')
    self.assertEqual(histogram.PercentToString(0.95), '095')


class StatisticsUnittest(unittest.TestCase):
  def testFindHighIndexInSortedArray(self):
    self.assertEqual(histogram.FindHighIndexInSortedArray(
        range(0, -10, -1), lambda x: x + 5), 6)

  def testUniformlySampleArray(self):
    self.assertEqual(len(histogram.UniformlySampleArray(
        range(10), 5)), 5)

  def testUniformlySampleStream(self):
    samples = []
    histogram.UniformlySampleStream(samples, 1, 'A', 5)
    self.assertEqual(samples, ['A'])
    histogram.UniformlySampleStream(samples, 2, 'B', 5)
    histogram.UniformlySampleStream(samples, 3, 'C', 5)
    histogram.UniformlySampleStream(samples, 4, 'D', 5)
    histogram.UniformlySampleStream(samples, 5, 'E', 5)
    self.assertEqual(samples, ['A', 'B', 'C', 'D', 'E'])
    histogram.UniformlySampleStream(samples, 6, 'F', 5)
    self.assertEqual(len(samples), 5)

    samples = [0, 0, 0]
    histogram.UniformlySampleStream(samples, 1, 'G', 5)
    self.assertEqual(samples, ['G', 0, 0])

  def testMergeSampledStreams(self):
    samples = []
    histogram.MergeSampledStreams(samples, 0, ['A'], 1, 5)
    self.assertEqual(samples, ['A'])
    histogram.MergeSampledStreams(samples, 1, ['B', 'C', 'D', 'E'], 4, 5)
    self.assertEqual(samples, ['A', 'B', 'C', 'D', 'E'])
    histogram.MergeSampledStreams(samples, 9, ['F', 'G', 'H', 'I', 'J'], 7, 5)
    self.assertEqual(len(samples), 5)


class RangeUnittest(unittest.TestCase):
  def testAddValue(self):
    r = histogram.Range()
    self.assertEqual(r.empty, True)
    r.AddValue(1)
    self.assertEqual(r.empty, False)
    self.assertEqual(r.min, 1)
    self.assertEqual(r.max, 1)
    self.assertEqual(r.center, 1)
    r.AddValue(2)
    self.assertEqual(r.empty, False)
    self.assertEqual(r.min, 1)
    self.assertEqual(r.max, 2)
    self.assertEqual(r.center, 1.5)


class RunningStatisticsUnittest(unittest.TestCase):
  def _Run(self, data):
    running = histogram.RunningStatistics()
    for datum in data:
      running.Add(datum)
    return running

  def testStatistics(self):
    running = self._Run([1, 2, 3])
    self.assertEqual(running.sum, 6)
    self.assertEqual(running.mean, 2)
    self.assertEqual(running.min, 1)
    self.assertEqual(running.max, 3)
    self.assertEqual(running.variance, 1)
    self.assertEqual(running.stddev, 1)
    self.assertEqual(running.geometric_mean, math.pow(6, 1./3))
    self.assertEqual(running.count, 3)

    running = self._Run([2, 4, 4, 2])
    self.assertEqual(running.sum, 12)
    self.assertEqual(running.mean, 3)
    self.assertEqual(running.min, 2)
    self.assertEqual(running.max, 4)
    self.assertEqual(running.variance, 4./3)
    self.assertEqual(running.stddev, math.sqrt(4./3))
    self.assertAlmostEqual(running.geometric_mean, math.pow(64, 1./4))
    self.assertEqual(running.count, 4)

  def testMerge(self):
    def Compare(data1, data2):
      a_running = self._Run(data1 + data2)
      b_running = self._Run(data1).Merge(self._Run(data2))
      CompareRunningStatistics(a_running, b_running)
      a_running = histogram.RunningStatistics.FromDict(a_running.AsDict())
      CompareRunningStatistics(a_running, b_running)
      b_running = histogram.RunningStatistics.FromDict(b_running.AsDict())
      CompareRunningStatistics(a_running, b_running)

    def CompareRunningStatistics(a_running, b_running):
      self.assertEqual(a_running.sum, b_running.sum)
      self.assertEqual(a_running.mean, b_running.mean)
      self.assertEqual(a_running.min, b_running.min)
      self.assertEqual(a_running.max, b_running.max)
      self.assertAlmostEqual(a_running.variance, b_running.variance)
      self.assertAlmostEqual(a_running.stddev, b_running.stddev)
      self.assertAlmostEqual(a_running.geometric_mean, b_running.geometric_mean)
      self.assertEqual(a_running.count, b_running.count)

    Compare([], [])
    Compare([], [1, 2, 3])
    Compare([1, 2, 3], [])
    Compare([1, 2, 3], [10, 20, 100])
    Compare([1, 1, 1, 1, 1], [10, 20, 10, 40])


def ToJSON(x):
  return json.dumps(x, separators=(',', ':'))


class HistogramUnittest(unittest.TestCase):
  TEST_BOUNDARIES = histogram.HistogramBinBoundaries.CreateLinear(0, 1000, 10)

  def assertDeepEqual(self, a, b):
    self.assertEqual(ToJSON(a), ToJSON(b))

  def testSerializationSize(self):
    hist = histogram.Histogram('', 'unitless', self.TEST_BOUNDARIES)
    d = hist.AsDict()
    self.assertEqual(107, len(ToJSON(d)))
    self.assertIsNone(d.get('allBins'))
    self.assertDeepEqual(d, histogram.Histogram.FromDict(d).AsDict())

    hist.AddSample(100)
    d = hist.AsDict()
    self.assertEqual(198, len(ToJSON(d)))
    self.assertIsInstance(d['allBins'], dict)
    self.assertDeepEqual(d, histogram.Histogram.FromDict(d).AsDict())

    hist.AddSample(100)
    d = hist.AsDict()
    # SAMPLE_VALUES grew by "100,"
    self.assertEqual(202, len(ToJSON(d)))
    self.assertIsInstance(d['allBins'], dict)
    self.assertDeepEqual(d, histogram.Histogram.FromDict(d).AsDict())

    hist.AddSample(271, {'foo': histogram.Generic('bar')})
    d = hist.AsDict()
    self.assertEqual(262, len(ToJSON(d)))
    self.assertIsInstance(d['allBins'], dict)
    self.assertDeepEqual(d, histogram.Histogram.FromDict(d).AsDict())

    # Add samples to most bins so that allBinsArray is more efficient than
    # allBinsDict.
    for i in xrange(10, 100):
      hist.AddSample(10 * i)
    d = hist.AsDict()
    self.assertEqual(691, len(ToJSON(d)))
    self.assertIsInstance(d['allBins'], list)
    self.assertDeepEqual(d, histogram.Histogram.FromDict(d).AsDict())

    # Lowering maxNumSampleValues takes a random sub-sample of the existing
    # sampleValues. We have deliberately set all samples to 3-digit numbers so
    # that the serialized size is constant regardless of which samples are
    # retained.
    hist.max_num_sample_values = 10
    d = hist.AsDict()
    self.assertEqual(383, len(ToJSON(d)))
    self.assertIsInstance(d['allBins'], list)
    self.assertDeepEqual(d, histogram.Histogram.FromDict(d).AsDict())

  def testBasic(self):
    hist = histogram.Histogram('', 'unitless', self.TEST_BOUNDARIES)
    self.assertEqual(hist.GetBinForValue(250).range.min, 200)
    self.assertEqual(hist.GetBinForValue(250).range.max, 300)

    hist.AddSample(-1)
    hist.AddSample(0)
    hist.AddSample(0)
    hist.AddSample(500)
    hist.AddSample(999)
    hist.AddSample(1000)
    self.assertEqual(hist.bins[0].count, 1)

    self.assertEqual(hist.GetBinForValue(0).count, 2)
    self.assertEqual(hist.GetBinForValue(500).count, 1)
    self.assertEqual(hist.GetBinForValue(999).count, 1)
    self.assertEqual(hist.bins[-1].count, 1)
    self.assertEqual(hist.num_values, 6)
    self.assertAlmostEqual(hist.average, 416.3333333)

  def testNans(self):
    hist = histogram.Histogram('', 'unitless', self.TEST_BOUNDARIES)
    hist.AddSample(None)
    hist.AddSample(float('nan'))
    self.assertEqual(hist.num_nans, 2)

  def testAddHistogramValid(self):
    hist0 = histogram.Histogram('', 'unitless', self.TEST_BOUNDARIES)
    hist1 = histogram.Histogram('', 'unitless', self.TEST_BOUNDARIES)
    hist0.AddSample(0)
    hist0.AddSample(None)
    hist1.AddSample(1)
    hist1.AddSample(float('nan'))
    hist0.AddHistogram(hist1)
    self.assertEqual(hist0.num_nans, 2)
    self.assertEqual(hist0.GetBinForValue(0).count, 2)

  def testAddHistogramInvalid(self):
    hist0 = histogram.Histogram(
        '', 'ms', histogram.HistogramBinBoundaries.CreateLinear(0, 1000, 10))
    hist1 = histogram.Histogram(
        '', 'unitless', histogram.HistogramBinBoundaries.CreateLinear(
            0, 1000, 10))
    hist2 = histogram.Histogram(
        '', 'ms', histogram.HistogramBinBoundaries.CreateLinear(0, 1001, 10))
    hist3 = histogram.Histogram(
        '', 'ms', histogram.HistogramBinBoundaries.CreateLinear(0, 1000, 11))
    hists = [hist0, hist1, hist2, hist3]
    for hista in hists:
      for histb in hists:
        if hista is histb:
          continue
        self.assertFalse(hista.CanAddHistogram(histb))
        with self.assertRaises(Exception):
          hista.AddHistogram(histb)

  def testPercentile(self):
    def Check(ary, mn, mx, bins, precision):
      boundaries = histogram.HistogramBinBoundaries.CreateLinear(mn, mx, bins)
      hist = histogram.Histogram('', 'ms', boundaries)
      for x in ary:
        hist.AddSample(x)
      for percent in [0.25, 0.5, 0.75, 0.8, 0.95, 0.99]:
        self.assertLessEqual(
            abs(histogram.Percentile(ary, percent) -
                hist.GetApproximatePercentile(percent)), precision)
    Check([1, 2, 5, 7], 0.5, 10.5, 10, 1e-3)
    Check([3, 3, 4, 4], 0.5, 10.5, 10, 1e-3)
    Check([1, 10], 0.5, 10.5, 10, 1e-3)
    Check([1, 2, 3, 4, 5], 0.5, 10.5, 10, 1e-3)
    Check([3, 3, 3, 3, 3], 0.5, 10.5, 10, 1e-3)
    Check([1, 2, 3, 4, 5, 6, 7, 8, 9, 10], 0.5, 10.5, 10, 1e-3)
    Check([1, 2, 3, 4, 5, 5, 6, 7, 8, 9, 10], 0.5, 10.5, 10, 1e-3)
    Check([0, 11], 0.5, 10.5, 10, 1)
    Check([0, 6, 11], 0.5, 10.5, 10, 1)
    array = []
    for i in xrange(1000):
      array.append((i * i) % 10 + 1)
    Check(array, 0.5, 10.5, 10, 1e-3)
    # If the real percentile is outside the bin range then the approximation
    # error can be high.
    Check([-10000], 0, 10, 10, 10000)
    Check([10000], 0, 10, 10, 10000 - 10)
    # The result is no more than the bin width away from the real percentile.
    Check([1, 1], 0, 10, 1, 10)

  def _CheckBoundaries(self, boundaries, expected_min_boundary,
                       expected_max_boundary, expected_bin_ranges):
    self.assertEqual(boundaries.range.min, expected_min_boundary)
    self.assertEqual(boundaries.range.max, expected_max_boundary)

    # Check that the boundaries can be used multiple times.
    for _ in xrange(3):
      hist = histogram.Histogram('', 'unitless', boundaries)
      self.assertEqual(len(expected_bin_ranges), len(hist.bins))
      for j, hbin in enumerate(hist.bins):
        self.assertAlmostEqual(hbin.range.min, expected_bin_ranges[j].min)
        self.assertAlmostEqual(hbin.range.max, expected_bin_ranges[j].max)

  def testAddBinBoundary(self):
    b = histogram.HistogramBinBoundaries(-100)
    b.AddBinBoundary(50)
    self._CheckBoundaries(b, -100, 50, [
        histogram.Range.FromExplicitRange(-histogram.JS_MAX_VALUE, -100),
        histogram.Range.FromExplicitRange(-100, 50),
        histogram.Range.FromExplicitRange(50, histogram.JS_MAX_VALUE),
    ])

    b.AddBinBoundary(60)
    b.AddBinBoundary(75)
    self._CheckBoundaries(b, -100, 75, [
        histogram.Range.FromExplicitRange(-histogram.JS_MAX_VALUE, -100),
        histogram.Range.FromExplicitRange(-100, 50),
        histogram.Range.FromExplicitRange(50, 60),
        histogram.Range.FromExplicitRange(60, 75),
        histogram.Range.FromExplicitRange(75, histogram.JS_MAX_VALUE),
    ])

  def testAddLinearBins(self):
    b = histogram.HistogramBinBoundaries(1000)
    b.AddLinearBins(1200, 5)
    self._CheckBoundaries(b, 1000, 1200, [
        histogram.Range.FromExplicitRange(-histogram.JS_MAX_VALUE, 1000),
        histogram.Range.FromExplicitRange(1000, 1040),
        histogram.Range.FromExplicitRange(1040, 1080),
        histogram.Range.FromExplicitRange(1080, 1120),
        histogram.Range.FromExplicitRange(1120, 1160),
        histogram.Range.FromExplicitRange(1160, 1200),
        histogram.Range.FromExplicitRange(1200, histogram.JS_MAX_VALUE),
    ])

  def testAddExponentialBins(self):
    b = histogram.HistogramBinBoundaries(0.5)
    b.AddExponentialBins(8, 4)
    self._CheckBoundaries(b, 0.5, 8, [
        histogram.Range.FromExplicitRange(-histogram.JS_MAX_VALUE, 0.5),
        histogram.Range.FromExplicitRange(0.5, 1),
        histogram.Range.FromExplicitRange(1, 2),
        histogram.Range.FromExplicitRange(2, 4),
        histogram.Range.FromExplicitRange(4, 8),
        histogram.Range.FromExplicitRange(8, histogram.JS_MAX_VALUE),
    ])

  def testBinBoundariesCombined(self):
    b = histogram.HistogramBinBoundaries(-273.15)
    b.AddBinBoundary(-50)
    b.AddLinearBins(4, 3)
    b.AddExponentialBins(16, 2)
    b.AddLinearBins(17, 4)
    b.AddBinBoundary(100)

    self._CheckBoundaries(b, -273.15, 100, [
        histogram.Range.FromExplicitRange(-histogram.JS_MAX_VALUE, -273.15),
        histogram.Range.FromExplicitRange(-273.15, -50),
        histogram.Range.FromExplicitRange(-50, -32),
        histogram.Range.FromExplicitRange(-32, -14),
        histogram.Range.FromExplicitRange(-14, 4),
        histogram.Range.FromExplicitRange(4, 8),
        histogram.Range.FromExplicitRange(8, 16),
        histogram.Range.FromExplicitRange(16, 16.25),
        histogram.Range.FromExplicitRange(16.25, 16.5),
        histogram.Range.FromExplicitRange(16.5, 16.75),
        histogram.Range.FromExplicitRange(16.75, 17),
        histogram.Range.FromExplicitRange(17, 100),
        histogram.Range.FromExplicitRange(100, histogram.JS_MAX_VALUE)
    ])

  def testBinBoundariesRaises(self):
    b = histogram.HistogramBinBoundaries(-7)
    with self.assertRaises(Exception):
      b.AddBinBoundary(-10)
    with self.assertRaises(Exception):
      b.AddBinBoundary(-7)
    with self.assertRaises(Exception):
      b.AddLinearBins(-10, 10)
    with self.assertRaises(Exception):
      b.AddLinearBins(-7, 10)
    with self.assertRaises(Exception):
      b.AddLinearBins(10, 0)
    with self.assertRaises(Exception):
      b.AddExponentialBins(16, 4)
    b = histogram.HistogramBinBoundaries(8)
    with self.assertRaises(Exception):
      b.AddExponentialBins(20, 0)
    with self.assertRaises(Exception):
      b.AddExponentialBins(5, 3)
    with self.assertRaises(Exception):
      b.AddExponentialBins(8, 3)

  def testStatisticsScalars(self):
    b = histogram.HistogramBinBoundaries.CreateLinear(0, 100, 100)
    hist = histogram.Histogram('', 'unitless', b)
    hist.AddSample(50)
    hist.AddSample(60)
    hist.AddSample(70)
    hist.AddSample('i am not a number')
    hist.CustomizeSummaryOptions({
        'count': True,
        'min': True,
        'max': True,
        'sum': True,
        'avg': True,
        'std': True,
        'nans': True,
        'geometricMean': True,
        'percentile': [0.5, 1],
    })

    # Test round-tripping summaryOptions
    hist = hist.Clone()
    stats = hist.statistics_scalars
    self.assertEqual(stats['nans'].unit, 'count')
    self.assertEqual(stats['nans'].value, 1)
    self.assertEqual(stats['count'].unit, 'count')
    self.assertEqual(stats['count'].value, 3)
    self.assertEqual(stats['min'].unit, hist.unit)
    self.assertEqual(stats['min'].value, 50)
    self.assertEqual(stats['max'].unit, hist.unit)
    self.assertEqual(stats['max'].value, 70)
    self.assertEqual(stats['sum'].unit, hist.unit)
    self.assertEqual(stats['sum'].value, 180)
    self.assertEqual(stats['avg'].unit, hist.unit)
    self.assertEqual(stats['avg'].value, 60)
    self.assertEqual(stats['std'].unit, hist.unit)
    self.assertEqual(stats['std'].value, 10)
    self.assertEqual(stats['pct_050'].unit, hist.unit)
    self.assertEqual(stats['pct_050'].value, 60.5)
    self.assertEqual(stats['pct_100'].unit, hist.unit)
    self.assertEqual(stats['pct_100'].value, 70.5)
    self.assertEqual(stats['geometricMean'].unit, hist.unit)
    self.assertLess(abs(stats['geometricMean'].value - 59.439), 1e-3)

    hist.CustomizeSummaryOptions({
        'count': False,
        'min': False,
        'max': False,
        'sum': False,
        'avg': False,
        'std': False,
        'nans': False,
        'geometricMean': False,
        'percentile': [],
    })
    self.assertEqual(0, len(hist.statistics_scalars))

  def testStatisticsScalarsEmpty(self):
    b = histogram.HistogramBinBoundaries.CreateLinear(0, 100, 100)
    hist = histogram.Histogram('', 'unitless', b)
    hist.CustomizeSummaryOptions({
        'count': True,
        'min': True,
        'max': True,
        'sum': True,
        'avg': True,
        'std': True,
        'nans': True,
        'geometricMean': True,
        'percentile': [0, 0.01, 0.1, 0.5, 0.995, 1],
    })
    stats = hist.statistics_scalars
    self.assertEqual(stats['nans'].value, 0)
    self.assertEqual(stats['count'].value, 0)
    self.assertEqual(stats['min'].value, histogram.JS_MAX_VALUE)
    self.assertEqual(stats['max'].value, -histogram.JS_MAX_VALUE)
    self.assertEqual(stats['sum'].value, 0)
    self.assertNotIn('avg', stats)
    self.assertNotIn('stddev', stats)
    self.assertEqual(stats['pct_000'].value, 0)
    self.assertEqual(stats['pct_001'].value, 0)
    self.assertEqual(stats['pct_010'].value, 0)
    self.assertEqual(stats['pct_050'].value, 0)
    self.assertEqual(stats['pct_099_5'].value, 0)
    self.assertEqual(stats['pct_100'].value, 0)

  def testSampleValues(self):
    hist0 = histogram.Histogram('', 'unitless', self.TEST_BOUNDARIES)
    hist1 = histogram.Histogram('', 'unitless', self.TEST_BOUNDARIES)
    self.assertEqual(hist0.max_num_sample_values, 120)
    self.assertEqual(hist1.max_num_sample_values, 120)
    values0 = []
    values1 = []
    for i in xrange(10):
      values0.append(i)
      hist0.AddSample(i)
      values1.append(10 + i)
      hist1.AddSample(10 + i)
    self.assertDeepEqual(hist0.sample_values, values0)
    self.assertDeepEqual(hist1.sample_values, values1)
    hist0.AddHistogram(hist1)
    self.assertDeepEqual(hist0.sample_values, values0 + values1)
    hist2 = hist0.Clone()
    self.assertDeepEqual(hist2.sample_values, values0 + values1)

    for i in xrange(200):
      hist0.AddSample(i)
    self.assertEqual(len(hist0.sample_values), hist0.max_num_sample_values)

    hist3 = histogram.Histogram('', 'unitless', self.TEST_BOUNDARIES)
    hist3.max_num_sample_values = 10
    for i in xrange(100):
      hist3.AddSample(i)
    self.assertEqual(len(hist3.sample_values), 10)

  def testSingularBin(self):
    hist = histogram.Histogram(
        '', 'unitless', histogram.HistogramBinBoundaries.SINGULAR)
    self.assertEqual(1, len(hist.bins))
    d = hist.AsDict()
    self.assertNotIn('binBoundaries', d)
    clone = histogram.Histogram.FromDict(d)
    self.assertEqual(1, len(clone.bins))
    self.assertDeepEqual(d, clone.AsDict())

    self.assertEqual(0, hist.GetApproximatePercentile(0))
    self.assertEqual(0, hist.GetApproximatePercentile(1))
    hist.AddSample(0)
    self.assertEqual(0, hist.GetApproximatePercentile(0))
    self.assertEqual(0, hist.GetApproximatePercentile(1))
    hist.AddSample(1)
    self.assertEqual(0, hist.GetApproximatePercentile(0))
    self.assertEqual(1, hist.GetApproximatePercentile(1))
    hist.AddSample(2)
    self.assertEqual(0, hist.GetApproximatePercentile(0))
    self.assertEqual(1, hist.GetApproximatePercentile(0.5))
    self.assertEqual(2, hist.GetApproximatePercentile(1))
    hist.AddSample(3)
    self.assertEqual(0, hist.GetApproximatePercentile(0))
    self.assertEqual(1, hist.GetApproximatePercentile(0.5))
    self.assertEqual(2, hist.GetApproximatePercentile(0.9))
    self.assertEqual(3, hist.GetApproximatePercentile(1))
    hist.AddSample(4)
    self.assertEqual(0, hist.GetApproximatePercentile(0))
    self.assertEqual(1, hist.GetApproximatePercentile(0.4))
    self.assertEqual(2, hist.GetApproximatePercentile(0.7))
    self.assertEqual(3, hist.GetApproximatePercentile(0.9))
    self.assertEqual(4, hist.GetApproximatePercentile(1))


class BreakdownUnittest(unittest.TestCase):
  def testRoundtrip(self):
    bd = histogram.Breakdown()
    bd.Set('one', 1)
    bd.Set('m1', -1)
    bd.Set('inf', float('inf'))
    bd.Set('nun', float('nan'))
    bd.Set('ninf', float('-inf'))
    d = bd.AsDict()
    clone = histogram.Diagnostic.FromDict(d)
    self.assertEqual(ToJSON(d), ToJSON(clone.AsDict()))
    self.assertEqual(clone.Get('one'), 1)
    self.assertEqual(clone.Get('m1'), -1)
    self.assertEqual(clone.Get('inf'), float('inf'))
    self.assertTrue(math.isnan(clone.Get('nun')))
    self.assertEqual(clone.Get('ninf'), float('-inf'))


class BuildbotInfoUnittest(unittest.TestCase):
  def testRoundtrip(self):
    info = histogram.BuildbotInfo({
        'displayMasterName': 'dmn',
        'displayBotName': 'dbn',
        'buildbotMasterName': 'bbmn',
        'buildbotName': 'bbn',
        'buildNumber': 42,
        'logUri': 'uri',
    })
    d = info.AsDict()
    clone = histogram.Diagnostic.FromDict(d)
    self.assertEqual(ToJSON(d), ToJSON(clone.AsDict()))
    self.assertEqual(clone.display_master_name, 'dmn')
    self.assertEqual(clone.display_bot_name, 'dbn')
    self.assertEqual(clone.buildbot_master_name, 'bbmn')
    self.assertEqual(clone.buildbot_name, 'bbn')
    self.assertEqual(clone.build_number, 42)
    self.assertEqual(clone.log_uri, 'uri')


class RevisionInfoUnittest(unittest.TestCase):
  def testRoundtrip(self):
    info = histogram.RevisionInfo({
        'chromiumCommitPosition': 42,
        'v8CommitPosition': 57,
        'chromium': ['b10563e'],
        'v8': ['0a12a6'],
        'catapult': ['e6e086'],
        'angle': ['d7b1ab', 'da9fb0'],
        'skia': ['966bb3', 'db402c'],
        'webrtc': ['277b25', 'f8b262'],
    })
    d = info.AsDict()
    clone = histogram.Diagnostic.FromDict(d)
    self.assertEqual(ToJSON(d), ToJSON(clone.AsDict()))
    self.assertEqual(clone.chromium_commit_position, 42)
    self.assertEqual(clone.v8_commit_position, 57)
    self.assertEqual(clone.chromium[0], 'b10563e')
    self.assertEqual(clone.v8[0], '0a12a6')
    self.assertEqual(clone.catapult[0], 'e6e086')
    self.assertEqual(clone.angle[1], 'da9fb0')
    self.assertEqual(clone.skia[1], 'db402c')
    self.assertEqual(clone.webrtc[1], 'f8b262')


class TelemetryInfoUnittest(unittest.TestCase):
  def testRoundtrip(self):
    info = histogram.TelemetryInfo()
    info.AddInfo({
        'benchmarkName': 'foo',
        'benchmarkStartMs': 42,
        'label': 'lbl',
        'storyDisplayName': 'story',
        'storyGroupingKeys': {'a': 'b'},
        'storysetRepeatCounter': 1,
        'storyUrl': 'url',
        'legacyTIRLabel': 'tir',
    })
    d = info.AsDict()
    clone = histogram.Diagnostic.FromDict(d)
    self.assertEqual(ToJSON(d), ToJSON(clone.AsDict()))
    self.assertEqual(clone.benchmark_name, 'foo')
    self.assertEqual(clone.benchmark_start, 42)
    self.assertEqual(clone.label, 'lbl')
    self.assertEqual(clone.story_display_name, 'story')
    self.assertEqual(clone.story_grouping_keys['a'], 'b')
    self.assertEqual(clone.storyset_repeat_counter, 1)
    self.assertEqual(clone.story_url, 'url')
    self.assertEqual(clone.legacy_tir_label, 'tir')


class DeviceInfoUnittest(unittest.TestCase):
  def testRoundtrip(self):
    info = histogram.DeviceInfo()
    info.chrome_version = '1.2.3.4'
    info.os_name = 'linux'
    info.os_version = '5.6.7'
    info.gpu_info = {'some': 'stuff'}
    info.arch = {'more': 'stuff'}
    info.ram = 42
    d = info.AsDict()
    clone = histogram.Diagnostic.FromDict(d)
    self.assertEqual(ToJSON(d), ToJSON(clone.AsDict()))
    self.assertEqual(clone.chrome_version, '1.2.3.4')
    self.assertEqual(clone.os_name, 'linux')
    self.assertEqual(clone.os_version, '5.6.7')
    self.assertEqual(clone.gpu_info['some'], 'stuff')
    self.assertEqual(clone.arch['more'], 'stuff')
    self.assertEqual(clone.ram, 42)


class RelatedEventSetUnittest(unittest.TestCase):
  def testRoundtrip(self):
    events = histogram.RelatedEventSet()
    events.Add({
        'stableId': '0.0',
        'title': 'foo',
        'start': 0,
        'duration': 1,
    })
    d = events.AsDict()
    clone = histogram.Diagnostic.FromDict(d)
    self.assertEqual(ToJSON(d), ToJSON(clone.AsDict()))
    self.assertEqual(len(events), 1)
    event = list(events)[0]
    self.assertEqual(event['stableId'], '0.0')
    self.assertEqual(event['title'], 'foo')
    self.assertEqual(event['start'], 0)
    self.assertEqual(event['duration'], 1)


class RelatedHistogramBreakdownUnittest(unittest.TestCase):
  def testRoundtrip(self):
    breakdown = histogram.RelatedHistogramBreakdown()
    hista = histogram.Histogram('a', 'unitless')
    histb = histogram.Histogram('b', 'unitless')
    breakdown.Add(hista)
    breakdown.Add(histb)
    d = breakdown.AsDict()
    clone = histogram.Diagnostic.FromDict(d)
    self.assertEqual(ToJSON(d), ToJSON(clone.AsDict()))
    self.assertEqual(hista.guid, clone.Get('a').guid)
    self.assertEqual(histb.guid, clone.Get('b').guid)


class DiagnosticMapUnittest(unittest.TestCase):
  def testMerge(self):
    events = histogram.RelatedEventSet()
    events.Add({
        'stableId': '0.0',
        'title': 'foo',
        'start': 0,
        'duration': 1,
    })
    generic = histogram.Generic('generic diagnostic')
    generic2 = histogram.Generic('generic diagnostic 2')
    related_set = histogram.RelatedHistogramSet([
        histogram.Histogram('histogram', 'count'),
    ])

    hist = histogram.Histogram('', 'count')

    # When Histograms are merged, first an empty clone is created with an empty
    # DiagnosticMap.
    hist2 = histogram.Histogram('', 'count')
    hist2.diagnostics['a'] = generic
    hist.diagnostics.Merge(hist2.diagnostics, hist, hist2)
    self.assertIs(generic, hist.diagnostics['a'])

    # Separate keys are not merged.
    hist3 = histogram.Histogram('', 'count')
    hist3.diagnostics['b'] = generic2
    hist.diagnostics.Merge(hist3.diagnostics, hist, hist3)
    self.assertIs(generic, hist.diagnostics['a'])
    self.assertIs(generic2, hist.diagnostics['b'])

    # Merging unmergeable diagnostics should produce an
    # UnmergeableDiagnosticSet.
    hist4 = histogram.Histogram('', 'count')
    hist4.diagnostics['a'] = related_set
    hist.diagnostics.Merge(hist4.diagnostics, hist, hist4)
    self.assertIsInstance(
        hist.diagnostics['a'], histogram.UnmergeableDiagnosticSet)
    diagnostics = list(hist.diagnostics['a'])
    self.assertIs(generic, diagnostics[0])
    self.assertIs(related_set, diagnostics[1])

    # UnmergeableDiagnosticSets are mergeable.
    hist5 = histogram.Histogram('', 'count')
    hist5.diagnostics['a'] = histogram.UnmergeableDiagnosticSet(
        [events, generic2])
    hist.diagnostics.Merge(hist5.diagnostics, hist, hist5)
    self.assertIsInstance(
        hist.diagnostics['a'], histogram.UnmergeableDiagnosticSet)
    diagnostics = list(hist.diagnostics['a'])
    self.assertIs(generic, diagnostics[0])
    self.assertIs(related_set, diagnostics[1])
    self.assertIs(events, diagnostics[2])
    self.assertIs(generic2, diagnostics[3])


class HistogramSetUnittest(unittest.TestCase):
  def assertDeepEqual(self, a, b):
    self.assertEqual(ToJSON(a), ToJSON(b))

  def testRelatedHistogramSet(self):
    a = histogram.Histogram('a', 'unitless')
    b = histogram.Histogram('b', 'unitless')
    c = histogram.Histogram('c', 'unitless')
    a.diagnostics['rhs'] = histogram.RelatedHistogramSet([b, c])

    # Don't serialize c yet.
    hists = histogram.HistogramSet([a, b])
    hists2 = histogram.HistogramSet()
    hists2.ImportDicts(hists.AsDicts())
    hists2.ResolveRelatedHistograms()
    a2 = hists2.GetHistogramsNamed('a')
    self.assertEqual(len(a2), 1)
    a2 = a2[0]
    self.assertEqual(a2.guid, a.guid)
    self.assertIsInstance(a2, histogram.Histogram)
    self.assertIsNot(a2, a)
    b2 = hists2.GetHistogramsNamed('b')
    self.assertEqual(len(b2), 1)
    b2 = b2[0]
    self.assertEqual(b2.guid, b.guid)
    self.assertIsInstance(b2, histogram.Histogram)
    self.assertIsNot(b2, b)
    rhs2 = a2.diagnostics['rhs']
    self.assertIsInstance(rhs2, histogram.RelatedHistogramSet)
    self.assertEqual(len(rhs2), 2)

    # Assert that b and c are in a2's RelatedHistogramSet, rhs2.
    rhs2hs = list(rhs2)
    rhs2guids = [h.guid for h in rhs2hs]
    b2i = rhs2guids.index(b.guid)
    self.assertIs(rhs2hs[b2i], b2)

    c2i = rhs2guids.index(c.guid)
    self.assertIsInstance(rhs2hs[c2i], histogram.HistogramRef)

    # Now serialize c and add it to hists2.
    hists2.ImportDicts([c.AsDict()])
    hists2.ResolveRelatedHistograms()

    c2 = hists2.GetHistogramsNamed('c')
    self.assertEqual(len(c2), 1)
    c2 = c2[0]
    self.assertEqual(c2.guid, c.guid)
    self.assertIsNot(c2, c)

    rhs2hs = list(rhs2)
    rhs2guids = [h.guid for h in rhs2hs]
    b2i = rhs2guids.index(b.guid)
    c2i = rhs2guids.index(c.guid)
    self.assertIs(b2, rhs2hs[b2i])
    self.assertIs(c2, rhs2hs[c2i])

  def testRelatedHistogramMap(self):
    a = histogram.Histogram('a', 'unitless')
    b = histogram.Histogram('b', 'unitless')
    c = histogram.Histogram('c', 'unitless')
    rhm = histogram.RelatedHistogramMap()
    rhm.Set('y', b)
    rhm.Set('z', c)
    a.diagnostics['rhm'] = rhm

    # Don't serialize c yet.
    hists = histogram.HistogramSet([a, b])
    hists2 = histogram.HistogramSet()
    hists2.ImportDicts(hists.AsDicts())
    hists2.ResolveRelatedHistograms()
    a2 = hists2.GetHistogramsNamed('a')
    self.assertEqual(len(a2), 1)
    a2 = a2[0]
    self.assertEqual(a2.guid, a.guid)
    self.assertIsInstance(a2, histogram.Histogram)
    self.assertIsNot(a2, a)
    b2 = hists2.GetHistogramsNamed('b')
    self.assertEqual(len(b2), 1)
    b2 = b2[0]
    self.assertEqual(b2.guid, b.guid)
    self.assertIsInstance(b2, histogram.Histogram)
    self.assertIsNot(b2, b)
    rhm2 = a2.diagnostics['rhm']
    self.assertIsInstance(rhm2, histogram.RelatedHistogramMap)
    self.assertEqual(len(rhm2), 2)

    # Assert that b and c are in a2's RelatedHistogramMap, rhm2.
    self.assertIs(b2, rhm2.Get('y'))
    self.assertIsInstance(rhm2.Get('z'), histogram.HistogramRef)

    # Now serialize c and add it to hists2.
    hists2.ImportDicts([c.AsDict()])
    hists2.ResolveRelatedHistograms()

    c2 = hists2.GetHistogramsNamed('c')
    self.assertEqual(len(c2), 1)
    c2 = c2[0]
    self.assertEqual(c2.guid, c.guid)
    self.assertIsNot(c2, c)

    self.assertIs(b2, rhm2.Get('y'))
    self.assertIs(c2, rhm2.Get('z'))

  def testImportDicts(self):
    hist = histogram.Histogram('', 'unitless')
    hists = histogram.HistogramSet([hist])
    hists2 = histogram.HistogramSet()
    hists2.ImportDicts(hists.AsDicts())
    self.assertEqual(len(hists), len(hists2))

  def testAddHistogramRaises(self):
    hist = histogram.Histogram('', 'unitless')
    hists = histogram.HistogramSet([hist])
    with self.assertRaises(Exception):
      hists.AddHistogram(hist)
    hist2 = histogram.Histogram('', 'unitless')
    # Do not ever do this in real code:
    hist2.guid = hist.guid
    with self.assertRaises(Exception):
      hists.AddHistogram(hist2)

  def testSharedDiagnostic(self):
    hist = histogram.Histogram('', 'unitless')
    hists = histogram.HistogramSet([hist])
    diag = histogram.Generic('shared')
    hists.AddSharedDiagnostic('generic', diag)

    # Serializing a single Histogram with a single shared diagnostic should
    # produce 2 dicts.
    ds = hists.AsDicts()
    self.assertEqual(len(ds), 2)
    self.assertDeepEqual(diag.AsDict(), ds[0])

    # The serialized Histogram should refer to the shared diagnostic by its
    # guid.
    self.assertEqual(ds[1]['diagnostics']['generic'], diag.guid)

    # Deserialize ds.
    hists2 = histogram.HistogramSet()
    hists2.ImportDicts(ds)
    self.assertEqual(len(hists2), 1)
    hist2 = [h for h in hists2][0]

    # The diagnostic reference should be deserialized as a DiagnosticRef until
    # resolveRelatedHistograms is called.
    self.assertIsInstance(
        hist2.diagnostics.get('generic'), histogram.DiagnosticRef)
    hists2.ResolveRelatedHistograms()
    self.assertIsInstance(
        hist2.diagnostics.get('generic'), histogram.Generic)
    self.assertEqual(diag.value, hist2.diagnostics.get('generic').value)

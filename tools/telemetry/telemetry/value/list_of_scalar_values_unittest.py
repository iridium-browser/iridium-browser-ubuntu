# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import story
from telemetry import page as page_module
from telemetry import value
from telemetry.value import list_of_scalar_values
from telemetry.value import none_values


class StatisticComputationTest(unittest.TestCase):
  def testVariance(self):
    self.assertAlmostEqual(
        list_of_scalar_values.Variance([]), 0)
    self.assertAlmostEqual(
        list_of_scalar_values.Variance([3]), 0)
    self.assertAlmostEqual(
        list_of_scalar_values.Variance([600, 470, 170, 430, 300]), 27130)

  def testStandardDeviation(self):
    self.assertAlmostEqual(
        list_of_scalar_values.StandardDeviation([]), 0)
    self.assertAlmostEqual(
        list_of_scalar_values.StandardDeviation([1]), 0)
    self.assertAlmostEqual(
        list_of_scalar_values.StandardDeviation([600, 470, 170, 430, 300]),
        164.71186, places=4)

  def testPooledVariance(self):
    self.assertAlmostEqual(
        list_of_scalar_values.PooledStandardDeviation([[], [], []]), 0)
    self.assertAlmostEqual(
        list_of_scalar_values.PooledStandardDeviation([[1], [], [3], []]), 0)
    self.assertAlmostEqual(
        list_of_scalar_values.PooledStandardDeviation([[1], [2], [3], [4]]), 0)
    self.assertAlmostEqual(list_of_scalar_values.PooledStandardDeviation(
        [[600, 470, 170, 430, 300],           # variance = 27130, std = 164.7
        [4000, 4020, 4230],                   # variance = 16233, std = 127.41
        [260, 700, 800, 900, 0, 120, 150]]),  # variance = 136348, std = 369.2
        282.7060,  # SQRT((27130 4 + 16233*2 + 136348*6)/12)
        places=4)
    self.assertAlmostEqual(list_of_scalar_values.PooledStandardDeviation(
        [[600, 470, 170, 430, 300],
         [4000, 4020, 4230],
         [260, 700, 800, 900, 0, 120, 150]],
        list_of_variances=[100000, 200000, 300000]),
        465.47466,  # SQRT((100000*4 + 200000* 2 + 300000*6)/12)
        places=4)


class TestBase(unittest.TestCase):
  def setUp(self):
    story_set = story.StorySet(base_dir=os.path.dirname(__file__))
    story_set.AddStory(
        page_module.Page('http://www.bar.com/', story_set, story_set.base_dir))
    story_set.AddStory(
        page_module.Page('http://www.baz.com/', story_set, story_set.base_dir))
    story_set.AddStory(
        page_module.Page('http://www.foo.com/', story_set, story_set.base_dir))
    self.story_set = story_set

  @property
  def pages(self):
    return self.story_set.stories

class ValueTest(TestBase):
  def testListSamePageMergingWithSamePageConcatenatePolicy(self):
    page0 = self.pages[0]
    v0 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [10, 9, 9, 7], same_page_merge_policy=value.CONCATENATE)
    v1 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [300, 302, 303, 304], same_page_merge_policy=value.CONCATENATE)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = (list_of_scalar_values.ListOfScalarValues.
          MergeLikeValuesFromSamePage([v0, v1]))
    self.assertEquals(page0, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals(value.CONCATENATE, vM.same_page_merge_policy)
    self.assertEquals(True, vM.important)
    self.assertEquals([10, 9, 9, 7, 300, 302, 303, 304], vM.values)
    # SQRT((19/12 * 3 + 35/12 * 3)/6) = 1.5
    self.assertAlmostEqual(1.5, vM.std)

  def testListSamePageMergingWithPickFirstPolicy(self):
    page0 = self.pages[0]
    v0 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [1, 2], same_page_merge_policy=value.PICK_FIRST)
    v1 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [3, 4], same_page_merge_policy=value.PICK_FIRST)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = (list_of_scalar_values.ListOfScalarValues.
          MergeLikeValuesFromSamePage([v0, v1]))
    self.assertEquals(page0, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals(value.PICK_FIRST, vM.same_page_merge_policy)
    self.assertEquals(True, vM.important)
    self.assertEquals([1, 2], vM.values)

  def testListDifferentPageMerging(self):
    page0 = self.pages[0]
    page1 = self.pages[1]
    v0 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [1, 2], same_page_merge_policy=value.CONCATENATE)
    v1 = list_of_scalar_values.ListOfScalarValues(
        page1, 'x', 'unit',
        [3, 4], same_page_merge_policy=value.CONCATENATE)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = (list_of_scalar_values.ListOfScalarValues.
          MergeLikeValuesFromDifferentPages([v0, v1]))
    self.assertEquals(None, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals(value.CONCATENATE, vM.same_page_merge_policy)
    self.assertEquals(True, vM.important)
    self.assertEquals([1, 2, 3, 4], vM.values)

  def testListWithNoneValueMerging(self):
    page0 = self.pages[0]
    v0 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [1, 2], same_page_merge_policy=value.CONCATENATE)
    v1 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        None, same_page_merge_policy=value.CONCATENATE, none_value_reason='n')
    self.assertTrue(v1.IsMergableWith(v0))

    vM = (list_of_scalar_values.ListOfScalarValues.
          MergeLikeValuesFromSamePage([v0, v1]))
    self.assertEquals(None, vM.values)
    self.assertEquals(none_values.MERGE_FAILURE_REASON,
                      vM.none_value_reason)

  def testListWithNoneValueMustHaveNoneReason(self):
    page0 = self.pages[0]
    self.assertRaises(none_values.NoneValueMissingReason,
                      lambda: list_of_scalar_values.ListOfScalarValues(
                          page0, 'x', 'unit', None))

  def testListWithNoneReasonMustHaveNoneValue(self):
    page0 = self.pages[0]
    self.assertRaises(none_values.ValueMustHaveNoneValue,
                      lambda: list_of_scalar_values.ListOfScalarValues(
                          page0, 'x', 'unit', [1, 2],
                          none_value_reason='n'))

  def testAsDict(self):
    v = list_of_scalar_values.ListOfScalarValues(
        None, 'x', 'unit', [1, 2],
        same_page_merge_policy=value.PICK_FIRST, important=False)
    d = v.AsDictWithoutBaseClassEntries()

    self.assertEquals(d['values'], [1, 2])
    self.assertAlmostEqual(d['std'], 0.7071, places=4)

  def testMergedValueAsDict(self):
    page0 = self.pages[0]
    v0 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [10, 9, 9, 7], same_page_merge_policy=value.CONCATENATE)
    v1 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [300, 302, 303, 304], same_page_merge_policy=value.CONCATENATE)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = (list_of_scalar_values.ListOfScalarValues.
          MergeLikeValuesFromSamePage([v0, v1]))
    d = vM.AsDict()
    self.assertEquals(d['values'], [10, 9, 9, 7, 300, 302, 303, 304])
    # SQRT((19/12 * 3 + 35/12 * 3)/6)
    self.assertAlmostEqual(d['std'], 1.5)


  def testNoneValueAsDict(self):
    v = list_of_scalar_values.ListOfScalarValues(
        None, 'x', 'unit', None, same_page_merge_policy=value.PICK_FIRST,
        important=False, none_value_reason='n')
    d = v.AsDictWithoutBaseClassEntries()

    self.assertEquals(d, {
          'values': None,
          'none_value_reason': 'n',
          'std': None
        })

  def testFromDictInts(self):
    d = {
      'type': 'list_of_scalar_values',
      'name': 'x',
      'units': 'unit',
      'values': [1, 2],
      'std': 0.7071
    }
    v = value.Value.FromDict(d, {})

    self.assertTrue(isinstance(v, list_of_scalar_values.ListOfScalarValues))
    self.assertEquals(v.values, [1, 2])
    self.assertEquals(v.std, 0.7071)

  def testFromDictFloats(self):
    d = {
      'type': 'list_of_scalar_values',
      'name': 'x',
      'units': 'unit',
      'values': [1.3, 2.7, 4.5, 2.1, 3.4],
      'std': 0.901
    }
    v = value.Value.FromDict(d, {})

    self.assertTrue(isinstance(v, list_of_scalar_values.ListOfScalarValues))
    self.assertEquals(v.values, [1.3, 2.7, 4.5, 2.1, 3.4])
    self.assertEquals(v.std, 0.901)

  def testFromDictNoneValue(self):
    d = {
      'type': 'list_of_scalar_values',
      'name': 'x',
      'units': 'unit',
      'values': None,
      'std': None,
      'none_value_reason': 'n'
    }
    v = value.Value.FromDict(d, {})

    self.assertTrue(isinstance(v, list_of_scalar_values.ListOfScalarValues))
    self.assertEquals(v.values, None)
    self.assertEquals(v.none_value_reason, 'n')

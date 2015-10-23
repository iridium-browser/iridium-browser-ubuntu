# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import story
from telemetry import page as page_module
from telemetry import value
from telemetry.value import none_values
from telemetry.value import scalar


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
  def testBuildbotValueType(self):
    page0 = self.pages[0]
    v = scalar.ScalarValue(page0, 'x', 'unit', 3, important=True)
    self.assertEquals('default', v.GetBuildbotDataType(
        value.COMPUTED_PER_PAGE_SUMMARY_OUTPUT_CONTEXT))
    self.assertEquals([3], v.GetBuildbotValue())
    self.assertEquals(('x', page0.display_name),
                      v.GetChartAndTraceNameForPerPageResult())

    v = scalar.ScalarValue(page0, 'x', 'unit', 3, important=False)
    self.assertEquals(
        'unimportant',
        v.GetBuildbotDataType(value.COMPUTED_PER_PAGE_SUMMARY_OUTPUT_CONTEXT))

  def testScalarSamePageMerging(self):
    page0 = self.pages[0]
    v0 = scalar.ScalarValue(page0, 'x', 'unit', 1)
    v1 = scalar.ScalarValue(page0, 'x', 'unit', 2)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = scalar.ScalarValue.MergeLikeValuesFromSamePage([v0, v1])
    self.assertEquals(page0, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals(True, vM.important)
    self.assertEquals([1, 2], vM.values)

  def testScalarDifferentPageMerging(self):
    page0 = self.pages[0]
    page1 = self.pages[1]
    v0 = scalar.ScalarValue(page0, 'x', 'unit', 1)
    v1 = scalar.ScalarValue(page1, 'x', 'unit', 2)

    vM = scalar.ScalarValue.MergeLikeValuesFromDifferentPages([v0, v1])
    self.assertEquals(None, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals(True, vM.important)
    self.assertEquals([1, 2], vM.values)

  def testScalarWithNoneValueMerging(self):
    page0 = self.pages[0]
    v0 = scalar.ScalarValue(page0, 'x', 'unit', 1)
    v1 = scalar.ScalarValue(page0, 'x', 'unit', None, none_value_reason='n')
    self.assertTrue(v1.IsMergableWith(v0))

    vM = scalar.ScalarValue.MergeLikeValuesFromSamePage([v0, v1])
    self.assertEquals(None, vM.values)
    self.assertEquals(none_values.MERGE_FAILURE_REASON,
                      vM.none_value_reason)

  def testScalarWithNoneValueMustHaveNoneReason(self):
    page0 = self.pages[0]
    self.assertRaises(none_values.NoneValueMissingReason,
                      lambda: scalar.ScalarValue(page0, 'x', 'unit', None))

  def testScalarWithNoneReasonMustHaveNoneValue(self):
    page0 = self.pages[0]
    self.assertRaises(none_values.ValueMustHaveNoneValue,
                      lambda: scalar.ScalarValue(page0, 'x', 'unit', 1,
                                                 none_value_reason='n'))

  def testAsDict(self):
    v = scalar.ScalarValue(None, 'x', 'unit', 42, important=False)
    d = v.AsDictWithoutBaseClassEntries()

    self.assertEquals(d, {
          'value': 42
        })

  def testNoneValueAsDict(self):
    v = scalar.ScalarValue(None, 'x', 'unit', None, important=False,
                           none_value_reason='n')
    d = v.AsDictWithoutBaseClassEntries()

    self.assertEquals(d, {
          'value': None,
          'none_value_reason': 'n'
        })

  def testFromDictInt(self):
    d = {
      'type': 'scalar',
      'name': 'x',
      'units': 'unit',
      'value': 42
    }

    v = value.Value.FromDict(d, {})

    self.assertTrue(isinstance(v, scalar.ScalarValue))
    self.assertEquals(v.value, 42)

  def testFromDictFloat(self):
    d = {
      'type': 'scalar',
      'name': 'x',
      'units': 'unit',
      'value': 42.4
    }

    v = value.Value.FromDict(d, {})

    self.assertTrue(isinstance(v, scalar.ScalarValue))
    self.assertEquals(v.value, 42.4)

  def testFromDictNoneValue(self):
    d = {
      'type': 'scalar',
      'name': 'x',
      'units': 'unit',
      'value': None,
      'none_value_reason': 'n'
    }

    v = value.Value.FromDict(d, {})

    self.assertTrue(isinstance(v, scalar.ScalarValue))
    self.assertEquals(v.value, None)
    self.assertEquals(v.none_value_reason, 'n')

# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from telemetry import page as page_module
from telemetry.page import page_set
from telemetry import value
from telemetry.value import list_of_scalar_values
from telemetry.value import none_values


class TestBase(unittest.TestCase):
  def setUp(self):
    ps = page_set.PageSet(file_path=os.path.dirname(__file__))
    ps.AddUserStory(page_module.Page('http://www.bar.com/', ps, ps.base_dir))
    ps.AddUserStory(page_module.Page('http://www.baz.com/', ps, ps.base_dir))
    ps.AddUserStory(page_module.Page('http://www.foo.com/', ps, ps.base_dir))
    self.page_set = ps

  @property
  def pages(self):
    return self.page_set.pages

class ValueTest(TestBase):
  def testListSamePageMergingWithSamePageConcatenatePolicy(self):
    page0 = self.pages[0]
    v0 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [1, 2], same_page_merge_policy=value.CONCATENATE)
    v1 = list_of_scalar_values.ListOfScalarValues(
        page0, 'x', 'unit',
        [3, 4], same_page_merge_policy=value.CONCATENATE)
    self.assertTrue(v1.IsMergableWith(v0))

    vM = (list_of_scalar_values.ListOfScalarValues.
          MergeLikeValuesFromSamePage([v0, v1]))
    self.assertEquals(page0, vM.page)
    self.assertEquals('x', vM.name)
    self.assertEquals('unit', vM.units)
    self.assertEquals(value.CONCATENATE, vM.same_page_merge_policy)
    self.assertEquals(True, vM.important)
    self.assertEquals([1, 2, 3, 4], vM.values)

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

    self.assertEquals(d, {
          'values': [1, 2]
        })

  def testNoneValueAsDict(self):
    v = list_of_scalar_values.ListOfScalarValues(
        None, 'x', 'unit', None, same_page_merge_policy=value.PICK_FIRST,
        important=False, none_value_reason='n')
    d = v.AsDictWithoutBaseClassEntries()

    self.assertEquals(d, {
          'values': None,
          'none_value_reason': 'n'
        })

  def testFromDictInts(self):
    d = {
      'type': 'list_of_scalar_values',
      'name': 'x',
      'units': 'unit',
      'values': [1, 2]
    }
    v = value.Value.FromDict(d, {})

    self.assertTrue(isinstance(v, list_of_scalar_values.ListOfScalarValues))
    self.assertEquals(v.values, [1, 2])

  def testFromDictFloats(self):
    d = {
      'type': 'list_of_scalar_values',
      'name': 'x',
      'units': 'unit',
      'values': [1.3, 2.7]
    }
    v = value.Value.FromDict(d, {})

    self.assertTrue(isinstance(v, list_of_scalar_values.ListOfScalarValues))
    self.assertEquals(v.values, [1.3, 2.7])

  def testFromDictNoneValue(self):
    d = {
      'type': 'list_of_scalar_values',
      'name': 'x',
      'units': 'unit',
      'values': None,
      'none_value_reason': 'n'
    }
    v = value.Value.FromDict(d, {})

    self.assertTrue(isinstance(v, list_of_scalar_values.ListOfScalarValues))
    self.assertEquals(v.values, None)
    self.assertEquals(v.none_value_reason, 'n')

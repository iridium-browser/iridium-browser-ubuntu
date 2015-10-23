# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import fieldtrial_to_struct
import os


class FieldTrialToStruct(unittest.TestCase):

  def test_FieldTrialToDescription(self):
    config = {
      'Study1': [
        {
          'group_name': 'Group1',
          'params': {
            'x': '1',
            'y': '2'
          }
        }
      ],
      'Study2': [{'group_name': 'OtherGroup'}]
    }
    result = fieldtrial_to_struct._FieldTrialConfigToDescription(config)
    expected = {
      'elements': {
        'kFieldTrialConfig': {
          'groups': [
            {
              'study': 'Study1',
              'group_name': 'Group1',
              'params': [
                {'key': 'x', 'value': '1'},
                {'key': 'y', 'value': '2'}
              ]
            },
            {
              'study': 'Study2',
              'group_name': 'OtherGroup'
            }
          ]
        }
      }
    }
    self.assertEqual(expected, result)

  def test_FieldTrialToStructMain(self):
    schema = (
        '../../chrome/common/variations/fieldtrial_testing_config_schema.json')
    test_ouput_filename = 'test_ouput'
    fieldtrial_to_struct.main([
      '--schema=' + schema,
      '--output=' + test_ouput_filename,
      '--year=2015',
      'unittest_data/test_config.json'
    ])
    header_filename = test_ouput_filename + '.h'
    with open(header_filename, 'r') as header:
      test_header = header.read()
      with open('unittest_data/expected_output.h', 'r') as expected:
        expected_header = expected.read()
        self.assertEqual(expected_header, test_header)
    os.unlink(header_filename)

    cc_filename = test_ouput_filename + '.cc'
    with open(cc_filename, 'r') as cc:
      test_cc = cc.read()
      with open('unittest_data/expected_output.cc', 'r') as expected:
        expected_cc = expected.read()
        self.assertEqual(expected_cc, test_cc)
    os.unlink(cc_filename)

if __name__ == '__main__':
  unittest.main()

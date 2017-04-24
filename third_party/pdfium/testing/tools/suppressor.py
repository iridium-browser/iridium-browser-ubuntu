#!/usr/bin/env python
# Copyright 2015 The PDFium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import common

class Suppressor:
  def __init__(self, finder, feature_string):
    feature_vector = feature_string.strip().split(",")
    self.has_v8 = "V8" in feature_vector
    self.has_xfa = "XFA" in feature_vector
    self.is_asan = "ASAN" in feature_vector
    v8_option = "v8" if self.has_v8 else "nov8"
    xfa_option = "xfa" if self.has_xfa else "noxfa"

    with open(os.path.join(finder.TestingDir(), 'SUPPRESSIONS')) as f:
      self.suppression_set = set(self._FilterSuppressions(
        common.os_name(), v8_option, xfa_option, self._ExtractSuppressions(f)))

  def _ExtractSuppressions(self, f):
    return [y.split(' ') for y in
            [x.split('#')[0].strip() for x in
             f.readlines()] if y]

  def _FilterSuppressions(self, os, js, xfa, unfiltered_list):
    return [x[0] for x in unfiltered_list
            if self._MatchSuppression(x, os, js, xfa)]

  def _MatchSuppression(self, item, os, js, xfa):
    os_column = item[1].split(",");
    js_column = item[2].split(",");
    xfa_column = item[3].split(",");
    if self.is_asan and 'asan' in os_column:
      return True
    return (('*' in os_column or os in os_column) and
            ('*' in js_column or js in js_column) and
            ('*' in xfa_column or xfa in xfa_column))

  def IsResultSuppressed(self, input_filename):
    if input_filename in self.suppression_set:
      print "%s result is suppressed" % input_filename
      return True
    return False

  def IsExecutionSuppressed(self, input_filepath):
    if "xfa_specific" in input_filepath and not self.has_xfa:
      print "%s execution is suppressed" % input_filepath
      return True
    return False

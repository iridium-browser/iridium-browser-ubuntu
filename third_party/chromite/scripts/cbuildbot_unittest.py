# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the cbuildbot program"""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.lib import cros_test_lib
from chromite.scripts import cbuildbot


# pylint: disable=protected-access


class SiteConfigTests(cros_test_lib.MockTestCase):
  """Test cbuildbot._SetupSiteConfig."""
  def setUp(self):
    self.options = mock.Mock()
    self.options.config_repo = None

    self.expected_result = mock.Mock()

    self.exists_mock = self.PatchObject(os.path, 'exists')
    self.load_mock = self.PatchObject(config_lib, 'LoadConfigFromFile',
                                      return_value=self.expected_result)

  def testDefaultChromeOsBehavior(self):
    # Setup Fakes and Mocks.
    self.exists_mock.return_value = False

    # Run Tests
    result = cbuildbot._SetupSiteConfig(self.options)

    # Evaluate Results
    self.assertIs(result, self.expected_result)
    self.load_mock.assert_called_once_with(constants.CHROMEOS_CONFIG_FILE)

  def testDefaultSiteBehavior(self):
    # Setup Fakes and Mocks.
    self.exists_mock.return_value = True

    # Run Tests
    result = cbuildbot._SetupSiteConfig(self.options)

    # Evaluate Results
    self.assertIs(result, self.expected_result)
    self.load_mock.assert_called_once_with(constants.SITE_CONFIG_FILE)

  # TODO(dgarrett): Test a specified site URL, when it's implemented.


class IsDistributedBuilderTest(cros_test_lib.TestCase):
  """Test for cbuildbot._IsDistributedBuilder."""

  # pylint: disable=W0212
  def testIsDistributedBuilder(self):
    """Tests for _IsDistributedBuilder() under various configurations."""
    parser = cbuildbot._CreateParser()
    argv = ['x86-generic-paladin']
    (options, _) = cbuildbot._ParseCommandLine(parser, argv)
    options.buildbot = False
    options.pre_cq = False

    build_config = dict(pre_cq=False,
                        manifest_version=False)
    chrome_rev = None

    def _TestConfig(expected):
      self.assertEquals(expected,
                        cbuildbot._IsDistributedBuilder(
                            options=options,
                            chrome_rev=chrome_rev,
                            build_config=build_config))

    # Default options.
    _TestConfig(False)

    # In Pre-CQ mode, we run as as a distributed builder.
    options.pre_cq = True
    _TestConfig(True)

    options.pre_cq = False
    build_config['pre_cq'] = True
    _TestConfig(True)

    build_config['pre_cq'] = False
    build_config['manifest_version'] = True
    # Not running in buildbot mode even though manifest_version=True.
    _TestConfig(False)
    options.buildbot = True
    _TestConfig(True)

    for chrome_rev in (constants.CHROME_REV_TOT,
                       constants.CHROME_REV_LOCAL,
                       constants.CHROME_REV_SPEC):
      _TestConfig(False)

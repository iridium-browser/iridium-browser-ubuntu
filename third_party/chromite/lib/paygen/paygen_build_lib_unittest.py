# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for paygen_build_lib."""

from __future__ import print_function

import itertools
import mox
import os
import shutil
import tempfile
import unittest

from chromite.cbuildbot import commands
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import config_lib_unittest
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import parallel

from chromite.lib.paygen import download_cache
from chromite.lib.paygen import gslock
from chromite.lib.paygen import gslib
from chromite.lib.paygen import gspaths
from chromite.lib.paygen import urilib
from chromite.lib.paygen import paygen_build_lib
from chromite.lib.paygen import paygen_payload_lib
from chromite.lib.paygen import utils


# We access a lot of protected members during testing.
# pylint: disable=protected-access


class PaygenBuildLibTest(cros_test_lib.MoxTempDirTestCase):
  """Test PaygenBuildLib class."""

  def setUp(self):
    self.work_dir = '/work/foo'

    self.prev_image = gspaths.Image(channel='foo-channel',
                                    board='foo-board',
                                    version='1.0.0',
                                    key='mp')
    self.prev2_image = gspaths.Image(channel='foo-channel',
                                     board='foo-board',
                                     version='1.1.0',
                                     key='mp')

    self.foo_build = gspaths.Build(bucket='crt',
                                   channel='foo-channel',
                                   board='foo-board',
                                   version='1.2.3')

    # Create an additional 'special' image like NPO that isn't NPO,
    # and keyed with a weird key. It should match none of the filters.
    self.special_image = gspaths.Image(bucket='crt',
                                       channel='foo-channel',
                                       board='foo-board',
                                       version='1.2.3',
                                       key='foo-key',
                                       image_channel='special-channel')

    self.images = self._GetBuildImages(self.foo_build)
    (self.basic_image, self.premp_image,
     self.npo_image, self.premp_npo_image) = self.images

    self.test_image = self._GetBuildTestImage(self.foo_build)

  def _GetPaygenBuildInstance(self, skip_test_payloads=False,
                              disable_tests=False):
    """Helper method to create a standard Paygen instance."""
    control_dir = None if disable_tests else '/tmp/foo'

    return paygen_build_lib._PaygenBuild(self.foo_build, self.work_dir,
                                         config_lib_unittest.MockSiteConfig(),
                                         control_dir=control_dir,
                                         skip_test_payloads=skip_test_payloads)

  def _GetBuildImages(self, build):
    """Create basic_image, npo_image, premp_image, premp_npo_image.

    Args:
      build: gspaths.Build object describing the build to create fake images
        for.
    """
    # NPOs should have image_version incremented, but it doesn't matter for our
    # testing.
    basic_image = gspaths.Image(key='mp-v2', **build)
    npo_image = gspaths.Image(key='mp-v2',
                              image_channel='nplusone-channel',
                              image_version=build.version,
                              **build)
    premp_image = gspaths.Image(key='premp', **build)
    premp_npo_image = gspaths.Image(key='premp',
                                    image_channel='nplusone-channel',
                                    image_version=build.version,
                                    **build)

    # Code in several places depends on the order.
    return [basic_image, premp_image, npo_image, premp_npo_image]

  def _GetBuildTestImage(self, build):
    """Returns a test image object for the build.

    Args:
      build: gspaths.Build object describing the build to create fake images
        for.
    """
    return gspaths.UnsignedImageArchive(bucket=build.bucket,
                                        channel=build.channel,
                                        board=build.board,
                                        version=build.version,
                                        milestone='R12',
                                        image_type='test')

  def testGetFlagURI(self):
    """Validate the helper method to create flag URIs for our current build."""
    paygen = self._GetPaygenBuildInstance()

    self.assertEqual(
        paygen._GetFlagURI(gspaths.ChromeosReleases.LOCK),
        'gs://crt/foo-channel/foo-board/1.2.3/payloads/LOCK_flag')
    self.assertEqual(
        paygen._GetFlagURI(gspaths.ChromeosReleases.SKIP),
        'gs://crt/foo-channel/foo-board/1.2.3/payloads/SKIP_flag')
    self.assertEqual(
        paygen._GetFlagURI(gspaths.ChromeosReleases.FINISHED),
        'gs://crt/foo-channel/foo-board/1.2.3/payloads/FINISHED_flag')

  def testFilterHelpers(self):
    """Test _FilterForMp helper method."""

    # All of the filter helpers should handle empty list.
    self.assertEqual(paygen_build_lib._FilterForMp([]), [])
    self.assertEqual(paygen_build_lib._FilterForPremp([]), [])
    self.assertEqual(paygen_build_lib._FilterForBasic([]), [])
    self.assertEqual(paygen_build_lib._FilterForNpo([]), [])

    # prev_image lets us test with an 'mp' key, instead of an 'mp-v2' key.
    images = list(self.images) + [self.special_image, self.prev_image]

    self.assertEqual(paygen_build_lib._FilterForMp(images),
                     [self.basic_image, self.npo_image, self.prev_image])

    self.assertEqual(paygen_build_lib._FilterForPremp(images),
                     [self.premp_image, self.premp_npo_image])

    self.assertEqual(paygen_build_lib._FilterForBasic(images),
                     [self.basic_image, self.premp_image, self.prev_image])

    self.assertEqual(paygen_build_lib._FilterForNpo(images),
                     [self.npo_image, self.premp_npo_image])

  def testValidateExpectedBuildImages(self):
    """Test a function that validates expected images are found on a build."""
    paygen = self._GetPaygenBuildInstance()

    # Test with basic mp image only.
    paygen._ValidateExpectedBuildImages(self.foo_build, (self.basic_image,))

    # Test with basic mp and mp npo images.
    paygen._ValidateExpectedBuildImages(self.foo_build, (self.basic_image,
                                                         self.npo_image))
    # Test with basic mp and premp images.
    paygen._ValidateExpectedBuildImages(self.foo_build, (self.basic_image,
                                                         self.premp_image))

    # Test with basic mp and premp images.
    paygen._ValidateExpectedBuildImages(self.foo_build, (self.basic_image,
                                                         self.premp_image,
                                                         self.npo_image))

    # Test with 4 different images.
    paygen._ValidateExpectedBuildImages(self.foo_build, (self.basic_image,
                                                         self.premp_image,
                                                         self.npo_image,
                                                         self.premp_npo_image))

    # No images isn't valid.
    with self.assertRaises(paygen_build_lib.ImageMissing):
      paygen._ValidateExpectedBuildImages(self.foo_build, [])

    # NPO image only isn't valid.
    with self.assertRaises(paygen_build_lib.ImageMissing):
      paygen._ValidateExpectedBuildImages(self.foo_build, (self.npo_image,))

    # NPO without matching basic isn't valid.
    with self.assertRaises(paygen_build_lib.ImageMissing):
      paygen._ValidateExpectedBuildImages(self.foo_build,
                                          (self.premp_image,
                                           self.npo_image,
                                           self.premp_npo_image))

    # More than one of the same type of image should trigger BuildCorrupt
    with self.assertRaises(paygen_build_lib.BuildCorrupt):
      paygen._ValidateExpectedBuildImages(self.foo_build, (self.basic_image,
                                                           self.basic_image))

    # Unexpected images should trigger BuildCorrupt
    with self.assertRaises(paygen_build_lib.BuildCorrupt):
      paygen._ValidateExpectedBuildImages(self.foo_build,
                                          (self.basic_image,
                                           self.npo_image,
                                           self.special_image))

  def _TestDiscoverArtifacts(self, list_files_uri, list_files_result,
                             test_func, test_args, should_succeed,
                             expected_result):
    """Test artifact discovery using mocked gsutil results."""
    self.mox.StubOutWithMock(urilib, 'ListFiles')
    urilib.ListFiles(list_files_uri).AndReturn(list_files_result)
    self.mox.ReplayAll()

    if should_succeed:
      self.assertEqual(test_func(*test_args), expected_result)
    else:
      self.assertRaises(expected_result, test_func, *test_args)

  def testDiscoverImages(self):
    """Test _DiscoverImages."""
    paygen = self._GetPaygenBuildInstance()
    uri_base = 'gs://crt/foo-channel/foo-board/1.2.3'

    uri_basic = os.path.join(
        uri_base, 'chromeos_1.2.3_foo-board_recovery_foo-channel_mp-v3.bin')
    uri_premp = os.path.join(
        uri_base, 'chromeos_1.2.3_foo-board_recovery_foo-channel_premp.bin')
    uri_npo = os.path.join(
        uri_base,
        'chromeos_1.2.4_foo-board_recovery_nplusone-channel_mp-v3.bin')
    file_list_result = [uri_basic, uri_premp, uri_npo]

    base_image_params = {'channel': 'foo-channel',
                         'board': 'foo-board',
                         'version': '1.2.3',
                         'bucket': 'crt'}
    expected_basic = gspaths.Image(key='mp-v3', uri=uri_basic,
                                   **base_image_params)
    expected_premp = gspaths.Image(key='premp', uri=uri_premp,
                                   **base_image_params)
    expected_npo = gspaths.Image(key='mp-v3', image_channel='nplusone-channel',
                                 image_version='1.2.4', uri=uri_npo,
                                 **base_image_params)
    expected_result = [expected_basic, expected_premp, expected_npo]

    self._TestDiscoverArtifacts(
        os.path.join(uri_base, 'chromeos_*_foo-board_recovery_*_*.bin'),
        file_list_result,
        paygen._DiscoverImages,
        [self.foo_build],
        True,
        expected_result)

  def testDiscoverTestImageArchives(self):
    """Test _DiscoverTestImageArchives (success)."""
    paygen = self._GetPaygenBuildInstance()
    uri_base = 'gs://crt/foo-channel/foo-board/1.2.3'

    uri_test_archive = os.path.join(
        uri_base, 'ChromeOS-test-R12-1.2.3-foo-board.tar.xz')
    file_list_result = [uri_test_archive]

    expected_test_archive = gspaths.UnsignedImageArchive(
        channel='foo-channel',
        board='foo-board',
        version='1.2.3',
        bucket='crt',
        uri=uri_test_archive,
        milestone='R12',
        image_type='test')
    expected_result = [expected_test_archive]

    self._TestDiscoverArtifacts(
        os.path.join(uri_base, 'ChromeOS-test-*-1.2.3-foo-board.tar.xz'),
        file_list_result,
        paygen._DiscoverTestImageArchives,
        [self.foo_build],
        True,
        expected_result)

  def testDiscoverTestImageArchivesMultipleResults(self):
    """Test _DiscoverTestImageArchives (fails due to multiple results)."""
    paygen = self._GetPaygenBuildInstance()
    uri_base = 'gs://crt/foo-channel/foo-board/1.2.3'

    uri_test_archive1 = os.path.join(
        uri_base, 'ChromeOS-test-R12-1.2.3-foo-board.tar.xz')
    uri_test_archive2 = os.path.join(
        uri_base, 'ChromeOS-test-R13-1.2.3-foo-board.tar.xz')
    file_list_result = [uri_test_archive1, uri_test_archive2]

    self._TestDiscoverArtifacts(
        os.path.join(uri_base, 'ChromeOS-test-*-1.2.3-foo-board.tar.xz'),
        file_list_result,
        paygen._DiscoverTestImageArchives,
        [self.foo_build],
        False,
        paygen_build_lib.BuildCorrupt)

  def testDiscoverTestImageArchivesMissing(self):
    """Test _DiscoverTestImageArchives (fails due to missing images)."""
    paygen = self._GetPaygenBuildInstance()
    uri_base = 'gs://crt/foo-channel/foo-board/1.2.3'

    self._TestDiscoverArtifacts(
        os.path.join(uri_base, 'ChromeOS-test-*-1.2.3-foo-board.tar.xz'),
        [],
        paygen._DiscoverTestImageArchives,
        [self.foo_build],
        False,
        paygen_build_lib.ImageMissing)

  @unittest.skipIf(not paygen_build_lib.config, 'Internal crostools required.')
  def testDiscoverActiveFsiBuilds(self):
    """Using test release.conf values, test _DiscoverActiveFsiBuilds."""

    test_config = """
[valid-board]
fsi_images: 2913.331.0,2465.105.0

[no-fsi-board]
"""
    paygen_build_lib.config.LoadTestConfig(test_config)

    # Test a board with FSI values on stable-channel.
    paygen = paygen_build_lib._PaygenBuild(
        gspaths.Build(channel='stable-channel', board='valid-board',
                      version='1.2.3'),
        self.work_dir,
        config_lib_unittest.MockSiteConfig())

    self.assertEqual(
        sorted(paygen._DiscoverActiveFsiBuilds()),
        [gspaths.Build(board='valid-board',
                       channel='stable-channel',
                       version='2465.105.0'),
         gspaths.Build(board='valid-board',
                       channel='stable-channel',
                       version='2913.331.0')])

    # Test a board without FSI values on stable-channel.
    paygen = paygen_build_lib._PaygenBuild(
        gspaths.Build(channel='stable-channel', board='no-fsi-board',
                      version='1.2.3'),
        self.work_dir,
        config_lib_unittest.MockSiteConfig())

    self.assertEqual(paygen._DiscoverActiveFsiBuilds(), [])

    # Test a board with FSI values on non-stable-channel.
    paygen = paygen_build_lib._PaygenBuild(
        gspaths.Build(channel='beta-channel', board='valid-board',
                      version='1.2.3'),
        self.work_dir,
        config_lib_unittest.MockSiteConfig())

    self.assertEqual(paygen._DiscoverActiveFsiBuilds(), [])

    paygen_build_lib.config.LoadGlobalConfig()

  @cros_test_lib.NetworkTest()
  @unittest.skipIf(not paygen_build_lib.config, 'Internal crostools required.')
  def testDiscoverAllFsiBuilds(self):
    """Using test release.conf values, test _DiscoverActiveFsiBuilds."""
    paygen = paygen_build_lib._PaygenBuild(
        gspaths.Build(channel='stable-channel', board='x86-alex-he',
                      version='1.2.3'),
        self.work_dir,
        config_lib_unittest.MockSiteConfig())

    # Search for real FSIs for an older/live board.
    self.assertEqual(paygen._DiscoverAllFsiBuilds(),
                     ['0.12.433.257', '0.14.811.132', '1412.205.0'])

  @unittest.skipIf(not paygen_build_lib.query, 'Internal crostools required.')
  def testDiscoverNmoBuild(self):
    """Test _DiscoverNmoBuild (N minus One)."""
    paygen = self._GetPaygenBuildInstance()

    self.mox.StubOutWithMock(paygen_build_lib.query, 'FindLatestPublished')

    # Set up the test replay script.
    paygen_build_lib.query.FindLatestPublished(
        'foo-channel', 'foo-board').AndReturn('1.0.0')

    paygen_build_lib.query.FindLatestPublished(
        'foo-channel', 'foo-board').AndReturn(None)

    # Run the test verification.
    self.mox.ReplayAll()

    self.assertEqual(paygen._DiscoverNmoBuild(),
                     [gspaths.Build(bucket='crt',
                                    channel='foo-channel',
                                    board='foo-board',
                                    version='1.0.0')])

    self.assertEqual(paygen._DiscoverNmoBuild(), [])

  def testDiscoverRequiredFullPayloads(self):
    """Test _DiscoverRequiredFullPayloads."""
    paygen = self._GetPaygenBuildInstance()

    self.assertEqual(paygen._DiscoverRequiredFullPayloads([]), [])

    self.assertItemsEqual(
        paygen._DiscoverRequiredFullPayloads(self.images + [self.test_image]),
        [gspaths.Payload(tgt_image=self.basic_image),
         gspaths.Payload(tgt_image=self.npo_image),
         gspaths.Payload(tgt_image=self.premp_image),
         gspaths.Payload(tgt_image=self.premp_npo_image),
         gspaths.Payload(tgt_image=self.test_image)])

  def testDiscoverRequiredNpoDeltas(self):
    """Test _DiscoverRequiredNpoDeltas."""
    paygen = self._GetPaygenBuildInstance()

    self.assertEqual(paygen._DiscoverRequiredNpoDeltas([]), [])

    self.assertEqual(paygen._DiscoverRequiredNpoDeltas([self.basic_image]), [])

    self.assertEqual(paygen._DiscoverRequiredNpoDeltas([self.npo_image]), [])

    expected = [gspaths.Payload(tgt_image=self.npo_image,
                                src_image=self.basic_image)]
    self.assertEqual(paygen._DiscoverRequiredNpoDeltas([self.basic_image,
                                                        self.npo_image]),
                     expected)

    self.assertEqual(paygen._DiscoverRequiredNpoDeltas([self.npo_image,
                                                        self.basic_image]),
                     expected)

    self.assertEqual(paygen._DiscoverRequiredNpoDeltas([self.premp_image,
                                                        self.premp_npo_image]),
                     [gspaths.Payload(tgt_image=self.premp_npo_image,
                                      src_image=self.premp_image)])

  def testDiscoverRequiredTestNpoDeltas(self):
    """Test _DiscoverRequiredTestNpoDeltas."""
    paygen = self._GetPaygenBuildInstance()

    self.assertEqual(
        paygen._DiscoverRequiredTestNpoDeltas([]), [])
    self.assertItemsEqual(
        paygen._DiscoverRequiredTestNpoDeltas([self.test_image]),
        [gspaths.Payload(tgt_image=self.test_image,
                         src_image=self.test_image)])

  def testDiscoverRequiredFromPreviousDeltas(self):
    """Test _DiscoverRequiredFromPreviousDeltas."""
    paygen = self._GetPaygenBuildInstance()

    images = [self.basic_image]
    prevs = [self.prev_image, self.prev2_image]

    # Empty lists.
    results = paygen._DiscoverRequiredFromPreviousDeltas([], [])
    expected = []
    self.assertEqual(results, expected)

    # Empty previous list.
    results = paygen._DiscoverRequiredFromPreviousDeltas(images, [])
    expected = []
    self.assertEqual(results, expected)

    # Empty target list.
    results = paygen._DiscoverRequiredFromPreviousDeltas([], prevs)
    expected = []
    self.assertEqual(results, expected)

    # Basic list.
    results = paygen._DiscoverRequiredFromPreviousDeltas(images, prevs)
    expected = [gspaths.Payload(tgt_image=self.basic_image,
                                src_image=self.prev_image),
                gspaths.Payload(tgt_image=self.basic_image,
                                src_image=self.prev2_image)]
    self.assertEqual(results, expected)

    # Inverted order (should return nothing).
    results = paygen._DiscoverRequiredFromPreviousDeltas(
        [self.prev_image], images)
    expected = []
    self.assertEqual(results, expected)

  def testDiscoverRequiredPayloadsIncompleteBuild(self):
    """Test _DiscoverRequiredPayloads."""

    paygen = self._GetPaygenBuildInstance()

    self.mox.StubOutWithMock(paygen, '_DiscoverImages')
    self.mox.StubOutWithMock(paygen, '_DiscoverNmoBuild')
    self.mox.StubOutWithMock(paygen, '_DiscoverActiveFsiBuilds')

    paygen.BUILD_DISCOVER_RETRY_SLEEP = 0

    # Check that we retry 3 times.
    paygen._DiscoverImages(paygen._build).AndRaise(
        paygen_build_lib.ImageMissing())
    paygen._DiscoverImages(paygen._build).AndRaise(
        paygen_build_lib.ImageMissing())
    paygen._DiscoverImages(paygen._build).AndRaise(
        paygen_build_lib.ImageMissing())
    paygen._DiscoverImages(paygen._build).AndRaise(
        paygen_build_lib.ImageMissing())

    # Run the test verification.
    self.mox.ReplayAll()

    with self.assertRaises(paygen_build_lib.BuildNotReady):
      paygen._DiscoverRequiredPayloads()

  def testDiscoverRequiredPayloads(self):
    """Test _DiscoverRequiredPayloads."""

    paygen = self._GetPaygenBuildInstance()

    output_uri = 'gs://foo'

    self.mox.StubOutWithMock(paygen, '_DiscoverImages')
    self.mox.StubOutWithMock(paygen, '_DiscoverTestImageArchives')
    self.mox.StubOutWithMock(paygen, '_DiscoverNmoBuild')
    self.mox.StubOutWithMock(paygen, '_DiscoverActiveFsiBuilds')
    self.mox.StubOutWithMock(paygen_payload_lib, 'DefaultPayloadUri')

    nmo_build = gspaths.Build(bucket='crt',
                              channel='foo-channel',
                              board='foo-board',
                              version='1.2.2')
    fsi1_build = gspaths.Build(bucket='crt',
                               channel='foo-channel',
                               board='foo-board',
                               version='1.0.0')
    fsi2_build = gspaths.Build(bucket='crt',
                               channel='foo-channel',
                               board='foo-board',
                               version='1.1.0')

    nmo_images = self._GetBuildImages(nmo_build)
    nmo_test_image = self._GetBuildTestImage(nmo_build)
    fsi1_images = self._GetBuildImages(fsi1_build)
    fsi1_test_image = self._GetBuildTestImage(fsi1_build)
    fsi2_images = self._GetBuildImages(fsi2_build)
    fsi2_test_image = self._GetBuildTestImage(fsi2_build)

    paygen._DiscoverImages(paygen._build).AndReturn(self.images)
    paygen._DiscoverTestImageArchives(paygen._build).AndReturn(
        [self.test_image])
    paygen._DiscoverNmoBuild().AndReturn([nmo_build])
    paygen._DiscoverActiveFsiBuilds().AndReturn([fsi1_build, fsi2_build])
    paygen._DiscoverImages(nmo_build).AndReturn(nmo_images)
    paygen._DiscoverTestImageArchives(nmo_build).AndReturn([nmo_test_image])
    paygen._DiscoverImages(fsi1_build).AndReturn(fsi1_images)
    paygen._DiscoverTestImageArchives(fsi1_build).AndReturn([fsi1_test_image])
    paygen._DiscoverImages(fsi2_build).AndReturn(fsi2_images)
    paygen._DiscoverTestImageArchives(fsi2_build).AndReturn([fsi2_test_image])

    # Simplify the output URIs, so it's easy to check them below.
    paygen_payload_lib.DefaultPayloadUri(
        mox.IsA(gspaths.Payload), None).MultipleTimes().AndReturn(output_uri)

    # Run the test verification.
    self.mox.ReplayAll()

    results = paygen._DiscoverRequiredPayloads()

    expected = [gspaths.Payload(tgt_image=self.basic_image, uri=output_uri),
                gspaths.Payload(tgt_image=self.npo_image, uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_image, uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_npo_image, uri=output_uri),
                # NPO Deltas
                gspaths.Payload(tgt_image=self.npo_image,
                                src_image=self.basic_image,
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_npo_image,
                                src_image=self.premp_image,
                                uri=output_uri),
                # NMO Delta
                gspaths.Payload(tgt_image=self.basic_image,
                                src_image=nmo_images[0],
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_image,
                                src_image=nmo_images[1],
                                uri=output_uri),
                # FSI Deltas
                gspaths.Payload(tgt_image=self.basic_image,
                                src_image=fsi1_images[0],
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_image,
                                src_image=fsi1_images[1],
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.basic_image,
                                src_image=fsi2_images[0],
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_image,
                                src_image=fsi2_images[1],
                                uri=output_uri),

                # Test full payload.
                gspaths.Payload(tgt_image=self.test_image,
                                uri=output_uri),

                # Test NPO delta.
                gspaths.Payload(tgt_image=self.test_image,
                                src_image=self.test_image,
                                uri=output_uri),

                # Test NMO delta.
                gspaths.Payload(tgt_image=self.test_image,
                                src_image=nmo_test_image,
                                uri=output_uri),

                # Test FSI deltas.
                gspaths.Payload(tgt_image=self.test_image,
                                src_image=fsi1_test_image,
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.test_image,
                                src_image=fsi2_test_image,
                                uri=output_uri)]
    expected = zip(expected, itertools.repeat(False))

    self.assertItemsEqual(sorted(results), sorted(expected))

  def testDiscoverRequiredPayloadsPreviousSkipped(self):
    """Test _DiscoverRequiredPayload.

    Ensures that no test delta payload is generated if generation of a
    signed delta from the same build was skipped.
    """

    paygen = self._GetPaygenBuildInstance()

    output_uri = 'gs://foo'

    self.mox.StubOutWithMock(paygen, '_DiscoverImages')
    self.mox.StubOutWithMock(paygen, '_DiscoverTestImageArchives')
    self.mox.StubOutWithMock(paygen, '_DiscoverNmoBuild')
    self.mox.StubOutWithMock(paygen, '_DiscoverActiveFsiBuilds')
    self.mox.StubOutWithMock(paygen_payload_lib, 'DefaultPayloadUri')

    nmo_build = gspaths.Build(bucket='crt',
                              channel='foo-channel',
                              board='foo-board',
                              version='1.2.2')
    fsi1_build = gspaths.Build(bucket='crt',
                               channel='foo-channel',
                               board='foo-board',
                               version='1.0.0')
    fsi2_build = gspaths.Build(bucket='crt',
                               channel='foo-channel',
                               board='foo-board',
                               version='1.1.0')

    fsi1_images = self._GetBuildImages(fsi1_build)
    fsi1_test_image = self._GetBuildTestImage(fsi1_build)
    fsi2_images = self._GetBuildImages(fsi2_build)
    fsi2_test_image = self._GetBuildTestImage(fsi2_build)

    paygen._DiscoverImages(paygen._build).AndReturn(self.images)
    paygen._DiscoverTestImageArchives(paygen._build).AndReturn(
        [self.test_image])
    paygen._DiscoverNmoBuild().AndReturn([nmo_build])
    paygen._DiscoverActiveFsiBuilds().AndReturn([fsi1_build, fsi2_build])
    paygen._DiscoverImages(nmo_build).AndRaise(
        paygen_build_lib.ImageMissing('nmo build is missing some image'))
    # _DiscoverTestImageArchives(nmo_build) should NOT be called.
    paygen._DiscoverImages(fsi1_build).AndReturn(fsi1_images)
    paygen._DiscoverTestImageArchives(fsi1_build).AndReturn([fsi1_test_image])
    paygen._DiscoverImages(fsi2_build).AndReturn(fsi2_images)
    paygen._DiscoverTestImageArchives(fsi2_build).AndReturn([fsi2_test_image])

    # Simplify the output URIs, so it's easy to check them below.
    paygen_payload_lib.DefaultPayloadUri(
        mox.IsA(gspaths.Payload), None).MultipleTimes().AndReturn(output_uri)

    # Run the test verification.
    self.mox.ReplayAll()

    results = paygen._DiscoverRequiredPayloads()

    # IMPORTANT: we intentionally omit the NMO payload from the expected list
    # of payloads as it is a duplicate of one of the FSIs.
    expected = [gspaths.Payload(tgt_image=self.basic_image, uri=output_uri),
                gspaths.Payload(tgt_image=self.npo_image, uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_image, uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_npo_image, uri=output_uri),
                # NPO Deltas
                gspaths.Payload(tgt_image=self.npo_image,
                                src_image=self.basic_image,
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_npo_image,
                                src_image=self.premp_image,
                                uri=output_uri),
                # FSI Deltas
                gspaths.Payload(tgt_image=self.basic_image,
                                src_image=fsi1_images[0],
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_image,
                                src_image=fsi1_images[1],
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.basic_image,
                                src_image=fsi2_images[0],
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_image,
                                src_image=fsi2_images[1],
                                uri=output_uri),

                # Test full payload.
                gspaths.Payload(tgt_image=self.test_image,
                                uri=output_uri),

                # Test NPO delta.
                gspaths.Payload(tgt_image=self.test_image,
                                src_image=self.test_image,
                                uri=output_uri),

                # Test FSI deltas.
                gspaths.Payload(tgt_image=self.test_image,
                                src_image=fsi1_test_image,
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.test_image,
                                src_image=fsi2_test_image,
                                uri=output_uri)]
    expected = zip(expected, itertools.repeat(False))

    self.assertItemsEqual(sorted(results), sorted(expected))

  def testDiscoverRequiredPayloadsNmoIsAlsoFsi(self):
    """Test _DiscoverRequiredPayloads."""

    paygen = self._GetPaygenBuildInstance()

    output_uri = 'gs://foo'

    self.mox.StubOutWithMock(paygen, '_DiscoverImages')
    self.mox.StubOutWithMock(paygen, '_DiscoverTestImageArchives')
    self.mox.StubOutWithMock(paygen, '_DiscoverNmoBuild')
    self.mox.StubOutWithMock(paygen, '_DiscoverActiveFsiBuilds')
    self.mox.StubOutWithMock(paygen_payload_lib, 'DefaultPayloadUri')

    nmo_build = gspaths.Build(bucket='crt',
                              channel='foo-channel',
                              board='foo-board',
                              version='1.2.2')
    fsi1_build = gspaths.Build(bucket='crt',
                               channel='foo-channel',
                               board='foo-board',
                               version='1.0.0')
    fsi2_build = gspaths.Build(bucket='crt',
                               channel='foo-channel',
                               board='foo-board',
                               version='1.2.2')

    fsi1_images = self._GetBuildImages(fsi1_build)
    fsi1_test_image = self._GetBuildTestImage(fsi1_build)
    fsi2_images = self._GetBuildImages(fsi2_build)
    fsi2_test_image = self._GetBuildTestImage(fsi2_build)

    paygen._DiscoverImages(paygen._build).AndReturn(self.images)
    paygen._DiscoverTestImageArchives(paygen._build).AndReturn(
        [self.test_image])
    paygen._DiscoverActiveFsiBuilds().AndReturn([fsi1_build, fsi2_build])
    paygen._DiscoverNmoBuild().AndReturn([nmo_build])
    paygen._DiscoverImages(fsi1_build).AndReturn(fsi1_images)
    paygen._DiscoverImages(fsi2_build).AndReturn(fsi2_images)
    paygen._DiscoverTestImageArchives(fsi1_build).AndReturn([fsi1_test_image])
    paygen._DiscoverTestImageArchives(fsi2_build).AndReturn([fsi2_test_image])

    # Simplify the output URIs, so it's easy to check them below.
    paygen_payload_lib.DefaultPayloadUri(
        mox.IsA(gspaths.Payload), None).MultipleTimes().AndReturn(output_uri)

    # Run the test verification.
    self.mox.ReplayAll()

    results = paygen._DiscoverRequiredPayloads()

    expected = [gspaths.Payload(tgt_image=self.basic_image, uri=output_uri),
                gspaths.Payload(tgt_image=self.npo_image, uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_image, uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_npo_image, uri=output_uri),
                # NPO Deltas
                gspaths.Payload(tgt_image=self.npo_image,
                                src_image=self.basic_image,
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_npo_image,
                                src_image=self.premp_image,
                                uri=output_uri),
                # FSI Deltas
                gspaths.Payload(tgt_image=self.basic_image,
                                src_image=fsi1_images[0],
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_image,
                                src_image=fsi1_images[1],
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.basic_image,
                                src_image=fsi2_images[0],
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.premp_image,
                                src_image=fsi2_images[1],
                                uri=output_uri),

                # Test full payload.
                gspaths.Payload(tgt_image=self.test_image,
                                uri=output_uri),

                # Test NPO delta.
                gspaths.Payload(tgt_image=self.test_image,
                                src_image=self.test_image,
                                uri=output_uri),

                # Test FSI deltas.
                gspaths.Payload(tgt_image=self.test_image,
                                src_image=fsi1_test_image,
                                uri=output_uri),
                gspaths.Payload(tgt_image=self.test_image,
                                src_image=fsi2_test_image,
                                uri=output_uri)]

    expected = zip(expected, itertools.repeat(False))

    self.assertItemsEqual(sorted(results), sorted(expected))

  def testFindFullTestPayloads(self):
    paygen = self._GetPaygenBuildInstance()

    self.mox.StubOutWithMock(urilib, 'ListFiles')

    urilib.ListFiles(
        'gs://crt/find_channel/foo-board/find_full_version/payloads/'
        'chromeos_find_full_version_foo-board_find_channel_full_test.bin-*'
    ).AndReturn(['foo', 'foo.json', 'foo.log', 'bar'])

    urilib.ListFiles(
        'gs://crt/diff_channel/foo-board/find_full_version/payloads/'
        'chromeos_find_full_version_foo-board_diff_channel_full_test.bin-*'
    ).AndReturn(['foo'])

    # Run the test verification.
    self.mox.ReplayAll()

    # Call once and use mocked look up. Make sure we filter properly.
    self.assertEqual(
        paygen._FindFullTestPayloads('find_channel', 'find_full_version'),
        ['foo', 'bar'])

    # Call with different channel, which does a different lookup.
    self.assertEqual(
        paygen._FindFullTestPayloads('diff_channel', 'find_full_version'),
        ['foo'])


    # Call a second time to verify we get cached results (no lookup).
    self.assertEqual(
        paygen._FindFullTestPayloads('find_channel', 'find_full_version'),
        ['foo', 'bar'])

  def DoGeneratePayloadsTest(self, run_parallel, test_dry_run):
    """Test paygen_build_lib._GeneratePayloads."""
    paygen = paygen_build_lib._PaygenBuild(
        self.foo_build, self.tempdir,
        config_lib_unittest.MockSiteConfig(),
        dry_run=test_dry_run,
        run_parallel=run_parallel)

    basic_payload = gspaths.Payload(tgt_image=self.npo_image,
                                    src_image=self.basic_image)
    premp_payload = gspaths.Payload(tgt_image=self.premp_npo_image,
                                    src_image=self.premp_image)

    self.mox.StubOutWithMock(parallel, 'RunTasksInProcessPool')
    self.mox.StubOutWithMock(paygen_build_lib, '_GenerateSinglePayload')

    expected_payload_args = [
        (basic_payload, mox.IsA(str), True, None, test_dry_run),
        (premp_payload, mox.IsA(str), True, None, test_dry_run)
    ]

    if run_parallel:
      parallel.RunTasksInProcessPool(paygen_build_lib._GenerateSinglePayload,
                                     expected_payload_args)
    else:
      paygen_build_lib._GenerateSinglePayload(basic_payload, mox.IsA(str),
                                              True, None, test_dry_run)

      paygen_build_lib._GenerateSinglePayload(premp_payload, mox.IsA(str),
                                              True, None, test_dry_run)

    # Run the test verification.
    self.mox.ReplayAll()

    paygen._GeneratePayloads((basic_payload, premp_payload), lock=None)

    self.mox.UnsetStubs()

  def testGeneratePayloads(self):
    """Test paygen_build_lib._GeneratePayloads, no dry_run."""

    # Test every combination of the boolean arguments.
    for run_parallel in (True, False):
      for test_dry_run in (True, False):
        self.DoGeneratePayloadsTest(run_parallel, test_dry_run)

  def testGeneratePayloadInProcess(self):
    """Make sure the _GenerateSinglePayload calls into paygen_payload_lib."""

    basic_payload = gspaths.Payload(tgt_image=self.npo_image,
                                    src_image=self.basic_image)

    self.mox.StubOutWithMock(paygen_payload_lib, 'CreateAndUploadPayload')

    # Verify that we actually generate the payload.
    paygen_payload_lib.CreateAndUploadPayload(
        basic_payload,
        mox.IsA(download_cache.DownloadCache),
        work_dir=self.tempdir,
        sign=False,
        dry_run=True,
        au_generator_uri='foo.zip')

    # Run the test verification.
    self.mox.ReplayAll()

    paygen_build_lib._GenerateSinglePayload(basic_payload, self.tempdir,
                                            False, 'foo.zip', True)

  def testCleanupBuild(self):
    """Test _PaygenBuild._CleanupBuild."""
    paygen = self._GetPaygenBuildInstance()

    self.mox.StubOutWithMock(gslib, 'Remove')
    gslib.Remove('gs://crt/foo-channel/foo-board/1.2.3/payloads/signing',
                 recurse=True, ignore_no_match=True)
    self.mox.ReplayAll()

    paygen._CleanupBuild()

  def _CreatePayloadsSetup(self, skip_test_payloads=False, disable_tests=False):
    """Helper method for related CreatePayloads tests."""
    paygen = self._GetPaygenBuildInstance(skip_test_payloads=skip_test_payloads,
                                          disable_tests=disable_tests)

    self.mox.StubOutWithMock(gslock, 'Lock')
    self.mox.StubOutWithMock(gslib, 'CreateWithContents')
    self.mox.StubOutWithMock(gslib, 'Exists')
    self.mox.StubOutWithMock(gslib, 'Remove')
    self.mox.StubOutWithMock(paygen, '_DiscoverRequiredPayloads')
    self.mox.StubOutWithMock(paygen, '_MapToArchive')
    self.mox.StubOutWithMock(paygen, '_GeneratePayloads')
    self.mox.StubOutWithMock(paygen, '_AutotestPayloads')
    self.mox.StubOutWithMock(paygen, '_CreatePayloadTests')
    self.mox.StubOutWithMock(paygen, '_CleanupBuild')

    return paygen

  def testCreatePayloadsLockedBuild(self):
    """Test paygen_build_lib._GeneratePayloads if the build is locked."""
    paygen = self._CreatePayloadsSetup()
    lock_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.LOCK)

    gslock.Lock(lock_uri, dry_run=False).AndRaise(gslock.LockNotAcquired())

    # Run the test verification.
    self.mox.ReplayAll()

    with self.assertRaises(paygen_build_lib.BuildLocked):
      paygen.CreatePayloads()

  def testCreatePayloadsSkipBuild(self):
    """Test paygen_build_lib._GeneratePayloads if the build marked skip."""
    paygen = self._CreatePayloadsSetup()
    lock_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.LOCK)
    skip_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.SKIP)

    lock = self.mox.CreateMockAnything()

    gslock.Lock(lock_uri, dry_run=False).AndReturn(lock)
    lock.__enter__().AndReturn(lock)
    gslib.Exists(skip_uri).AndReturn(True)
    lock.__exit__(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(None)

    # Run the test verification.
    self.mox.ReplayAll()

    with self.assertRaises(paygen_build_lib.BuildSkip):
      paygen.CreatePayloads()

  def testCreatePayloadsFinishedBuild(self):
    """Test paygen_build_lib._GeneratePayloads if the build marked finished."""
    paygen = self._CreatePayloadsSetup()

    lock_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.LOCK)
    skip_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.SKIP)
    finished_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.FINISHED)

    lock = self.mox.CreateMockAnything()

    gslock.Lock(lock_uri, dry_run=False).AndReturn(lock)
    lock.__enter__().AndReturn(lock)
    gslib.Exists(skip_uri).AndReturn(False)
    gslib.Exists(finished_uri).AndReturn(True)
    lock.__exit__(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(None)

    # Run the test verification.
    self.mox.ReplayAll()

    with self.assertRaises(paygen_build_lib.BuildFinished):
      paygen.CreatePayloads()

  def testCreatePayloadsBuildNotReady(self):
    """Test paygen_build_lib._GeneratePayloads if not all images are there."""
    paygen = self._CreatePayloadsSetup()

    lock_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.LOCK)
    skip_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.SKIP)
    finished_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.FINISHED)

    lock = self.mox.CreateMockAnything()

    gslock.Lock(lock_uri, dry_run=False).AndReturn(lock)
    lock.__enter__().AndReturn(lock)
    gslib.Exists(skip_uri).AndReturn(False)
    gslib.Exists(finished_uri).AndReturn(False)
    paygen._DiscoverRequiredPayloads(
        ).AndRaise(paygen_build_lib.BuildNotReady())
    lock.__exit__(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(None)

    # Run the test verification.
    self.mox.ReplayAll()

    with self.assertRaises(paygen_build_lib.BuildNotReady):
      paygen.CreatePayloads()

  def testCreatePayloadsCreateFailed(self):
    """Test paygen_build_lib._GeneratePayloads if payload generation failed."""
    paygen = self._CreatePayloadsSetup()

    lock_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.LOCK)
    skip_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.SKIP)
    finished_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.FINISHED)

    lock = self.mox.CreateMockAnything()
    payload = 'foo'
    payload_list = [payload]
    payload_skip_list = [(payload, False)]
    mock_exception = Exception()

    gslock.Lock(lock_uri, dry_run=False).AndReturn(lock)
    lock.__enter__().AndReturn(lock)
    gslib.Exists(skip_uri).AndReturn(False)
    gslib.Exists(finished_uri).AndReturn(False)
    paygen._DiscoverRequiredPayloads(
        ).AndReturn(payload_skip_list)
    self.mox.StubOutWithMock(paygen_payload_lib, 'FindExistingPayloads')
    paygen_payload_lib.FindExistingPayloads(payload).AndReturn([])
    paygen._GeneratePayloads(payload_list, lock).AndRaise(mock_exception)
    lock.__exit__(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(None)

    # Run the test verification.
    self.mox.ReplayAll()

    with self.assertRaises(Exception):
      paygen.CreatePayloads()

  def testCreatePayloadsSuccess(self):
    """Test paygen_build_lib._GeneratePayloads success."""
    paygen = self._CreatePayloadsSetup()

    lock_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.LOCK)
    skip_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.SKIP)
    finished_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.FINISHED)

    lock = self.mox.CreateMockAnything()
    payload = 'foo'
    payload_list = [payload]
    payload_skip_list = [(payload, False)]

    gslock.Lock(lock_uri, dry_run=False).AndReturn(lock)
    lock.__enter__().AndReturn(lock)
    gslib.Exists(skip_uri).AndReturn(False)
    gslib.Exists(finished_uri).AndReturn(False)
    paygen._DiscoverRequiredPayloads(
        ).AndReturn(payload_skip_list)
    self.mox.StubOutWithMock(paygen_payload_lib, 'FindExistingPayloads')
    paygen_payload_lib.FindExistingPayloads(payload).AndReturn([])
    paygen._GeneratePayloads(payload_list, lock)
    paygen._MapToArchive('foo-board', '1.2.3').AndReturn(
        ('archive_board', 'archive_build', 'archive_build_uri'))
    paygen._CreatePayloadTests(['foo']).AndReturn(['Test Payloads'])
    paygen._AutotestPayloads(['Test Payloads'])

    paygen._CleanupBuild()
    gslib.CreateWithContents(finished_uri, mox.IgnoreArg())
    lock.__exit__(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(None)

    # Run the test verification.
    self.mox.ReplayAll()

    paygen.CreatePayloads()

  def testCreatePayloadsAlreadyExists(self):
    """Test paygen_build_lib._GeneratePayloads success."""
    paygen = self._CreatePayloadsSetup()

    lock_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.LOCK)
    skip_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.SKIP)
    finished_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.FINISHED)

    lock = self.mox.CreateMockAnything()
    self.mox.StubOutWithMock(paygen_payload_lib, 'FindExistingPayloads')
    self.mox.StubOutWithMock(paygen_payload_lib, 'SetPayloadUri')
    payload_existing = 'foo'
    payload_new = 'bar'
    payload_list = [(payload_existing, False), (payload_new, False)]

    gslock.Lock(lock_uri, dry_run=False).AndReturn(lock)
    lock.__enter__().AndReturn(lock)
    gslib.Exists(skip_uri).AndReturn(False)
    gslib.Exists(finished_uri).AndReturn(False)
    paygen._DiscoverRequiredPayloads(
        ).AndReturn(payload_list)
    paygen_payload_lib.FindExistingPayloads(payload_existing).AndReturn(
        [payload_existing])
    paygen_payload_lib.FindExistingPayloads(payload_new).AndReturn([])
    paygen_payload_lib.SetPayloadUri(payload_existing, payload_existing)
    paygen._GeneratePayloads([payload_new], lock)
    paygen._MapToArchive('foo-board', '1.2.3').AndReturn(
        ('archive_board', 'archive_build', 'archive_build_uri'))
    paygen._CreatePayloadTests(['foo', 'bar']).AndReturn(['Test Payloads'])
    paygen._AutotestPayloads(['Test Payloads'])
    gslib.CreateWithContents(finished_uri, mox.IgnoreArg())
    paygen._CleanupBuild()
    lock.__exit__(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(None)

    # Run the test verification.
    self.mox.ReplayAll()

    paygen.CreatePayloads()

  def testCreatePayloadsSkipTests(self):
    """Test paygen_build_lib._GeneratePayloads success."""
    paygen = self._CreatePayloadsSetup(skip_test_payloads=True,
                                       disable_tests=True)

    lock_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.LOCK)
    skip_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.SKIP)
    finished_uri = paygen._GetFlagURI(gspaths.ChromeosReleases.FINISHED)

    lock = self.mox.CreateMockAnything()
    payload = 'foo'
    payload_list = [payload]
    payload_skip_list = [(payload, False)]

    gslock.Lock(lock_uri, dry_run=False).AndReturn(lock)
    lock.__enter__().AndReturn(lock)
    gslib.Exists(skip_uri).AndReturn(False)
    gslib.Exists(finished_uri).AndReturn(False)
    paygen._DiscoverRequiredPayloads(
        ).AndReturn(payload_skip_list)
    self.mox.StubOutWithMock(paygen_payload_lib, 'FindExistingPayloads')
    paygen_payload_lib.FindExistingPayloads(payload).AndReturn([])
    paygen._GeneratePayloads(payload_list, lock)
    paygen._CleanupBuild()
    gslib.CreateWithContents(finished_uri, mox.IgnoreArg())
    lock.__exit__(
        mox.IgnoreArg(), mox.IgnoreArg(), mox.IgnoreArg()).AndReturn(None)

    # Run the test verification.
    self.mox.ReplayAll()

    paygen.CreatePayloads()

  def testFindControlFileDir(self):
    """Test that we find control files in the proper directory."""
    # Test default dir in /tmp.
    result = paygen_build_lib._FindControlFileDir(None)
    self.assertTrue(os.path.isdir(result))
    tempdir = tempfile.tempdir or '/tmp'
    self.assertTrue(result.startswith(tempdir + '/'))
    shutil.rmtree(result)

    # Test in specified dir.
    result = paygen_build_lib._FindControlFileDir(self.tempdir)
    self.assertTrue(os.path.isdir(result))
    self.assertTrue(result.startswith(
        os.path.join(self.tempdir, 'paygen_build-control_files')))

  @unittest.skipIf(not paygen_build_lib.config,
                   'Internal crostools repository needed.')
  @unittest.skipIf(not paygen_build_lib.test_control,
                   'Autotest repository needed.')
  def testEmitControlFile(self):
    """Test that we emit control files correctly."""
    payload = gspaths.Payload(tgt_image=self.npo_image,
                              src_image=self.basic_image)

    site_config = config_lib_unittest.MockSiteConfig()
    site_config.Add('build_to_introduce_boards',
                    boards=['foo_board'])

    suite_name = 'paygen_foo'
    control_dir = tempfile.mkdtemp(prefix='control_dir-')
    paygen = paygen_build_lib._PaygenBuild(
        self.foo_build, self.tempdir, site_config,
        control_dir=control_dir)
    with tempfile.NamedTemporaryFile(prefix='control_file-', delete=False) as f:
      control_file_name = f.name
      f.write("""
AUTHOR = "Chromium OS"
NAME = "autoupdate_EndToEndTest"
TIME = "MEDIUM"
TEST_CATEGORY = "Functional"
TEST_CLASS = "platform"
TEST_TYPE = "server"
DOC = "Faux doc"

""")

    self.mox.StubOutWithMock(urilib, 'ListFiles')
    urilib.ListFiles(
        gspaths.ChromeosReleases.PayloadUri(
            self.basic_image.channel, self.basic_image.board,
            self.basic_image.version,
            '*', bucket=self.basic_image.bucket)).AndReturn(
                ['gs://foo/bar.tar.bz2'])
    urilib.ListFiles(
        gspaths.ChromeosImageArchive.BuildUri(
            'foo_board', '*', self.basic_image.version)).AndReturn(
                ['gs://foo-archive/src-build'])

    self.mox.StubOutWithMock(
        paygen_build_lib.test_control, 'get_control_file_name')
    paygen_build_lib.test_control.get_control_file_name().AndReturn(
        control_file_name)

    self.mox.ReplayAll()

    payload_test = paygen_build_lib._PaygenBuild.PayloadTest(payload)
    paygen._EmitControlFile(payload_test, suite_name, control_dir)

    shutil.rmtree(control_dir)
    os.remove(control_file_name)

  def testAutotestPayloads(self):
    """Test the process of scheduling HWLab tests."""
    control_dir = '/tmp/control_dir'
    paygen = paygen_build_lib._PaygenBuild(
        self.foo_build, self.tempdir, config_lib_unittest.MockSiteConfig(),
        control_dir=control_dir)
    control_dump_dir = os.path.join(control_dir, paygen.CONTROL_FILE_SUBDIR)
    payloads = ['foo', 'bar']
    test_channel = self.foo_build.channel.rpartition('-')[0]
    suite_name = paygen.PAYGEN_AU_SUITE_TEMPLATE % test_channel
    tarball_name = paygen.CONTROL_TARBALL_TEMPLATE % test_channel
    tarball_path = os.path.join(control_dir, tarball_name)
    test_archive_build = '%s-release/R99-%s' % (self.foo_build.board,
                                                self.foo_build.version)
    test_archive_build_uri = ('gs://chromeos-image-archive/%s' %
                              test_archive_build)
    test_upload_path = os.path.join(test_archive_build_uri, tarball_name)

    self.mox.StubOutWithMock(os, 'makedirs')
    os.makedirs(os.path.join(control_dir, paygen.CONTROL_FILE_SUBDIR))

    self.mox.StubOutWithMock(paygen, '_EmitControlFile')
    paygen._EmitControlFile('foo', suite_name, control_dump_dir)
    paygen._EmitControlFile('bar', suite_name, control_dump_dir)

    self.mox.StubOutWithMock(cros_build_lib, 'CreateTarball')
    cros_build_lib.CreateTarball(
        tarball_path, control_dir,
        compression=cros_build_lib.COMP_BZIP2,
        inputs=[paygen.CONTROL_FILE_SUBDIR]).AndReturn(
            cros_build_lib.CommandResult(returncode=0))

    # Setup preliminary values needed for running autotests.
    paygen._archive_board = self.foo_build.board
    paygen._archive_build = test_archive_build
    paygen._archive_build_uri = test_archive_build_uri

    self.mox.StubOutWithMock(gslib, 'Copy')
    gslib.Copy(tarball_path, test_upload_path, acl='public-read')

    # Both utils and cros_build_lib versions of RunCommand exist. For now, stub
    # them both out just to be safe (don't want unit tests running actual
    # commands).
    # TODO(garnold) remove the dryrun argument.
    self.mox.StubOutWithMock(utils, 'RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')

    timeout_mins = config_lib.HWTestConfig.DEFAULT_HW_TEST_TIMEOUT / 60
    expected_command = [
        mox.StrContains('site_utils/run_suite.py'),
        '--board', 'foo-board',
        '--build', 'foo-board-release/R99-1.2.3',
        '--suite_name', 'paygen_au_foo',
        '--file_bugs', 'True',
        '--pool', 'bvt',
        '--retry', 'True',
        '--timeout_mins', str(timeout_mins),
        '--no_wait', 'False',
        '--suite_min_duts', '2']

    job_id_output = '''
Autotest instance: cautotest
02-23-2015 [06:26:51] Submitted create_suite_job rpc
02-23-2015 [06:26:53] Created suite job: http://cautotest.corp.google.com/afe/#tab_id=view_job&object_id=26960110
@@@STEP_LINK@Suite created@http://cautotest.corp.google.com/afe/#tab_id=view_job&object_id=26960110@@@
The suite job has another 3:09:50.012887 till timeout.
The suite job has another 2:39:39.789250 till timeout.
    '''

    cros_build_lib.RunCommand(
        expected_command + ['-c'], capture_output=True,
        combine_stdout_stderr=True).AndReturn(
            utils.CommandResult(returncode=0, output=job_id_output))
    cros_build_lib.RunCommand(
        expected_command + ['-m', '26960110']).AndReturn(utils.CommandResult(
            returncode=0,
            output=job_id_output))


    self.mox.ReplayAll()

    paygen._AutotestPayloads(payloads)

  def testScheduleAutotestTestsNormal(self):
    """Test scheduling autotest tests with run_suite.py."""
    paygen = paygen_build_lib._PaygenBuild(
        self.foo_build, self.tempdir,
        config_lib_unittest.MockSiteConfig())

    self.mox.StubOutWithMock(commands, 'RunHWTestSuite')
    self.mox.StubOutWithMock(utils, 'RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')

    timeout_mins = config_lib.HWTestConfig.DEFAULT_HW_TEST_TIMEOUT / 60
    expected_command = [
        mox.StrContains('site_utils/run_suite.py'),
        '--board', 'foo-board',
        '--build', 'foo-board-release/R99-1.2.3',
        '--suite_name', 'paygen_au_foo',
        '--file_bugs', 'True',
        '--pool', 'bvt',
        '--retry', 'True',
        '--timeout_mins', str(timeout_mins),
        '--no_wait', 'False',
        '--suite_min_duts', '2']
    cros_build_lib.RunCommand(
        expected_command + ['-c'], capture_output=True,
        combine_stdout_stderr=True).AndReturn(
            utils.CommandResult(returncode=0, output=''))

    self.mox.ReplayAll()

    # Setup preliminary values needed for scheduling autotests.
    paygen._archive_board = 'foo-board'
    paygen._archive_build = 'foo-board-release/R99-1.2.3'

    paygen._ScheduleAutotestTests('paygen_au_foo')

  def testScheduleAutotestTestsBuilderEnvironment(self):
    """Test scheduling autotest tests with build autotest proxy."""
    paygen = paygen_build_lib._PaygenBuild(
        self.foo_build, self.tempdir,
        config_lib_unittest.MockSiteConfig(), run_on_builder=True)

    self.mox.StubOutWithMock(commands, 'RunHWTestSuite')
    self.mox.StubOutWithMock(utils, 'RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')

    timeout_mins = config_lib.HWTestConfig.DEFAULT_HW_TEST_TIMEOUT / 60
    paygen_build_lib.commands.RunHWTestSuite(
        board='foo-board', build='foo-board-release/R99-1.2.3', file_bugs=True,
        pool='bvt', priority=constants.HWTEST_BUILD_PRIORITY,
        suite='paygen_au_foo', timeout_mins=timeout_mins,
        retry=True, wait_for_results=True, suite_min_duts=2, debug=False)

    self.mox.ReplayAll()

    # Setup preliminary values needed for scheduling autotests.
    paygen._archive_board = 'foo-board'
    paygen._archive_build = 'foo-board-release/R99-1.2.3'

    paygen._ScheduleAutotestTests('paygen_au_foo')

  def testScheduleAutotestTestsBuilderEnvironmentWarn(self):
    """Test scheduling autotest tests with build autotest proxy."""
    paygen = paygen_build_lib._PaygenBuild(
        self.foo_build, self.tempdir,
        config_lib_unittest.MockSiteConfig(), run_on_builder=True)

    self.mox.StubOutWithMock(commands, 'RunHWTestSuite')
    self.mox.StubOutWithMock(utils, 'RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')

    timeout_mins = config_lib.HWTestConfig.DEFAULT_HW_TEST_TIMEOUT / 60
    paygen_build_lib.commands.RunHWTestSuite(
        board='foo-board', build='foo-board-release/R99-1.2.3', file_bugs=True,
        pool='bvt', priority=constants.HWTEST_BUILD_PRIORITY,
        suite='paygen_au_foo', timeout_mins=timeout_mins,
        retry=True, wait_for_results=True, suite_min_duts=2,
        debug=False).AndRaise(
            failures_lib.TestWarning('** Suite passed with a warning code **'))

    self.mox.ReplayAll()

    # Setup preliminary values needed for scheduling autotests.
    paygen._archive_board = 'foo-board'
    paygen._archive_build = 'foo-board-release/R99-1.2.3'

    paygen._ScheduleAutotestTests('paygen_au_foo')

  def testMapToArchive(self):
    """Test that mapping to images archive names/locations works."""
    self.mox.StubOutWithMock(urilib, 'ListFiles')
    urilib.ListFiles(
        gspaths.ChromeosImageArchive.BuildUri(
            'foo_board', '*', '1.2.3')).AndReturn(
                ['gs://foo-archive/foo_board/R11-1.2.3/somefile'])

    self.mox.ReplayAll()

    site_config = config_lib_unittest.MockSiteConfig()
    site_config.Add('build_to_introduce_boards',
                    boards=['foo_board', 'bar_board', 'bar-board'])

    paygen = paygen_build_lib._PaygenBuild(
        self.foo_build, self.tempdir, site_config)

    # Case 1: mapping successful.
    self.assertEqual(
        paygen._MapToArchive('foo-board', '1.2.3'),
        ('foo_board', 'foo_board/R11-1.2.3',
         'gs://foo-archive/foo_board/R11-1.2.3'))

    # Case 2: failure, too many build board names found.
    with self.assertRaises(paygen_build_lib.ArchiveError):
      paygen._MapToArchive('bar-board', '1.2.3')

    # Case 3: failure, build board name not found.
    with self.assertRaises(paygen_build_lib.ArchiveError):
      paygen._MapToArchive('baz-board', '1.2.3')

  def testValidateBoardConfig(self):
    """Test ValidateBoardConfig."""

    # If we are running on an external builder, we can't see the config.
    # Without the config, we can't validate.
    if not paygen_build_lib.config:
      return

    # Test a known board works.
    paygen_build_lib.ValidateBoardConfig('x86-mario')

    # Test an unknown board doesn't.
    self.assertRaises(paygen_build_lib.BoardNotConfigured,
                      paygen_build_lib.ValidateBoardConfig, 'goofy-board')

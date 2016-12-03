# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_mark_android_as_stable.py."""

from __future__ import print_function

import itertools
import mock
import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.lib import portage_util
from chromite.scripts import cros_mark_android_as_stable


class _StubCommandResult(object):
  """Helper for mocking RunCommand results."""
  def __init__(self, msg):
    self.output = msg


class CrosMarkAndroidAsStable(cros_test_lib.MockTempDirTestCase):
  """Tests for cros_mark_android_as_stable."""

  unstable_data = 'KEYWORDS="~x86 ~arm"'
  stable_data = 'KEYWORDS="x86 arm"'

  STAT_OUTPUT = """%s:
        Creation time:    Sat, 23 Aug 2014 06:53:20 GMT
        Content-Language: en
        Content-Length:   74
        Content-Type:   application/octet-stream
        Hash (crc32c):    BBPMPA==
        Hash (md5):   ms+qSYvgI9SjXn8tW/5UpQ==
        ETag:     CNCgocbmqMACEAE=
        Generation:   1408776800850000
        Metageneration:   1
      """

  def setUp(self):
    """Setup vars and create mock dir."""
    self.tmp_overlay = os.path.join(self.tempdir, 'chromiumos-overlay')
    self.mock_android_dir = os.path.join(self.tmp_overlay, constants.ANDROID_CP)

    ebuild = os.path.join(self.mock_android_dir,
                          constants.ANDROID_PN + '-%s.ebuild')
    self.unstable = ebuild % '9999'
    self.old_version = '25'
    self.old = ebuild % self.old_version
    self.old2_version = '50'
    self.old2 = ebuild % self.old2_version
    self.new_version = '100'
    self.new = ebuild % ('%s-r1' % self.new_version)
    self.partial_new_version = '150'
    self.not_new_version = '200'

    osutils.WriteFile(self.unstable, self.unstable_data, makedirs=True)
    osutils.WriteFile(self.old, self.stable_data, makedirs=True)
    osutils.WriteFile(self.old2, self.stable_data, makedirs=True)

    self.arm_acl_data = '-g google.com:READ'
    self.x86_acl_data = '-g google.com:WRITE'
    self.cts_acl_data = '-g google.com:WRITE'
    self.arm_acl = os.path.join(self.mock_android_dir,
                                'googlestorage_arm_acl.txt')
    self.x86_acl = os.path.join(self.mock_android_dir,
                                'googlestorage_x86_acl.txt')
    self.cts_acl = os.path.join(self.mock_android_dir,
                                'googlestorage_cts_acl.txt')
    self.acls = {
        'ARM': self.arm_acl,
        'X86': self.x86_acl,
        'SDK_TOOLS': self.cts_acl,
    }

    osutils.WriteFile(self.arm_acl, self.arm_acl_data, makedirs=True)
    osutils.WriteFile(self.x86_acl, self.x86_acl_data, makedirs=True)
    osutils.WriteFile(self.cts_acl, self.cts_acl_data, makedirs=True)

    self.bucket_url = 'gs://u'
    self.build_branch = 'x'
    self.gs_mock = self.StartPatcher(gs_unittest.GSContextMock())
    self.arc_bucket_url = 'gs://a'
    self.targets = constants.ANDROID_BUILD_TARGETS

    builds = {
        'ARM': [
            self.old_version, self.old2_version, self.new_version,
            self.partial_new_version
        ],
        'X86': [self.old_version, self.old2_version, self.new_version],
        'SDK_TOOLS': [
            self.old_version, self.old2_version, self.new_version,
            self.partial_new_version
        ],
    }
    for build_type, builds in builds.iteritems():
      url = self.makeSrcTargetUrl(self.targets[build_type][0])
      builds = '\n'.join(os.path.join(url, version) for version in builds)
      self.gs_mock.AddCmdResult(['ls', '--', url], output=builds)

    for version in [self.old_version, self.old2_version, self.new_version]:
      for key in self.targets.iterkeys():
        self.setupMockBuild(key, version)
    self.new_subpaths = {
        'ARM': 'linux-cheets_arm-user100',
        'X86': 'linux-cheets_x86-user100',
        'SDK_TOOLS': 'linux-static_sdk_tools100',
    }

    self.setupMockBuild('ARM', self.partial_new_version)
    self.setupMockBuild('X86', self.partial_new_version, valid=False)
    self.setupMockBuild('SDK_TOOLS', self.partial_new_version)

    for key in self.targets.iterkeys():
      self.setupMockBuild(key, self.not_new_version, False)


  def setupMockBuild(self, key, version, valid=True):
    """Helper to mock a build."""
    def _RaiseGSNoSuchKey(*_args, **_kwargs):
      raise gs.GSNoSuchKey('file does not exist')

    target = self.targets[key][0]
    src_url = self.makeSrcUrl(target, version)
    if valid:
      # Show source subpath directory.
      src_subdir = os.path.join(src_url, self.makeSubpath(target, version))
      self.gs_mock.AddCmdResult(['ls', '--', src_url], output=src_subdir)

      # Show files.
      mock_file_template_list = {
          'ARM': ['file-%(version)s.zip', 'adb'],
          'X86': ['file-%(version)s.zip', 'adb'],
          'SDK_TOOLS': ['aapt', 'adb']
      }
      filelist = [template % {'version': version}
                  for template in mock_file_template_list[key]]
      src_filelist = [os.path.join(src_subdir, filename)
                      for filename in filelist]
      self.gs_mock.AddCmdResult(['ls', '--', src_subdir],
                                output='\n'.join(src_filelist))
      for src_file in src_filelist:
        self.gs_mock.AddCmdResult(['stat', '--', src_file],
                                  output=(self.STAT_OUTPUT) % src_url)

      # Show nothing in destination.
      dst_url = self.makeDstUrl(target, version)
      dst_filelist = [os.path.join(dst_url, filename)
                      for filename in filelist]
      for dst_file in dst_filelist:
        self.gs_mock.AddCmdResult(['stat', '--', dst_file],
                                  side_effect=_RaiseGSNoSuchKey)
      logging.warn('mocking no %s', dst_url)

      # Allow copying of source to dest.
      for src_file, dst_file in itertools.izip(src_filelist, dst_filelist):
        self.gs_mock.AddCmdResult(['cp', '-v', '--', src_file, dst_file])
    else:
      self.gs_mock.AddCmdResult(['ls', '--', src_url],
                                side_effect=_RaiseGSNoSuchKey)

  def makeSrcTargetUrl(self, target):
    """Helper to return the url for a target."""
    return os.path.join(self.bucket_url,
                        '%s-%s' % (self.build_branch, target))

  def makeSrcUrl(self, target, version):
    """Helper to return the url for a build."""
    return os.path.join(self.makeSrcTargetUrl(target), version)

  def makeDstTargetUrl(self, target):
    """Helper to return the url for a target."""
    return os.path.join(self.arc_bucket_url,
                        '%s-%s' % (self.build_branch, target))

  def makeDstUrl(self, target, version):
    """Helper to return the url for a build."""
    return os.path.join(self.makeDstTargetUrl(target), version)

  def makeSubpath(self, target, version):
    """Helper to return the subpath for a build."""
    return '%s%s' % (target, version)

  def testIsBuildIdValid(self):
    """Test if checking if build valid."""
    subpaths = cros_mark_android_as_stable.IsBuildIdValid(self.bucket_url,
                                                          self.build_branch,
                                                          self.old_version,
                                                          self.targets)
    self.assertTrue(subpaths)
    self.assertEquals(len(subpaths), 3)
    self.assertEquals(subpaths['ARM'], 'linux-cheets_arm-user25')
    self.assertEquals(subpaths['X86'], 'linux-cheets_x86-user25')
    self.assertEquals(subpaths['SDK_TOOLS'], 'linux-static_sdk_tools25')

    subpaths = cros_mark_android_as_stable.IsBuildIdValid(self.bucket_url,
                                                          self.build_branch,
                                                          self.new_version,
                                                          self.targets)
    self.assertEquals(subpaths, self.new_subpaths)

    subpaths = cros_mark_android_as_stable.IsBuildIdValid(
        self.bucket_url, self.build_branch, self.partial_new_version,
        self.targets)
    self.assertEqual(subpaths, None)

    subpaths = cros_mark_android_as_stable.IsBuildIdValid(self.bucket_url,
                                                          self.build_branch,
                                                          self.not_new_version,
                                                          self.targets)
    self.assertEqual(subpaths, None)

  def testFindAndroidCandidates(self):
    """Test creation of stable ebuilds from mock dir."""
    (unstable, stable) = cros_mark_android_as_stable.FindAndroidCandidates(
        self.mock_android_dir)

    stable_ebuild_paths = [x.ebuild_path for x in stable]
    self.assertEqual(unstable.ebuild_path, self.unstable)
    self.assertEqual(len(stable), 2)
    self.assertIn(self.old, stable_ebuild_paths)
    self.assertIn(self.old2, stable_ebuild_paths)

  def testGetLatestBuild(self):
    """Test determination of latest build from gs bucket."""
    version, subpaths = cros_mark_android_as_stable.GetLatestBuild(
        self.bucket_url, self.build_branch, self.targets)
    self.assertEqual(version, self.new_version)
    self.assertTrue(subpaths)
    self.assertEquals(len(subpaths), 3)
    self.assertEquals(subpaths['ARM'], 'linux-cheets_arm-user100')
    self.assertEquals(subpaths['X86'], 'linux-cheets_x86-user100')
    self.assertEquals(subpaths['SDK_TOOLS'], 'linux-static_sdk_tools100')

  def testCopyToArcBucket(self):
    """Test copying of images to ARC bucket."""
    # Allow setting of dest acls.
    self.gs_mock.AddCmdResult(partial_mock.In('acl'))
    cros_mark_android_as_stable.CopyToArcBucket(self.bucket_url,
                                                self.build_branch,
                                                self.new_version,
                                                self.new_subpaths,
                                                self.targets,
                                                self.arc_bucket_url,
                                                self.acls)

  def testMakeAclDict(self):
    """Test generation of acls dictionary."""
    acls = cros_mark_android_as_stable.MakeAclDict(self.mock_android_dir)
    self.assertEquals(acls['ARM'], os.path.join(self.mock_android_dir,
                                                'googlestorage_acl_arm.txt'))
    self.assertEquals(acls['X86'], os.path.join(self.mock_android_dir,
                                                'googlestorage_acl_x86.txt'))

  def testGetAndroidRevisionListLink(self):
    """Test generation of revision diff list."""
    old_ebuild = portage_util.EBuild(self.old)
    old2_ebuild = portage_util.EBuild(self.old2)
    link = cros_mark_android_as_stable.GetAndroidRevisionListLink(
        self.build_branch, old_ebuild, old2_ebuild)
    self.assertEqual(link, ('http://android-build-uber.corp.google.com/'
                            'repo.html?last_bid=25&bid=50&branch=x'))

  def testMarkAndroidEBuildAsStable(self):
    """Test updating of ebuild."""
    self.PatchObject(cros_build_lib, 'RunCommand')
    git_mock = self.PatchObject(git, 'RunGit')
    commit_mock = self.PatchObject(portage_util.EBuild, 'CommitChange')
    stable_candidate = portage_util.EBuild(self.old2)
    unstable = portage_util.EBuild(self.unstable)
    android_version = self.new_version
    package_dir = self.mock_android_dir
    version_atom = cros_mark_android_as_stable.MarkAndroidEBuildAsStable(
        stable_candidate, unstable, constants.ANDROID_PN, android_version,
        package_dir, self.build_branch, self.arc_bucket_url)
    git_mock.assert_has_calls([
        mock.call(package_dir, ['add', self.new]),
        mock.call(package_dir, ['add', 'Manifest']),
    ])
    commit_mock.assert_call(mock.call('latest', package_dir))
    self.assertEqual(version_atom,
                     '%s-%s-r1' % (constants.ANDROID_CP, self.new_version))

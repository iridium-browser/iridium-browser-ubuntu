# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.core.backends.chrome import desktop_browser_finder
from telemetry.core import browser_options
from telemetry.core.platform import desktop_device
from telemetry.unittest_util import system_stub


# This file verifies the logic for finding a browser instance on all platforms
# at once. It does so by providing stubs for the OS/sys/subprocess primitives
# that the underlying finding logic usually uses to locate a suitable browser.
# We prefer this approach to having to run the same test on every platform on
# which we want this code to work.

class FindTestBase(unittest.TestCase):
  def setUp(self):
    self._finder_options = browser_options.BrowserFinderOptions()
    self._finder_options.chrome_root = '../../../'
    self._finder_stubs = system_stub.Override(desktop_browser_finder,
                                              ['os', 'subprocess', 'sys'])
    self._path_stubs = system_stub.Override(desktop_browser_finder.path,
                                            ['os', 'sys'])

  def tearDown(self):
    self._finder_stubs.Restore()
    self._path_stubs.Restore()

  @property
  def _files(self):
    return self._path_stubs.os.path.files

  def DoFindAll(self):
    return desktop_browser_finder.FindAllAvailableBrowsers(
      self._finder_options, desktop_device.DesktopDevice())

  def DoFindAllTypes(self):
    browsers = self.DoFindAll()
    return [b.browser_type for b in browsers]

  def CanFindAvailableBrowsers(self):
    return desktop_browser_finder.CanFindAvailableBrowsers()


def has_type(array, browser_type):
  return len([x for x in array if x.browser_type == browser_type]) != 0


class FindSystemTest(FindTestBase):
  def setUp(self):
    super(FindSystemTest, self).setUp()
    self._finder_stubs.sys.platform = 'win32'
    self._path_stubs.sys.platform = 'win32'

  def testFindProgramFiles(self):
    if not self.CanFindAvailableBrowsers():
      return

    self._files.append(
        'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe')
    self._path_stubs.os.program_files = 'C:\\Program Files'
    self.assertIn('system', self.DoFindAllTypes())

  def testFindProgramFilesX86(self):
    if not self.CanFindAvailableBrowsers():
      return

    self._files.append(
        'C:\\Program Files(x86)\\Google\\Chrome\\Application\\chrome.exe')
    self._path_stubs.os.program_files_x86 = 'C:\\Program Files(x86)'
    self.assertIn('system', self.DoFindAllTypes())

  def testFindLocalAppData(self):
    if not self.CanFindAvailableBrowsers():
      return

    self._files.append(
        'C:\\Local App Data\\Google\\Chrome\\Application\\chrome.exe')
    self._path_stubs.os.local_app_data = 'C:\\Local App Data'
    self.assertIn('system', self.DoFindAllTypes())


class FindLocalBuildsTest(FindTestBase):
  def setUp(self):
    super(FindLocalBuildsTest, self).setUp()
    self._finder_stubs.sys.platform = 'win32'
    self._path_stubs.sys.platform = 'win32'

  def testFindBuild(self):
    if not self.CanFindAvailableBrowsers():
      return

    self._files.append('..\\..\\..\\build\\Release\\chrome.exe')
    self.assertIn('release', self.DoFindAllTypes())

  def testFindOut(self):
    if not self.CanFindAvailableBrowsers():
      return

    self._files.append('..\\..\\..\\out\\Release\\chrome.exe')
    self.assertIn('release', self.DoFindAllTypes())

  def testFindXcodebuild(self):
    if not self.CanFindAvailableBrowsers():
      return

    self._files.append('..\\..\\..\\xcodebuild\\Release\\chrome.exe')
    self.assertIn('release', self.DoFindAllTypes())


class OSXFindTest(FindTestBase):
  def setUp(self):
    super(OSXFindTest, self).setUp()
    self._finder_stubs.sys.platform = 'darwin'
    self._path_stubs.sys.platform = 'darwin'
    self._files.append('/Applications/Google Chrome Canary.app/'
                       'Contents/MacOS/Google Chrome Canary')
    self._files.append('/Applications/Google Chrome.app/' +
                       'Contents/MacOS/Google Chrome')
    self._files.append(
      '../../../out/Release/Chromium.app/Contents/MacOS/Chromium')
    self._files.append(
      '../../../out/Debug/Chromium.app/Contents/MacOS/Chromium')
    self._files.append(
      '../../../out/Release/Content Shell.app/Contents/MacOS/Content Shell')
    self._files.append(
      '../../../out/Debug/Content Shell.app/Contents/MacOS/Content Shell')

  def testFindAll(self):
    if not self.CanFindAvailableBrowsers():
      return

    types = self.DoFindAllTypes()
    self.assertEquals(
      set(types),
      set(['debug', 'release',
           'content-shell-debug', 'content-shell-release',
           'canary', 'system']))


class LinuxFindTest(FindTestBase):
  def setUp(self):
    super(LinuxFindTest, self).setUp()

    self._finder_stubs.sys.platform = 'linux2'
    self._path_stubs.sys.platform = 'linux2'
    self._files.append('/foo/chrome')
    self._files.append('../../../out/Release/chrome')
    self._files.append('../../../out/Debug/chrome')
    self._files.append('../../../out/Release/content_shell')
    self._files.append('../../../out/Debug/content_shell')

    self.has_google_chrome_on_path = False
    this = self
    def call_hook(*args, **kwargs): # pylint: disable=W0613
      if this.has_google_chrome_on_path:
        return 0
      raise OSError('Not found')
    self._finder_stubs.subprocess.call = call_hook

  def testFindAllWithExact(self):
    if not self.CanFindAvailableBrowsers():
      return

    types = self.DoFindAllTypes()
    self.assertEquals(
        set(types),
        set(['debug', 'release',
             'content-shell-debug', 'content-shell-release']))

  def testFindWithProvidedExecutable(self):
    if not self.CanFindAvailableBrowsers():
      return

    self._finder_options.browser_executable = '/foo/chrome'
    self.assertIn('exact', self.DoFindAllTypes())

  def testFindWithProvidedApk(self):
    if not self.CanFindAvailableBrowsers():
      return

    self._finder_options.browser_executable = '/foo/chrome.apk'
    self.assertNotIn('exact', self.DoFindAllTypes())

  def testFindUsingDefaults(self):
    if not self.CanFindAvailableBrowsers():
      return

    self.has_google_chrome_on_path = True
    self.assertIn('release', self.DoFindAllTypes())

    del self._files[1]
    self.has_google_chrome_on_path = True
    self.assertIn('system', self.DoFindAllTypes())

    self.has_google_chrome_on_path = False
    del self._files[1]
    self.assertEquals(['content-shell-debug', 'content-shell-release'],
                      self.DoFindAllTypes())

  def testFindUsingRelease(self):
    if not self.CanFindAvailableBrowsers():
      return

    self.assertIn('release', self.DoFindAllTypes())


class WinFindTest(FindTestBase):
  def setUp(self):
    super(WinFindTest, self).setUp()

    self._finder_stubs.sys.platform = 'win32'
    self._path_stubs.sys.platform = 'win32'
    self._path_stubs.os.local_app_data = 'c:\\Users\\Someone\\AppData\\Local'
    self._files.append('c:\\tmp\\chrome.exe')
    self._files.append('..\\..\\..\\build\\Release\\chrome.exe')
    self._files.append('..\\..\\..\\build\\Debug\\chrome.exe')
    self._files.append('..\\..\\..\\build\\Release\\content_shell.exe')
    self._files.append('..\\..\\..\\build\\Debug\\content_shell.exe')
    self._files.append(self._path_stubs.os.local_app_data + '\\' +
                       'Google\\Chrome\\Application\\chrome.exe')
    self._files.append(self._path_stubs.os.local_app_data + '\\' +
                       'Google\\Chrome SxS\\Application\\chrome.exe')

  def testFindAllGivenDefaults(self):
    if not self.CanFindAvailableBrowsers():
      return

    types = self.DoFindAllTypes()
    self.assertEquals(set(types),
                      set(['debug', 'release',
                           'content-shell-debug', 'content-shell-release',
                           'system', 'canary']))

  def testFindAllWithExact(self):
    if not self.CanFindAvailableBrowsers():
      return

    self._finder_options.browser_executable = 'c:\\tmp\\chrome.exe'
    types = self.DoFindAllTypes()
    self.assertEquals(
        set(types),
        set(['exact',
             'debug', 'release',
             'content-shell-debug', 'content-shell-release',
             'system', 'canary']))

  def testFindAllWithExactApk(self):
    if not self.CanFindAvailableBrowsers():
      return

    self._finder_options.browser_executable = 'c:\\tmp\\chrome_shell.apk'
    types = self.DoFindAllTypes()
    self.assertEquals(
        set(types),
        set(['debug', 'release',
             'content-shell-debug', 'content-shell-release',
             'system', 'canary']))

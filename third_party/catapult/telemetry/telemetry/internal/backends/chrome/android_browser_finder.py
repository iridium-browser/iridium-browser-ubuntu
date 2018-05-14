# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Finds android browsers that can be controlled by telemetry."""

import logging
import os
import subprocess
import sys

from py_utils import dependency_util
from devil import base_error
from devil.android import apk_helper
from devil.android import flag_changer

from telemetry.core import exceptions
from telemetry.core import platform
from telemetry.core import util
from telemetry import decorators
from telemetry.internal.backends import android_browser_backend_settings
from telemetry.internal.backends.chrome import android_browser_backend
from telemetry.internal.backends.chrome import chrome_startup_args
from telemetry.internal.browser import browser
from telemetry.internal.browser import possible_browser
from telemetry.internal.platform import android_device
from telemetry.internal.util import binary_manager


CHROME_PACKAGE_NAMES = {
    'android-content-shell': [
        'org.chromium.content_shell_apk',
        android_browser_backend_settings.ContentShellBackendSettings,
        'ContentShell.apk'
    ],
    'android-webview': [
        'org.chromium.webview_shell',
        android_browser_backend_settings.WebviewBackendSettings,
        'SystemWebView.apk'
    ],
    'android-webview-instrumentation': [
        'org.chromium.android_webview.shell',
        android_browser_backend_settings.WebviewShellBackendSettings,
        'WebViewInstrumentation.apk'
    ],
    'android-chromium': [
        'org.chromium.chrome',
        android_browser_backend_settings.ChromeBackendSettings,
        'ChromePublic.apk'
    ],
    'android-chrome': [
        'com.google.android.apps.chrome',
        android_browser_backend_settings.ChromeBackendSettings, 'Chrome.apk'
    ],
    'android-chrome-beta': [
        'com.chrome.beta',
        android_browser_backend_settings.ChromeBackendSettings, None
    ],
    'android-chrome-dev': [
        'com.chrome.dev',
        android_browser_backend_settings.ChromeBackendSettings, None
    ],
    'android-chrome-canary': [
        'com.chrome.canary',
        android_browser_backend_settings.ChromeBackendSettings, None
    ],
    'android-system-chrome': [
        'com.android.chrome',
        android_browser_backend_settings.ChromeBackendSettings, None
    ],
}


class PossibleAndroidBrowser(possible_browser.PossibleBrowser):
  """A launchable android browser instance."""
  def __init__(self, browser_type, finder_options, android_platform,
               backend_settings, apk_name):
    super(PossibleAndroidBrowser, self).__init__(
        browser_type, 'android', backend_settings.supports_tab_control)
    assert browser_type in FindAllBrowserTypes(finder_options), (
        'Please add %s to android_browser_finder.FindAllBrowserTypes' %
        browser_type)
    self._platform = android_platform
    self._platform_backend = (
        android_platform._platform_backend)  # pylint: disable=protected-access
    self._backend_settings = backend_settings
    self._local_apk = None
    self._flag_changer = None

    if browser_type == 'exact':
      if not os.path.exists(apk_name):
        raise exceptions.PathMissingError(
            'Unable to find exact apk %s specified by --browser-executable' %
            apk_name)
      self._local_apk = apk_name
    elif browser_type == 'reference':
      if not os.path.exists(apk_name):
        raise exceptions.PathMissingError(
            'Unable to find reference apk at expected location %s.' % apk_name)
      self._local_apk = apk_name
    elif apk_name:
      assert finder_options.chrome_root, (
          'Must specify Chromium source to use apk_name')
      chrome_root = finder_options.chrome_root
      candidate_apks = []
      for build_path in util.GetBuildDirectories(chrome_root):
        apk_full_name = os.path.join(build_path, 'apks', apk_name)
        if os.path.exists(apk_full_name):
          last_changed = os.path.getmtime(apk_full_name)
          candidate_apks.append((last_changed, apk_full_name))

      if candidate_apks:
        # Find the candidate .apk with the latest modification time.
        newest_apk_path = sorted(candidate_apks)[-1][1]
        self._local_apk = newest_apk_path

    self._webview_embedder_apk = None
    if finder_options.webview_embedder_apk:
      self._webview_embedder_apk = finder_options.webview_embedder_apk
      assert os.path.exists(self._webview_embedder_apk), (
          '%s does not exist.' % self._webview_embedder_apk)

  def __repr__(self):
    return 'PossibleAndroidBrowser(browser_type=%s)' % self.browser_type

  @property
  def settings(self):
    """Get the backend_settings for this possible browser."""
    return self._backend_settings

  @property
  def browser_directory(self):
    # On Android L+ the directory where base APK resides is also used for
    # keeping extracted native libraries and .odex. Here is an example layout:
    # /data/app/$package.apps.chrome-1/
    #                                  base.apk
    #                                  lib/arm/libchrome.so
    #                                  oat/arm/base.odex
    # Declaring this toplevel directory as 'browser_directory' allows the cold
    # startup benchmarks to flush OS pagecache for the native library, .odex and
    # the APK.
    apks = self._platform_backend.device.GetApplicationPaths(
        self._backend_settings.package)
    # A package can map to multiple APKs iff the package overrides the app on
    # the system image. Such overrides should not happen on perf bots.
    assert len(apks) == 1
    base_apk = apks[0]
    if not base_apk or not base_apk.endswith('/base.apk'):
      return None
    return base_apk[:-9]

  @property
  def profile_directory(self):
    return self._platform_backend.GetProfileDir(self._backend_settings.package)

  @property
  def last_modification_time(self):
    if self.HaveLocalAPK():
      return os.path.getmtime(self._local_apk)
    return -1

  def _GetPathsForOsPageCacheFlushing(self):
    paths_to_flush = [self.profile_directory]
    # On N+ the Monochrome is the most widely used configuration. Since Webview
    # is used often, the typical usage is closer to have the DEX and the native
    # library be resident in memory. Skip the pagecache flushing for browser
    # directory on N+.
    if self._platform_backend.device.build_version_sdk < 24:
      paths_to_flush.append(self.browser_directory)
    return paths_to_flush

  def _InitPlatformIfNeeded(self):
    pass

  def _SetupProfile(self):
    if self._browser_options.dont_override_profile:
      return
    if self._browser_options.profile_dir:
      # Push profile_dir path on the host to the device.
      self._platform_backend.PushProfile(
          self._backend_settings.package,
          self._browser_options.profile_dir)
    else:
      self._platform_backend.RemoveProfile(
          self._backend_settings.package,
          self._backend_settings.profile_ignore_list)

  def SetUpEnvironment(self, browser_options):
    super(PossibleAndroidBrowser, self).SetUpEnvironment(browser_options)
    self._platform_backend.DismissCrashDialogIfNeeded()
    device = self._platform_backend.device
    startup_args = self.GetBrowserStartupArgs(self._browser_options)
    device.adb.Logcat(clear=True)

    self._flag_changer = flag_changer.FlagChanger(
        device, self._backend_settings.command_line_name)
    self._flag_changer.ReplaceFlags(startup_args)
    # Stop any existing browser found already running on the device. This is
    # done *after* setting the command line flags, in case some other Android
    # process manages to trigger Chrome's startup before we do.
    self._platform_backend.StopApplication(self._backend_settings.package)
    self._SetupProfile()

  def _TearDownEnvironment(self):
    self._RestoreCommandLineFlags()

  def _RestoreCommandLineFlags(self):
    if self._flag_changer is not None:
      try:
        self._flag_changer.Restore()
      finally:
        self._flag_changer = None

  def Create(self):
    """Launch the browser on the device and return a Browser object."""
    return self._GetBrowserInstance(existing=False)

  def FindExistingBrowser(self):
    """Find a browser running on the device and bind a Browser object to it.

    The returned Browser object will only be bound to a running browser
    instance whose package name matches the one specified by the backend
    settings of this possible browser.

    A BrowserGoneException is raised if the browser cannot be found.
    """
    return self._GetBrowserInstance(existing=True)

  def _GetBrowserInstance(self, existing=False):
    browser_backend = android_browser_backend.AndroidBrowserBackend(
        self._platform_backend, self._browser_options,
        self.browser_directory, self.profile_directory,
        self._backend_settings)
    self._ClearCachesOnStart()
    try:
      return browser.Browser(
          browser_backend, self._platform_backend, startup_args=(),
          find_existing=existing)
    except Exception:
      exc_info = sys.exc_info()
      logging.error(
          'Failed with %s while creating Android browser.',
          exc_info[0].__name__)
      try:
        browser_backend.Close()
      except Exception: # pylint: disable=broad-except
        logging.exception('Secondary failure while closing browser backend.')

      raise exc_info[0], exc_info[1], exc_info[2]
    finally:
      # After the browser has been launched (or not) it's fine to restore the
      # command line flags on the device.
      self._RestoreCommandLineFlags()

  def GetBrowserStartupArgs(self, browser_options):
    startup_args = chrome_startup_args.GetFromBrowserOptions(browser_options)

    # TODO(crbug.com/753948): spki-list is not yet supported on WebView,
    # make this True when support is implemented.
    supports_spki_list = not isinstance(
        self._backend_settings,
        android_browser_backend_settings.WebviewBackendSettings)
    startup_args.extend(chrome_startup_args.GetReplayArgs(
        self._platform_backend.network_controller_backend,
        supports_spki_list=supports_spki_list))

    startup_args.append('--enable-remote-debugging')
    startup_args.append('--disable-fre')
    startup_args.append('--disable-external-intent-requests')

    # Need to specify the user profile directory for
    # --ignore-certificate-errors-spki-list to work.
    startup_args.append('--user-data-dir=' + self.profile_directory)

    return startup_args

  def SupportsOptions(self, browser_options):
    if len(browser_options.extensions_to_load) != 0:
      return False
    return True

  def HaveLocalAPK(self):
    return self._local_apk and os.path.exists(self._local_apk)

  def HaveWebViewEmbedderAPK(self):
    return bool(self._webview_embedder_apk)

  @decorators.Cache
  def UpdateExecutableIfNeeded(self):
    if self.HaveLocalAPK():
      logging.warn('Installing %s on device if needed.', self._local_apk)
      self.platform.InstallApplication(self._local_apk)

    if self.HaveWebViewEmbedderAPK():
      logging.warn('Installing %s on device if needed.',
                   self._webview_embedder_apk)
      self.platform.InstallApplication(self._webview_embedder_apk)


def SelectDefaultBrowser(possible_browsers):
  """Return the newest possible browser."""
  if not possible_browsers:
    return None
  return max(possible_browsers, key=lambda b: b.last_modification_time)


def CanFindAvailableBrowsers():
  return android_device.CanDiscoverDevices()


def _CanPossiblyHandlePath(apk_path):
  return apk_path and apk_path[-4:].lower() == '.apk'


def FindAllBrowserTypes(options):
  del options  # unused
  return CHROME_PACKAGE_NAMES.keys() + ['exact', 'reference']


def _FindAllPossibleBrowsers(finder_options, android_platform):
  """Testable version of FindAllAvailableBrowsers."""
  if not android_platform:
    return []
  possible_browsers = []

  # Add the exact APK if given.
  if _CanPossiblyHandlePath(finder_options.browser_executable):
    package_name = apk_helper.GetPackageName(finder_options.browser_executable)
    try:
      backend_settings = next(
          backend_settings for target_package, backend_settings, _
          in CHROME_PACKAGE_NAMES.itervalues()
          if package_name == target_package)
    except StopIteration:
      raise exceptions.UnknownPackageError(
          '%s specified by --browser-executable has an unknown package: %s' %
          (finder_options.browser_executable, package_name))

    possible_browsers.append(PossibleAndroidBrowser(
        'exact',
        finder_options,
        android_platform,
        backend_settings(package_name),
        finder_options.browser_executable))

  # Add the reference build if found.
  os_version = dependency_util.GetChromeApkOsVersion(
      android_platform.GetOSVersionName())
  arch = android_platform.GetArchName()
  try:
    reference_build = binary_manager.FetchPath(
        'chrome_stable', arch, 'android', os_version)
  except (binary_manager.NoPathFoundError,
          binary_manager.CloudStorageError):
    reference_build = None

  if reference_build and os.path.exists(reference_build):
    # TODO(aiolos): how do we stably map the android chrome_stable apk to the
    # correct package name?
    package, backend_settings, _ = CHROME_PACKAGE_NAMES['android-chrome']
    possible_browsers.append(PossibleAndroidBrowser(
        'reference',
        finder_options,
        android_platform,
        backend_settings(package),
        reference_build))

  # Add any known local versions.
  for name, package_info in CHROME_PACKAGE_NAMES.iteritems():
    package, backend_settings, apk_name = package_info
    if apk_name and not finder_options.chrome_root:
      continue
    b = PossibleAndroidBrowser(name,
                               finder_options,
                               android_platform,
                               backend_settings(package),
                               apk_name)
    if b.platform.CanLaunchApplication(package) or b.HaveLocalAPK():
      possible_browsers.append(b)
  return possible_browsers


def FindAllAvailableBrowsers(finder_options, device):
  """Finds all the possible browsers on one device.

  The device is either the only device on the host platform,
  or |finder_options| specifies a particular device.
  """
  if not isinstance(device, android_device.AndroidDevice):
    return []

  try:
    android_platform = platform.GetPlatformForDevice(device, finder_options)
    return _FindAllPossibleBrowsers(finder_options, android_platform)
  except base_error.BaseError as e:
    logging.error('Unable to find browsers on %s: %s', device.device_id, str(e))
    ps_output = subprocess.check_output(['ps', '-ef'])
    logging.error('Ongoing processes:\n%s', ps_output)
  return []

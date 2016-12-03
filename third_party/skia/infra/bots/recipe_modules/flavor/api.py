# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# pylint: disable=W0201


from recipe_engine import recipe_api

from . import android_flavor
from . import cmake_flavor
from . import coverage_flavor
from . import default_flavor
from . import gn_flavor
from . import ios_flavor
from . import pdfium_flavor
from . import valgrind_flavor
from . import xsan_flavor


TEST_EXPECTED_SKP_VERSION = '42'
TEST_EXPECTED_SVG_VERSION = '42'
TEST_EXPECTED_SK_IMAGE_VERSION = '42'

VERSION_FILE_SK_IMAGE = 'SK_IMAGE_VERSION'
VERSION_FILE_SKP = 'SKP_VERSION'
VERSION_FILE_SVG = 'SVG_VERSION'

VERSION_NONE = -1


def is_android(builder_cfg):
  """Determine whether the given builder is an Android builder."""
  return ('Android' in builder_cfg.get('extra_config', '') or
          builder_cfg.get('os') == 'Android')


def is_cmake(builder_cfg):
  return 'CMake' in builder_cfg.get('extra_config', '')


def is_ios(builder_cfg):
  return ('iOS' in builder_cfg.get('extra_config', '') or
          builder_cfg.get('os') == 'iOS')


def is_pdfium(builder_cfg):
  return 'PDFium' in builder_cfg.get('extra_config', '')


def is_valgrind(builder_cfg):
  return 'Valgrind' in builder_cfg.get('extra_config', '')


def is_xsan(builder_cfg):
  return ('ASAN' in builder_cfg.get('extra_config', '') or
          'MSAN' in builder_cfg.get('extra_config', '') or
          'TSAN' in builder_cfg.get('extra_config', ''))


class SkiaFlavorApi(recipe_api.RecipeApi):
  def get_flavor(self, builder_cfg):
    """Return a flavor utils object specific to the given builder."""
    gn = gn_flavor.GNFlavorUtils(self.m)
    if gn.supported():
      return gn

    if is_android(builder_cfg):
      return android_flavor.AndroidFlavorUtils(self.m)
    elif is_cmake(builder_cfg):
      return cmake_flavor.CMakeFlavorUtils(self.m)
    elif is_ios(builder_cfg):
      return ios_flavor.iOSFlavorUtils(self.m)
    elif is_pdfium(builder_cfg):
      return pdfium_flavor.PDFiumFlavorUtils(self.m)
    elif is_valgrind(builder_cfg):
      return valgrind_flavor.ValgrindFlavorUtils(self.m)
    elif is_xsan(builder_cfg):
      return xsan_flavor.XSanFlavorUtils(self.m)
    elif builder_cfg.get('configuration') == 'Coverage':
      return coverage_flavor.CoverageFlavorUtils(self.m)
    else:
      return default_flavor.DefaultFlavorUtils(self.m)

  def setup(self):
    self._f = self.get_flavor(self.m.vars.builder_cfg)

  def step(self, name, cmd, **kwargs):
    return self._f.step(name, cmd, **kwargs)

  def compile(self, target, **kwargs):
    return self._f.compile(target, **kwargs)

  def copy_extra_build_products(self, swarming_out_dir):
    return self._f.copy_extra_build_products(swarming_out_dir)

  @property
  def out_dir(self):
    return self._f.out_dir

  def device_path_join(self, *args):
    return self._f.device_path_join(*args)

  def copy_directory_contents_to_device(self, host_dir, device_dir):
    return self._f.copy_directory_contents_to_device(host_dir, device_dir)

  def copy_directory_contents_to_host(self, device_dir, host_dir):
    return self._f.copy_directory_contents_to_host(device_dir, host_dir)

  def copy_file_to_device(self, host_path, device_path):
    return self._f.copy_file_to_device(host_path, device_path)

  def create_clean_host_dir(self, path):
    return self._f.create_clean_host_dir(path)

  def create_clean_device_dir(self, path):
    return self._f.create_clean_device_dir(path)

  def read_file_on_device(self, path):
    return self._f.read_file_on_device(path)

  def remove_file_on_device(self, path):
    return self._f.remove_file_on_device(path)

  def install(self):
    self._f.install()
    self.device_dirs = self._f.device_dirs

    # TODO(borenet): Only copy files which have changed.
    # Resources
    self.copy_directory_contents_to_device(
        self.m.vars.resource_dir,
        self.device_dirs.resource_dir)

    self._copy_skps()
    self._copy_images()
    self._copy_svgs()

  def cleanup_steps(self):
    return self._f.cleanup_steps()

  def _copy_dir(self, host_version, version_file, tmp_dir,
                host_path, device_path, test_expected_version,
                test_actual_version):
    actual_version_file = self.m.path.join(tmp_dir, version_file)
    # Copy to device.
    device_version_file = self.device_path_join(
        self.device_dirs.tmp_dir, version_file)
    if str(actual_version_file) != str(device_version_file):
      try:
        device_version = self.read_file_on_device(device_version_file)
      except self.m.step.StepFailure:
        device_version = VERSION_NONE
      if device_version != host_version:
        self.remove_file_on_device(device_version_file)
        self.create_clean_device_dir(device_path)
        self.copy_directory_contents_to_device(
            host_path, device_path)

        # Copy the new version file.
        self.copy_file_to_device(actual_version_file, device_version_file)

  def _copy_images(self):
    """Download and copy test images if needed."""
    version_file = self.m.vars.infrabots_dir.join(
        'assets', 'skimage', 'VERSION')
    test_data = self.m.properties.get(
        'test_downloaded_sk_image_version', TEST_EXPECTED_SK_IMAGE_VERSION)
    version = self.m.run.readfile(
        version_file,
        name='Get downloaded skimage VERSION',
        test_data=test_data).rstrip()
    self.m.run.writefile(
        self.m.path.join(self.m.vars.tmp_dir, VERSION_FILE_SK_IMAGE),
        version)
    self._copy_dir(
        version,
        VERSION_FILE_SK_IMAGE,
        self.m.vars.tmp_dir,
        self.m.vars.images_dir,
        self.device_dirs.images_dir,
        test_expected_version=self.m.properties.get(
            'test_downloaded_sk_image_version',
            TEST_EXPECTED_SK_IMAGE_VERSION),
        test_actual_version=self.m.properties.get(
            'test_downloaded_sk_image_version',
            TEST_EXPECTED_SK_IMAGE_VERSION))
    return version

  def _copy_skps(self):
    """Download and copy the SKPs if needed."""
    version_file = self.m.vars.infrabots_dir.join(
        'assets', 'skp', 'VERSION')
    test_data = self.m.properties.get(
        'test_downloaded_skp_version', TEST_EXPECTED_SKP_VERSION)
    version = self.m.run.readfile(
        version_file,
        name='Get downloaded SKP VERSION',
        test_data=test_data).rstrip()
    self.m.run.writefile(
        self.m.path.join(self.m.vars.tmp_dir, VERSION_FILE_SKP),
        version)
    self._copy_dir(
        version,
        VERSION_FILE_SKP,
        self.m.vars.tmp_dir,
        self.m.vars.local_skp_dir,
        self.device_dirs.skp_dir,
        test_expected_version=self.m.properties.get(
            'test_downloaded_skp_version', TEST_EXPECTED_SKP_VERSION),
        test_actual_version=self.m.properties.get(
            'test_downloaded_skp_version', TEST_EXPECTED_SKP_VERSION))
    return version

  def _copy_svgs(self):
    """Download and copy the SVGs if needed."""
    version_file = self.m.vars.infrabots_dir.join(
        'assets', 'svg', 'VERSION')
    test_data = self.m.properties.get(
        'test_downloaded_svg_version', TEST_EXPECTED_SVG_VERSION)
    version = self.m.run.readfile(
        version_file,
        name='Get downloaded SVG VERSION',
        test_data=test_data).rstrip()
    self.m.run.writefile(
        self.m.path.join(self.m.vars.tmp_dir, VERSION_FILE_SVG),
        version)
    self._copy_dir(
        version,
        VERSION_FILE_SVG,
        self.m.vars.tmp_dir,
        self.m.vars.local_svg_dir,
        self.device_dirs.svg_dir,
        test_expected_version=self.m.properties.get(
            'test_downloaded_svg_version', TEST_EXPECTED_SVG_VERSION),
        test_actual_version=self.m.properties.get(
            'test_downloaded_svg_version', TEST_EXPECTED_SVG_VERSION))
    return version

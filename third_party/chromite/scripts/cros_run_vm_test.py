# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for running catapult smoke tests in a VM."""

from __future__ import print_function

import datetime

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.scripts import cros_vm


class VMTest(object):
  """Class for running VM tests."""

  CATAPULT_RUN_TESTS = \
      '/usr/local/telemetry/src/third_party/catapult/telemetry/bin/run_tests'
  TEST_PATTERNS = ['testBrowserCreation']
  GUEST_TEST_PATTERNS = []
  SANITY_TEST = '/usr/local/autotest/bin/vm_sanity.py'

  def __init__(self, image_path, qemu_path, enable_kvm, display, catapult_tests,
               vm_sanity, guest, start_vm, ssh_port):
    self.start_time = datetime.datetime.utcnow()
    self.test_pattern = catapult_tests
    self.vm_sanity = vm_sanity
    self.guest = guest
    self.start_vm = start_vm
    self.ssh_port = ssh_port

    self._vm = cros_vm.VM(image_path=image_path, qemu_path=qemu_path,
                          enable_kvm=enable_kvm, display=display,
                          ssh_port=ssh_port)

  def __del__(self):
    if self._vm and self.start_vm:
      self._vm.Stop()

    elapsed = datetime.datetime.utcnow() - self.start_time
    # Don't need trailing milliseconds.
    logging.info('Time elapsed %s.',
                 datetime.timedelta(seconds=elapsed.seconds))

  def StartVM(self):
    """Start a VM if necessary.

    If --start-vm is specified, we launch a new VM, otherwise we use an
    existing VM.
    """
    if not self._vm.IsRunning():
      self.start_vm = True

    if self.start_vm:
      self._vm.Start()
    self._vm.WaitForBoot()

  def Build(self, build_dir):
    """Build chrome.

    Args:
      build_dir: Build directory.
    """
    cros_build_lib.RunCommand(['ninja', '-C', build_dir, 'chrome',
                               'chrome_sandbox', 'nacl_helper'],
                              log_output=True)

  def Deploy(self, build_dir):
    """Deploy chrome.

    Args:
      build_dir: Build directory.
    """
    cros_build_lib.RunCommand(['deploy_chrome', '--build-dir', build_dir,
                               '--force', '--to', 'localhost',
                               '--port', str(self.ssh_port)], log_output=True)

  def _RunCatapultTests(self, test_pattern, guest):
    """Run catapult tests matching a pattern.

    Args:
      test_pattern: Run tests matching this pattern.
      guest: Run in guest mode.

    Returns:
      True if all tests passed.
    """

    self._vm.RemoteCommand(['chmod', '+x', self.CATAPULT_RUN_TESTS])

    browser = 'system-guest' if guest else 'system'
    result = self._vm.RemoteCommand([self.CATAPULT_RUN_TESTS,
                                     '--browser=%s' % browser,
                                     test_pattern])
    return result.returncode == 0

  def RunTests(self):
    """Run all eligible tests.

    If a test_pattern has been specified, only run those tests. Otherwise, run
    all tests in TEST_PATTERNS and GUEST_TEST_PATTERNS.

    Returns:
      True if all tests passed.
    """

    if self.vm_sanity:
      return self._vm.RemoteCommand([self.SANITY_TEST]).returncode == 0

    if self.test_pattern:
      return self._RunCatapultTests(self.test_pattern, self.guest)

    success = True
    for pattern in self.TEST_PATTERNS:
      success = self._RunCatapultTests(pattern, guest=False) and success

    for pattern in self.GUEST_TEST_PATTERNS:
      success = self._RunCatapultTests(pattern, guest=True) and success

    return success


def ParseCommandLine(argv):
  """Parse the command line.

  Args:
    argv: Command arguments.

  Returns:
    List of parsed args.
  """

  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--start-vm', action='store_true', default=False,
                      help='Start a new VM before running tests.')
  parser.add_argument('--catapult-tests',
                      help='Catapult test pattern to run, passed to run_tests.')
  parser.add_argument('--vm-sanity', action='store_true', default=False,
                      help='Run a minimal and fast sanity test that ensures '
                           'chrome starts and login works.')
  parser.add_argument('--guest', action='store_true', default=False,
                      help='Run tests in incognito mode.')
  parser.add_argument('--build-dir', type='path',
                      help='Directory for building and deploying chrome.')
  parser.add_argument('--build', action='store_true', default=False,
                      help='Before running tests, build chrome using ninja, '
                      '--build-dir must be specified.')
  parser.add_argument('--deploy', action='store_true', default=False,
                      help='Before running tests, deploy chrome to the VM, '
                      '--build-dir must be specified.')
  parser.add_argument('--image-path', type='path',
                      help='Path to VM image to launch with --start-vm.')
  parser.add_argument('--qemu-path', type='path',
                      help='Path of qemu binary to launch with --start-vm.')
  parser.add_argument('--disable-kvm', dest='enable_kvm',
                      action='store_false', default=True,
                      help='Disable KVM, use software emulation.')
  parser.add_argument('--no-display', dest='display',
                      action='store_false', default=True,
                      help='Do not display video output.')
  parser.add_argument('--ssh-port', type=int, default=cros_vm.VM.SSH_PORT,
                      help='ssh port to communicate with VM.')
  return parser.parse_args(argv)


def main(argv):
  args = ParseCommandLine(argv)
  vm_test = VMTest(image_path=args.image_path,
                   qemu_path=args.qemu_path, enable_kvm=args.enable_kvm,
                   display=args.display, catapult_tests=args.catapult_tests,
                   vm_sanity=args.vm_sanity, guest=args.guest,
                   start_vm=args.start_vm, ssh_port=args.ssh_port)

  if (args.build or args.deploy) and not args.build_dir:
    args.error('Must specifiy --build-dir with --build or --deploy.')

  vm_test.StartVM()

  if args.build:
    vm_test.Build(args.build_dir)

  if args.build or args.deploy:
    vm_test.Deploy(args.build_dir)

  success = vm_test.RunTests()
  success_str = 'succeeded' if success else 'failed'
  logging.info('Tests %s.', success_str)
  return not success

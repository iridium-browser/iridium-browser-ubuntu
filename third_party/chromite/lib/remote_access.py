# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing functions to access a remote test device."""

from __future__ import print_function

import glob
import os
import shutil
import socket
import stat
import string
import tempfile
import time

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import debug_link
from chromite.lib import osutils
from chromite.lib import timeout_util


_path = os.path.dirname(os.path.realpath(__file__))
TEST_PRIVATE_KEY = os.path.normpath(
    os.path.join(_path, '../ssh_keys/testing_rsa'))
del _path

LOCALHOST = 'localhost'
LOCALHOST_IP = '127.0.0.1'
ROOT_ACCOUNT = 'root'

REBOOT_MARKER = '/tmp/awaiting_reboot'
REBOOT_MAX_WAIT = 120
REBOOT_SSH_CONNECT_TIMEOUT = 2
REBOOT_SSH_CONNECT_ATTEMPTS = 2
CHECK_INTERVAL = 5
DEFAULT_SSH_PORT = 22
SSH_ERROR_CODE = 255

# SSH default known_hosts filepath.
KNOWN_HOSTS_PATH = os.path.expanduser('~/.ssh/known_hosts')

# Dev/test packages are installed in these paths.
DEV_BIN_PATHS = '/usr/local/bin:/usr/local/sbin'

# Brillo device.
BRILLO_DEBUG_LINK_SERVICE_NAME = '_brdebug._tcp.local'
BRILLO_DEVICE_PROPERTY_DIR = '/var/lib/brillo-device'
BRILLO_DEVICE_PROPERTY_MAX_LEN = 128
BRILLO_DEVICE_PROPERTY_ALIAS = 'alias'

# Remote device connection types.
CONNECTION_TYPE_ETHERNET = 'ethernet'
CONNECTION_TYPE_USB = 'usb'


class RemoteAccessException(Exception):
  """Base exception for this module."""


class SSHConnectionError(RemoteAccessException):
  """Raised when SSH connection has failed."""

  def IsKnownHostsMismatch(self):
    """Returns True if this error was caused by a known_hosts mismatch.

    Will only check for a mismatch, this will return False if the host
    didn't exist in known_hosts at all.
    """
    # Checking for string output is brittle, but there's no exit code that
    # indicates why SSH failed so this might be the best we can do.
    # RemoteAccess.RemoteSh() sets LC_MESSAGES=C so we only need to check for
    # the English error message.
    # Verified for OpenSSH_6.6.1p1.
    return 'REMOTE HOST IDENTIFICATION HAS CHANGED' in str(self)


class DeviceNotPingableError(RemoteAccessException):
  """Raised when device is not pingable."""


class DefaultDeviceError(RemoteAccessException):
  """Raised when a default ChromiumOSDevice can't be found."""


class CatFileError(RemoteAccessException):
  """Raised when error occurs while trying to cat a remote file."""


class RunningPidsError(RemoteAccessException):
  """Raised when unable to get running pids on the device."""


class InvalidDevicePropertyError(RemoteAccessException):
  """Raised when Brillo device property is invalid."""


def NormalizePort(port, str_ok=True):
  """Checks if |port| is a valid port number and returns the number.

  Args:
    port: The port to normalize.
    str_ok: Accept |port| in string. If set False, only accepts
      an integer. Defaults to True.

  Returns:
    A port number (integer).
  """
  err_msg = '%s is not a valid port number.' % port

  if not str_ok and not isinstance(port, int):
    raise ValueError(err_msg)

  port = int(port)
  if port <= 0 or port >= 65536:
    raise ValueError(err_msg)

  return port


def GetUnusedPort(ip=LOCALHOST, family=socket.AF_INET,
                  stype=socket.SOCK_STREAM):
  """Returns a currently unused port.

  Example:
    Note: Since this does not guarantee the port remains unused when you
    attempt to bind it, your code should retry in a loop like so:
    while True:
      try:
        port = remote_access.GetUnusedPort()
        <attempt to bind the port>
        break
      except socket.error as e:
        if e.errno == errno.EADDRINUSE:
          continue
        <fallback/raise>

  Args:
    ip: IP to use to bind the port.
    family: Address family.
    stype: Socket type.

  Returns:
    A port number (integer).
  """
  s = None
  try:
    s = socket.socket(family, stype)
    s.bind((ip, 0))
    return s.getsockname()[1]
  except (socket.error, OSError):
    if s:
      s.close()


def RunCommandFuncWrapper(func, msg, *args, **kwargs):
  """Wraps a function that invokes cros_build_lib.RunCommand.

  If the command failed, logs warning |msg| if error_code_ok is set;
  logs error |msg| if error_code_ok is not set.

  Args:
    func: The function to call.
    msg: The message to display if the command failed.
    *args: Arguments to pass to |func|.
    **kwargs: Keyword arguments to pass to |func|.

  Returns:
    The result of |func|.

  Raises:
    cros_build_lib.RunCommandError if the command failed and error_code_ok
    is not set.
  """
  error_code_ok = kwargs.pop('error_code_ok', False)
  result = func(*args, error_code_ok=True, **kwargs)
  if result.returncode != 0 and not error_code_ok:
    raise cros_build_lib.RunCommandError(msg, result)

  if result.returncode != 0:
    logging.warning(msg)


def CompileSSHConnectSettings(**kwargs):
  """Creates a list of SSH connection options.

  Any ssh_config option can be specified in |kwargs|, in addition,
  several options are set to default values if not specified. Any
  option can be set to None to prevent this function from assigning
  a value so that the SSH default value will be used.

  This function doesn't check to make sure the |kwargs| options are
  valid, so a typo or invalid setting won't be caught until the
  resulting arguments are passed into an SSH call.

  Args:
    kwargs: A dictionary of ssh_config settings.

  Returns:
    A list of arguments to pass to SSH.
  """
  settings = {
      'ConnectTimeout': 30,
      'ConnectionAttempts': 4,
      'NumberOfPasswordPrompts': 0,
      'Protocol': 2,
      'ServerAliveInterval': 10,
      'ServerAliveCountMax': 3,
      'StrictHostKeyChecking': 'no',
      'UserKnownHostsFile': '/dev/null',
  }
  settings.update(kwargs)
  return ['-o%s=%s' % (k, v) for k, v in settings.items() if v is not None]


def RemoveKnownHost(host, known_hosts_path=KNOWN_HOSTS_PATH):
  """Removes |host| from a known_hosts file.

  `ssh-keygen -R` doesn't work on bind mounted files as they can only
  be updated in place. Since we bind mount the default known_hosts file
  when entering the chroot, this function provides an alternate way
  to remove hosts from the file.

  Args:
    host: The host name to remove from the known_hosts file.
    known_hosts_path: Path to the known_hosts file to change. Defaults
                      to the standard SSH known_hosts file path.

  Raises:
    cros_build_lib.RunCommandError if ssh-keygen fails.
  """
  # `ssh-keygen -R` creates a backup file to retain the old 'known_hosts'
  # content and never deletes it. Using TempDir here to make sure both the temp
  # files created by us and `ssh-keygen -R` are deleted afterwards.
  with osutils.TempDir(prefix='remote-access-') as tempdir:
    temp_file = os.path.join(tempdir, 'temp_known_hosts')
    try:
      # Using shutil.copy2 to preserve the file ownership and permissions.
      shutil.copy2(known_hosts_path, temp_file)
    except IOError:
      # If |known_hosts_path| doesn't exist neither does |host| so we're done.
      return
    cros_build_lib.RunCommand(['ssh-keygen', '-R', host, '-f', temp_file],
                              quiet=True)
    shutil.copy2(temp_file, known_hosts_path)


class RemoteAccess(object):
  """Provides access to a remote test machine."""

  DEFAULT_USERNAME = ROOT_ACCOUNT

  def __init__(self, remote_host, tempdir, port=None, username=None,
               private_key=None, debug_level=logging.DEBUG, interactive=True):
    """Construct the object.

    Args:
      remote_host: The ip or hostname of the remote test machine.  The test
                   machine should be running a ChromeOS test image.
      tempdir: A directory that RemoteAccess can use to store temporary files.
               It's the responsibility of the caller to remove it.
      port: The ssh port of the test machine to connect to.
      username: The ssh login username (default: root).
      private_key: The identify file to pass to `ssh -i` (default: testing_rsa).
      debug_level: Logging level to use for all RunCommand invocations.
      interactive: If set to False, pass /dev/null into stdin for the sh cmd.
    """
    self.tempdir = tempdir
    self.remote_host = remote_host
    self.port = port if port else DEFAULT_SSH_PORT
    self.username = username if username else self.DEFAULT_USERNAME
    self.debug_level = debug_level
    private_key_src = private_key if private_key else TEST_PRIVATE_KEY
    self.private_key = os.path.join(
        tempdir, os.path.basename(private_key_src))

    self.interactive = interactive
    shutil.copyfile(private_key_src, self.private_key)
    os.chmod(self.private_key, stat.S_IRUSR)

  @property
  def target_ssh_url(self):
    return '%s@%s' % (self.username, self.remote_host)

  def _GetSSHCmd(self, connect_settings=None):
    if connect_settings is None:
      connect_settings = CompileSSHConnectSettings()

    cmd = (['ssh', '-p', str(self.port)] +
           connect_settings +
           ['-i', self.private_key])
    if not self.interactive:
      cmd.append('-n')

    return cmd

  def RemoteSh(self, cmd, connect_settings=None, error_code_ok=False,
               remote_sudo=False, ssh_error_ok=False, **kwargs):
    """Run a sh command on the remote device through ssh.

    Args:
      cmd: The command string or list to run. None or empty string/list will
           start an interactive session.
      connect_settings: The SSH connect settings to use.
      error_code_ok: Does not throw an exception when the command exits with a
                     non-zero returncode.  This does not cover the case where
                     the ssh command itself fails (return code 255).
                     See ssh_error_ok.
      ssh_error_ok: Does not throw an exception when the ssh command itself
                    fails (return code 255).
      remote_sudo: If set, run the command in remote shell with sudo.
      **kwargs: See cros_build_lib.RunCommand documentation.

    Returns:
      A CommandResult object.  The returncode is the returncode of the command,
      or 255 if ssh encountered an error (could not connect, connection
      interrupted, etc.)

    Raises:
      RunCommandError when error is not ignored through the error_code_ok flag.
      SSHConnectionError when ssh command error is not ignored through
      the ssh_error_ok flag.
    """
    kwargs.setdefault('capture_output', True)
    kwargs.setdefault('debug_level', self.debug_level)
    # Force English SSH messages. SSHConnectionError.IsKnownHostsMismatch()
    # requires English errors to detect a known_hosts key mismatch error.
    kwargs.setdefault('extra_env', {})['LC_MESSAGES'] = 'C'

    ssh_cmd = self._GetSSHCmd(connect_settings)
    ssh_cmd.append(self.target_ssh_url)

    if cmd:
      ssh_cmd.append('--')

      if remote_sudo and self.username != ROOT_ACCOUNT:
        # Prepend sudo to cmd.
        ssh_cmd.append('sudo')

      if isinstance(cmd, basestring):
        ssh_cmd += [cmd]
      else:
        ssh_cmd += cmd

    try:
      return cros_build_lib.RunCommand(ssh_cmd, **kwargs)
    except cros_build_lib.RunCommandError as e:
      if ((e.result.returncode == SSH_ERROR_CODE and ssh_error_ok) or
          (e.result.returncode and e.result.returncode != SSH_ERROR_CODE
           and error_code_ok)):
        return e.result
      elif e.result.returncode == SSH_ERROR_CODE:
        raise SSHConnectionError(e.result.error)
      else:
        raise

  def _CheckIfRebooted(self):
    """Checks whether a remote device has rebooted successfully.

    This uses a rapidly-retried SSH connection, which will wait for at most
    about ten seconds. If the network returns an error (e.g. host unreachable)
    the actual delay may be shorter.

    Returns:
      Whether the device has successfully rebooted.
    """
    # In tests SSH seems to be waiting rather longer than would be expected
    # from these parameters. These values produce a ~5 second wait.
    connect_settings = CompileSSHConnectSettings(
        ConnectTimeout=REBOOT_SSH_CONNECT_TIMEOUT,
        ConnectionAttempts=REBOOT_SSH_CONNECT_ATTEMPTS)
    cmd = "[ ! -e '%s' ]" % REBOOT_MARKER
    result = self.RemoteSh(cmd, connect_settings=connect_settings,
                           error_code_ok=True, ssh_error_ok=True,
                           capture_output=True)

    errors = {0: 'Reboot complete.',
              1: 'Device has not yet shutdown.',
              255: 'Cannot connect to device; reboot in progress.'}
    if result.returncode not in errors:
      raise Exception('Unknown error code %s returned by %s.'
                      % (result.returncode, cmd))

    logging.info(errors[result.returncode])
    return result.returncode == 0

  def RemoteReboot(self):
    """Reboot the remote device."""
    logging.info('Rebooting %s...', self.remote_host)
    if self.username != ROOT_ACCOUNT:
      self.RemoteSh('sudo sh -c "touch %s && sudo reboot"' % REBOOT_MARKER)
    else:
      self.RemoteSh('touch %s && reboot' % REBOOT_MARKER)

    time.sleep(CHECK_INTERVAL)
    try:
      timeout_util.WaitForReturnTrue(self._CheckIfRebooted, REBOOT_MAX_WAIT,
                                     period=CHECK_INTERVAL)
    except timeout_util.TimeoutError:
      cros_build_lib.Die('Reboot has not completed after %s seconds; giving up.'
                         % (REBOOT_MAX_WAIT,))

  def Rsync(self, src, dest, to_local=False, follow_symlinks=False,
            recursive=True, inplace=False, verbose=False, sudo=False,
            remote_sudo=False, **kwargs):
    """Rsync a path to the remote device.

    Rsync a path to the remote device. If |to_local| is set True, it
    rsyncs the path from the remote device to the local machine.

    Args:
      src: The local src directory.
      dest: The remote dest directory.
      to_local: If set, rsync remote path to local path.
      follow_symlinks: If set, transform symlinks into referent
        path. Otherwise, copy symlinks as symlinks.
      recursive: Whether to recursively copy entire directories.
      inplace: If set, cause rsync to overwrite the dest files in place.  This
        conserves space, but has some side effects - see rsync man page.
      verbose: If set, print more verbose output during rsync file transfer.
      sudo: If set, invoke the command via sudo.
      remote_sudo: If set, run the command in remote shell with sudo.
      **kwargs: See cros_build_lib.RunCommand documentation.
    """
    kwargs.setdefault('debug_level', self.debug_level)

    ssh_cmd = ' '.join(self._GetSSHCmd())
    rsync_cmd = ['rsync', '--perms', '--verbose', '--times', '--compress',
                 '--omit-dir-times', '--exclude', '.svn']
    rsync_cmd.append('--copy-links' if follow_symlinks else '--links')
    rsync_sudo = 'sudo' if (
        remote_sudo and self.username != ROOT_ACCOUNT) else ''
    rsync_cmd += ['--rsync-path',
                  'PATH=%s:$PATH %s rsync' % (DEV_BIN_PATHS, rsync_sudo)]

    if verbose:
      rsync_cmd.append('--progress')
    if recursive:
      rsync_cmd.append('--recursive')
    if inplace:
      rsync_cmd.append('--inplace')

    if to_local:
      rsync_cmd += ['--rsh', ssh_cmd,
                    '[%s]:%s' % (self.target_ssh_url, src), dest]
    else:
      rsync_cmd += ['--rsh', ssh_cmd, src,
                    '[%s]:%s' % (self.target_ssh_url, dest)]

    rc_func = cros_build_lib.RunCommand
    if sudo:
      rc_func = cros_build_lib.SudoRunCommand
    return rc_func(rsync_cmd, print_cmd=verbose, **kwargs)

  def RsyncToLocal(self, *args, **kwargs):
    """Rsync a path from the remote device to the local machine."""
    return self.Rsync(*args, to_local=kwargs.pop('to_local', True), **kwargs)

  def Scp(self, src, dest, to_local=False, recursive=True, verbose=False,
          sudo=False, **kwargs):
    """Scp a file or directory to the remote device.

    Args:
      src: The local src file or directory.
      dest: The remote dest location.
      to_local: If set, scp remote path to local path.
      recursive: Whether to recursively copy entire directories.
      verbose: If set, print more verbose output during scp file transfer.
      sudo: If set, invoke the command via sudo.
      remote_sudo: If set, run the command in remote shell with sudo.
      **kwargs: See cros_build_lib.RunCommand documentation.

    Returns:
      A CommandResult object containing the information and return code of
      the scp command.
    """
    remote_sudo = kwargs.pop('remote_sudo', False)
    if remote_sudo and self.username != ROOT_ACCOUNT:
      # TODO: Implement scp with remote sudo.
      raise NotImplementedError('Cannot run scp with sudo!')

    kwargs.setdefault('debug_level', self.debug_level)
    # scp relies on 'scp' being in the $PATH of the non-interactive,
    # SSH login shell.
    scp_cmd = (['scp', '-P', str(self.port)] +
               CompileSSHConnectSettings(ConnectTimeout=60) +
               ['-i', self.private_key])

    if not self.interactive:
      scp_cmd.append('-n')

    if recursive:
      scp_cmd.append('-r')
    if verbose:
      scp_cmd.append('-v')

    if to_local:
      scp_cmd += ['%s:%s' % (self.target_ssh_url, src), dest]
    else:
      scp_cmd += glob.glob(src) + ['%s:%s' % (self.target_ssh_url, dest)]

    rc_func = cros_build_lib.RunCommand
    if sudo:
      rc_func = cros_build_lib.SudoRunCommand

    return rc_func(scp_cmd, print_cmd=verbose, **kwargs)

  def ScpToLocal(self, *args, **kwargs):
    """Scp a path from the remote device to the local machine."""
    return self.Scp(*args, to_local=kwargs.pop('to_local', True), **kwargs)

  def PipeToRemoteSh(self, producer_cmd, cmd, **kwargs):
    """Run a local command and pipe it to a remote sh command over ssh.

    Args:
      producer_cmd: Command to run locally with its results piped to |cmd|.
      cmd: Command to run on the remote device.
      **kwargs: See RemoteSh for documentation.
    """
    result = cros_build_lib.RunCommand(producer_cmd, stdout_to_pipe=True,
                                       print_cmd=False, capture_output=True)
    return self.RemoteSh(cmd, input=kwargs.pop('input', result.output),
                         **kwargs)


class RemoteDeviceHandler(object):
  """A wrapper of RemoteDevice."""

  def __init__(self, *args, **kwargs):
    """Creates a RemoteDevice object."""
    self.device = RemoteDevice(*args, **kwargs)

  def __enter__(self):
    """Return the temporary directory."""
    return self.device

  def __exit__(self, _type, _value, _traceback):
    """Cleans up the device."""
    self.device.Cleanup()


class ChromiumOSDeviceHandler(object):
  """A wrapper of ChromiumOSDevice."""

  def __init__(self, *args, **kwargs):
    """Creates a RemoteDevice object."""
    self.device = ChromiumOSDevice(*args, **kwargs)

  def __enter__(self):
    """Return the temporary directory."""
    return self.device

  def __exit__(self, _type, _value, _traceback):
    """Cleans up the device."""
    self.device.Cleanup()


class RemoteDevice(object):
  """Handling basic SSH communication with a remote device."""

  DEFAULT_BASE_DIR = '/tmp/remote-access'

  def __init__(self, hostname, port=None, username=None,
               base_dir=DEFAULT_BASE_DIR, connect_settings=None,
               private_key=None, debug_level=logging.DEBUG, ping=True,
               connect=True):
    """Initializes a RemoteDevice object.

    Args:
      hostname: The hostname of the device.
      port: The ssh port of the device.
      username: The ssh login username.
      base_dir: The base work directory to create on the device, or
        None. Required in order to use RunCommand(), but
        BaseRunCommand() will be available in either case.
      connect_settings: Default SSH connection settings.
      private_key: The identify file to pass to `ssh -i`.
      debug_level: Setting debug level for logging.
      ping: Whether to ping the device before attempting to connect.
      connect: True to set up the connection, otherwise set up will
        be automatically deferred until device use.
    """
    self.hostname = hostname
    self.port = port
    self.username = username
    # The tempdir is for storing the rsa key and/or some temp files.
    self.tempdir = osutils.TempDir(prefix='ssh-tmp')
    self.connect_settings = (connect_settings if connect_settings else
                             CompileSSHConnectSettings())
    self.private_key = private_key
    self.debug_level = debug_level
    # The temporary work directories on the device.
    self._base_dir = base_dir
    self._work_dir = None
    # Use GetAgent() instead of accessing this directly for deferred connect.
    self._agent = None
    self.cleanup_cmds = []

    if ping and not self.Pingable():
      raise DeviceNotPingableError('Device %s is not pingable.' % self.hostname)

    if connect:
      self._Connect()

  def Pingable(self, timeout=20):
    """Returns True if the device is pingable.

    Args:
      timeout: Timeout in seconds (default: 20 seconds).

    Returns:
      True if the device responded to the ping before |timeout|.
    """
    result = cros_build_lib.RunCommand(
        ['ping', '-c', '1', '-w', str(timeout), self.hostname],
        error_code_ok=True,
        capture_output=True)
    return result.returncode == 0

  def GetAgent(self):
    """Agent accessor; connects the agent if necessary."""
    if not self._agent:
      self._Connect()
    return self._agent

  def _Connect(self):
    """Sets up the SSH connection and internal state."""
    self._agent = RemoteAccess(self.hostname, self.tempdir.tempdir,
                               port=self.port, username=self.username,
                               private_key=self.private_key)

  @property
  def work_dir(self):
    """The work directory to create on the device.

    This property exists so we can create the remote paths on demand.  For
    some use cases, it'll never be needed, so skipping creation is faster.
    """
    if self._base_dir is None:
      return None

    if self._work_dir is None:
      self._work_dir = self.BaseRunCommand(
          ['mkdir', '-p', self._base_dir, '&&',
           'mktemp', '-d', '--tmpdir=%s' % self._base_dir],
          capture_output=True).output.strip()
      logging.debug('The temporary working directory on the device is %s',
                    self._work_dir)
      self.RegisterCleanupCmd(['rm', '-rf', self._work_dir])

    return self._work_dir

  # Since this object is instantiated once per device, we can safely cache the
  # result of the rsync test.  We assume the remote side doesn't go and delete
  # or break rsync on us, but that's fine.
  @cros_build_lib.MemoizedSingleCall
  def HasRsync(self):
    """Checks if rsync exists on the device."""
    result = self.GetAgent().RemoteSh(['PATH=%s:$PATH rsync' % DEV_BIN_PATHS,
                                       '--version'], error_code_ok=True)
    return result.returncode == 0

  def RegisterCleanupCmd(self, cmd, **kwargs):
    """Register a cleanup command to be run on the device in Cleanup().

    Args:
      cmd: command to run. See RemoteAccess.RemoteSh documentation.
      **kwargs: keyword arguments to pass along with cmd. See
        RemoteAccess.RemoteSh documentation.
    """
    self.cleanup_cmds.append((cmd, kwargs))

  def Cleanup(self):
    """Remove work/temp directories and run all registered cleanup commands."""
    for cmd, kwargs in self.cleanup_cmds:
      # We want to run through all cleanup commands even if there are errors.
      kwargs.setdefault('error_code_ok', True)
      self.BaseRunCommand(cmd, **kwargs)

    self.tempdir.Cleanup()

  def CopyToDevice(self, src, dest, mode=None, **kwargs):
    """Copy path to device."""
    msg = 'Could not copy %s to device.' % src
    if mode is None:
      # Use rsync by default if it exists.
      mode = 'rsync' if self.HasRsync() else 'scp'

    if mode == 'scp':
      # scp always follow symlinks
      kwargs.pop('follow_symlinks', None)
      func = self.GetAgent().Scp
    else:
      func = self.GetAgent().Rsync

    return RunCommandFuncWrapper(func, msg, src, dest, **kwargs)

  def CopyFromDevice(self, src, dest, mode=None, **kwargs):
    """Copy path from device."""
    msg = 'Could not copy %s from device.' % src
    if mode is None:
      # Use rsync by default if it exists.
      mode = 'rsync' if self.HasRsync() else 'scp'

    if mode == 'scp':
      # scp always follow symlinks
      kwargs.pop('follow_symlinks', None)
      func = self.GetAgent().ScpToLocal
    else:
      func = self.GetAgent().RsyncToLocal

    return RunCommandFuncWrapper(func, msg, src, dest, **kwargs)

  def CopyFromWorkDir(self, src, dest, **kwargs):
    """Copy path from working directory on the device."""
    return self.CopyFromDevice(os.path.join(self.work_dir, src), dest, **kwargs)

  def CopyToWorkDir(self, src, dest='', **kwargs):
    """Copy path to working directory on the device."""
    return self.CopyToDevice(src, os.path.join(self.work_dir, dest), **kwargs)

  def IsDirWritable(self, path):
    """Checks if the given directory is writable on the device.

    Args:
      path: Directory on the device to check.
    """
    tmp_file = os.path.join(path, '.tmp.remote_access.is.writable')
    result = self.GetAgent().RemoteSh(
        ['touch', tmp_file, '&&', 'rm', tmp_file],
        error_code_ok=True, remote_sudo=True, capture_output=True)
    return result.returncode == 0

  def IsFileExecutable(self, path):
    """Check if the given file is executable on the device.

    Args:
      path: full path to the file on the device to check.

    Returns:
      True if the file is executable, and false if the file does not exist or is
      not executable.
    """
    cmd = ['test', '-f', path, '-a', '-x', path,]
    result = self.GetAgent().RemoteSh(cmd, remote_sudo=True, error_code_ok=True,
                                      capture_output=True)
    return result.returncode == 0

  def GetSize(self, path):
    """Gets the size of the given file on the device.

    Args:
      path: full path to the file on the device.

    Returns:
      Size of the file in number of bytes.

    Raises:
      ValueError if failed to get file size from the remote output.
      cros_build_lib.RunCommandError if |path| does not exist or the remote
      command to get file size has failed.
    """
    cmd = ['du', '-Lb', '--max-depth=0', path]
    result = self.BaseRunCommand(cmd, remote_sudo=True, capture_output=True)
    return int(result.output.split()[0])

  def CatFile(self, path, max_size=1000000):
    """Reads the file on device to string if its size is less than |max_size|.

    Args:
      path: The full path to the file on the device to read.
      max_size: Read the file only if its size is less than |max_size| in bytes.
        If None, do not check its size and always cat the path.

    Returns:
      A string of the file content.

    Raises:
      CatFileError if failed to read the remote file or the file size is larger
      than |max_size|.
    """
    if max_size is not None:
      try:
        file_size = self.GetSize(path)
      except (ValueError, cros_build_lib.RunCommandError) as e:
        raise CatFileError('Failed to get size of file "%s": %s' % (path, e))
      if file_size > max_size:
        raise CatFileError('File "%s" is larger than %d bytes' %
                           (path, max_size))

    result = self.BaseRunCommand(['cat', path], remote_sudo=True,
                                 error_code_ok=True, capture_output=True)
    if result.returncode:
      raise CatFileError('Failed to read file "%s" on the device' % path)
    return result.output

  def PipeOverSSH(self, filepath, cmd, **kwargs):
    """Cat a file and pipe over SSH."""
    producer_cmd = ['cat', filepath]
    return self.GetAgent().PipeToRemoteSh(producer_cmd, cmd, **kwargs)

  def GetRunningPids(self, exe, full_path=True):
    """Get all the running pids on the device with the executable path.

    Args:
      exe: The executable path to get pids for.
      full_path: Whether |exe| is a full executable path.

    Raises:
      RunningPidsError when failing to parse out pids from command output.
      SSHConnectionError when error occurs during SSH connection.
    """
    try:
      cmd = ['pgrep', exe]
      if full_path:
        cmd.append('-f')
      result = self.GetAgent().RemoteSh(cmd, error_code_ok=True,
                                        capture_output=True)
      try:
        return [int(pid) for pid in result.output.splitlines()]
      except ValueError:
        logging.error('Parsing output failed:\n%s', result.output)
        raise RunningPidsError('Unable to get running pids of %s' % exe)
    except SSHConnectionError:
      logging.error('Error connecting to device %s', self.hostname)
      raise

  def Reboot(self):
    """Reboot the device."""
    return self.GetAgent().RemoteReboot()

  def BaseRunCommand(self, cmd, **kwargs):
    """Executes a shell command on the device with output captured by default.

    Args:
      cmd: command to run. See RemoteAccess.RemoteSh documentation.
      **kwargs: keyword arguments to pass along with cmd. See
        RemoteAccess.RemoteSh documentation.
    """
    kwargs.setdefault('debug_level', self.debug_level)
    kwargs.setdefault('connect_settings', self.connect_settings)
    try:
      return self.GetAgent().RemoteSh(cmd, **kwargs)
    except SSHConnectionError:
      logging.error('Error connecting to device %s', self.hostname)
      raise

  def RunCommand(self, cmd, **kwargs):
    """Executes a shell command on the device with output captured by default.

    Also sets environment variables using dictionary provided by
    keyword argument |extra_env|.

    Args:
      cmd: command to run. See RemoteAccess.RemoteSh documentation.
      **kwargs: keyword arguments to pass along with cmd. See
        RemoteAccess.RemoteSh documentation.
    """
    # Handle setting environment variables on the device by copying
    # and sourcing a temporary environment file.
    extra_env = kwargs.pop('extra_env', None)
    if extra_env:
      remote_sudo = kwargs.pop('remote_sudo', False)
      if remote_sudo and self.GetAgent().username == ROOT_ACCOUNT:
        remote_sudo = False

      new_cmd = []
      flat_vars = ['%s=%s' % (k, cros_build_lib.ShellQuote(v))
                   for k, v in extra_env.iteritems()]

      # If the vars are too large for the command line, do it indirectly.
      # We pick 32k somewhat arbitrarily -- the kernel should accept this
      # and rarely should remote commands get near that size.
      ARG_MAX = 32 * 1024

      # What the command line would generally look like on the remote.
      cmdline = ' '.join(flat_vars + cmd)
      if len(cmdline) > ARG_MAX:
        env_list = ['export %s' % x for x in flat_vars]
        with tempfile.NamedTemporaryFile(dir=self.tempdir.tempdir,
                                         prefix='env') as f:
          logging.debug('Environment variables: %s', ' '.join(env_list))
          osutils.WriteFile(f.name, '\n'.join(env_list))
          self.CopyToWorkDir(f.name)
          env_file = os.path.join(self.work_dir, os.path.basename(f.name))
          new_cmd += ['.', '%s;' % env_file]
          if remote_sudo:
            new_cmd += ['sudo', '-E']
      else:
        if remote_sudo:
          new_cmd += ['sudo']
        new_cmd += flat_vars

      cmd = new_cmd + cmd

    return self.BaseRunCommand(cmd, **kwargs)


class ChromiumOSDevice(RemoteDevice):
  """Basic commands to interact with a ChromiumOS device over SSH connection."""

  MAKE_DEV_SSD_BIN = '/usr/share/vboot/bin/make_dev_ssd.sh'
  MOUNT_ROOTFS_RW_CMD = ['mount', '-o', 'remount,rw', '/']
  LIST_MOUNTS_CMD = ['cat', '/proc/mounts']

  def __init__(self, hostname, alias=None, connection_type=None, **kwargs):
    """Initializes this object.

    Args:
      hostname: A network hostname or a user-friendly USB device name (alias);
        None to find the default ChromiumOSDevice.
      alias: A user-friendly USB device name.
      connection_type: A CONNECTION_TYPE_xxx value, or None if unknown.
        Overwritten with the discovered value if |hostname| is None.
    """
    if hostname:
      self._alias = alias
      self.connection_type = connection_type
      # _ResolveHostname() may update |self.connection_type| and/or
      # |self._alias| so they need to be initialized beforehand.
      hostname = self._ResolveHostname(hostname)
    else:
      service, self.connection_type = _GetDefaultService()
      self._alias = service.text[BRILLO_DEVICE_PROPERTY_ALIAS]
      hostname = service.ip
      # We know this exists because it responded to the mDNS, no need to ping.
      kwargs['ping'] = False
    super(ChromiumOSDevice, self).__init__(hostname, **kwargs)
    self._orig_path = None
    self._path = None
    self._lsb_release = {}

  @property
  def orig_path(self):
    """The $PATH variable on the device."""
    if not self._orig_path:
      try:
        result = self.BaseRunCommand(['echo', "${PATH}"])
      except cros_build_lib.RunCommandError as e:
        logging.error('Failed to get $PATH on the device: %s', e.result.error)
        raise

      self._orig_path = result.output.strip()

    return self._orig_path

  @property
  def path(self):
    """The $PATH variable on the device prepended with DEV_BIN_PATHS."""
    if not self._path:
      # If the remote path already has our dev paths (which is common), then
      # there is no need for us to prepend.
      orig_paths = self.orig_path.split(':')
      for path in reversed(DEV_BIN_PATHS.split(':')):
        if path not in orig_paths:
          orig_paths.insert(0, path)

      self._path = ':'.join(orig_paths)

    return self._path

  @property
  def lsb_release(self):
    """The /etc/lsb-release content on the device.

    Returns a dict of entries in /etc/lsb-release file. If multiple entries
    have the same key, only the first entry is recorded. Returns an empty dict
    if the reading command failed or the file is corrupted (i.e., does not have
    the format of <key>=<value> for every line).
    """
    if not self._lsb_release:
      try:
        content = self.CatFile(constants.LSB_RELEASE_PATH, max_size=None)
      except CatFileError as e:
        logging.debug(
            'Failed to read "%s" on the device: %s',
            constants.LSB_RELEASE_PATH, e)
      else:
        try:
          self._lsb_release = dict(e.split('=', 1)
                                   for e in reversed(content.splitlines()))
        except ValueError:
          logging.error('File "%s" on the device is mal-formatted.',
                        constants.LSB_RELEASE_PATH)

    return self._lsb_release

  @property
  def board(self):
    """The board name of the device."""
    return self.lsb_release.get('CHROMEOS_RELEASE_BOARD', '')

  @property
  def sdk_version(self):
    """The SDK version of the device."""
    # TODO(garnold) Use the actual SDK version field, once known (brillo:280).
    return self.lsb_release.get('CHROMEOS_RELEASE_VERSION', '')

  @property
  def alias(self):
    """The user-friendly alias name assigned to the device."""
    if not self._alias:
      alias_file_path = os.path.join(BRILLO_DEVICE_PROPERTY_DIR,
                                     BRILLO_DEVICE_PROPERTY_ALIAS)
      try:
        self._alias = self.CatFile(alias_file_path,
                                   BRILLO_DEVICE_PROPERTY_MAX_LEN+1)
      except CatFileError as e:
        logging.debug('Unable to read alias of the device: %s', e)
      else:
        self._alias = self._alias.strip()

    return self._alias

  def SetAlias(self, alias_name):
    """Assign to the device a user-friendly alias name.

    Args:
      alias_name: The alias name to set. It must be no more than 128 in length
        containing only alphanumeric characters and/or underscores.

    Raises:
      InvalidDevicePropertyError if |alias_name| is invalid.
    """
    if len(alias_name) > BRILLO_DEVICE_PROPERTY_MAX_LEN:
      raise InvalidDevicePropertyError(
          'The alias name cannot be more than %d characters.' %
          BRILLO_DEVICE_PROPERTY_MAX_LEN)
    valid_alias_chars = string.ascii_letters + string.digits + '_'
    if not all(c in valid_alias_chars for c in alias_name):
      raise InvalidDevicePropertyError(
          'The alias name can only contain alphanumeric characters and/or '
          'underscores.')

    self.RunCommand(['mkdir', '-p', BRILLO_DEVICE_PROPERTY_DIR],
                    remote_sudo=True)
    alias_file_path = os.path.join(BRILLO_DEVICE_PROPERTY_DIR,
                                   BRILLO_DEVICE_PROPERTY_ALIAS)
    self.RunCommand(['echo', alias_name, '>', alias_file_path],
                    remote_sudo=True)
    self._alias = alias_name

    logging.info('Successfully set alias to "%s".', alias_name)

  def _ResolveHostname(self, hostname):
    """Resolve |hostname| into a network hostname.

    If |hostname| is an alias, |self._alias| is updated to be |hostname|.
    If the connection type can be determined during hostname resolution,
    |self.connection_type| is updated to the proper value.

    Args:
      hostname: Can either be a network hostname or user-friendly USB device
        name (aka alias).

    Returns:
      Network hostname as as string.
    """
    # If |hostname| is resolvable via DNS, then it's a valid hostname.
    # If |hostname| is resolvable via Debug Link mDNS, then it's an alias.
    try:
      socket.getaddrinfo(hostname, 0)
      return hostname
    except socket.gaierror:
      ip = GetUSBDeviceIP(hostname)
      if ip:
        self._alias = hostname
        self.connection_type = CONNECTION_TYPE_USB
        return ip
      # |hostname| is not resolvable but may still be valid (eg. ssh hostname).
      # Leave the hostname be.
      return hostname

  def _RemountRootfsAsWritable(self):
    """Attempts to Remount the root partition."""
    logging.info("Remounting '/' with rw...")
    self.RunCommand(self.MOUNT_ROOTFS_RW_CMD, error_code_ok=True,
                    remote_sudo=True)

  def _RootfsIsReadOnly(self):
    """Returns True if rootfs on is mounted as read-only."""
    r = self.RunCommand(self.LIST_MOUNTS_CMD, capture_output=True)
    for line in r.output.splitlines():
      if not line:
        continue

      chunks = line.split()
      if chunks[1] == '/' and 'ro' in chunks[3].split(','):
        return True

    return False

  def DisableRootfsVerification(self):
    """Disables device rootfs verification."""
    logging.info('Disabling rootfs verification on device...')
    self.RunCommand(
        [self.MAKE_DEV_SSD_BIN, '--remove_rootfs_verification', '--force'],
        error_code_ok=True, remote_sudo=True)
    # TODO(yjhong): Make sure an update is not pending.
    logging.info('Need to reboot to actually disable the verification.')
    self.Reboot()

  def MountRootfsReadWrite(self):
    """Checks mount types and remounts them as read-write if needed.

    Returns:
      True if rootfs is mounted as read-write. False otherwise.
    """
    if not self._RootfsIsReadOnly():
      return True

    # If the image on the device is built with rootfs verification
    # disabled, we can simply remount '/' as read-write.
    self._RemountRootfsAsWritable()

    if not self._RootfsIsReadOnly():
      return True

    logging.info('Unable to remount rootfs as rw (normal w/verified rootfs).')
    # If the image is built with rootfs verification, turn off the
    # rootfs verification. After reboot, the rootfs will be mounted as
    # read-write (there is no need to remount).
    self.DisableRootfsVerification()

    return not self._RootfsIsReadOnly()

  def RunCommand(self, cmd, **kwargs):
    """Executes a shell command on the device with output captured by default.

    Also makes sure $PATH is set correctly by adding DEV_BIN_PATHS to
    'PATH' in |extra_env|.

    Args:
      cmd: command to run. See RemoteAccess.RemoteSh documentation.
      **kwargs: keyword arguments to pass along with cmd. See
        RemoteAccess.RemoteSh documentation.
    """
    extra_env = kwargs.pop('extra_env', {})
    path_env = extra_env.get('PATH', None)
    if path_env is None:
      # Optimization: if the default path is already what we want, don't bother
      # passing it through.
      if self.orig_path != self.path:
        path_env = self.path
    if path_env is not None:
      extra_env['PATH'] = path_env
    kwargs['extra_env'] = extra_env
    return super(ChromiumOSDevice, self).RunCommand(cmd, **kwargs)


def _DiscoverUSBServices():
  """Performs service discovery over the USB link.

  Initializes the USB link and sends the mDNS query to find all
  available Brillo services.

  GetUSBConnectedDevices() can be used instead to get a list of full
  ChromiumOSDevice objects.

  Returns:
    A list of mdns.Service objects.
  """
  # Lazy import mdns so that we don't break the chromite requirement that
  # bootstrapping should not depend on third_party packages. mdns pulls in
  # dpkt which is a third_party package.
  from chromite.lib import mdns
  try:
    source_ip = debug_link.InitializeDebugLink()
    return mdns.FindServices(source_ip, BRILLO_DEBUG_LINK_SERVICE_NAME)
  except debug_link.DebugLinkException as e:
    logging.debug('Failed to initialize debug link: %s', e)
    return []


def _GetDefaultService():
  """Returns the default service if one exists.

  If there is exactly one device connected over USB it will be
  returned. Otherwise DefaultDeviceError will be raised.

  Returns:
    A (mdns.Service, CONNECTION_TYPE_xxx value) tuple.

  Raises:
    DefaultDeviceError: no default device was found.
  """
  services = _DiscoverUSBServices()
  if not services:
    raise DefaultDeviceError('No default device could be found.')
  elif len(services) > 1:
    raise DefaultDeviceError(
        'More than one device was found, please specify a device from: %s.' %
        ', '.join(service.text[BRILLO_DEVICE_PROPERTY_ALIAS]
                  for service in services))
  return (services[0], CONNECTION_TYPE_USB)


def GetUSBConnectedDevices():
  """Returns a list of all USB-connected devices."""
  # Use connect=False so that we don't try to set up the device connections
  # until the device is used.
  return [ChromiumOSDevice(service.ip, connection_type=CONNECTION_TYPE_USB,
                           alias=service.text[BRILLO_DEVICE_PROPERTY_ALIAS],
                           ping=False, connect=False)
          for service in _DiscoverUSBServices()]


def GetUSBDeviceIP(alias):
  """Gets the USB-connected device IP address using its |alias|.

  Args:
    alias: User-friendly name of USB-connected device.

  Returns:
    USB-connected device IP address or None if |alias| is not found.  If there
    are duplicate aliases on the network, the first IP address is returned.
  """
  if not alias:
    return None

  # Lazy import mdns so that we don't break the chromite requirement that
  # bootstrapping should not depend on third_party packages. mdns pulls in
  # dpkt which is a third_party package.
  from chromite.lib import mdns

  # For now, swallow missing debug link error until we have a better way of
  # differentiating between ChromeOS and Brillo.
  try:
    source_ip = debug_link.InitializeDebugLink()
  except debug_link.DebugLinkMissingError:
    return None

  should_add = lambda x: x.text.get(BRILLO_DEVICE_PROPERTY_ALIAS) == alias
  should_continue = lambda x: x.text.get(BRILLO_DEVICE_PROPERTY_ALIAS) != alias
  services = mdns.FindServices(source_ip, BRILLO_DEBUG_LINK_SERVICE_NAME,
                               should_add_func=should_add,
                               should_continue_func=should_continue)
  if not services:
    return None
  return services[0].ip

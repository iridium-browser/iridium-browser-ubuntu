# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# pylint: disable=W0212

import fcntl
import logging
import os
import psutil

from devil import base_error
from devil import devil_env
from devil.android import device_errors
from devil.android.constants import file_system
from devil.android.valgrind_tools import base_tool
from devil.utils import cmd_helper


def _GetProcessStartTime(pid):
  return psutil.Process(pid).create_time


class _FileLock(object):
  """With statement-aware implementation of a file lock.

  File locks are needed for cross-process synchronization when the
  multiprocessing Python module is used.
  """

  def __init__(self, path):
    self._fd = -1
    self._path = path

  def __enter__(self):
    self._fd = os.open(self._path, os.O_RDONLY | os.O_CREAT)
    if self._fd < 0:
      raise Exception('Could not open file %s for reading' % self._path)
    fcntl.flock(self._fd, fcntl.LOCK_EX)

  def __exit__(self, _exception_type, _exception_value, traceback):
    fcntl.flock(self._fd, fcntl.LOCK_UN)
    os.close(self._fd)


class HostForwarderError(base_error.BaseError):
  """Exception for failures involving host_forwarder."""

  def __init__(self, message):
    super(HostForwarderError, self).__init__(message)


class Forwarder(object):
  """Thread-safe class to manage port forwards from the device to the host."""

  _DEVICE_FORWARDER_FOLDER = (file_system.TEST_EXECUTABLE_DIR +
                              '/forwarder/')
  _DEVICE_FORWARDER_PATH = (file_system.TEST_EXECUTABLE_DIR +
                            '/forwarder/device_forwarder')
  _LOCK_PATH = '/tmp/chrome.forwarder.lock'
  # Defined in host_forwarder_main.cc
  _HOST_FORWARDER_LOG = '/tmp/host_forwarder_log'

  _instance = None

  @staticmethod
  def Map(port_pairs, device, tool=None):
    """Runs the forwarder.

    Args:
      port_pairs: A list of tuples (device_port, host_port) to forward. Note
                 that you can specify 0 as a device_port, in which case a
                 port will by dynamically assigned on the device. You can
                 get the number of the assigned port using the
                 DevicePortForHostPort method.
      device: A DeviceUtils instance.
      tool: Tool class to use to get wrapper, if necessary, for executing the
            forwarder (see valgrind_tools.py).

    Raises:
      Exception on failure to forward the port.
    """
    if not tool:
      tool = base_tool.BaseTool()
    with _FileLock(Forwarder._LOCK_PATH):
      instance = Forwarder._GetInstanceLocked(tool)
      instance._InitDeviceLocked(device, tool)

      device_serial = str(device)
      redirection_commands = [
          ['--adb=' + devil_env.config.FetchPath('adb'),
           '--serial-id=' + device_serial,
           '--map', str(device_port), str(host_port)]
          for device_port, host_port in port_pairs]
      logging.info('Forwarding using commands: %s', redirection_commands)

      for redirection_command in redirection_commands:
        try:
          (exit_code, output) = cmd_helper.GetCmdStatusAndOutput(
              [instance._host_forwarder_path] + redirection_command)
        except OSError as e:
          if e.errno == 2:
            raise HostForwarderError(
                'Unable to start host forwarder. '
                'Make sure you have built host_forwarder.')
          else: raise
        if exit_code != 0:
          Forwarder._KillDeviceLocked(device, tool)
          # Log alive forwarders
          ps_out = device.RunShellCommand(['ps'])
          logging.info('Currently running device_forwarders:')
          for line in ps_out:
            if 'device_forwarder' in line:
              logging.info('    %s', line)
          raise HostForwarderError(
              '%s exited with %d:\n%s' % (instance._host_forwarder_path,
                                          exit_code, '\n'.join(output)))
        tokens = output.split(':')
        if len(tokens) != 2:
          raise HostForwarderError(
              'Unexpected host forwarder output "%s", '
              'expected "device_port:host_port"' % output)
        device_port = int(tokens[0])
        host_port = int(tokens[1])
        serial_with_port = (device_serial, device_port)
        instance._device_to_host_port_map[serial_with_port] = host_port
        instance._host_to_device_port_map[host_port] = serial_with_port
        logging.info('Forwarding device port: %d to host port: %d.',
                     device_port, host_port)

  @staticmethod
  def UnmapDevicePort(device_port, device):
    """Unmaps a previously forwarded device port.

    Args:
      device: A DeviceUtils instance.
      device_port: A previously forwarded port (through Map()).
    """
    with _FileLock(Forwarder._LOCK_PATH):
      Forwarder._UnmapDevicePortLocked(device_port, device)

  @staticmethod
  def UnmapAllDevicePorts(device):
    """Unmaps all the previously forwarded ports for the provided device.

    Args:
      device: A DeviceUtils instance.
      port_pairs: A list of tuples (device_port, host_port) to unmap.
    """
    with _FileLock(Forwarder._LOCK_PATH):
      if not Forwarder._instance:
        return
      adb_serial = str(device)
      if adb_serial not in Forwarder._instance._initialized_devices:
        return
      port_map = Forwarder._GetInstanceLocked(
          None)._device_to_host_port_map
      for (device_serial, device_port) in port_map.keys():
        if adb_serial == device_serial:
          Forwarder._UnmapDevicePortLocked(device_port, device)
      # There are no more ports mapped, kill the device_forwarder.
      tool = base_tool.BaseTool()
      Forwarder._KillDeviceLocked(device, tool)

  @staticmethod
  def DevicePortForHostPort(host_port):
    """Returns the device port that corresponds to a given host port."""
    with _FileLock(Forwarder._LOCK_PATH):
      _, device_port = Forwarder._GetInstanceLocked(
          None)._host_to_device_port_map.get(host_port)
      return device_port

  @staticmethod
  def RemoveHostLog():
    if os.path.exists(Forwarder._HOST_FORWARDER_LOG):
      os.unlink(Forwarder._HOST_FORWARDER_LOG)

  @staticmethod
  def GetHostLog():
    if not os.path.exists(Forwarder._HOST_FORWARDER_LOG):
      return ''
    with file(Forwarder._HOST_FORWARDER_LOG, 'r') as f:
      return f.read()

  @staticmethod
  def _GetInstanceLocked(tool):
    """Returns the singleton instance.

    Note that the global lock must be acquired before calling this method.

    Args:
      tool: Tool class to use to get wrapper, if necessary, for executing the
            forwarder (see valgrind_tools.py).
    """
    if not Forwarder._instance:
      Forwarder._instance = Forwarder(tool)
    return Forwarder._instance

  def __init__(self, tool):
    """Constructs a new instance of Forwarder.

    Note that Forwarder is a singleton therefore this constructor should be
    called only once.

    Args:
      tool: Tool class to use to get wrapper, if necessary, for executing the
            forwarder (see valgrind_tools.py).
    """
    assert not Forwarder._instance
    self._tool = tool
    self._initialized_devices = set()
    self._device_to_host_port_map = dict()
    self._host_to_device_port_map = dict()
    self._host_forwarder_path = devil_env.config.FetchPath('forwarder_host')
    assert os.path.exists(self._host_forwarder_path), 'Please build forwarder2'
    self._InitHostLocked()

  @staticmethod
  def _UnmapDevicePortLocked(device_port, device):
    """Internal method used by UnmapDevicePort().

    Note that the global lock must be acquired before calling this method.
    """
    instance = Forwarder._GetInstanceLocked(None)
    serial = str(device)
    serial_with_port = (serial, device_port)
    if not serial_with_port in instance._device_to_host_port_map:
      logging.error('Trying to unmap non-forwarded port %d', device_port)
      return
    redirection_command = ['--adb=' + devil_env.config.FetchPath('adb'),
                           '--serial-id=' + serial,
                           '--unmap', str(device_port)]
    logging.info('Undo forwarding using command: %s', redirection_command)
    (exit_code, output) = cmd_helper.GetCmdStatusAndOutput(
        [instance._host_forwarder_path] + redirection_command)
    if exit_code != 0:
      logging.error(
          '%s exited with %d:\n%s',
          instance._host_forwarder_path, exit_code, '\n'.join(output))
    host_port = instance._device_to_host_port_map[serial_with_port]
    del instance._device_to_host_port_map[serial_with_port]
    del instance._host_to_device_port_map[host_port]

  @staticmethod
  def _GetPidForLock():
    """Returns the PID used for host_forwarder initialization.

    The PID of the "sharder" is used to handle multiprocessing. The "sharder"
    is the initial process that forks that is the parent process.
    """
    return os.getpgrp()

  def _InitHostLocked(self):
    """Initializes the host forwarder daemon.

    Note that the global lock must be acquired before calling this method. This
    method kills any existing host_forwarder process that could be stale.
    """
    # See if the host_forwarder daemon was already initialized by a concurrent
    # process or thread (in case multi-process sharding is not used).
    pid_for_lock = Forwarder._GetPidForLock()
    fd = os.open(Forwarder._LOCK_PATH, os.O_RDWR | os.O_CREAT)
    with os.fdopen(fd, 'r+') as pid_file:
      pid_with_start_time = pid_file.readline()
      if pid_with_start_time:
        (pid, process_start_time) = pid_with_start_time.split(':')
        if pid == str(pid_for_lock):
          if process_start_time == str(_GetProcessStartTime(pid_for_lock)):
            return
      self._KillHostLocked()
      pid_file.seek(0)
      pid_file.write(
          '%s:%s' % (pid_for_lock, str(_GetProcessStartTime(pid_for_lock))))
      pid_file.truncate()

  def _InitDeviceLocked(self, device, tool):
    """Initializes the device_forwarder daemon for a specific device (once).

    Note that the global lock must be acquired before calling this method. This
    method kills any existing device_forwarder daemon on the device that could
    be stale, pushes the latest version of the daemon (to the device) and starts
    it.

    Args:
      device: A DeviceUtils instance.
      tool: Tool class to use to get wrapper, if necessary, for executing the
            forwarder (see valgrind_tools.py).
    """
    device_serial = str(device)
    if device_serial in self._initialized_devices:
      return
    try:
      Forwarder._KillDeviceLocked(device, tool)
    except device_errors.CommandFailedError:
      logging.warning('Failed to kill device forwarder. Rebooting.')
      device.Reboot()
    forwarder_device_path_on_host = devil_env.config.FetchPath(
        'forwarder_device', device=device)
    forwarder_device_path_on_device = (
        Forwarder._DEVICE_FORWARDER_FOLDER
        if os.path.isdir(forwarder_device_path_on_host)
        else Forwarder._DEVICE_FORWARDER_PATH)
    device.PushChangedFiles([(
        forwarder_device_path_on_host,
        forwarder_device_path_on_device)])

    cmd = '%s %s' % (tool.GetUtilWrapper(), Forwarder._DEVICE_FORWARDER_PATH)
    device.RunShellCommand(
        cmd, env={'LD_LIBRARY_PATH': Forwarder._DEVICE_FORWARDER_FOLDER},
        check_return=True)
    self._initialized_devices.add(device_serial)

  def _KillHostLocked(self):
    """Kills the forwarder process running on the host.

    Note that the global lock must be acquired before calling this method.
    """
    logging.info('Killing host_forwarder.')
    (exit_code, output) = cmd_helper.GetCmdStatusAndOutput(
        [self._host_forwarder_path, '--kill-server'])
    if exit_code != 0:
      (exit_code, output) = cmd_helper.GetCmdStatusAndOutput(
          ['pkill', '-9', 'host_forwarder'])
      if exit_code != 0:
        raise HostForwarderError(
            '%s exited with %d:\n%s' % (self._host_forwarder_path, exit_code,
                                        '\n'.join(output)))

  @staticmethod
  def _KillDeviceLocked(device, tool):
    """Kills the forwarder process running on the device.

    Note that the global lock must be acquired before calling this method.

    Args:
      device: Instance of DeviceUtils for talking to the device.
      tool: Wrapper tool (e.g. valgrind) that can be used to execute the device
            forwarder (see valgrind_tools.py).
    """
    logging.info('Killing device_forwarder.')
    Forwarder._instance._initialized_devices.discard(str(device))
    if not device.FileExists(Forwarder._DEVICE_FORWARDER_PATH):
      return

    cmd = '%s %s --kill-server' % (tool.GetUtilWrapper(),
                                   Forwarder._DEVICE_FORWARDER_PATH)
    device.RunShellCommand(
        cmd, env={'LD_LIBRARY_PATH': Forwarder._DEVICE_FORWARDER_FOLDER},
        check_return=True)

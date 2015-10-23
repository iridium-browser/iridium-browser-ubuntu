#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to keep track of devices across builds and report state."""
import argparse
import json
import logging
import os
import psutil
import re
import signal
import smtplib
import subprocess
import sys
import time
import urllib

import bb_annotations
import bb_utils

sys.path.append(os.path.join(os.path.dirname(__file__),
                             os.pardir, os.pardir, 'util', 'lib',
                             'common'))
import perf_tests_results_helper  # pylint: disable=F0401

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from devil.utils import reset_usb
from pylib import constants
from pylib.cmd_helper import GetCmdOutput
from pylib.device import adb_wrapper
from pylib.device import battery_utils
from pylib.device import device_blacklist
from pylib.device import device_errors
from pylib.device import device_list
from pylib.device import device_utils
from pylib.utils import run_tests_helper
from pylib.utils import timeout_retry

_RE_DEVICE_ID = re.compile('Device ID = (\d+)')

def DeviceInfo(device, args):
  """Gathers info on a device via various adb calls.

  Args:
    device: A DeviceUtils instance for the device to construct info about.

  Returns:
    Tuple of device type, build id, report as a string, error messages, and
    boolean indicating whether or not device can be used for testing.
  """
  battery = battery_utils.BatteryUtils(device)

  build_product = ''
  build_id = ''
  battery_level = 100
  errors = []
  dev_good = True
  json_data = {}

  try:
    build_product = device.build_product
    build_id = device.build_id

    json_data = {
      'serial': device.adb.GetDeviceSerial(),
      'type': build_product,
      'build': build_id,
      'build_detail': device.GetProp('ro.build.fingerprint'),
      'battery': {},
      'imei_slice': 'Unknown',
      'wifi_ip': device.GetProp('dhcp.wlan0.ipaddress'),
    }

    battery_info = {}
    try:
      battery_info = battery.GetBatteryInfo(timeout=5)
      battery_level = int(battery_info.get('level', battery_level))
      json_data['battery'] = battery_info
    except device_errors.CommandFailedError:
      logging.exception('Failed to get battery information for %s', str(device))

    try:
      for l in device.RunShellCommand(['dumpsys', 'iphonesubinfo'],
                                      check_return=True, timeout=5):
        m = _RE_DEVICE_ID.match(l)
        if m:
          json_data['imei_slice'] = m.group(1)[-6:]
    except device_errors.CommandFailedError:
      logging.exception('Failed to get IMEI slice for %s', str(device))

    if battery_level < 15:
      errors += ['Device critically low in battery.']
      dev_good = False
      if not battery.GetCharging():
        battery.SetCharging(True)
    if not args.no_provisioning_check:
      setup_wizard_disabled = (
          device.GetProp('ro.setupwizard.mode') == 'DISABLED')
      if not setup_wizard_disabled and device.build_type != 'user':
        errors += ['Setup wizard not disabled. Was it provisioned correctly?']
    if (device.product_name == 'mantaray' and
        battery_info.get('AC powered', None) != 'true'):
      errors += ['Mantaray device not connected to AC power.']
  except device_errors.CommandFailedError:
    logging.exception('Failure while getting device status.')
    dev_good = False
  except device_errors.CommandTimeoutError:
    logging.exception('Timeout while getting device status.')
    dev_good = False

  return (build_product, build_id, battery_level, errors, dev_good, json_data)


def CheckForMissingDevices(args, devices):
  """Uses file of previous online devices to detect broken phones.

  Args:
    args: out_dir parameter of args argument is used as the base
      directory to load and update the cache file.
    devices: A list of DeviceUtils instance for the currently visible and
      online attached devices.
  """
  out_dir = os.path.abspath(args.out_dir)
  device_serials = set(d.adb.GetDeviceSerial() for d in devices)

  # last_devices denotes all known devices prior to this run
  last_devices_path = os.path.join(out_dir, device_list.LAST_DEVICES_FILENAME)
  last_missing_devices_path = os.path.join(out_dir,
      device_list.LAST_MISSING_DEVICES_FILENAME)
  try:
    last_devices = device_list.GetPersistentDeviceList(last_devices_path)
  except IOError:
    # Ignore error, file might not exist
    last_devices = []

  try:
    last_missing_devices = device_list.GetPersistentDeviceList(
        last_missing_devices_path)
  except IOError:
    last_missing_devices = []

  missing_devs = list(set(last_devices) - device_serials)
  new_missing_devs = list(set(missing_devs) - set(last_missing_devices))

  buildbot_slavename = os.environ.get('BUILDBOT_SLAVENAME')
  buildbot_buildername = os.environ.get('BUILDBOT_BUILDERNAME')
  buildbot_buildnumber = os.environ.get('BUILDBOT_BUILDNUMBER')

  if new_missing_devs and buildbot_slavename:
    logging.info('new_missing_devs %s' % new_missing_devs)
    devices_missing_msg = '%d devices not detected.' % len(missing_devs)
    bb_annotations.PrintSummaryText(devices_missing_msg)

    from_address = 'chrome-bot@chromium.org'
    to_addresses = ['chrome-labs-tech-ticket@google.com',
                    'chrome-android-device-alert@google.com']
    cc_addresses = ['chrome-android-device-alert@google.com']
    subject = 'Devices offline on %s, %s, %s' % (
        buildbot_slavename, buildbot_buildername, buildbot_buildnumber)
    msg = ('Please reboot the following devices:\n%s' %
           '\n'.join(map(str, new_missing_devs)))
    SendEmail(from_address, to_addresses, cc_addresses, subject, msg)

  unauthorized_devices = adb_wrapper.AdbWrapper.Devices(
      desired_state='unauthorized')
  if unauthorized_devices:
    logging.info('unauthorized devices:')
    for ud in unauthorized_devices:
      logging.info('  %s', ud)

    if buildbot_slavename:
      from_address = 'chrome-bot@chromium.org'
      to_addresses = [
          'jbudorick@chromium.org',
          'chrome-android-device-alert@google.com']
      subject = 'Unauthorized devices on %s' % buildbot_slavename
      msg = ['The following devices are offline on %s' % buildbot_slavename]
      msg += ['  %s' % ud for ud in unauthorized_devices]
      msg += ['', 'Detected on %s #%s' % (buildbot_buildername,
                                          buildbot_buildnumber)]
      msg = '\n'.join(msg)
      SendEmail(from_address, to_addresses, [], subject, msg)

  all_known_devices = list(device_serials | set(last_devices))
  device_list.WritePersistentDeviceList(last_devices_path, all_known_devices)
  device_list.WritePersistentDeviceList(last_missing_devices_path, missing_devs)

  if not all_known_devices:
    # This can happen if for some reason the .last_devices file is not
    # present or if it was empty.
    return ['No online devices. Have any devices been plugged in?']
  if missing_devs:
    devices_missing_msg = '%d devices not detected.' % len(missing_devs)
    bb_annotations.PrintSummaryText(devices_missing_msg)
    return ['Current online devices: %s' % ', '.join(d for d in device_serials),
            '%s are no longer visible. Were they removed?' % missing_devs]
  else:
    new_devs = device_serials - set(last_devices)
    if new_devs and os.path.exists(last_devices_path):
      bb_annotations.PrintWarning()
      bb_annotations.PrintSummaryText(
          '%d new devices detected' % len(new_devs))
      logging.info('New devices detected:')
      for d in new_devs:
        logging.info('  %s', d)


def SendEmail(from_address, to_addresses, cc_addresses, subject, msg):
  # TODO(jbudorick): Transition from email alerts for every failure to a more
  # sustainable solution.
  msg_body = '\r\n'.join(['From: %s' % from_address,
                          'To: %s' % ', '.join(to_addresses),
                          'CC: %s' % ', '.join(cc_addresses),
                          'Subject: %s' % subject, '', msg])
  try:
    server = smtplib.SMTP('localhost')
    server.sendmail(from_address, to_addresses, msg_body)
    server.quit()
  except Exception:
    logging.exception('Failed to send alert email.')


def KillAllAdb():
  def GetAllAdb():
    for p in psutil.process_iter():
      try:
        if 'adb' in p.name:
          yield p
      except (psutil.NoSuchProcess, psutil.AccessDenied):
        pass

  for sig in [signal.SIGTERM, signal.SIGQUIT, signal.SIGKILL]:
    for p in GetAllAdb():
      try:
        logging.info('kill %d %d (%s [%s])', sig, p.pid, p.name,
                     ' '.join(p.cmdline))
        p.send_signal(sig)
      except (psutil.NoSuchProcess, psutil.AccessDenied):
        pass
  for p in GetAllAdb():
    try:
      logging.error('Unable to kill %d (%s [%s])', p.pid, p.name,
                    ' '.join(p.cmdline))
    except (psutil.NoSuchProcess, psutil.AccessDenied):
      pass


def RecoverDevices(blacklist, output_directory):
  # Remove the last build's "bad devices" before checking device statuses.
  blacklist.Reset()

  previous_devices = set(a.GetDeviceSerial()
                         for a in adb_wrapper.AdbWrapper.Devices())

  KillAllAdb()
  reset_usb.reset_all_android_devices()

  try:
    expected_devices = set(device_list.GetPersistentDeviceList(
        os.path.join(output_directory, device_list.LAST_DEVICES_FILENAME)))
  except IOError:
    expected_devices = set()

  all_devices = [device_utils.DeviceUtils(d)
                 for d in previous_devices.union(expected_devices)]

  def blacklisting_recovery(device):
    try:
      device.WaitUntilFullyBooted()
    except device_errors.CommandFailedError:
      logging.exception('Failure while waiting for %s. Adding to blacklist.',
                        str(device))
      blacklist.Extend([str(device)])
    except device_errors.CommandTimeoutError:
      logging.exception('Timed out while waiting for %s. Adding to blacklist.',
                        str(device))
      blacklist.Extend([str(device)])

  device_utils.DeviceUtils.parallel(all_devices).pMap(blacklisting_recovery)

  devices = device_utils.DeviceUtils.HealthyDevices(blacklist)
  device_serials = set(d.adb.GetDeviceSerial() for d in devices)

  missing_devices = expected_devices.difference(device_serials)
  new_devices = device_serials.difference(expected_devices)

  if missing_devices or new_devices:
    logging.warning('expected_devices:')
    for d in sorted(expected_devices):
      logging.warning('  %s', d)
    logging.warning('devices:')
    for d in sorted(device_serials):
      logging.warning('  %s', d)

  return devices


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--out-dir',
                      help='Directory where the device path is stored',
                      default=os.path.join(constants.DIR_SOURCE_ROOT, 'out'))
  parser.add_argument('--no-provisioning-check', action='store_true',
                      help='Will not check if devices are provisioned '
                           'properly.')
  parser.add_argument('--device-status-dashboard', action='store_true',
                      help='Output device status data for dashboard.')
  parser.add_argument('--restart-usb', action='store_true',
                      help='DEPRECATED. '
                           'This script now always tries to reset USB.')
  parser.add_argument('--json-output',
                      help='Output JSON information into a specified file.')
  parser.add_argument('--blacklist-file', help='Device blacklist JSON file.')
  parser.add_argument('-v', '--verbose', action='count', default=1,
                      help='Log more information.')

  args = parser.parse_args()

  run_tests_helper.SetLogLevel(args.verbose)

  if args.blacklist_file:
    blacklist = device_blacklist.Blacklist(args.blacklist_file)
  else:
    # TODO(jbudorick): Remove this once bots pass the blacklist file.
    blacklist = device_blacklist.Blacklist(device_blacklist.BLACKLIST_JSON)

  devices = RecoverDevices(blacklist, args.out_dir)

  types, builds, batteries, errors, devices_ok, json_data = (
      [], [], [], [], [], [])
  if devices:
    types, builds, batteries, errors, devices_ok, json_data = (
        zip(*[DeviceInfo(dev, args) for dev in devices]))

  # Write device info to file for buildbot info display.
  if os.path.exists('/home/chrome-bot'):
    with open('/home/chrome-bot/.adb_device_info', 'w') as f:
      for device in json_data:
        try:
          f.write('%s %s %s %.1fC %s%%\n' % (device['serial'], device['type'],
              device['build'], float(device['battery']['temperature']) / 10,
              device['battery']['level']))
        except Exception:
          pass

  err_msg = CheckForMissingDevices(args, devices) or []

  unique_types = list(set(types))
  unique_builds = list(set(builds))

  bb_annotations.PrintMsg('Online devices: %d. Device types %s, builds %s'
                           % (len(devices), unique_types, unique_builds))

  for j in json_data:
    logging.info('Device %s (%s)', j.get('serial'), j.get('type'))
    logging.info('  Build: %s (%s)', j.get('build'), j.get('build_detail'))
    logging.info('  Current Battery Service state:')
    for k, v in j.get('battery', {}).iteritems():
      logging.info('    %s: %s', k, v)
    logging.info('  IMEI slice: %s', j.get('imei_slice'))
    logging.info('  WiFi IP: %s', j.get('wifi_ip'))


  for dev, dev_errors in zip(devices, errors):
    if dev_errors:
      err_msg += ['%s errors:' % str(dev)]
      err_msg += ['    %s' % error for error in dev_errors]

  if err_msg:
    bb_annotations.PrintWarning()
    for e in err_msg:
      logging.error(e)
    from_address = 'buildbot@chromium.org'
    to_addresses = ['chromium-android-device-alerts@google.com']
    bot_name = os.environ.get('BUILDBOT_BUILDERNAME')
    slave_name = os.environ.get('BUILDBOT_SLAVENAME')
    subject = 'Device status check errors on %s, %s.' % (slave_name, bot_name)
    SendEmail(from_address, to_addresses, [], subject, '\n'.join(err_msg))

  if args.device_status_dashboard:
    offline_devices = [
        device_utils.DeviceUtils(a)
        for a in adb_wrapper.AdbWrapper.Devices(desired_state=None)
        if a.GetState() == 'offline']

    perf_tests_results_helper.PrintPerfResult('BotDevices', 'OnlineDevices',
                                              [len(devices)], 'devices')
    perf_tests_results_helper.PrintPerfResult('BotDevices', 'OfflineDevices',
                                              [len(offline_devices)], 'devices',
                                              'unimportant')
    for dev, battery in zip(devices, batteries):
      perf_tests_results_helper.PrintPerfResult('DeviceBattery', str(dev),
                                                [battery], '%',
                                                'unimportant')

  if args.json_output:
    with open(args.json_output, 'wb') as f:
      f.write(json.dumps(json_data, indent=4))

  num_failed_devs = 0
  for device_ok, device in zip(devices_ok, devices):
    if not device_ok:
      logging.warning('Blacklisting %s', str(device))
      blacklist.Extend([str(device)])
      num_failed_devs += 1

  if num_failed_devs == len(devices):
    return 2

  if not devices:
    return 1


if __name__ == '__main__':
  sys.exit(main())

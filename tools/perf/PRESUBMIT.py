# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting tools/perf/.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
import re
import sys


def _CommonChecks(input_api, output_api):
  """Performs common checks, which includes running pylint."""
  results = []
  old_sys_path = sys.path
  try:
    # Modules in tools/perf depend on telemetry.
    sys.path = [os.path.join(os.pardir, 'telemetry')] + sys.path
    results.extend(input_api.canned_checks.RunPylint(
        input_api, output_api, black_list=[], pylintrc='pylintrc',
        extra_paths_list=_GetPathsToPrepend(input_api)))
    results.extend(_CheckJson(input_api, output_api))
    results.extend(_CheckWprShaFiles(input_api, output_api))
  finally:
    sys.path = old_sys_path
  return results


def _GetPathsToPrepend(input_api):
  perf_dir = input_api.PresubmitLocalPath()
  chromium_src_dir = input_api.os_path.join(perf_dir, '..', '..')
  telemetry_dir = input_api.os_path.join(chromium_src_dir, 'tools', 'telemetry')
  return [
      telemetry_dir,
      input_api.os_path.join(telemetry_dir, 'third_party', 'mock'),
  ]


def _CheckWprShaFiles(input_api, output_api):
  """Check whether the wpr sha files have matching URLs."""
  from catapult_base import cloud_storage
  results = []
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    filename = affected_file.AbsoluteLocalPath()
    if not filename.endswith('wpr.sha1'):
      continue
    expected_hash = cloud_storage.ReadHash(filename)
    is_wpr_file_uploaded = any(
        cloud_storage.Exists(bucket, expected_hash)
        for bucket in cloud_storage.BUCKET_ALIASES.itervalues())
    if not is_wpr_file_uploaded:
      wpr_filename = filename[:-5]
      results.append(output_api.PresubmitError(
          'The file matching %s is not in Cloud Storage yet.\n'
          'You can upload your new WPR archive file with the command:\n'
          'depot_tools/upload_to_google_storage.py --bucket '
          '<Your pageset\'s bucket> %s.\nFor more info: see '
          'http://www.chromium.org/developers/telemetry/'
          'record_a_page_set#TOC-Upload-the-recording-to-Cloud-Storage' %
          (filename, wpr_filename)))
  return results


def _CheckJson(input_api, output_api):
  """Checks whether JSON files in this change can be parsed."""
  for affected_file in input_api.AffectedFiles(include_deletes=False):
    filename = affected_file.AbsoluteLocalPath()
    if os.path.splitext(filename)[1] != '.json':
      continue
    try:
      input_api.json.load(open(filename))
    except ValueError:
      return [output_api.PresubmitError('Error parsing JSON in %s!' % filename)]
  return []


def CheckChangeOnUpload(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report


def CheckChangeOnCommit(input_api, output_api):
  report = []
  report.extend(_CommonChecks(input_api, output_api))
  return report


def _IsBenchmarksModified(change):
  """Checks whether CL contains any modification to Telemetry benchmarks."""
  for affected_file in change.AffectedFiles():
    affected_file_path = affected_file.LocalPath()
    file_path, _ = os.path.splitext(affected_file_path)
    if (os.path.join('tools', 'perf', 'benchmarks') in file_path or
        os.path.join('tools', 'perf', 'measurements') in file_path):
      return True
  return False


def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook adds extra try bots list to the CL description in order to run
  Telemetry benchmarks on Perf trybots in addtion to CQ trybots if the CL
  contains any changes to Telemetry benchmarks.
  """
  benchmarks_modified = _IsBenchmarksModified(change)
  rietveld_obj = cl.RpcServer()
  issue = cl.issue
  original_description = rietveld_obj.get_description(issue)
  if not benchmarks_modified or re.search(
     r'^CQ_EXTRA_TRYBOTS=.*', original_description, re.M | re.I):
    return []

  results = []
  bots = [
    'linux_perf_bisect',
    'mac_perf_bisect',
    'win_perf_bisect',
    'android_nexus5_perf_bisect'
  ]
  bots = ['tryserver.chromium.perf:%s' % s for s in bots]
  bots_string = ';'.join(bots)
  description = original_description
  description += '\nCQ_EXTRA_TRYBOTS=%s' % bots_string
  results.append(output_api.PresubmitNotifyResult(
      'Automatically added Perf trybots to run Telemetry benchmarks on CQ.'))

  if description != original_description:
   rietveld_obj.update_description(issue, description)

  return results

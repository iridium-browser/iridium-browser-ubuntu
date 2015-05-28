#!/usr/bin/python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Checks third-party licenses for the purposes of the Android WebView build.

The Android tree includes a snapshot of Chromium in order to power the system
WebView.  This tool checks that all code uses open-source licenses compatible
with Android, and that we meet the requirements of those licenses. It can also
be used to generate an Android NOTICE file for the third-party code.

It makes use of src/tools/licenses.py and the README.chromium files on which
it depends. It also makes use of a data file, third_party_files_whitelist.txt,
which whitelists indicidual files which contain third-party code but which
aren't in a third-party directory with a README.chromium file.
"""

import glob
import imp
import json
import multiprocessing
import optparse
import os
import re
import sys
import textwrap


REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))

# Import third_party/PRESUBMIT.py via imp to avoid importing a random
# PRESUBMIT.py from $PATH, also make sure we don't generate a .pyc file.
sys.dont_write_bytecode = True
third_party = \
  imp.load_source('PRESUBMIT', \
                  os.path.join(REPOSITORY_ROOT, 'third_party', 'PRESUBMIT.py'))

sys.path.append(os.path.join(REPOSITORY_ROOT, 'third_party'))
import jinja2
sys.path.append(os.path.join(REPOSITORY_ROOT, 'tools'))
import licenses

import copyright_scanner
import known_issues

class InputApi(object):
  def __init__(self):
    self.os_path = os.path
    self.os_walk = os.walk
    self.re = re
    self.ReadFile = _ReadFile
    self.change = InputApiChange()

class InputApiChange(object):
  def __init__(self):
    self.RepositoryRoot = lambda: REPOSITORY_ROOT


def GetIncompatibleDirectories():
  """Gets a list of third-party directories which use licenses incompatible
  with Android. This is used by the snapshot tool.
  Returns:
    A list of directories.
  """

  result = []
  for directory in _FindThirdPartyDirs():
    if directory in known_issues.KNOWN_ISSUES:
      result.append(directory)
      continue
    try:
      metadata = licenses.ParseDir(directory, REPOSITORY_ROOT,
                                   require_license_file=False,
                                   optional_keys=['License Android Compatible'])
    except licenses.LicenseError as e:
      print 'Got LicenseError while scanning ' + directory
      raise
    if metadata.get('License Android Compatible', 'no').upper() == 'YES':
      continue
    license = re.split(' [Ll]icenses?$', metadata['License'])[0]
    if not third_party.LicenseIsCompatibleWithAndroid(InputApi(), license):
      result.append(directory)
  return result

def GetUnknownIncompatibleDirectories():
  """Gets a list of third-party directories which use licenses incompatible
  with Android which are not present in the known_issues.py file.
  This is used by the AOSP bot.
  Returns:
    A list of directories.
  """
  incompatible_directories = frozenset(GetIncompatibleDirectories())
  known_incompatible = []
  input_api = InputApi()
  for path, exclude_list in known_issues.KNOWN_INCOMPATIBLE.iteritems():
    path = copyright_scanner.ForwardSlashesToOsPathSeps(input_api, path)
    for exclude in exclude_list:
      exclude = copyright_scanner.ForwardSlashesToOsPathSeps(input_api, exclude)
      if glob.has_magic(exclude):
        exclude_dirname = os.path.dirname(exclude)
        if glob.has_magic(exclude_dirname):
          print ('Exclude path %s contains an unexpected glob expression,' \
                 ' skipping.' % exclude)
        exclude = exclude_dirname
      known_incompatible.append(os.path.normpath(os.path.join(path, exclude)))
  known_incompatible = frozenset(known_incompatible)
  return incompatible_directories.difference(known_incompatible)


class ScanResult(object):
  Ok, Warnings, Errors = range(3)

# Needs to be a top-level function for multiprocessing
def _FindCopyrightViolations(files_to_scan_as_string):
  return copyright_scanner.FindCopyrightViolations(
    InputApi(), REPOSITORY_ROOT, files_to_scan_as_string)

def _ShardList(l, shard_len):
  return [l[i:i + shard_len] for i in range(0, len(l), shard_len)]

def _CheckLicenseHeaders(excluded_dirs_list, whitelisted_files):
  """Checks that all files which are not in a listed third-party directory,
  and which do not use the standard Chromium license, are whitelisted.
  Args:
    excluded_dirs_list: The list of directories to exclude from scanning.
    whitelisted_files: The whitelist of files.
  Returns:
    ScanResult.Ok if all files with non-standard license headers are whitelisted
    and the whitelist contains no stale entries;
    ScanResult.Warnings if there are stale entries;
    ScanResult.Errors if new non-whitelisted entries found.
  """
  input_api = InputApi()
  files_to_scan = copyright_scanner.FindFiles(
    input_api, REPOSITORY_ROOT, ['.'], excluded_dirs_list)
  sharded_files_to_scan = _ShardList(files_to_scan, 2000)
  pool = multiprocessing.Pool()
  offending_files_chunks = pool.map_async(
      _FindCopyrightViolations, sharded_files_to_scan).get(999999)
  pool.close()
  pool.join()
  # Flatten out the result
  offending_files = \
    [item for sublist in offending_files_chunks for item in sublist]

  (unknown, missing, stale) = copyright_scanner.AnalyzeScanResults(
    input_api, whitelisted_files, offending_files)

  if unknown:
    print 'The following files contain a third-party license but are not in ' \
          'a listed third-party directory and are not whitelisted. You must ' \
          'add the following files to the whitelist.\n%s' % \
          '\n'.join(sorted(unknown))
  if missing:
    print 'The following files are whitelisted, but do not exist.\n%s' % \
        '\n'.join(sorted(missing))
  if stale:
    print 'The following files are whitelisted unnecessarily. You must ' \
          'remove the following files from the whitelist.\n%s' % \
          '\n'.join(sorted(stale))

  if unknown:
    code = ScanResult.Errors
  elif stale or missing:
    code = ScanResult.Warnings
  else:
    code = ScanResult.Ok

  problem_paths = sorted(set(unknown + missing + stale))
  return (code, problem_paths)


def _ReadFile(full_path, mode='rU'):
  """Reads a file from disk. This emulates presubmit InputApi.ReadFile func.
  Args:
    full_path: The path of the file to read.
  Returns:
    The contents of the file as a string.
  """

  with open(full_path, mode) as f:
    return f.read()


def _ReadLocalFile(path, mode='rb'):
  """Reads a file from disk.
  Args:
    path: The path of the file to read, relative to the root of the repository.
  Returns:
    The contents of the file as a string.
  """

  return _ReadFile(os.path.join(REPOSITORY_ROOT, path), mode)


def _FindThirdPartyDirs():
  """Gets the list of third-party directories.
  Returns:
    The list of third-party directories.
  """

  # Please don't add here paths that have problems with license files,
  # as they will end up included in Android WebView snapshot.
  # Instead, add them into known_issues.py.
  prune_paths = [
    # Temporary until we figure out how not to check out quickoffice on the
    # Android license check bot. Tracked in crbug.com/350472.
    os.path.join('chrome', 'browser', 'resources', 'chromeos', 'quickoffice'),
    # Placeholder directory, no third-party code.
    os.path.join('third_party', 'adobe'),
    # Apache 2.0 license. See
    # https://code.google.com/p/chromium/issues/detail?id=140478.
    os.path.join('third_party', 'bidichecker'),
    # Isn't checked out on clients
    os.path.join('third_party', 'gles2_conform'),
    # The llvm-build doesn't exist for non-clang builder
    os.path.join('third_party', 'llvm-build'),
    # Binaries doesn't apply to android
    os.path.join('third_party', 'widevine'),
    # third_party directories in this tree aren't actually third party, but
    # provide a way to shadow experimental buildfiles into those directories.
    os.path.join('build', 'secondary'),
    # Not shipped, Chromium code
    os.path.join('tools', 'swarming_client'),
    # Not shipped, only relates to Chrome for Android, but not to WebView
    os.path.join('clank'),
    # Bots only, is not a part of the build
    os.path.join('isolate_deps_dir'),
  ]
  third_party_dirs = licenses.FindThirdPartyDirs(prune_paths, REPOSITORY_ROOT)
  return licenses.FilterDirsWithFiles(third_party_dirs, REPOSITORY_ROOT)


def _Scan():
  """Checks that license meta-data is present for all third-party code and
     that all non third-party code doesn't contain external copyrighted code.
  Returns:
    ScanResult.Ok if everything is in order;
    ScanResult.Warnings if there are non-fatal problems (e.g. stale whitelist
      entries)
    ScanResult.Errors otherwise.
  """

  third_party_dirs = _FindThirdPartyDirs()

  problem_paths = []

  # First, check designated third-party directories using src/tools/licenses.py.
  all_licenses_valid = True
  for path in sorted(third_party_dirs):
    try:
      licenses.ParseDir(path, REPOSITORY_ROOT)
    except licenses.LicenseError, e:
      if not (path in known_issues.KNOWN_ISSUES):
        print 'Got LicenseError "%s" while scanning %s' % (e, path)
        problem_paths.append(path)
        all_licenses_valid = False

  # Second, check for non-standard license text.
  whitelisted_files = copyright_scanner.LoadWhitelistedFilesList(InputApi())
  licenses_check, more_problem_paths = _CheckLicenseHeaders(
      third_party_dirs, whitelisted_files)

  problem_paths.extend(more_problem_paths)

  return (licenses_check if all_licenses_valid else ScanResult.Errors,
          problem_paths)


class TemplateEntryGenerator(object):
  def __init__(self):
    self._generate_licenses_file_list_only = False
    self._toc_index = 0

  def SetGenerateLicensesFileListOnly(self, generate_licenses_file_list_only):
    self._generate_licenses_file_list_only = generate_licenses_file_list_only

  def _ReadFileGuessEncoding(self, name):
    if self._generate_licenses_file_list_only:
      return ''
    contents = ''
    with open(name, 'rb') as input_file:
      contents = input_file.read()
    try:
      return contents.decode('utf8')
    except UnicodeDecodeError:
      pass
    # If it's not UTF-8, it must be CP-1252. Fail otherwise.
    return contents.decode('cp1252')

  def MetadataToTemplateEntry(self, metadata):
    self._toc_index += 1
    return {
      'name': metadata['Name'],
      'url': metadata['URL'],
      'license_file': metadata['License File'],
      'license': self._ReadFileGuessEncoding(metadata['License File']),
      'toc_href': 'entry' + str(self._toc_index),
    }


def GenerateNoticeFile(generate_licenses_file_list_only=False):
  """Generates the contents of an Android NOTICE file for the third-party code.
  This is used by the snapshot tool.
  Returns:
    The contents of the NOTICE file.
  """

  generator = TemplateEntryGenerator()
  generator.SetGenerateLicensesFileListOnly(generate_licenses_file_list_only)
  # Start from Chromium's LICENSE file
  entries = [generator.MetadataToTemplateEntry({
    'Name': 'The Chromium Project',
    'URL': 'http://www.chromium.org',
    'License File': os.path.join(REPOSITORY_ROOT, 'LICENSE') })
  ]

  third_party_dirs = _FindThirdPartyDirs()
  # We provide attribution for all third-party directories.
  # TODO(mnaganov): Limit this to only code used by the WebView binary.
  for directory in sorted(third_party_dirs):
    try:
      metadata = licenses.ParseDir(directory, REPOSITORY_ROOT,
                                   require_license_file=False)
    except licenses.LicenseError:
      # Since this code is called during project files generation,
      # we don't want to break the it. But we assume that release
      # WebView apks are built using checkouts that pass
      # 'webview_licenses.py scan' check, thus they don't contain
      # projects with non-compatible licenses.
      continue
    license_file = metadata['License File']
    if license_file and license_file != licenses.NOT_SHIPPED:
      entries.append(generator.MetadataToTemplateEntry(metadata))

  if generate_licenses_file_list_only:
    return [entry['license_file'] for entry in entries]
  else:
    env = jinja2.Environment(
      loader=jinja2.FileSystemLoader(os.path.dirname(__file__)),
      extensions=['jinja2.ext.autoescape'])
    template = env.get_template('licenses_notice.tmpl')
    return template.render({ 'entries': entries }).encode('utf8')


def _ProcessIncompatibleResult(incompatible_directories):
  if incompatible_directories:
    print ("Incompatibly licensed directories found:\n" +
           "\n".join(sorted(incompatible_directories)))
    return ScanResult.Errors
  return ScanResult.Ok

def main():
  class FormatterWithNewLines(optparse.IndentedHelpFormatter):
    def format_description(self, description):
      paras = description.split('\n')
      formatted_paras = [textwrap.fill(para, self.width) for para in paras]
      return '\n'.join(formatted_paras) + '\n'

  parser = optparse.OptionParser(formatter=FormatterWithNewLines(),
                                 usage='%prog [options]')
  parser.add_option('--json', help='Path to JSON output file')
  parser.description = (__doc__ +
                        '\nCommands:\n'
                        '  scan Check licenses.\n'
                        '  notice_deps Generate the list of dependencies for '
                        'Android NOTICE file.\n'
                        '  notice [file] Generate Android NOTICE file on '
                        'stdout or into |file|.\n'
                        '  incompatible_directories Scan for incompatibly'
                        ' licensed directories.\n'
                        '  all_incompatible_directories Scan for incompatibly'
                        ' licensed directories (even those in'
                        ' known_issues.py).\n'
                        '  display_copyrights Display autorship on the files'
                        ' using names provided via stdin.\n')
  (options, args) = parser.parse_args()
  if len(args) < 1:
    parser.print_help()
    return ScanResult.Errors

  if args[0] == 'scan':
    scan_result, problem_paths = _Scan()
    if scan_result == ScanResult.Ok:
      print 'OK!'
    if options.json:
      with open(options.json, 'w') as f:
        json.dump(problem_paths, f)
    return scan_result
  elif args[0] == 'notice_deps':
    # 'set' is used to eliminate duplicate references to the same license file.
    print ' '.join(
      sorted(set(GenerateNoticeFile(generate_licenses_file_list_only=True))))
    return ScanResult.Ok
  elif args[0] == 'notice':
    notice_file_contents = GenerateNoticeFile()
    if len(args) == 1:
      print notice_file_contents
    else:
      with open(args[1], 'w') as output_file:
        output_file.write(notice_file_contents)
    return ScanResult.Ok
  elif args[0] == 'incompatible_directories':
    return _ProcessIncompatibleResult(GetUnknownIncompatibleDirectories())
  elif args[0] == 'all_incompatible_directories':
    return _ProcessIncompatibleResult(GetIncompatibleDirectories())
  elif args[0] == 'display_copyrights':
    files = sys.stdin.read().splitlines()
    for f, c in \
        zip(files, copyright_scanner.FindCopyrights(InputApi(), '.', files)):
      print f, '\t', ' / '.join(sorted(c))
    return ScanResult.Ok
  parser.print_help()
  return ScanResult.Errors

if __name__ == '__main__':
  sys.exit(main())

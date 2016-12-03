# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Top-level presubmit script for Skia.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import collections
import csv
import fnmatch
import os
import re
import subprocess
import sys
import traceback


REVERT_CL_SUBJECT_PREFIX = 'Revert '

SKIA_TREE_STATUS_URL = 'http://skia-tree-status.appspot.com'

# Please add the complete email address here (and not just 'xyz@' or 'xyz').
PUBLIC_API_OWNERS = (
    'reed@chromium.org',
    'reed@google.com',
    'bsalomon@chromium.org',
    'bsalomon@google.com',
    'djsollen@chromium.org',
    'djsollen@google.com',
)

AUTHORS_FILE_NAME = 'AUTHORS'

DOCS_PREVIEW_URL = 'https://skia.org/?cl='
GOLD_TRYBOT_URL = 'https://gold.skia.org/search?issue='

# Path to CQ bots feature is described in https://bug.skia.org/4364
PATH_PREFIX_TO_EXTRA_TRYBOTS = {
    # pylint: disable=line-too-long
    'cmake/': 'master.client.skia.compile:Build-Mac-Clang-x86_64-Release-CMake-Trybot,Build-Ubuntu-GCC-x86_64-Release-CMake-Trybot',
    # pylint: disable=line-too-long
    'src/opts/': 'master.client.skia:Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Release-SKNX_NO_SIMD-Trybot',

    'include/private/SkAtomics.h': ('master.client.skia:'
      'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Release-TSAN-Trybot,'
      'Test-Ubuntu-GCC-Golo-GPU-GT610-x86_64-Release-TSAN-Trybot'
    ),

    # Below are examples to show what is possible with this feature.
    # 'src/svg/': 'master1:abc;master2:def',
    # 'src/svg/parser/': 'master3:ghi,jkl;master4:mno',
    # 'src/image/SkImage_Base.h': 'master5:pqr,stu;master1:abc1;master2:def',
}


def _CheckChangeHasEol(input_api, output_api, source_file_filter=None):
  """Checks that files end with atleast one \n (LF)."""
  eof_files = []
  for f in input_api.AffectedSourceFiles(source_file_filter):
    contents = input_api.ReadFile(f, 'rb')
    # Check that the file ends in atleast one newline character.
    if len(contents) > 1 and contents[-1:] != '\n':
      eof_files.append(f.LocalPath())

  if eof_files:
    return [output_api.PresubmitPromptWarning(
      'These files should end in a newline character:',
      items=eof_files)]
  return []


def _PythonChecks(input_api, output_api):
  """Run checks on any modified Python files."""
  pylint_disabled_warnings = (
      'F0401',  # Unable to import.
      'E0611',  # No name in module.
      'W0232',  # Class has no __init__ method.
      'E1002',  # Use of super on an old style class.
      'W0403',  # Relative import used.
      'R0201',  # Method could be a function.
      'E1003',  # Using class name in super.
      'W0613',  # Unused argument.
      'W0105',  # String statement has no effect.
  )
  # Run Pylint on only the modified python files. Unfortunately it still runs
  # Pylint on the whole file instead of just the modified lines.
  affected_python_files = []
  for affected_file in input_api.AffectedSourceFiles(None):
    affected_file_path = affected_file.LocalPath()
    if affected_file_path.endswith('.py'):
      affected_python_files.append(affected_file_path)
  return input_api.canned_checks.RunPylint(
      input_api, output_api,
      disabled_warnings=pylint_disabled_warnings,
      white_list=affected_python_files)


def _IfDefChecks(input_api, output_api):
  """Ensures if/ifdef are not before includes. See skbug/3362 for details."""
  comment_block_start_pattern = re.compile('^\s*\/\*.*$')
  comment_block_middle_pattern = re.compile('^\s+\*.*')
  comment_block_end_pattern = re.compile('^\s+\*\/.*$')
  single_line_comment_pattern = re.compile('^\s*//.*$')
  def is_comment(line):
    return (comment_block_start_pattern.match(line) or
            comment_block_middle_pattern.match(line) or
            comment_block_end_pattern.match(line) or
            single_line_comment_pattern.match(line))

  empty_line_pattern = re.compile('^\s*$')
  def is_empty_line(line):
    return empty_line_pattern.match(line)

  failing_files = []
  for affected_file in input_api.AffectedSourceFiles(None):
    affected_file_path = affected_file.LocalPath()
    if affected_file_path.endswith('.cpp') or affected_file_path.endswith('.h'):
      f = open(affected_file_path)
      for line in f.xreadlines():
        if is_comment(line) or is_empty_line(line):
          continue
        # The below will be the first real line after comments and newlines.
        if line.startswith('#if 0 '):
          pass
        elif line.startswith('#if ') or line.startswith('#ifdef '):
          failing_files.append(affected_file_path)
        break

  results = []
  if failing_files:
    results.append(
        output_api.PresubmitError(
            'The following files have #if or #ifdef before includes:\n%s\n\n'
            'See https://bug.skia.org/3362 for why this should be fixed.' %
                '\n'.join(failing_files)))
  return results


def _CopyrightChecks(input_api, output_api, source_file_filter=None):
  results = []
  year_pattern = r'\d{4}'
  year_range_pattern = r'%s(-%s)?' % (year_pattern, year_pattern)
  years_pattern = r'%s(,%s)*,?' % (year_range_pattern, year_range_pattern)
  copyright_pattern = (
      r'Copyright (\([cC]\) )?%s \w+' % years_pattern)

  for affected_file in input_api.AffectedSourceFiles(source_file_filter):
    if 'third_party' in affected_file.LocalPath():
      continue
    contents = input_api.ReadFile(affected_file, 'rb')
    if not re.search(copyright_pattern, contents):
      results.append(output_api.PresubmitError(
          '%s is missing a correct copyright header.' % affected_file))
  return results


def _ToolFlags(input_api, output_api):
  """Make sure `{dm,nanobench}_flags.py test` passes if modified."""
  results = []
  sources = lambda x: ('dm_flags.py'        in x.LocalPath() or
                       'nanobench_flags.py' in x.LocalPath())
  for f in input_api.AffectedSourceFiles(sources):
    if 0 != subprocess.call(['python', f.LocalPath(), 'test']):
      results.append(output_api.PresubmitError('`python %s test` failed' % f))
  return results


def _RecipeSimulationTest(input_api, output_api):
  """Run the recipe simulation test."""
  results = []
  if not any(f.LocalPath().startswith('infra')
             for f in input_api.AffectedFiles()):
    return results

  recipes_py = os.path.join('infra', 'bots', 'recipes.py')
  cmd = ['python', recipes_py, 'simulation_test']
  try:
    subprocess.check_output(cmd)
  except subprocess.CalledProcessError as e:
    results.append(output_api.PresubmitError(
        '`%s` failed:\n%s' % (' '.join(cmd), e.output)))
  return results

def _CheckGNFormatted(input_api, output_api):
  """Make sure any .gn files we're changing have been formatted."""
  results = []
  for f in input_api.AffectedFiles():
    if not f.LocalPath().endswith('.gn'):
      continue

    cmd = ['gn', 'format', '--dry-run', f.LocalPath()]
    try:
      subprocess.check_output(cmd)
    except subprocess.CalledProcessError:
      fix = 'gn format ' + f.LocalPath()
      results.append(output_api.PresubmitError(
          '`%s` failed, try\n\t%s' % (' '.join(cmd), fix)))
  return results


def _CommonChecks(input_api, output_api):
  """Presubmit checks common to upload and commit."""
  results = []
  sources = lambda x: (x.LocalPath().endswith('.h') or
                       x.LocalPath().endswith('.gypi') or
                       x.LocalPath().endswith('.gyp') or
                       x.LocalPath().endswith('.py') or
                       x.LocalPath().endswith('.sh') or
                       x.LocalPath().endswith('.m') or
                       x.LocalPath().endswith('.mm') or
                       x.LocalPath().endswith('.go') or
                       x.LocalPath().endswith('.c') or
                       x.LocalPath().endswith('.cc') or
                       x.LocalPath().endswith('.cpp'))
  results.extend(
      _CheckChangeHasEol(
          input_api, output_api, source_file_filter=sources))
  results.extend(_PythonChecks(input_api, output_api))
  results.extend(_IfDefChecks(input_api, output_api))
  results.extend(_CopyrightChecks(input_api, output_api,
                                  source_file_filter=sources))
  results.extend(_ToolFlags(input_api, output_api))
  return results


def CheckChangeOnUpload(input_api, output_api):
  """Presubmit checks for the change on upload.

  The following are the presubmit checks:
  * Check change has one and only one EOL.
  """
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  # Run on upload, not commit, since the presubmit bot apparently doesn't have
  # coverage installed.
  results.extend(_RecipeSimulationTest(input_api, output_api))
  results.extend(_CheckGNFormatted(input_api, output_api))
  return results


def _CheckTreeStatus(input_api, output_api, json_url):
  """Check whether to allow commit.

  Args:
    input_api: input related apis.
    output_api: output related apis.
    json_url: url to download json style status.
  """
  tree_status_results = input_api.canned_checks.CheckTreeIsOpen(
      input_api, output_api, json_url=json_url)
  if not tree_status_results:
    # Check for caution state only if tree is not closed.
    connection = input_api.urllib2.urlopen(json_url)
    status = input_api.json.loads(connection.read())
    connection.close()
    if ('caution' in status['message'].lower() and
        os.isatty(sys.stdout.fileno())):
      # Display a prompt only if we are in an interactive shell. Without this
      # check the commit queue behaves incorrectly because it considers
      # prompts to be failures.
      short_text = 'Tree state is: ' + status['general_state']
      long_text = status['message'] + '\n' + json_url
      tree_status_results.append(
          output_api.PresubmitPromptWarning(
              message=short_text, long_text=long_text))
  else:
    # Tree status is closed. Put in message about contacting sheriff.
    connection = input_api.urllib2.urlopen(
        SKIA_TREE_STATUS_URL + '/current-sheriff')
    sheriff_details = input_api.json.loads(connection.read())
    if sheriff_details:
      tree_status_results[0]._message += (
          '\n\nPlease contact the current Skia sheriff (%s) if you are trying '
          'to submit a build fix\nand do not know how to submit because the '
          'tree is closed') % sheriff_details['username']
  return tree_status_results


def _CheckOwnerIsInAuthorsFile(input_api, output_api):
  results = []
  issue = input_api.change.issue
  if issue and input_api.rietveld:
    issue_properties = input_api.rietveld.get_issue_properties(
        issue=int(issue), messages=False)
    owner_email = issue_properties['owner_email']

    try:
      authors_content = ''
      for line in open(AUTHORS_FILE_NAME):
        if not line.startswith('#'):
          authors_content += line
      email_fnmatches = re.findall('<(.*)>', authors_content)
      for email_fnmatch in email_fnmatches:
        if fnmatch.fnmatch(owner_email, email_fnmatch):
          # Found a match, the user is in the AUTHORS file break out of the loop
          break
      else:
        results.append(
          output_api.PresubmitError(
            'The email %s is not in Skia\'s AUTHORS file.\n'
            'Issue owner, this CL must include an addition to the Skia AUTHORS '
            'file.'
            % owner_email))
    except IOError:
      # Do not fail if authors file cannot be found.
      traceback.print_exc()
      input_api.logging.error('AUTHORS file not found!')

  return results


def _CheckLGTMsForPublicAPI(input_api, output_api):
  """Check LGTMs for public API changes.

  For public API files make sure there is an LGTM from the list of owners in
  PUBLIC_API_OWNERS.
  """
  results = []
  requires_owner_check = False
  for affected_file in input_api.AffectedFiles():
    affected_file_path = affected_file.LocalPath()
    file_path, file_ext = os.path.splitext(affected_file_path)
    # We only care about files that end in .h and are under the top-level
    # include dir, but not include/private.
    if (file_ext == '.h' and
        'include' == file_path.split(os.path.sep)[0] and
        'private' not in file_path):
      requires_owner_check = True

  if not requires_owner_check:
    return results

  lgtm_from_owner = False
  issue = input_api.change.issue
  if issue and input_api.rietveld:
    issue_properties = input_api.rietveld.get_issue_properties(
        issue=int(issue), messages=True)
    if re.match(REVERT_CL_SUBJECT_PREFIX, issue_properties['subject'], re.I):
      # It is a revert CL, ignore the public api owners check.
      return results

    if issue_properties['cq_dry_run']:
      # Ignore public api owners check for dry run CLs since they are not
      # going to be committed.
      return results

    match = re.search(r'^TBR=(.*)$', issue_properties['description'], re.M)
    if match:
      tbr_entries = match.group(1).strip().split(',')
      for owner in PUBLIC_API_OWNERS:
        if owner in tbr_entries or owner.split('@')[0] in tbr_entries:
          # If an owner is specified in the TBR= line then ignore the public
          # api owners check.
          return results

    if issue_properties['owner_email'] in PUBLIC_API_OWNERS:
      # An owner created the CL that is an automatic LGTM.
      lgtm_from_owner = True

    messages = issue_properties.get('messages')
    if messages:
      for message in messages:
        if (message['sender'] in PUBLIC_API_OWNERS and
            'lgtm' in message['text'].lower()):
          # Found an lgtm in a message from an owner.
          lgtm_from_owner = True
          break

  if not lgtm_from_owner:
    results.append(
        output_api.PresubmitError(
            "If this CL adds to or changes Skia's public API, you need an LGTM "
            "from any of %s.  If this CL only removes from or doesn't change "
            "Skia's public API, please add a short note to the CL saying so "
            "and add one of those reviewers on a TBR= line.  If you don't know "
            "if this CL affects Skia's public API, treat it like it does."
            % str(PUBLIC_API_OWNERS)))
  return results


def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook does the following:
  * Adds a link to the CL's Gold trybot results.
  * Adds a link to preview docs changes if there are any docs changes in the CL.
  * Adds 'NOTRY=true' if the CL contains only docs changes.
  * Adds 'NOTREECHECKS=true' for non master branch changes since they do not
    need to be gated on the master branch's tree.
  * Adds 'NOTRY=true' for non master branch changes since trybots do not yet
    work on them.
  * Adds 'NOPRESUBMIT=true' for non master branch changes since those don't
    run the presubmit checks.
  * Adds extra trybots for the paths defined in PATH_TO_EXTRA_TRYBOTS.
  """

  results = []
  atleast_one_docs_change = False
  all_docs_changes = True
  for affected_file in change.AffectedFiles():
    affected_file_path = affected_file.LocalPath()
    file_path, _ = os.path.splitext(affected_file_path)
    if 'site' == file_path.split(os.path.sep)[0]:
      atleast_one_docs_change = True
    else:
      all_docs_changes = False
    if atleast_one_docs_change and not all_docs_changes:
      break

  issue = cl.issue
  rietveld_obj = cl.RpcServer()
  if issue and rietveld_obj:
    original_description = rietveld_obj.get_description(issue)
    new_description = original_description

    # Add GOLD_TRYBOT_URL if it does not exist yet.
    if not re.search(r'^GOLD_TRYBOT_URL=', new_description, re.M | re.I):
      new_description += '\nGOLD_TRYBOT_URL= %s%s' % (GOLD_TRYBOT_URL, issue)
      results.append(
          output_api.PresubmitNotifyResult(
              'Added link to Gold trybot runs to the CL\'s description.\n'
              'Note: Results may take sometime to be populated after trybots '
              'complete.'))

    # If the change includes only doc changes then add NOTRY=true in the
    # CL's description if it does not exist yet.
    if all_docs_changes and not re.search(
        r'^NOTRY=true$', new_description, re.M | re.I):
      new_description += '\nNOTRY=true'
      results.append(
          output_api.PresubmitNotifyResult(
              'This change has only doc changes. Automatically added '
              '\'NOTRY=true\' to the CL\'s description'))

    # If there is atleast one docs change then add preview link in the CL's
    # description if it does not already exist there.
    if atleast_one_docs_change and not re.search(
        r'^DOCS_PREVIEW=.*', new_description, re.M | re.I):
      # Automatically add a link to where the docs can be previewed.
      new_description += '\nDOCS_PREVIEW= %s%s' % (DOCS_PREVIEW_URL, issue)
      results.append(
          output_api.PresubmitNotifyResult(
              'Automatically added a link to preview the docs changes to the '
              'CL\'s description'))

    # If the target ref is not master then add NOTREECHECKS=true and NOTRY=true
    # to the CL's description if it does not already exist there.
    target_ref = rietveld_obj.get_issue_properties(issue, False).get(
        'target_ref', '')
    if target_ref != 'refs/heads/master':
      if not re.search(
          r'^NOTREECHECKS=true$', new_description, re.M | re.I):
        new_description += "\nNOTREECHECKS=true"
        results.append(
            output_api.PresubmitNotifyResult(
                'Branch changes do not need to rely on the master branch\'s '
                'tree status. Automatically added \'NOTREECHECKS=true\' to the '
                'CL\'s description'))
      if not re.search(
          r'^NOTRY=true$', new_description, re.M | re.I):
        new_description += "\nNOTRY=true"
        results.append(
            output_api.PresubmitNotifyResult(
                'Trybots do not yet work for non-master branches. '
                'Automatically added \'NOTRY=true\' to the CL\'s description'))
      if not re.search(
          r'^NOPRESUBMIT=true$', new_description, re.M | re.I):
        new_description += "\nNOPRESUBMIT=true"
        results.append(
            output_api.PresubmitNotifyResult(
                'Branch changes do not run the presubmit checks.'))

    # Automatically set CQ_INCLUDE_TRYBOTS if any of the changed files here
    # begin with the paths of interest.
    cq_master_to_trybots = collections.defaultdict(set)
    for affected_file in change.AffectedFiles():
      affected_file_path = affected_file.LocalPath()
      for path_prefix, extra_bots in PATH_PREFIX_TO_EXTRA_TRYBOTS.iteritems():
        if affected_file_path.startswith(path_prefix):
          results.append(
              output_api.PresubmitNotifyResult(
                  'Your CL modifies the path %s.\nAutomatically adding %s to '
                  'the CL description.' % (affected_file_path, extra_bots)))
          _MergeCQExtraTrybotsMaps(
              cq_master_to_trybots, _GetCQExtraTrybotsMap(extra_bots))
    if cq_master_to_trybots:
      new_description = _AddCQExtraTrybotsToDesc(
          cq_master_to_trybots, new_description)

    # If the description has changed update it.
    if new_description != original_description:
      rietveld_obj.update_description(issue, new_description)

    return results


def _AddCQExtraTrybotsToDesc(cq_master_to_trybots, description):
  """Adds the specified master and trybots to the CQ_INCLUDE_TRYBOTS keyword.

  If the keyword already exists in the description then it appends to it only
  if the specified values do not already exist.
  If the keyword does not exist then it creates a new section in the
  description.
  """
  match = re.search(r'^CQ_INCLUDE_TRYBOTS=(.*)$', description, re.M | re.I)
  if match:
    original_trybots_map = _GetCQExtraTrybotsMap(match.group(1))
    _MergeCQExtraTrybotsMaps(cq_master_to_trybots, original_trybots_map)
    new_description = description.replace(
        match.group(0), _GetCQExtraTrybotsStr(cq_master_to_trybots))
  else:
    new_description = description + "\n%s" % (
        _GetCQExtraTrybotsStr(cq_master_to_trybots))
  return new_description


def _MergeCQExtraTrybotsMaps(dest_map, map_to_be_consumed):
  """Merges two maps of masters to trybots into one."""
  for master, trybots in map_to_be_consumed.iteritems():
    dest_map[master].update(trybots)
  return dest_map


def _GetCQExtraTrybotsMap(cq_extra_trybots_str):
  """Parses CQ_INCLUDE_TRYBOTS str and returns a map of masters to trybots."""
  cq_master_to_trybots = collections.defaultdict(set)
  for section in cq_extra_trybots_str.split(';'):
    if section:
      master, bots = section.split(':')
      cq_master_to_trybots[master].update(bots.split(','))
  return cq_master_to_trybots


def _GetCQExtraTrybotsStr(cq_master_to_trybots):
  """Constructs the CQ_INCLUDE_TRYBOTS str from a map of masters to trybots."""
  sections = []
  for master, trybots in cq_master_to_trybots.iteritems():
    sections.append('%s:%s' % (master, ','.join(trybots)))
  return 'CQ_INCLUDE_TRYBOTS=%s' % ';'.join(sections)


def CheckChangeOnCommit(input_api, output_api):
  """Presubmit checks for the change on commit.

  The following are the presubmit checks:
  * Check change has one and only one EOL.
  * Ensures that the Skia tree is open in
    http://skia-tree-status.appspot.com/. Shows a warning if it is in 'Caution'
    state and an error if it is in 'Closed' state.
  """
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(
      _CheckTreeStatus(input_api, output_api, json_url=(
          SKIA_TREE_STATUS_URL + '/banner-status?format=json')))
  results.extend(_CheckLGTMsForPublicAPI(input_api, output_api))
  results.extend(_CheckOwnerIsInAuthorsFile(input_api, output_api))
  return results

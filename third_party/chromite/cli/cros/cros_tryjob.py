# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros tryjob: Schedule a tryjob."""

from __future__ import print_function

import os

from chromite.lib import constants
from chromite.cli import command
from chromite.lib import config_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import remote_try

from chromite.cbuildbot import trybot_patch_pool


REMOTE = 'remote'
SWARMING = 'swarming'
LOCAL = 'local'
CBUILDBOT = 'cbuildbot'


def ConfigsToPrint(site_config, production, build_config_fragments):
  """Select a list of buildbot configs to print out.

  Args:
    site_config: config_lib.SiteConfig containing all config info.
    production: Display tryjob or production configs?.
    build_config_fragments: List of strings to filter config names with.

  Returns:
    List of config_lib.BuildConfig objects.
  """
  configs = site_config.values()

  def optionsMatch(config):
    # In this case, build_configs are config name fragments. If the config
    # name doesn't contain any of the fragments, filter it out.
    for build_config in build_config_fragments:
      if build_config not in config.name:
        return False

    return config_lib.isTryjobConfig(config) != production

  # All configs, filtered by optionsMatch.
  configs = [config for config in configs if optionsMatch(config)]

  # Sort build type, then board.
  # 'daisy-paladin-tryjob' -> ['tryjob', 'paladin', 'daisy']
  configs.sort(key=lambda c: list(reversed(c.name.split('-'))))

  return configs

def PrintKnownConfigs(site_config, production, build_config_fragments):
  """Print a list of known buildbot configs.

  Args:
    site_config: config_lib.SiteConfig containing all config info.
    production: Display tryjob or production configs?.
    build_config_fragments: List of strings to filter config names with.
  """
  configs = ConfigsToPrint(site_config, production, build_config_fragments)

  COLUMN_WIDTH = max(len(c.name) for c in configs) + 1
  if production:
    print('Production configs:')
  else:
    print('Tryjob configs:')

  print('config'.ljust(COLUMN_WIDTH), 'description')
  print('------'.ljust(COLUMN_WIDTH), '-----------')
  for config in configs:
    desc = config.description or ''
    print(config.name.ljust(COLUMN_WIDTH), desc)


def CbuildbotArgs(options):
  """Function to generate cbuidlbot command line args.

  This are pre-api version filtering.

  Args:
    options: Parsed cros tryjob tryjob arguments.

  Returns:
    List of strings in ['arg1', 'arg2'] format
  """
  args = []

  if options.where in (REMOTE, SWARMING):
    if options.production:
      args.append('--buildbot')
    else:
      args.append('--remote-trybot')

  elif options.where == LOCAL:
    args.append('--no-buildbot-tags')
    if options.production:
      # This is expected to fail on workstations without an explicit --debug,
      # or running 'branch-util'.
      args.append('--buildbot')
    else:
      args.append('--debug')

  elif options.where == CBUILDBOT:
    args.extend(('--buildbot', '--nobootstrap', '--noreexec',
                 '--no-buildbot-tags'))
    if not options.production:
      args.append('--debug')

  else:
    raise Exception('Unknown options.where: %s', options.where)

  if options.buildroot:
    args.extend(('--buildroot', options.buildroot))

  if options.branch:
    args.extend(('-b', options.branch))

  for g in options.gerrit_patches:
    args.extend(('-g', g))

  if options.passthrough:
    args.extend(options.passthrough)

  if options.passthrough_raw:
    args.extend(options.passthrough_raw)

  return args


def CreateBuildrootIfNeeded(buildroot):
  """Create the buildroot is it doesn't exist with confirmation prompt.

  Args:
    buildroot: The buildroot path to create as a string.

  Returns:
    boolean: Does the buildroot now exist?
  """
  if os.path.exists(buildroot):
    return True

  prompt = 'Create %s as buildroot' % buildroot
  if not cros_build_lib.BooleanPrompt(prompt=prompt, default=False):
    print('Please specify a different buildroot via the --buildroot option.')
    return False

  os.makedirs(buildroot)
  return True


def RunLocal(options):
  """Run a local tryjob.

  Args:
    options: Parsed cros tryjob tryjob arguments.

  Returns:
    Exit code of build as an int.
  """
  if cros_build_lib.IsInsideChroot():
    cros_build_lib.Die('Local tryjobs cannot be started inside the chroot.')

  args = CbuildbotArgs(options)

  if not CreateBuildrootIfNeeded(options.buildroot):
    return 1

  # Define the command to run.
  launcher = os.path.join(constants.CHROMITE_DIR, 'scripts', 'cbuildbot_launch')
  cmd = [launcher] + args + options.build_configs

  # Run the tryjob.
  result = cros_build_lib.RunCommand(cmd, debug_level=logging.CRITICAL,
                                     error_code_ok=True, cwd=options.buildroot)
  return result.returncode


def RunCbuildbot(options):
  """Run a cbuildbot build.

  Args:
    options: Parsed cros tryjob tryjob arguments.

  Returns:
    Exit code of build as an int.
  """
  if cros_build_lib.IsInsideChroot():
    cros_build_lib.Die('cbuildbot tryjobs cannot be started inside the chroot.')

  args = CbuildbotArgs(options)

  if not CreateBuildrootIfNeeded(options.buildroot):
    return 1

  # Define the command to run.
  cbuildbot = os.path.join(constants.CHROMITE_BIN_DIR, 'cbuildbot')
  cmd = [cbuildbot] + args + options.build_configs

  # Run the tryjob.
  result = cros_build_lib.RunCommand(cmd, debug_level=logging.CRITICAL,
                                     error_code_ok=True, cwd=options.buildroot)
  return result.returncode


def DisplayLabel(site_config, options, build_config_name):
  """Decide which display_label to use.

  Args:
    site_config: config_lib.SiteConfig containing all config info.
    options: Parsed command line options for cros tryjob.
    build_config_name: Name of the build config we are scheduling.

  Returns:
    String to use as the cbb_build_label value.
  """
  # Our site_config is only valid for the current branch. If the build
  # config is known and has an explicit display_label, use it.
  # to be 'master'.
  if (options.branch == 'master' and
      build_config_name in site_config and
      site_config[build_config_name].display_label):
    return site_config[build_config_name].display_label

  # Fall back to defaults.
  if options.production:
    return config_lib.DISPLAY_LABEL_UNKNOWN_PRODUCTION

  return config_lib.DISPLAY_LABEL_TRYJOB


def RunRemote(site_config, options, patch_pool):
  """Schedule remote tryjobs."""
  logging.info('Scheduling remote tryjob(s): %s',
               ', '.join(options.build_configs))

  # Figure out the cbuildbot command line to pass in.
  args = CbuildbotArgs(options)

  # Figure out the tryjob description.
  description = options.remote_description
  if description is None:
    description = remote_try.DefaultDescription(
        options.branch,
        options.gerrit_patches+options.local_patches)

  print('Submitting tryjob...')
  links = []
  for build_config in options.build_configs:
    tryjob = remote_try.RemoteTryJob(
        build_configs=[build_config],
        display_label=DisplayLabel(site_config, options, build_config),
        remote_description=description,
        branch=options.branch,
        pass_through_args=args,
        local_patches=patch_pool.local_patches,
        committer_email=options.committer_email,
        swarming=options.where == SWARMING,
        master_buildbucket_id='',  # TODO: Add new option to populate.
    )

    tryjob.Submit(dryrun=False)
    links.extend(tryjob.GetTrybotWaterfallLinks())

  print('Tryjob submitted!')
  print('To view your tryjobs, visit:')
  for link in links:
    print('  %s' % link)

def AdjustOptions(options):
  """Set defaults that require some logic.

  Args:
    options: Parsed cros tryjob tryjob arguments.
    site_config: config_lib.SiteConfig containing all config info.
  """
  if options.buildroot:
    return

  if options.where == CBUILDBOT:
    options.buildroot = os.path.join(
        os.path.dirname(constants.SOURCE_ROOT), 'cbuild')

  if options.where == LOCAL:
    options.buildroot = os.path.join(
        os.path.dirname(constants.SOURCE_ROOT), 'tryjob')


def VerifyOptions(options, site_config):
  """Verify that our command line options make sense.

  Args:
    options: Parsed cros tryjob tryjob arguments.
    site_config: config_lib.SiteConfig containing all config info.
  """
  # Handle --list before checking that everything else is valid.
  if options.list:
    PrintKnownConfigs(site_config,
                      options.production,
                      options.build_configs)
    raise cros_build_lib.DieSystemExit(0)  # Exit with success code.

  # Validate specified build_configs.
  if not options.build_configs:
    cros_build_lib.Die('At least one build_config is required.')

  unknown_build_configs = [b for b in options.build_configs
                           if b not in site_config]
  if unknown_build_configs and not options.yes:
    prompt = ('Unknown build configs; are you sure you want to schedule '
              'for %s?' % ', '.join(unknown_build_configs))
    if not cros_build_lib.BooleanPrompt(prompt=prompt, default=False):
      cros_build_lib.Die('No confirmation.')

  # Ensure that production configs are only run with --production.
  if not (options.production or options.where == CBUILDBOT):
    # We can't know if branched configs are tryjob safe.
    # It should always be safe to run a tryjob config with --production.
    prod_configs = []
    for b in options.build_configs:
      if b in site_config and not config_lib.isTryjobConfig(site_config[b]):
        prod_configs.append(b)

    if prod_configs:
      # Die, and explain why.
      alternative_configs = ['%s-tryjob' % b for b in prod_configs]
      msg = ('These configs are not tryjob safe:\n'
             '  %s\n'
             'Consider these configs instead:\n'
             '  %s\n'
             'See go/cros-explicit-tryjob-build-configs-psa.' %
             (', '.join(prod_configs), ', '.join(alternative_configs)))

      if options.branch == 'master':
        # On master branch, we know the status of configs for sure.
        cros_build_lib.Die(msg)
      elif not options.yes:
        # On branches, we are just guessing. Let people override.
        prompt = '%s\nAre you sure you want to continue?' % msg
        if not cros_build_lib.BooleanPrompt(prompt=prompt, default=False):
          cros_build_lib.Die('No confirmation.')

  patches_given = options.gerrit_patches or options.local_patches
  if options.production:
    # Make sure production builds don't have patches.
    if patches_given and not options.debug:
      cros_build_lib.Die('Patches cannot be included in production builds.')
  elif options.where != CBUILDBOT:
    # Ask for confirmation if there are no patches to test.
    if not patches_given and not options.yes:
      prompt = ('No patches were provided; are you sure you want to just '
                'run a build of %s?' % (
                    options.branch if options.branch else 'ToT'))
      if not cros_build_lib.BooleanPrompt(prompt=prompt, default=False):
        cros_build_lib.Die('No confirmation.')

  if options.where in (REMOTE, SWARMING) and options.buildroot:
    cros_build_lib.Die('--buildroot is not used for remote tryjobs.')

@command.CommandDecorator('tryjob')
class TryjobCommand(command.CliCommand):
  """Schedule a tryjob."""

  EPILOG = """
Remote Examples:
  cros tryjob -g 123 lumpy-compile-only-pre-cq
  cros tryjob -g 123 -g 456 lumpy-compile-only-pre-cq daisy-pre-cq
  cros tryjob -g *123 --hwtest daisy-paladin
  cros tryjob -p chromiumos/chromite lumpy-compile-only-pre-cq
  cros tryjob -p chromiumos/chromite:foo_branch lumpy-paladin

Local Examples:
  cros tryjob --local -g 123 daisy-paladin
  cros tryjob --local --buildroot /my/cool/path -g 123 daisy-paladin

Production Examples (danger, can break production if misused):
  cros tryjob --production --branch release-R61-9765.B asuka-release
  cros tryjob --production --version 9795.0.0 --channel canary  lumpy-payloads

List Examples:
  cros tryjob --list
  cros tryjob --production --list
  cros tryjob --list lumpy
  cros tryjob --list lumpy vmtest
"""

  @classmethod
  def AddParser(cls, parser):
    """Adds a parser."""
    super(cls, TryjobCommand).AddParser(parser)
    parser.add_argument(
        'build_configs', nargs='*',
        help='One or more configs to build.')
    parser.add_argument(
        '-b', '--branch', default='master',
        help='The manifest branch to test.  The branch to '
             'check the buildroot out to.')
    parser.add_argument(
        '--profile', dest='passthrough', action='append_option_value',
        help='Name of profile to sub-specify board variant.')
    parser.add_argument(
        '--yes', action='store_true', default=False,
        help='Never prompt to confirm.')
    parser.add_argument(
        '--production', action='store_true', default=False,
        help='This is a production build, NOT a test build. '
             'Confirm with Chrome OS deputy before use.')
    parser.add_argument(
        '--pass-through', dest='passthrough_raw', action='append',
        help='Arguments to pass to cbuildbot. To be avoided.'
             'Confirm with Chrome OS deputy before use.')

    # Do we build locally, on on a trybot builder?
    where_group = parser.add_argument_group(
        'Where',
        description='Where do we run the tryjob?')
    where_ex = where_group.add_mutually_exclusive_group()
    where_ex.add_argument(
        '--remote', dest='where', action='store_const', const=REMOTE,
        default=REMOTE,
        help='Run the tryjob on a remote builder. (default)')
    where_ex.add_argument(
        '--local', dest='where', action='store_const', const=LOCAL,
        help='Run the tryjob on your local machine.')
    where_ex.add_argument(
        '--cbuildbot', dest='where', action='store_const', const=CBUILDBOT,
        help='Run special local build from current checkout in buildroot.')
    where_ex.add_argument(
        '--swarming', dest='where', action='store_const', const=SWARMING,
        help='Run the tryjob on a swarming builder (experimental)')
    where_group.add_argument(
        '-r', '--buildroot', type='path', dest='buildroot',
        help='Root directory to use for the local tryjob. '
             'NOT the current checkout.')

    # What patches do we include in the build?
    what_group = parser.add_argument_group(
        'Patch',
        description='Which patches should be included with the tryjob?')
    what_group.add_argument(
        '-g', '--gerrit-patches', action='split_extend', default=[],
        metavar='Id1 *int_Id2...IdN',
        help='Space-separated list of short-form Gerrit '
             "Change-Id's or change numbers to patch. "
             "Please prepend '*' to internal Change-Id's")
    # We have to format metavar poorly to workaround an argparse bug.
    # https://bugs.python.org/issue11874
    what_group.add_argument(
        '-p', '--local-patches', action='split_extend', default=[],
        metavar="'<project1>[:<branch1>] ... <projectN>[:<branchN>] '",
        help='Space-separated list of project branches with '
             'patches to apply.  Projects are specified by name. '
             'If no branch is specified the current branch of the '
             'project will be used.  NOTE: -p is known to be buggy; '
             'prefer using -g instead (see https://crbug.com/806963 '
             'and https://crbug.com/807834).')

    # Identifing the request.
    who_group = parser.add_argument_group(
        'Requestor',
        description='Who is submitting the jobs?')
    who_group.add_argument(
        '--committer-email',
        help='Override default git committer email.')
    who_group.add_argument(
        '--remote-description',
        help='Attach an optional description to a --remote run '
             'to make it easier to identify the results when it '
             'finishes')

    # Modify the build.
    how_group = parser.add_argument_group(
        'Modifiers',
        description='How do we modify build behavior?')
    how_group.add_argument(
        '--latest-toolchain', dest='passthrough', action='append_option',
        help='Use the latest toolchain.')
    how_group.add_argument(
        '--nochromesdk', dest='passthrough', action='append_option',
        help="Don't run the ChromeSDK stage which builds "
             'Chrome outside of the chroot.')
    how_group.add_argument(
        '--timeout', dest='passthrough', action='append_option_value',
        help='Specify the maximum amount of time this job '
             'can run for, at which point the build will be '
             'aborted.  If set to zero, then there is no '
             'timeout.')
    how_group.add_argument(
        '--sanity-check-build', dest='passthrough', action='append_option',
        help='Run the build as a sanity check build.')
    how_group.add_argument(
        '--chrome_version', dest='passthrough', action='append_option_value',
        help='Used with SPEC logic to force a particular '
             'git revision of chrome rather than the latest. '
             'HEAD is a valid value.')

    # Overrides for the build configs testing behaviors.
    test_group = parser.add_argument_group(
        'Testing Flags',
        description='How do we change testing behavior?')
    test_group.add_argument(
        '--hwtest', dest='passthrough', action='append_option',
        help='Enable hwlab testing. Default false.')
    test_group.add_argument(
        '--notests', dest='passthrough', action='append_option',
        help='Override values from buildconfig, run no '
             'tests, and build no autotest artifacts.')
    test_group.add_argument(
        '--novmtests', dest='passthrough', action='append_option',
        help='Override values from buildconfig, run no vmtests.')
    test_group.add_argument(
        '--noimagetests', dest='passthrough', action='append_option',
        help='Override values from buildconfig and run no image tests.')

    # <board>-payloads tryjob specific options.
    payloads_group = parser.add_argument_group(
        'Payloads',
        description='Options only used by payloads tryjobs.')
    payloads_group.add_argument(
        '--version', dest='passthrough', action='append_option_value',
        help='Specify the release version for payload regeneration. '
             'Ex: 9799.0.0')
    payloads_group.add_argument(
        '--channel', dest='passthrough', action='append_option_value',
        help='Specify a channel for a payloads trybot. Can '
             'be specified multiple times. No valid for '
             'non-payloads configs.')

    # branch_util tryjob specific options.
    branch_util_group = parser.add_argument_group(
        'branch_util',
        description='Options only used by branch-util tryjobs.')

    branch_util_group.add_argument(
        '--branch-name', dest='passthrough', action='append_option_value',
        help='The branch to create or delete.')
    branch_util_group.add_argument(
        '--delete-branch', dest='passthrough', action='append_option',
        help='Delete the branch specified in --branch-name.')
    branch_util_group.add_argument(
        '--rename-to', dest='passthrough', action='append_option_value',
        help='Rename a branch to the specified name.')
    branch_util_group.add_argument(
        '--force-create', dest='passthrough', action='append_option',
        help='Overwrites an existing branch.')
    branch_util_group.add_argument(
        '--skip-remote-push', dest='passthrough', action='append_option',
        help='Do not actually push to remote git repos.  '
             'Used for end-to-end testing branching.')

    configs_group = parser.add_argument_group(
        'Configs',
        description='Options for displaying available build configs.')
    configs_group.add_argument(
        '-l', '--list', action='store_true', dest='list', default=False,
        help='List the trybot configs (adjusted by --production).')

  def Run(self):
    """Runs `cros chroot`."""
    site_config = config_lib.GetConfig()

    AdjustOptions(self.options)
    self.options.Freeze()

    VerifyOptions(self.options, site_config)

    # Verify gerrit patches are valid.
    print('Verifying patches...')
    patch_pool = trybot_patch_pool.TrybotPatchPool.FromOptions(
        gerrit_patches=self.options.gerrit_patches,
        local_patches=self.options.local_patches,
        sourceroot=constants.SOURCE_ROOT,
        remote_patches=[])

    if self.options.where in (REMOTE, SWARMING):
      return RunRemote(site_config, self.options, patch_pool)
    elif self.options.where == LOCAL:
      return RunLocal(self.options)
    elif self.options.where == CBUILDBOT:
      return RunCbuildbot(self.options)
    else:
      raise Exception('Unknown options.where: %s', self.options.where)

# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Recipe module for Skia Swarming trigger.


import os
import json


DEPS = [
  'build/file',
  'build/gsutil',
  'builder_name_schema',
  'core',
  'depot_tools/depot_tools',
  'depot_tools/git',
  'depot_tools/tryserver',
  'recipe_engine/json',
  'recipe_engine/path',
  'recipe_engine/properties',
  'recipe_engine/python',
  'recipe_engine/raw_io',
  'recipe_engine/step',
  'recipe_engine/time',
  'run',
  'swarming',
  'vars',
]


TEST_BUILDERS = {
  'client.skia': {
    'skiabot-linux-swarm-000': [
      'Test-Ubuntu-GCC-ShuttleA-GPU-GTX550Ti-x86_64-Release-Valgrind',
      'Test-Ubuntu-Clang-GCE-CPU-AVX2-x86_64-Coverage-Trybot',
      'Build-Mac-Clang-x86_64-Release',
      'Build-Ubuntu-GCC-Arm64-Debug-Android_Vulkan',
      'Build-Ubuntu-GCC-x86_64-Debug',
      'Build-Ubuntu-GCC-x86_64-Debug-GN',
      'Build-Ubuntu-GCC-x86_64-Release-RemoteRun',
      'Build-Ubuntu-GCC-x86_64-Release-Trybot',
      'Build-Win-MSVC-x86_64-Release',
      'Build-Win-MSVC-x86_64-Release-Vulkan',
      'Housekeeper-PerCommit',
      'Housekeeper-Nightly-RecreateSKPs_Canary',
      'Infra-PerCommit',
      'Perf-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Release-Trybot',
      'Perf-Ubuntu-GCC-Golo-GPU-GT610-x86_64-Release-CT_BENCH_1k_SKPs',
      'Test-Android-GCC-Nexus7v2-GPU-Tegra3-Arm7-Release',
      'Test-Android-GCC-NVIDIA_Shield-GPU-TegraX1-Arm64-Debug-Vulkan',
      'Test-iOS-Clang-iPad4-GPU-SGX554-Arm7-Release',
      'Test-Mac-Clang-MacMini6.2-CPU-AVX-x86_64-Release',
      'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Debug',
      'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Debug-MSAN',
      'Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Release-Shared',
      'Test-Win8-MSVC-ShuttleA-GPU-HD7770-x86_64-Release',
      'Test-Win8-MSVC-ShuttleB-CPU-AVX2-x86_64-Release',
    ],
  },
}


def derive_compile_bot_name(api):
  builder_name = api.properties['buildername']
  builder_cfg = api.builder_name_schema.DictForBuilderName(builder_name)
  if builder_cfg['role'] == 'Housekeeper':
    return 'Build-Ubuntu-GCC-x86_64-Release-Shared'
  if builder_cfg['role'] in ('Test', 'Perf'):
    task_os = builder_cfg['os']
    extra_config = builder_cfg.get('extra_config')
    if task_os == 'Android':
      if extra_config == 'Vulkan':
        extra_config = '%s_%s' % (task_os, 'Vulkan')
      else:
        extra_config = task_os
      task_os = 'Ubuntu'
    elif task_os == 'iOS':
      extra_config = task_os
      task_os = 'Mac'
    elif 'Win' in task_os:
      task_os = 'Win'
    return api.builder_name_schema.MakeBuilderName(
        role=api.builder_name_schema.BUILDER_ROLE_BUILD,
        os=task_os,
        compiler=builder_cfg['compiler'],
        target_arch=builder_cfg['arch'],
        configuration=builder_cfg['configuration'],
        extra_config=extra_config,
        is_trybot=api.builder_name_schema.IsTrybot(builder_name))
  return builder_name


def swarm_dimensions(builder_cfg):
  """Return a dict of keys and values to be used as Swarming bot dimensions."""
  dimensions = {
    'pool': 'Skia',
  }
  dimensions['os'] = builder_cfg.get('os', 'Ubuntu')
  if builder_cfg.get('extra_config', '').startswith('CT_'):
    dimensions['pool'] = 'SkiaCT'
    return dimensions  # Do not need any more dimensions for CT builders.
  if 'Win' in builder_cfg.get('os', ''):
    dimensions['os'] = 'Windows'
  if builder_cfg['role'] in ('Test', 'Perf'):
    if 'Android' in builder_cfg['os']:
      # For Android, the device type is a better dimension than CPU or GPU.
      dimensions['device_type'] = {
        'AndroidOne':    'sprout',
        'GalaxyS3':      'm0',  #'smdk4x12', Detected incorrectly by swarming?
        'GalaxyS4':      None,  # TODO(borenet,kjlubick)
        'NVIDIA_Shield': 'foster',
        'Nexus10':       'manta',
        'Nexus5':        'hammerhead',
        'Nexus6':        'shamu',
        'Nexus7':        'grouper',
        'Nexus7v2':      'flo',
        'Nexus9':        'flounder',
        'NexusPlayer':   'fugu',
      }[builder_cfg['model']]
    elif 'iOS' in builder_cfg['os']:
      # For iOS, the device type is a better dimension than CPU or GPU.
      dimensions['device'] = {
        'iPad4': 'iPad4,1',
      }[builder_cfg['model']]
      # TODO(borenet): Replace this hack with something better.
      dimensions['os'] = 'iOS-9.2'
    elif builder_cfg['cpu_or_gpu'] == 'CPU':
      dimensions['gpu'] = 'none'
      dimensions['cpu'] = {
        'AVX':  'x86-64',
        'AVX2': 'x86-64-avx2',
        'SSE4': 'x86-64',
      }[builder_cfg['cpu_or_gpu_value']]
      if ('Win' in builder_cfg['os'] and
          builder_cfg['cpu_or_gpu_value'] == 'AVX2'):
        # AVX2 is not correctly detected on Windows. Fall back on other
        # dimensions to ensure that we correctly target machines which we know
        # have AVX2 support.
        dimensions['cpu'] = 'x86-64'
        dimensions['os'] = 'Windows-2008ServerR2-SP1'
    else:
      dimensions['gpu'] = {
        'GeForce320M': '10de:08a4',
        'GT610':       '10de:104a',
        'GTX550Ti':    '10de:1244',
        'GTX660':      '10de:11c0',
        'GTX960':      '10de:1401',
        'HD4000':      '8086:0a2e',
        'HD4600':      '8086:0412',
        'HD7770':      '1002:683d',
      }[builder_cfg['cpu_or_gpu_value']]
  else:
    dimensions['gpu'] = 'none'
  return dimensions


def fix_filemodes(api, path):
  """Set all filemodes to 644 or 755 in the given directory path."""
  api.python.inline(
      name='fix filemodes',
      program='''import os
for r, _, files in os.walk(os.getcwd()):
  for fname in files:
    f = os.path.join(r, fname)
    if os.path.isfile(f):
      if os.access(f, os.X_OK):
        os.chmod(f, 0755)
      else:
        os.chmod(f, 0644)
''',
      cwd=path)


def trigger_task(api, task_name, builder, master, slave, buildnumber,
                 builder_cfg, got_revision, infrabots_dir, idempotent=False,
                 store_output=True, extra_isolate_hashes=None, expiration=None,
                 hard_timeout=None, io_timeout=None, cipd_packages=None):
  """Trigger the given bot to run as a Swarming task."""
  # TODO(borenet): We're using Swarming directly to run the recipe through
  # recipes.py. Once it's possible to track the state of a Buildbucket build,
  # we should switch to use the trigger recipe module instead.

  properties = {
    'buildername': builder,
    'mastername': master,
    'buildnumber': buildnumber,
    'reason': 'Triggered by Skia swarm_trigger Recipe',
    'revision': got_revision,
    'slavename': slave,
    'swarm_out_dir': '${ISOLATED_OUTDIR}',
  }
  if builder_cfg['is_trybot']:
    if api.properties.get('patch_storage') == 'gerrit':
      properties['patch_storage'] = api.properties['patch_storage']
      properties['repository'] = api.properties['repository']
      properties['event.patchSet.ref'] = api.properties['event.patchSet.ref']
      properties['event.change.number'] = api.properties['event.change.number']
    else:
      properties['issue'] = str(api.properties['issue'])
      properties['patchset'] = str(api.properties['patchset'])
      properties['rietveld'] = api.properties['rietveld']

  extra_args = [
      '--workdir', '../../..',
      'swarm_%s' % task_name,
  ]
  for k, v in properties.iteritems():
    extra_args.append('%s=%s' % (k, v))

  isolate_base_dir = api.path['slave_build']
  dimensions = swarm_dimensions(builder_cfg)
  isolate_blacklist = ['.git', 'out', '*.pyc', '.recipe_deps']
  isolate_vars = {
    'WORKDIR': api.path['slave_build'],
  }

  isolate_file = '%s_skia.isolate' % task_name
  if 'Coverage' == builder_cfg.get('configuration'):
    isolate_file = 'coverage_skia.isolate'
  if 'RecreateSKPs' in builder:
    isolate_file = 'compile_skia.isolate'
  return api.swarming.isolate_and_trigger_task(
      infrabots_dir.join(isolate_file),
      isolate_base_dir,
      '%s_skia' % task_name,
      isolate_vars,
      dimensions,
      isolate_blacklist=isolate_blacklist,
      extra_isolate_hashes=extra_isolate_hashes,
      idempotent=idempotent,
      store_output=store_output,
      extra_args=extra_args,
      expiration=expiration,
      hard_timeout=hard_timeout,
      io_timeout=io_timeout,
      cipd_packages=cipd_packages)


def checkout_steps(api):
  """Run the steps to obtain a checkout of Skia."""
  # In this case, we're already running inside a checkout of Skia, so just
  # report the currently-checked-out commit.
  checkout_path = api.path['slave_build'].join('skia')
  got_revision = api.git(
      'rev-parse', 'HEAD', cwd=checkout_path,
      stdout=api.raw_io.output(),
      step_test_data=lambda: api.raw_io.test_api.stream_output('abc123\n'),
  ).stdout.rstrip()
  cmd = ['python', '-c', '"print \'%s\'"' % got_revision]
  res = api.step('got_revision', cmd=cmd)
  res.presentation.properties['got_revision'] = got_revision
  api.path['checkout'] = checkout_path

  # Write a fake .gclient file if none exists. This is required by .isolates.
  dot_gclient = api.path['slave_build'].join('.gclient')
  if not api.path.exists(dot_gclient):
    api.run.writefile(dot_gclient, '')

  fix_filemodes(api, api.path['checkout'])
  return got_revision


def housekeeper_swarm(api, builder_cfg, got_revision, infrabots_dir,
                      extra_isolate_hashes):
  task = trigger_task(
      api,
      'housekeeper',
      api.properties['buildername'],
      api.properties['mastername'],
      api.properties['slavename'],
      api.properties['buildnumber'],
      builder_cfg,
      got_revision,
      infrabots_dir,
      idempotent=False,
      store_output=False,
      extra_isolate_hashes=extra_isolate_hashes,
      cipd_packages=[cipd_pkg(api, infrabots_dir, 'go')],
      )
  return api.swarming.collect_swarming_task(task)


def recreate_skps_swarm(api, builder_cfg, got_revision, infrabots_dir,
                        extra_isolate_hashes):
  task = trigger_task(
      api,
      'RecreateSKPs',
      api.properties['buildername'],
      api.properties['mastername'],
      api.properties['slavename'],
      api.properties['buildnumber'],
      builder_cfg,
      got_revision,
      infrabots_dir,
      idempotent=False,
      store_output=False,
      extra_isolate_hashes=extra_isolate_hashes)
  return api.swarming.collect_swarming_task(task)


def ct_skps_swarm(api, builder_cfg, got_revision, infrabots_dir,
                  extra_isolate_hashes):
  expiration, hard_timeout, io_timeout = get_timeouts(builder_cfg)
  task = trigger_task(
      api,
      'ct_skps',
      api.properties['buildername'],
      api.properties['mastername'],
      api.properties['slavename'],
      api.properties['buildnumber'],
      builder_cfg,
      got_revision,
      infrabots_dir,
      idempotent=False,
      store_output=False,
      extra_isolate_hashes=extra_isolate_hashes,
      expiration=expiration,
      hard_timeout=hard_timeout,
      io_timeout=io_timeout)
  return api.swarming.collect_swarming_task(task)


def infra_swarm(api, got_revision, infrabots_dir, extra_isolate_hashes):
  # Fake the builder cfg.
  builder_cfg = {
    'role': 'Infra',
    'is_trybot': api.builder_name_schema.IsTrybot(
         api.properties['buildername'])
  }
  task = trigger_task(
      api,
      'infra',
      api.properties['buildername'],
      api.properties['mastername'],
      api.properties['slavename'],
      api.properties['buildnumber'],
      builder_cfg,
      got_revision,
      infrabots_dir,
      idempotent=False,
      store_output=False,
      extra_isolate_hashes=extra_isolate_hashes)
  return api.swarming.collect_swarming_task(task)


def compile_steps_swarm(api, builder_cfg, got_revision, infrabots_dir):
  builder_name = derive_compile_bot_name(api)
  compile_builder_cfg = api.builder_name_schema.DictForBuilderName(builder_name)

  cipd_packages = []

  # Android bots require a toolchain.
  if 'Android' in api.properties['buildername']:
    cipd_packages.append(cipd_pkg(api, infrabots_dir, 'android_sdk'))

  # Windows bots require a toolchain.
  if 'Win' in builder_name:
    version_file = infrabots_dir.join('assets', 'win_toolchain', 'VERSION')
    version = api.run.readfile(version_file,
                               name='read win_toolchain VERSION',
                               test_data='0').rstrip()
    version = 'version:%s' % version
    pkg = ('t', 'skia/bots/win_toolchain', version)
    cipd_packages.append(pkg)

    if 'Vulkan' in builder_name:
      cipd_packages.append(cipd_pkg(api, infrabots_dir, 'win_vulkan_sdk'))

  # Fake these properties for compile tasks so that they can be de-duped.
  master = 'client.skia.compile'
  slave = 'skiabot-dummy-compile-slave'
  buildnumber = 1

  task = trigger_task(
      api,
      'compile',
      builder_name,
      master,
      slave,
      buildnumber,
      compile_builder_cfg,
      got_revision,
      infrabots_dir,
      idempotent=True,
      store_output=False,
      cipd_packages=cipd_packages)

  # Wait for compile to finish, record the results hash.
  return api.swarming.collect_swarming_task_isolate_hash(task)


def get_timeouts(builder_cfg):
  """Some builders require longer than the default timeouts.

  Returns tuple of (expiration, hard_timeout, io_timeout). If those values are
  none then default timeouts should be used.
  """
  expiration = None
  hard_timeout = None
  io_timeout = None
  if 'Valgrind' in builder_cfg.get('extra_config', ''):
    expiration = 2*24*60*60
    hard_timeout = 9*60*60
    io_timeout = 60*60
  if builder_cfg.get('extra_config', '').startswith('CT_'):
    hard_timeout = 24*60*60
    io_timeout = 60*60
  return expiration, hard_timeout, io_timeout


def gsutil_env(api, boto_file):
  """Environment variables for gsutil."""
  home_dir = os.path.expanduser('~')
  if api.path._test_data.enabled:
    home_dir = '[HOME]'

  boto_path = None
  if boto_file:
    boto_path = api.path.join(home_dir, boto_file)
  return {'AWS_CREDENTIAL_FILE': boto_path,
          'BOTO_CONFIG': boto_path}


def get_issue_num(api):
  if api.properties.get('patch_storage') == 'gerrit':
    return str(api.properties['event.change.number'])
  else:
    return str(api.properties['issue'])


def perf_steps_trigger(api, builder_cfg, got_revision, infrabots_dir,
                       extra_hashes, cipd_packages):
  """Trigger perf tests via Swarming."""

  expiration, hard_timeout, io_timeout = get_timeouts(builder_cfg)
  return trigger_task(
      api,
      'perf',
      api.properties['buildername'],
      api.properties['mastername'],
      api.properties['slavename'],
      api.properties['buildnumber'],
      builder_cfg,
      got_revision,
      infrabots_dir,
      extra_isolate_hashes=extra_hashes,
      expiration=expiration,
      hard_timeout=hard_timeout,
      io_timeout=io_timeout,
      cipd_packages=cipd_packages)


def perf_steps_collect(api, task, got_revision, is_trybot):
  """Wait for perf steps to finish and upload results."""
  # Wait for nanobench to finish, download the results.
  api.run.rmtree(task.task_output_dir)
  api.swarming.collect_swarming_task(task)

  # Upload the results.
  if api.vars.upload_perf_results:
    perf_data_dir = api.path['slave_build'].join(
        'perfdata', api.properties['buildername'], 'data')
    git_timestamp = api.git.get_timestamp(test_data='1408633190',
                                          infra_step=True)
    api.run.rmtree(perf_data_dir)
    api.file.makedirs('perf_dir', perf_data_dir, infra_step=True)
    src_results_file = task.task_output_dir.join(
        '0', 'perfdata', api.properties['buildername'], 'data',
        'nanobench_%s.json' % got_revision)
    dst_results_file = perf_data_dir.join(
        'nanobench_%s_%s.json' % (got_revision, git_timestamp))
    api.file.copy('perf_results', src_results_file, dst_results_file,
                  infra_step=True)

    gsutil_path = api.path['slave_build'].join(
        'skia', 'infra', 'bots', '.recipe_deps', 'depot_tools', 'third_party',
        'gsutil', 'gsutil')
    upload_args = [api.properties['buildername'], api.properties['buildnumber'],
                   perf_data_dir, got_revision, gsutil_path]
    if is_trybot:
      upload_args.append(get_issue_num(api))
    api.python(
        'Upload perf results',
        script=api.core.resource('upload_bench_results.py'),
        args=upload_args,
        cwd=api.path['checkout'],
        infra_step=True)


def test_steps_trigger(api, builder_cfg, got_revision, infrabots_dir,
                       extra_hashes, cipd_packages):
  """Trigger DM via Swarming."""
  expiration, hard_timeout, io_timeout = get_timeouts(builder_cfg)
  return trigger_task(
      api,
      'test',
      api.properties['buildername'],
      api.properties['mastername'],
      api.properties['slavename'],
      api.properties['buildnumber'],
      builder_cfg,
      got_revision,
      infrabots_dir,
      extra_isolate_hashes=extra_hashes,
      expiration=expiration,
      hard_timeout=hard_timeout,
      io_timeout=io_timeout,
      cipd_packages=cipd_packages)


def test_steps_collect(api, task, got_revision, is_trybot, builder_cfg):
  """Collect the test results from Swarming."""
  # Wait for tests to finish, download the results.
  api.run.rmtree(task.task_output_dir)
  api.swarming.collect_swarming_task(task)

  # Upload the results.
  if api.vars.upload_dm_results:
    dm_dir = api.path['slave_build'].join('dm')
    dm_src = task.task_output_dir.join('0', 'dm')
    api.run.rmtree(dm_dir)
    api.file.copytree('dm_dir', dm_src, dm_dir, infra_step=True)

    # Upload them to Google Storage.
    api.python(
        'Upload DM Results',
        script=api.core.resource('upload_dm_results.py'),
        args=[
          dm_dir,
          got_revision,
          api.properties['buildername'],
          api.properties['buildnumber'],
          get_issue_num(api) if is_trybot else '',
          api.path['slave_build'].join('skia', 'common', 'py', 'utils'),
        ],
        cwd=api.path['checkout'],
        env=gsutil_env(api, 'chromium-skia-gm.boto'),
        infra_step=True)

  if builder_cfg['configuration']  == 'Coverage':
    upload_coverage_results(api, task, got_revision, is_trybot)


def upload_coverage_results(api, task, got_revision, is_trybot):
  results_dir = task.task_output_dir.join('0')
  git_timestamp = api.git.get_timestamp(test_data='1408633190',
                                        infra_step=True)

  # Upload raw coverage data.
  cov_file_basename = '%s.cov' % got_revision
  cov_file = results_dir.join(cov_file_basename)
  now = api.time.utcnow()
  gs_json_path = '/'.join((
      str(now.year).zfill(4), str(now.month).zfill(2),
      str(now.day).zfill(2), str(now.hour).zfill(2),
      api.properties['buildername'],
      str(api.properties['buildnumber'])))
  if is_trybot:
    gs_json_path = '/'.join(('trybot', gs_json_path, get_issue_num(api)))
  api.gsutil.upload(
      name='upload raw coverage data',
      source=cov_file,
      bucket='skia-infra',
      dest='/'.join(('coverage-raw-v1', gs_json_path,
                     cov_file_basename)),
      env={'AWS_CREDENTIAL_FILE': None, 'BOTO_CONFIG': None},
  )

  # Transform the nanobench_${git_hash}.json file received from swarming bot
  # into the nanobench_${git_hash}_${timestamp}.json file
  # upload_bench_results.py expects.
  src_nano_file = results_dir.join('nanobench_%s.json' % got_revision)
  dst_nano_file = results_dir.join(
      'nanobench_%s_%s.json' % (got_revision, git_timestamp))
  api.file.copy('nanobench JSON', src_nano_file, dst_nano_file,
                infra_step=True)
  api.file.remove('old nanobench JSON', src_nano_file)

  # Upload nanobench JSON data.
  gsutil_path = api.depot_tools.gsutil_py_path
  upload_args = [api.properties['buildername'], api.properties['buildnumber'],
                 results_dir, got_revision, gsutil_path]
  if is_trybot:
    upload_args.append(get_issue_num(api))
  api.python(
      'upload nanobench coverage results',
      script=api.core.resource('upload_bench_results.py'),
      args=upload_args,
      cwd=api.path['checkout'],
      env=gsutil_env(api, 'chromium-skia-gm.boto'),
      infra_step=True)

  # Transform the coverage_by_line_${git_hash}.json file received from
  # swarming bot into a coverage_by_line_${git_hash}_${timestamp}.json file.
  src_lbl_file = results_dir.join('coverage_by_line_%s.json' % got_revision)
  dst_lbl_file_basename = 'coverage_by_line_%s_%s.json' % (
      got_revision, git_timestamp)
  dst_lbl_file = results_dir.join(dst_lbl_file_basename)
  api.file.copy('Line-by-line coverage JSON', src_lbl_file, dst_lbl_file,
                infra_step=True)
  api.file.remove('old line-by-line coverage JSON', src_lbl_file)

  # Upload line-by-line coverage data.
  api.gsutil.upload(
      name='upload line-by-line coverage data',
      source=dst_lbl_file,
      bucket='skia-infra',
      dest='/'.join(('coverage-json-v1', gs_json_path,
                     dst_lbl_file_basename)),
      env={'AWS_CREDENTIAL_FILE': None, 'BOTO_CONFIG': None},
  )


def cipd_pkg(api, infrabots_dir, asset_name):
  """Find and return the CIPD package info for the given asset."""
  version_file = infrabots_dir.join('assets', asset_name, 'VERSION')
  version = api.run.readfile(version_file,
                               name='read %s VERSION' % asset_name,
                               test_data='0').rstrip()
  version = 'version:%s' % version
  return (asset_name, 'skia/bots/%s' % asset_name, version)


def print_properties(api):
  """Dump out all properties for debugging purposes."""
  props = {}
  for k, v in api.properties.iteritems():
    props[k] = v
  api.python.inline(
      'print properties',
      '''
import json
import sys

with open(sys.argv[1]) as f:
  content = json.load(f)

print json.dumps(content, indent=2)
''',
      args=[api.json.input(props)])


def RunSteps(api):
  # TODO(borenet): Remove this once SwarmBucket is working.
  print_properties(api)

  got_revision = checkout_steps(api)
  infrabots_dir = api.path['checkout'].join('infra', 'bots')
  api.swarming.setup(
      infrabots_dir.join('tools', 'luci-go'),
      swarming_rev='')

  # Run gsutil.py to ensure that it's installed.
  api.gsutil(['help'])

  extra_hashes = []

  # Get ready to compile.
  infrabots_dir = api.path['checkout'].join('infra', 'bots')
  if 'Infra' in api.properties['buildername']:
    return infra_swarm(api, got_revision, infrabots_dir, extra_hashes)

  builder_cfg = api.builder_name_schema.DictForBuilderName(
      api.properties['buildername'])

  if 'RecreateSKPs' in api.properties['buildername']:
    recreate_skps_swarm(api, builder_cfg, got_revision, infrabots_dir,
                        extra_hashes)
    return

  if '-CT_' in api.properties['buildername']:
    ct_skps_swarm(api, builder_cfg, got_revision, infrabots_dir, extra_hashes)
    return

  # Compile.
  do_compile_steps = True
  if 'Coverage' in api.properties['buildername']:
    do_compile_steps = False
  if do_compile_steps:
    extra_hashes.append(compile_steps_swarm(
        api, builder_cfg, got_revision, infrabots_dir))

  if builder_cfg['role'] == 'Housekeeper':
    housekeeper_swarm(api, builder_cfg, got_revision, infrabots_dir,
                      extra_hashes)
    return

  # Get ready to test/perf.

  # CIPD packages needed by test/perf.
  cipd_packages = []

  do_test_steps = (
      builder_cfg['role'] == api.builder_name_schema.BUILDER_ROLE_TEST)
  do_perf_steps = (
      builder_cfg['role'] == api.builder_name_schema.BUILDER_ROLE_PERF)

  if not (do_test_steps or do_perf_steps):
    return

  # SKPs, SkImages, SVGs.
  cipd_packages.append(cipd_pkg(api, infrabots_dir, 'skp'))
  cipd_packages.append(cipd_pkg(api, infrabots_dir, 'skimage'))
  cipd_packages.append(cipd_pkg(api, infrabots_dir, 'svg'))

  # Trigger test and perf tasks.
  test_task = None
  perf_task = None
  if do_test_steps:
    test_task = test_steps_trigger(api, builder_cfg, got_revision,
                                   infrabots_dir, extra_hashes, cipd_packages)
  if do_perf_steps:
    perf_task = perf_steps_trigger(api, builder_cfg, got_revision,
                                   infrabots_dir, extra_hashes, cipd_packages)
  is_trybot = builder_cfg['is_trybot']

  # Wait for results, then upload them if necessary.

  if test_task:
    test_steps_collect(api, test_task,
                       got_revision, is_trybot, builder_cfg)

  if perf_task:
    perf_steps_collect(api, perf_task,
                       got_revision, is_trybot)


def test_for_bot(api, builder, mastername, slavename, testname=None):
  """Generate a test for the given bot."""
  testname = testname or builder
  test = (
    api.test(testname) +
    api.properties(buildername=builder,
                   mastername=mastername,
                   slavename=slavename,
                   buildnumber=5,
                   path_config='kitchen',
                   revision='abc123')
  )
  paths = [
      api.path['slave_build'].join('skia'),
      api.path['slave_build'].join('tmp', 'uninteresting_hashes.txt'),
  ]
  if 'Trybot' in builder:
    test += api.properties(issue=500,
                           patchset=1,
                           rietveld='https://codereview.chromium.org')
  if 'Android' in builder:
    paths.append(api.path['slave_build'].join(
        'skia', 'infra', 'bots', 'assets', 'android_sdk', 'VERSION'))
  if 'Test' in builder and 'Coverage' not in builder:
    test += api.step_data(
        'upload new .isolated file for test_skia',
        stdout=api.raw_io.output('def456 XYZ.isolated'))
  if 'Perf' in builder and '-CT_' not in builder:
    test += api.step_data(
        'upload new .isolated file for perf_skia',
        stdout=api.raw_io.output('def456 XYZ.isolated'))
  if 'Housekeeper' in builder and 'RecreateSKPs' not in builder:
    test += api.step_data(
        'upload new .isolated file for housekeeper_skia',
        stdout=api.raw_io.output('def456 XYZ.isolated'))
  if 'Win' in builder:
    paths.append(api.path['slave_build'].join(
        'skia', 'infra', 'bots', 'assets', 'win_toolchain', 'VERSION'))
    paths.append(api.path['slave_build'].join(
        'skia', 'infra', 'bots', 'assets', 'win_vulkan_sdk', 'VERSION'))
  paths.append(api.path['slave_build'].join(
      'skia', 'infra', 'bots', 'assets', 'skimage', 'VERSION'))
  paths.append(api.path['slave_build'].join(
      'skia', 'infra', 'bots', 'assets', 'skp', 'VERSION'))
  paths.append(api.path['slave_build'].join(
      'skia', 'infra', 'bots', 'assets', 'svg', 'VERSION'))

  test += api.path.exists(*paths)

  return test


def GenTests(api):
  for mastername, slaves in TEST_BUILDERS.iteritems():
    for slavename, builders_by_slave in slaves.iteritems():
      for builder in builders_by_slave:
        yield test_for_bot(api, builder, mastername, slavename)

  gerrit_kwargs = {
    'patch_storage': 'gerrit',
    'repository': 'skia',
    'event.patchSet.ref': 'refs/changes/00/2100/2',
    'event.change.number': '2100',
  }
  yield (
      api.test('recipe_with_gerrit_patch') +
      api.properties(
          buildername='Test-Ubuntu-GCC-GCE-CPU-AVX2-x86_64-Debug-Trybot',
          mastername='client.skia',
          slavename='skiabot-linux-swarm-000',
          buildnumber=5,
          path_config='kitchen',
          revision='abc123',
          **gerrit_kwargs) +
      api.step_data(
          'upload new .isolated file for test_skia',
          stdout=api.raw_io.output('def456 XYZ.isolated'))
  )

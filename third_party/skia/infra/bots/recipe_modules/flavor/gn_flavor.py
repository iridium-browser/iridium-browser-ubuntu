# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import default_flavor

"""GN flavor utils, used for building Skia with GN."""
class GNFlavorUtils(default_flavor.DefaultFlavorUtils):
  def _run(self, title, cmd, infra_step=False, **kwargs):
    return self.m.run(self.m.step, title, cmd=cmd,
               infra_step=infra_step, **kwargs)

  def _py(self, title, script, infra_step=True, args=()):
    return self.m.run(self.m.python, title, script=script, args=args,
               infra_step=infra_step)

  def build_command_buffer(self):
    self.m.run(self.m.python, 'build command_buffer',
        script=self.m.vars.skia_dir.join('tools', 'build_command_buffer.py'),
        args=[
          '--chrome-dir', self.m.vars.checkout_root,
          '--output-dir', self.m.vars.skia_out.join(self.m.vars.configuration),
          '--no-sync', '--no-hooks', '--make-output-dir'])

  def compile(self, unused_target):
    """Build Skia with GN."""
    compiler      = self.m.vars.builder_cfg.get('compiler',      '')
    configuration = self.m.vars.builder_cfg.get('configuration', '')
    extra_tokens  = self.m.vars.extra_tokens
    os            = self.m.vars.builder_cfg.get('os',            '')
    target_arch   = self.m.vars.builder_cfg.get('target_arch',   '')

    clang_linux        = str(self.m.vars.slave_dir.join('clang_linux'))
    emscripten_sdk     = str(self.m.vars.slave_dir.join('emscripten_sdk'))
    linux_vulkan_sdk   = str(self.m.vars.slave_dir.join('linux_vulkan_sdk'))
    win_toolchain = str(self.m.vars.slave_dir.join(
      't', 'depot_tools', 'win_toolchain', 'vs_files',
      'a9e1098bba66d2acccc377d5ee81265910f29272'))
    win_vulkan_sdk = str(self.m.vars.slave_dir.join('win_vulkan_sdk'))

    cc, cxx = None, None
    extra_cflags = []
    extra_ldflags = []
    args = {}
    env = {}

    if compiler == 'Clang' and self.m.vars.is_linux:
      cc  = clang_linux + '/bin/clang'
      cxx = clang_linux + '/bin/clang++'
      extra_cflags .append('-B%s/bin' % clang_linux)
      extra_ldflags.append('-B%s/bin' % clang_linux)
      extra_ldflags.append('-fuse-ld=lld')
      extra_cflags.append('-DDUMMY_clang_linux_version=%s' %
                          self.m.run.asset_version('clang_linux'))
      if os == 'Ubuntu14':
        extra_ldflags.extend(['-static-libstdc++', '-static-libgcc'])

    elif compiler == 'Clang':
      cc, cxx = 'clang', 'clang++'
    elif compiler == 'GCC':
      if target_arch in ['mips64el', 'loongson3a']:
        mips64el_toolchain_linux = str(self.m.vars.slave_dir.join(
            'mips64el_toolchain_linux'))
        cc  = mips64el_toolchain_linux + '/bin/mips64el-linux-gnuabi64-gcc-7'
        cxx = mips64el_toolchain_linux + '/bin/mips64el-linux-gnuabi64-g++-7'
        env['LD_LIBRARY_PATH'] = (
            mips64el_toolchain_linux + '/lib/x86_64-linux-gnu/')
        extra_ldflags.append('-L' + mips64el_toolchain_linux +
                             '/mips64el-linux-gnuabi64/lib')
        extra_cflags.extend([
            '-Wno-format-truncation',
            '-Wno-uninitialized',
            ('-DDUMMY_mips64el_toolchain_linux_version=%s' %
             self.m.run.asset_version('mips64el_toolchain_linux'))
        ])
        if configuration == 'Release':
          # This warning is only triggered when fuzz_canvas is inlined.
          extra_cflags.append('-Wno-strict-overflow')
        args.update({
          'skia_use_system_freetype2': 'false',
          'skia_use_fontconfig':       'false',
          'skia_enable_gpu':           'false',
        })
      else:
        cc, cxx = 'gcc', 'g++'
    elif compiler == 'EMCC':
      cc   = emscripten_sdk + '/emscripten/incoming/emcc'
      cxx  = emscripten_sdk + '/emscripten/incoming/em++'
      extra_cflags.append('-Wno-unknown-warning-option')
      extra_cflags.append('-DDUMMY_emscripten_sdk_version=%s' %
                          self.m.run.asset_version('emscripten_sdk'))

    if 'Coverage' in extra_tokens:
      # See https://clang.llvm.org/docs/SourceBasedCodeCoverage.html for
      # more info on using llvm to gather coverage information.
      extra_cflags.append('-fprofile-instr-generate')
      extra_cflags.append('-fcoverage-mapping')
      extra_ldflags.append('-fprofile-instr-generate')
      extra_ldflags.append('-fcoverage-mapping')

    if compiler != 'MSVC' and configuration == 'Debug':
      extra_cflags.append('-O1')

    if 'Exceptions' in extra_tokens:
      extra_cflags.append('/EHsc')
    if 'Fast' in extra_tokens:
      extra_cflags.extend(['-march=native', '-fomit-frame-pointer', '-O3',
                           '-ffp-contract=off'])

    if len(extra_tokens) == 1 and extra_tokens[0].startswith('SK'):
      extra_cflags.append('-D' + extra_tokens[0])
      # If we're limiting Skia at all, drop skcms to portable code.
      if 'SK_CPU_LIMIT' in extra_tokens[0]:
        extra_cflags.append('-DSKCMS_PORTABLE')


    if 'MSAN' in extra_tokens:
      extra_ldflags.append('-L' + clang_linux + '/msan')

    if configuration != 'Debug':
      args['is_debug'] = 'false'
    if 'ANGLE' in extra_tokens:
      args['skia_use_angle'] = 'true'
    if 'CommandBuffer' in extra_tokens:
      self.m.run.run_once(self.build_command_buffer)
    if 'MSAN' in extra_tokens:
      args['skia_enable_gpu']     = 'false'
      args['skia_use_fontconfig'] = 'false'
    if 'ASAN' in extra_tokens or 'UBSAN' in extra_tokens:
      args['skia_enable_spirv_validation'] = 'false'
    if 'Mini' in extra_tokens:
      args.update({
        'is_component_build':     'true',   # Proves we can link a coherent .so.
        'is_official_build':      'true',   # No debug symbols, no tools.
        'skia_enable_effects':    'false',
        'skia_enable_gpu':        'true',
        'skia_enable_pdf':        'false',
        'skia_use_expat':         'false',
        'skia_use_libjpeg_turbo': 'false',
        'skia_use_libpng':        'false',
        'skia_use_libwebp':       'false',
        'skia_use_zlib':          'false',
      })
    if 'NoDEPS' in extra_tokens:
      args.update({
        'is_official_build':         'true',
        'skia_enable_fontmgr_empty': 'true',
        'skia_enable_gpu':           'true',

        'skia_enable_effects':    'false',
        'skia_enable_pdf':        'false',
        'skia_use_expat':         'false',
        'skia_use_freetype':      'false',
        'skia_use_libjpeg_turbo': 'false',
        'skia_use_libpng':        'false',
        'skia_use_libwebp':       'false',
        'skia_use_vulkan':        'false',
        'skia_use_zlib':          'false',
      })
    if 'NoGPU' in extra_tokens:
      args['skia_enable_gpu'] = 'false'
    if 'EmbededResouces' in extra_tokens:
      args['skia_embed_resoucres'] = 'true'
    if 'Shared' in extra_tokens:
      args['is_component_build'] = 'true'
    if 'Vulkan' in extra_tokens and not 'Android' in extra_tokens:
      args['skia_enable_vulkan_debug_layers'] = 'false'
      if self.m.vars.is_linux:
        args['skia_vulkan_sdk'] = '"%s"' % linux_vulkan_sdk
      if 'Win' in os:
        args['skia_vulkan_sdk'] = '"%s"' % win_vulkan_sdk
    if 'Metal' in extra_tokens:
      args['skia_use_metal'] = 'true'
    if 'iOS' in extra_tokens:
      # Bots use Chromium signing cert.
      args['skia_ios_identity'] = '".*GS9WA.*"'
      args['skia_ios_profile'] = '"Upstream Testing Provisioning Profile"'
    if 'CheckGeneratedFiles' in extra_tokens:
      args['skia_compile_processors'] = 'true'
    if compiler == 'Clang' and 'Win' in os:
      args['clang_win'] = '"%s"' % self.m.vars.slave_dir.join('clang_win')
      extra_cflags.append('-DDUMMY_clang_win_version=%s' %
                          self.m.run.asset_version('clang_win'))
    if target_arch == 'wasm':
      args.update({
        'skia_use_freetype':   'false',
        'skia_use_fontconfig': 'false',
        'skia_use_dng_sdk':    'false',
        'skia_use_icu':        'false',
        'skia_enable_gpu':     'false',
      })

    sanitize = ''
    for t in extra_tokens:
      if t.endswith('SAN'):
        sanitize = t
    if 'SafeStack' in extra_tokens:
      assert sanitize == ''
      sanitize = 'safe-stack'

    for (k,v) in {
      'cc':  cc,
      'cxx': cxx,
      'sanitize': sanitize,
      'target_cpu': target_arch,
      'target_os': 'ios' if 'iOS' in extra_tokens else '',
      'win_sdk': win_toolchain + '/win_sdk' if 'Win' in os else '',
      'win_vc': win_toolchain + '/VC' if 'Win' in os else '',
    }.iteritems():
      if v:
        args[k] = '"%s"' % v
    if extra_cflags:
      args['extra_cflags'] = repr(extra_cflags).replace("'", '"')
    if extra_ldflags:
      args['extra_ldflags'] = repr(extra_ldflags).replace("'", '"')

    gn_args = ' '.join('%s=%s' % (k,v) for (k,v) in sorted(args.iteritems()))

    gn    = 'gn.exe'    if 'Win' in os else 'gn'
    ninja = 'ninja.exe' if 'Win' in os else 'ninja'
    gn = self.m.vars.skia_dir.join('bin', gn)

    with self.m.context(cwd=self.m.vars.skia_dir):
      self._py('fetch-gn', self.m.vars.skia_dir.join('bin', 'fetch-gn'))
      if 'CheckGeneratedFiles' in extra_tokens:
        env['PATH'] = '%s:%%(PATH)s' % self.m.vars.skia_dir.join('bin')
        self._py(
            'fetch-clang-format',
            self.m.vars.skia_dir.join('bin', 'fetch-clang-format'))
      if target_arch == 'wasm':
        fastcomp = emscripten_sdk + '/clang/fastcomp/build_incoming_64/bin'
        env['PATH'] = '%s:%%(PATH)s' % fastcomp

      with self.m.env(env):
        self._run('gn gen', [gn, 'gen', self.out_dir, '--args=' + gn_args])
        self._run('ninja', [ninja, '-k', '0', '-C', self.out_dir])

  def step(self, name, cmd):
    app = self.m.vars.skia_out.join(self.m.vars.configuration, cmd[0])
    cmd = [app] + cmd[1:]
    env = self.m.context.env
    path = []
    ld_library_path = []

    slave_dir = self.m.vars.slave_dir
    clang_linux = str(slave_dir.join('clang_linux'))
    extra_tokens = self.m.vars.extra_tokens

    if self.m.vars.is_linux:
      if (self.m.vars.builder_cfg.get('cpu_or_gpu', '') == 'GPU'
          and 'Intel' in self.m.vars.builder_cfg.get('cpu_or_gpu_value', '')):
        # The vulkan in this asset name simply means that the graphics driver
        # supports Vulkan. It is also the driver used for GL code.
        dri_path = slave_dir.join('linux_vulkan_intel_driver_release')
        if self.m.vars.builder_cfg.get('configuration', '') == 'Debug':
          dri_path = slave_dir.join('linux_vulkan_intel_driver_debug')
        ld_library_path.append(dri_path)
        env['LIBGL_DRIVERS_PATH'] = str(dri_path)
        env['VK_ICD_FILENAMES'] = str(dri_path.join('intel_icd.x86_64.json'))

      if 'Vulkan' in extra_tokens:
        path.append(slave_dir.join('linux_vulkan_sdk', 'bin'))
        ld_library_path.append(slave_dir.join('linux_vulkan_sdk', 'lib'))

    if 'MSAN' in extra_tokens:
      # Find the MSAN-built libc++.
      ld_library_path.append(clang_linux + '/msan')

    if any('SAN' in t for t in extra_tokens):
      # Sanitized binaries may want to run clang_linux/bin/llvm-symbolizer.
      path.append(clang_linux + '/bin')
      # We find that testing sanitizer builds with libc++ uncovers more issues
      # than with the system-provided C++ standard library, which is usually
      # libstdc++. libc++ proactively hooks into sanitizers to help their
      # analyses. We ship a copy of libc++ with our Linux toolchain in /lib.
      ld_library_path.append(clang_linux + '/lib')
    elif self.m.vars.is_linux:
      cmd = ['catchsegv'] + cmd
    elif 'ProcDump' in extra_tokens:
      self.m.file.ensure_directory('makedirs dumps', self.m.vars.dumps_dir)
      procdump = str(self.m.vars.slave_dir.join('procdump_win',
                                                'procdump64.exe'))
      # Full docs for ProcDump here:
      # https://docs.microsoft.com/en-us/sysinternals/downloads/procdump
      # -accepteula automatically accepts the license agreement
      # -mp saves a packed minidump to save space
      # -e 1 tells procdump to dump once
      # -x <dump dir> <exe> <args> launches exe and writes dumps to the
      #   specified dir
      cmd = [procdump, '-accepteula', '-mp', '-e', '1',
             '-x', self.m.vars.dumps_dir] + cmd

    if 'ASAN' in extra_tokens or 'UBSAN' in extra_tokens:
      if 'Mac' in self.m.vars.builder_cfg.get('os', ''):
        env['ASAN_OPTIONS'] = 'symbolize=1'  # Mac doesn't support detect_leaks.
      else:
        env['ASAN_OPTIONS'] = 'symbolize=1 detect_leaks=1'
      env[ 'LSAN_OPTIONS'] = 'symbolize=1 print_suppressions=1'
      env['UBSAN_OPTIONS'] = 'symbolize=1 print_stacktrace=1'

    if 'TSAN' in extra_tokens:
      # We don't care about malloc(), fprintf, etc. used in signal handlers.
      # If we're in a signal handler, we're already crashing...
      env['TSAN_OPTIONS'] = 'report_signal_unsafe=0'

    if 'Coverage' in extra_tokens:
      # This is the output file for the coverage data. Just running the binary
      # will produce the output. The output_file is in the swarming_out_dir and
      # thus will be an isolated output of the Test step.
      profname = '%s.profraw' % self.m.vars.builder_cfg.get('test_filter','o')
      env['LLVM_PROFILE_FILE'] = self.m.path.join(self.m.vars.swarming_out_dir,
                                                  profname)

    if path:
      env['PATH'] = '%%(PATH)s:%s' % ':'.join('%s' % p for p in path)
    if ld_library_path:
      env['LD_LIBRARY_PATH'] = ':'.join('%s' % p for p in ld_library_path)

    to_symbolize = ['dm', 'nanobench']
    if name in to_symbolize and self.m.vars.is_linux:
      # Convert path objects or placeholders into strings such that they can
      # be passed to symbolize_stack_trace.py
      args = [slave_dir] + [str(x) for x in cmd]
      with self.m.context(cwd=self.m.vars.skia_dir, env=env):
        self._py('symbolized %s' % name,
                 self.module.resource('symbolize_stack_trace.py'),
                 args=args,
                 infra_step=False)

    else:
      with self.m.context(env=env):
        self._run(name, cmd)

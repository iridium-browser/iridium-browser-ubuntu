// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/variables.h"

namespace variables {

// Built-in variables ----------------------------------------------------------

const char kHostCpu[] = "host_cpu";
const char kHostCpu_HelpShort[] =
    "host_cpu: [string] The processor architecture that GN is running on.";
const char kHostCpu_Help[] =
    "host_cpu: The processor architecture that GN is running on.\n"
    "\n"
    "  This is value is exposed so that cross-compile toolchains can\n"
    "  access the host architecture when needed.\n"
    "\n"
    "  The value should generally be considered read-only, but it can be\n"
    "  overriden in order to handle unusual cases where there might\n"
    "  be multiple plausible values for the host architecture (e.g., if\n"
    "  you can do either 32-bit or 64-bit builds). The value is not used\n"
    "  internally by GN for any purpose.\n"
    "\n"
    "Some possible values:\n"
    "  - \"x64\"\n"
    "  - \"x86\"\n";

const char kHostOs[] = "host_os";
const char kHostOs_HelpShort[] =
    "host_os: [string] The operating system that GN is running on.";
const char kHostOs_Help[] =
    "host_os: [string] The operating system that GN is running on.\n"
    "\n"
    "  This value is exposed so that cross-compiles can access the host\n"
    "  build system's settings.\n"
    "\n"
    "  This value should generally be treated as read-only. It, however,\n"
    "  is not used internally by GN for any purpose.\n"
    "\n"
    "Some possible values:\n"
    "  - \"linux\"\n"
    "  - \"mac\"\n"
    "  - \"win\"\n";

const char kTargetCpu[] = "target_cpu";
const char kTargetCpu_HelpShort[] =
    "target_cpu: [string] The desired cpu architecture for the build.";
const char kTargetCpu_Help[] =
    "target_cpu: The desired cpu architecture for the build.\n"
    "\n"
    "  This value should be used to indicate the desired architecture for\n"
    "  the primary objects of the build. It will match the cpu architecture\n"
    "  of the default toolchain.\n"
    "\n"
    "  In many cases, this is the same as \"host_cpu\", but in the case\n"
    "  of cross-compiles, this can be set to something different. This \n"
    "  value is different from \"current_cpu\" in that it can be referenced\n"
    "  from inside any toolchain. This value can also be ignored if it is\n"
    "  not needed or meaningful for a project.\n"
    "\n"
    "  This value is not used internally by GN for any purpose, so it\n"
    "  may be set to whatever value is needed for the build.\n"
    "  GN defaults this value to the empty string (\"\") and the\n"
    "  configuration files should set it to an appropriate value\n"
    "  (e.g., setting it to the value of \"host_cpu\") if it is not\n"
    "  overridden on the command line or in the args.gn file.\n"
    "\n"
    "  Where practical, use one of the following list of common values:\n"
    "\n"
    "Possible values:\n"
    "  - \"x86\"\n"
    "  - \"x64\"\n"
    "  - \"arm\"\n"
    "  - \"arm64\"\n"
    "  - \"mipsel\"\n";

const char kTargetOs[] = "target_os";
const char kTargetOs_HelpShort[] =
    "target_os: [string] The desired operating system for the build.";
const char kTargetOs_Help[] =
    "target_os: The desired operating system for the build.\n"
    "\n"
    "  This value should be used to indicate the desired operating system\n"
    "  for the primary object(s) of the build. It will match the OS of\n"
    "  the default toolchain.\n"
    "\n"
    "  In many cases, this is the same as \"host_os\", but in the case of\n"
    "  cross-compiles, it may be different. This variable differs from\n"
    "  \"current_os\" in that it can be referenced from inside any\n"
    "  toolchain and will always return the initial value.\n"
    "\n"
    "  This should be set to the most specific value possible. So,\n"
    "  \"android\" or \"chromeos\" should be used instead of \"linux\"\n"
    "  where applicable, even though Android and ChromeOS are both Linux\n"
    "  variants. This can mean that one needs to write\n"
    "\n"
    "      if (target_os == \"android\" || target_os == \"linux\") {\n"
    "          # ...\n"
    "      }\n"
    "\n"
    "  and so forth.\n"
    "\n"
    "  This value is not used internally by GN for any purpose, so it\n"
    "  may be set to whatever value is needed for the build.\n"
    "  GN defaults this value to the empty string (\"\") and the\n"
    "  configuration files should set it to an appropriate value\n"
    "  (e.g., setting it to the value of \"host_os\") if it is not\n"
    "  set via the command line or in the args.gn file.\n"
    "\n"
    "  Where practical, use one of the following list of common values:\n"
    "\n"
    "Possible values:\n"
    "  - \"android\"\n"
    "  - \"chromeos\"\n"
    "  - \"ios\"\n"
    "  - \"linux\"\n"
    "  - \"nacl\"\n"
    "  - \"mac\"\n"
    "  - \"win\"\n";

const char kCurrentCpu[] = "current_cpu";
const char kCurrentCpu_HelpShort[] =
    "current_cpu: [string] The processor architecture of the current "
        "toolchain.";
const char kCurrentCpu_Help[] =
    "current_cpu: The processor architecture of the current toolchain.\n"
    "\n"
    "  The build configuration usually sets this value based on the value\n"
    "  of \"host_cpu\" (see \"gn help host_cpu\") and then threads\n"
    "  this through the toolchain definitions to ensure that it always\n"
    "  reflects the appropriate value.\n"
    "\n"
    "  This value is not used internally by GN for any purpose. It is\n"
    "  set it to the empty string (\"\") by default but is declared so\n"
    "  that it can be overridden on the command line if so desired.\n"
    "\n"
    "  See \"gn help target_cpu\" for a list of common values returned.\n";

const char kCurrentOs[] = "current_os";
const char kCurrentOs_HelpShort[] =
    "current_os: [string] The operating system of the current toolchain.";
const char kCurrentOs_Help[] =
    "current_os: The operating system of the current toolchain.\n"
    "\n"
    "  The build configuration usually sets this value based on the value\n"
    "  of \"target_os\" (see \"gn help target_os\"), and then threads this\n"
    "  through the toolchain definitions to ensure that it always reflects\n"
    "  the appropriate value.\n"
    "\n"
    "  This value is not used internally by GN for any purpose. It is\n"
    "  set it to the empty string (\"\") by default but is declared so\n"
    "  that it can be overridden on the command line if so desired.\n"
    "\n"
    "  See \"gn help target_os\" for a list of common values returned.\n";

const char kCurrentToolchain[] = "current_toolchain";
const char kCurrentToolchain_HelpShort[] =
    "current_toolchain: [string] Label of the current toolchain.";
const char kCurrentToolchain_Help[] =
    "current_toolchain: Label of the current toolchain.\n"
    "\n"
    "  A fully-qualified label representing the current toolchain. You can\n"
    "  use this to make toolchain-related decisions in the build. See also\n"
    "  \"default_toolchain\".\n"
    "\n"
    "Example:\n"
    "\n"
    "  if (current_toolchain == \"//build:64_bit_toolchain\") {\n"
    "    executable(\"output_thats_64_bit_only\") {\n"
    "      ...\n";

const char kDefaultToolchain[] = "default_toolchain";
const char kDefaultToolchain_HelpShort[] =
    "default_toolchain: [string] Label of the default toolchain.";
const char kDefaultToolchain_Help[] =
    "default_toolchain: [string] Label of the default toolchain.\n"
    "\n"
    "  A fully-qualified label representing the default toolchain, which may\n"
    "  not necessarily be the current one (see \"current_toolchain\").\n";

const char kPythonPath[] = "python_path";
const char kPythonPath_HelpShort[] =
    "python_path: [string] Absolute path of Python.";
const char kPythonPath_Help[] =
    "python_path: Absolute path of Python.\n"
    "\n"
    "  Normally used in toolchain definitions if running some command\n"
    "  requires Python. You will normally not need this when invoking scripts\n"
    "  since GN automatically finds it for you.\n";

const char kRootBuildDir[] = "root_build_dir";
const char kRootBuildDir_HelpShort[] =
  "root_build_dir: [string] Directory where build commands are run.";
const char kRootBuildDir_Help[] =
  "root_build_dir: [string] Directory where build commands are run.\n"
  "\n"
  "  This is the root build output directory which will be the current\n"
  "  directory when executing all compilers and scripts.\n"
  "\n"
  "  Most often this is used with rebase_path (see \"gn help rebase_path\")\n"
  "  to convert arguments to be relative to a script's current directory.\n";

const char kRootGenDir[] = "root_gen_dir";
const char kRootGenDir_HelpShort[] =
    "root_gen_dir: [string] Directory for the toolchain's generated files.";
const char kRootGenDir_Help[] =
    "root_gen_dir: Directory for the toolchain's generated files.\n"
    "\n"
    "  Absolute path to the root of the generated output directory tree for\n"
    "  the current toolchain. An example would be \"//out/Debug/gen\" for the\n"
    "  default toolchain, or \"//out/Debug/arm/gen\" for the \"arm\"\n"
    "  toolchain.\n"
    "\n"
    "  This is primarily useful for setting up include paths for generated\n"
    "  files. If you are passing this to a script, you will want to pass it\n"
    "  through rebase_path() (see \"gn help rebase_path\") to convert it\n"
    "  to be relative to the build directory.\n"
    "\n"
    "  See also \"target_gen_dir\" which is usually a better location for\n"
    "  generated files. It will be inside the root generated dir.\n";

const char kRootOutDir[] = "root_out_dir";
const char kRootOutDir_HelpShort[] =
    "root_out_dir: [string] Root directory for toolchain output files.";
const char kRootOutDir_Help[] =
    "root_out_dir: [string] Root directory for toolchain output files.\n"
    "\n"
    "  Absolute path to the root of the output directory tree for the current\n"
    "  toolchain. It will not have a trailing slash.\n"
    "\n"
    "  For the default toolchain this will be the same as the root_build_dir.\n"
    "  An example would be \"//out/Debug\" for the default toolchain, or\n"
    "  \"//out/Debug/arm\" for the \"arm\" toolchain.\n"
    "\n"
    "  This is primarily useful for setting up script calls. If you are\n"
    "  passing this to a script, you will want to pass it through\n"
    "  rebase_path() (see \"gn help rebase_path\") to convert it\n"
    "  to be relative to the build directory.\n"
    "\n"
    "  See also \"target_out_dir\" which is usually a better location for\n"
    "  output files. It will be inside the root output dir.\n"
    "\n"
    "Example:\n"
    "\n"
    "  action(\"myscript\") {\n"
    "    # Pass the output dir to the script.\n"
    "    args = [ \"-o\", rebase_path(root_out_dir, root_build_dir) ]\n"
    "  }\n";

const char kTargetGenDir[] = "target_gen_dir";
const char kTargetGenDir_HelpShort[] =
    "target_gen_dir: [string] Directory for a target's generated files.";
const char kTargetGenDir_Help[] =
    "target_gen_dir: Directory for a target's generated files.\n"
    "\n"
    "  Absolute path to the target's generated file directory. This will be\n"
    "  the \"root_gen_dir\" followed by the relative path to the current\n"
    "  build file. If your file is in \"//tools/doom_melon\" then\n"
    "  target_gen_dir would be \"//out/Debug/gen/tools/doom_melon\". It will\n"
    "  not have a trailing slash.\n"
    "\n"
    "  This is primarily useful for setting up include paths for generated\n"
    "  files. If you are passing this to a script, you will want to pass it\n"
    "  through rebase_path() (see \"gn help rebase_path\") to convert it\n"
    "  to be relative to the build directory.\n"
    "\n"
    "  See also \"gn help root_gen_dir\".\n"
    "\n"
    "Example:\n"
    "\n"
    "  action(\"myscript\") {\n"
    "    # Pass the generated output dir to the script.\n"
    "    args = [ \"-o\", rebase_path(target_gen_dir, root_build_dir) ]"
    "\n"
    "  }\n";

const char kTargetOutDir[] = "target_out_dir";
const char kTargetOutDir_HelpShort[] =
    "target_out_dir: [string] Directory for target output files.";
const char kTargetOutDir_Help[] =
    "target_out_dir: [string] Directory for target output files.\n"
    "\n"
    "  Absolute path to the target's generated file directory. If your\n"
    "  current target is in \"//tools/doom_melon\" then this value might be\n"
    "  \"//out/Debug/obj/tools/doom_melon\". It will not have a trailing\n"
    "  slash.\n"
    "\n"
    "  This is primarily useful for setting up arguments for calling\n"
    "  scripts. If you are passing this to a script, you will want to pass it\n"
    "  through rebase_path() (see \"gn help rebase_path\") to convert it\n"
    "  to be relative to the build directory.\n"
    "\n"
    "  See also \"gn help root_out_dir\".\n"
    "\n"
    "Example:\n"
    "\n"
    "  action(\"myscript\") {\n"
    "    # Pass the output dir to the script.\n"
    "    args = [ \"-o\", rebase_path(target_out_dir, root_build_dir) ]"
    "\n"
    "  }\n";

// Target variables ------------------------------------------------------------

#define COMMON_ORDERING_HELP \
    "\n" \
    "Ordering of flags and values:\n" \
    "\n" \
    "  1. Those set on the current target (not in a config).\n" \
    "  2. Those set on the \"configs\" on the target in order that the\n" \
    "     configs appear in the list.\n" \
    "  3. Those set on the \"all_dependent_configs\" on the target in order\n" \
    "     that the configs appear in the list.\n" \
    "  4. Those set on the \"public_configs\" on the target in order that\n" \
    "     those configs appear in the list.\n" \
    "  5. all_dependent_configs pulled from dependencies, in the order of\n" \
    "     the \"deps\" list. This is done recursively. If a config appears\n" \
    "     more than once, only the first occurance will be used.\n" \
    "  6. public_configs pulled from dependencies, in the order of the\n" \
    "     \"deps\" list. If a dependency is public, they will be applied\n" \
    "     recursively.\n"

const char kAllDependentConfigs[] = "all_dependent_configs";
const char kAllDependentConfigs_HelpShort[] =
    "all_dependent_configs: [label list] Configs to be forced on dependents.";
const char kAllDependentConfigs_Help[] =
    "all_dependent_configs: Configs to be forced on dependents.\n"
    "\n"
    "  A list of config labels.\n"
    "\n"
    "  All targets depending on this one, and recursively, all targets\n"
    "  depending on those, will have the configs listed in this variable\n"
    "  added to them. These configs will also apply to the current target.\n"
    "\n"
    "  This addition happens in a second phase once a target and all of its\n"
    "  dependencies have been resolved. Therefore, a target will not see\n"
    "  these force-added configs in their \"configs\" variable while the\n"
    "  script is running, and then can not be removed. As a result, this\n"
    "  capability should generally only be used to add defines and include\n"
    "  directories necessary to compile a target's headers.\n"
    "\n"
    "  See also \"public_configs\".\n"
    COMMON_ORDERING_HELP;

const char kAllowCircularIncludesFrom[] = "allow_circular_includes_from";
const char kAllowCircularIncludesFrom_HelpShort[] =
    "allow_circular_includes_from: [label list] Permit includes from deps.";
const char kAllowCircularIncludesFrom_Help[] =
    "allow_circular_includes_from: Permit includes from deps.\n"
    "\n"
    "  A list of target labels. Must be a subset of the target's \"deps\".\n"
    "  These targets will be permitted to include headers from the current\n"
    "  target despite the dependency going in the opposite direction.\n"
    "\n"
    "Tedious exposition\n"
    "\n"
    "  Normally, for a file in target A to include a file from target B,\n"
    "  A must list B as a dependency. This invariant is enforced by the\n"
    "  \"gn check\" command (and the --check flag to \"gn gen\").\n"
    "\n"
    "  Sometimes, two targets might be the same unit for linking purposes\n"
    "  (two source sets or static libraries that would always be linked\n"
    "  together in a final executable or shared library). In this case,\n"
    "  you want A to be able to include B's headers, and B to include A's\n"
    "  headers.\n"
    "\n"
    "  This list, if specified, lists which of the dependencies of the\n"
    "  current target can include header files from the current target.\n"
    "  That is, if A depends on B, B can only include headers from A if it is\n"
    "  in A's allow_circular_includes_from list.\n"
    "\n"
    "Example\n"
    "\n"
    "  source_set(\"a\") {\n"
    "    deps = [ \":b\", \":c\" ]\n"
    "    allow_circular_includes_from = [ \":b\" ]\n"
    "    ...\n"
    "  }\n";

const char kArgs[] = "args";
const char kArgs_HelpShort[] =
    "args: [string list] Arguments passed to an action.";
const char kArgs_Help[] =
    "args: Arguments passed to an action.\n"
    "\n"
    "  For action and action_foreach targets, args is the list of arguments\n"
    "  to pass to the script. Typically you would use source expansion (see\n"
    "  \"gn help source_expansion\") to insert the source file names.\n"
    "\n"
    "  See also \"gn help action\" and \"gn help action_foreach\".\n";

const char kCflags[] = "cflags";
const char kCflags_HelpShort[] =
    "cflags: [string list] Flags passed to all C compiler variants.";
const char kCommonCflagsHelp[] =
    "cflags*: Flags passed to the C compiler.\n"
    "\n"
    "  A list of strings.\n"
    "\n"
    "  \"cflags\" are passed to all invocations of the C, C++, Objective C,\n"
    "  and Objective C++ compilers.\n"
    "\n"
    "  To target one of these variants individually, use \"cflags_c\",\n"
    "  \"cflags_cc\", \"cflags_objc\", and \"cflags_objcc\", respectively.\n"
    "  These variant-specific versions will be appended to the \"cflags\".\n"
    COMMON_ORDERING_HELP;
const char* kCflags_Help = kCommonCflagsHelp;

const char kCflagsC[] = "cflags_c";
const char kCflagsC_HelpShort[] =
    "cflags_c: [string list] Flags passed to the C compiler.";
const char* kCflagsC_Help = kCommonCflagsHelp;

const char kCflagsCC[] = "cflags_cc";
const char kCflagsCC_HelpShort[] =
    "cflags_cc: [string list] Flags passed to the C++ compiler.";
const char* kCflagsCC_Help = kCommonCflagsHelp;

const char kCflagsObjC[] = "cflags_objc";
const char kCflagsObjC_HelpShort[] =
    "cflags_objc: [string list] Flags passed to the Objective C compiler.";
const char* kCflagsObjC_Help = kCommonCflagsHelp;

const char kCflagsObjCC[] = "cflags_objcc";
const char kCflagsObjCC_HelpShort[] =
    "cflags_objcc: [string list] Flags passed to the Objective C++ compiler.";
const char* kCflagsObjCC_Help = kCommonCflagsHelp;

const char kCheckIncludes[] = "check_includes";
const char kCheckIncludes_HelpShort[] =
    "check_includes: [boolean] Controls whether a target's files are checked.";
const char kCheckIncludes_Help[] =
    "check_includes: [boolean] Controls whether a target's files are checked.\n"
    "\n"
    "  When true (the default), the \"gn check\" command (as well as\n"
    "  \"gn gen\" with the --check flag) will check this target's sources\n"
    "  and headers for proper dependencies.\n"
    "\n"
    "  When false, the files in this target will be skipped by default.\n"
    "  This does not affect other targets that depend on the current target,\n"
    "  it just skips checking the includes of the current target's files.\n"
    "\n"
    "Controlling includes individually\n"
    "\n"
    "  If only certain includes are problematic, you can annotate them\n"
    "  individually rather than disabling header checking on an entire\n"
    "  target. Add the string \"nogncheck\" to the include line:\n"
    "\n"
    "    #include \"foo/something_weird.h\"  // nogncheck (bug 12345)\n"
    "\n"
    "  It is good form to include a reference to a bug (if the include is\n"
    "  improper, or some other comment expressing why the header checker\n"
    "  doesn't work for this particular case.\n"
    "\n"
    "  The most common reason to need \"nogncheck\" is conditional includes.\n"
    "  The header checker does not understand the preprocessor, so may flag\n"
    "  some includes as improper even if the dependencies and #defines are\n"
    "  always matched correctly:\n"
    "\n"
    "    #if defined(ENABLE_DOOM_MELON)\n"
    "    #include \"doom_melon/beam_controller.h\"  // nogncheck\n"
    "    #endif\n"
    "\n"
    "Example\n"
    "\n"
    "  source_set(\"busted_includes\") {\n"
    "    # This target's includes are messed up, exclude it from checking.\n"
    "    check_includes = false\n"
    "    ...\n"
    "  }\n";

const char kCompleteStaticLib[] = "complete_static_lib";
const char kCompleteStaticLib_HelpShort[] =
    "complete_static_lib: [boolean] Links all deps into a static library.";
const char kCompleteStaticLib_Help[] =
    "complete_static_lib: [boolean] Links all deps into a static library.\n"
    "\n"
    "  A static library normally doesn't include code from dependencies, but\n"
    "  instead forwards the static libraries and source sets in its deps up\n"
    "  the dependency chain until a linkable target (an executable or shared\n"
    "  library) is reached. The final linkable target only links each static\n"
    "  library once, even if it appears more than once in its dependency\n"
    "  graph.\n"
    "\n"
    "  In some cases the static library might be the final desired output.\n"
    "  For example, you may be producing a static library for distribution to\n"
    "  third parties. In this case, the static library should include code\n"
    "  for all dependencies in one complete package. Since GN does not unpack\n"
    "  static libraries to forward their contents up the dependency chain,\n"
    "  it is an error for complete static libraries to depend on other static\n"
    "  libraries.\n"
    "\n"
    "Example\n"
    "\n"
    "  static_library(\"foo\") {\n"
    "    complete_static_lib = true\n"
    "    deps = [ \"bar\" ]\n"
    "  }\n";

const char kConfigs[] = "configs";
const char kConfigs_HelpShort[] =
    "configs: [label list] Configs applying to this target.";
const char kConfigs_Help[] =
    "configs: Configs applying to this target.\n"
    "\n"
    "  A list of config labels.\n"
    "\n"
    "  The include_dirs, defines, etc. in each config are appended in the\n"
    "  order they appear to the compile command for each file in the target.\n"
    "  They will appear after the include_dirs, defines, etc. that the target\n"
    "  sets directly.\n"
    "\n"
    "  The build configuration script will generally set up the default\n"
    "  configs applying to a given target type (see \"set_defaults\").\n"
    "  When a target is being defined, it can add to or remove from this\n"
    "  list.\n"
    COMMON_ORDERING_HELP
    "\n"
    "Example:\n"
    "  static_library(\"foo\") {\n"
    "    configs -= \"//build:no_rtti\"  # Don't use the default RTTI config.\n"
    "    configs += \":mysettings\"      # Add some of our own settings.\n"
    "  }\n";

const char kData[] = "data";
const char kData_HelpShort[] =
    "data: [file list] Runtime data file dependencies.";
const char kData_Help[] =
    "data: Runtime data file dependencies.\n"
    "\n"
    "  Lists files or directories required to run the given target. These are\n"
    "  typically data files or directories of data files. The paths are\n"
    "  interpreted as being relative to the current build file. Since these\n"
    "  are runtime dependencies, they do not affect which targets are built\n"
    "  or when. To declare input files to a script, use \"inputs\".\n"
    "\n"
    "  Appearing in the \"data\" section does not imply any special handling\n"
    "  such as copying them to the output directory. This is just used for\n"
    "  declaring runtime dependencies. Runtime dependencies can be queried\n"
    "  using the \"runtime_deps\" category of \"gn desc\" or written during\n"
    "  build generation via \"--runtime-deps-list-file\".\n"
    "\n"
    "  GN doesn't require data files to exist at build-time. So actions that\n"
    "  produce files that are in turn runtime dependencies can list those\n"
    "  generated files both in the \"outputs\" list as well as the \"data\"\n"
    "  list.\n"
    "\n"
    "  By convention, directories are be listed with a trailing slash:\n"
    "    data = [ \"test/data/\" ]\n"
    "  However, no verification is done on these so GN doesn't enforce this.\n"
    "  The paths are just rebased and passed along when requested.\n"
    "\n"
    "  See \"gn help runtime_deps\" for how these are used.\n";

const char kDataDeps[] = "data_deps";
const char kDataDeps_HelpShort[] =
    "data_deps: [label list] Non-linked dependencies.";
const char kDataDeps_Help[] =
    "data_deps: Non-linked dependencies.\n"
    "\n"
    "  A list of target labels.\n"
    "\n"
    "  Specifies dependencies of a target that are not actually linked into\n"
    "  the current target. Such dependencies will built and will be available\n"
    "  at runtime.\n"
    "\n"
    "  This is normally used for things like plugins or helper programs that\n"
    "  a target needs at runtime.\n"
    "\n"
    "  See also \"gn help deps\" and \"gn help data\".\n"
    "\n"
    "Example:\n"
    "  executable(\"foo\") {\n"
    "    deps = [ \"//base\" ]\n"
    "    data_deps = [ \"//plugins:my_runtime_plugin\" ]\n"
    "  }\n";

const char kDefines[] = "defines";
const char kDefines_HelpShort[] =
    "defines: [string list] C preprocessor defines.";
const char kDefines_Help[] =
    "defines: C preprocessor defines.\n"
    "\n"
    "  A list of strings\n"
    "\n"
    "  These strings will be passed to the C/C++ compiler as #defines. The\n"
    "  strings may or may not include an \"=\" to assign a value.\n"
    COMMON_ORDERING_HELP
    "\n"
    "Example:\n"
    "  defines = [ \"AWESOME_FEATURE\", \"LOG_LEVEL=3\" ]\n";

const char kDepfile[] = "depfile";
const char kDepfile_HelpShort[] =
    "depfile: [string] File name for input dependencies for actions.";
const char kDepfile_Help[] =
    "depfile: [string] File name for input dependencies for actions.\n"
    "\n"
    "  If nonempty, this string specifies that the current action or\n"
    "  action_foreach target will generate the given \".d\" file containing\n"
    "  the dependencies of the input. Empty or unset means that the script\n"
    "  doesn't generate the files.\n"
    "\n"
    "  The .d file should go in the target output directory. If you have more\n"
    "  than one source file that the script is being run over, you can use\n"
    "  the output file expansions described in \"gn help action_foreach\" to\n"
    "  name the .d file according to the input."
    "\n"
    "  The format is that of a Makefile, and all of the paths should be\n"
    "  relative to the root build directory.\n"
    "\n"
    "Example:\n"
    "  action_foreach(\"myscript_target\") {\n"
    "    script = \"myscript.py\"\n"
    "    sources = [ ... ]\n"
    "\n"
    "    # Locate the depfile in the output directory named like the\n"
    "    # inputs but with a \".d\" appended.\n"
    "    depfile = \"$relative_target_output_dir/{{source_name}}.d\"\n"
    "\n"
    "    # Say our script uses \"-o <d file>\" to indicate the depfile.\n"
    "    args = [ \"{{source}}\", \"-o\", depfile ]\n"
    "  }\n";

const char kDeps[] = "deps";
const char kDeps_HelpShort[] =
    "deps: [label list] Private linked dependencies.";
const char kDeps_Help[] =
    "deps: Private linked dependencies.\n"
    "\n"
    "  A list of target labels.\n"
    "\n"
    "  Specifies private dependencies of a target. Shared and dynamic\n"
    "  libraries will be linked into the current target. Other target types\n"
    "  that can't be linked (like actions and groups) listed in \"deps\" will\n"
    "  be treated as \"data_deps\". Likewise, if the current target isn't\n"
    "  linkable, then all deps will be treated as \"data_deps\".\n"
    "\n"
    "  These dependencies are private in that it does not grant dependent\n"
    "  targets the ability to include headers from the dependency, and direct\n"
    "  dependent configs are not forwarded.\n"
    "\n"
    "  See also \"public_deps\" and \"data_deps\".\n";

// TODO(brettw) remove this, deprecated.
const char kForwardDependentConfigsFrom[] = "forward_dependent_configs_from";
const char kForwardDependentConfigsFrom_HelpShort[] =
    "forward_dependent_configs_from: [label list] DEPRECATED.";
const char kForwardDependentConfigsFrom_Help[] =
    "forward_dependent_configs_from\n"
    "\n"
    "  A list of target labels.\n"
    "\n"
    "  DEPRECATED. Use public_deps instead which will have the same effect.\n"
    "\n"
    "  Exposes the public_configs from a private dependent target as\n"
    "  public_configs of the current one. Each label in this list\n"
    "  must also be in the deps.\n"
    "\n"
    "  Generally you should use public_deps instead of this variable to\n"
    "  express the concept of exposing a dependency as part of a target's\n"
    "  public API. We're considering removing this variable.\n"
    "\n"
    "Discussion\n"
    "\n"
    "  Sometimes you depend on a child library that exports some necessary\n"
    "  configuration via public_configs. If your target in turn exposes the\n"
    "  child library's headers in its public headers, it might mean that\n"
    "  targets that depend on you won't work: they'll be seeing the child\n"
    "  library's code but not the necessary configuration. This list\n"
    "  specifies which of your deps' direct dependent configs to expose as\n"
    "  your own.\n"
    "\n"
    "Examples\n"
    "\n"
    "  If we use a given library \"a\" from our public headers:\n"
    "\n"
    "    deps = [ \":a\", \":b\", ... ]\n"
    "    forward_dependent_configs_from = [ \":a\" ]\n"
    "\n"
    "  This example makes a \"transparent\" target that forwards a dependency\n"
    "  to another:\n"
    "\n"
    "    group(\"frob\") {\n"
    "      if (use_system_frob) {\n"
    "        deps = \":system_frob\"\n"
    "      } else {\n"
    "        deps = \"//third_party/fallback_frob\"\n"
    "      }\n"
    "      forward_dependent_configs_from = deps\n"
    "    }\n";

const char kIncludeDirs[] = "include_dirs";
const char kIncludeDirs_HelpShort[] =
    "include_dirs: [directory list] Additional include directories.";
const char kIncludeDirs_Help[] =
    "include_dirs: Additional include directories.\n"
    "\n"
    "  A list of source directories.\n"
    "\n"
    "  The directories in this list will be added to the include path for\n"
    "  the files in the affected target.\n"
    COMMON_ORDERING_HELP
    "\n"
    "Example:\n"
    "  include_dirs = [ \"src/include\", \"//third_party/foo\" ]\n";

const char kInputs[] = "inputs";
const char kInputs_HelpShort[] =
    "inputs: [file list] Additional compile-time dependencies.";
const char kInputs_Help[] =
    "inputs: Additional compile-time dependencies.\n"
    "\n"
    "  Inputs are compile-time dependencies of the current target. This means\n"
    "  that all inputs must be available before compiling any of the sources\n"
    "  or executing any actions.\n"
    "\n"
    "  Inputs are typically only used for action and action_foreach targets.\n"
    "\n"
    "Inputs for actions\n"
    "\n"
    "  For action and action_foreach targets, inputs should be the inputs to\n"
    "  script that don't vary. These should be all .py files that the script\n"
    "  uses via imports (the main script itself will be an implcit dependency\n"
    "  of the action so need not be listed).\n"
    "\n"
    "  For action targets, inputs should be the entire set of inputs the\n"
    "  script needs. For action_foreach targets, inputs should be the set of\n"
    "  dependencies that don't change. These will be applied to each script\n"
    "  invocation over the sources.\n"
    "\n"
    "  Note that another way to declare input dependencies from an action\n"
    "  is to have the action write a depfile (see \"gn help depfile\"). This\n"
    "  allows the script to dynamically write input dependencies, that might\n"
    "  not be known until actually executing the script. This is more\n"
    "  efficient than doing processing while running GN to determine the\n"
    "  inputs, and is easier to keep in-sync than hardcoding the list.\n"
    "\n"
    "Inputs for binary targets\n"
    "\n"
    "  Any input dependencies will be resolved before compiling any sources.\n"
    "  Normally, all actions that a target depends on will be run before any\n"
    "  files in a target are compiled. So if you depend on generated headers,\n"
    "  you do not typically need to list them in the inputs section.\n"
    "\n"
    "Example\n"
    "\n"
    "  action(\"myscript\") {\n"
    "    script = \"domything.py\"\n"
    "    inputs = [ \"input.data\" ]\n"
    "  }\n";

const char kLdflags[] = "ldflags";
const char kLdflags_HelpShort[] =
    "ldflags: [string list] Flags passed to the linker.";
const char kLdflags_Help[] =
    "ldflags: Flags passed to the linker.\n"
    "\n"
    "  A list of strings.\n"
    "\n"
    "  These flags are passed on the command-line to the linker and generally\n"
    "  specify various linking options. Most targets will not need these and\n"
    "  will use \"libs\" and \"lib_dirs\" instead.\n"
    "\n"
    "  ldflags are NOT pushed to dependents, so applying ldflags to source\n"
    "  sets or static libraries will be a no-op. If you want to apply ldflags\n"
    "  to dependent targets, put them in a config and set it in the\n"
    "  all_dependent_configs or public_configs.\n";

#define COMMON_LIB_INHERITANCE_HELP \
    "\n" \
    "  libs and lib_dirs work differently than other flags in two respects.\n" \
    "  First, then are inherited across static library boundaries until a\n" \
    "  shared library or executable target is reached. Second, they are\n" \
    "  uniquified so each one is only passed once (the first instance of it\n" \
    "  will be the one used).\n"

const char kLibDirs[] = "lib_dirs";
const char kLibDirs_HelpShort[] =
    "lib_dirs: [directory list] Additional library directories.";
const char kLibDirs_Help[] =
    "lib_dirs: Additional library directories.\n"
    "\n"
    "  A list of directories.\n"
    "\n"
    "  Specifies additional directories passed to the linker for searching\n"
    "  for the required libraries. If an item is not an absolute path, it\n"
    "  will be treated as being relative to the current build file.\n"
    COMMON_LIB_INHERITANCE_HELP
    COMMON_ORDERING_HELP
    "\n"
    "Example:\n"
    "  lib_dirs = [ \"/usr/lib/foo\", \"lib/doom_melon\" ]\n";

const char kLibs[] = "libs";
const char kLibs_HelpShort[] =
    "libs: [string list] Additional libraries to link.";
const char kLibs_Help[] =
    "libs: Additional libraries to link.\n"
    "\n"
    "  A list of strings.\n"
    "\n"
    "  These files will be passed to the linker, which will generally search\n"
    "  the library include path. Unlike a normal list of files, they will be\n"
    "  passed to the linker unmodified rather than being treated as file\n"
    "  names relative to the current build file. Generally you would set\n"
    "  the \"lib_dirs\" so your library is found. If you need to specify\n"
    "  a path, you can use \"rebase_path\" to convert a path to be relative\n"
    "  to the build directory.\n"
    "\n"
    "  When constructing the linker command, the \"lib_prefix\" attribute of\n"
    "  the linker tool in the current toolchain will be prepended to each\n"
    "  library. So your BUILD file should not specify the switch prefix\n"
    "  (like \"-l\"). On Mac, libraries ending in \".framework\" will be\n"
    "  special-cased: the switch \"-framework\" will be prepended instead of\n"
    "  the lib_prefix, and the \".framework\" suffix will be trimmed.\n"
    COMMON_LIB_INHERITANCE_HELP
    COMMON_ORDERING_HELP
    "\n"
    "Examples:\n"
    "  On Windows:\n"
    "    libs = [ \"ctl3d.lib\" ]\n"
    "  On Linux:\n"
    "    libs = [ \"ld\" ]\n";

const char kOutputExtension[] = "output_extension";
const char kOutputExtension_HelpShort[] =
    "output_extension: [string] Value to use for the output's file extension.";
const char kOutputExtension_Help[] =
    "output_extension: Value to use for the output's file extension.\n"
    "\n"
    "  Normally the file extension for a target is based on the target\n"
    "  type and the operating system, but in rare cases you will need to\n"
    "  override the name (for example to use \"libfreetype.so.6\" instead\n"
    "  of libfreetype.so on Linux).";

const char kOutputName[] = "output_name";
const char kOutputName_HelpShort[] =
    "output_name: [string] Name for the output file other than the default.";
const char kOutputName_Help[] =
    "output_name: Define a name for the output file other than the default.\n"
    "\n"
    "  Normally the output name of a target will be based on the target name,\n"
    "  so the target \"//foo/bar:bar_unittests\" will generate an output\n"
    "  file such as \"bar_unittests.exe\" (using Windows as an example).\n"
    "\n"
    "  Sometimes you will want an alternate name to avoid collisions or\n"
    "  if the internal name isn't appropriate for public distribution.\n"
    "\n"
    "  The output name should have no extension or prefixes, these will be\n"
    "  added using the default system rules. For example, on Linux an output\n"
    "  name of \"foo\" will produce a shared library \"libfoo.so\".\n"
    "\n"
    "  This variable is valid for all binary output target types.\n"
    "\n"
    "Example:\n"
    "  static_library(\"doom_melon\") {\n"
    "    output_name = \"fluffy_bunny\"\n"
    "  }\n";

const char kOutputs[] = "outputs";
const char kOutputs_HelpShort[] =
    "outputs: [file list] Output files for actions and copy targets.";
const char kOutputs_Help[] =
    "outputs: Output files for actions and copy targets.\n"
    "\n"
    "  Outputs is valid for \"copy\", \"action\", and \"action_foreach\"\n"
    "  target types and indicates the resulting files. The values may contain\n"
    "  source expansions to generate the output names from the sources (see\n"
    "  \"gn help source_expansion\").\n"
    "\n"
    "  For copy targets, the outputs is the destination for the copied\n"
    "  file(s). For actions, the outputs should be the list of files\n"
    "  generated by the script.\n";

const char kPrecompiledHeader[] = "precompiled_header";
const char kPrecompiledHeader_HelpShort[] =
    "precompiled_header: [string] Header file to precompile.";
const char kPrecompiledHeader_Help[] =
    "precompiled_header: [string] Header file to precompile.\n"
    "\n"
    "  Precompiled headers will be used when a target specifies this\n"
    "  value, or a config applying to this target specifies this value.\n"
    "  In addition, the tool corresponding to the source files must also\n"
    "  specify precompiled headers (see \"gn help tool\"). The tool\n"
    "  will also specify what type of precompiled headers to use.\n"
    "\n"
    "  The precompiled header/source variables can be specified on a target\n"
    "  or a config, but must be the same for all configs applying to a given\n"
    "  target since a target can only have one precompiled header.\n"
    "\n"
    "MSVC precompiled headers\n"
    "\n"
    "  When using MSVC-style precompiled headers, the \"precompiled_header\"\n"
    "  value is a string corresponding to the header. This is NOT a path\n"
    "  to a file that GN recognises, but rather the exact string that appears\n"
    "  in quotes after an #include line in source code. The compiler will\n"
    "  match this string against includes or forced includes (/FI).\n"
    "\n"
    "  MSVC also requires a source file to compile the header with. This must\n"
    "  be specified by the \"precompiled_source\" value. In contrast to the\n"
    "  header value, this IS a GN-style file name, and tells GN which source\n"
    "  file to compile to make the .pch file used for subsequent compiles.\n"
    "\n"
    "  If you use both C and C++ sources, the precompiled header and source\n"
    "  file will be compiled using both tools. You will want to make sure\n"
    "  to wrap C++ includes in __cplusplus #ifdefs so the file will compile\n"
    "  in C mode.\n"
    "\n"
    "  For example, if the toolchain specifies MSVC headers:\n"
    "\n"
    "    toolchain(\"vc_x64\") {\n"
    "      ...\n"
    "      tool(\"cxx\") {\n"
    "        precompiled_header_type = \"msvc\"\n"
    "        ...\n"
    "\n"
    "  You might make a config like this:\n"
    "\n"
    "    config(\"use_precompiled_headers\") {\n"
    "      precompiled_header = \"build/precompile.h\"\n"
    "      precompiled_source = \"//build/precompile.cc\"\n"
    "\n"
    "      # Either your source files should #include \"build/precompile.h\"\n"
    "      # first, or you can do this to force-include the header.\n"
    "      cflags = [ \"/FI$precompiled_header\" ]\n"
    "    }\n"
    "\n"
    "  And then define a target that uses the config:\n"
    "\n"
    "    executable(\"doom_melon\") {\n"
    "      configs += [ \":use_precompiled_headers\" ]\n"
    "      ...\n"
    "\n";

const char kPrecompiledSource[] = "precompiled_source";
const char kPrecompiledSource_HelpShort[] =
    "precompiled_source: [file name] Source file to precompile.";
const char kPrecompiledSource_Help[] =
    "precompiled_source: [file name] Source file to precompile.\n"
    "\n"
    "  The source file that goes along with the precompiled_header when\n"
    "  using \"msvc\"-style precompiled headers. It will be implicitly added\n"
    "  to the sources of the target. See \"gn help precompiled_header\".\n";

const char kPublic[] = "public";
const char kPublic_HelpShort[] =
    "public: [file list] Declare public header files for a target.";
const char kPublic_Help[] =
    "public: Declare public header files for a target.\n"
    "\n"
    "  A list of files that other targets can include. These permissions are\n"
    "  checked via the \"check\" command (see \"gn help check\").\n"
    "\n"
    "  If no public files are declared, other targets (assuming they have\n"
    "  visibility to depend on this target can include any file in the\n"
    "  sources list. If this variable is defined on a target, dependent\n"
    "  targets may only include files on this whitelist.\n"
    "\n"
    "  Header file permissions are also subject to visibility. A target\n"
    "  must be visible to another target to include any files from it at all\n"
    "  and the public headers indicate which subset of those files are\n"
    "  permitted. See \"gn help visibility\" for more.\n"
    "\n"
    "  Public files are inherited through the dependency tree. So if there is\n"
    "  a dependency A -> B -> C, then A can include C's public headers.\n"
    "  However, the same is NOT true of visibility, so unless A is in C's\n"
    "  visibility list, the include will be rejected.\n"
    "\n"
    "  GN only knows about files declared in the \"sources\" and \"public\"\n"
    "  sections of targets. If a file is included that is not known to the\n"
    "  build, it will be allowed.\n"
    "\n"
    "Examples:\n"
    "  These exact files are public:\n"
    "    public = [ \"foo.h\", \"bar.h\" ]\n"
    "\n"
    "  No files are public (no targets may include headers from this one):\n"
    "    public = []\n";

const char kPublicConfigs[] = "public_configs";
const char kPublicConfigs_HelpShort[] =
    "public_configs: [label list] Configs applied to dependents.";
const char kPublicConfigs_Help[] =
    "public_configs: Configs to be applied on dependents.\n"
    "\n"
    "  A list of config labels.\n"
    "\n"
    "  Targets directly depending on this one will have the configs listed in\n"
    "  this variable added to them. These configs will also apply to the\n"
    "  current target.\n"
    "\n"
    "  This addition happens in a second phase once a target and all of its\n"
    "  dependencies have been resolved. Therefore, a target will not see\n"
    "  these force-added configs in their \"configs\" variable while the\n"
    "  script is running, and then can not be removed. As a result, this\n"
    "  capability should generally only be used to add defines and include\n"
    "  directories necessary to compile a target's headers.\n"
    "\n"
    "  See also \"all_dependent_configs\".\n"
    COMMON_ORDERING_HELP;

const char kPublicDeps[] = "public_deps";
const char kPublicDeps_HelpShort[] =
    "public_deps: [label list] Declare public dependencies.";
const char kPublicDeps_Help[] =
    "public_deps: Declare public dependencies.\n"
    "\n"
    "  Public dependencies are like private dependencies (\"deps\") but\n"
    "  additionally express that the current target exposes the listed deps\n"
    "  as part of its public API.\n"
    "\n"
    "  This has several ramifications:\n"
    "\n"
    "    - public_configs that are part of the dependency are forwarded\n"
    "      to direct dependents.\n"
    "\n"
    "    - Public headers in the dependency are usable by dependents\n"
    "      (includes do not require a direct dependency or visibility).\n"
    "\n"
    "    - If the current target is a shared library, other shared libraries\n"
    "      that it publicly depends on (directly or indirectly) are\n"
    "      propagated up the dependency tree to dependents for linking.\n"
    "\n"
    "Discussion\n"
    "\n"
    "  Say you have three targets: A -> B -> C. C's visibility may allow\n"
    "  B to depend on it but not A. Normally, this would prevent A from\n"
    "  including any headers from C, and C's public_configs would apply\n"
    "  only to B.\n"
    "\n"
    "  If B lists C in its public_deps instead of regular deps, A will now\n"
    "  inherit C's public_configs and the ability to include C's public\n"
    "  headers.\n"
    "\n"
    "  Generally if you are writing a target B and you include C's headers\n"
    "  as part of B's public headers, or targets depending on B should\n"
    "  consider B and C to be part of a unit, you should use public_deps\n"
    "  instead of deps.\n"
    "\n"
    "Example\n"
    "\n"
    "  # This target can include files from \"c\" but not from\n"
    "  # \"super_secret_implementation_details\".\n"
    "  executable(\"a\") {\n"
    "    deps = [ \":b\" ]\n"
    "  }\n"
    "\n"
    "  shared_library(\"b\") {\n"
    "    deps = [ \":super_secret_implementation_details\" ]\n"
    "    public_deps = [ \":c\" ]\n"
    "  }\n";

const char kScript[] = "script";
const char kScript_HelpShort[] =
    "script: [file name] Script file for actions.";
const char kScript_Help[] =
    "script: Script file for actions.\n"
    "\n"
    "  An absolute or buildfile-relative file name of a Python script to run\n"
    "  for a action and action_foreach targets (see \"gn help action\" and\n"
    "  \"gn help action_foreach\").\n";

const char kSources[] = "sources";
const char kSources_HelpShort[] =
    "sources: [file list] Source files for a target.";
const char kSources_Help[] =
    "sources: Source files for a target\n"
    "\n"
    "  A list of files relative to the current buildfile.\n";

const char kTestonly[] = "testonly";
const char kTestonly_HelpShort[] =
    "testonly: [boolean] Declares a target must only be used for testing.";
const char kTestonly_Help[] =
    "testonly: Declares a target must only be used for testing.\n"
    "\n"
    "  Boolean. Defaults to false.\n"
    "\n"
    "  When a target is marked \"testonly = true\", it must only be depended\n"
    "  on by other test-only targets. Otherwise, GN will issue an error\n"
    "  that the depenedency is not allowed.\n"
    "\n"
    "  This feature is intended to prevent accidentally shipping test code\n"
    "  in a final product.\n"
    "\n"
    "Example\n"
    "\n"
    "  source_set(\"test_support\") {\n"
    "    testonly = true\n"
    "    ...\n"
    "  }\n";

const char kVisibility[] = "visibility";
const char kVisibility_HelpShort[] =
    "visibility: [label list] A list of labels that can depend on a target.";
const char kVisibility_Help[] =
    "visibility: A list of labels that can depend on a target.\n"
    "\n"
    "  A list of labels and label patterns that define which targets can\n"
    "  depend on the current one. These permissions are checked via the\n"
    "  \"check\" command (see \"gn help check\").\n"
    "\n"
    "  If visibility is not defined, it defaults to public (\"*\").\n"
    "\n"
    "  If visibility is defined, only the targets with labels that match it\n"
    "  can depend on the current target. The empty list means no targets\n"
    "  can depend on the current target.\n"
    "\n"
    "  Tip: Often you will want the same visibility for all targets in a\n"
    "  BUILD file. In this case you can just put the definition at the top,\n"
    "  outside of any target, and the targets will inherit that scope and see\n"
    "  the definition.\n"
    "\n"
    "Patterns\n"
    "\n"
    "  See \"gn help label_pattern\" for more details on what types of\n"
    "  patterns are supported. If a toolchain is specified, only targets\n"
    "  in that toolchain will be matched. If a toolchain is not specified on\n"
    "  a pattern, targets in all toolchains will be matched.\n"
    "\n"
    "Examples\n"
    "\n"
    "  Only targets in the current buildfile (\"private\"):\n"
    "    visibility = [ \":*\" ]\n"
    "\n"
    "  No targets (used for targets that should be leaf nodes):\n"
    "    visibility = []\n"
    "\n"
    "  Any target (\"public\", the default):\n"
    "    visibility = [ \"*\" ]\n"
    "\n"
    "  All targets in the current directory and any subdirectory:\n"
    "    visibility = [ \"./*\" ]\n"
    "\n"
    "  Any target in \"//bar/BUILD.gn\":\n"
    "    visibility = [ \"//bar:*\" ]\n"
    "\n"
    "  Any target in \"//bar/\" or any subdirectory thereof:\n"
    "    visibility = [ \"//bar/*\" ]\n"
    "\n"
    "  Just these specific targets:\n"
    "    visibility = [ \":mything\", \"//foo:something_else\" ]\n"
    "\n"
    "  Any target in the current directory and any subdirectory thereof, plus\n"
    "  any targets in \"//bar/\" and any subdirectory thereof.\n"
    "    visibility = [ \"./*\", \"//bar/*\" ]\n";

// -----------------------------------------------------------------------------

VariableInfo::VariableInfo()
    : help_short(""),
      help("") {
}

VariableInfo::VariableInfo(const char* in_help_short, const char* in_help)
    : help_short(in_help_short),
      help(in_help) {
}

#define INSERT_VARIABLE(var) \
    info_map[k##var] = VariableInfo(k##var##_HelpShort, k##var##_Help);

const VariableInfoMap& GetBuiltinVariables() {
  static VariableInfoMap info_map;
  if (info_map.empty()) {
    INSERT_VARIABLE(CurrentCpu)
    INSERT_VARIABLE(CurrentOs)
    INSERT_VARIABLE(CurrentToolchain)
    INSERT_VARIABLE(DefaultToolchain)
    INSERT_VARIABLE(HostCpu)
    INSERT_VARIABLE(HostOs)
    INSERT_VARIABLE(PythonPath)
    INSERT_VARIABLE(RootBuildDir)
    INSERT_VARIABLE(RootGenDir)
    INSERT_VARIABLE(RootOutDir)
    INSERT_VARIABLE(TargetCpu)
    INSERT_VARIABLE(TargetOs)
    INSERT_VARIABLE(TargetGenDir)
    INSERT_VARIABLE(TargetOutDir)
  }
  return info_map;
}

const VariableInfoMap& GetTargetVariables() {
  static VariableInfoMap info_map;
  if (info_map.empty()) {
    INSERT_VARIABLE(AllDependentConfigs)
    INSERT_VARIABLE(AllowCircularIncludesFrom)
    INSERT_VARIABLE(Args)
    INSERT_VARIABLE(Cflags)
    INSERT_VARIABLE(CflagsC)
    INSERT_VARIABLE(CflagsCC)
    INSERT_VARIABLE(CflagsObjC)
    INSERT_VARIABLE(CflagsObjCC)
    INSERT_VARIABLE(CheckIncludes)
    INSERT_VARIABLE(CompleteStaticLib)
    INSERT_VARIABLE(Configs)
    INSERT_VARIABLE(Data)
    INSERT_VARIABLE(DataDeps)
    INSERT_VARIABLE(Defines)
    INSERT_VARIABLE(Depfile)
    INSERT_VARIABLE(Deps)
    INSERT_VARIABLE(ForwardDependentConfigsFrom)
    INSERT_VARIABLE(IncludeDirs)
    INSERT_VARIABLE(Inputs)
    INSERT_VARIABLE(Ldflags)
    INSERT_VARIABLE(Libs)
    INSERT_VARIABLE(LibDirs)
    INSERT_VARIABLE(OutputExtension)
    INSERT_VARIABLE(OutputName)
    INSERT_VARIABLE(Outputs)
    INSERT_VARIABLE(PrecompiledHeader)
    INSERT_VARIABLE(PrecompiledSource)
    INSERT_VARIABLE(Public)
    INSERT_VARIABLE(PublicConfigs)
    INSERT_VARIABLE(PublicDeps)
    INSERT_VARIABLE(Script)
    INSERT_VARIABLE(Sources)
    INSERT_VARIABLE(Testonly)
    INSERT_VARIABLE(Visibility)
  }
  return info_map;
}

#undef INSERT_VARIABLE

}  // namespace variables

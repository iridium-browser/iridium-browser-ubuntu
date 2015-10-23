// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/setup.h"

#include <stdlib.h>

#include <algorithm>
#include <sstream>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "tools/gn/commands.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/parser.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/switches.h"
#include "tools/gn/tokenizer.h"
#include "tools/gn/trace.h"
#include "tools/gn/value.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

extern const char kDotfile_Help[] =
    ".gn file\n"
    "\n"
    "  When gn starts, it will search the current directory and parent\n"
    "  directories for a file called \".gn\". This indicates the source root.\n"
    "  You can override this detection by using the --root command-line\n"
    "  argument\n"
    "\n"
    "  The .gn file in the source root will be executed. The syntax is the\n"
    "  same as a buildfile, but with very limited build setup-specific\n"
    "  meaning.\n"
    "\n"
    "  If you specify --root, by default GN will look for the file .gn in\n"
    "  that directory. If you want to specify a different file, you can\n"
    "  additionally pass --dotfile:\n"
    "\n"
    "    gn gen out/Debug --root=/home/build --dotfile=/home/my_gn_file.gn\n"
    "\n"
    "Variables\n"
    "\n"
    "  buildconfig [required]\n"
    "      Label of the build config file. This file will be used to set up\n"
    "      the build file execution environment for each toolchain.\n"
    "\n"
    "  check_targets [optional]\n"
    "      A list of labels and label patterns that should be checked when\n"
    "      running \"gn check\" or \"gn gen --check\". If unspecified, all\n"
    "      targets will be checked. If it is the empty list, no targets will\n"
    "      be checked.\n"
    "\n"
    "      The format of this list is identical to that of \"visibility\"\n"
    "      so see \"gn help visibility\" for examples.\n"
    "\n"
    "  exec_script_whitelist [optional]\n"
    "      A list of .gn/.gni files (not labels) that have permission to call\n"
    "      the exec_script function. If this list is defined, calls to\n"
    "      exec_script will be checked against this list and GN will fail if\n"
    "      the current file isn't in the list.\n"
    "\n"
    "      This is to allow the use of exec_script to be restricted since\n"
    "      is easy to use inappropriately. Wildcards are not supported.\n"
    "      Files in the secondary_source tree (if defined) should be\n"
    "      referenced by ignoring the secondary tree and naming them as if\n"
    "      they are in the main tree.\n"
    "\n"
    "      If unspecified, the ability to call exec_script is unrestricted.\n"
    "\n"
    "      Example:\n"
    "        exec_script_whitelist = [\n"
    "          \"//base/BUILD.gn\",\n"
    "          \"//build/my_config.gni\",\n"
    "        ]\n"
    "\n"
    "  root [optional]\n"
    "      Label of the root build target. The GN build will start by loading\n"
    "      the build file containing this target name. This defaults to\n"
    "      \"//:\" which will cause the file //BUILD.gn to be loaded.\n"
    "\n"
    "  secondary_source [optional]\n"
    "      Label of an alternate directory tree to find input files. When\n"
    "      searching for a BUILD.gn file (or the build config file discussed\n"
    "      above), the file will first be looked for in the source root.\n"
    "      If it's not found, the secondary source root will be checked\n"
    "      (which would contain a parallel directory hierarchy).\n"
    "\n"
    "      This behavior is intended to be used when BUILD.gn files can't be\n"
    "      checked in to certain source directories for whatever reason.\n"
    "\n"
    "      The secondary source root must be inside the main source tree.\n"
    "\n"
    "Example .gn file contents\n"
    "\n"
    "  buildconfig = \"//build/config/BUILDCONFIG.gn\"\n"
    "\n"
    "  check_targets = [\n"
    "    \"//doom_melon/*\",  # Check everything in this subtree.\n"
    "    \"//tools:mind_controlling_ant\",  # Check this specific target.\n"
    "  ]\n"
    "\n"
    "  root = \"//:root\"\n"
    "\n"
    "  secondary_source = \"//build/config/temporary_buildfiles/\"\n";

namespace {

const base::FilePath::CharType kGnFile[] = FILE_PATH_LITERAL(".gn");

base::FilePath FindDotFile(const base::FilePath& current_dir) {
  base::FilePath try_this_file = current_dir.Append(kGnFile);
  if (base::PathExists(try_this_file))
    return try_this_file;

  base::FilePath with_no_slash = current_dir.StripTrailingSeparators();
  base::FilePath up_one_dir = with_no_slash.DirName();
  if (up_one_dir == current_dir)
    return base::FilePath();  // Got to the top.

  return FindDotFile(up_one_dir);
}

// Called on any thread. Post the item to the builder on the main thread.
void ItemDefinedCallback(base::MessageLoop* main_loop,
                         scoped_refptr<Builder> builder,
                         scoped_ptr<Item> item) {
  DCHECK(item);
  main_loop->PostTask(FROM_HERE, base::Bind(&Builder::ItemDefined, builder,
                                            base::Passed(&item)));
}

void DecrementWorkCount() {
  g_scheduler->DecrementWorkCount();
}

}  // namespace

const char Setup::kBuildArgFileName[] = "args.gn";

Setup::Setup()
    : build_settings_(),
      loader_(new LoaderImpl(&build_settings_)),
      builder_(new Builder(loader_.get())),
      root_build_file_("//BUILD.gn"),
      check_public_headers_(false),
      dotfile_settings_(&build_settings_, std::string()),
      dotfile_scope_(&dotfile_settings_),
      fill_arguments_(true) {
  dotfile_settings_.set_toolchain_label(Label());
  build_settings_.set_item_defined_callback(
      base::Bind(&ItemDefinedCallback, scheduler_.main_loop(), builder_));

  loader_->set_complete_callback(base::Bind(&DecrementWorkCount));
  // The scheduler's main loop wasn't created when the Loader was created, so
  // we need to set it now.
  loader_->set_main_loop(scheduler_.main_loop());
}

Setup::~Setup() {
}

bool Setup::DoSetup(const std::string& build_dir, bool force_create) {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();

  scheduler_.set_verbose_logging(cmdline->HasSwitch(switches::kVerbose));
  if (cmdline->HasSwitch(switches::kTime) ||
      cmdline->HasSwitch(switches::kTracelog))
    EnableTracing();

  ScopedTrace setup_trace(TraceItem::TRACE_SETUP, "DoSetup");

  if (!FillSourceDir(*cmdline))
    return false;
  if (!RunConfigFile())
    return false;
  if (!FillOtherConfig(*cmdline))
    return false;

  // Must be after FillSourceDir to resolve.
  if (!FillBuildDir(build_dir, !force_create))
    return false;

  // Check for unused variables in the .gn file.
  Err err;
  if (!dotfile_scope_.CheckForUnusedVars(&err)) {
    err.PrintToStdout();
    return false;
  }

  if (fill_arguments_) {
    if (!FillArguments(*cmdline))
      return false;
  }
  FillPythonPath();

  return true;
}

bool Setup::Run() {
  RunPreMessageLoop();
  if (!scheduler_.Run())
    return false;
  return RunPostMessageLoop();
}

SourceFile Setup::GetBuildArgFile() const {
  return SourceFile(build_settings_.build_dir().value() + kBuildArgFileName);
}

void Setup::RunPreMessageLoop() {
  // Load the root build file.
  loader_->Load(root_build_file_, LocationRange(), Label());

  // Will be decremented with the loader is drained.
  g_scheduler->IncrementWorkCount();
}

bool Setup::RunPostMessageLoop() {
  Err err;
  if (build_settings_.check_for_bad_items()) {
    if (!builder_->CheckForBadItems(&err)) {
      err.PrintToStdout();
      return false;
    }

    if (!build_settings_.build_args().VerifyAllOverridesUsed(&err)) {
      // TODO(brettw) implement a system of warnings. Until we have a better
      // system, print the error but don't return failure.
      err.PrintToStdout();
      return true;
    }
  }

  if (check_public_headers_) {
    std::vector<const Target*> all_targets = builder_->GetAllResolvedTargets();
    std::vector<const Target*> to_check;
    if (check_patterns()) {
      commands::FilterTargetsByPatterns(all_targets, *check_patterns(),
                                        &to_check);
    } else {
      to_check = all_targets;
    }

    if (!commands::CheckPublicHeaders(&build_settings_, all_targets,
                                      to_check, false)) {
      return false;
    }
  }

  // Write out tracing and timing if requested.
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(switches::kTime))
    PrintLongHelp(SummarizeTraces());
  if (cmdline->HasSwitch(switches::kTracelog))
    SaveTraces(cmdline->GetSwitchValuePath(switches::kTracelog));

  return true;
}

bool Setup::FillArguments(const base::CommandLine& cmdline) {
  // Use the args on the command line if specified, and save them. Do this even
  // if the list is empty (this means clear any defaults).
  if (cmdline.HasSwitch(switches::kArgs)) {
    if (!FillArgsFromCommandLine(cmdline.GetSwitchValueASCII(switches::kArgs)))
      return false;
    SaveArgsToFile();
    return true;
  }

  // No command line args given, use the arguments from the build dir (if any).
  return FillArgsFromFile();
}

bool Setup::FillArgsFromCommandLine(const std::string& args) {
  args_input_file_.reset(new InputFile(SourceFile()));
  args_input_file_->SetContents(args);
  args_input_file_->set_friendly_name("the command-line \"--args\"");
  return FillArgsFromArgsInputFile();
}

bool Setup::FillArgsFromFile() {
  ScopedTrace setup_trace(TraceItem::TRACE_SETUP, "Load args file");

  SourceFile build_arg_source_file = GetBuildArgFile();
  base::FilePath build_arg_file =
      build_settings_.GetFullPath(build_arg_source_file);

  std::string contents;
  if (!base::ReadFileToString(build_arg_file, &contents))
    return true;  // File doesn't exist, continue with default args.

  // Add a dependency on the build arguments file. If this changes, we want
  // to re-generate the build.
  g_scheduler->AddGenDependency(build_arg_file);

  if (contents.empty())
    return true;  // Empty file, do nothing.

  args_input_file_.reset(new InputFile(build_arg_source_file));
  args_input_file_->SetContents(contents);
  args_input_file_->set_friendly_name(
      "build arg file (use \"gn args <out_dir>\" to edit)");

  setup_trace.Done();  // Only want to count the load as part of the trace.
  return FillArgsFromArgsInputFile();
}

bool Setup::FillArgsFromArgsInputFile() {
  ScopedTrace setup_trace(TraceItem::TRACE_SETUP, "Parse args");

  Err err;
  args_tokens_ = Tokenizer::Tokenize(args_input_file_.get(), &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  args_root_ = Parser::Parse(args_tokens_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  Scope arg_scope(&dotfile_settings_);
  args_root_->Execute(&arg_scope, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  // Save the result of the command args.
  Scope::KeyValueMap overrides;
  arg_scope.GetCurrentScopeValues(&overrides);
  build_settings_.build_args().AddArgOverrides(overrides);
  return true;
}

bool Setup::SaveArgsToFile() {
  ScopedTrace setup_trace(TraceItem::TRACE_SETUP, "Save args file");

  std::ostringstream stream;
  for (const auto& pair : build_settings_.build_args().GetAllOverrides()) {
    stream << pair.first.as_string() << " = " << pair.second.ToString(true);
    stream << std::endl;
  }

  // For the first run, the build output dir might not be created yet, so do
  // that so we can write a file into it. Ignore errors, we'll catch the error
  // when we try to write a file to it below.
  base::FilePath build_arg_file =
      build_settings_.GetFullPath(GetBuildArgFile());
  base::CreateDirectory(build_arg_file.DirName());

  std::string contents = stream.str();
#if defined(OS_WIN)
  // Use Windows lineendings for this file since it will often open in
  // Notepad which can't handle Unix ones.
  base::ReplaceSubstringsAfterOffset(&contents, 0, "\n", "\r\n");
#endif
  if (base::WriteFile(build_arg_file, contents.c_str(),
      static_cast<int>(contents.size())) == -1) {
    Err(Location(), "Args file could not be written.",
      "The file is \"" + FilePathToUTF8(build_arg_file) +
        "\"").PrintToStdout();
    return false;
  }

  // Add a dependency on the build arguments file. If this changes, we want
  // to re-generate the build.
  g_scheduler->AddGenDependency(build_arg_file);

  return true;
}

bool Setup::FillSourceDir(const base::CommandLine& cmdline) {
  // Find the .gn file.
  base::FilePath root_path;

  // Prefer the command line args to the config file.
  base::FilePath relative_root_path =
      cmdline.GetSwitchValuePath(switches::kRoot);
  if (!relative_root_path.empty()) {
    root_path = base::MakeAbsoluteFilePath(relative_root_path);
    if (root_path.empty()) {
      Err(Location(), "Root source path not found.",
          "The path \"" + FilePathToUTF8(relative_root_path) +
          "\" doesn't exist.").PrintToStdout();
      return false;
    }

    // When --root is specified, an alternate --dotfile can also be set.
    // --dotfile should be a real file path and not a "//foo" source-relative
    // path.
    base::FilePath dot_file_path =
        cmdline.GetSwitchValuePath(switches::kDotfile);
    if (dot_file_path.empty()) {
      dotfile_name_ = root_path.Append(kGnFile);
    } else {
      dotfile_name_ = base::MakeAbsoluteFilePath(dot_file_path);
      if (dotfile_name_.empty()) {
        Err(Location(), "Could not load dotfile.",
            "The file \"" + FilePathToUTF8(dot_file_path) +
            "\" cound't be loaded.").PrintToStdout();
        return false;
      }
    }
  } else {
    // In the default case, look for a dotfile and that also tells us where the
    // source root is.
    base::FilePath cur_dir;
    base::GetCurrentDirectory(&cur_dir);
    dotfile_name_ = FindDotFile(cur_dir);
    if (dotfile_name_.empty()) {
      Err(Location(), "Can't find source root.",
          "I could not find a \".gn\" file in the current directory or any "
          "parent,\nand the --root command-line argument was not specified.")
          .PrintToStdout();
      return false;
    }
    root_path = dotfile_name_.DirName();
  }

  if (scheduler_.verbose_logging())
    scheduler_.Log("Using source root", FilePathToUTF8(root_path));
  build_settings_.SetRootPath(root_path);

  return true;
}

bool Setup::FillBuildDir(const std::string& build_dir, bool require_exists) {
  Err err;
  SourceDir resolved =
      SourceDirForCurrentDirectory(build_settings_.root_path()).
    ResolveRelativeDir(Value(nullptr, build_dir), &err,
        build_settings_.root_path_utf8());
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  if (scheduler_.verbose_logging())
    scheduler_.Log("Using build dir", resolved.value());

  if (require_exists) {
    base::FilePath build_dir_path = build_settings_.GetFullPath(resolved);
    if (!base::PathExists(build_dir_path.Append(
            FILE_PATH_LITERAL("build.ninja")))) {
      Err(Location(), "Not a build directory.",
          "This command requires an existing build directory. I interpreted "
          "your input\n\"" + build_dir + "\" as:\n  " +
          FilePathToUTF8(build_dir_path) +
          "\nwhich doesn't seem to contain a previously-generated build.")
          .PrintToStdout();
      return false;
    }
  }

  build_settings_.SetBuildDir(resolved);
  return true;
}

void Setup::FillPythonPath() {
  // Trace this since it tends to be a bit slow on Windows.
  ScopedTrace setup_trace(TraceItem::TRACE_SETUP, "Fill Python Path");
#if defined(OS_WIN)
  // Find Python on the path so we can use the absolute path in the build.
  const base::char16 kGetPython[] =
      L"cmd.exe /c python -c \"import sys; print sys.executable\"";
  std::string python_path;
  if (base::GetAppOutput(kGetPython, &python_path)) {
    base::TrimWhitespaceASCII(python_path, base::TRIM_ALL, &python_path);
    if (scheduler_.verbose_logging())
      scheduler_.Log("Found python", python_path);
  } else {
    scheduler_.Log("WARNING", "Could not find python on path, using "
        "just \"python.exe\"");
    python_path = "python.exe";
  }
  build_settings_.set_python_path(base::FilePath(base::UTF8ToUTF16(python_path))
                                      .NormalizePathSeparatorsTo('/'));
#else
  build_settings_.set_python_path(base::FilePath("python"));
#endif
}

bool Setup::RunConfigFile() {
  if (scheduler_.verbose_logging())
    scheduler_.Log("Got dotfile", FilePathToUTF8(dotfile_name_));

  dotfile_input_file_.reset(new InputFile(SourceFile("//.gn")));
  if (!dotfile_input_file_->Load(dotfile_name_)) {
    Err(Location(), "Could not load dotfile.",
        "The file \"" + FilePathToUTF8(dotfile_name_) + "\" cound't be loaded")
        .PrintToStdout();
    return false;
  }

  Err err;
  dotfile_tokens_ = Tokenizer::Tokenize(dotfile_input_file_.get(), &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  dotfile_root_ = Parser::Parse(dotfile_tokens_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  dotfile_root_->Execute(&dotfile_scope_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  return true;
}

bool Setup::FillOtherConfig(const base::CommandLine& cmdline) {
  Err err;
  SourceDir current_dir("//");

  // Secondary source path, read from the config file if present.
  // Read from the config file if present.
  const Value* secondary_value =
      dotfile_scope_.GetValue("secondary_source", true);
  if (secondary_value) {
    if (!secondary_value->VerifyTypeIs(Value::STRING, &err)) {
      err.PrintToStdout();
      return false;
    }
    build_settings_.SetSecondarySourcePath(
        SourceDir(secondary_value->string_value()));
  }

  // Root build file.
  const Value* root_value = dotfile_scope_.GetValue("root", true);
  if (root_value) {
    if (!root_value->VerifyTypeIs(Value::STRING, &err)) {
      err.PrintToStdout();
      return false;
    }

    Label root_target_label =
        Label::Resolve(current_dir, Label(), *root_value, &err);
    if (err.has_error()) {
      err.PrintToStdout();
      return false;
    }

    root_build_file_ = Loader::BuildFileForLabel(root_target_label);
  }

  // Build config file.
  const Value* build_config_value =
      dotfile_scope_.GetValue("buildconfig", true);
  if (!build_config_value) {
    Err(Location(), "No build config file.",
        "Your .gn file (\"" + FilePathToUTF8(dotfile_name_) + "\")\n"
        "didn't specify a \"buildconfig\" value.").PrintToStdout();
    return false;
  } else if (!build_config_value->VerifyTypeIs(Value::STRING, &err)) {
    err.PrintToStdout();
    return false;
  }
  build_settings_.set_build_config_file(
      SourceFile(build_config_value->string_value()));

  // Targets to check.
  const Value* check_targets_value =
      dotfile_scope_.GetValue("check_targets", true);
  if (check_targets_value) {
    // Fill the list of targets to check.
    if (!check_targets_value->VerifyTypeIs(Value::LIST, &err)) {
      err.PrintToStdout();
      return false;
    }

    check_patterns_.reset(new std::vector<LabelPattern>);
    for (const auto& item : check_targets_value->list_value()) {
      check_patterns_->push_back(
          LabelPattern::GetPattern(current_dir, item, &err));
      if (err.has_error()) {
        err.PrintToStdout();
        return false;
      }
    }
  }

  // Fill exec_script_whitelist.
  const Value* exec_script_whitelist_value =
      dotfile_scope_.GetValue("exec_script_whitelist", true);
  if (exec_script_whitelist_value) {
    // Fill the list of targets to check.
    if (!exec_script_whitelist_value->VerifyTypeIs(Value::LIST, &err)) {
      err.PrintToStdout();
      return false;
    }
    scoped_ptr<std::set<SourceFile>> whitelist(new std::set<SourceFile>);
    for (const auto& item : exec_script_whitelist_value->list_value()) {
      if (!item.VerifyTypeIs(Value::STRING, &err)) {
        err.PrintToStdout();
        return false;
      }
      whitelist->insert(current_dir.ResolveRelativeFile(item, &err));
      if (err.has_error()) {
        err.PrintToStdout();
        return false;
      }
    }
    build_settings_.set_exec_script_whitelist(whitelist.Pass());
  }

  return true;
}

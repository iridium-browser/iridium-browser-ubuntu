// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/runtime_deps.h"

#include <map>
#include <set>
#include <sstream>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/builder.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/loader.h"
#include "tools/gn/output_file.h"
#include "tools/gn/settings.h"
#include "tools/gn/switches.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

namespace {

using RuntimeDepsVector = std::vector<std::pair<OutputFile, const Target*>>;

// Adds the given file to the deps list if it hasn't already been listed in
// the found_files list. Updates the list.
void AddIfNew(const OutputFile& output_file,
              const Target* source,
              RuntimeDepsVector* deps,
              std::set<OutputFile>* found_file) {
  if (found_file->find(output_file) != found_file->end())
    return;  // Already there.
  deps->push_back(std::make_pair(output_file, source));
}

// Automatically converts a string that looks like a source to an OutputFile.
void AddIfNew(const std::string& str,
              const Target* source,
              RuntimeDepsVector* deps,
              std::set<OutputFile>* found_file) {
  OutputFile output_file(RebasePath(
      str,
      source->settings()->build_settings()->build_dir(),
      source->settings()->build_settings()->root_path_utf8()));
  AddIfNew(output_file, source, deps, found_file);
}

// Returns the output file that the runtime deps considers for the given
// targets. This is weird only for shared libraries.
const OutputFile& GetMainOutput(const Target* target) {
  if (target->output_type() == Target::SHARED_LIBRARY)
    return target->link_output_file();
  return target->dependency_output_file();
}

// To avoid duplicate traversals of targets, or duplicating output files that
// might be listed by more than one target, the set of targets and output files
// that have been found so far is passed. The "value" of the seen_targets map
// is a boolean indicating if the seen dep was a data dep (true = data_dep).
// data deps add more stuff, so we will want to revisit a target if it's a
// data dependency and we've previously only seen it as a regular dep.
void RecursiveCollectRuntimeDeps(const Target* target,
                                 bool is_target_data_dep,
                                 RuntimeDepsVector* deps,
                                 std::map<const Target*, bool>* seen_targets,
                                 std::set<OutputFile>* found_files) {
  const auto& found_seen_target = seen_targets->find(target);
  if (found_seen_target != seen_targets->end()) {
    // Already visited.
    if (found_seen_target->second || !is_target_data_dep) {
      // Already visited as a data dep, or the current dep is not a data
      // dep so visiting again will be a no-op.
      return;
    }
    // In the else case, the previously seen target was a regular dependency
    // and we'll now process it as a data dependency.
  }
  (*seen_targets)[target] = is_target_data_dep;

  // Add the main output file for executables and shared libraries.
  if (target->output_type() == Target::EXECUTABLE ||
      target->output_type() == Target::SHARED_LIBRARY)
    AddIfNew(GetMainOutput(target), target, deps, found_files);

  // Add all data files.
  for (const auto& file : target->data())
    AddIfNew(file, target, deps, found_files);

  // Actions/copy have all outputs considered when the're a data dep.
  if (is_target_data_dep &&
      (target->output_type() == Target::ACTION ||
       target->output_type() == Target::ACTION_FOREACH ||
       target->output_type() == Target::COPY_FILES)) {
    std::vector<SourceFile> outputs;
    target->action_values().GetOutputsAsSourceFiles(target, &outputs);
    for (const auto& output_file : outputs)
      AddIfNew(output_file.value(), target, deps, found_files);
  }

  // Non-data dependencies (both public and private).
  for (const auto& dep_pair : target->GetDeps(Target::DEPS_LINKED)) {
    if (dep_pair.ptr->output_type() == Target::EXECUTABLE)
      continue;  // Skip executables that aren't data deps.
    RecursiveCollectRuntimeDeps(dep_pair.ptr, false,
                                deps, seen_targets, found_files);
  }

  // Data dependencies.
  for (const auto& dep_pair : target->data_deps()) {
    RecursiveCollectRuntimeDeps(dep_pair.ptr, true,
                                deps, seen_targets, found_files);
  }
}

bool WriteRuntimeDepsFile(const Target* target) {
  SourceFile target_output_as_source =
      GetMainOutput(target).AsSourceFile(target->settings()->build_settings());
  std::string data_deps_file_as_str = target_output_as_source.value();
  data_deps_file_as_str.append(".runtime_deps");
  base::FilePath data_deps_file =
      target->settings()->build_settings()->GetFullPath(
          SourceFile(SourceFile::SwapIn(), &data_deps_file_as_str));

  std::stringstream contents;
  for (const auto& pair : ComputeRuntimeDeps(target))
    contents << pair.first.value() << std::endl;

  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE, data_deps_file_as_str);
  base::CreateDirectory(data_deps_file.DirName());

  std::string contents_str = contents.str();
  return base::WriteFile(data_deps_file, contents_str.c_str(),
                         static_cast<int>(contents_str.size())) > -1;
}

}  // namespace

const char kRuntimeDeps_Help[] =
    "Runtime dependencies\n"
    "\n"
    "  Runtime dependencies of a target are exposed via the \"runtime_deps\"\n"
    "  category of \"gn desc\" (see \"gn help desc\") or they can be written\n"
    "  at build generation time via \"--runtime-deps-list-file\"\n"
    "  (see \"gn help --runtime-deps-list-file\").\n"
    "\n"
    "  To a first approximation, the runtime dependencies of a target are\n"
    "  the set of \"data\" files, data directories, and the shared libraries\n"
    "  from all transitive dependencies. Executables and shared libraries are\n"
    "  considered runtime dependencies of themselves.\n"
    "\n"
    "Executables\n"
    "\n"
    "  Executable targets and those executable targets' transitive\n"
    "  dependencies are not considered unless that executable is listed in\n"
    "  \"data_deps\". Otherwise, GN assumes that the executable (and\n"
    "  everything it requires) is a build-time dependency only.\n"
    "\n"
    "Actions and copies\n"
    "\n"
    "  Action and copy targets that are listed as \"data_deps\" will have all\n"
    "  of their outputs and data files considered as runtime dependencies.\n"
    "  Action and copy targets that are \"deps\" or \"public_deps\" will have\n"
    "  only their data files considered as runtime dependencies. These\n"
    "  targets can list an output file in both the \"outputs\" and \"data\"\n"
    "  lists to force an output file as a runtime dependency in all cases.\n"
    "\n"
    "  The different rules for deps and data_deps are to express build-time\n"
    "  (deps) vs. run-time (data_deps) outputs. If GN counted all build-time\n"
    "  copy steps as data dependencies, there would be a lot of extra stuff,\n"
    "  and if GN counted all run-time dependencies as regular deps, the\n"
    "  build's parallelism would be unnecessarily constrained.\n"
    "\n"
    "  This rule can sometimes lead to unintuitive results. For example,\n"
    "  given the three targets:\n"
    "    A  --[data_deps]-->  B  --[deps]-->  ACTION\n"
    "  GN would say that A does not have runtime deps on the result of the\n"
    "  ACTION, which is often correct. But the purpose of the B target might\n"
    "  be to collect many actions into one logic unit, and the \"data\"-ness\n"
    "  of A's dependency is lost. Solutions:\n"
    "\n"
    "   - List the outputs of the action in it's data section (if the\n"
    "     results of that action are always runtime files).\n"
    "   - Have B list the action in data_deps (if the outputs of the actions\n"
    "     are always runtime files).\n"
    "   - Have B list the action in both deps and data deps (if the outputs\n"
    "     might be used in both contexts and you don't care about unnecessary\n"
    "     entries in the list of files required at runtime).\n"
    "   - Split B into run-time and build-time versions with the appropriate\n"
    "     \"deps\" for each.\n"
    "\n"
    "Static libraries and source sets\n"
    "\n"
    "  The results of static_library or source_set targets are not considered\n"
    "  runtime dependencies since these are assumed to be intermediate\n"
    "  targets only. If you need to list a static library as a runtime\n"
    "  dependency, you can manually compute the .a/.lib file name for the\n"
    "  current platform and list it in the \"data\" list of a target\n"
    "  (possibly on the static library target itself).\n"
    "\n"
    "Multiple outputs\n"
    "\n"
    "  When a tool produces more than one output, only the first output\n"
    "  is considered. For example, a shared library target may produce a\n"
    "  .dll and a .lib file on Windows. Only the .dll file will be considered\n"
    "  a runtime dependency. This applies only to linker tools, scripts and\n"
    "  copy steps with multiple outputs will also get all outputs listed.\n";

RuntimeDepsVector ComputeRuntimeDeps(const Target* target) {
  RuntimeDepsVector result;
  std::map<const Target*, bool> seen_targets;
  std::set<OutputFile> found_files;

  // The initial target is not considered a data dependency so that actions's
  // outputs (if the current target is an action) are not automatically
  // considered data deps.
  RecursiveCollectRuntimeDeps(target, false,
                              &result, &seen_targets, &found_files);
  return result;
}

bool WriteRuntimeDepsFilesIfNecessary(const Builder& builder, Err* err) {
  std::string deps_target_list_file =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRuntimeDepsListFile);
  if (deps_target_list_file.empty())
    return true;  // Nothing to do.

  std::string list_contents;
  ScopedTrace load_trace(TraceItem::TRACE_FILE_LOAD, deps_target_list_file);
  if (!base::ReadFileToString(UTF8ToFilePath(deps_target_list_file),
                              &list_contents)) {
    *err = Err(Location(),
        std::string("File for --") + switches::kRuntimeDepsListFile +
            " doesn't exist.",
        "The file given was \"" + deps_target_list_file + "\"");
    return false;
  }
  load_trace.Done();

  SourceDir root_dir("//");
  Label default_toolchain_label = builder.loader()->GetDefaultToolchain();
  for (const auto& line : base::SplitString(
           list_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    if (line.empty())
      continue;
    Label label = Label::Resolve(root_dir, default_toolchain_label,
                                 Value(nullptr, line), err);
    if (err->has_error())
      return false;

    const Item* item = builder.GetItem(label);
    const Target* target = item ? item->AsTarget() : nullptr;
    if (!target) {
      *err = Err(Location(), "The label \"" + label.GetUserVisibleName(true) +
          "\" isn't a target.",
          "When reading the line:\n  " + line + "\n"
          "from the --" + switches::kRuntimeDepsListFile + "=" +
          deps_target_list_file);
      return false;
    }

    // Currently this writes all runtime deps files sequentially. We generally
    // expect few of these. We can run this on the worker pool if it looks
    // like it's talking a long time.
    WriteRuntimeDepsFile(target);
  }
  return true;
}

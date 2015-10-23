// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_binary_target_writer.h"

#include <cstring>
#include <set>
#include <sstream>

#include "base/containers/hash_tables.h"
#include "base/strings/string_util.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/err.h"
#include "tools/gn/escape.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/ninja_utils.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/settings.h"
#include "tools/gn/source_file_type.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/target.h"

// Represents a set of tool types. Must be first since it is also shared by
// some helper functions in the anonymous namespace below.
class NinjaBinaryTargetWriter::SourceFileTypeSet {
 public:
  SourceFileTypeSet() {
    memset(flags_, 0, sizeof(bool) * static_cast<int>(SOURCE_NUMTYPES));
  }

  void Set(SourceFileType type) {
    flags_[static_cast<int>(type)] = true;
  }
  bool Get(SourceFileType type) const {
    return flags_[static_cast<int>(type)];
  }

 private:
  bool flags_[static_cast<int>(SOURCE_NUMTYPES)];
};

namespace {

// Returns the proper escape options for writing compiler and linker flags.
EscapeOptions GetFlagOptions() {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_COMMAND;

  // Some flag strings are actually multiple flags that expect to be just
  // added to the command line. We assume that quoting is done by the
  // buildfiles if it wants such things quoted.
  opts.inhibit_quoting = true;

  return opts;
}

struct DefineWriter {
  DefineWriter() {
    options.mode = ESCAPE_NINJA_COMMAND;
  }

  void operator()(const std::string& s, std::ostream& out) const {
    out << " -D";
    EscapeStringToStream(out, s, options);
  }

  EscapeOptions options;
};

struct IncludeWriter {
  explicit IncludeWriter(PathOutput& path_output) : path_output_(path_output) {
  }
  ~IncludeWriter() {
  }

  void operator()(const SourceDir& d, std::ostream& out) const {
    std::ostringstream path_out;
    path_output_.WriteDir(path_out, d, PathOutput::DIR_NO_LAST_SLASH);
    const std::string& path = path_out.str();
    if (path[0] == '"')
      out << " \"-I" << path.substr(1);
    else
      out << " -I" << path;
  }

  PathOutput& path_output_;
};

// Computes the set of output files resulting from compiling the given source
// file. If the file can be compiled and the tool exists, fills the outputs in
// and writes the tool type to computed_tool_type. If the file is not
// compilable, returns false.
//
// The target that the source belongs to is passed as an argument. In the case
// of linking to source sets, this can be different than the target this class
// is currently writing.
//
// The function can succeed with a "NONE" tool type for object files which are
// just passed to the output. The output will always be overwritten, not
// appended to.
bool GetOutputFilesForSource(const Target* target,
                             const SourceFile& source,
                             Toolchain::ToolType* computed_tool_type,
                             std::vector<OutputFile>* outputs) {
  outputs->clear();
  *computed_tool_type = Toolchain::TYPE_NONE;

  SourceFileType file_type = GetSourceFileType(source);
  if (file_type == SOURCE_UNKNOWN)
    return false;
  if (file_type == SOURCE_O) {
    // Object files just get passed to the output and not compiled.
    outputs->push_back(
        OutputFile(target->settings()->build_settings(), source));
    return true;
  }

  *computed_tool_type =
      target->toolchain()->GetToolTypeForSourceType(file_type);
  if (*computed_tool_type == Toolchain::TYPE_NONE)
    return false;  // No tool for this file (it's a header file or something).
  const Tool* tool = target->toolchain()->GetTool(*computed_tool_type);
  if (!tool)
    return false;  // Tool does not apply for this toolchain.file.

  // Figure out what output(s) this compiler produces.
  SubstitutionWriter::ApplyListToCompilerAsOutputFile(
      target, source, tool->outputs(), outputs);
  return !outputs->empty();
}

// Returns the language-specific prefix/suffix for precomiled header files.
const char* GetPCHLangForToolType(Toolchain::ToolType type) {
  switch (type) {
    case Toolchain::TYPE_CC:
      return "c";
    case Toolchain::TYPE_CXX:
      return "cc";
    case Toolchain::TYPE_OBJC:
      return "m";
    case Toolchain::TYPE_OBJCXX:
      return "mm";
    default:
      NOTREACHED() << "Not a valid PCH tool type type";
      return "";
  }
}

// Returns the object files for the precompiled header of the given type (flag
// type and tool type must match).
void GetWindowsPCHObjectFiles(const Target* target,
                              Toolchain::ToolType tool_type,
                              std::vector<OutputFile>* outputs) {
  outputs->clear();

  // Compute the tool. This must use the tool type passed in rather than the
  // detected file type of the precompiled source file since the same
  // precompiled source file will be used for separate C/C++ compiles.
  const Tool* tool = target->toolchain()->GetTool(tool_type);
  if (!tool)
    return;
  SubstitutionWriter::ApplyListToCompilerAsOutputFile(
      target, target->config_values().precompiled_source(),
      tool->outputs(), outputs);

  if (outputs->empty())
    return;
  if (outputs->size() > 1)
    outputs->resize(1);  // Only link the first output from the compiler tool.

  // Need to annotate the obj files with the language type. For example:
  //   obj/foo/target_name.precompile.obj ->
  //   obj/foo/target_name.precompile.cc.obj
  const char* lang_suffix = GetPCHLangForToolType(tool_type);
  std::string& output_value = (*outputs)[0].value();
  size_t extension_offset = FindExtensionOffset(output_value);
  if (extension_offset == std::string::npos) {
    NOTREACHED() << "No extension found";
  } else {
    DCHECK(extension_offset >= 1);
    DCHECK(output_value[extension_offset - 1] == '.');
    output_value.insert(extension_offset - 1, ".");
    output_value.insert(extension_offset, lang_suffix);
  }
}

// Appends the object files generated by the given source set to the given
// output vector.
void AddSourceSetObjectFiles(const Target* source_set,
                             UniqueVector<OutputFile>* obj_files) {
  std::vector<OutputFile> tool_outputs;  // Prevent allocation in loop.
  NinjaBinaryTargetWriter::SourceFileTypeSet used_types;

  // Compute object files for all sources. Only link the first output from
  // the tool if there are more than one.
  for (const auto& source : source_set->sources()) {
    Toolchain::ToolType tool_type = Toolchain::TYPE_NONE;
    if (GetOutputFilesForSource(source_set, source, &tool_type, &tool_outputs))
      obj_files->push_back(tool_outputs[0]);

    used_types.Set(GetSourceFileType(source));
  }

  // Precompiled header object files.
  if (source_set->config_values().has_precompiled_headers()) {
    if (used_types.Get(SOURCE_C)) {
      GetWindowsPCHObjectFiles(source_set, Toolchain::TYPE_CC, &tool_outputs);
      obj_files->Append(tool_outputs.begin(), tool_outputs.end());
    }
    if (used_types.Get(SOURCE_CPP)) {
      GetWindowsPCHObjectFiles(source_set, Toolchain::TYPE_CXX, &tool_outputs);
      obj_files->Append(tool_outputs.begin(), tool_outputs.end());
    }
    if (used_types.Get(SOURCE_M)) {
      GetWindowsPCHObjectFiles(source_set, Toolchain::TYPE_OBJC, &tool_outputs);
      obj_files->Append(tool_outputs.begin(), tool_outputs.end());
    }
    if (used_types.Get(SOURCE_MM)) {
      GetWindowsPCHObjectFiles(source_set, Toolchain::TYPE_OBJCXX,
                               &tool_outputs);
      obj_files->Append(tool_outputs.begin(), tool_outputs.end());
    }
  }
}

}  // namespace

NinjaBinaryTargetWriter::NinjaBinaryTargetWriter(const Target* target,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, out),
      tool_(target->toolchain()->GetToolForTargetFinalOutput(target)),
      rule_prefix_(GetNinjaRulePrefixForToolchain(settings_)) {
}

NinjaBinaryTargetWriter::~NinjaBinaryTargetWriter() {
}

void NinjaBinaryTargetWriter::Run() {
  // Figure out what source types are needed.
  SourceFileTypeSet used_types;
  for (const auto& source : target_->sources())
    used_types.Set(GetSourceFileType(source));

  WriteCompilerVars(used_types);

  // The input dependencies will be an order-only dependency. This will cause
  // Ninja to make sure the inputs are up-to-date before compiling this source,
  // but changes in the inputs deps won't cause the file to be recompiled.
  //
  // This is important to prevent changes in unrelated actions that are
  // upstream of this target from causing everything to be recompiled
  //
  // Why can we get away with this rather than using implicit deps ("|", which
  // will force rebuilds when the inputs change)?  For source code, the
  // computed dependencies of all headers will be computed by the compiler,
  // which will cause source rebuilds if any "real" upstream dependencies
  // change.
  //
  // If a .cc file is generated by an input dependency, Ninja will see the
  // input to the build rule doesn't exist, and that it is an output from a
  // previous step, and build the previous step first. This is a "real"
  // dependency and doesn't need | or || to express.
  //
  // The only case where this rule matters is for the first build where no .d
  // files exist, and Ninja doesn't know what that source file depends on. In
  // this case it's sufficient to ensure that the upstream dependencies are
  // built first. This is exactly what Ninja's order-only dependencies
  // expresses.
  OutputFile order_only_dep =
      WriteInputDepsStampAndGetDep(std::vector<const Target*>());

  std::vector<OutputFile> pch_obj_files;
  WritePrecompiledHeaderCommands(used_types, order_only_dep, &pch_obj_files);

  // Treat all precompiled object files as explicit dependencies of all
  // compiles. Some notes:
  //
  //  - Technically only the language-specific one is required for any specific
  //    compile, but that's more difficult to express and the additional logic
  //    doesn't buy much reduced parallelism. Just list them all (there's
  //    usually only one anyway).
  //
  //  - Technically the .pch file is the input to the compile, not the
  //    precompiled header's corresponding object file that we're using here.
  //    But Ninja's depslog doesn't support multiple outputs from the
  //    precompiled header compile step (it outputs both the .pch file and a
  //    corresponding .obj file). So we consistently list the .obj file and the
  //    .pch file we really need comes along with it.
  std::vector<OutputFile> obj_files;
  std::vector<SourceFile> other_files;
  WriteSources(pch_obj_files, order_only_dep, &obj_files, &other_files);

  // Also link all pch object files.
  obj_files.insert(obj_files.end(), pch_obj_files.begin(), pch_obj_files.end());

  if (!CheckForDuplicateObjectFiles(obj_files))
    return;

  if (target_->output_type() == Target::SOURCE_SET) {
    WriteSourceSetStamp(obj_files);
#ifndef NDEBUG
    // Verify that the function that separately computes a source set's object
    // files match the object files just computed.
    UniqueVector<OutputFile> computed_obj;
    AddSourceSetObjectFiles(target_, &computed_obj);
    DCHECK_EQ(obj_files.size(), computed_obj.size());
    for (const auto& obj : obj_files)
      DCHECK_NE(static_cast<size_t>(-1), computed_obj.IndexOf(obj));
#endif
  } else {
    WriteLinkerStuff(obj_files, other_files);
  }
}

void NinjaBinaryTargetWriter::WriteCompilerVars(
    const SourceFileTypeSet& used_types) {
  const SubstitutionBits& subst = target_->toolchain()->substitution_bits();

  // Defines.
  if (subst.used[SUBSTITUTION_DEFINES]) {
    out_ << kSubstitutionNinjaNames[SUBSTITUTION_DEFINES] << " =";
    RecursiveTargetConfigToStream<std::string>(
        target_, &ConfigValues::defines, DefineWriter(), out_);
    out_ << std::endl;
  }

  // Include directories.
  if (subst.used[SUBSTITUTION_INCLUDE_DIRS]) {
    out_ << kSubstitutionNinjaNames[SUBSTITUTION_INCLUDE_DIRS] << " =";
    PathOutput include_path_output(
        path_output_.current_dir(),
        settings_->build_settings()->root_path_utf8(),
        ESCAPE_NINJA_COMMAND);
    RecursiveTargetConfigToStream<SourceDir>(
        target_, &ConfigValues::include_dirs,
        IncludeWriter(include_path_output), out_);
    out_ << std::endl;
  }

  bool has_precompiled_headers =
      target_->config_values().has_precompiled_headers();

  // Some toolchains pass cflags to the assembler since it's the same command,
  // and cflags_c might also be sent to the objective C compiler.
  //
  // TODO(brettw) remove the SOURCE_M from the CFLAGS_C writing once the Chrome
  // Mac build is updated not to pass cflags_c to .m files.
  EscapeOptions opts = GetFlagOptions();
  if (used_types.Get(SOURCE_C) || used_types.Get(SOURCE_CPP) ||
      used_types.Get(SOURCE_M) || used_types.Get(SOURCE_MM) ||
      used_types.Get(SOURCE_S) || used_types.Get(SOURCE_ASM)) {
    WriteOneFlag(SUBSTITUTION_CFLAGS, false, Toolchain::TYPE_NONE,
                 &ConfigValues::cflags, opts);
  }
  if (used_types.Get(SOURCE_C) || used_types.Get(SOURCE_M) ||
      used_types.Get(SOURCE_S) || used_types.Get(SOURCE_ASM)) {
    WriteOneFlag(SUBSTITUTION_CFLAGS_C, has_precompiled_headers,
                 Toolchain::TYPE_CC, &ConfigValues::cflags_c, opts);
  }
  if (used_types.Get(SOURCE_CPP)) {
    WriteOneFlag(SUBSTITUTION_CFLAGS_CC, has_precompiled_headers,
                 Toolchain::TYPE_CXX, &ConfigValues::cflags_cc, opts);
  }
  if (used_types.Get(SOURCE_M)) {
    WriteOneFlag(SUBSTITUTION_CFLAGS_OBJC, has_precompiled_headers,
                 Toolchain::TYPE_OBJC, &ConfigValues::cflags_objc, opts);
  }
  if (used_types.Get(SOURCE_MM)) {
    WriteOneFlag(SUBSTITUTION_CFLAGS_OBJCC, has_precompiled_headers,
                 Toolchain::TYPE_OBJCXX, &ConfigValues::cflags_objcc, opts);
  }

  WriteSharedVars(subst);
}

void NinjaBinaryTargetWriter::WriteOneFlag(
    SubstitutionType subst_enum,
    bool has_precompiled_headers,
    Toolchain::ToolType tool_type,
    const std::vector<std::string>& (ConfigValues::* getter)() const,
    EscapeOptions flag_escape_options) {
  if (!target_->toolchain()->substitution_bits().used[subst_enum])
    return;

  out_ << kSubstitutionNinjaNames[subst_enum] << " =";

  if (has_precompiled_headers) {
    const Tool* tool = target_->toolchain()->GetTool(tool_type);
    if (tool && tool->precompiled_header_type() == Tool::PCH_MSVC) {
      // Name the .pch file.
      out_ << " /Fp";
      path_output_.WriteFile(out_, GetWindowsPCHFile(tool_type));

      // Enables precompiled headers and names the .h file. It's a string
      // rather than a file name (so no need to rebase or use path_output_).
      out_ << " /Yu" << target_->config_values().precompiled_header();
    }
  }

  RecursiveTargetConfigStringsToStream(target_, getter,
                                       flag_escape_options, out_);
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WritePrecompiledHeaderCommands(
    const SourceFileTypeSet& used_types,
    const OutputFile& order_only_dep,
    std::vector<OutputFile>* object_files) {
  if (!target_->config_values().has_precompiled_headers())
    return;

  const Tool* tool_c = target_->toolchain()->GetTool(Toolchain::TYPE_CC);
  if (tool_c &&
      tool_c->precompiled_header_type() == Tool::PCH_MSVC &&
      used_types.Get(SOURCE_C)) {
    WriteWindowsPCHCommand(SUBSTITUTION_CFLAGS_C,
                           Toolchain::TYPE_CC,
                           order_only_dep, object_files);
  }
  const Tool* tool_cxx = target_->toolchain()->GetTool(Toolchain::TYPE_CXX);
  if (tool_cxx &&
      tool_cxx->precompiled_header_type() == Tool::PCH_MSVC &&
      used_types.Get(SOURCE_CPP)) {
    WriteWindowsPCHCommand(SUBSTITUTION_CFLAGS_CC,
                           Toolchain::TYPE_CXX,
                           order_only_dep, object_files);
  }
}

void NinjaBinaryTargetWriter::WriteWindowsPCHCommand(
    SubstitutionType flag_type,
    Toolchain::ToolType tool_type,
    const OutputFile& order_only_dep,
    std::vector<OutputFile>* object_files) {
  // Compute the object file (it will be language-specific).
  std::vector<OutputFile> outputs;
  GetWindowsPCHObjectFiles(target_, tool_type, &outputs);
  if (outputs.empty())
    return;
  object_files->insert(object_files->end(), outputs.begin(), outputs.end());

  // Build line to compile the file.
  WriteCompilerBuildLine(target_->config_values().precompiled_source(),
                         std::vector<OutputFile>(), order_only_dep, tool_type,
                         outputs);

  // This build line needs a custom language-specific flags value. It needs to
  // include the switch to generate the .pch file in addition to the normal
  // ones. Rule-specific variables are just indented underneath the rule line,
  // and this defines the new one in terms of the old value.
  out_ << "  " << kSubstitutionNinjaNames[flag_type] << " =";
  out_ << " ${" << kSubstitutionNinjaNames[flag_type] << "}";

  // Append the command to generate the .pch file.
  out_ << " /Yc" << target_->config_values().precompiled_header();

  // Write two blank lines to help separate the PCH build lines from the
  // regular source build lines.
  out_ << std::endl << std::endl;
}

void NinjaBinaryTargetWriter::WriteSources(
    const std::vector<OutputFile>& extra_deps,
    const OutputFile& order_only_dep,
    std::vector<OutputFile>* object_files,
    std::vector<SourceFile>* other_files) {
  object_files->reserve(object_files->size() + target_->sources().size());

  std::vector<OutputFile> tool_outputs;  // Prevent reallocation in loop.
  for (const auto& source : target_->sources()) {
    Toolchain::ToolType tool_type = Toolchain::TYPE_NONE;
    if (!GetOutputFilesForSource(target_, source, &tool_type, &tool_outputs)) {
      if (GetSourceFileType(source) == SOURCE_DEF)
        other_files->push_back(source);
      continue;  // No output for this source.
    }

    if (tool_type != Toolchain::TYPE_NONE) {
      WriteCompilerBuildLine(source, extra_deps, order_only_dep, tool_type,
                             tool_outputs);
    }

    // It's theoretically possible for a compiler to produce more than one
    // output, but we'll only link to the first output.
    object_files->push_back(tool_outputs[0]);
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteCompilerBuildLine(
    const SourceFile& source,
    const std::vector<OutputFile>& extra_deps,
    const OutputFile& order_only_dep,
    Toolchain::ToolType tool_type,
    const std::vector<OutputFile>& outputs) {
  out_ << "build";
  path_output_.WriteFiles(out_, outputs);

  out_ << ": " << rule_prefix_ << Toolchain::ToolTypeToName(tool_type);
  out_ << " ";
  path_output_.WriteFile(out_, source);

  if (!extra_deps.empty()) {
    out_ << " |";
    for (const OutputFile& dep : extra_deps) {
      out_ << " ";
      path_output_.WriteFile(out_, dep);
    }
  }

  if (!order_only_dep.value().empty()) {
    out_ << " || ";
    path_output_.WriteFile(out_, order_only_dep);
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLinkerStuff(
    const std::vector<OutputFile>& object_files,
    const std::vector<SourceFile>& other_files) {
  std::vector<OutputFile> output_files;
  SubstitutionWriter::ApplyListToLinkerAsOutputFile(
      target_, tool_, tool_->outputs(), &output_files);

  out_ << "build";
  path_output_.WriteFiles(out_, output_files);

  out_ << ": " << rule_prefix_
       << Toolchain::ToolTypeToName(
              target_->toolchain()->GetToolTypeForTargetFinalOutput(target_));

  UniqueVector<OutputFile> extra_object_files;
  UniqueVector<const Target*> linkable_deps;
  UniqueVector<const Target*> non_linkable_deps;
  GetDeps(&extra_object_files, &linkable_deps, &non_linkable_deps);

  // Object files.
  path_output_.WriteFiles(out_, object_files);
  path_output_.WriteFiles(out_, extra_object_files);

  // Dependencies.
  std::vector<OutputFile> implicit_deps;
  std::vector<OutputFile> solibs;
  for (const Target* cur : linkable_deps) {
    // All linkable deps should have a link output file.
    DCHECK(!cur->link_output_file().value().empty())
        << "No link output file for "
        << target_->label().GetUserVisibleName(false);

    if (cur->dependency_output_file().value() !=
        cur->link_output_file().value()) {
      // This is a shared library with separate link and deps files. Save for
      // later.
      implicit_deps.push_back(cur->dependency_output_file());
      solibs.push_back(cur->link_output_file());
    } else {
      // Normal case, just link to this target.
      out_ << " ";
      path_output_.WriteFile(out_, cur->link_output_file());
    }
  }

  const SourceFile* optional_def_file = nullptr;
  if (!other_files.empty()) {
    for (const SourceFile& src_file : other_files) {
      if (GetSourceFileType(src_file) == SOURCE_DEF) {
        optional_def_file = &src_file;
        implicit_deps.push_back(
            OutputFile(settings_->build_settings(), src_file));
        break;  // Only one def file is allowed.
      }
    }
  }

  // Append implicit dependencies collected above.
  if (!implicit_deps.empty()) {
    out_ << " |";
    path_output_.WriteFiles(out_, implicit_deps);
  }

  // Append data dependencies as order-only dependencies.
  //
  // This will include data dependencies and input dependencies (like when
  // this target depends on an action). Having the data dependencies in this
  // list ensures that the data is available at runtime when the user builds
  // this target.
  //
  // The action dependencies are not strictly necessary in this case. They
  // should also have been collected via the input deps stamp that each source
  // file has for an order-only dependency, and since this target depends on
  // the sources, there is already an implicit order-only dependency. However,
  // it's extra work to separate these out and there's no disadvantage to
  // listing them again.
  WriteOrderOnlyDependencies(non_linkable_deps);

  // End of the link "build" line.
  out_ << std::endl;

  // These go in the inner scope of the link line.
  WriteLinkerFlags(optional_def_file);

  WriteLibs();
  WriteOutputExtension();
  WriteSolibs(solibs);
}

void NinjaBinaryTargetWriter::WriteLinkerFlags(
    const SourceFile* optional_def_file) {
  out_ << "  ldflags =";

  // First the ldflags from the target and its config.
  EscapeOptions flag_options = GetFlagOptions();
  RecursiveTargetConfigStringsToStream(target_, &ConfigValues::ldflags,
                                       flag_options, out_);

  // Followed by library search paths that have been recursively pushed
  // through the dependency tree.
  const OrderedSet<SourceDir> all_lib_dirs = target_->all_lib_dirs();
  if (!all_lib_dirs.empty()) {
    // Since we're passing these on the command line to the linker and not
    // to Ninja, we need to do shell escaping.
    PathOutput lib_path_output(path_output_.current_dir(),
                               settings_->build_settings()->root_path_utf8(),
                               ESCAPE_NINJA_COMMAND);
    for (size_t i = 0; i < all_lib_dirs.size(); i++) {
      out_ << " " << tool_->lib_dir_switch();
      lib_path_output.WriteDir(out_, all_lib_dirs[i],
                               PathOutput::DIR_NO_LAST_SLASH);
    }
  }

  if (optional_def_file) {
    out_ << " /DEF:";
    path_output_.WriteFile(out_, *optional_def_file);
  }

  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLibs() {
  out_ << "  libs =";

  // Libraries that have been recursively pushed through the dependency tree.
  EscapeOptions lib_escape_opts;
  lib_escape_opts.mode = ESCAPE_NINJA_COMMAND;
  const OrderedSet<std::string> all_libs = target_->all_libs();
  const std::string framework_ending(".framework");
  for (size_t i = 0; i < all_libs.size(); i++) {
    if (settings_->IsMac() &&
        base::EndsWith(all_libs[i], framework_ending,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      // Special-case libraries ending in ".framework" on Mac. Add the
      // -framework switch and don't add the extension to the output.
      out_ << " -framework ";
      EscapeStringToStream(out_,
          all_libs[i].substr(0, all_libs[i].size() - framework_ending.size()),
          lib_escape_opts);
    } else {
      out_ << " " << tool_->lib_switch();
      EscapeStringToStream(out_, all_libs[i], lib_escape_opts);
    }
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteOutputExtension() {
  out_ << "  output_extension = ";
  if (target_->output_extension().empty()) {
    // Use the default from the tool.
    out_ << tool_->default_output_extension();
  } else {
    // Use the one specified in the target. Note that the one in the target
    // does not include the leading dot, so add that.
    out_ << "." << target_->output_extension();
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteSolibs(
    const std::vector<OutputFile>& solibs) {
  if (solibs.empty())
    return;

  out_ << "  solibs =";
  path_output_.WriteFiles(out_, solibs);
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteSourceSetStamp(
    const std::vector<OutputFile>& object_files) {
  // The stamp rule for source sets is generally not used, since targets that
  // depend on this will reference the object files directly. However, writing
  // this rule allows the user to type the name of the target and get a build
  // which can be convenient for development.
  UniqueVector<OutputFile> extra_object_files;
  UniqueVector<const Target*> linkable_deps;
  UniqueVector<const Target*> non_linkable_deps;
  GetDeps(&extra_object_files, &linkable_deps, &non_linkable_deps);

  // The classifier should never put extra object files in a source set:
  // any source sets that we depend on should appear in our non-linkable
  // deps instead.
  DCHECK(extra_object_files.empty());

  std::vector<OutputFile> order_only_deps;
  for (const auto& dep : non_linkable_deps)
    order_only_deps.push_back(dep->dependency_output_file());

  WriteStampForTarget(object_files, order_only_deps);
}

void NinjaBinaryTargetWriter::GetDeps(
    UniqueVector<OutputFile>* extra_object_files,
    UniqueVector<const Target*>* linkable_deps,
    UniqueVector<const Target*>* non_linkable_deps) const {
  // Normal public/private deps.
  for (const auto& pair : target_->GetDeps(Target::DEPS_LINKED)) {
    ClassifyDependency(pair.ptr, extra_object_files,
                       linkable_deps, non_linkable_deps);
  }

  // Inherited libraries.
  for (const auto& inherited_target :
           target_->inherited_libraries().GetOrdered()) {
    ClassifyDependency(inherited_target, extra_object_files,
                       linkable_deps, non_linkable_deps);
  }

  // Data deps.
  for (const auto& data_dep_pair : target_->data_deps())
    non_linkable_deps->push_back(data_dep_pair.ptr);
}

void NinjaBinaryTargetWriter::ClassifyDependency(
    const Target* dep,
    UniqueVector<OutputFile>* extra_object_files,
    UniqueVector<const Target*>* linkable_deps,
    UniqueVector<const Target*>* non_linkable_deps) const {
  // Only the following types of outputs have libraries linked into them:
  //  EXECUTABLE
  //  SHARED_LIBRARY
  //  _complete_ STATIC_LIBRARY
  //
  // Child deps of intermediate static libraries get pushed up the
  // dependency tree until one of these is reached, and source sets
  // don't link at all.
  bool can_link_libs = target_->IsFinal();

  if (dep->output_type() == Target::SOURCE_SET) {
    // Source sets have their object files linked into final targets
    // (shared libraries, executables, and complete static
    // libraries). Intermediate static libraries and other source sets
    // just forward the dependency, otherwise the files in the source
    // set can easily get linked more than once which will cause
    // multiple definition errors.
    if (can_link_libs)
      AddSourceSetObjectFiles(dep, extra_object_files);

    // Add the source set itself as a non-linkable dependency on the current
    // target. This will make sure that anything the source set's stamp file
    // depends on (like data deps) are also built before the current target
    // can be complete. Otherwise, these will be skipped since this target
    // will depend only on the source set's object files.
    non_linkable_deps->push_back(dep);
  } else if (can_link_libs && dep->IsLinkable()) {
    linkable_deps->push_back(dep);
  } else {
    non_linkable_deps->push_back(dep);
  }
}

void NinjaBinaryTargetWriter::WriteOrderOnlyDependencies(
    const UniqueVector<const Target*>& non_linkable_deps) {
  if (!non_linkable_deps.empty()) {
    out_ << " ||";

    // Non-linkable targets.
    for (const auto& non_linkable_dep : non_linkable_deps) {
      out_ << " ";
      path_output_.WriteFile(out_, non_linkable_dep->dependency_output_file());
    }
  }
}

OutputFile NinjaBinaryTargetWriter::GetWindowsPCHFile(
    Toolchain::ToolType tool_type) const {
  // Use "obj/{dir}/{target_name}_{lang}.pch" which ends up
  // looking like "obj/chrome/browser/browser.cc.pch"
  OutputFile ret = GetTargetOutputDirAsOutputFile(target_);
  ret.value().append(target_->label().name());
  ret.value().push_back('_');
  ret.value().append(GetPCHLangForToolType(tool_type));
  ret.value().append(".pch");

  return ret;
}

bool NinjaBinaryTargetWriter::CheckForDuplicateObjectFiles(
    const std::vector<OutputFile>& files) const {
  base::hash_set<std::string> set;
  for (const auto& file : files) {
    if (!set.insert(file.value()).second) {
      Err err(
          target_->defined_from(),
          "Duplicate object file",
          "The target " + target_->label().GetUserVisibleName(false) +
          "\ngenerates two object files with the same name:\n  " +
          file.value() + "\n"
          "\n"
          "It could be you accidentally have a file listed twice in the\n"
          "sources. Or, depending on how your toolchain maps sources to\n"
          "object files, two source files with the same name in different\n"
          "directories could map to the same object file.\n"
          "\n"
          "In the latter case, either rename one of the files or move one of\n"
          "the sources to a separate source_set to avoid them both being in\n"
          "the same target.");
      g_scheduler->FailWithError(err);
      return false;
    }
  }
  return true;
}

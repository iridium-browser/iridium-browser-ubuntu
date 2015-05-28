// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_toolchain_writer.h"

#include <fstream>

#include "base/files/file_util.h"
#include "base/strings/stringize_macros.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/ninja_utils.h"
#include "tools/gn/settings.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"
#include "tools/gn/trace.h"

namespace {

const char kIndent[] = "  ";

}  // namespace

NinjaToolchainWriter::NinjaToolchainWriter(
    const Settings* settings,
    const Toolchain* toolchain,
    const std::vector<const Target*>& targets,
    std::ostream& out)
    : settings_(settings),
      toolchain_(toolchain),
      targets_(targets),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(),
                   settings_->build_settings()->root_path_utf8(),
                   ESCAPE_NINJA) {
}

NinjaToolchainWriter::~NinjaToolchainWriter() {
}

void NinjaToolchainWriter::Run() {
  WriteRules();
  WriteSubninjas();
}

// static
bool NinjaToolchainWriter::RunAndWriteFile(
    const Settings* settings,
    const Toolchain* toolchain,
    const std::vector<const Target*>& targets) {
  base::FilePath ninja_file(settings->build_settings()->GetFullPath(
      GetNinjaFileForToolchain(settings)));
  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE, FilePathToUTF8(ninja_file));

  base::CreateDirectory(ninja_file.DirName());

  std::ofstream file;
  file.open(FilePathToUTF8(ninja_file).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (file.fail())
    return false;

  NinjaToolchainWriter gen(settings, toolchain, targets, file);
  gen.Run();
  return true;
}

void NinjaToolchainWriter::WriteRules() {
  std::string rule_prefix = GetNinjaRulePrefixForToolchain(settings_);

  for (int i = Toolchain::TYPE_NONE + 1; i < Toolchain::TYPE_NUMTYPES; i++) {
    Toolchain::ToolType tool_type = static_cast<Toolchain::ToolType>(i);
    const Tool* tool = toolchain_->GetTool(tool_type);
    if (tool)
      WriteToolRule(tool_type, tool, rule_prefix);
  }
  out_ << std::endl;
}

void NinjaToolchainWriter::WriteToolRule(const Toolchain::ToolType type,
                                         const Tool* tool,
                                         const std::string& rule_prefix) {
  out_ << "rule " << rule_prefix << Toolchain::ToolTypeToName(type)
       << std::endl;

  // Rules explicitly include shell commands, so don't try to escape.
  EscapeOptions options;
  options.mode = ESCAPE_NINJA_PREFORMATTED_COMMAND;

  CHECK(!tool->command().empty()) << "Command should not be empty";
  WriteRulePattern("command", tool->command(), options);

  WriteRulePattern("description", tool->description(), options);
  WriteRulePattern("rspfile", tool->rspfile(), options);
  WriteRulePattern("rspfile_content", tool->rspfile_content(), options);

  if (tool->depsformat() == Tool::DEPS_GCC) {
    // GCC-style deps require a depfile.
    if (!tool->depfile().empty()) {
      WriteRulePattern("depfile", tool->depfile(), options);
      out_ << kIndent << "deps = gcc" << std::endl;
    }
  } else if (tool->depsformat() == Tool::DEPS_MSVC) {
    // MSVC deps don't have a depfile.
    out_ << kIndent << "deps = msvc" << std::endl;
  }

  // The link pool applies to linker tools. Don't count TYPE_ALINK since
  // static libraries are not generally intensive to write.
  if (type == Toolchain::TYPE_SOLINK || type == Toolchain::TYPE_LINK)
    out_ << kIndent << "pool = link_pool\n";

  if (tool->restat())
    out_ << kIndent << "restat = 1" << std::endl;
}

void NinjaToolchainWriter::WriteRulePattern(const char* name,
                                            const SubstitutionPattern& pattern,
                                            const EscapeOptions& options) {
  if (pattern.empty())
    return;
  out_ << kIndent << name << " = ";
  SubstitutionWriter::WriteWithNinjaVariables(pattern, options, out_);
  out_ << std::endl;
}

void NinjaToolchainWriter::WriteSubninjas() {
  // Write subninja commands for each generated target.
  for (const auto& target : targets_) {
    OutputFile ninja_file(target->settings()->build_settings(),
                          GetNinjaFileForTarget(target));
    out_ << "subninja ";
    path_output_.WriteFile(out_, ninja_file);
    out_ << std::endl;
  }
  out_ << std::endl;
}

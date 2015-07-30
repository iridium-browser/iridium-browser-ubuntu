// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_BUILD_WRITER_H_
#define TOOLS_GN_NINJA_BUILD_WRITER_H_

#include <iosfwd>
#include <set>
#include <vector>

#include "tools/gn/path_output.h"

class BuildSettings;
class Err;
class Settings;
class Target;
class Toolchain;

// Generates the toplevel "build.ninja" file. This references the individual
// toolchain files and lists all input .gn files as dependencies of the
// build itself.
class NinjaBuildWriter {
 public:
  static bool RunAndWriteFile(
      const BuildSettings* settings,
      const std::vector<const Settings*>& all_settings,
      const Toolchain* default_toolchain,
      const std::vector<const Target*>& default_toolchain_targets,
      Err* err);

 private:
  NinjaBuildWriter(const BuildSettings* settings,
                   const std::vector<const Settings*>& all_settings,
                   const Toolchain* default_toolchain,
                   const std::vector<const Target*>& default_toolchain_targets,
                   std::ostream& out,
                   std::ostream& dep_out);
  ~NinjaBuildWriter();

  bool Run(Err* err);

  void WriteNinjaRules();
  void WriteLinkPool();
  void WriteSubninjas();
  bool WritePhonyAndAllRules(Err* err);

  // Writes a phony rule for the given target with the given name. Adds the new
  // name to the given set. If the name is already in the set, does nothing.
  void WritePhonyRule(const Target* target,
                      const OutputFile& target_file,
                      const std::string& phony_name,
                      std::set<std::string>* written_rules);

  const BuildSettings* build_settings_;
  std::vector<const Settings*> all_settings_;
  const Toolchain* default_toolchain_;
  std::vector<const Target*> default_toolchain_targets_;
  std::ostream& out_;
  std::ostream& dep_out_;
  PathOutput path_output_;

  DISALLOW_COPY_AND_ASSIGN(NinjaBuildWriter);
};

#endif  // TOOLS_GN_NINJA_BUILD_WRITER_H_


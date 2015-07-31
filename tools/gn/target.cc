// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/target.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/substitution_writer.h"

namespace {

typedef std::set<const Config*> ConfigSet;

// Merges the public configs from the given target to the given config list.
void MergePublicConfigsFrom(const Target* from_target,
                            UniqueVector<LabelConfigPair>* dest) {
  const UniqueVector<LabelConfigPair>& pub = from_target->public_configs();
  dest->Append(pub.begin(), pub.end());
}

// Like MergePublicConfigsFrom above except does the "all dependent" ones. This
// additionally adds all configs to the all_dependent_configs_ of the dest
// target given in *all_dest.
void MergeAllDependentConfigsFrom(const Target* from_target,
                                  UniqueVector<LabelConfigPair>* dest,
                                  UniqueVector<LabelConfigPair>* all_dest) {
  for (const auto& pair : from_target->all_dependent_configs()) {
    all_dest->push_back(pair);
    dest->push_back(pair);
  }
}

Err MakeTestOnlyError(const Target* from, const Target* to) {
  return Err(from->defined_from(), "Test-only dependency not allowed.",
      from->label().GetUserVisibleName(false) + "\n"
      "which is NOT marked testonly can't depend on\n" +
      to->label().GetUserVisibleName(false) + "\n"
      "which is marked testonly. Only targets with \"testonly = true\"\n"
      "can depend on other test-only targets.\n"
      "\n"
      "Either mark it test-only or don't do this dependency.");
}

Err MakeStaticLibDepsError(const Target* from, const Target* to) {
  return Err(from->defined_from(),
             "Complete static libraries can't depend on static libraries.",
             from->label().GetUserVisibleName(false) +
                 "\n"
                 "which is a complete static library can't depend on\n" +
                 to->label().GetUserVisibleName(false) +
                 "\n"
                 "which is a static library.\n"
                 "\n"
                 "Use source sets for intermediate targets instead.");
}

}  // namespace

Target::Target(const Settings* settings, const Label& label)
    : Item(settings, label),
      output_type_(UNKNOWN),
      all_headers_public_(true),
      check_includes_(true),
      complete_static_lib_(false),
      testonly_(false),
      toolchain_(nullptr) {
}

Target::~Target() {
}

// static
const char* Target::GetStringForOutputType(OutputType type) {
  switch (type) {
    case UNKNOWN:
      return "Unknown";
    case GROUP:
      return "Group";
    case EXECUTABLE:
      return "Executable";
    case SHARED_LIBRARY:
      return "Shared library";
    case STATIC_LIBRARY:
      return "Static library";
    case SOURCE_SET:
      return "Source set";
    case COPY_FILES:
      return "Copy";
    case ACTION:
      return "Action";
    case ACTION_FOREACH:
      return "ActionForEach";
    default:
      return "";
  }
}

Target* Target::AsTarget() {
  return this;
}

const Target* Target::AsTarget() const {
  return this;
}

bool Target::OnResolved(Err* err) {
  DCHECK(output_type_ != UNKNOWN);
  DCHECK(toolchain_) << "Toolchain should have been set before resolving.";

  // Copy our own dependent configs to the list of configs applying to us.
  configs_.Append(all_dependent_configs_.begin(), all_dependent_configs_.end());
  MergePublicConfigsFrom(this, &configs_);

  // Copy our own libs and lib_dirs to the final set. This will be from our
  // target and all of our configs. We do this specially since these must be
  // inherited through the dependency tree (other flags don't work this way).
  for (ConfigValuesIterator iter(this); !iter.done(); iter.Next()) {
    const ConfigValues& cur = iter.cur();
    all_lib_dirs_.append(cur.lib_dirs().begin(), cur.lib_dirs().end());
    all_libs_.append(cur.libs().begin(), cur.libs().end());
  }

  PullDependentTargets();
  PullForwardedDependentConfigs();
  PullRecursiveHardDeps();

  FillOutputFiles();

  if (!CheckVisibility(err))
    return false;
  if (!CheckTestonly(err))
    return false;
  if (!CheckNoNestedStaticLibs(err))
    return false;

  return true;
}

bool Target::IsLinkable() const {
  return output_type_ == STATIC_LIBRARY || output_type_ == SHARED_LIBRARY;
}

bool Target::IsFinal() const {
  return output_type_ == EXECUTABLE || output_type_ == SHARED_LIBRARY ||
         (output_type_ == STATIC_LIBRARY && complete_static_lib_);
}

DepsIteratorRange Target::GetDeps(DepsIterationType type) const {
  if (type == DEPS_LINKED) {
    return DepsIteratorRange(DepsIterator(
        &public_deps_, &private_deps_, nullptr));
  }
  // All deps.
  return DepsIteratorRange(DepsIterator(
      &public_deps_, &private_deps_, &data_deps_));
}

std::string Target::GetComputedOutputName(bool include_prefix) const {
  DCHECK(toolchain_)
      << "Toolchain must be specified before getting the computed output name.";

  const std::string& name = output_name_.empty() ? label().name()
                                                 : output_name_;

  std::string result;
  if (include_prefix) {
    const Tool* tool = toolchain_->GetToolForTargetFinalOutput(this);
    const std::string& prefix = tool->output_prefix();
    // Only add the prefix if the name doesn't already have it.
    if (!StartsWithASCII(name, prefix, true))
      result = prefix;
  }

  result.append(name);
  return result;
}

bool Target::SetToolchain(const Toolchain* toolchain, Err* err) {
  DCHECK(!toolchain_);
  DCHECK_NE(UNKNOWN, output_type_);
  toolchain_ = toolchain;

  const Tool* tool = toolchain->GetToolForTargetFinalOutput(this);
  if (tool)
    return true;

  // Tool not specified for this target type.
  if (err) {
    *err = Err(defined_from(), "This target uses an undefined tool.",
        base::StringPrintf(
            "The target %s\n"
            "of type \"%s\"\n"
            "uses toolchain %s\n"
            "which doesn't have the tool \"%s\" defined.\n\n"
            "Alas, I can not continue.",
            label().GetUserVisibleName(false).c_str(),
            GetStringForOutputType(output_type_),
            label().GetToolchainLabel().GetUserVisibleName(false).c_str(),
            Toolchain::ToolTypeToName(
                toolchain->GetToolTypeForTargetFinalOutput(this)).c_str()));
  }
  return false;
}

void Target::PullDependentTarget(const Target* dep, bool is_public) {
  MergeAllDependentConfigsFrom(dep, &configs_, &all_dependent_configs_);
  MergePublicConfigsFrom(dep, &configs_);

  // Direct dependent libraries.
  if (dep->output_type() == STATIC_LIBRARY ||
      dep->output_type() == SHARED_LIBRARY ||
      dep->output_type() == SOURCE_SET)
    inherited_libraries_.Append(dep, is_public);

  if (dep->output_type() == SHARED_LIBRARY) {
    // Shared library dependendencies are inherited across public shared
    // library boundaries.
    //
    // In this case:
    //   EXE -> INTERMEDIATE_SHLIB --[public]--> FINAL_SHLIB
    // The EXE will also link to to FINAL_SHLIB. The public dependeny means
    // that the EXE can use the headers in FINAL_SHLIB so the FINAL_SHLIB
    // will need to appear on EXE's link line.
    //
    // However, if the dependency is private:
    //   EXE -> INTERMEDIATE_SHLIB --[private]--> FINAL_SHLIB
    // the dependency will not be propogated because INTERMEDIATE_SHLIB is
    // not granting permission to call functiosn from FINAL_SHLIB. If EXE
    // wants to use functions (and link to) FINAL_SHLIB, it will need to do
    // so explicitly.
    //
    // Static libraries and source sets aren't inherited across shared
    // library boundaries because they will be linked into the shared
    // library.
    inherited_libraries_.AppendPublicSharedLibraries(
        dep->inherited_libraries(), is_public);
  } else if (!dep->IsFinal()) {
    // The current target isn't linked, so propogate linked deps and
    // libraries up the dependency tree.
    inherited_libraries_.AppendInherited(dep->inherited_libraries(), is_public);

    // Inherited library settings.
    all_lib_dirs_.append(dep->all_lib_dirs());
    all_libs_.append(dep->all_libs());
  }
}

void Target::PullDependentTargets() {
  for (const auto& dep : public_deps_)
    PullDependentTarget(dep.ptr, true);
  for (const auto& dep : private_deps_)
    PullDependentTarget(dep.ptr, false);
}

void Target::PullForwardedDependentConfigs() {
  // Pull public configs from each of our dependency's public deps.
  for (const auto& dep : public_deps_)
    PullForwardedDependentConfigsFrom(dep.ptr);

  // Forward public configs if explicitly requested.
  for (const auto& dep : forward_dependent_configs_) {
    const Target* from_target = dep.ptr;

    // The forward_dependent_configs_ must be in the deps (public or private)
    // already, so we don't need to bother copying to our configs, only
    // forwarding.
    DCHECK(std::find_if(private_deps_.begin(), private_deps_.end(),
                        LabelPtrPtrEquals<Target>(from_target)) !=
               private_deps_.end() ||
           std::find_if(public_deps_.begin(), public_deps_.end(),
                        LabelPtrPtrEquals<Target>(from_target)) !=
               public_deps_.end());

    PullForwardedDependentConfigsFrom(from_target);
  }
}

void Target::PullForwardedDependentConfigsFrom(const Target* from) {
  public_configs_.Append(from->public_configs().begin(),
                         from->public_configs().end());
}

void Target::PullRecursiveHardDeps() {
  for (const auto& pair : GetDeps(DEPS_LINKED)) {
    if (pair.ptr->hard_dep())
      recursive_hard_deps_.insert(pair.ptr);

    // Android STL doesn't like insert(begin, end) so do it manually.
    // TODO(brettw) this can be changed to
    // insert(iter.target()->begin(), iter.target()->end())
    // when Android uses a better STL.
    for (std::set<const Target*>::const_iterator cur =
             pair.ptr->recursive_hard_deps().begin();
         cur != pair.ptr->recursive_hard_deps().end(); ++cur)
      recursive_hard_deps_.insert(*cur);
  }
}

void Target::FillOutputFiles() {
  const Tool* tool = toolchain_->GetToolForTargetFinalOutput(this);
  switch (output_type_) {
    case GROUP:
    case SOURCE_SET:
    case COPY_FILES:
    case ACTION:
    case ACTION_FOREACH: {
      // These don't get linked to and use stamps which should be the first
      // entry in the outputs. These stamps are named
      // "<target_out_dir>/<targetname>.stamp".
      dependency_output_file_ = GetTargetOutputDirAsOutputFile(this);
      dependency_output_file_.value().append(GetComputedOutputName(true));
      dependency_output_file_.value().append(".stamp");
      break;
    }
    case EXECUTABLE:
      // Executables don't get linked to, but the first output is used for
      // dependency management.
      CHECK_GE(tool->outputs().list().size(), 1u);
      dependency_output_file_ =
          SubstitutionWriter::ApplyPatternToLinkerAsOutputFile(
              this, tool, tool->outputs().list()[0]);
      break;
    case STATIC_LIBRARY:
      // Static libraries both have dependencies and linking going off of the
      // first output.
      CHECK(tool->outputs().list().size() >= 1);
      link_output_file_ = dependency_output_file_ =
          SubstitutionWriter::ApplyPatternToLinkerAsOutputFile(
              this, tool, tool->outputs().list()[0]);
      break;
    case SHARED_LIBRARY:
      CHECK(tool->outputs().list().size() >= 1);
      if (tool->link_output().empty() && tool->depend_output().empty()) {
        // Default behavior, use the first output file for both.
        link_output_file_ = dependency_output_file_ =
            SubstitutionWriter::ApplyPatternToLinkerAsOutputFile(
                this, tool, tool->outputs().list()[0]);
      } else {
        // Use the tool-specified ones.
        if (!tool->link_output().empty()) {
          link_output_file_ =
              SubstitutionWriter::ApplyPatternToLinkerAsOutputFile(
                  this, tool, tool->link_output());
        }
        if (!tool->depend_output().empty()) {
          dependency_output_file_ =
              SubstitutionWriter::ApplyPatternToLinkerAsOutputFile(
                  this, tool, tool->depend_output());
        }
      }
      break;
    case UNKNOWN:
    default:
      NOTREACHED();
  }
}

bool Target::CheckVisibility(Err* err) const {
  for (const auto& pair : GetDeps(DEPS_ALL)) {
    if (!Visibility::CheckItemVisibility(this, pair.ptr, err))
      return false;
  }
  return true;
}

bool Target::CheckTestonly(Err* err) const {
  // If the current target is marked testonly, it can include both testonly
  // and non-testonly targets, so there's nothing to check.
  if (testonly())
    return true;

  // Verify no deps have "testonly" set.
  for (const auto& pair : GetDeps(DEPS_ALL)) {
    if (pair.ptr->testonly()) {
      *err = MakeTestOnlyError(this, pair.ptr);
      return false;
    }
  }

  return true;
}

bool Target::CheckNoNestedStaticLibs(Err* err) const {
  // If the current target is not a complete static library, it can depend on
  // static library targets with no problem.
  if (!(output_type() == Target::STATIC_LIBRARY && complete_static_lib()))
    return true;

  // Verify no deps are static libraries.
  for (const auto& pair : GetDeps(DEPS_ALL)) {
    if (pair.ptr->output_type() == Target::STATIC_LIBRARY) {
      *err = MakeStaticLibDepsError(this, pair.ptr);
      return false;
    }
  }

  // Verify no inherited libraries are static libraries.
  for (const auto& lib : inherited_libraries().GetOrdered()) {
    if (lib->output_type() == Target::STATIC_LIBRARY) {
      *err = MakeStaticLibDepsError(this, lib);
      return false;
    }
  }
  return true;
}

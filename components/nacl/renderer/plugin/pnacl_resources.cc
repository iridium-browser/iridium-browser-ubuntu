// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/plugin/pnacl_resources.h"

#include <vector>

#include "components/nacl/renderer/plugin/plugin.h"
#include "components/nacl/renderer/plugin/utility.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_errors.h"

namespace plugin {

namespace {

static const char kPnaclBaseUrl[] = "chrome://pnacl-translator/";

std::string GetFullUrl(const std::string& partial_url) {
  return std::string(kPnaclBaseUrl) + GetNaClInterface()->GetSandboxArch() +
         "/" + partial_url;
}

}  // namespace

PnaclResources::PnaclResources(Plugin* plugin, bool use_subzero)
    : plugin_(plugin), use_subzero_(use_subzero) {
  for (PnaclResourceEntry& entry : resources_) {
    entry.file_info = kInvalidNaClFileInfo;
  }
}

PnaclResources::~PnaclResources() {
  for (PnaclResourceEntry& entry : resources_) {
    if (entry.file_info.handle != PP_kInvalidFileHandle)
      CloseFileHandle(entry.file_info.handle);
  }
}

const std::string& PnaclResources::GetUrl(ResourceType type) const {
  size_t index = static_cast<size_t>(type);
  if (index < NUM_TYPES) {
    return resources_[index].tool_name;
  }
  // TODO(jvoung): Use NOTREACHED() from base/logging.h once
  // we are able to use base/logging.h without conflicting
  // with NaCl macros.
  DCHECK(false && "Index out of bounds");
  // Return a dummy tool name.
  return resources_[index].tool_name;
}

PP_NaClFileInfo PnaclResources::TakeFileInfo(ResourceType type) {
  size_t index = static_cast<size_t>(type);
  if (index >= NUM_TYPES) {
    DCHECK(false && "Index out of bounds");
    return kInvalidNaClFileInfo;
  }
  PP_NaClFileInfo to_return = resources_[index].file_info;
  resources_[index].file_info = kInvalidNaClFileInfo;
  return to_return;
}

bool PnaclResources::ReadResourceInfo() {
  PP_Var pp_llc_tool_name_var;
  PP_Var pp_ld_tool_name_var;
  PP_Var pp_subzero_tool_name_var;
  if (!plugin_->nacl_interface()->GetPnaclResourceInfo(
          plugin_->pp_instance(), &pp_llc_tool_name_var, &pp_ld_tool_name_var,
          &pp_subzero_tool_name_var)) {
    return false;
  }
  pp::Var llc_tool_name(pp::PASS_REF, pp_llc_tool_name_var);
  pp::Var ld_tool_name(pp::PASS_REF, pp_ld_tool_name_var);
  pp::Var subzero_tool_name(pp::PASS_REF, pp_subzero_tool_name_var);
  resources_[LLC].tool_name = GetFullUrl(llc_tool_name.AsString());
  resources_[LD].tool_name = GetFullUrl(ld_tool_name.AsString());
  resources_[SUBZERO].tool_name = GetFullUrl(subzero_tool_name.AsString());
  return true;
}


bool PnaclResources::StartLoad() {
  PLUGIN_PRINTF(("PnaclResources::StartLoad\n"));

  // Do a blocking load of each of the resources.
  std::vector<ResourceType> to_load;
  if (use_subzero_) {
    to_load.push_back(SUBZERO);
  } else {
    to_load.push_back(LLC);
  }
  to_load.push_back(LD);
  bool all_valid = true;
  for (ResourceType t : to_load) {
    plugin_->nacl_interface()->GetReadExecPnaclFd(
        resources_[t].tool_name.c_str(), &resources_[t].file_info);
    all_valid =
        all_valid && resources_[t].file_info.handle != PP_kInvalidFileHandle;
  }
  return all_valid;
}

}  // namespace plugin

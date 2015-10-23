// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/disk_cache_dir_policy_handler.h"

#include "base/files/file_path.h"
#include "base/prefs/pref_value_map.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "policy/policy_constants.h"

namespace policy {

DiskCacheDirPolicyHandler::DiskCacheDirPolicyHandler()
    : TypeCheckingPolicyHandler(key::kDiskCacheDir, base::Value::TYPE_STRING) {}

DiskCacheDirPolicyHandler::~DiskCacheDirPolicyHandler() {}

void DiskCacheDirPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  base::FilePath::StringType string_value;
  if (value && value->GetAsString(&string_value)) {
    base::FilePath::StringType expanded_value =
        policy::path_parser::ExpandPathVariables(string_value);
    prefs->SetValue(prefs::kDiskCacheDir,
                    make_scoped_ptr(new base::StringValue(expanded_value)));
  }
}

}  // namespace policy

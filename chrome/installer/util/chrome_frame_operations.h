// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CHROME_FRAME_OPERATIONS_H_
#define CHROME_INSTALLER_UTIL_CHROME_FRAME_OPERATIONS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/installer/util/product_operations.h"

namespace installer {

// Operations specific to Chrome Frame; see ProductOperations for general info.
class ChromeFrameOperations : public ProductOperations {
 public:
  ChromeFrameOperations() {}

  void ReadOptions(const MasterPreferences& prefs,
                   std::set<base::string16>* options) const override;

  void ReadOptions(const base::CommandLine& uninstall_command,
                   std::set<base::string16>* options) const override;

  void AddKeyFiles(const std::set<base::string16>& options,
                   std::vector<base::FilePath>* key_files) const override;

  void AddComDllList(const std::set<base::string16>& options,
                     std::vector<base::FilePath>* com_dll_list) const override;

  void AppendProductFlags(const std::set<base::string16>& options,
                          base::CommandLine* cmd_line) const override;

  void AppendRenameFlags(const std::set<base::string16>& options,
                         base::CommandLine* cmd_line) const override;

  bool SetChannelFlags(const std::set<base::string16>& options,
                       bool set,
                       ChannelInfo* channel_info) const override;

  bool ShouldCreateUninstallEntry(
      const std::set<base::string16>& options) const override;

  void AddDefaultShortcutProperties(
      BrowserDistribution* dist,
      const base::FilePath& target_exe,
      ShellUtil::ShortcutProperties* properties) const override;

  void LaunchUserExperiment(const base::FilePath& setup_path,
                            const std::set<base::string16>& options,
                            InstallStatus status,
                            bool system_level) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeFrameOperations);
};

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_CHROME_FRAME_OPERATIONS_H_

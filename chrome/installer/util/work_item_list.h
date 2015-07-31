// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_WORK_ITEM_LIST_H_
#define CHROME_INSTALLER_UTIL_WORK_ITEM_LIST_H_

#include <windows.h>

#include <list>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/installer/util/work_item.h"

namespace base {
class FilePath;
}

// A WorkItem subclass that recursively contains a list of WorkItems. Thus it
// provides functionalities to carry out or roll back the sequence of actions
// defined by the list of WorkItems it contains.
// The WorkItems are executed in the same order as they are added to the list.
class WorkItemList : public WorkItem {
 public:
  ~WorkItemList() override;

  // Execute the WorkItems in the same order as they are added to the list.
  // It aborts as soon as one WorkItem fails.
  bool Do() override;

  // Rollback the WorkItems in the reverse order as they are executed.
  void Rollback() override;

  // Add a WorkItem to the list.
  // A WorkItem can only be added to the list before the list's DO() is called.
  // Once a WorkItem is added to the list. The list owns the WorkItem.
  virtual void AddWorkItem(WorkItem* work_item);

  // Add a CallbackWorkItem that invokes a callback.
  virtual WorkItem* AddCallbackWorkItem(
      base::Callback<bool(const CallbackWorkItem&)> callback);

  // Add a CopyTreeWorkItem to the list of work items.
  // See the NOTE in the documentation for the CopyTreeWorkItem class for
  // special considerations regarding |temp_dir|.
  virtual WorkItem* AddCopyTreeWorkItem(
      const std::wstring& source_path,
      const std::wstring& dest_path,
      const std::wstring& temp_dir,
      CopyOverWriteOption overwrite_option,
      const std::wstring& alternative_path = L"");

  // Add a CreateDirWorkItem that creates a directory at the given path.
  virtual WorkItem* AddCreateDirWorkItem(const base::FilePath& path);

  // Add a CreateRegKeyWorkItem that creates a registry key at the given
  // path.
  virtual WorkItem* AddCreateRegKeyWorkItem(HKEY predefined_root,
                                            const std::wstring& path,
                                            REGSAM wow64_access);

  // Add a DeleteRegKeyWorkItem that deletes a registry key from the given
  // path.
  virtual WorkItem* AddDeleteRegKeyWorkItem(HKEY predefined_root,
                                            const std::wstring& path,
                                            REGSAM wow64_access);

  // Add a DeleteRegValueWorkItem that deletes registry value of type REG_SZ
  // or REG_DWORD.
  virtual WorkItem* AddDeleteRegValueWorkItem(HKEY predefined_root,
                                              const std::wstring& key_path,
                                              REGSAM wow64_access,
                                              const std::wstring& value_name);

  // Add a DeleteTreeWorkItem that recursively deletes a file system
  // hierarchy at the given root path. A key file can be optionally specified
  // by key_path.
  virtual WorkItem* AddDeleteTreeWorkItem(
      const base::FilePath& root_path,
      const base::FilePath& temp_path,
      const std::vector<base::FilePath>& key_paths);

  // Same as above but without support for key files.
  virtual WorkItem* AddDeleteTreeWorkItem(const base::FilePath& root_path,
                                          const base::FilePath& temp_path);

  // Add a MoveTreeWorkItem to the list of work items.
  virtual WorkItem* AddMoveTreeWorkItem(const std::wstring& source_path,
                                        const std::wstring& dest_path,
                                        const std::wstring& temp_dir,
                                        MoveTreeOption duplicate_option);

  // Add a SetRegValueWorkItem that sets a registry value with REG_SZ type
  // at the key with specified path.
  virtual WorkItem* AddSetRegValueWorkItem(HKEY predefined_root,
                                           const std::wstring& key_path,
                                           REGSAM wow64_access,
                                           const std::wstring& value_name,
                                           const std::wstring& value_data,
                                           bool overwrite);

  // Add a SetRegValueWorkItem that sets a registry value with REG_DWORD type
  // at the key with specified path.
  virtual WorkItem* AddSetRegValueWorkItem(HKEY predefined_root,
                                           const std::wstring& key_path,
                                           REGSAM wow64_access,
                                           const std::wstring& value_name,
                                           DWORD value_data,
                                           bool overwrite);

  // Add a SetRegValueWorkItem that sets a registry value with REG_QWORD type
  // at the key with specified path.
  virtual WorkItem* AddSetRegValueWorkItem(HKEY predefined_root,
                                           const std::wstring& key_path,
                                           REGSAM wow64_access,
                                           const std::wstring& value_name,
                                           int64 value_data,
                                           bool overwrite);

  // Add a SetRegValueWorkItem that sets a registry value based on the value
  // provided by |get_value_callback| given the existing value under
  // |key_path\value_name|.
  virtual WorkItem* AddSetRegValueWorkItem(
      HKEY predefined_root,
      const std::wstring& key_path,
      REGSAM wow64_access,
      const std::wstring& value_name,
      const WorkItem::GetValueFromExistingCallback& get_value_callback);

  // Add a SelfRegWorkItem that registers or unregisters a DLL at the
  // specified path. If user_level_registration is true, then alternate
  // registration and unregistration entry point names will be used.
  virtual WorkItem* AddSelfRegWorkItem(const std::wstring& dll_path,
                                       bool do_register,
                                       bool user_level_registration);

 protected:
  friend class WorkItem;

  typedef std::list<WorkItem*> WorkItems;
  typedef WorkItems::iterator WorkItemIterator;

  enum ListStatus {
    // List has not been executed. Ok to add new WorkItem.
    ADD_ITEM,
    // List has been executed. Can not add new WorkItem.
    LIST_EXECUTED,
    // List has been executed and rolled back. No further action is acceptable.
    LIST_ROLLED_BACK
  };

  WorkItemList();

  ListStatus status_;

  // The list of WorkItems, in the order of them being added.
  WorkItems list_;

  // The list of executed WorkItems, in the reverse order of them being
  // executed.
  WorkItems executed_list_;
};

// A specialization of WorkItemList that executes items in the list on a
// best-effort basis.  Failure of individual items to execute does not prevent
// subsequent items from being executed.
// Also, as the class name suggests, Rollback is not possible.
class NoRollbackWorkItemList : public WorkItemList {
 public:
  ~NoRollbackWorkItemList() override;

  // Execute the WorkItems in the same order as they are added to the list.
  // If a WorkItem fails, the function will return failure but all other
  // WorkItems will still be executed.
  bool Do() override;

  // No-op.
  void Rollback() override;
};

#endif  // CHROME_INSTALLER_UTIL_WORK_ITEM_LIST_H_

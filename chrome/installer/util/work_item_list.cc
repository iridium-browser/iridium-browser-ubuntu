// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/work_item_list.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/installer/util/callback_work_item.h"
#include "chrome/installer/util/copy_tree_work_item.h"
#include "chrome/installer/util/create_dir_work_item.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/delete_reg_key_work_item.h"
#include "chrome/installer/util/delete_reg_value_work_item.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/move_tree_work_item.h"
#include "chrome/installer/util/self_reg_work_item.h"
#include "chrome/installer/util/set_reg_value_work_item.h"

WorkItemList::~WorkItemList() {
  for (WorkItemIterator itr = list_.begin(); itr != list_.end(); ++itr) {
    delete (*itr);
  }
  for (WorkItemIterator itr = executed_list_.begin();
       itr != executed_list_.end(); ++itr) {
    delete (*itr);
  }
}

WorkItemList::WorkItemList()
    : status_(ADD_ITEM) {
}

bool WorkItemList::Do() {
  if (status_ != ADD_ITEM)
    return false;

  bool result = true;
  while (!list_.empty()) {
    WorkItem* work_item = list_.front();
    list_.pop_front();
    executed_list_.push_front(work_item);
    if (!work_item->Do()) {
      LOG(ERROR) << "item execution failed " << work_item->log_message();
      result = false;
      break;
    }
  }

  if (result)
    VLOG(1) << "list execution succeeded";

  status_ = LIST_EXECUTED;
  return result;
}

void WorkItemList::Rollback() {
  if (status_ != LIST_EXECUTED)
    return;

  for (WorkItemIterator itr = executed_list_.begin();
       itr != executed_list_.end(); ++itr) {
    (*itr)->Rollback();
  }

  status_ = LIST_ROLLED_BACK;
  return;
}

void WorkItemList::AddWorkItem(WorkItem* work_item) {
  DCHECK(status_ == ADD_ITEM);
  list_.push_back(work_item);
}

WorkItem* WorkItemList::AddCallbackWorkItem(
    base::Callback<bool(const CallbackWorkItem&)> callback) {
  WorkItem* item = WorkItem::CreateCallbackWorkItem(callback);
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddCopyTreeWorkItem(
    const std::wstring& source_path,
    const std::wstring& dest_path,
    const std::wstring& temp_dir,
    CopyOverWriteOption overwrite_option,
    const std::wstring& alternative_path) {
  WorkItem* item = WorkItem::CreateCopyTreeWorkItem(
      base::FilePath(source_path),
      base::FilePath(dest_path),
      base::FilePath(temp_dir),
      overwrite_option,
      base::FilePath(alternative_path));
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddCreateDirWorkItem(const base::FilePath& path) {
  WorkItem* item = WorkItem::CreateCreateDirWorkItem(path);
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddCreateRegKeyWorkItem(HKEY predefined_root,
                                                const std::wstring& path,
                                                REGSAM wow64_access) {
  WorkItem* item =
      WorkItem::CreateCreateRegKeyWorkItem(predefined_root, path, wow64_access);
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddDeleteRegKeyWorkItem(HKEY predefined_root,
                                                const std::wstring& path,
                                                REGSAM wow64_access) {
  WorkItem* item =
      WorkItem::CreateDeleteRegKeyWorkItem(predefined_root, path, wow64_access);
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddDeleteRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name) {
  WorkItem* item = WorkItem::CreateDeleteRegValueWorkItem(
      predefined_root, key_path, wow64_access, value_name);
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddDeleteTreeWorkItem(
    const base::FilePath& root_path,
    const base::FilePath& temp_path,
    const std::vector<base::FilePath>& key_paths) {
  WorkItem* item = WorkItem::CreateDeleteTreeWorkItem(root_path, temp_path,
                                                      key_paths);
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddDeleteTreeWorkItem(const base::FilePath& root_path,
                                              const base::FilePath& temp_path) {
  std::vector<base::FilePath> no_key_files;
  return AddDeleteTreeWorkItem(root_path, temp_path, no_key_files);
}

WorkItem* WorkItemList::AddMoveTreeWorkItem(const std::wstring& source_path,
                                            const std::wstring& dest_path,
                                            const std::wstring& temp_dir,
                                            MoveTreeOption duplicate_option) {
  WorkItem* item = WorkItem::CreateMoveTreeWorkItem(base::FilePath(source_path),
                                                    base::FilePath(dest_path),
                                                    base::FilePath(temp_dir),
                                                    duplicate_option);
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddSetRegValueWorkItem(HKEY predefined_root,
                                               const std::wstring& key_path,
                                               REGSAM wow64_access,
                                               const std::wstring& value_name,
                                               const std::wstring& value_data,
                                               bool overwrite) {
  WorkItem* item = WorkItem::CreateSetRegValueWorkItem(predefined_root,
                                                       key_path,
                                                       wow64_access,
                                                       value_name,
                                                       value_data,
                                                       overwrite);
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddSetRegValueWorkItem(HKEY predefined_root,
                                               const std::wstring& key_path,
                                               REGSAM wow64_access,
                                               const std::wstring& value_name,
                                               DWORD value_data,
                                               bool overwrite) {
  WorkItem* item = WorkItem::CreateSetRegValueWorkItem(predefined_root,
                                                       key_path,
                                                       wow64_access,
                                                       value_name,
                                                       value_data,
                                                       overwrite);
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddSetRegValueWorkItem(HKEY predefined_root,
                                               const std::wstring& key_path,
                                               REGSAM wow64_access,
                                               const std::wstring& value_name,
                                               int64 value_data,
                                               bool overwrite) {
  WorkItem* item = reinterpret_cast<WorkItem*>(
      WorkItem::CreateSetRegValueWorkItem(predefined_root,
                                          key_path,
                                          wow64_access,
                                          value_name,
                                          value_data,
                                          overwrite));
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddSetRegValueWorkItem(
    HKEY predefined_root,
    const std::wstring& key_path,
    REGSAM wow64_access,
    const std::wstring& value_name,
    const WorkItem::GetValueFromExistingCallback& get_value_callback) {
  WorkItem* item = WorkItem::CreateSetRegValueWorkItem(predefined_root,
                                                       key_path,
                                                       wow64_access,
                                                       value_name,
                                                       get_value_callback);
  AddWorkItem(item);
  return item;
}

WorkItem* WorkItemList::AddSelfRegWorkItem(const std::wstring& dll_path,
                                           bool do_register,
                                           bool user_level_registration) {
  WorkItem* item = WorkItem::CreateSelfRegWorkItem(dll_path, do_register,
                                                   user_level_registration);
  AddWorkItem(item);
  return item;
}

////////////////////////////////////////////////////////////////////////////////
NoRollbackWorkItemList::~NoRollbackWorkItemList() {
}

bool NoRollbackWorkItemList::Do() {
  if (status_ != ADD_ITEM)
    return false;

  bool result = true;
  while (!list_.empty()) {
    WorkItem* work_item = list_.front();
    list_.pop_front();
    executed_list_.push_front(work_item);
    work_item->set_ignore_failure(true);
    if (!work_item->Do()) {
      LOG(ERROR) << "NoRollbackWorkItemList: item execution failed "
                 << work_item->log_message();
      result = false;
    }
  }

  if (result)
    VLOG(1) << "NoRollbackWorkItemList: list execution succeeded";

  status_ = LIST_EXECUTED;
  return result;
}

void NoRollbackWorkItemList::Rollback() {
  // Ignore rollback.
}

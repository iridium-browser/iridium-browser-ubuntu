// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_CONDITIONAL_WORK_ITEM_LIST_H_
#define CHROME_INSTALLER_UTIL_CONDITIONAL_WORK_ITEM_LIST_H_

#include "chrome/installer/util/work_item_list.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"

// A WorkItemList subclass that permits conditionally executing a set of
// WorkItems.
class ConditionalWorkItemList : public WorkItemList {
 public:
  explicit ConditionalWorkItemList(Condition* condition);
  ~ConditionalWorkItemList() override;

  // If condition_->ShouldRun() returns true, then execute the items in this
  // list and return true iff they all succeed. If condition_->ShouldRun()
  // returns false, does nothing and returns true.
  bool Do() override;

  // Does a rollback of the items (if any) that were run in Do.
  void Rollback() override;

 protected:
  // Pointer to a Condition that is used to determine whether to run this
  // WorkItemList.
  scoped_ptr<Condition> condition_;
};


// Pre-defined conditions:
//------------------------------------------------------------------------------
class ConditionRunIfFileExists : public WorkItem::Condition {
 public:
  explicit ConditionRunIfFileExists(const base::FilePath& key_path)
      : key_path_(key_path) {}
  bool ShouldRun() const override;

 private:
  base::FilePath key_path_;
};

// Condition class that inverts the ShouldRun result of another Condition.
// This class assumes ownership of original_condition.
class Not : public WorkItem::Condition {
 public:
  explicit Not(WorkItem::Condition* original_condition);
  ~Not() override;

  bool ShouldRun() const override;

 private:
  scoped_ptr<WorkItem::Condition> original_condition_;
};

#endif  // CHROME_INSTALLER_UTIL_CONDITIONAL_WORK_ITEM_LIST_H_

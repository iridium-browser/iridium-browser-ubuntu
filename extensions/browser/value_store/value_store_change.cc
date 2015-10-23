// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/value_store_change.h"

#include "base/json/json_writer.h"
#include "base/logging.h"

// static
std::string ValueStoreChange::ToJson(
    const ValueStoreChangeList& changes) {
  base::DictionaryValue changes_value;
  for (ValueStoreChangeList::const_iterator it = changes.begin();
      it != changes.end(); ++it) {
    base::DictionaryValue* change_value = new base::DictionaryValue();
    if (it->old_value()) {
      change_value->Set("oldValue", it->old_value()->DeepCopy());
    }
    if (it->new_value()) {
      change_value->Set("newValue", it->new_value()->DeepCopy());
    }
    changes_value.SetWithoutPathExpansion(it->key(), change_value);
  }
  std::string json;
  base::JSONWriter::Write(changes_value, &json);
  return json;
}

ValueStoreChange::ValueStoreChange(
    const std::string& key, base::Value* old_value, base::Value* new_value)
    : inner_(new Inner(key, old_value, new_value)) {}

ValueStoreChange::~ValueStoreChange() {}

const std::string& ValueStoreChange::key() const {
  DCHECK(inner_.get());
  return inner_->key_;
}

const base::Value* ValueStoreChange::old_value() const {
  DCHECK(inner_.get());
  return inner_->old_value_.get();
}

const base::Value* ValueStoreChange::new_value() const {
  DCHECK(inner_.get());
  return inner_->new_value_.get();
}

ValueStoreChange::Inner::Inner(
    const std::string& key, base::Value* old_value, base::Value* new_value)
    : key_(key), old_value_(old_value), new_value_(new_value) {}

ValueStoreChange::Inner::~Inner() {}

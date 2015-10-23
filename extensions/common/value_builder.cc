// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/value_builder.h"

#include "base/json/json_writer.h"

namespace extensions {

// DictionaryBuilder

DictionaryBuilder::DictionaryBuilder() : dict_(new base::DictionaryValue) {}

DictionaryBuilder::DictionaryBuilder(const base::DictionaryValue& init)
    : dict_(init.DeepCopy()) {}

DictionaryBuilder::~DictionaryBuilder() {}

std::string DictionaryBuilder::ToJSON() const {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *dict_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          int in_value) {
  dict_->SetWithoutPathExpansion(path, new base::FundamentalValue(in_value));
  return *this;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          double in_value) {
  dict_->SetWithoutPathExpansion(path, new base::FundamentalValue(in_value));
  return *this;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          const std::string& in_value) {
  dict_->SetWithoutPathExpansion(path, new base::StringValue(in_value));
  return *this;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          const base::string16& in_value) {
  dict_->SetWithoutPathExpansion(path, new base::StringValue(in_value));
  return *this;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          DictionaryBuilder& in_value) {
  dict_->SetWithoutPathExpansion(path, in_value.Build().release());
  return *this;
}

DictionaryBuilder& DictionaryBuilder::Set(const std::string& path,
                                          ListBuilder& in_value) {
  dict_->SetWithoutPathExpansion(path, in_value.Build().release());
  return *this;
}

DictionaryBuilder& DictionaryBuilder::SetBoolean(
    const std::string& path, bool in_value) {
  dict_->SetWithoutPathExpansion(path, new base::FundamentalValue(in_value));
  return *this;
}

// ListBuilder

ListBuilder::ListBuilder() : list_(new base::ListValue) {}
ListBuilder::ListBuilder(const base::ListValue& init) : list_(init.DeepCopy()) {
}
ListBuilder::~ListBuilder() {}

ListBuilder& ListBuilder::Append(int in_value) {
  list_->Append(new base::FundamentalValue(in_value));
  return *this;
}

ListBuilder& ListBuilder::Append(double in_value) {
  list_->Append(new base::FundamentalValue(in_value));
  return *this;
}

ListBuilder& ListBuilder::Append(const std::string& in_value) {
  list_->Append(new base::StringValue(in_value));
  return *this;
}

ListBuilder& ListBuilder::Append(const base::string16& in_value) {
  list_->Append(new base::StringValue(in_value));
  return *this;
}

ListBuilder& ListBuilder::Append(DictionaryBuilder& in_value) {
  list_->Append(in_value.Build().release());
  return *this;
}

ListBuilder& ListBuilder::Append(ListBuilder& in_value) {
  list_->Append(in_value.Build().release());
  return *this;
}

ListBuilder& ListBuilder::AppendBoolean(bool in_value) {
  list_->Append(new base::FundamentalValue(in_value));
  return *this;
}

}  // namespace extensions

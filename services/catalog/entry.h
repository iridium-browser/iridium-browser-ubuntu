// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CATALOG_ENTRY_H_
#define SERVICES_CATALOG_ENTRY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "services/catalog/public/interfaces/catalog.mojom.h"
#include "services/shell/public/cpp/capabilities.h"
#include "services/shell/public/interfaces/resolver.mojom.h"

namespace base {
class DictionaryValue;
}

namespace catalog {

// Static information about a service package known to the Catalog.
class Entry {
 public:
  Entry();
  explicit Entry(const std::string& name);
  ~Entry();

  std::unique_ptr<base::DictionaryValue> Serialize() const;

  // If the constructed Entry is a package that provides other Entrys, the
  // caller must assume ownership of the tree of Entrys by enumerating
  // services().
  static std::unique_ptr<Entry> Deserialize(const base::DictionaryValue& value);

  bool ProvidesClass(const std::string& clazz) const;

  bool operator==(const Entry& other) const;
  bool operator<(const Entry& other) const;

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }
  const base::FilePath& path() const { return path_; }
  void set_path(const base::FilePath& path) { path_ = path; }
  const std::string& qualifier() const { return qualifier_; }
  void set_qualifier(const std::string& qualifier) { qualifier_ = qualifier; }
  const std::string& display_name() const { return display_name_; }
  void set_display_name(const std::string& display_name) {
    display_name_ = display_name;
  }
  const shell::CapabilitySpec& capabilities() const { return capabilities_; }
  void set_capabilities(const shell::CapabilitySpec& capabilities) {
    capabilities_ = capabilities;
  }
  const Entry* package() const { return package_; }
  void set_package(Entry* package) { package_ = package; }

  std::vector<std::unique_ptr<Entry>> TakeChildren() {
    return std::move(children_);
  }

 private:
  std::string name_;
  base::FilePath path_;
  std::string qualifier_;
  std::string display_name_;
  shell::CapabilitySpec capabilities_;
  Entry* package_ = nullptr;
  std::vector<std::unique_ptr<Entry>> children_;
};

}  // namespace catalog

namespace mojo {
template <>
struct TypeConverter<shell::mojom::ResolveResultPtr, catalog::Entry> {
  static shell::mojom::ResolveResultPtr Convert(const catalog::Entry& input);
};

template<>
struct TypeConverter<catalog::mojom::EntryPtr, catalog::Entry> {
  static catalog::mojom::EntryPtr Convert(const catalog::Entry& input);
};

}  // namespace mojo

#endif  // SERVICES_CATALOG_ENTRY_H_

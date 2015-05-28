// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEBVIEW_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEBVIEW_INFO_H_

#include <string>

#include "base/memory/scoped_vector.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

class PartitionItem;

// A class to hold the <webview> accessible extension resources
// that may be specified in the manifest of an extension using the
// "webview" key.
class WebviewInfo : public Extension::ManifestData {
 public:
  // Define out of line constructor/destructor to please Clang.
  WebviewInfo(const std::string& extension_id);
  ~WebviewInfo() override;

  // Returns true if the specified resource is web accessible and the extension
  // matches the manifest's extension.
  bool IsResourceWebviewAccessible(const Extension* extension,
                                   const std::string& partition_id,
                                   const std::string& relative_path) const;

  void AddPartitionItem(scoped_ptr<PartitionItem> item);

 private:
  std::string extension_id_;
  ScopedVector<PartitionItem> partition_items_;
};

// Parses the "webview" manifest key.
class WebviewHandler : public ManifestHandler {
 public:
  WebviewHandler();
  ~WebviewHandler() override;

  bool Parse(Extension* extension, base::string16* error) override;

 private:
  const std::vector<std::string> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(WebviewHandler);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEBVIEW_INFO_H_

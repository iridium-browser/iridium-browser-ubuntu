// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_
#define COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_

#include "base/macros.h"
#include "components/filesystem/directory_impl.h"
#include "components/filesystem/file_system_impl.h"
#include "components/filesystem/lock_table.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"

namespace mojo {
class Connector;
}

namespace filesystem {

class FileSystemApp : public shell::Service,
                      public shell::InterfaceFactory<mojom::FileSystem> {
 public:
  FileSystemApp();
  ~FileSystemApp() override;

 private:
  // Gets the system specific toplevel profile directory.
  static base::FilePath GetUserDataDir();

  // |shell::Service| override:
  void OnStart(const shell::Identity& identity) override;
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // |InterfaceFactory<Files>| implementation:
  void Create(const shell::Identity& remote_identity,
              mojo::InterfaceRequest<mojom::FileSystem> request) override;

  tracing::Provider tracing_;

  scoped_refptr<LockTable> lock_table_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemApp);
};

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_FILE_SYSTEM_APP_H_

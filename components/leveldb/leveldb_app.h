// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_LEVELDB_APP_H_
#define COMPONENTS_LEVELDB_LEVELDB_APP_H_

#include <memory>

#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"

namespace leveldb {

class LevelDBApp : public shell::Service,
                   public shell::InterfaceFactory<mojom::LevelDBService> {
 public:
  LevelDBApp();
  ~LevelDBApp() override;

 private:
  // |Service| override:
  void OnStart(const shell::Identity& identity) override;
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // |InterfaceFactory<mojom::LevelDBService>| implementation:
  void Create(const shell::Identity& remote_identity,
              leveldb::mojom::LevelDBServiceRequest request) override;

  tracing::Provider tracing_;
  std::unique_ptr<mojom::LevelDBService> service_;
  mojo::BindingSet<mojom::LevelDBService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBApp);
};

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_LEVELDB_APP_H_

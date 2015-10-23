// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/offline_pages/offline_page_metadata_store_impl.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/proto/offline_pages.pb.h"
#include "content/public/browser/browser_thread.h"

namespace offline_pages {

OfflinePageModelFactory::OfflinePageModelFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflinePageModel",
          BrowserContextDependencyManager::GetInstance()) {
}

// static
OfflinePageModelFactory* OfflinePageModelFactory::GetInstance() {
  return Singleton<OfflinePageModelFactory>::get();
}

// static
OfflinePageModel* OfflinePageModelFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  if (context->IsOffTheRecord())
    return nullptr;

  return static_cast<OfflinePageModel*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService* OfflinePageModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      content::BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken());

  scoped_ptr<leveldb_proto::ProtoDatabaseImpl<OfflinePageEntry>> database(
      new leveldb_proto::ProtoDatabaseImpl<OfflinePageEntry>(
          background_task_runner));

  base::FilePath store_path;
  CHECK(PathService::Get(chrome::DIR_OFFLINE_PAGE_METADATA, &store_path));
  scoped_ptr<OfflinePageMetadataStoreImpl> metadata_store(
      new OfflinePageMetadataStoreImpl(database.Pass(), store_path));

  return new OfflinePageModel(metadata_store.Pass(), background_task_runner);
}

content::BrowserContext* OfflinePageModelFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace offline_pages


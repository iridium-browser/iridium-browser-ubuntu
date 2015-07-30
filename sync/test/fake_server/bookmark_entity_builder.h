// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_FAKE_SERVER_BOOKMARK_ENTITY_BUILDER_H_
#define SYNC_TEST_FAKE_SERVER_BOOKMARK_ENTITY_BUILDER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/test/fake_server/entity_builder.h"
#include "sync/test/fake_server/fake_server_entity.h"
#include "url/gurl.h"

namespace fake_server {

// Builder for BookmarkEntity objects.
class BookmarkEntityBuilder : public EntityBuilder {
 public:
  BookmarkEntityBuilder(const std::string& title,
                        const GURL& url,
                        const std::string& originator_cache_guid,
                        const std::string& originator_client_item_id);

  ~BookmarkEntityBuilder() override;

  // Sets the parend ID of the bookmark to be built. If this is not called,
  // the bookmark will be included in the bookmarks bar.
  void SetParentId(const std::string& parent_id);

  // EntityBuilder
  scoped_ptr<FakeServerEntity> Build() override;

 private:
  // The bookmark's URL.
  GURL url_;
  std::string originator_cache_guid_;
  std::string originator_client_item_id_;

  // The ID of the parent bookmark folder.
  std::string parent_id_;
};

}  // namespace fake_server

#endif  // SYNC_TEST_FAKE_SERVER_BOOKMARK_ENTITY_BUILDER_H_

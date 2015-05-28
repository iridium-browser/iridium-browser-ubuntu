// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_TEST_IMAGE_STORE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_TEST_IMAGE_STORE_H_

#include "components/enhanced_bookmarks/image_store.h"

// The TestImageStore is an implementation of ImageStore that keeps all its
// data in memory. When deallocated all the associations are lost.
// Used in tests.
class TestImageStore : public ImageStore {
 public:
  TestImageStore();
  bool HasKey(const GURL& page_url) override;
  void Insert(
      const GURL& page_url,
      scoped_refptr<enhanced_bookmarks::ImageRecord> image_record) override;
  void Erase(const GURL& page_url) override;
  scoped_refptr<enhanced_bookmarks::ImageRecord> Get(
      const GURL& page_url) override;
  gfx::Size GetSize(const GURL& page_url) override;
  void GetAllPageUrls(std::set<GURL>* urls) override;
  void ClearAll() override;
  int64 GetStoreSizeInBytes() override;

 protected:
  ~TestImageStore() override;

 private:
  typedef std::map<const GURL, scoped_refptr<enhanced_bookmarks::ImageRecord>>
      ImageMap;
  ImageMap store_;

  DISALLOW_COPY_AND_ASSIGN(TestImageStore);
};

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_TEST_IMAGE_STORE_H_

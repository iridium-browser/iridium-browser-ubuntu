// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/test_image_store.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

TestImageStore::TestImageStore() {
}

bool TestImageStore::HasKey(const GURL& page_url) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  return store_.find(page_url) != store_.end();
}

void TestImageStore::Insert(
    const GURL& page_url,
    scoped_refptr<enhanced_bookmarks::ImageRecord> image_record) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  Erase(page_url);
  store_.insert(std::make_pair(page_url, image_record));
}

void TestImageStore::Erase(const GURL& page_url) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  store_.erase(page_url);
}

scoped_refptr<enhanced_bookmarks::ImageRecord> TestImageStore::Get(
    const GURL& page_url) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  if (store_.find(page_url) == store_.end()) {
    return scoped_refptr<enhanced_bookmarks::ImageRecord>(
        new enhanced_bookmarks::ImageRecord());
  }

  return store_[page_url];
}

gfx::Size TestImageStore::GetSize(const GURL& page_url) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  ImageMap::const_iterator pair_enumerator = store_.find(page_url);
  if (pair_enumerator == store_.end())
    return gfx::Size();

  return store_[page_url]->image->Size();
}

void TestImageStore::GetAllPageUrls(std::set<GURL>* urls) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(urls->empty());

  for (ImageMap::const_iterator it = store_.begin(); it != store_.end(); ++it)
    urls->insert(it->first);
}

void TestImageStore::ClearAll() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());

  store_.clear();
}

int64 TestImageStore::GetStoreSizeInBytes() {
  // Not 100% accurate, but it's for testing so the actual value is not
  // important.
  int64 size = sizeof(store_);
  for (ImageMap::const_iterator it = store_.begin(); it != store_.end(); ++it) {
    size += sizeof(it->first);
    size += it->first.spec().length();
    size += sizeof(it->second);
    SkBitmap bitmap = it->second->image->AsBitmap();
    size += bitmap.getSize();
    size += it->second->url.spec().length();
    size += sizeof(it->second->dominant_color);
  }
  return size;
}

TestImageStore::~TestImageStore() {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
}

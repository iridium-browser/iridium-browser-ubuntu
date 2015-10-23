// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_THUMBNAIL_THUMBNAIL_CACHE_H_
#define CHROME_BROWSER_ANDROID_THUMBNAIL_THUMBNAIL_CACHE_H_

#include <list>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/android/thumbnail/scoped_ptr_expiring_cache.h"
#include "chrome/browser/android/thumbnail/thumbnail.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/resources/ui_resource_client_android.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "url/gurl.h"

namespace base {
class File;
class Time;
}

namespace content {
class ContentViewCore;
}

typedef std::list<TabId> TabIdList;

class ThumbnailCacheObserver {
 public:
  virtual void OnFinishedThumbnailRead(TabId tab_id) = 0;
};

class ThumbnailCache : ThumbnailDelegate {
 public:
  ThumbnailCache(size_t default_cache_size,
                 size_t approximation_cache_size,
                 size_t compression_queue_max_size,
                 size_t write_queue_max_size,
                 bool use_approximation_thumbnail);

  ~ThumbnailCache() override;

  void SetUIResourceProvider(ui::UIResourceProvider* ui_resource_provider);

  void AddThumbnailCacheObserver(ThumbnailCacheObserver* observer);
  void RemoveThumbnailCacheObserver(
      ThumbnailCacheObserver* observer);

  void Put(TabId tab_id, const SkBitmap& bitmap, float thumbnail_scale);
  void Remove(TabId tab_id);
  Thumbnail* Get(TabId tab_id, bool force_disk_read, bool allow_approximation);

  void RemoveFromDiskAtAndAboveId(TabId min_id);
  void InvalidateThumbnailIfChanged(TabId tab_id, const GURL& url);
  bool CheckAndUpdateThumbnailMetaData(TabId tab_id, const GURL& url);
  void UpdateVisibleIds(const TabIdList& priority);
  void DecompressThumbnailFromFile(
      TabId tab_id,
      const base::Callback<void(bool, SkBitmap)>&
          post_decompress_callback);

  // ThumbnailDelegate implementation
  void InvalidateCachedThumbnail(Thumbnail* thumbnail) override;
  static base::FilePath GetCacheDirectory();
  static base::FilePath GetFilePath(TabId tab_id);

 private:
  class ThumbnailMetaData {
   public:
    ThumbnailMetaData();
    ThumbnailMetaData(const base::Time& current_time, const GURL& url);
    const GURL& url() const { return url_; }
    base::Time capture_time() const { return capture_time_; }

   private:
    base::Time capture_time_;
    GURL url_;
  };

  typedef ScopedPtrExpiringCache<TabId, Thumbnail> ExpiringThumbnailCache;
  typedef base::hash_map<TabId, ThumbnailMetaData> ThumbnailMetaDataMap;

  void RemoveFromDisk(TabId tab_id);
  static void RemoveFromDiskTask(TabId tab_id);
  static void RemoveFromDiskAtAndAboveIdTask(TabId min_id);
  void WriteThumbnailIfNecessary(TabId tab_id,
                                 skia::RefPtr<SkPixelRef> compressed_data,
                                 float scale,
                                 const gfx::Size& content_size);
  void CompressThumbnailIfNecessary(TabId tab_id,
                                    const base::Time& time_stamp,
                                    const SkBitmap& bitmap,
                                    float scale);
  void ReadNextThumbnail();
  void MakeSpaceForNewItemIfNecessary(TabId tab_id);
  void RemoveFromReadQueue(TabId tab_id);
  static void WriteTask(TabId tab_id,
                        skia::RefPtr<SkPixelRef> compressed_data,
                        float scale,
                        const gfx::Size& content_size,
                        const base::Callback<void()>& post_write_task);
  void PostWriteTask();
  static void CompressionTask(
      SkBitmap raw_data,
      gfx::Size encoded_size,
      const base::Callback<void(skia::RefPtr<SkPixelRef>, const gfx::Size&)>&
          post_compression_task);
  void PostCompressionTask(TabId tab_id,
                           const base::Time& time_stamp,
                           float scale,
                           skia::RefPtr<SkPixelRef> compressed_data,
                           const gfx::Size& content_size);
  static void DecompressionTask(
      const base::Callback<void(bool, SkBitmap)>&
          post_decompress_callback,
          skia::RefPtr<SkPixelRef> compressed_data,
          float scale,
          const gfx::Size& encoded_size);
  static void ReadTask(
      bool decompress,
      TabId tab_id,
      const base::Callback<
          void(skia::RefPtr<SkPixelRef>, float, const gfx::Size&)>&
          post_read_task);
  void PostReadTask(TabId tab_id,
                    skia::RefPtr<SkPixelRef> compressed_data,
                    float scale,
                    const gfx::Size& content_size);
  void NotifyObserversOfThumbnailRead(TabId tab_id);
  void RemoveOnMatchedTimeStamp(TabId tab_id, const base::Time& time_stamp);
  static std::pair<SkBitmap, float> CreateApproximation(const SkBitmap& bitmap,
                                                        float scale);

  const size_t compression_queue_max_size_;
  const size_t write_queue_max_size_;
  const bool use_approximation_thumbnail_;

  size_t compression_tasks_count_;
  size_t write_tasks_count_;
  bool read_in_progress_;

  ExpiringThumbnailCache cache_;
  ExpiringThumbnailCache approximation_cache_;
  base::ObserverList<ThumbnailCacheObserver> observers_;
  ThumbnailMetaDataMap thumbnail_meta_data_;
  TabIdList read_queue_;
  TabIdList visible_ids_;

  ui::UIResourceProvider* ui_resource_provider_;

  base::WeakPtrFactory<ThumbnailCache> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailCache);
};

#endif  // CHROME_BROWSER_ANDROID_THUMBNAIL_THUMBNAIL_CACHE_H_

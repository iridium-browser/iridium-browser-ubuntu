// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_content_manager.h"

#include <android/bitmap.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "cc/layers/layer.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/thumbnail/thumbnail.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/TabContentManager_jni.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace {

const size_t kMaxReadbacks = 1;
typedef base::Callback<void(float, const SkBitmap&)> TabReadbackCallback;

}  // namespace

namespace chrome {
namespace android {

class TabContentManager::TabReadbackRequest {
 public:
  TabReadbackRequest(jobject content_view_core,
                     float thumbnail_scale,
                     const TabReadbackCallback& end_callback)
      : thumbnail_scale_(thumbnail_scale),
        end_callback_(end_callback),
        drop_after_readback_(false),
        weak_factory_(this) {
    JNIEnv* env = base::android::AttachCurrentThread();
    j_content_view_core_.Reset(env, content_view_core);
  }

  virtual ~TabReadbackRequest() {}

  void Run() {
    JNIEnv* env = base::android::AttachCurrentThread();
    content::ReadbackRequestCallback result_callback =
        base::Bind(&TabReadbackRequest::OnFinishGetTabThumbnailBitmap,
                   weak_factory_.GetWeakPtr());

    if (j_content_view_core_.is_null()) {
      result_callback.Run(SkBitmap(), content::READBACK_FAILED);
      return;
    }

    content::ContentViewCore* view =
        content::ContentViewCore::GetNativeContentViewCore(
            env, j_content_view_core_.obj());

    if (!view) {
      result_callback.Run(SkBitmap(), content::READBACK_FAILED);
      return;
    }

    DCHECK(view->GetWebContents());
    view->GetWebContents()->GetRenderViewHost()->LockBackingStore();

    SkColorType color_type = kN32_SkColorType;

    // Calling this method with an empty rect will return a bitmap of the size
    // of the content.
    view->GetScaledContentBitmap(thumbnail_scale_, color_type, gfx::Rect(),
                                 result_callback);
  }

  void OnFinishGetTabThumbnailBitmap(const SkBitmap& bitmap,
                                     content::ReadbackResponse response) {
    DCHECK(!j_content_view_core_.is_null());
    JNIEnv* env = base::android::AttachCurrentThread();
    content::ContentViewCore* view =
        content::ContentViewCore::GetNativeContentViewCore(
            env, j_content_view_core_.obj());

    if (view) {
      DCHECK(view->GetWebContents());
      view->GetWebContents()->GetRenderViewHost()->UnlockBackingStore();
    }

    // TODO(jdduke): Tailor response to different failure values appropriately.
    if (response != content::READBACK_SUCCESS || drop_after_readback_) {
      end_callback_.Run(0.f, SkBitmap());
      return;
    }

    SkBitmap result_bitmap = bitmap;
    result_bitmap.setImmutable();
    end_callback_.Run(thumbnail_scale_, bitmap);
  }

  void SetToDropAfterReadback() { drop_after_readback_ = true; }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_content_view_core_;
  const float thumbnail_scale_;
  TabReadbackCallback end_callback_;
  bool drop_after_readback_;

  base::WeakPtrFactory<TabReadbackRequest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabReadbackRequest);
};

// static
TabContentManager* TabContentManager::FromJavaObject(jobject jobj) {
  if (!jobj)
    return NULL;
  return reinterpret_cast<TabContentManager*>(
      Java_TabContentManager_getNativePtr(base::android::AttachCurrentThread(),
                                          jobj));
}

TabContentManager::TabContentManager(JNIEnv* env,
                                     jobject obj,
                                     jint default_cache_size,
                                     jint approximation_cache_size,
                                     jint compression_queue_max_size,
                                     jint write_queue_max_size,
                                     jboolean use_approximation_thumbnail)
    : weak_java_tab_content_manager_(env, obj), weak_factory_(this) {
  thumbnail_cache_ = make_scoped_ptr(new ThumbnailCache(
      (size_t)default_cache_size, (size_t)approximation_cache_size,
      (size_t)compression_queue_max_size, (size_t)write_queue_max_size,
      use_approximation_thumbnail));
  thumbnail_cache_->AddThumbnailCacheObserver(this);
}

TabContentManager::~TabContentManager() {
}

void TabContentManager::Destroy(JNIEnv* env, jobject obj) {
  thumbnail_cache_->RemoveThumbnailCacheObserver(this);
  delete this;
}

void TabContentManager::SetUIResourceProvider(JNIEnv* env,
                                              jobject obj,
                                              jlong ui_resource_provider_ptr) {
  ui::UIResourceProvider* ui_resource_provider =
      reinterpret_cast<ui::UIResourceProvider*>(ui_resource_provider_ptr);
  SetUIResourceProvider(ui_resource_provider);
}

void TabContentManager::SetUIResourceProvider(
    ui::UIResourceProvider* ui_resource_provider) {
  thumbnail_cache_->SetUIResourceProvider(ui_resource_provider);
}

scoped_refptr<cc::Layer> TabContentManager::GetLiveLayer(int tab_id) {
  scoped_refptr<cc::Layer> layer = live_layer_list_[tab_id];
  if (!layer.get())
    return NULL;

  return layer;
}

scoped_refptr<ThumbnailLayer> TabContentManager::GetStaticLayer(
    int tab_id,
    bool force_disk_read) {
  Thumbnail* thumbnail = thumbnail_cache_->Get(tab_id, force_disk_read, true);
  scoped_refptr<ThumbnailLayer> static_layer = static_layer_cache_[tab_id];

  if (!thumbnail || !thumbnail->ui_resource_id()) {
    if (static_layer.get()) {
      static_layer->layer()->RemoveFromParent();
      static_layer_cache_.erase(tab_id);
    }
    return NULL;
  }

  if (!static_layer.get()) {
    static_layer = ThumbnailLayer::Create();
    static_layer_cache_[tab_id] = static_layer;
  }

  static_layer->SetThumbnail(thumbnail);
  return static_layer;
}

void TabContentManager::AttachLiveLayer(int tab_id,
                                        scoped_refptr<cc::Layer> layer) {
  if (!layer.get())
    return;

  scoped_refptr<cc::Layer> cached_layer = live_layer_list_[tab_id];
  if (cached_layer != layer)
    live_layer_list_[tab_id] = layer;
}

void TabContentManager::DetachLiveLayer(int tab_id,
                                        scoped_refptr<cc::Layer> layer) {
  scoped_refptr<cc::Layer> current_layer = live_layer_list_[tab_id];
  if (!current_layer.get()) {
    // Empty cached layer should not exist but it is ok if it happens.
    return;
  }

  // We need to remove if we're getting a detach for our current layer or we're
  // getting a detach with NULL and we have a current layer, which means remove
  //  all layers.
  if (current_layer.get() &&
      (layer.get() == current_layer.get() || !layer.get())) {
    live_layer_list_.erase(tab_id);
  }
}

void TabContentManager::OnFinishDecompressThumbnail(int tab_id,
                                                    bool success,
                                                    SkBitmap bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (success)
    java_bitmap = gfx::ConvertToJavaBitmap(&bitmap);

  Java_TabContentManager_notifyDecompressBitmapFinished(
      env, weak_java_tab_content_manager_.get(env).obj(), tab_id,
      java_bitmap.obj());
}

jboolean TabContentManager::HasFullCachedThumbnail(JNIEnv* env,
                                                   jobject obj,
                                                   jint tab_id) {
  return thumbnail_cache_->Get(tab_id, false, false) != nullptr;
}

void TabContentManager::CacheTab(JNIEnv* env,
                                 jobject obj,
                                 jobject tab,
                                 jobject content_view_core,
                                 jfloat thumbnail_scale) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  DCHECK(tab_android);
  int tab_id = tab_android->GetAndroidId();
  GURL url = tab_android->GetURL();

  content::ContentViewCore* view =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         content_view_core);

  if (thumbnail_cache_->CheckAndUpdateThumbnailMetaData(tab_id, url)) {
    if (!view ||
        !view->GetWebContents()
             ->GetRenderViewHost()
             ->CanCopyFromBackingStore() ||
        pending_tab_readbacks_.find(tab_id) != pending_tab_readbacks_.end() ||
        pending_tab_readbacks_.size() >= kMaxReadbacks) {
      thumbnail_cache_->Remove(tab_id);
      return;
    }

    TabReadbackCallback readback_done_callback =
        base::Bind(&TabContentManager::PutThumbnailIntoCache,
                   weak_factory_.GetWeakPtr(), tab_id);
    scoped_ptr<TabReadbackRequest> readback_request =
        make_scoped_ptr(new TabReadbackRequest(
            content_view_core, thumbnail_scale, readback_done_callback));
    pending_tab_readbacks_.set(tab_id, readback_request.Pass());
    pending_tab_readbacks_.get(tab_id)->Run();
  }
}

void TabContentManager::CacheTabWithBitmap(JNIEnv* env,
                                           jobject obj,
                                           jobject tab,
                                           jobject bitmap,
                                           jfloat thumbnail_scale) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  DCHECK(tab_android);
  int tab_id = tab_android->GetAndroidId();
  GURL url = tab_android->GetURL();

  gfx::JavaBitmap java_bitmap_lock(bitmap);
  SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(java_bitmap_lock);
  skbitmap.setImmutable();

  if (thumbnail_cache_->CheckAndUpdateThumbnailMetaData(tab_id, url))
    PutThumbnailIntoCache(tab_id, thumbnail_scale, skbitmap);
}

void TabContentManager::InvalidateIfChanged(JNIEnv* env,
                                            jobject obj,
                                            jint tab_id,
                                            jstring jurl) {
  thumbnail_cache_->InvalidateThumbnailIfChanged(
      tab_id, GURL(base::android::ConvertJavaStringToUTF8(env, jurl)));
}

void TabContentManager::UpdateVisibleIds(JNIEnv* env,
                                         jobject obj,
                                         jintArray priority) {
  std::list<int> priority_ids;
  jsize length = env->GetArrayLength(priority);
  jint* ints = env->GetIntArrayElements(priority, NULL);
  for (jsize i = 0; i < length; ++i)
    priority_ids.push_back(static_cast<int>(ints[i]));

  env->ReleaseIntArrayElements(priority, ints, JNI_ABORT);
  thumbnail_cache_->UpdateVisibleIds(priority_ids);
}

void TabContentManager::RemoveTabThumbnail(JNIEnv* env,
                                           jobject obj,
                                           jint tab_id) {
  TabReadbackRequestMap::iterator readback_iter =
      pending_tab_readbacks_.find(tab_id);
  if (readback_iter != pending_tab_readbacks_.end())
    readback_iter->second->SetToDropAfterReadback();
  thumbnail_cache_->Remove(tab_id);
}

void TabContentManager::RemoveTabThumbnailFromDiskAtAndAboveId(
    JNIEnv* env,
    jobject obj,
    jint min_forbidden_id) {
  thumbnail_cache_->RemoveFromDiskAtAndAboveId(min_forbidden_id);
}

void TabContentManager::GetDecompressedThumbnail(JNIEnv* env,
                                                 jobject obj,
                                                 jint tab_id) {
  base::Callback<void(bool, SkBitmap)> decompress_done_callback =
      base::Bind(&TabContentManager::OnFinishDecompressThumbnail,
                 weak_factory_.GetWeakPtr(), reinterpret_cast<int>(tab_id));
  thumbnail_cache_->DecompressThumbnailFromFile(reinterpret_cast<int>(tab_id),
                                                decompress_done_callback);
}

void TabContentManager::OnFinishedThumbnailRead(int tab_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TabContentManager_notifyListenersOfThumbnailChange(
      env, weak_java_tab_content_manager_.get(env).obj(), tab_id);
}

void TabContentManager::PutThumbnailIntoCache(int tab_id,
                                              float thumbnail_scale,
                                              const SkBitmap& bitmap) {
  TabReadbackRequestMap::iterator readback_iter =
      pending_tab_readbacks_.find(tab_id);

  if (readback_iter != pending_tab_readbacks_.end())
    pending_tab_readbacks_.erase(tab_id);

  if (thumbnail_scale > 0 && !bitmap.empty())
    thumbnail_cache_->Put(tab_id, bitmap, thumbnail_scale);
}

bool RegisterTabContentManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env,
           jobject obj,
           jint default_cache_size,
           jint approximation_cache_size,
           jint compression_queue_max_size,
           jint write_queue_max_size,
           jboolean use_approximation_thumbnail) {
  TabContentManager* manager = new TabContentManager(
      env, obj, default_cache_size, approximation_cache_size,
      compression_queue_max_size, write_queue_max_size,
      use_approximation_thumbnail);
  return reinterpret_cast<intptr_t>(manager);
}

}  // namespace android
}  // namespace chrome

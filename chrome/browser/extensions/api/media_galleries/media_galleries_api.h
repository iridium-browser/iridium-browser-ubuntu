// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Chrome Extensions Media Galleries API functions for accessing
// user's media files, as specified in the extension API IDL.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/media_galleries/gallery_watch_manager_observer.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_scan_manager_observer.h"
#include "chrome/common/extensions/api/media_galleries.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "components/storage_monitor/media_storage_util.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace MediaGalleries = extensions::api::media_galleries;

class MediaGalleriesScanResultController;

namespace content {
class BlobHandle;
class WebContents;
}

namespace metadata {
class SafeMediaMetadataParser;
}

namespace extensions {

class Extension;

// The profile-keyed service that manages the media galleries extension API.
// Created at the same time as the Profile. This is also the event router.
class MediaGalleriesEventRouter : public BrowserContextKeyedAPI,
                                  public GalleryWatchManagerObserver,
                                  public MediaScanManagerObserver,
                                  public extensions::EventRouter::Observer {
 public:
  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<MediaGalleriesEventRouter>*
      GetFactoryInstance();

  // Convenience method to get the MediaGalleriesAPI for a profile.
  static MediaGalleriesEventRouter* Get(content::BrowserContext* context);

  bool ExtensionHasGalleryChangeListener(const std::string& extension_id) const;
  bool ExtensionHasScanProgressListener(const std::string& extension_id) const;

  // MediaScanManagerObserver implementation.
  void OnScanStarted(const std::string& extension_id) override;
  void OnScanCancelled(const std::string& extension_id) override;
  void OnScanFinished(const std::string& extension_id,
                      int gallery_count,
                      const MediaGalleryScanResult& file_counts) override;
  void OnScanError(const std::string& extension_id) override;

 private:
  friend class BrowserContextKeyedAPIFactory<MediaGalleriesEventRouter>;

  void DispatchEventToExtension(const std::string& extension_id,
                                events::HistogramValue histogram_value,
                                const std::string& event_name,
                                scoped_ptr<base::ListValue> event_args);

  explicit MediaGalleriesEventRouter(content::BrowserContext* context);
  ~MediaGalleriesEventRouter() override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "MediaGalleriesAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // GalleryWatchManagerObserver
  void OnGalleryChanged(const std::string& extension_id,
                        MediaGalleryPrefId gallery_id) override;
  void OnGalleryWatchDropped(const std::string& extension_id,
                             MediaGalleryPrefId gallery_id) override;

  // extensions::EventRouter::Observer implementation.
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // Current profile.
  Profile* profile_;

  base::WeakPtrFactory<MediaGalleriesEventRouter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaGalleriesEventRouter);
};

class MediaGalleriesGetMediaFileSystemsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.getMediaFileSystems",
                             MEDIAGALLERIES_GETMEDIAFILESYSTEMS)

 protected:
  ~MediaGalleriesGetMediaFileSystemsFunction() override;
  bool RunAsync() override;

 private:
  // Bottom half for RunAsync, invoked after the preferences is initialized.
  void OnPreferencesInit(
      MediaGalleries::GetMediaFileSystemsInteractivity interactive);

  // Always show the dialog.
  void AlwaysShowDialog(const std::vector<MediaFileSystemInfo>& filesystems);

  // If no galleries are found, show the dialog, otherwise return them.
  void ShowDialogIfNoGalleries(
      const std::vector<MediaFileSystemInfo>& filesystems);

  // Grabs galleries from the media file system registry and passes them to
  // |ReturnGalleries|.
  void GetAndReturnGalleries();

  // Returns galleries to the caller.
  void ReturnGalleries(const std::vector<MediaFileSystemInfo>& filesystems);

  // Shows the configuration dialog to edit gallery preferences.
  void ShowDialog();

  // A helper method that calls
  // MediaFileSystemRegistry::GetMediaFileSystemsForExtension().
  void GetMediaFileSystemsForExtension(const MediaFileSystemsCallback& cb);
};

class MediaGalleriesGetAllMediaFileSystemMetadataFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.getAllMediaFileSystemMetadata",
                             MEDIAGALLERIES_GETALLMEDIAFILESYSTEMMETADATA)

 protected:
  ~MediaGalleriesGetAllMediaFileSystemMetadataFunction() override;
  bool RunAsync() override;

 private:
  // Bottom half for RunAsync, invoked after the preferences is initialized.
  // Gets the list of permitted galleries and checks if they are available.
  void OnPreferencesInit();

  // Callback to run upon getting the list of available devices.
  // Sends the list of media filesystem metadata back to the extension.
  void OnGetGalleries(
      const MediaGalleryPrefIdSet& permitted_gallery_ids,
      const storage_monitor::MediaStorageUtil::DeviceIdSet* available_devices);
};

class MediaGalleriesAddUserSelectedFolderFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.addUserSelectedFolder",
                             MEDIAGALLERIES_ADDUSERSELECTEDFOLDER)

 protected:
  ~MediaGalleriesAddUserSelectedFolderFunction() override;
  bool RunAsync() override;

 private:
  // Bottom half for RunAsync, invoked after the preferences is initialized.
  void OnPreferencesInit();

  // Callback for the directory prompt request, with the input from the user.
  // If |selected_directory| is empty, then the user canceled.
  // Either handle the user canceled case or add the selected gallery.
  void OnDirectorySelected(const base::FilePath& selected_directory);

  // Callback for the directory prompt request. |pref_id| is for the gallery
  // the user just added. |filesystems| is the entire list of file systems.
  // The fsid for the file system that corresponds to |pref_id| will be
  // appended to the list of file systems returned to the caller. The
  // Javascript binding for this API will interpret the list appropriately.
  void ReturnGalleriesAndId(
      MediaGalleryPrefId pref_id,
      const std::vector<MediaFileSystemInfo>& filesystems);

  // A helper method that calls
  // MediaFileSystemRegistry::GetMediaFileSystemsForExtension().
  void GetMediaFileSystemsForExtension(const MediaFileSystemsCallback& cb);
};

class MediaGalleriesDropPermissionForMediaFileSystemFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.dropPermissionForMediaFileSystem",
                             MEDIAGALLERIES_DROPPERMISSIONFORMEDIAFILESYSTEM)

 protected:
  ~MediaGalleriesDropPermissionForMediaFileSystemFunction() override;
  bool RunAsync() override;

 private:
  // Bottom half for RunAsync, invoked after the preferences is initialized.
  void OnPreferencesInit(MediaGalleryPrefId pref_id);
};

class MediaGalleriesStartMediaScanFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.startMediaScan",
                             MEDIAGALLERIES_STARTMEDIASCAN)

 protected:
  ~MediaGalleriesStartMediaScanFunction() override;
  bool RunAsync() override;

 private:
  // Bottom half for RunAsync, invoked after the preferences is initialized.
  void OnPreferencesInit();
};

class MediaGalleriesCancelMediaScanFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.cancelMediaScan",
                             MEDIAGALLERIES_CANCELMEDIASCAN)

 protected:
  ~MediaGalleriesCancelMediaScanFunction() override;
  bool RunAsync() override;

 private:
  // Bottom half for RunAsync, invoked after the preferences is initialized.
  void OnPreferencesInit();
};

class MediaGalleriesAddScanResultsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.addScanResults",
                             MEDIAGALLERIES_ADDSCANRESULTS)

 protected:
  ~MediaGalleriesAddScanResultsFunction() override;
  bool RunAsync() override;

  // Pulled out for testing.
  virtual MediaGalleriesScanResultController* MakeDialog(
      content::WebContents* web_contents,
      const extensions::Extension& extension,
      const base::Closure& on_finish);

 private:
  // Bottom half for RunAsync, invoked after the preferences is initialized.
  void OnPreferencesInit();

  // Grabs galleries from the media file system registry and passes them to
  // ReturnGalleries().
  void GetAndReturnGalleries();

  // Returns galleries to the caller.
  void ReturnGalleries(const std::vector<MediaFileSystemInfo>& filesystems);
};

class MediaGalleriesGetMetadataFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.getMetadata",
                             MEDIAGALLERIES_GETMETADATA)

 protected:
  ~MediaGalleriesGetMetadataFunction() override;
  bool RunAsync() override;

 private:
  // Bottom half for RunAsync, invoked after the preferences is initialized.
  void OnPreferencesInit(MediaGalleries::GetMetadataType metadata_type,
                         const std::string& blob_uuid);

  void GetMetadata(MediaGalleries::GetMetadataType metadata_type,
                   const std::string& blob_uuid,
                   scoped_ptr<std::string> blob_header,
                   int64 total_blob_length);

  void OnSafeMediaMetadataParserDone(
      bool parse_success, scoped_ptr<base::DictionaryValue> result_dictionary,
      scoped_ptr<std::vector<metadata::AttachedImage> > attached_images);

  void ConstructNextBlob(
      scoped_ptr<base::DictionaryValue> result_dictionary,
      scoped_ptr<std::vector<metadata::AttachedImage> > attached_images,
      scoped_ptr<std::vector<std::string> > blob_uuids,
      scoped_ptr<content::BlobHandle> current_blob);
};

class MediaGalleriesAddGalleryWatchFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.addGalleryWatch",
                             MEDIAGALLERIES_ADDGALLERYWATCH);

 protected:
  ~MediaGalleriesAddGalleryWatchFunction() override;
  bool RunAsync() override;

 private:
  void OnPreferencesInit(const std::string& pref_id);

  // Gallery watch request handler.
  void HandleResponse(MediaGalleryPrefId gallery_id, const std::string& error);
};

class MediaGalleriesRemoveGalleryWatchFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.removeGalleryWatch",
                             MEDIAGALLERIES_REMOVEGALLERYWATCH);

 protected:
  ~MediaGalleriesRemoveGalleryWatchFunction() override;
  bool RunAsync() override;

 private:
  void OnPreferencesInit(const std::string& pref_id);
};

class MediaGalleriesGetAllGalleryWatchFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.getAllGalleryWatch",
                             MEDIAGALLERIES_GETALLGALLERYWATCH);

 protected:
  ~MediaGalleriesGetAllGalleryWatchFunction() override;
  bool RunAsync() override;

 private:
  void OnPreferencesInit();
};

class MediaGalleriesRemoveAllGalleryWatchFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("mediaGalleries.removeAllGalleryWatch",
                             MEDIAGALLERIES_REMOVEALLGALLERYWATCH);

 protected:
  ~MediaGalleriesRemoveAllGalleryWatchFunction() override;
  bool RunAsync() override;

 private:
  void OnPreferencesInit();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_GALLERIES_MEDIA_GALLERIES_API_H_

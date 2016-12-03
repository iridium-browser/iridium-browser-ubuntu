// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_file_util.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/itunes_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/native_file_util.h"
#include "storage/common/fileapi/file_system_util.h"

using storage::DirectoryEntry;

namespace itunes {

namespace {

base::File::Error MakeDirectoryFileInfo(base::File::Info* file_info) {
  base::File::Info result;
  result.is_directory = true;
  *file_info = result;
  return base::File::FILE_OK;
}

std::vector<std::string> GetVirtualPathComponents(
    const storage::FileSystemURL& url) {
  ImportedMediaGalleryRegistry* imported_registry =
      ImportedMediaGalleryRegistry::GetInstance();
  base::FilePath root = imported_registry->ImportedRoot().AppendASCII("itunes");

  DCHECK(root.IsParent(url.path()) || root == url.path());
  base::FilePath virtual_path;
  root.AppendRelativePath(url.path(), &virtual_path);

  std::vector<std::string> result;
  storage::VirtualPath::GetComponentsUTF8Unsafe(virtual_path, &result);
  return result;
}

}  // namespace

const char kITunesLibraryXML[] = "iTunes Music Library.xml";
const char kITunesMediaDir[] = "iTunes Media";
const char kITunesMusicDir[] = "Music";
const char kITunesAutoAddDir[] = "Automatically Add to iTunes";

ITunesFileUtil::ITunesFileUtil(MediaPathFilter* media_path_filter)
    : NativeMediaFileUtil(media_path_filter),
      imported_registry_(NULL),
      weak_factory_(this) {
}

ITunesFileUtil::~ITunesFileUtil() {
}

void ITunesFileUtil::GetFileInfoOnTaskRunnerThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  ITunesDataProvider* data_provider = GetDataProvider();
  // |data_provider| may be NULL if the file system was revoked before this
  // operation had a chance to run.
  if (!data_provider) {
    GetFileInfoWithFreshDataProvider(std::move(context), url, callback, false);
  } else {
    data_provider->RefreshData(
        base::Bind(&ITunesFileUtil::GetFileInfoWithFreshDataProvider,
                   weak_factory_.GetWeakPtr(), base::Passed(&context), url,
                   callback));
  }
}

void ITunesFileUtil::ReadDirectoryOnTaskRunnerThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  ITunesDataProvider* data_provider = GetDataProvider();
  // |data_provider| may be NULL if the file system was revoked before this
  // operation had a chance to run.
  if (!data_provider) {
    ReadDirectoryWithFreshDataProvider(std::move(context), url, callback,
                                       false);
  } else {
    data_provider->RefreshData(
        base::Bind(&ITunesFileUtil::ReadDirectoryWithFreshDataProvider,
                   weak_factory_.GetWeakPtr(), base::Passed(&context), url,
                   callback));
  }
}

void ITunesFileUtil::CreateSnapshotFileOnTaskRunnerThread(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  ITunesDataProvider* data_provider = GetDataProvider();
  // |data_provider| may be NULL if the file system was revoked before this
  // operation had a chance to run.
  if (!data_provider) {
    CreateSnapshotFileWithFreshDataProvider(std::move(context), url, callback,
                                            false);
  } else {
    data_provider->RefreshData(
        base::Bind(&ITunesFileUtil::CreateSnapshotFileWithFreshDataProvider,
                   weak_factory_.GetWeakPtr(), base::Passed(&context), url,
                   callback));
  }
}

// Contents of the iTunes media gallery:
//   /                                                - root directory
//   /iTunes Music Library.xml                        - library xml file
//   /iTunes Media/Automatically Add to iTunes        - auto-import directory
//   /iTunes Media/Music/<Artist>/<Album>/<Track>     - tracks
//
base::File::Error ITunesFileUtil::GetFileInfoSync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_path) {
  std::vector<std::string> components = GetVirtualPathComponents(url);

  if (components.size() == 0)
    return MakeDirectoryFileInfo(file_info);

  if (components.size() == 1 && components[0] == kITunesLibraryXML) {
    // We can't just call NativeMediaFileUtil::GetFileInfoSync() here because it
    // uses the MediaPathFilter. At this point, |library_path_| is known good
    // because GetFileInfoWithFreshDataProvider() gates access to this method.
    base::FilePath file_path = GetDataProvider()->library_path();
    if (platform_path)
      *platform_path = file_path;
    return storage::NativeFileUtil::GetFileInfo(file_path, file_info);
  }

  if (components[0] != kITunesMediaDir)
    return base::File::FILE_ERROR_NOT_FOUND;

  if (components[1] == kITunesAutoAddDir) {
    if (GetDataProvider()->auto_add_path().empty())
      return base::File::FILE_ERROR_NOT_FOUND;
    return NativeMediaFileUtil::GetFileInfoSync(context, url, file_info,
                                                platform_path);
  }

  if (components[1] == kITunesMusicDir) {
    switch (components.size()) {
      case 2:
        return MakeDirectoryFileInfo(file_info);

      case 3:
        if (GetDataProvider()->KnownArtist(components[2]))
          return MakeDirectoryFileInfo(file_info);
        break;

      case 4:
        if (GetDataProvider()->KnownAlbum(components[2], components[3]))
          return MakeDirectoryFileInfo(file_info);
        break;

      case 5: {
        base::FilePath location =
            GetDataProvider()->GetTrackLocation(components[2], components[3],
                                                components[4]);
         if (!location.empty()) {
          return NativeMediaFileUtil::GetFileInfoSync(context, url, file_info,
                                                      platform_path);
        }
        break;
      }
    }
  }

  return base::File::FILE_ERROR_NOT_FOUND;
}

base::File::Error ITunesFileUtil::ReadDirectorySync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    EntryList* file_list) {
  DCHECK(file_list->empty());
  std::vector<std::string> components = GetVirtualPathComponents(url);

  if (components.size() == 0) {
    base::File::Info xml_info;
    if (!base::GetFileInfo(GetDataProvider()->library_path(), &xml_info))
      return base::File::FILE_ERROR_IO;
    file_list->push_back(
        DirectoryEntry(kITunesLibraryXML, DirectoryEntry::FILE));
    file_list->push_back(
        DirectoryEntry(kITunesMediaDir, DirectoryEntry::DIRECTORY));
    return base::File::FILE_OK;
  }

  if (components.size() == 1 && components[0] == kITunesLibraryXML)
    return base::File::FILE_ERROR_NOT_A_DIRECTORY;

  if (components[0] != kITunesMediaDir || components.size() > 5)
    return base::File::FILE_ERROR_NOT_FOUND;

  if (components.size() == 1) {
    if (!GetDataProvider()->auto_add_path().empty()) {
      file_list->push_back(
          DirectoryEntry(kITunesAutoAddDir, DirectoryEntry::DIRECTORY));
    }
    file_list->push_back(
        DirectoryEntry(kITunesMusicDir, DirectoryEntry::DIRECTORY));
    return base::File::FILE_OK;
  }

  if (components[1] == kITunesAutoAddDir &&
      !GetDataProvider()->auto_add_path().empty()) {
    return NativeMediaFileUtil::ReadDirectorySync(context, url, file_list);
  }

  if (components[1] != kITunesMusicDir)
    return base::File::FILE_ERROR_NOT_FOUND;

  if (components.size() == 2) {
    std::set<ITunesDataProvider::ArtistName> artists =
        GetDataProvider()->GetArtistNames();
    std::set<ITunesDataProvider::ArtistName>::const_iterator it;
    for (it = artists.begin(); it != artists.end(); ++it)
      file_list->push_back(DirectoryEntry(*it, DirectoryEntry::DIRECTORY));
    return base::File::FILE_OK;
  }

  if (components.size() == 3) {
    std::set<ITunesDataProvider::AlbumName> albums =
        GetDataProvider()->GetAlbumNames(components[2]);
    if (albums.size() == 0)
      return base::File::FILE_ERROR_NOT_FOUND;
    std::set<ITunesDataProvider::AlbumName>::const_iterator it;
    for (it = albums.begin(); it != albums.end(); ++it)
      file_list->push_back(DirectoryEntry(*it, DirectoryEntry::DIRECTORY));
    return base::File::FILE_OK;
  }

  if (components.size() == 4) {
    ITunesDataProvider::Album album =
        GetDataProvider()->GetAlbum(components[2], components[3]);
    if (album.size() == 0)
      return base::File::FILE_ERROR_NOT_FOUND;
    ITunesDataProvider::Album::const_iterator it;
    for (it = album.begin(); it != album.end(); ++it) {
      base::File::Info file_info;
      if (media_path_filter()->Match(it->second) &&
          base::GetFileInfo(it->second, &file_info)) {
        file_list->push_back(DirectoryEntry(it->first, DirectoryEntry::FILE));
      }
    }
    return base::File::FILE_OK;
  }

  // At this point, the only choice is one of two errors, but figuring out
  // which one is required.
  DCHECK_EQ(4UL, components.size());
  base::FilePath location;
  location = GetDataProvider()->GetTrackLocation(components[1], components[2],
                                                 components[3]);
  if (!location.empty())
    return base::File::FILE_ERROR_NOT_A_DIRECTORY;
  return base::File::FILE_ERROR_NOT_FOUND;
}

base::File::Error ITunesFileUtil::DeleteDirectorySync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url) {
  return base::File::FILE_ERROR_SECURITY;
}

base::File::Error ITunesFileUtil::DeleteFileSync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url) {
  return base::File::FILE_ERROR_SECURITY;
}

base::File::Error ITunesFileUtil::CreateSnapshotFileSync(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_path,
    scoped_refptr<storage::ShareableFileReference>* file_ref) {
  std::vector<std::string> components = GetVirtualPathComponents(url);
  if (components.size() != 1 || components[0] != kITunesLibraryXML) {
    return NativeMediaFileUtil::CreateSnapshotFileSync(context, url, file_info,
                                                       platform_path, file_ref);
  }

  // The following code is different than
  // NativeMediaFileUtil::CreateSnapshotFileSync in that it knows that the
  // library xml file is not a directory and it doesn't run mime sniffing on the
  // file. The only way to get here is by way of
  // CreateSnapshotFileWithFreshDataProvider() so the file has already been
  // parsed and deemed valid.
  *file_ref = scoped_refptr<storage::ShareableFileReference>();
  return GetFileInfoSync(context, url, file_info, platform_path);
}

base::File::Error ITunesFileUtil::GetLocalFilePath(
    storage::FileSystemOperationContext* context,
    const storage::FileSystemURL& url,
    base::FilePath* local_file_path) {
  std::vector<std::string> components = GetVirtualPathComponents(url);

  if (components.size() == 1 && components[0] == kITunesLibraryXML) {
    *local_file_path = GetDataProvider()->library_path();
    return base::File::FILE_OK;
  }

  if (components.size() >= 2 && components[0] == kITunesMediaDir &&
      components[1] == kITunesAutoAddDir) {
    *local_file_path = GetDataProvider()->auto_add_path();
    if (local_file_path->empty())
      return base::File::FILE_ERROR_NOT_FOUND;

    for (size_t i = 2; i < components.size(); ++i) {
      *local_file_path = local_file_path->Append(
          base::FilePath::FromUTF8Unsafe(components[i]));
    }
    return base::File::FILE_OK;
  }

  // Should only get here for files, i.e. the xml file and tracks.
  if (components[0] != kITunesMediaDir || components[1] != kITunesMusicDir||
      components.size() != 5) {
    return base::File::FILE_ERROR_NOT_FOUND;
  }

  *local_file_path = GetDataProvider()->GetTrackLocation(components[2],
                                                         components[3],
                                                         components[4]);
  if (!local_file_path->empty())
    return base::File::FILE_OK;

  return base::File::FILE_ERROR_NOT_FOUND;
}

void ITunesFileUtil::GetFileInfoWithFreshDataProvider(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const GetFileInfoCallback& callback,
    bool valid_parse) {
  if (!valid_parse) {
    if (!callback.is_null()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::File::FILE_ERROR_IO,
                     base::File::Info()));
    }
    return;
  }
  NativeMediaFileUtil::GetFileInfoOnTaskRunnerThread(std::move(context), url,
                                                     callback);
}

void ITunesFileUtil::ReadDirectoryWithFreshDataProvider(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const ReadDirectoryCallback& callback,
    bool valid_parse) {
  if (!valid_parse) {
    if (!callback.is_null()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::File::FILE_ERROR_IO, EntryList(), false));
    }
    return;
  }
  NativeMediaFileUtil::ReadDirectoryOnTaskRunnerThread(std::move(context), url,
                                                       callback);
}

void ITunesFileUtil::CreateSnapshotFileWithFreshDataProvider(
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const storage::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback,
    bool valid_parse) {
  if (!valid_parse) {
    if (!callback.is_null()) {
      base::File::Info file_info;
      base::FilePath platform_path;
      scoped_refptr<storage::ShareableFileReference> file_ref;
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(callback, base::File::FILE_ERROR_IO, file_info,
                     platform_path, file_ref));
    }
    return;
  }
  NativeMediaFileUtil::CreateSnapshotFileOnTaskRunnerThread(std::move(context),
                                                            url, callback);
}

ITunesDataProvider* ITunesFileUtil::GetDataProvider() {
  if (!imported_registry_)
    imported_registry_ = ImportedMediaGalleryRegistry::GetInstance();
  return imported_registry_->ITunesDataProvider();
}

}  // namespace itunes

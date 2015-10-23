// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/extensions/extensions_handler.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "chrome/utility/media_galleries/image_metadata_extractor.h"
#include "content/public/common/content_paths.h"
#include "content/public/utility/utility_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_utility_messages.h"
#include "extensions/utility/unpacker.h"
#include "media/base/media.h"
#include "media/base/media_file_checker.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"
#include "chrome/utility/media_galleries/itunes_pref_parser_win.h"
#include "components/wifi/wifi_service.h"
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
#include "chrome/utility/media_galleries/iphoto_library_parser.h"
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/utility/media_galleries/iapps_xml_utils.h"
#include "chrome/utility/media_galleries/itunes_library_parser.h"
#include "chrome/utility/media_galleries/picasa_album_table_reader.h"
#include "chrome/utility/media_galleries/picasa_albums_indexer.h"
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

namespace extensions {

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcessIfNeeded() {
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

}  // namespace

ExtensionsHandler::ExtensionsHandler() {
  ExtensionsClient::Set(ChromeExtensionsClient::GetInstance());
}

ExtensionsHandler::~ExtensionsHandler() {
}

// static
void ExtensionsHandler::PreSandboxStartup() {
  // Initialize libexif for image metadata parsing.
  metadata::ImageMetadataExtractor::InitializeLibrary();

  // Initialize media libraries for media file validation.
  media::InitializeMediaLibrary();
}

bool ExtensionsHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionsHandler, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_CheckMediaFile, OnCheckMediaFile)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseITunesPrefXml,
                        OnParseITunesPrefXml)
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseIPhotoLibraryXmlFile,
                        OnParseIPhotoLibraryXmlFile)
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseITunesLibraryXmlFile,
                        OnParseITunesLibraryXmlFile)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParsePicasaPMPDatabase,
                        OnParsePicasaPMPDatabase)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_IndexPicasaAlbumsContents,
                        OnIndexPicasaAlbumsContents)
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GetWiFiCredentials,
                        OnGetWiFiCredentials)
#endif  // defined(OS_WIN)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled || utility_handler_.OnMessageReceived(message);
}

void ExtensionsHandler::OnCheckMediaFile(
    int64 milliseconds_of_decoding,
    const IPC::PlatformFileForTransit& media_file) {
  media::MediaFileChecker checker(
      IPC::PlatformFileForTransitToFile(media_file));
  const bool check_success = checker.Start(
      base::TimeDelta::FromMilliseconds(milliseconds_of_decoding));
  Send(new ChromeUtilityHostMsg_CheckMediaFile_Finished(check_success));
  ReleaseProcessIfNeeded();
}

#if defined(OS_WIN)
void ExtensionsHandler::OnParseITunesPrefXml(
    const std::string& itunes_xml_data) {
  base::FilePath library_path(
      itunes::FindLibraryLocationInPrefXml(itunes_xml_data));
  Send(new ChromeUtilityHostMsg_GotITunesDirectory(library_path));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
void ExtensionsHandler::OnParseIPhotoLibraryXmlFile(
    const IPC::PlatformFileForTransit& iphoto_library_file) {
  iphoto::IPhotoLibraryParser parser;
  base::File file = IPC::PlatformFileForTransitToFile(iphoto_library_file);
  bool result = parser.Parse(iapps::ReadFileAsString(file.Pass()));
  Send(new ChromeUtilityHostMsg_GotIPhotoLibrary(result, parser.library()));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
void ExtensionsHandler::OnParseITunesLibraryXmlFile(
    const IPC::PlatformFileForTransit& itunes_library_file) {
  itunes::ITunesLibraryParser parser;
  base::File file = IPC::PlatformFileForTransitToFile(itunes_library_file);
  bool result = parser.Parse(iapps::ReadFileAsString(file.Pass()));
  Send(new ChromeUtilityHostMsg_GotITunesLibrary(result, parser.library()));
  ReleaseProcessIfNeeded();
}

void ExtensionsHandler::OnParsePicasaPMPDatabase(
    const picasa::AlbumTableFilesForTransit& album_table_files) {
  picasa::AlbumTableFiles files;
  files.indicator_file =
      IPC::PlatformFileForTransitToFile(album_table_files.indicator_file);
  files.category_file =
      IPC::PlatformFileForTransitToFile(album_table_files.category_file);
  files.date_file =
      IPC::PlatformFileForTransitToFile(album_table_files.date_file);
  files.filename_file =
      IPC::PlatformFileForTransitToFile(album_table_files.filename_file);
  files.name_file =
      IPC::PlatformFileForTransitToFile(album_table_files.name_file);
  files.token_file =
      IPC::PlatformFileForTransitToFile(album_table_files.token_file);
  files.uid_file =
      IPC::PlatformFileForTransitToFile(album_table_files.uid_file);

  picasa::PicasaAlbumTableReader reader(files.Pass());
  bool parse_success = reader.Init();
  Send(new ChromeUtilityHostMsg_ParsePicasaPMPDatabase_Finished(
      parse_success, reader.albums(), reader.folders()));
  ReleaseProcessIfNeeded();
}

void ExtensionsHandler::OnIndexPicasaAlbumsContents(
    const picasa::AlbumUIDSet& album_uids,
    const std::vector<picasa::FolderINIContents>& folders_inis) {
  picasa::PicasaAlbumsIndexer indexer(album_uids);
  indexer.ParseFolderINI(folders_inis);

  Send(new ChromeUtilityHostMsg_IndexPicasaAlbumsContents_Finished(
      indexer.albums_images()));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN)
void ExtensionsHandler::OnGetWiFiCredentials(const std::string& network_guid) {
  scoped_ptr<wifi::WiFiService> wifi_service(wifi::WiFiService::Create());
  wifi_service->Initialize(NULL);

  std::string key_data;
  std::string error;
  wifi_service->GetKeyFromSystem(network_guid, &key_data, &error);

  Send(new ChromeUtilityHostMsg_GotWiFiCredentials(key_data, error.empty()));
}
#endif  // defined(OS_WIN)

}  // namespace extensions

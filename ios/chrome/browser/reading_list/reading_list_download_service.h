// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DOWNLOAD_SERVICE_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DOWNLOAD_SERVICE_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reading_list/ios/reading_list_model_observer.h"
#include "ios/chrome/browser/reading_list/url_downloader.h"
#include "net/base/network_change_notifier.h"

class GURL;
class PrefService;
class ReadingListModel;
namespace base {
class FilePath;
}

namespace dom_distiller {
class DomDistillerService;
}

namespace reading_list {
class ReadingListDistillerPageFactory;
}

// Observes the reading list and downloads offline versions of its articles.
// Any calls made to DownloadAllEntries/DownloadEntry before the model is
// loaded will be ignored. When the model is loaded, DownloadAllEntries will be
// called automatically.
class ReadingListDownloadService
    : public KeyedService,
      public ReadingListModelObserver,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  ReadingListDownloadService(
      ReadingListModel* reading_list_model,
      dom_distiller::DomDistillerService* distiller_service,
      PrefService* prefs,
      base::FilePath chrome_profile_path,
      net::URLRequestContextGetter* url_request_context_getter,
      std::unique_ptr<reading_list::ReadingListDistillerPageFactory>
          distiller_page_factory);
  ~ReadingListDownloadService() override;

  // Initializes the reading list download service.
  void Initialize();

  // The root folder containing all the offline files.
  virtual base::FilePath OfflineRoot() const;

  // KeyedService implementation.
  void Shutdown() override;

  // ReadingListModelObserver implementation
  void ReadingListModelLoaded(const ReadingListModel* model) override;
  void ReadingListWillRemoveEntry(const ReadingListModel* model,
                                  const GURL& url) override;
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url,
                              reading_list::EntrySource entry_source) override;
  void ReadingListDidMoveEntry(const ReadingListModel* model,
                               const GURL& url) override;

 private:
  // Tries to save offline versions of all entries in the reading list that are
  // not yet saved. Must only be called after reading list model is loaded.
  void DownloadAllEntries();
  // Processes a new entry and schedule a download if needed.
  void ProcessNewEntry(const GURL& url);
  // Schedule a download of an offline version of the reading list entry,
  // according to the delay of the entry. Must only be called after reading list
  // model is loaded.
  void ScheduleDownloadEntry(const GURL& url);
  // Tries to save an offline version of the reading list entry if it is not yet
  // saved. Must only be called after reading list model is loaded.
  void DownloadEntry(const GURL& url);
  // Removes the offline version of the reading list entry if it exists. Must
  // only be called after reading list model is loaded.
  void RemoveDownloadedEntry(const GURL& url);
  // Callback for entry download.
  void OnDownloadEnd(const GURL& url,
                     const GURL& distilled_url,
                     URLDownloader::SuccessState success,
                     const base::FilePath& distilled_path,
                     const std::string& title);

  // Callback for entry deletion.
  void OnDeleteEnd(const GURL& url, bool success);

  // net::NetworkChangeNotifier::ConnectionTypeObserver:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  ReadingListModel* reading_list_model_;
  base::FilePath chrome_profile_path_;
  std::unique_ptr<URLDownloader> url_downloader_;
  std::vector<GURL> url_to_download_cellular_;
  std::vector<GURL> url_to_download_wifi_;
  bool had_connection_;
  std::unique_ptr<reading_list::ReadingListDistillerPageFactory>
      distiller_page_factory_;

  base::WeakPtrFactory<ReadingListDownloadService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ReadingListDownloadService);
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_DOWNLOAD_SERVICE_H_

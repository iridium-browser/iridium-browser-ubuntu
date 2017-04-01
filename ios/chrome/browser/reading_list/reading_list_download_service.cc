// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_download_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/reading_list/ios/offline_url_utils.h"
#include "components/reading_list/ios/reading_list_entry.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "ios/chrome/browser/reading_list/reading_list_distiller_page_factory.h"
#include "ios/web/public/web_thread.h"

namespace {
// Status of the download when it ends, for UMA report.
// These match tools/metrics/histograms/histograms.xml.
enum UMADownloadStatus {
  // The download was successful.
  SUCCESS = 0,
  // The download failed and it won't be retried.
  FAILURE = 1,
  // The download failed and it will be retried.
  RETRY = 2,
  // Add new enum above STATUS_MAX.
  STATUS_MAX
};

// Number of time the download must fail before the download occurs only in
// wifi.
const int kNumberOfFailsBeforeWifiOnly = 5;
// Number of time the download must fail before we give up trying to download
// it.
const int kNumberOfFailsBeforeStop = 7;
}  // namespace

ReadingListDownloadService::ReadingListDownloadService(
    ReadingListModel* reading_list_model,
    dom_distiller::DomDistillerService* distiller_service,
    PrefService* prefs,
    base::FilePath chrome_profile_path,
    net::URLRequestContextGetter* url_request_context_getter,
    std::unique_ptr<reading_list::ReadingListDistillerPageFactory>
        distiller_page_factory)
    : reading_list_model_(reading_list_model),
      chrome_profile_path_(chrome_profile_path),
      had_connection_(!net::NetworkChangeNotifier::IsOffline()),
      distiller_page_factory_(std::move(distiller_page_factory)),
      weak_ptr_factory_(this) {
  DCHECK(reading_list_model);

  url_downloader_ = base::MakeUnique<URLDownloader>(
      distiller_service, distiller_page_factory_.get(), prefs,
      chrome_profile_path, url_request_context_getter,
      base::Bind(&ReadingListDownloadService::OnDownloadEnd,
                 base::Unretained(this)),
      base::Bind(&ReadingListDownloadService::OnDeleteEnd,
                 base::Unretained(this)));
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

ReadingListDownloadService::~ReadingListDownloadService() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void ReadingListDownloadService::Initialize() {
  reading_list_model_->AddObserver(this);
}

base::FilePath ReadingListDownloadService::OfflineRoot() const {
  return reading_list::OfflineRootDirectoryPath(chrome_profile_path_);
}

void ReadingListDownloadService::Shutdown() {
  reading_list_model_->RemoveObserver(this);
}

void ReadingListDownloadService::ReadingListModelLoaded(
    const ReadingListModel* model) {
  DCHECK_EQ(reading_list_model_, model);
  DownloadAllEntries();
}

void ReadingListDownloadService::ReadingListWillRemoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  DCHECK_EQ(reading_list_model_, model);
  DCHECK(model->GetEntryByURL(url));
  RemoveDownloadedEntry(url);
}

void ReadingListDownloadService::ReadingListDidAddEntry(
    const ReadingListModel* model,
    const GURL& url,
    reading_list::EntrySource source) {
  DCHECK_EQ(reading_list_model_, model);
  ProcessNewEntry(url);
}

void ReadingListDownloadService::ReadingListDidMoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  DCHECK_EQ(reading_list_model_, model);
  ProcessNewEntry(url);
}

void ReadingListDownloadService::ProcessNewEntry(const GURL& url) {
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  if (!entry || entry->IsRead()) {
    url_downloader_->CancelDownloadOfflineURL(url);
  } else {
    ScheduleDownloadEntry(url);
  }
}

void ReadingListDownloadService::DownloadAllEntries() {
  DCHECK(reading_list_model_->loaded());
  for (const auto& url : reading_list_model_->Keys()) {
    this->ScheduleDownloadEntry(url);
  }
}

void ReadingListDownloadService::ScheduleDownloadEntry(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  if (!entry || entry->DistilledState() == ReadingListEntry::ERROR ||
      entry->DistilledState() == ReadingListEntry::PROCESSED || entry->IsRead())
    return;
  GURL local_url(url);
  web::WebThread::PostDelayedTask(
      web::WebThread::UI, FROM_HERE,
      base::Bind(&ReadingListDownloadService::DownloadEntry,
                 weak_ptr_factory_.GetWeakPtr(), local_url),
      entry->TimeUntilNextTry());
}

void ReadingListDownloadService::DownloadEntry(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  if (!entry || entry->DistilledState() == ReadingListEntry::ERROR ||
      entry->DistilledState() == ReadingListEntry::PROCESSED || entry->IsRead())
    return;

  if (net::NetworkChangeNotifier::IsOffline()) {
    // There is no connection, save it for download only if we did not exceed
    // the maximaxum number of tries.
    if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeWifiOnly)
      url_to_download_cellular_.push_back(entry->URL());
    if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeStop)
      url_to_download_wifi_.push_back(entry->URL());
    return;
  }

  // There is a connection.
  if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeWifiOnly) {
    // Try to download the page, whatever the connection.
    reading_list_model_->SetEntryDistilledState(entry->URL(),
                                                ReadingListEntry::PROCESSING);
    url_downloader_->DownloadOfflineURL(entry->URL());

  } else if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeStop) {
    // Try to download the page only if the connection is wifi.
    if (net::NetworkChangeNotifier::GetConnectionType() ==
        net::NetworkChangeNotifier::CONNECTION_WIFI) {
      // The connection is wifi, download the page.
      reading_list_model_->SetEntryDistilledState(entry->URL(),
                                                  ReadingListEntry::PROCESSING);
      url_downloader_->DownloadOfflineURL(entry->URL());

    } else {
      // The connection is not wifi, save it for download when the connection
      // changes to wifi.
      url_to_download_wifi_.push_back(entry->URL());
    }
  }
}

void ReadingListDownloadService::RemoveDownloadedEntry(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  url_downloader_->RemoveOfflineURL(url);
}

void ReadingListDownloadService::OnDownloadEnd(
    const GURL& url,
    const GURL& distilled_url,
    URLDownloader::SuccessState success,
    const base::FilePath& distilled_path,
    const std::string& title) {
  DCHECK(reading_list_model_->loaded());
  if ((success == URLDownloader::DOWNLOAD_SUCCESS ||
       success == URLDownloader::DOWNLOAD_EXISTS) &&
      !distilled_path.empty()) {
    reading_list_model_->SetEntryDistilledInfo(url, distilled_path,
                                               distilled_url);
    if (!title.empty())
      reading_list_model_->SetEntryTitle(url, title);

    const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
    if (entry)
      UMA_HISTOGRAM_COUNTS_100("ReadingList.Download.Failures",
                               entry->FailedDownloadCounter());
    UMA_HISTOGRAM_ENUMERATION("ReadingList.Download.Status", SUCCESS,
                              STATUS_MAX);

  } else if (success == URLDownloader::ERROR_RETRY) {
    reading_list_model_->SetEntryDistilledState(url,
                                                ReadingListEntry::WILL_RETRY);
    ScheduleDownloadEntry(url);

    const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
    if (entry) {
      if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeStop) {
        UMA_HISTOGRAM_ENUMERATION("ReadingList.Download.Status", RETRY,
                                  STATUS_MAX);
      } else {
        UMA_HISTOGRAM_ENUMERATION("ReadingList.Download.Status", FAILURE,
                                  STATUS_MAX);
      }
    }

  } else if (success == URLDownloader::ERROR_PERMANENT) {
    reading_list_model_->SetEntryDistilledState(url, ReadingListEntry::ERROR);
    UMA_HISTOGRAM_ENUMERATION("ReadingList.Download.Status", FAILURE,
                              STATUS_MAX);
  }
}

void ReadingListDownloadService::OnDeleteEnd(const GURL& url, bool success) {
  // Nothing to update as this is only called when deleting reading list entries
}

void ReadingListDownloadService::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE) {
    had_connection_ = false;
    return;
  }

  if (!had_connection_) {
    had_connection_ = true;
    for (auto& url : url_to_download_cellular_) {
      ScheduleDownloadEntry(url);
    }
  }
  if (type == net::NetworkChangeNotifier::CONNECTION_WIFI) {
    for (auto& url : url_to_download_wifi_) {
      ScheduleDownloadEntry(url);
    }
  }
}

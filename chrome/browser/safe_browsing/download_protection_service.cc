// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection_service.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_feedback_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/sandboxed_zip_analyzer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "chrome/common/safe_browsing/zip_analyzer_results.h"
#include "chrome/common/url_constants.h"
#include "components/google/core/browser/google_util.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/page_navigator.h"
#include "crypto/sha2.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using content::BrowserThread;

namespace {
static const int64 kDownloadRequestTimeoutMs = 7000;
}  // namespace

namespace safe_browsing {

const char DownloadProtectionService::kDownloadRequestUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/download";

namespace {
// List of extensions for which we track some UMA stats. The position of the
// extension in kDangerousFileTypes is considered to be the UMA enumeration
// value. Naturally, new values should only be added at the end.
const base::FilePath::CharType* const kDangerousFileTypes[] = {
    FILE_PATH_LITERAL(".exe"),
    FILE_PATH_LITERAL(".msi"),
    FILE_PATH_LITERAL(".cab"),
    FILE_PATH_LITERAL(".sys"),
    FILE_PATH_LITERAL(".scr"),
    FILE_PATH_LITERAL(".drv"),
    FILE_PATH_LITERAL(".bat"),
    FILE_PATH_LITERAL(".zip"),
    FILE_PATH_LITERAL(".rar"),
    FILE_PATH_LITERAL(".dll"),
    FILE_PATH_LITERAL(".pif"),
    FILE_PATH_LITERAL(".com"),
    FILE_PATH_LITERAL(".jar"),
    FILE_PATH_LITERAL(".class"),
    FILE_PATH_LITERAL(".pdf"),
    FILE_PATH_LITERAL(".vb"),
    FILE_PATH_LITERAL(".reg"),
    FILE_PATH_LITERAL(".grp"),
    nullptr,  // The "Other" bucket. This is in the middle of the array due to
              // historical reasons.
    FILE_PATH_LITERAL(".crx"),
    FILE_PATH_LITERAL(".apk"),
    FILE_PATH_LITERAL(".dmg"),
    FILE_PATH_LITERAL(".pkg"),
    FILE_PATH_LITERAL(".torrent"),
    FILE_PATH_LITERAL(".website"),
    FILE_PATH_LITERAL(".url"),
    FILE_PATH_LITERAL(".vbe"),
    FILE_PATH_LITERAL(".vbs"),
    FILE_PATH_LITERAL(".js"),
    FILE_PATH_LITERAL(".jse"),
    FILE_PATH_LITERAL(".mht"),
    FILE_PATH_LITERAL(".mhtml"),
    FILE_PATH_LITERAL(".msc"),
    FILE_PATH_LITERAL(".msp"),
    FILE_PATH_LITERAL(".mst"),
    FILE_PATH_LITERAL(".bas"),
    FILE_PATH_LITERAL(".hta"),
    FILE_PATH_LITERAL(".msh"),
    FILE_PATH_LITERAL(".msh1"),
    FILE_PATH_LITERAL(".msh1xml"),
    FILE_PATH_LITERAL(".msh2"),
    FILE_PATH_LITERAL(".msh2xml"),
    FILE_PATH_LITERAL(".mshxml"),
    FILE_PATH_LITERAL(".ps1"),
    FILE_PATH_LITERAL(".ps1xml"),
    FILE_PATH_LITERAL(".ps2"),
    FILE_PATH_LITERAL(".ps2xml"),
    FILE_PATH_LITERAL(".psc1"),
    FILE_PATH_LITERAL(".psc2"),
    FILE_PATH_LITERAL(".scf"),
    FILE_PATH_LITERAL(".sct"),
    FILE_PATH_LITERAL(".wsf"),
};

// UMA enumeration value for unrecognized file types. This is the array index of
// the "Other" bucket in kDangerousFileTypes.
const int EXTENSION_OTHER = 18;

void RecordFileExtensionType(const base::FilePath& file) {
  DCHECK_EQ(static_cast<base::FilePath::CharType*>(nullptr),
            kDangerousFileTypes[EXTENSION_OTHER]);

  int extension_type = EXTENSION_OTHER;
  for (const auto& extension : kDangerousFileTypes) {
    if (extension && file.MatchesExtension(extension)) {
      extension_type = &extension - kDangerousFileTypes;
      break;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("SBClientDownload.DownloadExtensions",
                            extension_type, arraysize(kDangerousFileTypes));
}

// Enumerate for histogramming purposes.
// DO NOT CHANGE THE ORDERING OF THESE VALUES (different histogram data will
// be mixed together based on their values).
enum SBStatsType {
  DOWNLOAD_URL_CHECKS_TOTAL,
  DOWNLOAD_URL_CHECKS_CANCELED,
  DOWNLOAD_URL_CHECKS_MALWARE,

  DOWNLOAD_HASH_CHECKS_TOTAL,
  DOWNLOAD_HASH_CHECKS_MALWARE,

  // Memory space for histograms is determined by the max.
  // ALWAYS ADD NEW VALUES BEFORE THIS ONE.
  DOWNLOAD_CHECKS_MAX
};

// Prepares URLs to be put into a ping message. Currently this just shortens
// data: URIs, other URLs are included verbatim.
std::string SanitizeUrl(const GURL& url) {
  std::string spec = url.spec();
  if (url.SchemeIs(url::kDataScheme)) {
    size_t comma_pos = spec.find(',');
    if (comma_pos != std::string::npos && comma_pos != spec.size() - 1) {
      std::string hash_value = crypto::SHA256HashString(spec);
      spec.erase(comma_pos + 1);
      spec += base::HexEncode(hash_value.data(), hash_value.size());
    }
  }
  return spec;
}

}  // namespace

// Parent SafeBrowsing::Client class used to lookup the bad binary
// URL and digest list.  There are two sub-classes (one for each list).
class DownloadSBClient
    : public SafeBrowsingDatabaseManager::Client,
      public base::RefCountedThreadSafe<DownloadSBClient> {
 public:
  DownloadSBClient(
      const content::DownloadItem& item,
      const DownloadProtectionService::CheckDownloadCallback& callback,
      const scoped_refptr<SafeBrowsingUIManager>& ui_manager,
      SBStatsType total_type,
      SBStatsType dangerous_type)
      : sha256_hash_(item.GetHash()),
        url_chain_(item.GetUrlChain()),
        referrer_url_(item.GetReferrerUrl()),
        callback_(callback),
        ui_manager_(ui_manager),
        start_time_(base::TimeTicks::Now()),
        total_type_(total_type),
        dangerous_type_(dangerous_type) {}

  virtual void StartCheck() = 0;
  virtual bool IsDangerous(SBThreatType threat_type) const = 0;

 protected:
  friend class base::RefCountedThreadSafe<DownloadSBClient>;
  ~DownloadSBClient() override {}

  void CheckDone(SBThreatType threat_type) {
    DownloadProtectionService::DownloadCheckResult result =
        IsDangerous(threat_type) ?
        DownloadProtectionService::DANGEROUS :
        DownloadProtectionService::SAFE;
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(callback_, result));
    UpdateDownloadCheckStats(total_type_);
    if (threat_type != SB_THREAT_TYPE_SAFE) {
      UpdateDownloadCheckStats(dangerous_type_);
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&DownloadSBClient::ReportMalware,
                     this, threat_type));
    }
  }

  void ReportMalware(SBThreatType threat_type) {
    std::string post_data;
    if (!sha256_hash_.empty())
      post_data += base::HexEncode(sha256_hash_.data(),
                                   sha256_hash_.size()) + "\n";
    for (size_t i = 0; i < url_chain_.size(); ++i) {
      post_data += url_chain_[i].spec() + "\n";
    }
    ui_manager_->ReportSafeBrowsingHit(
        url_chain_.back(),  // malicious_url
        url_chain_.front(), // page_url
        referrer_url_,
        true,  // is_subresource
        threat_type,
        post_data);
  }

  void UpdateDownloadCheckStats(SBStatsType stat_type) {
    UMA_HISTOGRAM_ENUMERATION("SB2.DownloadChecks",
                              stat_type,
                              DOWNLOAD_CHECKS_MAX);
  }

  std::string sha256_hash_;
  std::vector<GURL> url_chain_;
  GURL referrer_url_;
  DownloadProtectionService::CheckDownloadCallback callback_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  base::TimeTicks start_time_;

 private:
  const SBStatsType total_type_;
  const SBStatsType dangerous_type_;

  DISALLOW_COPY_AND_ASSIGN(DownloadSBClient);
};

class DownloadUrlSBClient : public DownloadSBClient {
 public:
  DownloadUrlSBClient(
      const content::DownloadItem& item,
      const DownloadProtectionService::CheckDownloadCallback& callback,
      const scoped_refptr<SafeBrowsingUIManager>& ui_manager,
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager)
      : DownloadSBClient(item, callback, ui_manager,
                         DOWNLOAD_URL_CHECKS_TOTAL,
                         DOWNLOAD_URL_CHECKS_MALWARE),
        database_manager_(database_manager) { }

  void StartCheck() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (!database_manager_.get() ||
        database_manager_->CheckDownloadUrl(url_chain_, this)) {
      CheckDone(SB_THREAT_TYPE_SAFE);
    } else {
      AddRef();  // SafeBrowsingService takes a pointer not a scoped_refptr.
    }
  }

  bool IsDangerous(SBThreatType threat_type) const override {
    return threat_type == SB_THREAT_TYPE_BINARY_MALWARE_URL;
  }

  void OnCheckDownloadUrlResult(const std::vector<GURL>& url_chain,
                                SBThreatType threat_type) override {
    CheckDone(threat_type);
    UMA_HISTOGRAM_TIMES("SB2.DownloadUrlCheckDuration",
                        base::TimeTicks::Now() - start_time_);
    Release();
  }

 protected:
  ~DownloadUrlSBClient() override {}

 private:
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUrlSBClient);
};

class DownloadProtectionService::CheckClientDownloadRequest
    : public base::RefCountedThreadSafe<
          DownloadProtectionService::CheckClientDownloadRequest,
          BrowserThread::DeleteOnUIThread>,
      public net::URLFetcherDelegate,
      public content::DownloadItem::Observer {
 public:
  CheckClientDownloadRequest(
      content::DownloadItem* item,
      const CheckDownloadCallback& callback,
      DownloadProtectionService* service,
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      BinaryFeatureExtractor* binary_feature_extractor)
      : item_(item),
        url_chain_(item->GetUrlChain()),
        referrer_url_(item->GetReferrerUrl()),
        tab_url_(item->GetTabUrl()),
        tab_referrer_url_(item->GetTabReferrerUrl()),
        zipped_executable_(false),
        callback_(callback),
        service_(service),
        binary_feature_extractor_(binary_feature_extractor),
        database_manager_(database_manager),
        pingback_enabled_(service_->enabled()),
        finished_(false),
        type_(ClientDownloadRequest::WIN_EXECUTABLE),
        start_time_(base::TimeTicks::Now()),
        weakptr_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    item_->AddObserver(this);
  }

  void Start() {
    DVLOG(2) << "Starting SafeBrowsing download check for: "
             << item_->DebugString(true);
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // TODO(noelutz): implement some cache to make sure we don't issue the same
    // request over and over again if a user downloads the same binary multiple
    // times.
    DownloadCheckResultReason reason = REASON_MAX;
    if (!IsSupportedDownload(
        *item_, item_->GetTargetFilePath(), &reason, &type_)) {
      switch (reason) {
        case REASON_EMPTY_URL_CHAIN:
        case REASON_INVALID_URL:
        case REASON_UNSUPPORTED_URL_SCHEME:
          PostFinishTask(UNKNOWN, reason);
          return;

        case REASON_NOT_BINARY_FILE:
          RecordFileExtensionType(item_->GetTargetFilePath());
          PostFinishTask(UNKNOWN, reason);
          return;

        default:
          // We only expect the reasons explicitly handled above.
          NOTREACHED();
      }
    }
    RecordFileExtensionType(item_->GetTargetFilePath());

    // Compute features from the file contents. Note that we record histograms
    // based on the result, so this runs regardless of whether the pingbacks
    // are enabled.
    if (item_->GetTargetFilePath().MatchesExtension(
        FILE_PATH_LITERAL(".zip"))) {
      StartExtractZipFeatures();
    } else {
      DCHECK(!download_protection_util::IsArchiveFile(
          item_->GetTargetFilePath()));
      StartExtractFileFeatures();
    }
  }

  // Start a timeout to cancel the request if it takes too long.
  // This should only be called after we have finished accessing the file.
  void StartTimeout() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!service_) {
      // Request has already been cancelled.
      return;
    }
    timeout_start_time_ = base::TimeTicks::Now();
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::Cancel,
                   weakptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            service_->download_request_timeout_ms()));
  }

  // Canceling a request will cause us to always report the result as UNKNOWN
  // unless a pending request is about to call FinishRequest.
  void Cancel() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (fetcher_.get()) {
      // The DownloadProtectionService is going to release its reference, so we
      // might be destroyed before the URLFetcher completes.  Cancel the
      // fetcher so it does not try to invoke OnURLFetchComplete.
      fetcher_.reset();
    }
    // Note: If there is no fetcher, then some callback is still holding a
    // reference to this object.  We'll eventually wind up in some method on
    // the UI thread that will call FinishRequest() again.  If FinishRequest()
    // is called a second time, it will be a no-op.
    FinishRequest(UNKNOWN, REASON_REQUEST_CANCELED);
    // Calling FinishRequest might delete this object, we may be deleted by
    // this point.
  }

  // content::DownloadItem::Observer implementation.
  void OnDownloadDestroyed(content::DownloadItem* download) override {
    Cancel();
    DCHECK(item_ == NULL);
  }

  // From the net::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(source, fetcher_.get());
    DVLOG(2) << "Received a response for URL: "
             << item_->GetUrlChain().back() << ": success="
             << source->GetStatus().is_success() << " response_code="
             << source->GetResponseCode();
    if (source->GetStatus().is_success()) {
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "SBClientDownload.DownloadRequestResponseCode",
          source->GetResponseCode());
    }
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "SBClientDownload.DownloadRequestNetError",
        -source->GetStatus().error());
    DownloadCheckResultReason reason = REASON_SERVER_PING_FAILED;
    DownloadCheckResult result = UNKNOWN;
    if (source->GetStatus().is_success() &&
        net::HTTP_OK == source->GetResponseCode()) {
      ClientDownloadResponse response;
      std::string data;
      bool got_data = source->GetResponseAsString(&data);
      DCHECK(got_data);
      if (!response.ParseFromString(data)) {
        reason = REASON_INVALID_RESPONSE_PROTO;
        result = UNKNOWN;
      } else if (response.verdict() == ClientDownloadResponse::SAFE) {
        reason = REASON_DOWNLOAD_SAFE;
        result = SAFE;
      } else if (service_ && !service_->IsSupportedDownload(
          *item_, item_->GetTargetFilePath())) {
        // The client of the download protection service assumes that we don't
        // support this download so we cannot return any other verdict than
        // UNKNOWN even if the server says it's dangerous to download this file.
        // Note: if service_ is NULL we already cancelled the request and
        // returned UNKNOWN.
        reason = REASON_DOWNLOAD_NOT_SUPPORTED;
        result = UNKNOWN;
      } else if (response.verdict() == ClientDownloadResponse::DANGEROUS) {
        reason = REASON_DOWNLOAD_DANGEROUS;
        result = DANGEROUS;
      } else if (response.verdict() == ClientDownloadResponse::UNCOMMON) {
        reason = REASON_DOWNLOAD_UNCOMMON;
        result = UNCOMMON;
      } else if (response.verdict() == ClientDownloadResponse::DANGEROUS_HOST) {
        reason = REASON_DOWNLOAD_DANGEROUS_HOST;
        result = DANGEROUS_HOST;
      } else if (
          response.verdict() == ClientDownloadResponse::POTENTIALLY_UNWANTED) {
        reason = REASON_DOWNLOAD_POTENTIALLY_UNWANTED;
        result = POTENTIALLY_UNWANTED;
      } else {
        LOG(DFATAL) << "Unknown download response verdict: "
                    << response.verdict();
        reason = REASON_INVALID_RESPONSE_VERDICT;
        result = UNKNOWN;
      }
      DownloadFeedbackService::MaybeStorePingsForDownload(
          result, item_, client_download_request_data_, data);
    }
    // We don't need the fetcher anymore.
    fetcher_.reset();
    UMA_HISTOGRAM_TIMES("SBClientDownload.DownloadRequestDuration",
                        base::TimeTicks::Now() - start_time_);
    UMA_HISTOGRAM_TIMES("SBClientDownload.DownloadRequestNetworkDuration",
                        base::TimeTicks::Now() - request_start_time_);
    FinishRequest(result, reason);
  }

  static bool IsSupportedDownload(const content::DownloadItem& item,
                                  const base::FilePath& target_path,
                                  DownloadCheckResultReason* reason,
                                  ClientDownloadRequest::DownloadType* type) {
    if (item.GetUrlChain().empty()) {
      *reason = REASON_EMPTY_URL_CHAIN;
      return false;
    }
    const GURL& final_url = item.GetUrlChain().back();
    if (!final_url.is_valid() || final_url.is_empty()) {
      *reason = REASON_INVALID_URL;
      return false;
    }
    if (!download_protection_util::IsBinaryFile(target_path)) {
      *reason = REASON_NOT_BINARY_FILE;
      return false;
    }
    if ((!final_url.IsStandard() && !final_url.SchemeIsBlob() &&
         !final_url.SchemeIs(url::kDataScheme)) ||
        final_url.SchemeIsFile()) {
      *reason = REASON_UNSUPPORTED_URL_SCHEME;
      return false;
    }
    *type = download_protection_util::GetDownloadType(target_path);
    return true;
  }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<CheckClientDownloadRequest>;

  ~CheckClientDownloadRequest() override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(item_ == NULL);
  }

  void OnFileFeatureExtractionDone() {
    // This can run in any thread, since it just posts more messages.

    // TODO(noelutz): DownloadInfo should also contain the IP address of
    // every URL in the redirect chain.  We also should check whether the
    // download URL is hosted on the internal network.
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::CheckWhitelists, this));

    // We wait until after the file checks finish to start the timeout, as
    // windows can cause permissions errors if the timeout fired while we were
    // checking the file signature and we tried to complete the download.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::StartTimeout, this));
  }

  void StartExtractFileFeatures() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(item_);  // Called directly from Start(), item should still exist.
    // Since we do blocking I/O, offload this to a worker thread.
    // The task does not need to block shutdown.
    BrowserThread::GetBlockingPool()->PostWorkerTaskWithShutdownBehavior(
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::ExtractFileFeatures,
                   this, item_->GetFullPath()),
        base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  }

  void ExtractFileFeatures(const base::FilePath& file_path) {
    base::TimeTicks start_time = base::TimeTicks::Now();
    binary_feature_extractor_->CheckSignature(file_path, &signature_info_);
    bool is_signed = (signature_info_.certificate_chain_size() > 0);
    if (is_signed) {
      DVLOG(2) << "Downloaded a signed binary: " << file_path.value();
    } else {
      DVLOG(2) << "Downloaded an unsigned binary: "
               << file_path.value();
    }
    UMA_HISTOGRAM_BOOLEAN("SBClientDownload.SignedBinaryDownload", is_signed);
    UMA_HISTOGRAM_TIMES("SBClientDownload.ExtractSignatureFeaturesTime",
                        base::TimeTicks::Now() - start_time);

    start_time = base::TimeTicks::Now();
    image_headers_.reset(new ClientDownloadRequest_ImageHeaders());
    if (!binary_feature_extractor_->ExtractImageFeatures(
            file_path,
            BinaryFeatureExtractor::kDefaultOptions,
            image_headers_.get(),
            nullptr /* signed_data */)) {
      image_headers_.reset();
    }
    UMA_HISTOGRAM_TIMES("SBClientDownload.ExtractImageHeadersTime",
                        base::TimeTicks::Now() - start_time);

    OnFileFeatureExtractionDone();
  }

  void StartExtractZipFeatures() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(item_);  // Called directly from Start(), item should still exist.
    zip_analysis_start_time_ = base::TimeTicks::Now();
    // We give the zip analyzer a weak pointer to this object.  Since the
    // analyzer is refcounted, it might outlive the request.
    analyzer_ = new SandboxedZipAnalyzer(
        item_->GetFullPath(),
        base::Bind(&CheckClientDownloadRequest::OnZipAnalysisFinished,
                   weakptr_factory_.GetWeakPtr()));
    analyzer_->Start();
  }

  void OnZipAnalysisFinished(const zip_analyzer::Results& results) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!service_)
      return;
    if (results.success) {
      zipped_executable_ = results.has_executable;
      archived_binary_.CopyFrom(results.archived_binary);
      DVLOG(1) << "Zip analysis finished for " << item_->GetFullPath().value()
               << ", has_executable=" << results.has_executable
               << " has_archive=" << results.has_archive;
    } else {
      DVLOG(1) << "Zip analysis failed for " << item_->GetFullPath().value();
    }
    UMA_HISTOGRAM_BOOLEAN("SBClientDownload.ZipFileHasExecutable",
                          zipped_executable_);
    UMA_HISTOGRAM_BOOLEAN("SBClientDownload.ZipFileHasArchiveButNoExecutable",
                          results.has_archive && !zipped_executable_);
    UMA_HISTOGRAM_TIMES("SBClientDownload.ExtractZipFeaturesTime",
                        base::TimeTicks::Now() - zip_analysis_start_time_);

    if (!zipped_executable_) {
      PostFinishTask(UNKNOWN, REASON_ARCHIVE_WITHOUT_BINARIES);
      return;
    }
    OnFileFeatureExtractionDone();
  }

  static void RecordCountOfSignedOrWhitelistedDownload() {
    UMA_HISTOGRAM_COUNTS("SBClientDownload.SignedOrWhitelistedDownload", 1);
  }

  void CheckWhitelists() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    if (!database_manager_.get()) {
      PostFinishTask(UNKNOWN, REASON_SB_DISABLED);
      return;
    }

    const GURL& url = url_chain_.back();
    // TODO(asanka): This may acquire a lock on the SB DB on the IO thread.
    if (url.is_valid() && database_manager_->MatchDownloadWhitelistUrl(url)) {
      DVLOG(2) << url << " is on the download whitelist.";
      RecordCountOfSignedOrWhitelistedDownload();
      // TODO(grt): Continue processing without uploading so that
      // ClientDownloadRequest callbacks can be run even for this type of safe
      // download.
      PostFinishTask(SAFE, REASON_WHITELISTED_URL);
      return;
    }

    if (signature_info_.trusted()) {
      RecordCountOfSignedOrWhitelistedDownload();
      for (int i = 0; i < signature_info_.certificate_chain_size(); ++i) {
        if (CertificateChainIsWhitelisted(
                signature_info_.certificate_chain(i))) {
          // TODO(grt): Continue processing without uploading so that
          // ClientDownloadRequest callbacks can be run even for this type of
          // safe download.
          PostFinishTask(SAFE, REASON_TRUSTED_EXECUTABLE);
          return;
        }
      }
    }

    if (!pingback_enabled_) {
      PostFinishTask(UNKNOWN, REASON_PING_DISABLED);
      return;
    }

    // Currently, the UI is only enabled on Windows and OSX so we don't even
    // bother with pinging the server if we're not on one of those platforms.
    // TODO(noelutz): change this code once the UI is done for Linux.
#if defined(OS_WIN) || defined(OS_MACOSX)
    // The URLFetcher is owned by the UI thread, so post a message to
    // start the pingback.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::GetTabRedirects, this));
#else
    PostFinishTask(UNKNOWN, REASON_OS_NOT_SUPPORTED);
#endif
  }

  void GetTabRedirects() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!service_)
      return;

    if (!tab_url_.is_valid()) {
      SendRequest();
      return;
    }

    Profile* profile = Profile::FromBrowserContext(item_->GetBrowserContext());
    history::HistoryService* history = HistoryServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    if (!history) {
      SendRequest();
      return;
    }

    history->QueryRedirectsTo(
        tab_url_,
        base::Bind(&CheckClientDownloadRequest::OnGotTabRedirects,
                   base::Unretained(this),
                   tab_url_),
        &request_tracker_);
  }

  void OnGotTabRedirects(const GURL& url,
                         const history::RedirectList* redirect_list) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_EQ(url, tab_url_);
    if (!service_)
      return;

    if (!redirect_list->empty()) {
      tab_redirects_.insert(
          tab_redirects_.end(), redirect_list->rbegin(), redirect_list->rend());
    }

    SendRequest();
  }

  void SendRequest() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    // This is our last chance to check whether the request has been canceled
    // before sending it.
    if (!service_)
      return;

    ClientDownloadRequest request;
    request.set_url(SanitizeUrl(item_->GetUrlChain().back()));
    request.mutable_digests()->set_sha256(item_->GetHash());
    request.set_length(item_->GetReceivedBytes());
    for (size_t i = 0; i < item_->GetUrlChain().size(); ++i) {
      ClientDownloadRequest::Resource* resource = request.add_resources();
      resource->set_url(SanitizeUrl(item_->GetUrlChain()[i]));
      if (i == item_->GetUrlChain().size() - 1) {
        // The last URL in the chain is the download URL.
        resource->set_type(ClientDownloadRequest::DOWNLOAD_URL);
        resource->set_referrer(SanitizeUrl(item_->GetReferrerUrl()));
        DVLOG(2) << "dl url " << resource->url();
        if (!item_->GetRemoteAddress().empty()) {
          resource->set_remote_ip(item_->GetRemoteAddress());
          DVLOG(2) << "  dl url remote addr: " << resource->remote_ip();
        }
        DVLOG(2) << "dl referrer " << resource->referrer();
      } else {
        DVLOG(2) << "dl redirect " << i << " " << resource->url();
        resource->set_type(ClientDownloadRequest::DOWNLOAD_REDIRECT);
      }
      // TODO(noelutz): fill out the remote IP addresses.
    }
    // TODO(mattm): fill out the remote IP addresses for tab resources.
    for (size_t i = 0; i < tab_redirects_.size(); ++i) {
      ClientDownloadRequest::Resource* resource = request.add_resources();
      DVLOG(2) << "tab redirect " << i << " " << tab_redirects_[i].spec();
      resource->set_url(SanitizeUrl(tab_redirects_[i]));
      resource->set_type(ClientDownloadRequest::TAB_REDIRECT);
    }
    if (tab_url_.is_valid()) {
      ClientDownloadRequest::Resource* resource = request.add_resources();
      resource->set_url(SanitizeUrl(tab_url_));
      DVLOG(2) << "tab url " << resource->url();
      resource->set_type(ClientDownloadRequest::TAB_URL);
      if (tab_referrer_url_.is_valid()) {
        resource->set_referrer(SanitizeUrl(tab_referrer_url_));
        DVLOG(2) << "tab referrer " << resource->referrer();
      }
    }

    request.set_user_initiated(item_->HasUserGesture());
    request.set_file_basename(
        item_->GetTargetFilePath().BaseName().AsUTF8Unsafe());
    request.set_download_type(type_);
    request.mutable_signature()->CopyFrom(signature_info_);
    if (image_headers_)
      request.set_allocated_image_headers(image_headers_.release());
    if (zipped_executable_)
      request.mutable_archived_binary()->Swap(&archived_binary_);
    if (!request.SerializeToString(&client_download_request_data_)) {
      FinishRequest(UNKNOWN, REASON_INVALID_REQUEST_PROTO);
      return;
    }
    service_->client_download_request_callbacks_.Notify(item_, &request);

    DVLOG(2) << "Sending a request for URL: "
             << item_->GetUrlChain().back();
    fetcher_ = net::URLFetcher::Create(0 /* ID used for testing */,
                                       GetDownloadRequestUrl(),
                                       net::URLFetcher::POST, this);
    fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
    fetcher_->SetAutomaticallyRetryOn5xx(false);  // Don't retry on error.
    fetcher_->SetRequestContext(service_->request_context_getter_.get());
    fetcher_->SetUploadData("application/octet-stream",
                            client_download_request_data_);
    request_start_time_ = base::TimeTicks::Now();
    UMA_HISTOGRAM_COUNTS("SBClientDownload.DownloadRequestPayloadSize",
                         client_download_request_data_.size());
    fetcher_->Start();
  }

  void PostFinishTask(DownloadCheckResult result,
                      DownloadCheckResultReason reason) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&CheckClientDownloadRequest::FinishRequest, this, result,
                   reason));
  }

  void FinishRequest(DownloadCheckResult result,
                     DownloadCheckResultReason reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (finished_) {
      return;
    }
    finished_ = true;
    // Ensure the timeout task is cancelled while we still have a non-zero
    // refcount. (crbug.com/240449)
    weakptr_factory_.InvalidateWeakPtrs();
    if (!request_start_time_.is_null()) {
      UMA_HISTOGRAM_ENUMERATION("SBClientDownload.DownloadRequestNetworkStats",
                                reason,
                                REASON_MAX);
    }
    if (!timeout_start_time_.is_null()) {
      UMA_HISTOGRAM_ENUMERATION("SBClientDownload.DownloadRequestTimeoutStats",
                                reason,
                                REASON_MAX);
      if (reason != REASON_REQUEST_CANCELED) {
        UMA_HISTOGRAM_TIMES("SBClientDownload.DownloadRequestTimeoutDuration",
                            base::TimeTicks::Now() - timeout_start_time_);
      }
    }
    if (result == SAFE && (reason == REASON_WHITELISTED_URL ||
                           reason == REASON_TRUSTED_EXECUTABLE)) {
      // Due to the short-circuit logic in CheckWhitelists (see TODOs there), a
      // ClientDownloadRequest was not generated for this download and callbacks
      // were not run. Run them now with null to indicate that a download has
      // taken place.
      // TODO(grt): persist metadata for these downloads as well.
      service_->client_download_request_callbacks_.Notify(item_, nullptr);
    }
    if (service_) {
      DVLOG(2) << "SafeBrowsing download verdict for: "
               << item_->DebugString(true) << " verdict:" << reason
               << " result:" << result;
      UMA_HISTOGRAM_ENUMERATION("SBClientDownload.CheckDownloadStats",
                                reason,
                                REASON_MAX);
      callback_.Run(result);
      item_->RemoveObserver(this);
      item_ = NULL;
      DownloadProtectionService* service = service_;
      service_ = NULL;
      service->RequestFinished(this);
      // DownloadProtectionService::RequestFinished will decrement our refcount,
      // so we may be deleted now.
    } else {
      callback_.Run(UNKNOWN);
    }
  }

  bool CertificateChainIsWhitelisted(
      const ClientDownloadRequest_CertificateChain& chain) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (chain.element_size() < 2) {
      // We need to have both a signing certificate and its issuer certificate
      // present to construct a whitelist entry.
      return false;
    }
    scoped_refptr<net::X509Certificate> cert =
        net::X509Certificate::CreateFromBytes(
            chain.element(0).certificate().data(),
            chain.element(0).certificate().size());
    if (!cert.get()) {
      return false;
    }

    for (int i = 1; i < chain.element_size(); ++i) {
      scoped_refptr<net::X509Certificate> issuer =
          net::X509Certificate::CreateFromBytes(
              chain.element(i).certificate().data(),
              chain.element(i).certificate().size());
      if (!issuer.get()) {
        return false;
      }
      std::vector<std::string> whitelist_strings;
      DownloadProtectionService::GetCertificateWhitelistStrings(
          *cert.get(), *issuer.get(), &whitelist_strings);
      for (size_t j = 0; j < whitelist_strings.size(); ++j) {
        if (database_manager_->MatchDownloadWhitelistString(
                whitelist_strings[j])) {
          DVLOG(2) << "Certificate matched whitelist, cert="
                   << cert->subject().GetDisplayName()
                   << " issuer=" << issuer->subject().GetDisplayName();
          return true;
        }
      }
      cert = issuer;
    }
    return false;
  }

  // The DownloadItem we are checking. Will be NULL if the request has been
  // canceled. Must be accessed only on UI thread.
  content::DownloadItem* item_;
  // Copies of data from |item_| for access on other threads.
  std::vector<GURL> url_chain_;
  GURL referrer_url_;
  // URL chain of redirects leading to (but not including) |tab_url|.
  std::vector<GURL> tab_redirects_;
  // URL and referrer of the window the download was started from.
  GURL tab_url_;
  GURL tab_referrer_url_;

  bool zipped_executable_;
  ClientDownloadRequest_SignatureInfo signature_info_;
  scoped_ptr<ClientDownloadRequest_ImageHeaders> image_headers_;
  google::protobuf::RepeatedPtrField<ClientDownloadRequest_ArchivedBinary>
      archived_binary_;
  CheckDownloadCallback callback_;
  // Will be NULL if the request has been canceled.
  DownloadProtectionService* service_;
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  const bool pingback_enabled_;
  scoped_ptr<net::URLFetcher> fetcher_;
  scoped_refptr<SandboxedZipAnalyzer> analyzer_;
  base::TimeTicks zip_analysis_start_time_;
  bool finished_;
  ClientDownloadRequest::DownloadType type_;
  std::string client_download_request_data_;
  base::CancelableTaskTracker request_tracker_;  // For HistoryService lookup.
  base::TimeTicks start_time_;  // Used for stats.
  base::TimeTicks timeout_start_time_;
  base::TimeTicks request_start_time_;
  base::WeakPtrFactory<CheckClientDownloadRequest> weakptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CheckClientDownloadRequest);
};

DownloadProtectionService::DownloadProtectionService(
    SafeBrowsingService* sb_service,
    net::URLRequestContextGetter* request_context_getter)
    : request_context_getter_(request_context_getter),
      enabled_(false),
      binary_feature_extractor_(new BinaryFeatureExtractor()),
      download_request_timeout_ms_(kDownloadRequestTimeoutMs),
      feedback_service_(new DownloadFeedbackService(
          request_context_getter, BrowserThread::GetBlockingPool())) {

  if (sb_service) {
    ui_manager_ = sb_service->ui_manager();
    database_manager_ = sb_service->database_manager();
  }
}

DownloadProtectionService::~DownloadProtectionService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CancelPendingRequests();
}

void DownloadProtectionService::SetEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (enabled == enabled_) {
    return;
  }
  enabled_ = enabled;
  if (!enabled_) {
    CancelPendingRequests();
  }
}

void DownloadProtectionService::CheckClientDownload(
    content::DownloadItem* item,
    const CheckDownloadCallback& callback) {
  scoped_refptr<CheckClientDownloadRequest> request(
      new CheckClientDownloadRequest(item, callback, this,
                                     database_manager_,
                                     binary_feature_extractor_.get()));
  download_requests_.insert(request);
  request->Start();
}

void DownloadProtectionService::CheckDownloadUrl(
    const content::DownloadItem& item,
    const CheckDownloadCallback& callback) {
  DCHECK(!item.GetUrlChain().empty());
  scoped_refptr<DownloadUrlSBClient> client(
      new DownloadUrlSBClient(item, callback, ui_manager_, database_manager_));
  // The client will release itself once it is done.
  BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&DownloadUrlSBClient::StartCheck, client));
}

bool DownloadProtectionService::IsSupportedDownload(
    const content::DownloadItem& item,
    const base::FilePath& target_path) const {
  // Currently, the UI is only enabled on Windows and OSX.  On Linux we still
  // want to show the dangerous file type warning if the file is possibly
  // dangerous which means we have to always return false here.
#if defined(OS_WIN) || defined(OS_MACOSX)
  DownloadCheckResultReason reason = REASON_MAX;
  ClientDownloadRequest::DownloadType type =
      ClientDownloadRequest::WIN_EXECUTABLE;
  return (CheckClientDownloadRequest::IsSupportedDownload(
              item, target_path, &reason, &type) &&
          (ClientDownloadRequest::CHROME_EXTENSION != type));
#else
  return false;
#endif
}

DownloadProtectionService::ClientDownloadRequestSubscription
DownloadProtectionService::RegisterClientDownloadRequestCallback(
    const ClientDownloadRequestCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return client_download_request_callbacks_.Add(callback);
}

void DownloadProtectionService::CancelPendingRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (std::set<scoped_refptr<CheckClientDownloadRequest> >::iterator it =
           download_requests_.begin();
       it != download_requests_.end();) {
    // We need to advance the iterator before we cancel because canceling
    // the request will invalidate it when RequestFinished is called below.
    scoped_refptr<CheckClientDownloadRequest> tmp = *it++;
    tmp->Cancel();
  }
  DCHECK(download_requests_.empty());
}

void DownloadProtectionService::RequestFinished(
    CheckClientDownloadRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<scoped_refptr<CheckClientDownloadRequest> >::iterator it =
      download_requests_.find(request);
  DCHECK(it != download_requests_.end());
  download_requests_.erase(*it);
}

void DownloadProtectionService::ShowDetailsForDownload(
    const content::DownloadItem& item,
    content::PageNavigator* navigator) {
  GURL learn_more_url(chrome::kDownloadScanningLearnMoreURL);
  learn_more_url = google_util::AppendGoogleLocaleParam(
      learn_more_url, g_browser_process->GetApplicationLocale());
  navigator->OpenURL(
      content::OpenURLParams(learn_more_url,
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK,
                             false));
}

namespace {
// Escapes a certificate attribute so that it can be used in a whitelist
// entry.  Currently, we only escape slashes, since they are used as a
// separator between attributes.
std::string EscapeCertAttribute(const std::string& attribute) {
  std::string escaped;
  for (size_t i = 0; i < attribute.size(); ++i) {
    if (attribute[i] == '%') {
      escaped.append("%25");
    } else if (attribute[i] == '/') {
      escaped.append("%2F");
    } else {
      escaped.push_back(attribute[i]);
    }
  }
  return escaped;
}
}  // namespace

// static
void DownloadProtectionService::GetCertificateWhitelistStrings(
    const net::X509Certificate& certificate,
    const net::X509Certificate& issuer,
    std::vector<std::string>* whitelist_strings) {
  // The whitelist paths are in the format:
  // cert/<ascii issuer fingerprint>[/CN=common_name][/O=org][/OU=unit]
  //
  // Any of CN, O, or OU may be omitted from the whitelist entry, in which
  // case they match anything.  However, the attributes that do appear will
  // always be in the order shown above.  At least one attribute will always
  // be present.

  const net::CertPrincipal& subject = certificate.subject();
  std::vector<std::string> ou_tokens;
  for (size_t i = 0; i < subject.organization_unit_names.size(); ++i) {
    ou_tokens.push_back(
        "/OU=" + EscapeCertAttribute(subject.organization_unit_names[i]));
  }

  std::vector<std::string> o_tokens;
  for (size_t i = 0; i < subject.organization_names.size(); ++i) {
    o_tokens.push_back(
        "/O=" + EscapeCertAttribute(subject.organization_names[i]));
  }

  std::string cn_token;
  if (!subject.common_name.empty()) {
    cn_token = "/CN=" + EscapeCertAttribute(subject.common_name);
  }

  std::set<std::string> paths_to_check;
  if (!cn_token.empty()) {
    paths_to_check.insert(cn_token);
  }
  for (size_t i = 0; i < o_tokens.size(); ++i) {
    paths_to_check.insert(cn_token + o_tokens[i]);
    paths_to_check.insert(o_tokens[i]);
    for (size_t j = 0; j < ou_tokens.size(); ++j) {
      paths_to_check.insert(cn_token + o_tokens[i] + ou_tokens[j]);
      paths_to_check.insert(o_tokens[i] + ou_tokens[j]);
    }
  }
  for (size_t i = 0; i < ou_tokens.size(); ++i) {
    paths_to_check.insert(cn_token + ou_tokens[i]);
    paths_to_check.insert(ou_tokens[i]);
  }

  std::string issuer_fp = base::HexEncode(issuer.fingerprint().data,
                                          sizeof(issuer.fingerprint().data));
  for (std::set<std::string>::iterator it = paths_to_check.begin();
       it != paths_to_check.end(); ++it) {
    whitelist_strings->push_back("cert/" + issuer_fp + *it);
  }
}

// static
GURL DownloadProtectionService::GetDownloadRequestUrl() {
  GURL url(kDownloadRequestUrl);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty())
    url = url.Resolve("?key=" + net::EscapeQueryParamValue(api_key, true));

  return url;
}

}  // namespace safe_browsing

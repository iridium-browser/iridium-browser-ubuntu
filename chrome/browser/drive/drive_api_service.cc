// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_api_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "google_apis/drive/auth_service.h"
#include "google_apis/drive/base_requests.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_api_requests.h"
#include "google_apis/drive/request_sender.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_request_context_getter.h"

using google_apis::AboutResourceCallback;
using google_apis::AppList;
using google_apis::AppListCallback;
using google_apis::AuthStatusCallback;
using google_apis::AuthorizeAppCallback;
using google_apis::CancelCallback;
using google_apis::ChangeList;
using google_apis::ChangeListCallback;
using google_apis::DownloadActionCallback;
using google_apis::EntryActionCallback;
using google_apis::FileList;
using google_apis::FileListCallback;
using google_apis::FileResource;
using google_apis::FileResourceCallback;
using google_apis::DRIVE_OTHER_ERROR;
using google_apis::DRIVE_PARSE_ERROR;
using google_apis::DriveApiErrorCode;
using google_apis::GetContentCallback;
using google_apis::GetShareUrlCallback;
using google_apis::HTTP_NOT_IMPLEMENTED;
using google_apis::HTTP_SUCCESS;
using google_apis::InitiateUploadCallback;
using google_apis::ProgressCallback;
using google_apis::RequestSender;
using google_apis::UploadRangeResponse;
using google_apis::drive::AboutGetRequest;
using google_apis::drive::AppsListRequest;
using google_apis::drive::ChangesListRequest;
using google_apis::drive::ChangesListNextPageRequest;
using google_apis::drive::ChildrenDeleteRequest;
using google_apis::drive::ChildrenInsertRequest;
using google_apis::drive::DownloadFileRequest;
using google_apis::drive::FilesCopyRequest;
using google_apis::drive::FilesGetRequest;
using google_apis::drive::FilesInsertRequest;
using google_apis::drive::FilesPatchRequest;
using google_apis::drive::FilesListRequest;
using google_apis::drive::FilesListNextPageRequest;
using google_apis::drive::FilesDeleteRequest;
using google_apis::drive::FilesTrashRequest;
using google_apis::drive::GetUploadStatusRequest;
using google_apis::drive::InitiateUploadExistingFileRequest;
using google_apis::drive::InitiateUploadNewFileRequest;
using google_apis::drive::ResumeUploadRequest;
using google_apis::drive::UploadRangeCallback;

namespace drive {

namespace {

// OAuth2 scopes for Drive API.
const char kDriveScope[] = "https://www.googleapis.com/auth/drive";
const char kDriveAppsReadonlyScope[] =
    "https://www.googleapis.com/auth/drive.apps.readonly";
const char kDriveAppsScope[] = "https://www.googleapis.com/auth/drive.apps";
const char kDocsListScope[] = "https://docs.google.com/feeds/";

// Mime type to create a directory.
const char kFolderMimeType[] = "application/vnd.google-apps.folder";

// Max number of file entries to be fetched in a single http request.
//
// The larger the number is,
// - The total running time to fetch the whole file list will become shorter.
// - The running time for a single request tends to become longer.
// Since the file list fetching is a completely background task, for our side,
// only the total time matters. However, the server seems to have a time limit
// per single request, which disables us to set the largest value (1000).
// TODO(kinaba): make it larger when the server gets faster.
const int kMaxNumFilesResourcePerRequest = 300;
const int kMaxNumFilesResourcePerRequestForSearch = 100;

// For performance, we declare all fields we use.
const char kAboutResourceFields[] =
    "kind,quotaBytesTotal,quotaBytesUsedAggregate,largestChangeId,rootFolderId";
const char kFileResourceFields[] =
    "kind,id,title,createdDate,sharedWithMeDate,mimeType,"
    "md5Checksum,fileSize,labels/trashed,imageMediaMetadata/width,"
    "imageMediaMetadata/height,imageMediaMetadata/rotation,etag,"
    "parents(id,parentLink),alternateLink,"
    "modifiedDate,lastViewedByMeDate,shared";
const char kFileResourceOpenWithLinksFields[] =
    "kind,id,openWithLinks/*";
const char kFileResourceShareLinkFields[] =
    "kind,id,shareLink";
const char kFileListFields[] =
    "kind,items(kind,id,title,createdDate,sharedWithMeDate,"
    "mimeType,md5Checksum,fileSize,labels/trashed,imageMediaMetadata/width,"
    "imageMediaMetadata/height,imageMediaMetadata/rotation,etag,"
    "parents(id,parentLink),alternateLink,"
    "modifiedDate,lastViewedByMeDate,shared),nextLink";
const char kChangeListFields[] =
    "kind,items(file(kind,id,title,createdDate,sharedWithMeDate,"
    "mimeType,md5Checksum,fileSize,labels/trashed,imageMediaMetadata/width,"
    "imageMediaMetadata/height,imageMediaMetadata/rotation,etag,"
    "parents(id,parentLink),alternateLink,modifiedDate,"
    "lastViewedByMeDate,shared),deleted,id,fileId,modificationDate),nextLink,"
    "largestChangeId";

void ExtractOpenUrlAndRun(const std::string& app_id,
                          const AuthorizeAppCallback& callback,
                          DriveApiErrorCode error,
                          scoped_ptr<FileResource> value) {
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, GURL());
    return;
  }

  const std::vector<FileResource::OpenWithLink>& open_with_links =
      value->open_with_links();
  for (size_t i = 0; i < open_with_links.size(); ++i) {
    if (open_with_links[i].app_id == app_id) {
      callback.Run(HTTP_SUCCESS, open_with_links[i].open_url);
      return;
    }
  }

  // Not found.
  callback.Run(DRIVE_OTHER_ERROR, GURL());
}

void ExtractShareUrlAndRun(const GetShareUrlCallback& callback,
                           DriveApiErrorCode error,
                           scoped_ptr<FileResource> value) {
  callback.Run(error, value ? value->share_link() : GURL());
}

// Ignores the |entry|, and runs the |callback|.
void EntryActionCallbackAdapter(
    const EntryActionCallback& callback,
    DriveApiErrorCode error, scoped_ptr<FileResource> entry) {
  callback.Run(error);
}

// The resource ID for the root directory for Drive API is defined in the spec:
// https://developers.google.com/drive/folder
const char kDriveApiRootDirectoryResourceId[] = "root";

}  // namespace

BatchRequestConfigurator::BatchRequestConfigurator(
    const base::WeakPtr<google_apis::drive::BatchUploadRequest>& batch_request,
    base::SequencedTaskRunner* task_runner,
    const google_apis::DriveApiUrlGenerator& url_generator,
    const google_apis::CancelCallback& cancel_callback)
    : batch_request_(batch_request),
      task_runner_(task_runner),
      url_generator_(url_generator),
      cancel_callback_(cancel_callback) {
}

BatchRequestConfigurator::~BatchRequestConfigurator() {
  // The batch requst has not been committed.
  if (batch_request_)
    cancel_callback_.Run();
}

google_apis::CancelCallback BatchRequestConfigurator::MultipartUploadNewFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const base::FilePath& local_file_path,
    const UploadNewFileOptions& options,
    const google_apis::FileResourceCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());

  scoped_ptr<google_apis::BatchableDelegate> delegate(
      new google_apis::drive::MultipartUploadNewFileDelegate(
          task_runner_.get(), title, parent_resource_id, content_type,
          content_length, options.modified_date, options.last_viewed_by_me_date,
          local_file_path, options.properties, url_generator_, callback,
          progress_callback));
  // Batch request can be null when pre-authorization for the requst is failed
  // in request sender.
  if (batch_request_)
    batch_request_->AddRequest(delegate.release());
  else
    delegate->NotifyError(DRIVE_OTHER_ERROR);
  return cancel_callback_;
}

google_apis::CancelCallback
BatchRequestConfigurator::MultipartUploadExistingFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const UploadExistingFileOptions& options,
    const google_apis::FileResourceCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!callback.is_null());

  scoped_ptr<google_apis::BatchableDelegate> delegate(
      new google_apis::drive::MultipartUploadExistingFileDelegate(
          task_runner_.get(), options.title, resource_id,
          options.parent_resource_id, content_type, content_length,
          options.modified_date, options.last_viewed_by_me_date,
          local_file_path, options.etag, options.properties, url_generator_,
          callback, progress_callback));
  // Batch request can be null when pre-authorization for the requst is failed
  // in request sender.
  if (batch_request_)
    batch_request_->AddRequest(delegate.release());
  else
    delegate->NotifyError(DRIVE_OTHER_ERROR);
  return cancel_callback_;
}

void BatchRequestConfigurator::Commit() {
  DCHECK(CalledOnValidThread());
  if (!batch_request_)
    return;
  batch_request_->Commit();
  batch_request_.reset();
}

DriveAPIService::DriveAPIService(
    OAuth2TokenService* oauth2_token_service,
    net::URLRequestContextGetter* url_request_context_getter,
    base::SequencedTaskRunner* blocking_task_runner,
    const GURL& base_url,
    const GURL& base_download_url,
    const std::string& custom_user_agent)
    : oauth2_token_service_(oauth2_token_service),
      url_request_context_getter_(url_request_context_getter),
      blocking_task_runner_(blocking_task_runner),
      url_generator_(base_url, base_download_url),
      custom_user_agent_(custom_user_agent) {
}

DriveAPIService::~DriveAPIService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (sender_.get())
    sender_->auth_service()->RemoveObserver(this);
}

void DriveAPIService::Initialize(const std::string& account_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<std::string> scopes;
  scopes.push_back(kDriveScope);
  scopes.push_back(kDriveAppsReadonlyScope);
  scopes.push_back(kDriveAppsScope);

  // Note: The following scope is used to support GetShareUrl on Drive API v2.
  // Unfortunately, there is no support on Drive API v2, so we need to fall back
  // to GData WAPI for the GetShareUrl.
  scopes.push_back(kDocsListScope);

  sender_.reset(new RequestSender(
      new google_apis::AuthService(oauth2_token_service_,
                                   account_id,
                                   url_request_context_getter_.get(),
                                   scopes),
      url_request_context_getter_.get(),
      blocking_task_runner_.get(),
      custom_user_agent_));
  sender_->auth_service()->AddObserver(this);
}

void DriveAPIService::AddObserver(DriveServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveAPIService::RemoveObserver(DriveServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool DriveAPIService::CanSendRequest() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return HasRefreshToken();
}

std::string DriveAPIService::GetRootResourceId() const {
  return kDriveApiRootDirectoryResourceId;
}

CancelCallback DriveAPIService::GetAllFileList(
    const FileListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  FilesListRequest* request = new FilesListRequest(
      sender_.get(), url_generator_, callback);
  request->set_max_results(kMaxNumFilesResourcePerRequest);
  request->set_q("trashed = false");  // Exclude trashed files.
  request->set_fields(kFileListFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::GetFileListInDirectory(
    const std::string& directory_resource_id,
    const FileListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!directory_resource_id.empty());
  DCHECK(!callback.is_null());

  // Because children.list method on Drive API v2 returns only the list of
  // children's references, but we need all file resource list.
  // So, here we use files.list method instead, with setting parents query.
  // After the migration from GData WAPI to Drive API v2, we should clean the
  // code up by moving the responsibility to include "parents" in the query
  // to client side.
  // We aren't interested in files in trash in this context, neither.
  FilesListRequest* request = new FilesListRequest(
      sender_.get(), url_generator_, callback);
  request->set_max_results(kMaxNumFilesResourcePerRequest);
  request->set_q(base::StringPrintf(
      "'%s' in parents and trashed = false",
      util::EscapeQueryStringValue(directory_resource_id).c_str()));
  request->set_fields(kFileListFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::Search(
    const std::string& search_query,
    const FileListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!search_query.empty());
  DCHECK(!callback.is_null());

  FilesListRequest* request = new FilesListRequest(
      sender_.get(), url_generator_, callback);
  request->set_max_results(kMaxNumFilesResourcePerRequestForSearch);
  request->set_q(util::TranslateQuery(search_query));
  request->set_fields(kFileListFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    const FileListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!title.empty());
  DCHECK(!callback.is_null());

  std::string query;
  base::StringAppendF(&query, "title = '%s'",
                      util::EscapeQueryStringValue(title).c_str());
  if (!directory_resource_id.empty()) {
    base::StringAppendF(
        &query, " and '%s' in parents",
        util::EscapeQueryStringValue(directory_resource_id).c_str());
  }
  query += " and trashed = false";

  FilesListRequest* request = new FilesListRequest(
      sender_.get(), url_generator_, callback);
  request->set_max_results(kMaxNumFilesResourcePerRequest);
  request->set_q(query);
  request->set_fields(kFileListFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::GetChangeList(
    int64 start_changestamp,
    const ChangeListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  ChangesListRequest* request = new ChangesListRequest(
      sender_.get(), url_generator_, callback);
  request->set_max_results(kMaxNumFilesResourcePerRequest);
  request->set_start_change_id(start_changestamp);
  request->set_fields(kChangeListFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::GetRemainingChangeList(
    const GURL& next_link,
    const ChangeListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!next_link.is_empty());
  DCHECK(!callback.is_null());

  ChangesListNextPageRequest* request = new ChangesListNextPageRequest(
      sender_.get(), callback);
  request->set_next_link(next_link);
  request->set_fields(kChangeListFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::GetRemainingFileList(
    const GURL& next_link,
    const FileListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!next_link.is_empty());
  DCHECK(!callback.is_null());

  FilesListNextPageRequest* request = new FilesListNextPageRequest(
      sender_.get(), callback);
  request->set_next_link(next_link);
  request->set_fields(kFileListFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::GetFileResource(
    const std::string& resource_id,
    const FileResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  FilesGetRequest* request = new FilesGetRequest(
      sender_.get(), url_generator_, google_apis::IsGoogleChromeAPIKeyUsed(),
      callback);
  request->set_file_id(resource_id);
  request->set_fields(kFileResourceFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::GetShareUrl(
    const std::string& resource_id,
    const GURL& embed_origin,
    const GetShareUrlCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  if (!google_apis::IsGoogleChromeAPIKeyUsed()) {
    LOG(ERROR) << "Only the official build of Chrome OS can open share dialogs "
               << "from the file manager.";
  }

  FilesGetRequest* request = new FilesGetRequest(
      sender_.get(), url_generator_, google_apis::IsGoogleChromeAPIKeyUsed(),
      base::Bind(&ExtractShareUrlAndRun, callback));
  request->set_file_id(resource_id);
  request->set_fields(kFileResourceShareLinkFields);
  request->set_embed_origin(embed_origin);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::GetAboutResource(
    const AboutResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  AboutGetRequest* request =
      new AboutGetRequest(sender_.get(), url_generator_, callback);
  request->set_fields(kAboutResourceFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::GetAppList(const AppListCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new AppsListRequest(sender_.get(), url_generator_,
                          google_apis::IsGoogleChromeAPIKeyUsed(),
                          callback));
}

CancelCallback DriveAPIService::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!download_action_callback.is_null());
  // get_content_callback may be null.

  return sender_->StartRequestWithRetry(
      new DownloadFileRequest(sender_.get(),
                              url_generator_,
                              resource_id,
                              local_cache_path,
                              download_action_callback,
                              get_content_callback,
                              progress_callback));
}

CancelCallback DriveAPIService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  FilesDeleteRequest* request = new FilesDeleteRequest(
      sender_.get(), url_generator_, callback);
  request->set_file_id(resource_id);
  request->set_etag(etag);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::TrashResource(
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  FilesTrashRequest* request = new FilesTrashRequest(
      sender_.get(), url_generator_,
      base::Bind(&EntryActionCallbackAdapter, callback));
  request->set_file_id(resource_id);
  request->set_fields(kFileResourceFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const AddNewDirectoryOptions& options,
    const FileResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  FilesInsertRequest* request = new FilesInsertRequest(
      sender_.get(), url_generator_, callback);
  request->set_last_viewed_by_me_date(options.last_viewed_by_me_date);
  request->set_mime_type(kFolderMimeType);
  request->set_modified_date(options.modified_date);
  request->add_parent(parent_resource_id);
  request->set_title(directory_title);
  request->set_properties(options.properties);
  request->set_fields(kFileResourceFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::CopyResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const FileResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  FilesCopyRequest* request = new FilesCopyRequest(
      sender_.get(), url_generator_, callback);
  request->set_file_id(resource_id);
  request->add_parent(parent_resource_id);
  request->set_title(new_title);
  request->set_modified_date(last_modified);
  request->set_fields(kFileResourceFields);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::UpdateResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const base::Time& last_viewed_by_me,
    const google_apis::drive::Properties& properties,
    const FileResourceCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  FilesPatchRequest* request = new FilesPatchRequest(
      sender_.get(), url_generator_, callback);
  request->set_file_id(resource_id);
  request->set_title(new_title);
  if (!parent_resource_id.empty())
    request->add_parent(parent_resource_id);
  if (!last_modified.is_null()) {
    // Need to set setModifiedDate to true to overwrite modifiedDate.
    request->set_set_modified_date(true);
    request->set_modified_date(last_modified);
  }
  if (!last_viewed_by_me.is_null()) {
    // Need to set updateViewedDate to false, otherwise the lastViewedByMeDate
    // will be set to the request time (not the specified time via request).
    request->set_update_viewed_date(false);
    request->set_last_viewed_by_me_date(last_viewed_by_me);
  }
  request->set_fields(kFileResourceFields);
  request->set_properties(properties);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  ChildrenInsertRequest* request =
      new ChildrenInsertRequest(sender_.get(), url_generator_, callback);
  request->set_folder_id(parent_resource_id);
  request->set_id(resource_id);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  ChildrenDeleteRequest* request =
      new ChildrenDeleteRequest(sender_.get(), url_generator_, callback);
  request->set_child_id(resource_id);
  request->set_folder_id(parent_resource_id);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::InitiateUploadNewFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const UploadNewFileOptions& options,
    const InitiateUploadCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  InitiateUploadNewFileRequest* request =
      new InitiateUploadNewFileRequest(sender_.get(),
                                       url_generator_,
                                       content_type,
                                       content_length,
                                       parent_resource_id,
                                       title,
                                       callback);
  request->set_modified_date(options.modified_date);
  request->set_last_viewed_by_me_date(options.last_viewed_by_me_date);
  request->set_properties(options.properties);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::InitiateUploadExistingFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const UploadExistingFileOptions& options,
    const InitiateUploadCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  InitiateUploadExistingFileRequest* request =
      new InitiateUploadExistingFileRequest(sender_.get(),
                                            url_generator_,
                                            content_type,
                                            content_length,
                                            resource_id,
                                            options.etag,
                                            callback);
  request->set_parent_resource_id(options.parent_resource_id);
  request->set_title(options.title);
  request->set_modified_date(options.modified_date);
  request->set_last_viewed_by_me_date(options.last_viewed_by_me_date);
  request->set_properties(options.properties);
  return sender_->StartRequestWithRetry(request);
}

CancelCallback DriveAPIService::ResumeUpload(
    const GURL& upload_url,
    int64 start_position,
    int64 end_position,
    int64 content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    const UploadRangeCallback& callback,
    const ProgressCallback& progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new ResumeUploadRequest(
          sender_.get(),
          upload_url,
          start_position,
          end_position,
          content_length,
          content_type,
          local_file_path,
          callback,
          progress_callback));
}

CancelCallback DriveAPIService::GetUploadStatus(
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(new GetUploadStatusRequest(
      sender_.get(),
      upload_url,
      content_length,
      callback));
}

CancelCallback DriveAPIService::MultipartUploadNewFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const base::FilePath& local_file_path,
    const drive::UploadNewFileOptions& options,
    const FileResourceCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new google_apis::drive::SingleBatchableDelegateRequest(
          sender_.get(),
          new google_apis::drive::MultipartUploadNewFileDelegate(
              sender_->blocking_task_runner(), title, parent_resource_id,
              content_type, content_length, options.modified_date,
              options.last_viewed_by_me_date, local_file_path,
              options.properties, url_generator_, callback,
              progress_callback)));
}

CancelCallback DriveAPIService::MultipartUploadExistingFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const drive::UploadExistingFileOptions& options,
    const FileResourceCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  return sender_->StartRequestWithRetry(
      new google_apis::drive::SingleBatchableDelegateRequest(
          sender_.get(),
          new google_apis::drive::MultipartUploadExistingFileDelegate(
              sender_->blocking_task_runner(), options.title, resource_id,
              options.parent_resource_id, content_type, content_length,
              options.modified_date, options.last_viewed_by_me_date,
              local_file_path, options.etag, options.properties, url_generator_,
              callback, progress_callback)));
}

CancelCallback DriveAPIService::AuthorizeApp(
    const std::string& resource_id,
    const std::string& app_id,
    const AuthorizeAppCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  // Files.Authorize is only available for whitelisted clients like official
  // Google Chrome. In other cases, we fall back to Files.Get that returns the
  // same value as Files.Authorize without doing authorization. In that case,
  // the app can open if it was authorized by other means (from whitelisted
  // clients or drive.google.com web UI.)
  if (google_apis::IsGoogleChromeAPIKeyUsed()) {
    google_apis::drive::FilesAuthorizeRequest* request =
        new google_apis::drive::FilesAuthorizeRequest(
            sender_.get(), url_generator_,
            base::Bind(&ExtractOpenUrlAndRun, app_id, callback));
    request->set_app_id(app_id);
    request->set_file_id(resource_id);
    request->set_fields(kFileResourceOpenWithLinksFields);
    return sender_->StartRequestWithRetry(request);
  } else {
    FilesGetRequest* request = new FilesGetRequest(
        sender_.get(), url_generator_, google_apis::IsGoogleChromeAPIKeyUsed(),
        base::Bind(&ExtractOpenUrlAndRun, app_id, callback));
    request->set_file_id(resource_id);
    request->set_fields(kFileResourceOpenWithLinksFields);
    return sender_->StartRequestWithRetry(request);
  }
}

CancelCallback DriveAPIService::UninstallApp(
    const std::string& app_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  google_apis::drive::AppsDeleteRequest* request =
      new google_apis::drive::AppsDeleteRequest(sender_.get(), url_generator_,
                                                callback);
  request->set_app_id(app_id);
  return sender_->StartRequestWithRetry(request);
}

google_apis::CancelCallback DriveAPIService::AddPermission(
    const std::string& resource_id,
    const std::string& email,
    google_apis::drive::PermissionRole role,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  google_apis::drive::PermissionsInsertRequest* request =
      new google_apis::drive::PermissionsInsertRequest(sender_.get(),
                                                       url_generator_,
                                                       callback);
  request->set_id(resource_id);
  request->set_role(role);
  request->set_type(google_apis::drive::PERMISSION_TYPE_USER);
  request->set_value(email);
  return sender_->StartRequestWithRetry(request);
}

bool DriveAPIService::HasAccessToken() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sender_->auth_service()->HasAccessToken();
}

void DriveAPIService::RequestAccessToken(const AuthStatusCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());

  const std::string access_token = sender_->auth_service()->access_token();
  if (!access_token.empty()) {
    callback.Run(google_apis::HTTP_NOT_MODIFIED, access_token);
    return;
  }

  // Retrieve the new auth token.
  sender_->auth_service()->StartAuthentication(callback);
}

bool DriveAPIService::HasRefreshToken() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sender_->auth_service()->HasRefreshToken();
}

void DriveAPIService::ClearAccessToken() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sender_->auth_service()->ClearAccessToken();
}

void DriveAPIService::ClearRefreshToken() {
  DCHECK(thread_checker_.CalledOnValidThread());
  sender_->auth_service()->ClearRefreshToken();
}

void DriveAPIService::OnOAuth2RefreshTokenChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (CanSendRequest()) {
    FOR_EACH_OBSERVER(
        DriveServiceObserver, observers_, OnReadyToSendRequests());
  } else if (!HasRefreshToken()) {
    FOR_EACH_OBSERVER(
        DriveServiceObserver, observers_, OnRefreshTokenInvalid());
  }
}

scoped_ptr<BatchRequestConfiguratorInterface>
DriveAPIService::StartBatchRequest() {
  scoped_ptr<google_apis::drive::BatchUploadRequest> request(
      new google_apis::drive::BatchUploadRequest(sender_.get(),
                                                 url_generator_));
  const base::WeakPtr<google_apis::drive::BatchUploadRequest> weak_ref =
      request->GetWeakPtrAsBatchUploadRequest();
  // Have sender_ manage the lifetime of the request.
  // TODO(hirono): Currently we need to pass the ownership of the request to
  // RequestSender before the request is committed because the request has a
  // reference to RequestSender and we should ensure to delete the request when
  // the sender is deleted. Resolve the circulating dependency and fix it.
  const google_apis::CancelCallback callback =
      sender_->StartRequestWithRetry(request.release());
  return make_scoped_ptr<BatchRequestConfiguratorInterface>(
      new BatchRequestConfigurator(weak_ref, sender_->blocking_task_runner(),
                                   url_generator_, callback));
}

}  // namespace drive

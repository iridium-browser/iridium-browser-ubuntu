// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_media_metadata_parser.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/blob_reader.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/utility_process_host.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace metadata {

SafeMediaMetadataParser::SafeMediaMetadataParser(Profile* profile,
                                                 const std::string& blob_uuid,
                                                 int64_t blob_size,
                                                 const std::string& mime_type,
                                                 bool get_attached_images)
    : profile_(profile),
      blob_uuid_(blob_uuid),
      blob_size_(blob_size),
      mime_type_(mime_type),
      get_attached_images_(get_attached_images),
      parser_state_(INITIAL_STATE) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void SafeMediaMetadataParser::Start(const DoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SafeMediaMetadataParser::StartWorkOnIOThread, this,
                 callback));
}

SafeMediaMetadataParser::~SafeMediaMetadataParser() {
}

void SafeMediaMetadataParser::StartWorkOnIOThread(
    const DoneCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(INITIAL_STATE, parser_state_);
  DCHECK(!callback.is_null());

  callback_ = callback;

  utility_process_host_ = content::UtilityProcessHost::Create(
      this, base::ThreadTaskRunnerHandle::Get())->AsWeakPtr();
  utility_process_host_->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_MEDIA_FILE_CHECKER_NAME));

  utility_process_host_->Send(
      new ChromeUtilityMsg_ParseMediaMetadata(mime_type_, blob_size_,
                                              get_attached_images_));

  parser_state_ = STARTED_PARSING_STATE;
}

void SafeMediaMetadataParser::OnParseMediaMetadataFinished(
    bool parse_success, const base::DictionaryValue& metadata_dictionary,
    const std::vector<AttachedImage>& attached_images) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback_.is_null());

  if (parser_state_ != STARTED_PARSING_STATE)
    return;

  // We need to make a scoped copy of this vector since it will be destroyed
  // at the end of the IPC message handler.
  std::unique_ptr<std::vector<metadata::AttachedImage>> attached_images_copy =
      base::WrapUnique(
          new std::vector<metadata::AttachedImage>(attached_images));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback_, parse_success,
                 base::Passed(base::WrapUnique(metadata_dictionary.DeepCopy())),
                 base::Passed(&attached_images_copy)));
  parser_state_ = FINISHED_PARSING_STATE;
}

void SafeMediaMetadataParser::OnUtilityProcessRequestBlobBytes(
    int64_t request_id,
    int64_t byte_start,
    int64_t length) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&SafeMediaMetadataParser::StartBlobReaderOnUIThread, this,
                 request_id, byte_start, length));
}

void SafeMediaMetadataParser::StartBlobReaderOnUIThread(int64_t request_id,
                                                        int64_t byte_start,
                                                        int64_t length) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // BlobReader is self-deleting.
  BlobReader* reader = new BlobReader(profile_, blob_uuid_, base::Bind(
      &SafeMediaMetadataParser::OnBlobReaderDoneOnUIThread, this, request_id));
  reader->SetByteRange(byte_start, length);
  reader->Start();
}

void SafeMediaMetadataParser::OnBlobReaderDoneOnUIThread(
    int64_t request_id,
    std::unique_ptr<std::string> data,
    int64_t /* blob_total_size */) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SafeMediaMetadataParser::FinishRequestBlobBytes, this,
                 request_id, base::Passed(std::move(data))));
}

void SafeMediaMetadataParser::FinishRequestBlobBytes(
    int64_t request_id,
    std::unique_ptr<std::string> data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!utility_process_host_.get())
    return;
  utility_process_host_->Send(new ChromeUtilityMsg_RequestBlobBytes_Finished(
      request_id, *data));
}

void SafeMediaMetadataParser::OnProcessCrashed(int exit_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!callback_.is_null());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback_, false,
                 base::Passed(std::unique_ptr<base::DictionaryValue>()),
                 base::Passed(std::unique_ptr<std::vector<AttachedImage>>())));
  parser_state_ = FINISHED_PARSING_STATE;
}

bool SafeMediaMetadataParser::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafeMediaMetadataParser, message)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_ParseMediaMetadata_Finished,
        OnParseMediaMetadataFinished)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_RequestBlobBytes,
        OnUtilityProcessRequestBlobBytes)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace metadata

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_CORE_ATTACHMENTS_ATTACHMENT_UPLOADER_H_
#define COMPONENTS_SYNC_CORE_ATTACHMENTS_ATTACHMENT_UPLOADER_H_

#include "base/callback.h"
#include "components/sync/api/attachments/attachment.h"

namespace syncer {

// AttachmentUploader is responsible for uploading attachments to the server.
class AttachmentUploader {
 public:
  // The result of an UploadAttachment operation.
  enum UploadResult {
    UPLOAD_SUCCESS,            // No error, attachment was uploaded
                               // successfully.
    UPLOAD_TRANSIENT_ERROR,    // A transient error occurred, try again later.
    UPLOAD_UNSPECIFIED_ERROR,  // An unspecified error occurred.
  };

  typedef base::Callback<void(const UploadResult&, const AttachmentId&)>
      UploadCallback;

  AttachmentUploader();
  virtual ~AttachmentUploader();

  // Upload |attachment| and invoke |callback| when done.
  //
  // |callback| will be invoked when the operation has completed (successfully
  // or otherwise).
  //
  // |callback| will receive an UploadResult code and the AttachmentId of the
  // newly uploaded attachment.
  virtual void UploadAttachment(const Attachment& attachment,
                                const UploadCallback& callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_CORE_ATTACHMENTS_ATTACHMENT_UPLOADER_H_

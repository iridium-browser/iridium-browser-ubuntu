// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/test_url_loader_client.h"

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TestURLLoaderClient::TestURLLoaderClient() : binding_(this) {}
TestURLLoaderClient::~TestURLLoaderClient() {}

void TestURLLoaderClient::OnReceiveResponse(
    const ResourceResponseHead& response_head,
    mojom::DownloadedTempFilePtr downloaded_file) {
  EXPECT_FALSE(has_received_response_);
  EXPECT_FALSE(has_received_cached_metadata_);
  EXPECT_FALSE(has_received_completion_);
  has_received_response_ = true;
  response_head_ = response_head;
  if (quit_closure_for_on_receive_response_)
    quit_closure_for_on_receive_response_.Run();
}

void TestURLLoaderClient::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  EXPECT_FALSE(has_received_cached_metadata_);
  EXPECT_FALSE(response_body_.is_valid());
  EXPECT_FALSE(has_received_response_);
  // Use ClearHasReceivedRedirect to accept more redirects.
  EXPECT_FALSE(has_received_redirect_);
  EXPECT_FALSE(has_received_completion_);
  has_received_redirect_ = true;
  redirect_info_ = redirect_info;
  response_head_ = response_head;
  if (quit_closure_for_on_receive_redirect_)
    quit_closure_for_on_receive_redirect_.Run();
}

void TestURLLoaderClient::OnDataDownloaded(int64_t data_length,
                                           int64_t encoded_data_length) {
  EXPECT_TRUE(has_received_response_);
  EXPECT_FALSE(has_received_completion_);
  has_data_downloaded_ = true;
  download_data_length_ += data_length;
  encoded_download_data_length_ += encoded_data_length;
  if (quit_closure_for_on_data_downloaded_)
    quit_closure_for_on_data_downloaded_.Run();
}

void TestURLLoaderClient::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  EXPECT_FALSE(has_received_cached_metadata_);
  EXPECT_TRUE(has_received_response_);
  EXPECT_FALSE(has_received_completion_);
  has_received_cached_metadata_ = true;
  cached_metadata_ =
      std::string(reinterpret_cast<const char*>(data.data()), data.size());
  if (quit_closure_for_on_receive_cached_metadata_)
    quit_closure_for_on_receive_cached_metadata_.Run();
}

void TestURLLoaderClient::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  EXPECT_TRUE(has_received_response_);
  EXPECT_FALSE(has_received_completion_);
  EXPECT_GT(transfer_size_diff, 0);
  body_transfer_size_ += transfer_size_diff;
}

void TestURLLoaderClient::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  EXPECT_TRUE(has_received_response_);
  EXPECT_FALSE(has_received_completion_);
  response_body_ = std::move(body);
  if (quit_closure_for_on_start_loading_response_body_)
    quit_closure_for_on_start_loading_response_body_.Run();
}

void TestURLLoaderClient::OnComplete(
    const ResourceRequestCompletionStatus& status) {
  EXPECT_FALSE(has_received_completion_);
  has_received_completion_ = true;
  completion_status_ = status;
  if (quit_closure_for_on_complete_)
    quit_closure_for_on_complete_.Run();
}

void TestURLLoaderClient::ClearHasReceivedRedirect() {
  has_received_redirect_ = false;
}

mojom::URLLoaderClientAssociatedPtrInfo
TestURLLoaderClient::CreateRemoteAssociatedPtrInfo(
    mojo::AssociatedGroup* associated_group) {
  mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info;
  binding_.Bind(&client_ptr_info, associated_group);
  return client_ptr_info;
}

void TestURLLoaderClient::Unbind() {
  binding_.Unbind();
  response_body_.reset();
}

void TestURLLoaderClient::RunUntilResponseReceived() {
  if (has_received_response_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_receive_response_ = run_loop.QuitClosure();
  run_loop.Run();
  quit_closure_for_on_receive_response_.Reset();
}

void TestURLLoaderClient::RunUntilRedirectReceived() {
  if (has_received_redirect_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_receive_redirect_ = run_loop.QuitClosure();
  run_loop.Run();
  quit_closure_for_on_receive_redirect_.Reset();
}

void TestURLLoaderClient::RunUntilDataDownloaded() {
  if (has_data_downloaded_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_data_downloaded_ = run_loop.QuitClosure();
  run_loop.Run();
  quit_closure_for_on_data_downloaded_.Reset();
}

void TestURLLoaderClient::RunUntilCachedMetadataReceived() {
  if (has_received_cached_metadata_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_receive_cached_metadata_ = run_loop.QuitClosure();
  run_loop.Run();
  quit_closure_for_on_receive_cached_metadata_.Reset();
}

void TestURLLoaderClient::RunUntilResponseBodyArrived() {
  if (response_body_.is_valid())
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_start_loading_response_body_ = run_loop.QuitClosure();
  run_loop.Run();
  quit_closure_for_on_start_loading_response_body_.Reset();
}

void TestURLLoaderClient::RunUntilComplete() {
  if (has_received_completion_)
    return;
  base::RunLoop run_loop;
  quit_closure_for_on_complete_ = run_loop.QuitClosure();
  run_loop.Run();
  quit_closure_for_on_complete_.Reset();
}

}  // namespace content

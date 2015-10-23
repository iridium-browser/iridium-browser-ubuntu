// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_request_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequest implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequest::WebDataRequest(WebDataServiceConsumer* consumer,
                               WebDataRequestManager* manager)
    : manager_(manager), cancelled_(false), consumer_(consumer) {
  handle_ = manager_->GetNextRequestHandle();
  message_loop_ = base::MessageLoop::current();
  manager_->RegisterRequest(this);
}

WebDataRequest::~WebDataRequest() {
  if (manager_) {
    manager_->CancelRequest(handle_);
  }
  if (result_.get()) {
    result_->Destroy();
  }
}

WebDataServiceBase::Handle WebDataRequest::GetHandle() const {
  return handle_;
}

WebDataServiceConsumer* WebDataRequest::GetConsumer() const {
  return consumer_;
}

base::MessageLoop* WebDataRequest::GetMessageLoop() const {
  return message_loop_;
}

bool WebDataRequest::IsCancelled() const {
  base::AutoLock l(cancel_lock_);
  return cancelled_;
}

void WebDataRequest::Cancel() {
  base::AutoLock l(cancel_lock_);
  cancelled_ = true;
  consumer_ = NULL;
  manager_ = NULL;
}

void WebDataRequest::OnComplete() {
  manager_= NULL;
}

void WebDataRequest::SetResult(scoped_ptr<WDTypedResult> r) {
  result_ = r.Pass();
}

scoped_ptr<WDTypedResult> WebDataRequest::GetResult(){
  return result_.Pass();
}

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequestManager implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequestManager::WebDataRequestManager()
    : next_request_handle_(1) {
}

WebDataRequestManager::~WebDataRequestManager() {
  base::AutoLock l(pending_lock_);
  for (RequestMap::iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    i->second->Cancel();
  }
  pending_requests_.clear();
}

void WebDataRequestManager::RegisterRequest(WebDataRequest* request) {
  base::AutoLock l(pending_lock_);
  pending_requests_[request->GetHandle()] = request;
}

int WebDataRequestManager::GetNextRequestHandle() {
  base::AutoLock l(pending_lock_);
  return ++next_request_handle_;
}

void WebDataRequestManager::CancelRequest(WebDataServiceBase::Handle h) {
  base::AutoLock l(pending_lock_);
  RequestMap::iterator i = pending_requests_.find(h);
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Canceling a nonexistent web data service request";
    return;
  }
  i->second->Cancel();
  pending_requests_.erase(i);
}

void WebDataRequestManager::RequestCompleted(
    scoped_ptr<WebDataRequest> request) {
  base::MessageLoop* loop = request->GetMessageLoop();
  loop->task_runner()->PostTask(
      FROM_HERE, base::Bind(&WebDataRequestManager::RequestCompletedOnThread,
                            this, base::Passed(&request)));
}

void WebDataRequestManager::RequestCompletedOnThread(
    scoped_ptr<WebDataRequest> request) {
  if (request->IsCancelled())
    return;

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 WebDataRequestManager::RequestCompletedOnThread::UpdateMap"));
  {
    base::AutoLock l(pending_lock_);
    RequestMap::iterator i = pending_requests_.find(request->GetHandle());
    if (i == pending_requests_.end()) {
      NOTREACHED() << "Request completed called for an unknown request";
      return;
    }

    // Take ownership of the request object and remove it from the map.
    pending_requests_.erase(i);
  }

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 "
          "WebDataRequestManager::RequestCompletedOnThread::NotifyConsumer"));

  // Notify the consumer if needed.
  if (!request->IsCancelled()) {
    WebDataServiceConsumer* consumer = request->GetConsumer();
    request->OnComplete();
    if (consumer) {
      scoped_ptr<WDTypedResult> r = request->GetResult();
      consumer->OnWebDataServiceRequestDone(request->GetHandle(), r.get());
    }
  }

}

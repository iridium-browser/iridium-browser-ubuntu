// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_STORE_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "components/offline_pages/background/request_queue.h"
#include "components/offline_pages/background/save_page_request.h"
#include "components/offline_pages/offline_page_item.h"

namespace offline_pages {

// Interface for classes storing save page requests.
class RequestQueueStore {
 public:
  enum class UpdateStatus {
    ADDED,    // Request was added successfully.
    UPDATED,  // Request was updated successfully.
    FAILED,   // Add or update attempt failed.
  };

  typedef base::Callback<void(
      bool /* success */,
      const std::vector<SavePageRequest>& /* requests */)>
      GetRequestsCallback;
  typedef base::Callback<void(UpdateStatus)> UpdateCallback;
  // TODO(petewil) - UpdateMultiple looks exactly like Remove, consider
  // merging them into a single callback.
  typedef base::Callback<void(
      const RequestQueue::UpdateMultipleRequestResults& /* statuses*/,
      const std::vector<SavePageRequest>& /* requests */)>
      UpdateMultipleRequestsCallback;
  typedef base::Callback<void(
      const RequestQueue::UpdateMultipleRequestResults& /* statuses */,
      const std::vector<SavePageRequest>& /* requests */)>
      RemoveCallback;
  typedef base::Callback<void(bool /* success */)> ResetCallback;

  virtual ~RequestQueueStore(){};

  // Gets all of the requests from the store.
  virtual void GetRequests(const GetRequestsCallback& callback) = 0;

  // Asynchronously adds or updates request in store.
  // Result of the update is passed in the callback.
  virtual void AddOrUpdateRequest(const SavePageRequest& request,
                                  const UpdateCallback& callback) = 0;

  // Asynchronously removes requests from the store using their IDs.
  // Result of the update, and a number of removed pages is passed in the
  // callback.
  // Result of remove should be false, when one of the provided items couldn't
  // be deleted, e.g. because it was missing.
  virtual void RemoveRequests(const std::vector<int64_t>& request_ids,
                              const RemoveCallback& callback) = 0;

  // Asynchronously changes the state of requests from the store using their
  // request id.
  virtual void ChangeRequestsState(
      const std::vector<int64_t>& request_ids,
      const SavePageRequest::RequestState new_state,
      const UpdateMultipleRequestsCallback& callback) = 0;

  // Resets the store.
  virtual void Reset(const ResetCallback& callback) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_REQUEST_QUEUE_STORE_H_

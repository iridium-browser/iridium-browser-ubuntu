// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_MEDIA_SINKS_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_MEDIA_SINKS_OBSERVER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "content/public/browser/presentation_service_delegate.h"

namespace content {
class PresentationScreenAvailabilityListener;
}

namespace media_router {

class MediaRouter;
class MediaSource;

// Receives SinkQueryResults for |source| from |router| and propagates results
// to |listener|. |listener| is notified only when availability status has
// changed, i.e. sinks have become available or sinks are no longer available.
class PresentationMediaSinksObserver : public MediaSinksObserver {
 public:
  // |router|: Media router that publishes sink query results.
  // |listener|: Notified when sinks availability changes.
  // |source|: Filters available sink.
  // Does not take ownership of |listener| or |router|.
  PresentationMediaSinksObserver(
      MediaRouter* router,
      content::PresentationScreenAvailabilityListener* listener,
      const MediaSource& source);
  ~PresentationMediaSinksObserver() override;

  // MediaSinksObserver implementation.
  void OnSinksReceived(const std::vector<MediaSink>& result) override;

  content::PresentationScreenAvailabilityListener* listener() const {
    return listener_;
  }

 private:
  enum Availability { UNKNOWN, AVAILABLE, UNAVAILABLE };

  content::PresentationScreenAvailabilityListener* listener_;
  Availability previous_availablity_;

  DISALLOW_COPY_AND_ASSIGN(PresentationMediaSinksObserver);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PRESENTATION_MEDIA_SINKS_OBSERVER_H_

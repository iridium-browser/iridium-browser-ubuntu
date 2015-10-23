// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEER_CONNECTION_IDENTITY_STORE_H_
#define CONTENT_RENDERER_MEDIA_PEER_CONNECTION_IDENTITY_STORE_H_

#include "base/basictypes.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/libjingle/source/talk/app/webrtc/dtlsidentitystore.h"
#include "url/gurl.h"

namespace content {

// This class is associated with a peer connection and handles WebRTC DTLS
// identity requests by delegating to the per-renderer WebRTCIdentityProxy.
class PeerConnectionIdentityStore
    : public webrtc::DtlsIdentityStoreInterface {
 public:
  PeerConnectionIdentityStore(const GURL& origin,
                              const GURL& first_party_for_cookies);
  ~PeerConnectionIdentityStore() override;

  // webrtc::DtlsIdentityStoreInterface override;
  void RequestIdentity(
      rtc::KeyType key_type,
      const rtc::scoped_refptr<webrtc::DtlsIdentityRequestObserver>& observer)
          override;

 private:
  base::ThreadChecker signaling_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  const GURL url_;
  const GURL first_party_for_cookies_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionIdentityStore);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_PEER_CONNECTION_IDENTITY_STORE_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/renderer/content_renderer_client.h"

namespace network_hints {
class PrescientNetworkingDispatcher;
}  // namespace network_hints

namespace chromecast {
namespace media {
class MediaCapsObserverImpl;
}

namespace shell {

void ExecuteJavaScript(content::RenderFrame* render_frame, int resourceId);

class CastContentRendererClient : public content::ContentRendererClient {
 public:
  // Creates an implementation of CastContentRendererClient. Platform should
  // link in an implementation as needed.
  static std::unique_ptr<CastContentRendererClient> Create();

  ~CastContentRendererClient() override;

  // ContentRendererClient implementation:
  void RenderThreadStarted() override;
  void RenderViewCreated(content::RenderView* render_view) override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<::media::KeySystemProperties>>*
          key_systems_properties) override;
  blink::WebPrescientNetworking* GetPrescientNetworking() override;
  void DeferMediaLoad(content::RenderFrame* render_frame,
                      bool render_frame_has_played_media_before,
                      const base::Closure& closure) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;

 protected:
  CastContentRendererClient();

  virtual void RunWhenInForeground(content::RenderFrame* render_frame,
                                   const base::Closure& closure);

 private:
  std::unique_ptr<network_hints::PrescientNetworkingDispatcher>
      prescient_networking_dispatcher_;
  std::unique_ptr<media::MediaCapsObserverImpl> media_caps_observer_;
  const bool allow_hidden_media_playback_;

  DISALLOW_COPY_AND_ASSIGN(CastContentRendererClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_

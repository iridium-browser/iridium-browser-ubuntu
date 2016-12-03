// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_player_renderer.h"

#include <memory>

#include "content/browser/media/android/media_resource_getter_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

// TODO(tguilbert): Remove this ID once MediaPlayerManager has been deleted
// and MediaPlayerBridge updated. See comment in header file.
constexpr int kUnusedAndIrrelevantPlayerId = 0;

namespace content {

MediaPlayerRenderer::MediaPlayerRenderer(RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host),
      has_error_(false),
      weak_factory_(this) {}

MediaPlayerRenderer::~MediaPlayerRenderer() {}

void MediaPlayerRenderer::Initialize(
    media::DemuxerStreamProvider* demuxer_stream_provider,
    media::RendererClient* client,
    const media::PipelineStatusCB& init_cb) {
  DVLOG(1) << __func__;
  if (demuxer_stream_provider->GetType() !=
      media::DemuxerStreamProvider::Type::URL) {
    DLOG(ERROR) << "DemuxerStreamProvider is not of Type URL";
    init_cb.Run(media::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  GURL url = demuxer_stream_provider->GetUrl();
  renderer_client_ = client;

  const std::string user_agent = GetContentClient()->GetUserAgent();

  // TODO(tguilbert): Get the first party cookies from WMPI. See
  // crbug.com/636604.
  media_player_.reset(new media::MediaPlayerBridge(
      kUnusedAndIrrelevantPlayerId, url,
      GURL(),  // first_party_for_cookies
      user_agent,
      false,  // hide_url_log
      this, base::Bind(&MediaPlayerRenderer::OnDecoderResourcesReleased,
                       weak_factory_.GetWeakPtr()),
      GURL(),  // frame_url
      false,   // allow_crendentials
      0));     // media_session_id

  // TODO(tguilbert): Register and Send the proper surface ID. See
  // crbug.com/627658

  media_player_->Initialize();
  init_cb.Run(media::PIPELINE_OK);
}

void MediaPlayerRenderer::SetCdm(media::CdmContext* cdm_context,
                                 const media::CdmAttachedCB& cdm_attached_cb) {
  NOTREACHED();
}

void MediaPlayerRenderer::Flush(const base::Closure& flush_cb) {
  DVLOG(3) << __func__;
  flush_cb.Run();
}

void MediaPlayerRenderer::StartPlayingFrom(base::TimeDelta time) {
  // MediaPlayerBridge's Start() is idempotent, except when it has encountered
  // an error (in which case, calling Start() again is logged as a new error).
  if (has_error_)
    return;

  media_player_->Start();
  media_player_->SeekTo(time);
}

void MediaPlayerRenderer::SetPlaybackRate(double playback_rate) {
  if (has_error_)
    return;

  if (playback_rate == 0) {
    media_player_->Pause(true);
  } else {
    // MediaPlayerBridge's Start() is idempotent.
    media_player_->Start();

    // TODO(tguilbert): MediaPlayer's interface allows variable playback rate,
    // but is not currently exposed in the MediaPlayerBridge interface.
    // Investigate wether or not we want to add variable playback speed.
  }
}

void MediaPlayerRenderer::SetVolume(float volume) {
  media_player_->SetVolume(volume);
}

base::TimeDelta MediaPlayerRenderer::GetMediaTime() {
  return media_player_->GetCurrentTime();
}

bool MediaPlayerRenderer::HasAudio() {
  return media_player_->HasAudio();
}

bool MediaPlayerRenderer::HasVideo() {
  return media_player_->HasVideo();
}

media::MediaResourceGetter* MediaPlayerRenderer::GetMediaResourceGetter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!media_resource_getter_.get()) {
    WebContents* web_contents =
        WebContents::FromRenderFrameHost(render_frame_host_);
    RenderProcessHost* host = web_contents->GetRenderProcessHost();
    BrowserContext* context = host->GetBrowserContext();
    StoragePartition* partition = host->GetStoragePartition();
    storage::FileSystemContext* file_system_context =
        partition ? partition->GetFileSystemContext() : nullptr;
    media_resource_getter_.reset(
        new MediaResourceGetterImpl(context, file_system_context, host->GetID(),
                                    render_frame_host_->GetRoutingID()));
  }
  return media_resource_getter_.get();
}

media::MediaUrlInterceptor* MediaPlayerRenderer::GetMediaUrlInterceptor() {
  // TODO(tguilbert): Offer a RegisterMediaUrlInterceptor equivalent for use in
  // webview. See crbug.com/636588.
  return nullptr;
}

void MediaPlayerRenderer::OnTimeUpdate(int player_id,
                                       base::TimeDelta current_timestamp,
                                       base::TimeTicks current_time_ticks) {}

void MediaPlayerRenderer::OnMediaMetadataChanged(int player_id,
                                                 base::TimeDelta duration,
                                                 int width,
                                                 int height,
                                                 bool success) {
  if (video_size_ != gfx::Size(width, height))
    OnVideoSizeChanged(kUnusedAndIrrelevantPlayerId, width, height);

  if (duration_ != duration) {
    duration_ = duration;
    renderer_client_->OnDurationChange(duration);
  }
}

void MediaPlayerRenderer::OnPlaybackComplete(int player_id) {
  renderer_client_->OnEnded();
}

void MediaPlayerRenderer::OnMediaInterrupted(int player_id) {}

void MediaPlayerRenderer::OnBufferingUpdate(int player_id, int percentage) {
  // As per Android documentation, |percentage| actually indicates "percentage
  // buffered or played". E.g. if we are at 50% playback and have 1%
  // buffered, |percentage| will be equal to 51.
  //
  // MediaPlayer manages its own buffering and will pause internally if ever it
  // runs out of data. Therefore, we can always return BUFFERING_HAVE_ENOUGH.
  renderer_client_->OnBufferingStateChange(media::BUFFERING_HAVE_ENOUGH);
}

void MediaPlayerRenderer::OnSeekComplete(int player_id,
                                         const base::TimeDelta& current_time) {}

void MediaPlayerRenderer::OnError(int player_id, int error) {
  // Some errors are forwarded to the MediaPlayerListener, but are of no
  // importance to us. Ignore these errors, which are reported as error 0 by
  // MediaPlayerListener.
  if (!error)
    return;

  LOG(ERROR) << __func__ << " Error: " << error;
  has_error_ = true;
  renderer_client_->OnError(media::PIPELINE_ERROR_EXTERNAL_RENDERER_FAILED);
}

void MediaPlayerRenderer::OnVideoSizeChanged(int player_id,
                                             int width,
                                             int height) {
  video_size_ = gfx::Size(width, height);
  renderer_client_->OnVideoNaturalSizeChange(video_size_);
}

void MediaPlayerRenderer::OnWaitingForDecryptionKey(int player_id) {
  NOTREACHED();
}

media::MediaPlayerAndroid* MediaPlayerRenderer::GetFullscreenPlayer() {
  NOTREACHED();
  return nullptr;
}

media::MediaPlayerAndroid* MediaPlayerRenderer::GetPlayer(int player_id) {
  NOTREACHED();
  return nullptr;
}

bool MediaPlayerRenderer::RequestPlay(int player_id,
                                      base::TimeDelta duration,
                                      bool has_audio) {
  // TODO(tguilbert): Throttle requests, via exponential backoff.
  // See crbug.com/636615.
  return true;
}

void MediaPlayerRenderer::OnDecoderResourcesReleased(int player_id) {
  // Since we are not using a pool of MediaPlayerAndroid instances, this
  // function is not relevant.

  // TODO(tguilbert): Throttle requests, via exponential backoff.
  // See crbug.com/636615.
}

}  // namespace content

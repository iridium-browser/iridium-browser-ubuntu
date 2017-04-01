// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/user_media_client_impl.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/hash.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/local_media_stream_audio_source.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_video_capturer_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/webrtc/processed_local_audio_source.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"
#include "content/renderer/media/webrtc_logging.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaDeviceInfo.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {
namespace {

void CopyFirstString(const blink::StringConstraint& constraint,
                     std::string* destination) {
  if (!constraint.exact().isEmpty())
    *destination = constraint.exact()[0].utf8();
}

bool FindDeviceId(const blink::WebVector<blink::WebString> candidates,
                  const MediaDeviceInfoArray& device_infos,
                  std::string* device_id) {
  for (const auto& candidate : candidates) {
    auto it = std::find_if(device_infos.begin(), device_infos.end(),
                           [&candidate](const MediaDeviceInfo& info) {
                             return info.device_id == candidate.utf8();
                           });

    if (it != device_infos.end()) {
      *device_id = it->device_id;
      return true;
    }
  }

  return false;
}

// If a device ID requested as exact basic constraint in |constraints| is found
// in |device_infos|, it is copied to |*device_id|, and the function returns
// false. If such a device is not found in |device_infos|, the function returns
// false and |*device_id| is left unmodified.
// If more than one device ID is requested as an exact basic constraint in
// |constraint|, the function returns false and |*device_id| is left unmodified.
// If no device ID is requested as an exact basic constraint, and at least one
// device ID requested as an ideal basic constraint or as an exact or ideal
// advanced constraint in |constraints| is found in |device_infos|, the first
// such device ID is copied to |*device_id| and the function returns true.
// If no such device ID is found, |*device_id| is left unmodified and the
// function returns true.
// TODO(guidou): Replace with a spec-compliant selection algorithm. See
// http://crbug.com/657733.
bool PickDeviceId(const blink::WebMediaConstraints& constraints,
                  const MediaDeviceInfoArray& device_infos,
                  std::string* device_id) {
  DCHECK(!constraints.isNull());
  DCHECK(device_id->empty());

  if (constraints.basic().deviceId.exact().size() > 1) {
    LOG(ERROR) << "Only one required device ID is supported";
    return false;
  }

  if (constraints.basic().deviceId.exact().size() == 1 &&
      !FindDeviceId(constraints.basic().deviceId.exact(), device_infos,
                    device_id)) {
    LOG(ERROR) << "Invalid mandatory device ID = "
               << constraints.basic().deviceId.exact()[0].utf8();
    return false;
  }

  // There is no required device ID. Look at the alternates.
  if (FindDeviceId(constraints.basic().deviceId.ideal(), device_infos,
                   device_id)) {
    return true;
  }

  for (const auto& advanced : constraints.advanced()) {
    if (FindDeviceId(advanced.deviceId.exact(), device_infos, device_id)) {
      return true;
    }
    if (FindDeviceId(advanced.deviceId.ideal(), device_infos, device_id)) {
      return true;
    }
  }

  // No valid alternate device ID found. Select default device.
  return true;
}

bool IsDeviceSource(const std::string& source) {
  return source.empty();
}

void CopyConstraintsToTrackControls(
    const blink::WebMediaConstraints& constraints,
    TrackControls* track_controls,
    bool* request_devices) {
  DCHECK(!constraints.isNull());
  track_controls->requested = true;
  CopyFirstString(constraints.basic().mediaStreamSource,
                  &track_controls->stream_source);
  if (IsDeviceSource(track_controls->stream_source)) {
    bool request_devices_advanced = false;
    for (const auto& advanced : constraints.advanced()) {
      if (!advanced.deviceId.isEmpty()) {
        request_devices_advanced = true;
        break;
      }
    }
    *request_devices =
        request_devices_advanced || !constraints.basic().deviceId.isEmpty();
  } else {
    CopyFirstString(constraints.basic().deviceId, &track_controls->device_id);
    *request_devices = false;
  }
}

void CopyHotwordAndLocalEchoToStreamControls(
    const blink::WebMediaConstraints& audio_constraints,
    StreamControls* controls) {
  if (audio_constraints.isNull())
    return;

  if (audio_constraints.basic().hotwordEnabled.hasExact()) {
    controls->hotword_enabled =
        audio_constraints.basic().hotwordEnabled.exact();
  } else {
    for (const auto& audio_advanced : audio_constraints.advanced()) {
      if (audio_advanced.hotwordEnabled.hasExact()) {
        controls->hotword_enabled = audio_advanced.hotwordEnabled.exact();
        break;
      }
    }
  }

  if (audio_constraints.basic().disableLocalEcho.hasExact()) {
    controls->disable_local_echo =
        audio_constraints.basic().disableLocalEcho.exact();
  } else {
    controls->disable_local_echo =
        controls->audio.stream_source != kMediaStreamSourceDesktop;
  }
}

bool IsSameDevice(const StreamDeviceInfo& device,
                  const StreamDeviceInfo& other_device) {
  return device.device.id == other_device.device.id &&
         device.device.type == other_device.device.type &&
         device.session_id == other_device.session_id;
}

bool IsSameSource(const blink::WebMediaStreamSource& source,
                  const blink::WebMediaStreamSource& other_source) {
  MediaStreamSource* const source_extra_data =
      static_cast<MediaStreamSource*>(source.getExtraData());
  const StreamDeviceInfo& device = source_extra_data->device_info();

  MediaStreamSource* const other_source_extra_data =
      static_cast<MediaStreamSource*>(other_source.getExtraData());
  const StreamDeviceInfo& other_device = other_source_extra_data->device_info();

  return IsSameDevice(device, other_device);
}

blink::WebMediaDeviceInfo::MediaDeviceKind ToMediaDeviceKind(
    MediaDeviceType type) {
  switch (type) {
    case MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      return blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput;
    case MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      return blink::WebMediaDeviceInfo::MediaDeviceKindVideoInput;
    case MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      return blink::WebMediaDeviceInfo::MediaDeviceKindAudioOutput;
    default:
      NOTREACHED();
      return blink::WebMediaDeviceInfo::MediaDeviceKindAudioInput;
  }
}

static int g_next_request_id = 0;

}  // namespace

UserMediaClientImpl::UserMediaClientImpl(
    RenderFrame* render_frame,
    PeerConnectionDependencyFactory* dependency_factory,
    std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher)
    : RenderFrameObserver(render_frame),
      dependency_factory_(dependency_factory),
      media_stream_dispatcher_(std::move(media_stream_dispatcher)),
      weak_factory_(this) {
  DCHECK(dependency_factory_);
  DCHECK(media_stream_dispatcher_.get());
}

UserMediaClientImpl::~UserMediaClientImpl() {
  // Force-close all outstanding user media requests and local sources here,
  // before the outstanding WeakPtrs are invalidated, to ensure a clean
  // shutdown.
  WillCommitProvisionalLoad();
}

void UserMediaClientImpl::requestUserMedia(
    const blink::WebUserMediaRequest& user_media_request) {
  // Save histogram data so we can see how much GetUserMedia is used.
  // The histogram counts the number of calls to the JS API
  // webGetUserMedia.
  UpdateWebRTCMethodCount(WEBKIT_GET_USER_MEDIA);
  DCHECK(CalledOnValidThread());
  DCHECK(!user_media_request.isNull());
  // ownerDocument may be null if we are in a test.
  // In that case, it's OK to not check frame().
  DCHECK(user_media_request.ownerDocument().isNull() ||
         render_frame()->GetWebFrame() ==
             static_cast<blink::WebFrame*>(
                 user_media_request.ownerDocument().frame()));

  if (RenderThreadImpl::current()) {
    RenderThreadImpl::current()->peer_connection_tracker()->TrackGetUserMedia(
        user_media_request);
  }

  int request_id = g_next_request_id++;
  std::unique_ptr<StreamControls> controls = base::MakeUnique<StreamControls>();

  bool enable_automatic_output_device_selection = false;
  bool request_audio_input_devices = false;
  if (user_media_request.audio()) {
    CopyConstraintsToTrackControls(user_media_request.audioConstraints(),
                                   &controls->audio,
                                   &request_audio_input_devices);
    CopyHotwordAndLocalEchoToStreamControls(
        user_media_request.audioConstraints(), controls.get());
    // Check if this input device should be used to select a matching output
    // device for audio rendering.
    GetConstraintValueAsBoolean(
        user_media_request.audioConstraints(),
        &blink::WebMediaTrackConstraintSet::renderToAssociatedSink,
        &enable_automatic_output_device_selection);
  }
  bool request_video_input_devices = false;
  if (user_media_request.video()) {
    CopyConstraintsToTrackControls(user_media_request.videoConstraints(),
                                   &controls->video,
                                   &request_video_input_devices);
  }

  url::Origin security_origin = user_media_request.getSecurityOrigin();
  if (request_audio_input_devices || request_video_input_devices) {
    GetMediaDevicesDispatcher()->EnumerateDevices(
        request_audio_input_devices, request_video_input_devices,
        false /* request_audio_output_devices */, security_origin,
        base::Bind(&UserMediaClientImpl::SelectUserMediaDevice,
                   weak_factory_.GetWeakPtr(), request_id, user_media_request,
                   base::Passed(&controls),
                   enable_automatic_output_device_selection, security_origin));
  } else {
    FinalizeRequestUserMedia(
        request_id, user_media_request, std::move(controls),
        enable_automatic_output_device_selection, security_origin);
  }
}

void UserMediaClientImpl::SelectUserMediaDevice(
    int request_id,
    const blink::WebUserMediaRequest& user_media_request,
    std::unique_ptr<StreamControls> controls,
    bool enable_automatic_output_device_selection,
    const url::Origin& security_origin,
    const EnumerationResult& device_enumeration) {
  DCHECK(CalledOnValidThread());

  if (controls->audio.requested &&
      IsDeviceSource(controls->audio.stream_source)) {
    if (!PickDeviceId(user_media_request.audioConstraints(),
                      device_enumeration[MEDIA_DEVICE_TYPE_AUDIO_INPUT],
                      &controls->audio.device_id)) {
      GetUserMediaRequestFailed(user_media_request, MEDIA_DEVICE_NO_HARDWARE,
                                "");
      return;
    }
  }

  if (controls->video.requested &&
      IsDeviceSource(controls->video.stream_source)) {
    if (!PickDeviceId(user_media_request.videoConstraints(),
                      device_enumeration[MEDIA_DEVICE_TYPE_VIDEO_INPUT],
                      &controls->video.device_id)) {
      GetUserMediaRequestFailed(user_media_request, MEDIA_DEVICE_NO_HARDWARE,
                                "");
      return;
    }
  }

  FinalizeRequestUserMedia(request_id, user_media_request, std::move(controls),
                           enable_automatic_output_device_selection,
                           security_origin);
}

void UserMediaClientImpl::FinalizeRequestUserMedia(
    int request_id,
    const blink::WebUserMediaRequest& user_media_request,
    std::unique_ptr<StreamControls> controls,
    bool enable_automatic_output_device_selection,
    const url::Origin& security_origin) {
  DCHECK(CalledOnValidThread());

  WebRtcLogMessage(
      base::StringPrintf("MSI::requestUserMedia. request_id=%d"
                         ", audio source id=%s"
                         ", video source id=%s",
                         request_id, controls->audio.device_id.c_str(),
                         controls->video.device_id.c_str()));

  user_media_requests_.push_back(std::unique_ptr<UserMediaRequestInfo>(
      new UserMediaRequestInfo(request_id, user_media_request,
                               enable_automatic_output_device_selection)));

  media_stream_dispatcher_->GenerateStream(
      request_id, weak_factory_.GetWeakPtr(), *controls, security_origin);
}

void UserMediaClientImpl::cancelUserMediaRequest(
    const blink::WebUserMediaRequest& user_media_request) {
  DCHECK(CalledOnValidThread());
  UserMediaRequestInfo* request = FindUserMediaRequestInfo(user_media_request);
  if (request) {
    // We can't abort the stream generation process.
    // Instead, erase the request. Once the stream is generated we will stop the
    // stream if the request does not exist.
    LogUserMediaRequestWithNoResult(MEDIA_STREAM_REQUEST_EXPLICITLY_CANCELLED);
    DeleteUserMediaRequestInfo(request);
  }
}

void UserMediaClientImpl::requestMediaDevices(
    const blink::WebMediaDevicesRequest& media_devices_request) {
  UpdateWebRTCMethodCount(WEBKIT_GET_MEDIA_DEVICES);
  DCHECK(CalledOnValidThread());

  // |media_devices_request| can't be mocked, so in tests it will be empty (the
  // underlying pointer is null). In order to use this function in a test we
  // need to check if it isNull.
  url::Origin security_origin;
  if (!media_devices_request.isNull())
    security_origin = media_devices_request.getSecurityOrigin();

  GetMediaDevicesDispatcher()->EnumerateDevices(
      true /* audio input */, true /* video input */, true /* audio output */,
      security_origin,
      base::Bind(&UserMediaClientImpl::FinalizeEnumerateDevices,
                 weak_factory_.GetWeakPtr(), media_devices_request));
}

void UserMediaClientImpl::setMediaDeviceChangeObserver(
    const blink::WebMediaDeviceChangeObserver& observer) {
  media_device_change_observer_ = observer;

  // Do nothing if setting a valid observer while already subscribed or setting
  // no observer while unsubscribed.
  if (media_device_change_observer_.isNull() ==
      device_change_subscription_ids_.empty())
    return;

  base::WeakPtr<MediaDevicesEventDispatcher> event_dispatcher =
      MediaDevicesEventDispatcher::GetForRenderFrame(render_frame());
  if (media_device_change_observer_.isNull()) {
    event_dispatcher->UnsubscribeDeviceChangeNotifications(
        device_change_subscription_ids_);
    device_change_subscription_ids_.clear();
  } else {
    DCHECK(device_change_subscription_ids_.empty());
    url::Origin security_origin =
        media_device_change_observer_.getSecurityOrigin();
    device_change_subscription_ids_ =
        event_dispatcher->SubscribeDeviceChangeNotifications(
            security_origin, base::Bind(&UserMediaClientImpl::DevicesChanged,
                                        weak_factory_.GetWeakPtr()));
  }
}

// Callback from MediaStreamDispatcher.
// The requested stream have been generated by the MediaStreamDispatcher.
void UserMediaClientImpl::OnStreamGenerated(
    int request_id,
    const std::string& label,
    const StreamDeviceInfoArray& audio_array,
    const StreamDeviceInfoArray& video_array) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnStreamGenerated stream:" << label;

  UserMediaRequestInfo* request_info = FindUserMediaRequestInfo(request_id);
  if (!request_info) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    DVLOG(1) << "Request ID not found";
    OnStreamGeneratedForCancelledRequest(audio_array, video_array);
    return;
  }
  request_info->generated = true;

  for (const auto* array : {&audio_array, &video_array}) {
    for (const auto& info : *array) {
      WebRtcLogMessage(base::StringPrintf("Request %d for device \"%s\"",
                                          request_id,
                                          info.device.name.c_str()));
    }
  }

  DCHECK(!request_info->request.isNull());
  blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
      audio_array.size());
  CreateAudioTracks(audio_array, request_info->request.audioConstraints(),
                    &audio_track_vector, request_info);

  blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
      video_array.size());
  CreateVideoTracks(video_array, request_info->request.videoConstraints(),
                    &video_track_vector, request_info);

  blink::WebString webkit_id = blink::WebString::fromUTF8(label);
  blink::WebMediaStream* web_stream = &(request_info->web_stream);

  web_stream->initialize(webkit_id, audio_track_vector, video_track_vector);
  web_stream->setExtraData(new MediaStream());

  // Wait for the tracks to be started successfully or to fail.
  request_info->CallbackOnTracksStarted(
      base::Bind(&UserMediaClientImpl::OnCreateNativeTracksCompleted,
                 weak_factory_.GetWeakPtr()));
}

void UserMediaClientImpl::OnStreamGeneratedForCancelledRequest(
    const StreamDeviceInfoArray& audio_array,
    const StreamDeviceInfoArray& video_array) {
  // Only stop the device if the device is not used in another MediaStream.
  for (StreamDeviceInfoArray::const_iterator device_it = audio_array.begin();
       device_it != audio_array.end(); ++device_it) {
    if (!FindLocalSource(*device_it))
      media_stream_dispatcher_->StopStreamDevice(*device_it);
  }

  for (StreamDeviceInfoArray::const_iterator device_it = video_array.begin();
       device_it != video_array.end(); ++device_it) {
    if (!FindLocalSource(*device_it))
      media_stream_dispatcher_->StopStreamDevice(*device_it);
  }
}

// static
void UserMediaClientImpl::OnAudioSourceStartedOnAudioThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<UserMediaClientImpl> weak_ptr,
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  task_runner->PostTask(FROM_HERE,
                        base::Bind(&UserMediaClientImpl::OnAudioSourceStarted,
                                   weak_ptr, source, result, result_name));
}

void UserMediaClientImpl::OnAudioSourceStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DCHECK(CalledOnValidThread());

  for (auto it = pending_local_sources_.begin();
       it != pending_local_sources_.end(); ++it) {
    MediaStreamSource* const source_extra_data =
        static_cast<MediaStreamSource*>((*it).getExtraData());
    if (source_extra_data != source)
      continue;
    if (result == MEDIA_DEVICE_OK)
      local_sources_.push_back((*it));
    pending_local_sources_.erase(it);

    NotifyAllRequestsOfAudioSourceStarted(source, result, result_name);
    return;
  }
  NOTREACHED();
}

void UserMediaClientImpl::NotifyAllRequestsOfAudioSourceStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  // Since a request object that receives the OnAudioSourceStarted event,
  // might get deleted and removed from the |user_media_requests_| array while
  // we iterate through it, we need to jump through this hoop here, copy
  // pointers to the objects we're notifying and avoid using
  // user_media_requests_ as we iterate+notify.
  std::vector<UserMediaRequestInfo*> requests;
  requests.reserve(user_media_requests_.size());
  for (const auto& request : user_media_requests_)
    requests.push_back(request.get());
  for (auto* request : requests)
    request->OnAudioSourceStarted(source, result, result_name);
}

void UserMediaClientImpl::FinalizeEnumerateDevices(
    blink::WebMediaDevicesRequest request,
    const EnumerationResult& result) {
  DCHECK_EQ(static_cast<size_t>(NUM_MEDIA_DEVICE_TYPES), result.size());

  blink::WebVector<blink::WebMediaDeviceInfo> devices(
      result[MEDIA_DEVICE_TYPE_AUDIO_INPUT].size() +
      result[MEDIA_DEVICE_TYPE_VIDEO_INPUT].size() +
      result[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT].size());
  size_t index = 0;
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    blink::WebMediaDeviceInfo::MediaDeviceKind device_kind =
        ToMediaDeviceKind(static_cast<MediaDeviceType>(i));
    for (const auto& device_info : result[i]) {
      devices[index++].initialize(
          blink::WebString::fromUTF8(device_info.device_id), device_kind,
          blink::WebString::fromUTF8(device_info.label),
          blink::WebString::fromUTF8(device_info.group_id));
    }
  }

  EnumerateDevicesSucceded(&request, devices);
}

// Callback from MediaStreamDispatcher.
// The requested stream failed to be generated.
void UserMediaClientImpl::OnStreamGenerationFailed(
    int request_id,
    MediaStreamRequestResult result) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnStreamGenerationFailed("
           << request_id << ")";
  UserMediaRequestInfo* request_info = FindUserMediaRequestInfo(request_id);
  if (!request_info) {
    // This can happen if the request is canceled or the frame reloads while
    // MediaStreamDispatcher is processing the request.
    DVLOG(1) << "Request ID not found";
    return;
  }

  GetUserMediaRequestFailed(request_info->request, result, "");
  DeleteUserMediaRequestInfo(request_info);
}

// Callback from MediaStreamDispatcher.
// The browser process has stopped a device used by a MediaStream.
void UserMediaClientImpl::OnDeviceStopped(
    const std::string& label,
    const StreamDeviceInfo& device_info) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnDeviceStopped("
           << "{device_id = " << device_info.device.id << "})";

  const blink::WebMediaStreamSource* source_ptr = FindLocalSource(device_info);
  if (!source_ptr) {
    // This happens if the same device is used in several guM requests or
    // if a user happen stop a track from JS at the same time
    // as the underlying media device is unplugged from the system.
    return;
  }
  // By creating |source| it is guaranteed that the blink::WebMediaStreamSource
  // object is valid during the cleanup.
  blink::WebMediaStreamSource source(*source_ptr);
  StopLocalSource(source, false);
  RemoveLocalSource(source);
}

blink::WebMediaStreamSource UserMediaClientImpl::InitializeVideoSourceObject(
    const StreamDeviceInfo& device,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(CalledOnValidThread());

  blink::WebMediaStreamSource source = FindOrInitializeSourceObject(device);
  if (!source.getExtraData()) {
    source.setExtraData(CreateVideoSource(
        device, base::Bind(&UserMediaClientImpl::OnLocalSourceStopped,
                           weak_factory_.GetWeakPtr())));
    local_sources_.push_back(source);
  }
  return source;
}

blink::WebMediaStreamSource UserMediaClientImpl::InitializeAudioSourceObject(
    const StreamDeviceInfo& device,
    const blink::WebMediaConstraints& constraints,
    bool* is_pending) {
  DCHECK(CalledOnValidThread());

  *is_pending = true;

  // See if the source is already being initialized.
  auto* pending = FindPendingLocalSource(device);
  if (pending)
    return *pending;

  blink::WebMediaStreamSource source = FindOrInitializeSourceObject(device);
  if (source.getExtraData()) {
    // The only return point for non-pending sources.
    *is_pending = false;
    return source;
  }

  // While sources are being initialized, keep them in a separate array.
  // Once they've finished initialized, they'll be moved over to local_sources_.
  // See OnAudioSourceStarted for more details.
  pending_local_sources_.push_back(source);

  MediaStreamSource::ConstraintsCallback source_ready = base::Bind(
      &UserMediaClientImpl::OnAudioSourceStartedOnAudioThread,
      base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr());

  MediaStreamAudioSource* const audio_source =
      CreateAudioSource(device, constraints, source_ready);
  audio_source->SetStopCallback(base::Bind(
      &UserMediaClientImpl::OnLocalSourceStopped, weak_factory_.GetWeakPtr()));
  source.setExtraData(audio_source);  // Takes ownership.
  return source;
}

MediaStreamAudioSource* UserMediaClientImpl::CreateAudioSource(
    const StreamDeviceInfo& device,
    const blink::WebMediaConstraints& constraints,
    const MediaStreamSource::ConstraintsCallback& source_ready) {
  DCHECK(CalledOnValidThread());
  // If the audio device is a loopback device (for screen capture), or if the
  // constraints/effects parameters indicate no audio processing is needed,
  // create an efficient, direct-path MediaStreamAudioSource instance.
  if (IsScreenCaptureMediaType(device.device.type) ||
      !MediaStreamAudioProcessor::WouldModifyAudio(
          constraints, device.device.input.effects)) {
    return new LocalMediaStreamAudioSource(RenderFrameObserver::routing_id(),
                                           device, source_ready);
  }

  // The audio device is not associated with screen capture and also requires
  // processing.
  ProcessedLocalAudioSource* source = new ProcessedLocalAudioSource(
      RenderFrameObserver::routing_id(), device, constraints, source_ready,
      dependency_factory_);
  return source;
}

MediaStreamVideoSource* UserMediaClientImpl::CreateVideoSource(
    const StreamDeviceInfo& device,
    const MediaStreamSource::SourceStoppedCallback& stop_callback) {
  DCHECK(CalledOnValidThread());
  content::MediaStreamVideoCapturerSource* ret =
      new content::MediaStreamVideoCapturerSource(stop_callback, device,
                                                  render_frame());
  return ret;
}

void UserMediaClientImpl::CreateVideoTracks(
    const StreamDeviceInfoArray& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
    UserMediaRequestInfo* request) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  for (size_t i = 0; i < devices.size(); ++i) {
    blink::WebMediaStreamSource source =
        InitializeVideoSourceObject(devices[i], constraints);
    (*webkit_tracks)[i] =
        request->CreateAndStartVideoTrack(source, constraints);
  }
}

void UserMediaClientImpl::CreateAudioTracks(
    const StreamDeviceInfoArray& devices,
    const blink::WebMediaConstraints& constraints,
    blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
    UserMediaRequestInfo* request) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(devices.size(), webkit_tracks->size());

  StreamDeviceInfoArray overridden_audio_array = devices;
  if (!request->enable_automatic_output_device_selection) {
    // If the GetUserMedia request did not explicitly set the constraint
    // kMediaStreamRenderToAssociatedSink, the output device parameters must
    // be removed.
    for (auto& device_info : overridden_audio_array) {
      device_info.device.matched_output_device_id = "";
      device_info.device.matched_output =
          MediaStreamDevice::AudioDeviceParameters();
    }
  }

  for (size_t i = 0; i < overridden_audio_array.size(); ++i) {
    bool is_pending = false;
    blink::WebMediaStreamSource source = InitializeAudioSourceObject(
        overridden_audio_array[i], constraints, &is_pending);
    (*webkit_tracks)[i].initialize(source);
    request->StartAudioTrack((*webkit_tracks)[i], is_pending);
  }
}

void UserMediaClientImpl::OnCreateNativeTracksCompleted(
    UserMediaRequestInfo* request,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnCreateNativeTracksComplete("
           << "{request_id = " << request->request_id << "} "
           << "{result = " << result << "})";

  if (result == content::MEDIA_DEVICE_OK) {
    GetUserMediaRequestSucceeded(request->web_stream, request->request);
  } else {
    GetUserMediaRequestFailed(request->request, result, result_name);

    blink::WebVector<blink::WebMediaStreamTrack> tracks;
    request->web_stream.audioTracks(tracks);
    for (auto& web_track : tracks) {
      MediaStreamTrack* track = MediaStreamTrack::GetTrack(web_track);
      if (track)
        track->Stop();
    }
    request->web_stream.videoTracks(tracks);
    for (auto& web_track : tracks) {
      MediaStreamTrack* track = MediaStreamTrack::GetTrack(web_track);
      if (track)
        track->Stop();
    }
  }

  DeleteUserMediaRequestInfo(request);
}

void UserMediaClientImpl::OnDeviceOpened(
    int request_id,
    const std::string& label,
    const StreamDeviceInfo& video_device) {
  DVLOG(1) << "UserMediaClientImpl::OnDeviceOpened("
           << request_id << ", " << label << ")";
  NOTIMPLEMENTED();
}

void UserMediaClientImpl::OnDeviceOpenFailed(int request_id) {
  DVLOG(1) << "UserMediaClientImpl::VideoDeviceOpenFailed("
           << request_id << ")";
  NOTIMPLEMENTED();
}

void UserMediaClientImpl::DevicesChanged(
    MediaDeviceType type,
    const MediaDeviceInfoArray& device_infos) {
  if (!media_device_change_observer_.isNull())
    media_device_change_observer_.didChangeMediaDevices();
}

void UserMediaClientImpl::GetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest request_info) {
  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClientImpl is destroyed if the JavaScript code request the
  // frame to be destroyed within the scope of the callback. Therefore,
  // post a task to complete the request with a clean stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded,
                 weak_factory_.GetWeakPtr(), stream, request_info));
}

void UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded(
    const blink::WebMediaStream& stream,
    blink::WebUserMediaRequest request_info) {
  DVLOG(1) << "UserMediaClientImpl::DelayedGetUserMediaRequestSucceeded";
  LogUserMediaRequestResult(MEDIA_DEVICE_OK);
  request_info.requestSucceeded(stream);
}

void UserMediaClientImpl::GetUserMediaRequestFailed(
    blink::WebUserMediaRequest request_info,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  // Completing the getUserMedia request can lead to that the RenderFrame and
  // the UserMediaClientImpl is destroyed if the JavaScript code request the
  // frame to be destroyed within the scope of the callback. Therefore,
  // post a task to complete the request with a clean stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&UserMediaClientImpl::DelayedGetUserMediaRequestFailed,
                 weak_factory_.GetWeakPtr(), request_info, result,
                 result_name));
}

void UserMediaClientImpl::DelayedGetUserMediaRequestFailed(
    blink::WebUserMediaRequest request_info,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  LogUserMediaRequestResult(result);
  switch (result) {
    case MEDIA_DEVICE_OK:
    case NUM_MEDIA_REQUEST_RESULTS:
      NOTREACHED();
      return;
    case MEDIA_DEVICE_PERMISSION_DENIED:
      request_info.requestDenied();
      return;
    case MEDIA_DEVICE_PERMISSION_DISMISSED:
      request_info.requestFailedUASpecific("PermissionDismissedError");
      return;
    case MEDIA_DEVICE_INVALID_STATE:
      request_info.requestFailedUASpecific("InvalidStateError");
      return;
    case MEDIA_DEVICE_NO_HARDWARE:
      request_info.requestFailedUASpecific("DevicesNotFoundError");
      return;
    case MEDIA_DEVICE_INVALID_SECURITY_ORIGIN:
      request_info.requestFailedUASpecific("InvalidSecurityOriginError");
      return;
    case MEDIA_DEVICE_TAB_CAPTURE_FAILURE:
      request_info.requestFailedUASpecific("TabCaptureError");
      return;
    case MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE:
      request_info.requestFailedUASpecific("ScreenCaptureError");
      return;
    case MEDIA_DEVICE_CAPTURE_FAILURE:
      request_info.requestFailedUASpecific("DeviceCaptureError");
      return;
    case MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED:
      request_info.requestFailedConstraint(result_name);
      return;
    case MEDIA_DEVICE_TRACK_START_FAILURE:
      request_info.requestFailedUASpecific("TrackStartError");
      return;
    case MEDIA_DEVICE_NOT_SUPPORTED:
      request_info.requestFailedUASpecific("MediaDeviceNotSupported");
      return;
    case MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN:
      request_info.requestFailedUASpecific("MediaDeviceFailedDueToShutdown");
      return;
    case MEDIA_DEVICE_KILL_SWITCH_ON:
      request_info.requestFailedUASpecific("MediaDeviceKillSwitchOn");
      return;
  }
  NOTREACHED();
  request_info.requestFailed();
}

void UserMediaClientImpl::EnumerateDevicesSucceded(
    blink::WebMediaDevicesRequest* request,
    blink::WebVector<blink::WebMediaDeviceInfo>& devices) {
  request->requestSucceeded(devices);
}

const blink::WebMediaStreamSource* UserMediaClientImpl::FindLocalSource(
    const LocalStreamSources& sources,
    const StreamDeviceInfo& device) const {
  for (const auto& local_source : sources) {
    MediaStreamSource* const source =
        static_cast<MediaStreamSource*>(local_source.getExtraData());
    const StreamDeviceInfo& active_device = source->device_info();
    if (IsSameDevice(active_device, device))
      return &local_source;
  }
  return nullptr;
}

blink::WebMediaStreamSource UserMediaClientImpl::FindOrInitializeSourceObject(
    const StreamDeviceInfo& device) {
  const blink::WebMediaStreamSource* existing_source = FindLocalSource(device);
  if (existing_source) {
    DVLOG(1) << "Source already exists. Reusing source with id "
             << existing_source->id().utf8();
    return *existing_source;
  }

  blink::WebMediaStreamSource::Type type =
      IsAudioInputMediaType(device.device.type)
          ? blink::WebMediaStreamSource::TypeAudio
          : blink::WebMediaStreamSource::TypeVideo;

  blink::WebMediaStreamSource source;
  source.initialize(blink::WebString::fromUTF8(device.device.id), type,
                    blink::WebString::fromUTF8(device.device.name),
                    false /* remote */);

  DVLOG(1) << "Initialize source object :"
           << "id = " << source.id().utf8()
           << ", name = " << source.name().utf8();
  return source;
}

bool UserMediaClientImpl::RemoveLocalSource(
    const blink::WebMediaStreamSource& source) {
  DCHECK(CalledOnValidThread());

  for (LocalStreamSources::iterator device_it = local_sources_.begin();
       device_it != local_sources_.end(); ++device_it) {
    if (IsSameSource(*device_it, source)) {
      local_sources_.erase(device_it);
      return true;
    }
  }

  // Check if the source was pending.
  for (LocalStreamSources::iterator device_it = pending_local_sources_.begin();
       device_it != pending_local_sources_.end(); ++device_it) {
    if (IsSameSource(*device_it, source)) {
      MediaStreamSource* const source_extra_data =
          static_cast<MediaStreamSource*>(source.getExtraData());
      NotifyAllRequestsOfAudioSourceStarted(
          source_extra_data, MEDIA_DEVICE_TRACK_START_FAILURE,
          "Failed to access audio capture device");
      pending_local_sources_.erase(device_it);
      return true;
    }
  }

  return false;
}

UserMediaClientImpl::UserMediaRequestInfo*
UserMediaClientImpl::FindUserMediaRequestInfo(int request_id) {
  DCHECK(CalledOnValidThread());
  for (auto& r : user_media_requests_) {
    if (r->request_id == request_id)
      return r.get();
  }
  return nullptr;
}

UserMediaClientImpl::UserMediaRequestInfo*
UserMediaClientImpl::FindUserMediaRequestInfo(
    const blink::WebUserMediaRequest& request) {
  DCHECK(CalledOnValidThread());
  for (auto& r : user_media_requests_) {
    if (r->request == request)
      return r.get();
  }
  return nullptr;
}

void UserMediaClientImpl::DeleteUserMediaRequestInfo(
    UserMediaRequestInfo* request) {
  DCHECK(CalledOnValidThread());
  auto new_end =
      std::remove_if(user_media_requests_.begin(), user_media_requests_.end(),
                     [&](std::unique_ptr<UserMediaRequestInfo>& r) {
                       return r.get() == request;
                     });
  DCHECK(new_end != user_media_requests_.end());
  user_media_requests_.erase(new_end, user_media_requests_.end());
}

void UserMediaClientImpl::DeleteAllUserMediaRequests() {
  UserMediaRequests::iterator request_it = user_media_requests_.begin();
  while (request_it != user_media_requests_.end()) {
    DVLOG(1) << "UserMediaClientImpl@" << this
             << "::DeleteAllUserMediaRequests: "
             << "Cancel user media request " << (*request_it)->request_id;
    // If the request is not generated, it means that a request
    // has been sent to the MediaStreamDispatcher to generate a stream
    // but MediaStreamDispatcher has not yet responded and we need to cancel
    // the request.
    if (!(*request_it)->generated) {
      DCHECK(!(*request_it)->HasPendingSources());
      media_stream_dispatcher_->CancelGenerateStream(
          (*request_it)->request_id, weak_factory_.GetWeakPtr());
      LogUserMediaRequestWithNoResult(MEDIA_STREAM_REQUEST_NOT_GENERATED);
    } else {
      DCHECK((*request_it)->HasPendingSources());
      LogUserMediaRequestWithNoResult(
          MEDIA_STREAM_REQUEST_PENDING_MEDIA_TRACKS);
    }
    request_it = user_media_requests_.erase(request_it);
  }
}

void UserMediaClientImpl::WillCommitProvisionalLoad() {
  // Cancel all outstanding UserMediaRequests.
  DeleteAllUserMediaRequests();

  // Loop through all current local sources and stop the sources.
  LocalStreamSources::iterator sources_it = local_sources_.begin();
  while (sources_it != local_sources_.end()) {
    StopLocalSource(*sources_it, true);
    sources_it = local_sources_.erase(sources_it);
  }
}

void UserMediaClientImpl::SetMediaDevicesDispatcherForTesting(
    ::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher) {
  media_devices_dispatcher_ = std::move(media_devices_dispatcher);
}

void UserMediaClientImpl::OnLocalSourceStopped(
    const blink::WebMediaStreamSource& source) {
  DCHECK(CalledOnValidThread());
  DVLOG(1) << "UserMediaClientImpl::OnLocalSourceStopped";

  const bool some_source_removed = RemoveLocalSource(source);
  CHECK(some_source_removed);

  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*>(source.getExtraData());
  media_stream_dispatcher_->StopStreamDevice(source_impl->device_info());
}

void UserMediaClientImpl::StopLocalSource(
    const blink::WebMediaStreamSource& source,
    bool notify_dispatcher) {
  MediaStreamSource* source_impl =
      static_cast<MediaStreamSource*>(source.getExtraData());
  DVLOG(1) << "UserMediaClientImpl::StopLocalSource("
           << "{device_id = " << source_impl->device_info().device.id << "})";

  if (notify_dispatcher)
    media_stream_dispatcher_->StopStreamDevice(source_impl->device_info());

  source_impl->ResetSourceStoppedCallback();
  source_impl->StopSource();
}

const ::mojom::MediaDevicesDispatcherHostPtr&
UserMediaClientImpl::GetMediaDevicesDispatcher() {
  if (!media_devices_dispatcher_) {
    render_frame()->GetRemoteInterfaces()->GetInterface(
        mojo::MakeRequest(&media_devices_dispatcher_));
  }

  return media_devices_dispatcher_;
}

UserMediaClientImpl::UserMediaRequestInfo::UserMediaRequestInfo(
    int request_id,
    const blink::WebUserMediaRequest& request,
    bool enable_automatic_output_device_selection)
    : request_id(request_id),
      generated(false),
      enable_automatic_output_device_selection(
          enable_automatic_output_device_selection),
      request(request),
      request_result_(MEDIA_DEVICE_OK),
      request_result_name_("") {
}

UserMediaClientImpl::UserMediaRequestInfo::~UserMediaRequestInfo() {
  DVLOG(1) << "~UserMediaRequestInfo";
}

void UserMediaClientImpl::UserMediaRequestInfo::StartAudioTrack(
    const blink::WebMediaStreamTrack& track,
    bool is_pending) {
  DCHECK(track.source().getType() == blink::WebMediaStreamSource::TypeAudio);
  MediaStreamAudioSource* native_source =
      MediaStreamAudioSource::From(track.source());
  // Add the source as pending since OnTrackStarted will expect it to be there.
  sources_waiting_for_callback_.push_back(native_source);

  sources_.push_back(track.source());
  bool connected = native_source->ConnectToTrack(track);
  if (!is_pending) {
    OnTrackStarted(
        native_source,
        connected ? MEDIA_DEVICE_OK : MEDIA_DEVICE_TRACK_START_FAILURE, "");
#if defined(OS_ANDROID)
  } else if (connected) {
    CHECK(native_source->is_local_source());
    // On Android, we won't get the callback indicating the device readyness.
    // TODO(tommi): Update the android implementation to support the
    // OnAudioSourceStarted notification.  http://crbug.com/679302
    OnTrackStarted(native_source, MEDIA_DEVICE_OK, "");
#endif
  }
}

blink::WebMediaStreamTrack
UserMediaClientImpl::UserMediaRequestInfo::CreateAndStartVideoTrack(
    const blink::WebMediaStreamSource& source,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(source.getType() == blink::WebMediaStreamSource::TypeVideo);
  MediaStreamVideoSource* native_source =
      MediaStreamVideoSource::GetVideoSource(source);
  DCHECK(native_source);
  sources_.push_back(source);
  sources_waiting_for_callback_.push_back(native_source);
  return MediaStreamVideoTrack::CreateVideoTrack(
      native_source, constraints, base::Bind(
          &UserMediaClientImpl::UserMediaRequestInfo::OnTrackStarted,
          AsWeakPtr()),
      true);
}

void UserMediaClientImpl::UserMediaRequestInfo::CallbackOnTracksStarted(
    const ResourcesReady& callback) {
  DCHECK(ready_callback_.is_null());
  ready_callback_ = callback;
  CheckAllTracksStarted();
}

void UserMediaClientImpl::UserMediaRequestInfo::OnTrackStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  DVLOG(1) << "OnTrackStarted result " << result;
  std::vector<MediaStreamSource*>::iterator it =
      std::find(sources_waiting_for_callback_.begin(),
                sources_waiting_for_callback_.end(),
                source);
  DCHECK(it != sources_waiting_for_callback_.end());
  sources_waiting_for_callback_.erase(it);
  // All tracks must be started successfully. Otherwise the request is a
  // failure.
  if (result != MEDIA_DEVICE_OK) {
    request_result_ = result;
    request_result_name_ = result_name;
  }

  CheckAllTracksStarted();
}

void UserMediaClientImpl::UserMediaRequestInfo::CheckAllTracksStarted() {
  if (!ready_callback_.is_null() && sources_waiting_for_callback_.empty()) {
    ready_callback_.Run(this, request_result_, request_result_name_);
    // NOTE: |this| might now be deleted.
  }
}

bool UserMediaClientImpl::UserMediaRequestInfo::HasPendingSources() const {
  return !sources_waiting_for_callback_.empty();
}

void UserMediaClientImpl::UserMediaRequestInfo::OnAudioSourceStarted(
    MediaStreamSource* source,
    MediaStreamRequestResult result,
    const blink::WebString& result_name) {
  // Check if we're waiting to be notified of this source.  If not, then we'll
  // ignore the notification.
  auto found = std::find(sources_waiting_for_callback_.begin(),
                         sources_waiting_for_callback_.end(), source);
  if (found != sources_waiting_for_callback_.end())
    OnTrackStarted(source, result, result_name);
}

void UserMediaClientImpl::OnDestruct() {
  delete this;
}

}  // namespace content

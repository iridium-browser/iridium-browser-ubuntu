// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_
#define CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.h"
#include "content/common/media/media_devices.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/media/media_devices_event_dispatcher.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "content/renderer/media/media_stream_source.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebMediaDeviceChangeObserver.h"
#include "third_party/WebKit/public/web/WebMediaDevicesRequest.h"
#include "third_party/WebKit/public/web/WebUserMediaClient.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"

namespace base {
class TaskRunner;
}

namespace content {
class PeerConnectionDependencyFactory;
class MediaStreamAudioSource;
class MediaStreamDispatcher;
class MediaStreamVideoSource;
struct VideoDeviceCaptureSourceSelectionResult;

// UserMediaClientImpl is a delegate for the Media Stream GetUserMedia API.
// It ties together WebKit and MediaStreamManager
// (via MediaStreamDispatcher and MediaStreamDispatcherHost)
// in the browser process. It must be created, called and destroyed on the
// render thread.
class CONTENT_EXPORT UserMediaClientImpl
    : public RenderFrameObserver,
      NON_EXPORTED_BASE(public blink::WebUserMediaClient),
      public MediaStreamDispatcherEventHandler,
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // |render_frame| and |dependency_factory| must outlive this instance.
  UserMediaClientImpl(
      RenderFrame* render_frame,
      PeerConnectionDependencyFactory* dependency_factory,
      std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher,
      const scoped_refptr<base::TaskRunner>& worker_task_runner);
  ~UserMediaClientImpl() override;

  MediaStreamDispatcher* media_stream_dispatcher() const {
    return media_stream_dispatcher_.get();
  }

  // blink::WebUserMediaClient implementation
  void requestUserMedia(
      const blink::WebUserMediaRequest& user_media_request) override;
  void cancelUserMediaRequest(
      const blink::WebUserMediaRequest& user_media_request) override;
  void requestMediaDevices(
      const blink::WebMediaDevicesRequest& media_devices_request) override;
  void setMediaDeviceChangeObserver(
      const blink::WebMediaDeviceChangeObserver& observer) override;

  // MediaStreamDispatcherEventHandler implementation.
  void OnStreamGenerated(int request_id,
                         const std::string& label,
                         const StreamDeviceInfoArray& audio_array,
                         const StreamDeviceInfoArray& video_array) override;
  void OnStreamGenerationFailed(int request_id,
                                MediaStreamRequestResult result) override;
  void OnDeviceStopped(const std::string& label,
                       const StreamDeviceInfo& device_info) override;
  void OnDeviceOpened(int request_id,
                      const std::string& label,
                      const StreamDeviceInfo& device_info) override;
  void OnDeviceOpenFailed(int request_id) override;

  // RenderFrameObserver override
  void WillCommitProvisionalLoad() override;

  void SetMediaDevicesDispatcherForTesting(
      ::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher);

 protected:
  // Called when |source| has been stopped from JavaScript.
  void OnLocalSourceStopped(const blink::WebMediaStreamSource& source);

  // These methods are virtual for test purposes. A test can override them to
  // test requesting local media streams. The function notifies WebKit that the
  // |request| have completed.
  virtual void GetUserMediaRequestSucceeded(
       const blink::WebMediaStream& stream,
       blink::WebUserMediaRequest request_info);
  void DelayedGetUserMediaRequestSucceeded(
      const blink::WebMediaStream& stream,
      blink::WebUserMediaRequest request_info);
  virtual void GetUserMediaRequestFailed(
      blink::WebUserMediaRequest request_info,
      MediaStreamRequestResult result,
      const blink::WebString& result_name);
  void DelayedGetUserMediaRequestFailed(
      blink::WebUserMediaRequest request_info,
      MediaStreamRequestResult result,
      const blink::WebString& result_name);

  virtual void EnumerateDevicesSucceded(
      blink::WebMediaDevicesRequest* request,
      blink::WebVector<blink::WebMediaDeviceInfo>& devices);

  // Creates a MediaStreamAudioSource/MediaStreamVideoSource objects.
  // These are virtual for test purposes.
  virtual MediaStreamAudioSource* CreateAudioSource(
      const StreamDeviceInfo& device,
      const blink::WebMediaConstraints& constraints,
      const MediaStreamSource::ConstraintsCallback& source_ready);
  virtual MediaStreamVideoSource* CreateVideoSource(
      const StreamDeviceInfo& device,
      const MediaStreamSource::SourceStoppedCallback& stop_callback);

  // Class for storing information about a WebKit request to create a
  // MediaStream.
  class UserMediaRequestInfo
      : public base::SupportsWeakPtr<UserMediaRequestInfo> {
   public:
    typedef base::Callback<void(UserMediaRequestInfo* request_info,
                                MediaStreamRequestResult result,
                                const blink::WebString& result_name)>
      ResourcesReady;

    UserMediaRequestInfo(int request_id,
                         const blink::WebUserMediaRequest& request,
                         bool enable_automatic_output_device_selection);
    ~UserMediaRequestInfo();
    int request_id;
    // True if MediaStreamDispatcher has generated the stream, see
    // OnStreamGenerated.
    bool generated;
    const bool enable_automatic_output_device_selection;
    blink::WebMediaStream web_stream;
    blink::WebUserMediaRequest request;

    void StartAudioTrack(const blink::WebMediaStreamTrack& track,
                         bool is_pending);

    blink::WebMediaStreamTrack CreateAndStartVideoTrack(
        const blink::WebMediaStreamSource& source,
        const blink::WebMediaConstraints& constraints);

    // Triggers |callback| when all sources used in this request have either
    // successfully started, or a source has failed to start.
    void CallbackOnTracksStarted(const ResourcesReady& callback);

    bool HasPendingSources() const;

    // Called when a local audio source has finished (or failed) initializing.
    void OnAudioSourceStarted(MediaStreamSource* source,
                              MediaStreamRequestResult result,
                              const blink::WebString& result_name);

   private:
    void OnTrackStarted(
        MediaStreamSource* source,
        MediaStreamRequestResult result,
        const blink::WebString& result_name);

    // Cheks if the sources for all tracks have been started and if so,
    // invoke the |ready_callback_|.  Note that the caller should expect
    // that |this| might be deleted when the function returns.
    void CheckAllTracksStarted();

    ResourcesReady ready_callback_;
    MediaStreamRequestResult request_result_;
    blink::WebString request_result_name_;
    // Sources used in this request.
    std::vector<blink::WebMediaStreamSource> sources_;
    std::vector<MediaStreamSource*> sources_waiting_for_callback_;
  };
  typedef std::vector<std::unique_ptr<UserMediaRequestInfo>> UserMediaRequests;

 protected:
  // These methods can be accessed in unit tests.
  UserMediaRequestInfo* FindUserMediaRequestInfo(int request_id);
  UserMediaRequestInfo* FindUserMediaRequestInfo(
      const blink::WebUserMediaRequest& request);

  void DeleteUserMediaRequestInfo(UserMediaRequestInfo* request);

 private:
  typedef std::vector<blink::WebMediaStreamSource> LocalStreamSources;

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Creates a WebKit representation of stream sources based on
  // |devices| from the MediaStreamDispatcher.
  blink::WebMediaStreamSource InitializeVideoSourceObject(
      const StreamDeviceInfo& device,
      const blink::WebMediaConstraints& constraints);

  blink::WebMediaStreamSource InitializeAudioSourceObject(
      const StreamDeviceInfo& device,
      const blink::WebMediaConstraints& constraints,
      bool* is_pending);

  void CreateVideoTracks(
      const StreamDeviceInfoArray& devices,
      const blink::WebMediaConstraints& constraints,
      blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
      UserMediaRequestInfo* request);

  void CreateAudioTracks(
      const StreamDeviceInfoArray& devices,
      const blink::WebMediaConstraints& constraints,
      blink::WebVector<blink::WebMediaStreamTrack>* webkit_tracks,
      UserMediaRequestInfo* request);

  // Callback function triggered when all native versions of the
  // underlying media sources and tracks have been created and started.
  void OnCreateNativeTracksCompleted(
      UserMediaRequestInfo* request,
      MediaStreamRequestResult result,
      const blink::WebString& result_name);

  void OnStreamGeneratedForCancelledRequest(
      const StreamDeviceInfoArray& audio_array,
      const StreamDeviceInfoArray& video_array);

  static void OnAudioSourceStartedOnAudioThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<UserMediaClientImpl> weak_ptr,
      MediaStreamSource* source,
      MediaStreamRequestResult result,
      const blink::WebString& result_name);

  void OnAudioSourceStarted(MediaStreamSource* source,
                            MediaStreamRequestResult result,
                            const blink::WebString& result_name);

  void NotifyAllRequestsOfAudioSourceStarted(
      MediaStreamSource* source,
      MediaStreamRequestResult result,
      const blink::WebString& result_name);

  using EnumerationResult = std::vector<MediaDeviceInfoArray>;
  void FinalizeEnumerateDevices(blink::WebMediaDevicesRequest request,
                                const EnumerationResult& result);

  void DeleteAllUserMediaRequests();

  // Returns the source that use a device with |device.session_id|
  // and |device.device.id|. NULL if such source doesn't exist.
  const blink::WebMediaStreamSource* FindLocalSource(
      const StreamDeviceInfo& device) const {
    return FindLocalSource(local_sources_, device);
  }
  const blink::WebMediaStreamSource* FindPendingLocalSource(
      const StreamDeviceInfo& device) const {
    return FindLocalSource(pending_local_sources_, device);
  }
  const blink::WebMediaStreamSource* FindLocalSource(
      const LocalStreamSources& sources,
      const StreamDeviceInfo& device) const;

  // Looks up a local source and returns it if found. If not found, prepares
  // a new WebMediaStreamSource with a NULL extraData pointer.
  blink::WebMediaStreamSource FindOrInitializeSourceObject(
      const StreamDeviceInfo& device);

  // Returns true if we do find and remove the |source|.
  // Otherwise returns false.
  bool RemoveLocalSource(const blink::WebMediaStreamSource& source);

  void StopLocalSource(const blink::WebMediaStreamSource& source,
                       bool notify_dispatcher);

  const ::mojom::MediaDevicesDispatcherHostPtr& GetMediaDevicesDispatcher();

  struct RequestSettings;

  void SelectAudioInputDevice(
      int request_id,
      const blink::WebUserMediaRequest& user_media_request,
      std::unique_ptr<StreamControls> controls,
      const RequestSettings& request_settings,
      const EnumerationResult& device_enumeration);

  void SetupVideoInput(int request_id,
                       const blink::WebUserMediaRequest& user_media_request,
                       std::unique_ptr<StreamControls> controls,
                       const RequestSettings& request_settings);

  void SelectVideoDeviceSourceSettings(
      int request_id,
      const blink::WebUserMediaRequest& user_media_request,
      std::unique_ptr<StreamControls> controls,
      const RequestSettings& request_settings,
      std::vector<::mojom::VideoInputDeviceCapabilitiesPtr>
          video_input_capabilities);

  void FinalizeSelectVideoDeviceSourceSettings(
      int request_id,
      const blink::WebUserMediaRequest& user_media_request,
      std::unique_ptr<StreamControls> controls,
      const RequestSettings& request_settings,
      const VideoDeviceCaptureSourceSelectionResult& selection_result);

  void FinalizeRequestUserMedia(
      int request_id,
      const blink::WebUserMediaRequest& user_media_request,
      std::unique_ptr<StreamControls> controls,
      const RequestSettings& request_settings);

  // Callback invoked by MediaDevicesEventDispatcher when a device-change
  // notification arrives.
  void DevicesChanged(MediaDeviceType device_type,
                      const MediaDeviceInfoArray& device_infos);

  // Weak ref to a PeerConnectionDependencyFactory, owned by the RenderThread.
  // It's valid for the lifetime of RenderThread.
  // TODO(xians): Remove this dependency once audio do not need it for local
  // audio.
  PeerConnectionDependencyFactory* const dependency_factory_;

  // UserMediaClientImpl owns MediaStreamDispatcher instead of RenderFrameImpl
  // (or RenderFrameObserver) to ensure tear-down occurs in the right order.
  const std::unique_ptr<MediaStreamDispatcher> media_stream_dispatcher_;

  ::mojom::MediaDevicesDispatcherHostPtr media_devices_dispatcher_;

  LocalStreamSources local_sources_;
  LocalStreamSources pending_local_sources_;

  UserMediaRequests user_media_requests_;
  MediaDevicesEventDispatcher::SubscriptionIdList
      device_change_subscription_ids_;

  blink::WebMediaDeviceChangeObserver media_device_change_observer_;

  const scoped_refptr<base::TaskRunner> worker_task_runner_;

  // Note: This member must be the last to ensure all outstanding weak pointers
  // are invalidated first.
  base::WeakPtrFactory<UserMediaClientImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserMediaClientImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_USER_MEDIA_CLIENT_IMPL_H_

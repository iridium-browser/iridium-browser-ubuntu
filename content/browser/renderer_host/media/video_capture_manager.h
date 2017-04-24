// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// VideoCaptureManager is used to open/close, start/stop, enumerate available
// video capture devices, and manage VideoCaptureController's.
// All functions are expected to be called from Browser::IO thread. Some helper
// functions (*OnDeviceThread) will dispatch operations to the device thread.
// VideoCaptureManager will open OS dependent instances of VideoCaptureDevice.
// A device can only be opened once.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_

#include <list>
#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_checker.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "media/base/video_facing.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video_capture_types.h"

#if defined(OS_ANDROID)
#include "base/android/application_status_listener.h"
#endif

namespace content {
class VideoCaptureController;
class VideoCaptureControllerEventHandler;

// VideoCaptureManager opens/closes and start/stops video capture devices.
class CONTENT_EXPORT VideoCaptureManager : public MediaStreamProvider {
 public:
  using VideoCaptureDevice = media::VideoCaptureDevice;

  // Callback used to signal the completion of a controller lookup.
  using DoneCB =
      base::Callback<void(const base::WeakPtr<VideoCaptureController>&)>;

  VideoCaptureManager(
      std::unique_ptr<media::VideoCaptureDeviceFactory> factory,
      scoped_refptr<base::SingleThreadTaskRunner> device_task_runner);

  // AddVideoCaptureObserver() can be called only before any devices are opened.
  // RemoveAllVideoCaptureObservers() can be called only after all devices
  // are closed.
  // They can be called more than once and it's ok to not call at all if the
  // client is not interested in receiving media::VideoCaptureObserver callacks.
  // This methods can be called on whatever thread. The callbacks of
  // media::VideoCaptureObserver arrive on browser IO thread.
  void AddVideoCaptureObserver(media::VideoCaptureObserver* observer);
  void RemoveAllVideoCaptureObservers();

  // Implements MediaStreamProvider.
  void RegisterListener(MediaStreamProviderListener* listener) override;
  void UnregisterListener() override;
  int Open(const StreamDeviceInfo& device) override;
  void Close(int capture_session_id) override;

  // Called by VideoCaptureHost to locate a capture device for |capture_params|,
  // adding the Host as a client of the device's controller if successful. The
  // value of |session_id| controls which device is selected;
  // this value should be a session id previously returned by Open().
  //
  // If the device is not already started (i.e., no other client is currently
  // capturing from this device), this call will cause a VideoCaptureController
  // and VideoCaptureDevice to be created, possibly asynchronously.
  //
  // On success, the controller is returned via calling |done_cb|, indicating
  // that the client was successfully added. A NULL controller is passed to
  // the callback on failure.
  void StartCaptureForClient(media::VideoCaptureSessionId session_id,
                             const media::VideoCaptureParams& capture_params,
                             VideoCaptureControllerID client_id,
                             VideoCaptureControllerEventHandler* client_handler,
                             const DoneCB& done_cb);

  // Called by VideoCaptureHost to remove |client_handler|. If this is the last
  // client of the device, the |controller| and its VideoCaptureDevice may be
  // destroyed. The client must not access |controller| after calling this
  // function.
  void StopCaptureForClient(VideoCaptureController* controller,
                            VideoCaptureControllerID client_id,
                            VideoCaptureControllerEventHandler* client_handler,
                            bool aborted_due_to_error);

  // Called by VideoCaptureHost to pause to update video buffer specified by
  // |client_id| and |client_handler|. If all clients of |controller| are
  // paused, the corresponding device will be closed.
  void PauseCaptureForClient(
      VideoCaptureController* controller,
      VideoCaptureControllerID client_id,
      VideoCaptureControllerEventHandler* client_handler);

  // Called by VideoCaptureHost to resume to update video buffer specified by
  // |client_id| and |client_handler|. The |session_id| and |params| should be
  // same as those used in StartCaptureForClient().
  // If this is first active client of |controller|, device will be allocated
  // and it will take a little time to resume.
  // Allocating device could failed if other app holds the camera, the error
  // will be notified through VideoCaptureControllerEventHandler::OnError().
  void ResumeCaptureForClient(
      media::VideoCaptureSessionId session_id,
      const media::VideoCaptureParams& params,
      VideoCaptureController* controller,
      VideoCaptureControllerID client_id,
      VideoCaptureControllerEventHandler* client_handler);

  // Called by VideoCaptureHost to request a refresh frame from the video
  // capture device.
  void RequestRefreshFrameForClient(VideoCaptureController* controller);

  // Retrieves all capture supported formats for a particular device. Returns
  // false if the |capture_session_id| is not found. The supported formats are
  // cached during device(s) enumeration, and depending on the underlying
  // implementation, could be an empty list.
  bool GetDeviceSupportedFormats(
      media::VideoCaptureSessionId capture_session_id,
      media::VideoCaptureFormats* supported_formats);
  // Retrieves all capture supported formats for a particular device. Returns
  // false if the  |device_id| is not found. The supported formats are cached
  // during device(s) enumeration, and depending on the underlying
  // implementation, could be an empty list.
  bool GetDeviceSupportedFormats(const std::string& device_id,
                                 media::VideoCaptureFormats* supported_formats);

  // Retrieves the format(s) currently in use.  Returns false if the
  // |capture_session_id| is not found. Returns true and |formats_in_use|
  // otherwise. |formats_in_use| is empty if the device is not in use.
  bool GetDeviceFormatsInUse(media::VideoCaptureSessionId capture_session_id,
                             media::VideoCaptureFormats* formats_in_use);
  // Retrieves the format(s) currently in use.  Returns false if the
  // |stream_type|, |device_id| pair is not found. Returns true and
  // |formats_in_use| otherwise. |formats_in_use| is empty if the device is not
  // in use.
  bool GetDeviceFormatsInUse(MediaStreamType stream_type,
                             const std::string& device_id,
                             media::VideoCaptureFormats* supported_formats);

  // Sets the platform-dependent window ID for the desktop capture notification
  // UI for the given session.
  void SetDesktopCaptureWindowId(media::VideoCaptureSessionId session_id,
                                 gfx::NativeViewId window_id);

  // Gets a weak reference to the device factory, used for tests.
  media::VideoCaptureDeviceFactory* video_capture_device_factory() const {
    return video_capture_device_factory_.get();
  }

#if defined(OS_WIN)
  void set_device_task_runner(
      const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner) {
    device_task_runner_ = device_task_runner;
  }
#endif

  // Returns the SingleThreadTaskRunner where devices are enumerated on and
  // started.
  scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner() {
    return device_task_runner_;
  }

  void GetPhotoCapabilities(
      int session_id,
      VideoCaptureDevice::GetPhotoCapabilitiesCallback callback);
  void SetPhotoOptions(int session_id,
                       media::mojom::PhotoSettingsPtr settings,
                       VideoCaptureDevice::SetPhotoOptionsCallback callback);
  void TakePhoto(int session_id,
                 VideoCaptureDevice::TakePhotoCallback callback);

#if defined(OS_ANDROID)
  // Some devices had troubles when stopped and restarted quickly, so the device
  // is only stopped when Chrome is sent to background and not when, e.g., a tab
  // is hidden, see http://crbug.com/582295.
  void OnApplicationStateChange(base::android::ApplicationState state);
#endif

  using EnumerationCallback =
      base::Callback<void(const media::VideoCaptureDeviceDescriptors&)>;
  void EnumerateDevices(const EnumerationCallback& client_callback);

  // Retrieves camera calibration information for a particular device. Returns
  // nullopt_t if the |device_id| is not found or camera calibration information
  // is not available for the device.  Camera calibration is cached during
  // device(s) enumeration.
  base::Optional<CameraCalibration> GetCameraCalibration(
      const std::string& device_id);

 private:
  class CaptureDeviceStartRequest;
  struct DeviceEntry;
  struct DeviceInfo;

  using SessionMap = std::map<media::VideoCaptureSessionId, MediaStreamDevice>;
  using DeviceEntries = std::vector<std::unique_ptr<DeviceEntry>>;
  using DeviceInfos = std::vector<DeviceInfo>;
  using DeviceStartQueue = std::list<CaptureDeviceStartRequest>;
  using VideoCaptureDeviceDescriptor = media::VideoCaptureDeviceDescriptor;
  using VideoCaptureDeviceDescriptors = media::VideoCaptureDeviceDescriptors;

  ~VideoCaptureManager() override;

  // Helpers to report an event to our Listener.
  void OnOpened(MediaStreamType type,
                media::VideoCaptureSessionId capture_session_id);
  void OnClosed(MediaStreamType type,
                media::VideoCaptureSessionId capture_session_id);
  void OnDevicesInfoEnumerated(base::ElapsedTimer* timer,
                               const EnumerationCallback& client_callback,
                               const DeviceInfos& new_devices_info_cache);

  bool IsOnDeviceThread() const;

  // Consolidates the cached devices list with the list of currently connected
  // devices in the system |names_snapshot|. Retrieves the supported formats of
  // the new devices and sends the new cache to OnDevicesInfoEnumerated().
  void ConsolidateDevicesInfoOnDeviceThread(
      base::Callback<void(const DeviceInfos&)> on_devices_enumerated_callback,
      const DeviceInfos& old_device_info_cache,
      std::unique_ptr<VideoCaptureDeviceDescriptors> descriptors_snapshot);

  // Checks to see if |entry| has no clients left on its controller. If so,
  // remove it from the list of devices, and delete it asynchronously. |entry|
  // may be freed by this function.
  void DestroyDeviceEntryIfNoClients(DeviceEntry* entry);

  // Finds a DeviceEntry in different ways: by |session_id|, by its |device_id|
  // and |type| (if it is already opened), by its |controller| or by its
  // |serial_id|. In all cases, if not found, nullptr is returned.
  DeviceEntry* GetDeviceEntryBySessionId(int session_id);
  DeviceEntry* GetDeviceEntryByTypeAndId(MediaStreamType type,
                                         const std::string& device_id) const;
  DeviceEntry* GetDeviceEntryByController(
      const VideoCaptureController* controller) const;
  DeviceEntry* GetDeviceEntryBySerialId(int serial_id) const;

  // Finds the device info by |id| in |devices_info_cache_|, or nullptr.
  DeviceInfo* GetDeviceInfoById(const std::string& id);

  // Finds a DeviceEntry entry for the indicated |capture_session_id|, creating
  // a fresh one if necessary. Returns nullptr if said |capture_session_id| is
  // invalid.
  DeviceEntry* GetOrCreateDeviceEntry(
      media::VideoCaptureSessionId capture_session_id,
      const media::VideoCaptureParams& params);

  // Starting a capture device can take 1-2 seconds.
  // To avoid multiple unnecessary start/stop commands to the OS, each start
  // request is queued in |device_start_queue_|.
  // QueueStartDevice creates a new entry in |device_start_queue_| and posts a
  // request to start the device on the device thread unless there is
  // another request pending start.
  void QueueStartDevice(media::VideoCaptureSessionId session_id,
                        DeviceEntry* entry,
                        const media::VideoCaptureParams& params);
  void OnDeviceStarted(
      int serial_id,
      std::unique_ptr<VideoCaptureDevice> device);
  void DoStopDevice(DeviceEntry* entry);
  void HandleQueuedStartRequest();

  // Creates and Starts a new VideoCaptureDevice. The resulting
  // VideoCaptureDevice is returned to the IO-thread and stored in
  // a DeviceEntry in |devices_|. Ownership of |client| passes to
  // the device.
  std::unique_ptr<VideoCaptureDevice> DoStartDeviceCaptureOnDeviceThread(
      const VideoCaptureDeviceDescriptor& descriptor,
      const media::VideoCaptureParams& params,
      std::unique_ptr<VideoCaptureDevice::Client> client);

  std::unique_ptr<VideoCaptureDevice> DoStartTabCaptureOnDeviceThread(
      const std::string& device_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<VideoCaptureDevice::Client> client);

  std::unique_ptr<VideoCaptureDevice> DoStartDesktopCaptureOnDeviceThread(
      const std::string& device_id,
      const media::VideoCaptureParams& params,
      std::unique_ptr<VideoCaptureDevice::Client> client);

  // Stops and destroys the VideoCaptureDevice held in |device|.
  void DoStopDeviceOnDeviceThread(std::unique_ptr<VideoCaptureDevice> device);

  void MaybePostDesktopCaptureWindowId(media::VideoCaptureSessionId session_id);
  void SetDesktopCaptureWindowIdOnDeviceThread(
      media::VideoCaptureDevice* device,
      gfx::NativeViewId window_id);

  // Internal versions of the Image Capture public ones, for delayed execution.
  void DoGetPhotoCapabilities(
      VideoCaptureDevice::GetPhotoCapabilitiesCallback callback,
      VideoCaptureDevice* device);
  void DoSetPhotoOptions(
      VideoCaptureDevice::SetPhotoOptionsCallback callback,
      media::mojom::PhotoSettingsPtr settings,
      VideoCaptureDevice* device);
  void DoTakePhoto(VideoCaptureDevice::TakePhotoCallback callback,
                   VideoCaptureDevice* device);

#if defined(OS_ANDROID)
  void ReleaseDevices();
  void ResumeDevices();

  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;
  bool application_state_has_running_activities_;
#endif

  // The message loop of media stream device thread, where VCD's live.
  scoped_refptr<base::SingleThreadTaskRunner> device_task_runner_;

  // Only accessed on Browser::IO thread.
  MediaStreamProviderListener* listener_;
  media::VideoCaptureSessionId new_capture_session_id_;

  // An entry is kept in this map for every session that has been created via
  // the Open() entry point. The keys are session_id's. This map is used to
  // determine which device to use when StartCaptureForClient() occurs. Used
  // only on the IO thread.
  SessionMap sessions_;

  // Currently opened DeviceEntry instances (each owning a VideoCaptureDevice -
  // VideoCaptureController pair). The device may or may not be started. This
  // member is only accessed on IO thread.
  DeviceEntries devices_;

  DeviceStartQueue device_start_queue_;

  // Queue to keep photo-associated requests waiting for a device to initialize,
  // bundles a session id integer and an associated photo-related request.
  std::list<std::pair<int, base::Callback<void(media::VideoCaptureDevice*)>>>
      photo_request_queue_;

  // Device creation factory injected on construction from MediaStreamManager or
  // from the test harness.
  std::unique_ptr<media::VideoCaptureDeviceFactory>
      video_capture_device_factory_;

  base::ObserverList<media::VideoCaptureObserver> capture_observers_;

  // Local cache of the enumerated video capture devices' names and capture
  // supported formats. A snapshot of the current devices and their capabilities
  // is composed in VideoCaptureDeviceFactory::EnumerateDeviceNames() and
  // ConsolidateDevicesInfoOnDeviceThread(), and this snapshot is used to update
  // this list in OnDevicesInfoEnumerated(). GetDeviceSupportedFormats() will
  // use this list if the device is not started, otherwise it will retrieve the
  // active device capture format from the VideoCaptureController associated.
  DeviceInfos devices_info_cache_;

  // Map used by DesktopCapture.
  std::map<media::VideoCaptureSessionId, gfx::NativeViewId>
      notification_window_ids_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_MANAGER_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/media/capture/desktop_capture_device_uma_types.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_gpu_jpeg_decoder.h"
#include "content/browser/renderer_host/media/video_frame_receiver_on_io_thread.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_device_factory.h"

#if defined(ENABLE_SCREEN_CAPTURE)

#if BUILDFLAG(ENABLE_WEBRTC) && !defined(OS_ANDROID)
#include "content/browser/media/capture/desktop_capture_device.h"
#endif

#if defined(USE_AURA)
#include "content/browser/media/capture/desktop_capture_device_aura.h"
#endif

#if defined(OS_ANDROID)
#include "content/browser/media/capture/screen_capture_device_android.h"
#endif

#endif  // defined(ENABLE_SCREEN_CAPTURE)

namespace {

class VideoFrameConsumerFeedbackObserverOnTaskRunner
    : public media::VideoFrameConsumerFeedbackObserver {
 public:
  VideoFrameConsumerFeedbackObserverOnTaskRunner(
      media::VideoFrameConsumerFeedbackObserver* observer,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : observer_(observer), task_runner_(std::move(task_runner)) {}

  void OnUtilizationReport(int frame_feedback_id, double utilization) override {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(
            &media::VideoFrameConsumerFeedbackObserver::OnUtilizationReport,
            base::Unretained(observer_), frame_feedback_id, utilization));
  }

 private:
  media::VideoFrameConsumerFeedbackObserver* const observer_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

// Compares two VideoCaptureFormat by checking smallest frame_size area, then
// by _largest_ frame_rate. Used to order a VideoCaptureFormats vector so that
// the first entry for a given resolution has the largest frame rate, as needed
// by the ConsolidateCaptureFormats() method.
bool IsCaptureFormatSmaller(const media::VideoCaptureFormat& format1,
                            const media::VideoCaptureFormat& format2) {
  DCHECK(format1.frame_size.GetCheckedArea().IsValid());
  DCHECK(format2.frame_size.GetCheckedArea().IsValid());
  if (format1.frame_size.GetCheckedArea().ValueOrDefault(0) ==
      format2.frame_size.GetCheckedArea().ValueOrDefault(0)) {
    return format1.frame_rate > format2.frame_rate;
  }
  return format1.frame_size.GetCheckedArea().ValueOrDefault(0) <
         format2.frame_size.GetCheckedArea().ValueOrDefault(0);
}

bool IsCaptureFormatSizeEqual(const media::VideoCaptureFormat& format1,
                              const media::VideoCaptureFormat& format2) {
  DCHECK(format1.frame_size.GetCheckedArea().IsValid());
  DCHECK(format2.frame_size.GetCheckedArea().IsValid());
  return format1.frame_size.GetCheckedArea().ValueOrDefault(0) ==
         format2.frame_size.GetCheckedArea().ValueOrDefault(0);
}

// This function receives a list of capture formats, removes duplicated
// resolutions while keeping the highest frame rate for each, and forcing I420
// pixel format.
void ConsolidateCaptureFormats(media::VideoCaptureFormats* formats) {
  if (formats->empty())
    return;
  std::sort(formats->begin(), formats->end(), IsCaptureFormatSmaller);
  // Due to the ordering imposed, the largest frame_rate is kept while removing
  // duplicated resolutions.
  media::VideoCaptureFormats::iterator last =
      std::unique(formats->begin(), formats->end(), IsCaptureFormatSizeEqual);
  formats->erase(last, formats->end());
  // Mark all formats as I420, since this is what the renderer side will get
  // anyhow: the actual pixel format is decided at the device level.
  // Don't do this for Y16 format as it is handled separatelly.
  for (auto& format : *formats) {
    if (format.pixel_format != media::PIXEL_FORMAT_Y16)
      format.pixel_format = media::PIXEL_FORMAT_I420;
  }
}

// The maximum number of video frame buffers in-flight at any one time. This
// value should be based on the logical capacity of the capture pipeline, and
// not on hardware performance.  For example, tab capture requires more buffers
// than webcam capture because the pipeline is longer (it includes read-backs
// pending in the GPU pipeline).
const int kMaxNumberOfBuffers = 3;
// TODO(miu): The value for tab capture should be determined programmatically.
// http://crbug.com/460318
const int kMaxNumberOfBuffersForTabCapture = 10;

// Used for logging capture events.
// Elements in this enum should not be deleted or rearranged; the only
// permitted operation is to add new elements before NUM_VIDEO_CAPTURE_EVENT.
enum VideoCaptureEvent {
  VIDEO_CAPTURE_START_CAPTURE = 0,
  VIDEO_CAPTURE_STOP_CAPTURE_OK = 1,
  VIDEO_CAPTURE_STOP_CAPTURE_DUE_TO_ERROR = 2,
  VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DEVICE = 3,
  VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DESKTOP_OR_TAB = 4,
  NUM_VIDEO_CAPTURE_EVENT
};

void LogVideoCaptureEvent(VideoCaptureEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCaptureManager.Event",
                            event,
                            NUM_VIDEO_CAPTURE_EVENT);
}

// Counter used for identifying a DeviceRequest to start a capture device.
static int g_device_start_id = 0;

const media::VideoCaptureSessionId kFakeSessionId = -1;

std::unique_ptr<media::VideoCaptureJpegDecoder> CreateGpuJpegDecoder(
    const media::VideoCaptureJpegDecoder::DecodeDoneCB& decode_done_cb) {
  return base::MakeUnique<content::VideoCaptureGpuJpegDecoder>(decode_done_cb);
}

}  // namespace

namespace content {

// Instances of this struct go through several different phases during their
// lifetime.
// Phase 1: When first created (in GetOrCreateDeviceEntry()), this consists of
// only the |video_capture_controller|. Clients can already connect to the
// controller, but there is no |buffer_pool| or |video_capture_device| present.
// Phase 2: When a request to "start" the entry comes in (via
// HandleQueuedStartRequest()), |buffer_pool| is created and creation of
// |video_capture_device| is scheduled to run asynchronously on the Device
// Thread.
// Phase 3: As soon as the creation of the VideoCaptureDevice is complete, this
// newly created VideoCaptureDevice instance is connected to the
// VideoCaptureController via SetConsumerFeedbackObserver(). Furthermore, the
// |buffer_pool| is connected to the |video_capture_controller| as a
// FrameBufferPool via SetFrameBufferPool().
// Phase 4: This phase can only be reached on Android. When the application goes
// to the background, the |video_capture_device| is asynchronously stopped and
// released on the Device Thread. The existing |buffer_pool| is kept alive, and
// all clients of |video_capture_controller| stay connected. When the
// application is resumed, we transition to Phase 2, except that the existing
// |buffer_pool| get reused instead of creating a new one.
struct VideoCaptureManager::DeviceEntry {
 public:
  DeviceEntry(MediaStreamType stream_type,
              const std::string& id,
              const media::VideoCaptureParams& params);
  ~DeviceEntry();
  std::unique_ptr<media::VideoCaptureDevice::Client> CreateDeviceClient();
  std::unique_ptr<media::FrameBufferPool> CreateFrameBufferPool();

  const int serial_id;
  const MediaStreamType stream_type;
  const std::string id;
  const media::VideoCaptureParams parameters;
  VideoCaptureController video_capture_controller;
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool;
  std::unique_ptr<media::VideoCaptureDevice> video_capture_device;
};

// Bundles a media::VideoCaptureDeviceDescriptor with corresponding supported
// video formats.
struct VideoCaptureManager::DeviceInfo {
  DeviceInfo();
  DeviceInfo(media::VideoCaptureDeviceDescriptor descriptor);
  DeviceInfo(const DeviceInfo& other);
  ~DeviceInfo();
  DeviceInfo& operator=(const DeviceInfo& other);

  media::VideoCaptureDeviceDescriptor descriptor;
  media::VideoCaptureFormats supported_formats;
};

class BufferPoolFrameBufferPool : public media::FrameBufferPool {
 public:
  explicit BufferPoolFrameBufferPool(
      scoped_refptr<media::VideoCaptureBufferPool> buffer_pool)
      : buffer_pool_(std::move(buffer_pool)) {}

  void SetBufferHold(int buffer_id) override {
    buffer_pool_->HoldForConsumers(buffer_id, 1);
  }

  void ReleaseBufferHold(int buffer_id) override {
    buffer_pool_->RelinquishConsumerHold(buffer_id, 1);
  }

 private:
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool_;
};

// Class used for queuing request for starting a device.
class VideoCaptureManager::CaptureDeviceStartRequest {
 public:
  CaptureDeviceStartRequest(int serial_id,
                            media::VideoCaptureSessionId session_id,
                            const media::VideoCaptureParams& params);
  int serial_id() const { return serial_id_; }
  media::VideoCaptureSessionId session_id() const { return session_id_; }
  media::VideoCaptureParams params() const { return params_; }

  // Set to true if the device should be stopped before it has successfully
  // been started.
  bool abort_start() const { return abort_start_; }
  void set_abort_start() { abort_start_ = true; }

 private:
  const int serial_id_;
  const media::VideoCaptureSessionId session_id_;
  const media::VideoCaptureParams params_;
  // Set to true if the device should be stopped before it has successfully
  // been started.
  bool abort_start_;
};

VideoCaptureManager::DeviceEntry::DeviceEntry(
    MediaStreamType stream_type,
    const std::string& id,
    const media::VideoCaptureParams& params)
    : serial_id(g_device_start_id++),
      stream_type(stream_type),
      id(id),
      parameters(params) {}

VideoCaptureManager::DeviceEntry::~DeviceEntry() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // DCHECK that this DeviceEntry does not still own a
  // media::VideoCaptureDevice. media::VideoCaptureDevice must be deleted on
  // the device thread.
  DCHECK(video_capture_device == nullptr);
}

std::unique_ptr<media::VideoCaptureDevice::Client>
VideoCaptureManager::DeviceEntry::CreateDeviceClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const int max_buffers = stream_type == MEDIA_TAB_VIDEO_CAPTURE
                              ? kMaxNumberOfBuffersForTabCapture
                              : kMaxNumberOfBuffers;
  if (!buffer_pool) {
    buffer_pool = new media::VideoCaptureBufferPoolImpl(
        base::MakeUnique<media::VideoCaptureBufferTrackerFactoryImpl>(),
        max_buffers);
  }

  return base::MakeUnique<media::VideoCaptureDeviceClient>(
      base::MakeUnique<VideoFrameReceiverOnIOThread>(
          video_capture_controller.GetWeakPtrForIOThread()),
      buffer_pool,
      base::Bind(
          &CreateGpuJpegDecoder,
          base::Bind(&media::VideoFrameReceiver::OnIncomingCapturedVideoFrame,
                     video_capture_controller.GetWeakPtrForIOThread())));
}

std::unique_ptr<media::FrameBufferPool>
VideoCaptureManager::DeviceEntry::CreateFrameBufferPool() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(buffer_pool);
  return base::MakeUnique<BufferPoolFrameBufferPool>(buffer_pool);
}

VideoCaptureManager::DeviceInfo::DeviceInfo() = default;

VideoCaptureManager::DeviceInfo::DeviceInfo(
    media::VideoCaptureDeviceDescriptor descriptor)
    : descriptor(descriptor) {}

VideoCaptureManager::DeviceInfo::DeviceInfo(
    const VideoCaptureManager::DeviceInfo& other) = default;

VideoCaptureManager::DeviceInfo::~DeviceInfo() = default;

VideoCaptureManager::DeviceInfo& VideoCaptureManager::DeviceInfo::operator=(
    const VideoCaptureManager::DeviceInfo& other) = default;

VideoCaptureManager::CaptureDeviceStartRequest::CaptureDeviceStartRequest(
    int serial_id,
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params)
    : serial_id_(serial_id),
      session_id_(session_id),
      params_(params),
      abort_start_(false) {
}

VideoCaptureManager::VideoCaptureManager(
    std::unique_ptr<media::VideoCaptureDeviceFactory> factory)
    : listener_(nullptr),
      new_capture_session_id_(1),
      video_capture_device_factory_(std::move(factory)) {}

VideoCaptureManager::~VideoCaptureManager() {
  DCHECK(devices_.empty());
  DCHECK(device_start_queue_.empty());
}

void VideoCaptureManager::Register(
    MediaStreamProviderListener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& device_task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!listener_);
  DCHECK(!device_task_runner_.get());
  listener_ = listener;
  device_task_runner_ = device_task_runner;
#if defined(OS_ANDROID)
  application_state_has_running_activities_ = true;
  app_status_listener_.reset(new base::android::ApplicationStatusListener(
      base::Bind(&VideoCaptureManager::OnApplicationStateChange,
                 base::Unretained(this))));
#endif
}

void VideoCaptureManager::Unregister() {
  DCHECK(listener_);
  listener_ = nullptr;
}

void VideoCaptureManager::EnumerateDevices(
    const EnumerationCallback& client_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureManager::EnumerateDevices";

  // Bind a callback to ConsolidateDevicesInfoOnDeviceThread() with an argument
  // for another callback to OnDevicesInfoEnumerated() to be run in the current
  // loop, i.e. IO loop. Pass a timer for UMA histogram collection.
  base::Callback<void(std::unique_ptr<VideoCaptureDeviceDescriptors>)>
      devices_enumerated_callback = base::Bind(
          &VideoCaptureManager::ConsolidateDevicesInfoOnDeviceThread, this,
          media::BindToCurrentLoop(base::Bind(
              &VideoCaptureManager::OnDevicesInfoEnumerated, this,
              base::Owned(new base::ElapsedTimer()), client_callback)),
          devices_info_cache_);
  // OK to use base::Unretained() since we own the VCDFactory and |this| is
  // bound in |devices_enumerated_callback|.
  device_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&media::VideoCaptureDeviceFactory::EnumerateDeviceDescriptors,
                 base::Unretained(video_capture_device_factory_.get()),
                 devices_enumerated_callback));
}

int VideoCaptureManager::Open(const StreamDeviceInfo& device_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener_);

  // Generate a new id for the session being opened.
  const media::VideoCaptureSessionId capture_session_id =
      new_capture_session_id_++;

  DCHECK(sessions_.find(capture_session_id) == sessions_.end());
  DVLOG(1) << "VideoCaptureManager::Open, id " << capture_session_id;

  // We just save the stream info for processing later.
  sessions_[capture_session_id] = device_info.device;

  // Notify our listener asynchronously; this ensures that we return
  // |capture_session_id| to the caller of this function before using that same
  // id in a listener event.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&VideoCaptureManager::OnOpened, this,
                            device_info.device.type, capture_session_id));
  return capture_session_id;
}

void VideoCaptureManager::Close(int capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(listener_);
  DVLOG(1) << "VideoCaptureManager::Close, id " << capture_session_id;

  SessionMap::iterator session_it = sessions_.find(capture_session_id);
  if (session_it == sessions_.end()) {
    NOTREACHED();
    return;
  }

  DeviceEntry* const existing_device =
      GetDeviceEntryByTypeAndId(session_it->second.type, session_it->second.id);
  if (existing_device) {
    // Remove any client that is still using the session. This is safe to call
    // even if there are no clients using the session.
    existing_device->video_capture_controller.StopSession(capture_session_id);

    // StopSession() may have removed the last client, so we might need to
    // close the device.
    DestroyDeviceEntryIfNoClients(existing_device);
  }

  // Notify listeners asynchronously, and forget the session.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&VideoCaptureManager::OnClosed, this,
                            session_it->second.type, capture_session_id));
  sessions_.erase(session_it);
}

void VideoCaptureManager::QueueStartDevice(
    media::VideoCaptureSessionId session_id,
    DeviceEntry* entry,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  device_start_queue_.push_back(
      CaptureDeviceStartRequest(entry->serial_id, session_id, params));
  if (device_start_queue_.size() == 1)
    HandleQueuedStartRequest();
}

void VideoCaptureManager::DoStopDevice(DeviceEntry* entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mcasas): use a helper function https://crbug.com/624854.
  DCHECK(
      std::find_if(devices_.begin(), devices_.end(),
                   [entry](const std::unique_ptr<DeviceEntry>& device_entry) {
                     return device_entry.get() == entry;
                   }) != devices_.end());

  // Find the matching start request.
  for (DeviceStartQueue::reverse_iterator request =
           device_start_queue_.rbegin();
       request != device_start_queue_.rend(); ++request) {
    if (request->serial_id() == entry->serial_id) {
      request->set_abort_start();
      DVLOG(3) << "DoStopDevice, aborting start request for device "
               << entry->id << " serial_id = " << entry->serial_id;
      return;
    }
  }

  DVLOG(3) << "DoStopDevice. Send stop request for device = " << entry->id
           << " serial_id = " << entry->serial_id << ".";
  entry->video_capture_controller.OnLog(
      base::StringPrintf("Stopping device: id: %s", entry->id.c_str()));
  entry->video_capture_controller.SetConsumerFeedbackObserver(nullptr);
  entry->video_capture_controller.SetFrameBufferPool(nullptr);

  // |entry->video_capture_device| can be null if creating the device has
  // failed.
  if (entry->video_capture_device) {
    device_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureManager::DoStopDeviceOnDeviceThread, this,
                   base::Passed(&entry->video_capture_device)));
  }
}

void VideoCaptureManager::HandleQueuedStartRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Remove all start requests that have been aborted.
  while (device_start_queue_.begin() != device_start_queue_.end() &&
      device_start_queue_.begin()->abort_start()) {
    device_start_queue_.pop_front();
  }
  DeviceStartQueue::iterator request = device_start_queue_.begin();
  if (request == device_start_queue_.end())
    return;

  const int serial_id = request->serial_id();
  DeviceEntry* const entry = GetDeviceEntryBySerialId(serial_id);
  DCHECK(entry);

  DVLOG(3) << "HandleQueuedStartRequest, Post start to device thread, device = "
           << entry->id << " start id = " << entry->serial_id;

  std::unique_ptr<media::VideoCaptureDevice::Client> device_client =
      entry->CreateDeviceClient();
  std::unique_ptr<media::FrameBufferPool> frame_buffer_pool =
      entry->CreateFrameBufferPool();

  base::Callback<std::unique_ptr<VideoCaptureDevice>(void)>
      start_capture_function;

  switch (entry->stream_type) {
    case MEDIA_DEVICE_VIDEO_CAPTURE: {
      // We look up the device id from the renderer in our local enumeration
      // since the renderer does not have all the information that might be
      // held in the browser-side VideoCaptureDevice::Name structure.
      const DeviceInfo* found = GetDeviceInfoById(entry->id);
      if (found) {
        entry->video_capture_controller.OnLog(
            base::StringPrintf("Starting device: id: %s, name: %s, api: %s",
                               found->descriptor.device_id.c_str(),
                               found->descriptor.GetNameAndModel().c_str(),
                               found->descriptor.GetCaptureApiTypeString()));

        start_capture_function =
            base::Bind(&VideoCaptureManager::DoStartDeviceCaptureOnDeviceThread,
                       this, found->descriptor, request->params(),
                       base::Passed(std::move(device_client)));
      } else {
        // Errors from DoStartDeviceCaptureOnDeviceThread go via
        // VideoCaptureDeviceClient::OnError, which needs some thread
        // dancing to get errors processed on the IO thread. But since
        // we're on that thread, we call VideoCaptureController
        // methods directly.
        const std::string log_message = base::StringPrintf(
            "Error on %s:%d: device %s unknown. Maybe recently disconnected?",
            __FILE__, __LINE__, entry->id.c_str());
        DLOG(ERROR) << log_message;
        entry->video_capture_controller.OnLog(log_message);
        entry->video_capture_controller.OnError();
        // Drop the failed start request.
        device_start_queue_.pop_front();

        return;
      }
      break;
    }
    case MEDIA_TAB_VIDEO_CAPTURE:
      start_capture_function = base::Bind(
          &VideoCaptureManager::DoStartTabCaptureOnDeviceThread, this,
          entry->id, request->params(), base::Passed(std::move(device_client)));
      break;

    case MEDIA_DESKTOP_VIDEO_CAPTURE:
      start_capture_function = base::Bind(
          &VideoCaptureManager::DoStartDesktopCaptureOnDeviceThread, this,
          entry->id, request->params(), base::Passed(std::move(device_client)));
      break;

    default: {
      NOTIMPLEMENTED();
      return;
    }
  }
  base::PostTaskAndReplyWithResult(
      device_task_runner_.get(), FROM_HERE, start_capture_function,
      base::Bind(&VideoCaptureManager::OnDeviceStarted, this,
                 request->serial_id(), base::Passed(&frame_buffer_pool)));
}

void VideoCaptureManager::OnDeviceStarted(
    int serial_id,
    std::unique_ptr<media::FrameBufferPool> frame_buffer_pool,
    std::unique_ptr<VideoCaptureDevice> device) {
  DVLOG(3) << __func__;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(serial_id, device_start_queue_.begin()->serial_id());
  // |device| can be null if creation failed in
  //  DoStartDeviceCaptureOnDeviceThread.
  if (device_start_queue_.front().abort_start()) {
    // The device is no longer wanted. Stop the device again.
    DVLOG(3) << "OnDeviceStarted but start request have been aborted.";
    media::VideoCaptureDevice* device_ptr = device.get();
    base::Closure closure =
        base::Bind(&VideoCaptureManager::DoStopDeviceOnDeviceThread, this,
                   base::Passed(&device));
    if (device_ptr && !device_task_runner_->PostTask(FROM_HERE, closure)) {
      // PostTask failed. The device must be stopped anyway.
      device_ptr->StopAndDeAllocate();
    }
  } else {
    DeviceEntry* const entry = GetDeviceEntryBySerialId(serial_id);
    DCHECK(entry);
    DCHECK(!entry->video_capture_device);
    if (device) {
      entry->video_capture_controller.SetFrameBufferPool(
          std::move(frame_buffer_pool));
      // Passing raw pointer |device.get()| to the controller is safe,
      // because we transfer ownership of it to |entry|. We are calling
      // SetConsumerFeedbackObserver(nullptr) before releasing
      // |entry->video_capture_device_| on the |device_task_runner_|.
      entry->video_capture_controller.SetConsumerFeedbackObserver(
          base::MakeUnique<VideoFrameConsumerFeedbackObserverOnTaskRunner>(
              device.get(), device_task_runner_));
    }
    entry->video_capture_device = std::move(device);

    if (entry->stream_type == MEDIA_DESKTOP_VIDEO_CAPTURE) {
      const media::VideoCaptureSessionId session_id =
          device_start_queue_.front().session_id();
      DCHECK(session_id != kFakeSessionId);
      MaybePostDesktopCaptureWindowId(session_id);
    }

    auto it = photo_request_queue_.begin();
    while (it != photo_request_queue_.end()) {
      auto request = it++;
      DeviceEntry* maybe_entry = GetDeviceEntryBySessionId(request->first);
      if (maybe_entry && maybe_entry->video_capture_device) {
        request->second.Run(maybe_entry->video_capture_device.get());
        photo_request_queue_.erase(request);
      }
    }
  }

  device_start_queue_.pop_front();
  HandleQueuedStartRequest();
}

std::unique_ptr<media::VideoCaptureDevice>
VideoCaptureManager::DoStartDeviceCaptureOnDeviceThread(
    const VideoCaptureDeviceDescriptor& descriptor,
    const media::VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> device_client) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(IsOnDeviceThread());

  std::unique_ptr<VideoCaptureDevice> video_capture_device;
  video_capture_device =
      video_capture_device_factory_->CreateDevice(descriptor);

  if (!video_capture_device) {
    device_client->OnError(FROM_HERE, "Could not create capture device");
    return nullptr;
  }

  video_capture_device->AllocateAndStart(params, std::move(device_client));
  return video_capture_device;
}

std::unique_ptr<media::VideoCaptureDevice>
VideoCaptureManager::DoStartTabCaptureOnDeviceThread(
    const std::string& id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> device_client) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(IsOnDeviceThread());

  std::unique_ptr<VideoCaptureDevice> video_capture_device;
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
  video_capture_device = WebContentsVideoCaptureDevice::Create(id);
#endif

  if (!video_capture_device) {
    device_client->OnError(FROM_HERE, "Could not create capture device");
    return nullptr;
  }

  video_capture_device->AllocateAndStart(params, std::move(device_client));
  return video_capture_device;
}

std::unique_ptr<media::VideoCaptureDevice>
VideoCaptureManager::DoStartDesktopCaptureOnDeviceThread(
    const std::string& id,
    const media::VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> device_client) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StartDeviceTime");
  DCHECK(IsOnDeviceThread());

  std::unique_ptr<VideoCaptureDevice> video_capture_device;
#if defined(ENABLE_SCREEN_CAPTURE)
  DesktopMediaID desktop_id = DesktopMediaID::Parse(id);
  if (desktop_id.is_null()) {
    device_client->OnError(FROM_HERE, "Desktop media ID is null");
    return nullptr;
  }

  if (desktop_id.type == DesktopMediaID::TYPE_WEB_CONTENTS) {
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_WIN)
    video_capture_device = WebContentsVideoCaptureDevice::Create(id);
    IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED);
    if (desktop_id.audio_share) {
      IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED_WITH_AUDIO);
    } else {
      IncrementDesktopCaptureCounter(TAB_VIDEO_CAPTURER_CREATED_WITHOUT_AUDIO);
    }
#endif
  } else {
#if defined(OS_ANDROID)
    video_capture_device = base::MakeUnique<ScreenCaptureDeviceAndroid>();
#else
#if defined(USE_AURA)
    video_capture_device = DesktopCaptureDeviceAura::Create(desktop_id);
#endif  // defined(USE_AURA)
#if BUILDFLAG(ENABLE_WEBRTC)
  if (!video_capture_device)
    video_capture_device = DesktopCaptureDevice::Create(desktop_id);
#endif  // BUILDFLAG(ENABLE_WEBRTC)
#endif  // defined (OS_ANDROID)
  }
#endif  // defined(ENABLE_SCREEN_CAPTURE)

  if (!video_capture_device) {
    device_client->OnError(FROM_HERE, "Could not create capture device");
    return nullptr;
  }

  video_capture_device->AllocateAndStart(params, std::move(device_client));
  return video_capture_device;
}

void VideoCaptureManager::StartCaptureForClient(
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    const DoneCB& done_cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << __func__ << ", session_id = " << session_id << ", request: "
           << media::VideoCaptureFormat::ToString(params.requested_format);

  DeviceEntry* entry = GetOrCreateDeviceEntry(session_id, params);
  if (!entry) {
    done_cb.Run(base::WeakPtr<VideoCaptureController>());
    return;
  }

  LogVideoCaptureEvent(VIDEO_CAPTURE_START_CAPTURE);

  // First client starts the device.
  if (!entry->video_capture_controller.HasActiveClient() &&
      !entry->video_capture_controller.HasPausedClient()) {
    DVLOG(1) << "VideoCaptureManager starting device (type = "
             << entry->stream_type << ", id = " << entry->id << ")";
    QueueStartDevice(session_id, entry, params);
  }
  // Run the callback first, as AddClient() may trigger OnFrameInfo().
  done_cb.Run(entry->video_capture_controller.GetWeakPtrForIOThread());
  entry->video_capture_controller.AddClient(client_id, client_handler,
                                            session_id, params);
}

void VideoCaptureManager::StopCaptureForClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler,
    bool aborted_due_to_error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);

  DeviceEntry* entry = GetDeviceEntryByController(controller);
  if (!entry) {
    NOTREACHED();
    return;
  }
  if (!aborted_due_to_error) {
    if (controller->has_received_frames()) {
      LogVideoCaptureEvent(VIDEO_CAPTURE_STOP_CAPTURE_OK);
    } else if (entry->stream_type == MEDIA_DEVICE_VIDEO_CAPTURE) {
      LogVideoCaptureEvent(
          VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DEVICE);
    } else {
      LogVideoCaptureEvent(
          VIDEO_CAPTURE_STOP_CAPTURE_OK_NO_FRAMES_PRODUCED_BY_DESKTOP_OR_TAB);
    }
  } else {
    LogVideoCaptureEvent(VIDEO_CAPTURE_STOP_CAPTURE_DUE_TO_ERROR);
    for (auto it : sessions_) {
      if (it.second.type == entry->stream_type && it.second.id == entry->id) {
        listener_->Aborted(it.second.type, it.first);
        // Aborted() call might synchronously destroy |entry|, recheck.
        entry = GetDeviceEntryByController(controller);
        if (!entry)
          return;
        break;
      }
    }
  }

  // Detach client from controller.
  const media::VideoCaptureSessionId session_id =
      controller->RemoveClient(client_id, client_handler);
  DVLOG(1) << __func__ << ", session_id = " << session_id;

  // If controller has no more clients, delete controller and device.
  DestroyDeviceEntryIfNoClients(entry);
}

void VideoCaptureManager::PauseCaptureForClient(
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);
  DeviceEntry* entry = GetDeviceEntryByController(controller);
  if (!entry)
    NOTREACHED() << "Got Null entry while pausing capture";

  const bool had_active_client = controller->HasActiveClient();
  controller->PauseClient(client_id, client_handler);
  if (!had_active_client || controller->HasActiveClient())
    return;
  if (media::VideoCaptureDevice* device = entry->video_capture_device.get()) {
    device_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureDevice::MaybeSuspend,
                   // Unretained is safe to use here because |device| would be
                   // null if it was scheduled for shutdown and destruction, and
                   // because this task is guaranteed to run before the task
                   // that destroys the |device|.
                   base::Unretained(device)));
  }
}

void VideoCaptureManager::ResumeCaptureForClient(
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params,
    VideoCaptureController* controller,
    VideoCaptureControllerID client_id,
    VideoCaptureControllerEventHandler* client_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(controller);
  DCHECK(client_handler);

  DeviceEntry* entry = GetDeviceEntryByController(controller);
  if (!entry)
    NOTREACHED() << "Got Null entry while resuming capture";

  const bool had_active_client = controller->HasActiveClient();
  controller->ResumeClient(client_id, client_handler);
  if (had_active_client || !controller->HasActiveClient())
    return;
  if (media::VideoCaptureDevice* device = entry->video_capture_device.get()) {
    device_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureDevice::Resume,
                   // Unretained is safe to use here because |device| would be
                   // null if it was scheduled for shutdown and destruction, and
                   // because this task is guaranteed to run before the task
                   // that destroys the |device|.
                   base::Unretained(device)));
  }
}

void VideoCaptureManager::RequestRefreshFrameForClient(
    VideoCaptureController* controller) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (DeviceEntry* entry = GetDeviceEntryByController(controller)) {
    if (media::VideoCaptureDevice* device = entry->video_capture_device.get()) {
      device_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&VideoCaptureDevice::RequestRefreshFrame,
                     // Unretained is safe to use here because |device| would be
                     // null if it was scheduled for shutdown and destruction,
                     // and because this task is guaranteed to run before the
                     // task that destroys the |device|.
                     base::Unretained(device)));
    }
  }
}

bool VideoCaptureManager::GetDeviceSupportedFormats(
    media::VideoCaptureSessionId capture_session_id,
    media::VideoCaptureFormats* supported_formats) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(supported_formats->empty());

  SessionMap::iterator it = sessions_.find(capture_session_id);
  if (it == sessions_.end())
    return false;
  DVLOG(1) << "GetDeviceSupportedFormats for device: " << it->second.name;

  // Return all available formats of the device, regardless its started state.
  DeviceInfo* existing_device = GetDeviceInfoById(it->second.id);
  if (existing_device)
    *supported_formats = existing_device->supported_formats;
  return true;
}

bool VideoCaptureManager::GetDeviceFormatsInUse(
    media::VideoCaptureSessionId capture_session_id,
    media::VideoCaptureFormats* formats_in_use) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(formats_in_use->empty());

  SessionMap::iterator it = sessions_.find(capture_session_id);
  if (it == sessions_.end())
    return false;
  DVLOG(1) << "GetDeviceFormatsInUse for device: " << it->second.name;

  // Return the currently in-use format(s) of the device, if it's started.
  DeviceEntry* device_in_use =
      GetDeviceEntryByTypeAndId(it->second.type, it->second.id);
  if (device_in_use) {
    // Currently only one format-in-use is supported at the VCC level.
    formats_in_use->push_back(
        device_in_use->video_capture_controller.GetVideoCaptureFormat());
  }
  return true;
}

void VideoCaptureManager::SetDesktopCaptureWindowId(
    media::VideoCaptureSessionId session_id,
    gfx::NativeViewId window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(2) << "SetDesktopCaptureWindowId called for session " << session_id;

  notification_window_ids_[session_id] = window_id;
  MaybePostDesktopCaptureWindowId(session_id);
}

void VideoCaptureManager::MaybePostDesktopCaptureWindowId(
    media::VideoCaptureSessionId session_id) {
  SessionMap::iterator session_it = sessions_.find(session_id);
  if (session_it == sessions_.end())
    return;

  DeviceEntry* const existing_device =
      GetDeviceEntryByTypeAndId(session_it->second.type, session_it->second.id);
  if (!existing_device) {
    DVLOG(2) << "Failed to find an existing screen capture device.";
    return;
  }

  if (!existing_device->video_capture_device) {
    DVLOG(2) << "Screen capture device not yet started.";
    return;
  }

  DCHECK_EQ(MEDIA_DESKTOP_VIDEO_CAPTURE, existing_device->stream_type);
  DesktopMediaID id = DesktopMediaID::Parse(existing_device->id);
  if (id.is_null())
    return;

  auto window_id_it = notification_window_ids_.find(session_id);
  if (window_id_it == notification_window_ids_.end()) {
    DVLOG(2) << "Notification window id not set for screen capture.";
    return;
  }

  // Post |existing_device->video_capture_device| to the VideoCaptureDevice to
  // the device_task_runner_. This is safe since the device is destroyed on the
  // device_task_runner_.
  device_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureManager::SetDesktopCaptureWindowIdOnDeviceThread,
                 this, existing_device->video_capture_device.get(),
                 window_id_it->second));

  notification_window_ids_.erase(window_id_it);
}

void VideoCaptureManager::GetPhotoCapabilities(
    int session_id,
    VideoCaptureDevice::GetPhotoCapabilitiesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const DeviceEntry* entry = GetDeviceEntryBySessionId(session_id);
  if (!entry)
    return;
  VideoCaptureDevice* device = entry->video_capture_device.get();
  if (device) {
    VideoCaptureManager::DoGetPhotoCapabilities(std::move(callback), device);
    return;
  }
  // |entry| is known but |device| is nullptr, queue up a request for later.
  photo_request_queue_.emplace_back(
      session_id, base::Bind(&VideoCaptureManager::DoGetPhotoCapabilities, this,
                             base::Passed(&callback)));
}

void VideoCaptureManager::SetPhotoOptions(
    int session_id,
    media::mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const DeviceEntry* entry = GetDeviceEntryBySessionId(session_id);
  if (!entry)
    return;
  VideoCaptureDevice* device = entry->video_capture_device.get();
  if (device) {
    VideoCaptureManager::DoSetPhotoOptions(std::move(callback),
                                           std::move(settings), device);
    return;
  }
  // |entry| is known but |device| is nullptr, queue up a request for later.
  photo_request_queue_.emplace_back(
      session_id, base::Bind(&VideoCaptureManager::DoSetPhotoOptions, this,
                             base::Passed(&callback), base::Passed(&settings)));
}

void VideoCaptureManager::TakePhoto(
    int session_id,
    VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const DeviceEntry* entry = GetDeviceEntryBySessionId(session_id);
  if (!entry)
    return;
  VideoCaptureDevice* device = entry->video_capture_device.get();
  if (device) {
    VideoCaptureManager::DoTakePhoto(std::move(callback), device);
    return;
  }
  // |entry| is known but |device| is nullptr, queue up a request for later.
  photo_request_queue_.emplace_back(
      session_id, base::Bind(&VideoCaptureManager::DoTakePhoto, this,
                             base::Passed(&callback)));
}

void VideoCaptureManager::DoStopDeviceOnDeviceThread(
    std::unique_ptr<VideoCaptureDevice> device) {
  SCOPED_UMA_HISTOGRAM_TIMER("Media.VideoCaptureManager.StopDeviceTime");
  DCHECK(IsOnDeviceThread());
  device->StopAndDeAllocate();
  DVLOG(3) << "DoStopDeviceOnDeviceThread";
}

void VideoCaptureManager::OnOpened(
    MediaStreamType stream_type,
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Opened(stream_type, capture_session_id);
}

void VideoCaptureManager::OnClosed(
    MediaStreamType stream_type,
    media::VideoCaptureSessionId capture_session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!listener_) {
    // Listener has been removed.
    return;
  }
  listener_->Closed(stream_type, capture_session_id);
}

void VideoCaptureManager::OnDevicesInfoEnumerated(
    base::ElapsedTimer* timer,
    const EnumerationCallback& client_callback,
    const VideoCaptureManager::DeviceInfos& new_devices_info_cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_TIMES(
      "Media.VideoCaptureManager.GetAvailableDevicesInfoOnDeviceThreadTime",
      timer->Elapsed());
  devices_info_cache_ = new_devices_info_cache;

  // Walk the |devices_info_cache_| and produce a
  // media::VideoCaptureDeviceDescriptors for return purposes.
  media::VideoCaptureDeviceDescriptors devices;
  std::vector<std::tuple<media::VideoCaptureDeviceDescriptor,
                         media::VideoCaptureFormats>>
      descriptors_and_formats;
  for (const auto& it : devices_info_cache_) {
    devices.emplace_back(it.descriptor);
    descriptors_and_formats.emplace_back(it.descriptor, it.supported_formats);
    MediaInternals::GetInstance()->UpdateVideoCaptureDeviceCapabilities(
        descriptors_and_formats);
  }

  client_callback.Run(devices);
}

bool VideoCaptureManager::IsOnDeviceThread() const {
  return device_task_runner_->BelongsToCurrentThread();
}

void VideoCaptureManager::ConsolidateDevicesInfoOnDeviceThread(
    base::Callback<void(const VideoCaptureManager::DeviceInfos&)>
        on_devices_enumerated_callback,
    const VideoCaptureManager::DeviceInfos& old_device_info_cache,
    std::unique_ptr<VideoCaptureDeviceDescriptors> descriptors_snapshot) {
  DCHECK(IsOnDeviceThread());
  // Construct |new_devices_info_cache| with the cached devices that are still
  // present in the system, and remove their names from |names_snapshot|, so we
  // keep there the truly new devices.
  VideoCaptureManager::DeviceInfos new_devices_info_cache;
  for (const auto& device_info : old_device_info_cache) {
    for (VideoCaptureDeviceDescriptors::iterator it =
             descriptors_snapshot->begin();
         it != descriptors_snapshot->end(); ++it) {
      if (device_info.descriptor.device_id == it->device_id) {
        new_devices_info_cache.push_back(device_info);
        descriptors_snapshot->erase(it);
        break;
      }
    }
  }

  // Get the device info for the new devices in |names_snapshot|.
  for (const auto& it : *descriptors_snapshot) {
    DeviceInfo device_info(it);
    video_capture_device_factory_->GetSupportedFormats(
        it, &device_info.supported_formats);
    ConsolidateCaptureFormats(&device_info.supported_formats);
    new_devices_info_cache.push_back(device_info);
  }

  on_devices_enumerated_callback.Run(new_devices_info_cache);
}

void VideoCaptureManager::DestroyDeviceEntryIfNoClients(DeviceEntry* entry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Removal of the last client stops the device.
  if (!entry->video_capture_controller.HasActiveClient() &&
      !entry->video_capture_controller.HasPausedClient()) {
    DVLOG(1) << "VideoCaptureManager stopping device (type = "
             << entry->stream_type << ", id = " << entry->id << ")";

    // The DeviceEntry is removed from |devices_| immediately. The controller is
    // deleted immediately, and the device is freed asynchronously. After this
    // point, subsequent requests to open this same device ID will create a new
    // DeviceEntry, VideoCaptureController, and VideoCaptureDevice.
    DoStopDevice(entry);
    // TODO(mcasas): use a helper function https://crbug.com/624854.
    DeviceEntries::iterator device_it =
        std::find_if(devices_.begin(), devices_.end(),
                     [entry](const std::unique_ptr<DeviceEntry>& device_entry) {
                       return device_entry.get() == entry;
                     });
    devices_.erase(device_it);
  }
}

VideoCaptureManager::DeviceEntry*
VideoCaptureManager::GetDeviceEntryBySessionId(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  SessionMap::const_iterator session_it = sessions_.find(session_id);
  if (session_it == sessions_.end())
    return nullptr;

  return GetDeviceEntryByTypeAndId(session_it->second.type,
                                   session_it->second.id);
}

VideoCaptureManager::DeviceEntry*
VideoCaptureManager::GetDeviceEntryByTypeAndId(
    MediaStreamType type,
    const std::string& device_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const std::unique_ptr<DeviceEntry>& device : devices_) {
    if (type == device->stream_type && device_id == device->id)
      return device.get();
  }
  return nullptr;
}

VideoCaptureManager::DeviceEntry*
VideoCaptureManager::GetDeviceEntryByController(
    const VideoCaptureController* controller) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Look up |controller| in |devices_|.
  for (const std::unique_ptr<DeviceEntry>& device : devices_) {
    if (&device->video_capture_controller == controller)
      return device.get();
  }
  return nullptr;
}

VideoCaptureManager::DeviceEntry* VideoCaptureManager::GetDeviceEntryBySerialId(
    int serial_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const std::unique_ptr<DeviceEntry>& device : devices_) {
    if (device->serial_id == serial_id)
      return device.get();
  }
  return nullptr;
}

VideoCaptureManager::DeviceInfo* VideoCaptureManager::GetDeviceInfoById(
    const std::string& id) {
  for (auto& it : devices_info_cache_) {
    if (it.descriptor.device_id == id)
      return &it;
  }
  return nullptr;
}

VideoCaptureManager::DeviceEntry* VideoCaptureManager::GetOrCreateDeviceEntry(
    media::VideoCaptureSessionId capture_session_id,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SessionMap::iterator session_it = sessions_.find(capture_session_id);
  if (session_it == sessions_.end())
    return nullptr;
  const MediaStreamDevice& device_info = session_it->second;

  // Check if another session has already opened this device. If so, just
  // use that opened device.
  DeviceEntry* const existing_device =
      GetDeviceEntryByTypeAndId(device_info.type, device_info.id);
  if (existing_device) {
    DCHECK_EQ(device_info.type, existing_device->stream_type);
    return existing_device;
  }

  devices_.emplace_back(
      new DeviceEntry(device_info.type, device_info.id, params));
  return devices_.back().get();
}

void VideoCaptureManager::SetDesktopCaptureWindowIdOnDeviceThread(
    media::VideoCaptureDevice* device,
    gfx::NativeViewId window_id) {
  DCHECK(IsOnDeviceThread());
#if defined(ENABLE_SCREEN_CAPTURE) && BUILDFLAG(ENABLE_WEBRTC) && !defined(OS_ANDROID)
  DesktopCaptureDevice* desktop_device =
      static_cast<DesktopCaptureDevice*>(device);
  desktop_device->SetNotificationWindowId(window_id);
  VLOG(2) << "Screen capture notification window passed on device thread.";
#endif
}

void VideoCaptureManager::DoGetPhotoCapabilities(
    VideoCaptureDevice::GetPhotoCapabilitiesCallback callback,
    VideoCaptureDevice* device) {
  // Unretained() is safe to use here because |device| would be null if it
  // was scheduled for shutdown and destruction, and because this task is
  // guaranteed to run before the task that destroys the |device|.
  device_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoCaptureDevice::GetPhotoCapabilities,
                            base::Unretained(device), base::Passed(&callback)));
}

void VideoCaptureManager::DoSetPhotoOptions(
    VideoCaptureDevice::SetPhotoOptionsCallback callback,
    media::mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice* device) {
  // Unretained() is safe to use here because |device| would be null if it
  // was scheduled for shutdown and destruction, and because this task is
  // guaranteed to run before the task that destroys the |device|.
  device_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VideoCaptureDevice::SetPhotoOptions, base::Unretained(device),
                 base::Passed(&settings), base::Passed(&callback)));
}

void VideoCaptureManager::DoTakePhoto(
    VideoCaptureDevice::TakePhotoCallback callback,
    VideoCaptureDevice* device) {
  // Unretained() is safe to use here because |device| would be null if it
  // was scheduled for shutdown and destruction, and because this task is
  // guaranteed to run before the task that destroys the |device|.
  device_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VideoCaptureDevice::TakePhoto,
                            base::Unretained(device), base::Passed(&callback)));
}

#if defined(OS_ANDROID)
void VideoCaptureManager::OnApplicationStateChange(
    base::android::ApplicationState state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Only release/resume devices when the Application state changes from
  // RUNNING->STOPPED->RUNNING.
  if (state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES &&
      !application_state_has_running_activities_) {
    ResumeDevices();
    application_state_has_running_activities_ = true;
  } else if (state == base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES) {
    ReleaseDevices();
    application_state_has_running_activities_ = false;
  }
}

void VideoCaptureManager::ReleaseDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (auto& entry : devices_) {
    // Do not stop Content Video Capture devices, e.g. Tab or Screen capture.
    if (entry->stream_type != MEDIA_DEVICE_VIDEO_CAPTURE)
      continue;

    DoStopDevice(entry.get());
  }
}

void VideoCaptureManager::ResumeDevices() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (auto& entry : devices_) {
    // Do not resume Content Video Capture devices, e.g. Tab or Screen capture.
    // Do not try to restart already running devices.
    if (entry->stream_type != MEDIA_DEVICE_VIDEO_CAPTURE ||
        entry->video_capture_device)
      continue;

    // Check if the device is already in the start queue.
    bool device_in_queue = false;
    for (auto& request : device_start_queue_) {
      if (request.serial_id() == entry->serial_id) {
        device_in_queue = true;
        break;
      }
    }

    if (!device_in_queue) {
      // Session ID is only valid for Screen capture. So we can fake it to
      // resume video capture devices here.
      QueueStartDevice(kFakeSessionId, entry.get(), entry->parameters);
    }
  }
}
#endif  // defined(OS_ANDROID)

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_CONTENT_VIDEO_CAPTURE_DEVICE_CORE_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_CONTENT_VIDEO_CAPTURE_DEVICE_CORE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "content/browser/media/capture/capture_resolution_chooser.h"
#include "content/browser/media/capture/video_capture_oracle.h"
#include "content/common/content_export.h"
#include "media/base/video_frame.h"
#include "media/video/capture/video_capture_device.h"

namespace media {
class VideoCaptureParams;
class VideoFrame;
}  // namespace media

namespace content {

class VideoCaptureMachine;

// Thread-safe, refcounted proxy to the VideoCaptureOracle.  This proxy wraps
// the VideoCaptureOracle, which decides which frames to capture, and a
// VideoCaptureDevice::Client, which allocates and receives the captured
// frames, in a lock to synchronize state between the two.
class ThreadSafeCaptureOracle
    : public base::RefCountedThreadSafe<ThreadSafeCaptureOracle> {
 public:
  ThreadSafeCaptureOracle(scoped_ptr<media::VideoCaptureDevice::Client> client,
                          const media::VideoCaptureParams& params);

  // Called when a captured frame is available or an error has occurred.
  // If |success| is true then |frame| is valid and |timestamp| indicates when
  // the frame was painted.
  // If |success| is false, all other parameters are invalid.
  typedef base::Callback<void(const scoped_refptr<media::VideoFrame>& frame,
                              base::TimeTicks timestamp,
                              bool success)> CaptureFrameCallback;

  bool ObserveEventAndDecideCapture(VideoCaptureOracle::Event event,
                                    const gfx::Rect& damage_rect,
                                    base::TimeTicks event_time,
                                    scoped_refptr<media::VideoFrame>* storage,
                                    CaptureFrameCallback* callback);

  base::TimeDelta min_capture_period() const {
    return oracle_.min_capture_period();
  }

  gfx::Size max_frame_size() const {
    return params_.requested_format.frame_size;
  }

  // Returns the current capture resolution.
  gfx::Size GetCaptureSize() const;

  // Updates capture resolution based on the supplied source size and the
  // maximum frame size.
  void UpdateCaptureSize(const gfx::Size& source_size);

  // Stop new captures from happening (but doesn't forget the client).
  void Stop();

  // Signal an error to the client.
  void ReportError(const std::string& reason);

 private:
  friend class base::RefCountedThreadSafe<ThreadSafeCaptureOracle>;
  virtual ~ThreadSafeCaptureOracle();

  // Callback invoked on completion of all captures.
  void DidCaptureFrame(
      int frame_number,
      scoped_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
      base::TimeTicks capture_begin_time,
      const scoped_refptr<media::VideoFrame>& frame,
      base::TimeTicks timestamp,
      bool success);

  // Protects everything below it.
  mutable base::Lock lock_;

  // Recipient of our capture activity.
  scoped_ptr<media::VideoCaptureDevice::Client> client_;

  // Makes the decision to capture a frame.
  VideoCaptureOracle oracle_;

  // The video capture parameters used to construct the oracle proxy.
  const media::VideoCaptureParams params_;

  // Determines video capture frame sizes.
  CaptureResolutionChooser resolution_chooser_;
};

// Keeps track of the video capture source frames and executes copying on the
// UI BrowserThread.
class VideoCaptureMachine {
 public:
  VideoCaptureMachine() {}
  virtual ~VideoCaptureMachine() {}

  // Starts capturing. Returns true if succeeded.
  // Must be run on the UI BrowserThread.
  virtual bool Start(const scoped_refptr<ThreadSafeCaptureOracle>& oracle_proxy,
                     const media::VideoCaptureParams& params) = 0;

  // Stops capturing. Must be run on the UI BrowserThread.
  // |callback| is invoked after the capturing has stopped.
  virtual void Stop(const base::Closure& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureMachine);
};

// The "meat" of a content video capturer.
//
// Separating this from the "shell classes" WebContentsVideoCaptureDevice and
// DesktopCaptureDeviceAura allows safe destruction without needing to block any
// threads, as well as code sharing.
//
// ContentVideoCaptureDeviceCore manages a simple state machine and the pipeline
// (see notes at top of this file).  It times the start of successive captures
// and facilitates the processing of each through the stages of the
// pipeline.
class CONTENT_EXPORT ContentVideoCaptureDeviceCore
    : public base::SupportsWeakPtr<ContentVideoCaptureDeviceCore> {
 public:
  ContentVideoCaptureDeviceCore(
      scoped_ptr<VideoCaptureMachine> capture_machine);
  virtual ~ContentVideoCaptureDeviceCore();

  // Asynchronous requests to change ContentVideoCaptureDeviceCore state.
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        scoped_ptr<media::VideoCaptureDevice::Client> client);
  void StopAndDeAllocate();

 private:
  // Flag indicating current state.
  enum State {
    kIdle,
    kCapturing,
    kError
  };

  void TransitionStateTo(State next_state);

  // Called back in response to StartCaptureMachine().  |success| is true if
  // capture machine succeeded to start.
  void CaptureStarted(bool success);

  // Stops capturing and notifies client_ of an error state.
  void Error(const std::string& reason);

  // Tracks that all activity occurs on the media stream manager's thread.
  base::ThreadChecker thread_checker_;

  // Current lifecycle state.
  State state_;

  // Tracks the CaptureMachine that's doing work on our behalf on the UI thread.
  // This value should never be dereferenced by this class, other than to
  // create and destroy it on the UI thread.
  scoped_ptr<VideoCaptureMachine> capture_machine_;

  // Our thread-safe capture oracle which serves as the gateway to the video
  // capture pipeline. Besides the VideoCaptureDevice itself, it is the only
  // component of the system with direct access to |client_|.
  scoped_refptr<ThreadSafeCaptureOracle> oracle_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ContentVideoCaptureDeviceCore);
};


}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_CONTENT_VIDEO_CAPTURE_DEVICE_CORE_H_

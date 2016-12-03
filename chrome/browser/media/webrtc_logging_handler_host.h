// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "chrome/browser/media/rtp_dump_type.h"
#include "chrome/browser/media/webrtc_rtp_dump_handler.h"
#include "chrome/common/media/webrtc_logging_message_data.h"
#include "chrome/common/partial_circular_buffer.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_process_host.h"
#include "net/base/network_interfaces.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

class Profile;
class WebRtcLogUploader;

#if defined(OS_ANDROID)
const size_t kWebRtcLogSize = 1 * 1024 * 1024;  // 1 MB
#else
const size_t kWebRtcLogSize = 6 * 1024 * 1024;  // 6 MB
#endif

typedef std::map<std::string, std::string> MetaDataMap;

struct WebRtcLogPaths {
  base::FilePath log_path;  // todo: rename to directory.
  base::FilePath incoming_rtp_dump;
  base::FilePath outgoing_rtp_dump;
};

class WebRtcLogBuffer {
 public:
  WebRtcLogBuffer();
  ~WebRtcLogBuffer();

  void Log(const std::string& message);

  // Returns a circular buffer instance for reading the internal log buffer.
  // Must only be called after the log has been marked as complete
  // (see SetComplete) and the caller must ensure that the WebRtcLogBuffer
  // instance remains in scope for the lifetime of the returned circular buffer.
  PartialCircularBuffer Read();

  // Switches the buffer to read-only mode, where access to the internal
  // buffer is allowed from different threads than were used to contribute
  // to the log.  Calls to Log() won't be allowed after calling
  // SetComplete() and the call to SetComplete() must be done on the same
  // thread as constructed the buffer and calls Log().
  void SetComplete();

 private:
  base::ThreadChecker thread_checker_;
  uint8_t buffer_[kWebRtcLogSize];
  PartialCircularBuffer circular_;
  bool read_only_;
};

// WebRtcLoggingHandlerHost handles operations regarding the WebRTC logging:
// - Opens a shared memory buffer that the handler in the render process
//   writes to.
// - Writes basic machine info to the log.
// - Informs the handler in the render process when to stop logging.
// - Closes the shared memory (and thereby discarding it) or triggers uploading
//   of the log.
// - Detects when channel, i.e. renderer, is going away and possibly triggers
//   uploading the log.
class WebRtcLoggingHandlerHost : public content::BrowserMessageFilter {
 public:
  typedef base::Callback<void(bool, const std::string&)> GenericDoneCallback;
  typedef base::Callback<void(bool, const std::string&, const std::string&)>
      UploadDoneCallback;

  // Key used to attach the handler to the RenderProcessHost.
  static const char kWebRtcLoggingHandlerHostKey[];

  WebRtcLoggingHandlerHost(int render_process_id,
                           Profile* profile,
                           WebRtcLogUploader* log_uploader);

  // Sets meta data that will be uploaded along with the log and also written
  // in the beginning of the log. Must be called on the IO thread before calling
  // StartLogging.
  void SetMetaData(std::unique_ptr<MetaDataMap> meta_data,
                   const GenericDoneCallback& callback);

  // Opens a log and starts logging. Must be called on the IO thread.
  void StartLogging(const GenericDoneCallback& callback);

  // Stops logging. Log will remain open until UploadLog or DiscardLog is
  // called. Must be called on the IO thread.
  void StopLogging(const GenericDoneCallback& callback);

  // Uploads the log and the RTP dumps. Discards the local copy. May only be
  // called after logging has stopped. Must be called on the IO thread.
  void UploadLog(const UploadDoneCallback& callback);

  // Uploads a log that was previously saved via a call to StoreLog().
  // Otherwise operates in the same way as UploadLog.
  void UploadStoredLog(const std::string& log_id,
                       const UploadDoneCallback& callback);

  // Called by WebRtcLogUploader when uploading has finished. Must be called on
  // the IO thread.
  void UploadLogDone();

  // Discards the log and the RTP dumps. May only be called after logging has
  // stopped. Must be called on the IO thread.
  void DiscardLog(const GenericDoneCallback& callback);

  // Stores the log locally using a hash of log_id + security origin.
  void StoreLog(const std::string& log_id, const GenericDoneCallback& callback);

  // Adds a message to the log.
  // This method must be called on the IO thread.
  void LogMessage(const std::string& message);

  // May be called on any thread. |upload_log_on_render_close_| is used
  // for decision making and it's OK if it changes before the execution based
  // on that decision has finished.
  void set_upload_log_on_render_close(bool should_upload) {
    upload_log_on_render_close_ = should_upload;
  }

  // Starts dumping the RTP headers for the specified direction. Must be called
  // on the IO thread. |type| specifies which direction(s) of RTP packets should
  // be dumped. |callback| will be called when starting the dump is done.
  // |stop_callback| will be called when StopRtpDump is called.
  void StartRtpDump(RtpDumpType type,
                    const GenericDoneCallback& callback,
                    const content::RenderProcessHost::WebRtcStopRtpDumpCallback&
                        stop_callback);

  // Stops dumping the RTP headers for the specified direction. Must be called
  // on the IO thread. |type| specifies which direction(s) of RTP packet dumping
  // should be stopped. |callback| will be called when stopping the dump is
  // done.
  void StopRtpDump(RtpDumpType type, const GenericDoneCallback& callback);

  // Called when an RTP packet is sent or received. Must be called on the UI
  // thread.
  void OnRtpPacket(std::unique_ptr<uint8_t[]> packet_header,
                   size_t header_length,
                   size_t packet_length,
                   bool incoming);

 private:
  // States used for protecting from function calls made at non-allowed points
  // in time. For example, StartLogging() is only allowed in CLOSED state.
  // Transitions: SetMetaData(): CLOSED -> CLOSED.
  //              StartLogging(): CLOSED -> STARTING.
  //              Start done: STARTING -> STARTED.
  //              StopLogging(): STARTED -> STOPPING.
  //              Stop done: STOPPING -> STOPPED.
  //              UploadLog(): STOPPED -> UPLOADING.
  //              Upload done: UPLOADING -> CLOSED.
  //              DiscardLog(): STOPPED -> CLOSED.
  enum LoggingState {
    CLOSED,    // Logging not started, no log in memory.
    STARTING,  // Start logging is in progress.
    STARTED,   // Logging started.
    STOPPING,  // Stop logging is in progress.
    STOPPED,   // Logging has been stopped, log still open in memory.
  };

  friend class content::BrowserThread;
  friend class base::DeleteHelper<WebRtcLoggingHandlerHost>;

  ~WebRtcLoggingHandlerHost() override;

  // BrowserMessageFilter implementation.
  void OnChannelClosing() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Handles log message requests from renderer process.
  void OnAddLogMessages(const std::vector<WebRtcLoggingMessageData>& messages);
  void OnLoggingStoppedInRenderer();

  void LogInitialInfoOnFileThread(const GenericDoneCallback& callback);
  void LogInitialInfoOnIOThread(const net::NetworkInterfaceList& network_list,
                                const GenericDoneCallback& callback);

  void EnableBrowserProcessLoggingOnUIThread();
  void DisableBrowserProcessLoggingOnUIThread();

  // Called after stopping RTP dumps.
  void StoreLogContinue(const std::string& log_id,
      const GenericDoneCallback& callback);

  // Writes a formatted log |message| to the |circular_buffer_|.
  void LogToCircularBuffer(const std::string& message);

  // Gets the log directory path for |profile_| and ensure it exists. Must be
  // called on the FILE thread.
  base::FilePath GetLogDirectoryAndEnsureExists();

  void TriggerUpload(const UploadDoneCallback& callback,
                     const base::FilePath& log_directory);

  void StoreLogInDirectory(const std::string& log_id,
                           std::unique_ptr<WebRtcLogPaths> log_paths,
                           const GenericDoneCallback& done_callback,
                           const base::FilePath& directory);

  void UploadStoredLogOnFileThread(const std::string& log_id,
                                   const UploadDoneCallback& callback);

  // A helper for TriggerUpload to do the real work.
  void DoUploadLogAndRtpDumps(const base::FilePath& log_directory,
                              const UploadDoneCallback& callback);

  // Create the RTP dump handler and start dumping. Must be called after making
  // sure the log directory exists.
  void CreateRtpDumpHandlerAndStart(RtpDumpType type,
                                    const GenericDoneCallback& callback,
                                    const base::FilePath& dump_dir);

  // A helper for starting RTP dump assuming the RTP dump handler has been
  // created.
  void DoStartRtpDump(RtpDumpType type, const GenericDoneCallback& callback);

  // Adds the packet to the dump on IO thread.
  void DumpRtpPacketOnIOThread(std::unique_ptr<uint8_t[]> packet_header,
                               size_t header_length,
                               size_t packet_length,
                               bool incoming);

  bool ReleaseRtpDumps(WebRtcLogPaths* log_paths);

  void FireGenericDoneCallback(
      const WebRtcLoggingHandlerHost::GenericDoneCallback& callback,
      bool success,
      const std::string& error_message);

  std::unique_ptr<WebRtcLogBuffer> log_buffer_;

  // The profile associated with our renderer process.
  Profile* const profile_;

  // These are only accessed on the IO thread, except when in STARTING state. In
  // this state we are protected since entering any function that alters the
  // state is not allowed.
  std::unique_ptr<MetaDataMap> meta_data_;

  // These are only accessed on the IO thread.
  GenericDoneCallback stop_callback_;

  // Only accessed on the IO thread, except when in STARTING, STOPPING or
  // UPLOADING state if the action fails and the state must be reset. In these
  // states however, we are protected since entering any function that alters
  // the state is not allowed.
  LoggingState logging_state_;

  // Only accessed on the IO thread.
  bool upload_log_on_render_close_;

  // This is the handle to be passed to the render process. It's stored so that
  // it doesn't have to be passed on when posting messages between threads.
  // It's only accessed on the IO thread.
  base::SharedMemoryHandle foreign_memory_handle_;

  // The system time in ms when logging is started. Reset when logging_state_
  // changes to STOPPED.
  base::Time logging_started_time_;

  // The RTP dump handler responsible for creating the RTP header dump files.
  std::unique_ptr<WebRtcRtpDumpHandler> rtp_dump_handler_;

  // The callback to call when StopRtpDump is called.
  content::RenderProcessHost::WebRtcStopRtpDumpCallback stop_rtp_dump_callback_;

  // A pointer to the log uploader that's shared for all profiles.
  // Ownership lies with the browser process.
  WebRtcLogUploader* const log_uploader_;

  // The render process ID this object belongs to.
  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLoggingHandlerHost);
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_LOGGING_HANDLER_HOST_H_

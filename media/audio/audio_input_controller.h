// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_
#define MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/atomicops.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"

// An AudioInputController controls an AudioInputStream and records data
// from this input stream. The two main methods are Record() and Close() and
// they are both executed on the audio thread which is injected by the two
// alternative factory methods, Create() or CreateLowLatency().
//
// All public methods of AudioInputController are non-blocking.
//
// Here is a state diagram for the AudioInputController:
//
//                    .-->  [ Closed / Error ]  <--.
//                    |                            |
//                    |                            |
//               [ Created ]  ---------->  [ Recording ]
//                    ^
//                    |
//              *[  Empty  ]
//
// * Initial state
//
// State sequences (assuming low-latency):
//
//  [Creating Thread]                     [Audio Thread]
//
//      User               AudioInputController               EventHandler
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// CrateLowLatency() ==>        DoCreate()
//                   AudioManager::MakeAudioInputStream()
//                        AudioInputStream::Open()
//                                  .- - - - - - - - - - - - ->   OnError()
//                                  .------------------------->  OnCreated()
//                               kCreated
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Record() ==>                 DoRecord()
//                      AudioInputStream::Start()
//                                  .------------------------->  OnRecording()
//                              kRecording
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Close() ==>                  DoClose()
//                           state_ = kClosed
//                        AudioInputStream::Stop()
//                        AudioInputStream::Close()
//                          SyncWriter::Close()
// Closure::Run() <-----------------.
// (closure-task)
//
// The audio thread itself is owned by the AudioManager that the
// AudioInputController holds a reference to.  When performing tasks on the
// audio thread, the controller must not add or release references to the
// AudioManager or itself (since it in turn holds a reference to the manager).
//
namespace media {

// Only do power monitoring for non-mobile platforms to save resources.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
#define AUDIO_POWER_MONITORING
#endif

class AudioInputWriter;
class UserInputMonitor;

class MEDIA_EXPORT AudioInputController
    : public base::RefCountedThreadSafe<AudioInputController>,
      public AudioInputStream::AudioInputCallback {
 public:
  // Error codes to make native logging more clear. These error codes are added
  // to generic error strings to provide a higher degree of details.
  // Changing these values can lead to problems when matching native debug
  // logs with the actual cause of error.
  enum ErrorCode {
    // An unspecified error occured.
    UNKNOWN_ERROR = 0,

    // Failed to create an audio input stream.
    STREAM_CREATE_ERROR,  // = 1

    // Failed to open an audio input stream.
    STREAM_OPEN_ERROR,  // = 2

    // Native input stream reports an error. Exact reason differs between
    // platforms.
    STREAM_ERROR,  // = 3
  };

  // An event handler that receives events from the AudioInputController. The
  // following methods are all called on the audio thread.
  class MEDIA_EXPORT EventHandler {
   public:
    virtual void OnCreated(AudioInputController* controller) = 0;
    virtual void OnRecording(AudioInputController* controller) = 0;
    virtual void OnError(AudioInputController* controller,
                         ErrorCode error_code) = 0;
    virtual void OnData(AudioInputController* controller,
                        const AudioBus* data) = 0;
    virtual void OnLog(AudioInputController* controller,
                       const std::string& message) = 0;

   protected:
    virtual ~EventHandler() {}
  };

  // A synchronous writer interface used by AudioInputController for
  // synchronous writing.
  class MEDIA_EXPORT SyncWriter {
   public:
    virtual ~SyncWriter() {}

    // Write certain amount of data from |data|.
    virtual void Write(const AudioBus* data,
                       double volume,
                       bool key_pressed,
                       uint32_t hardware_delay_bytes) = 0;

    // Close this synchronous writer.
    virtual void Close() = 0;
  };

  // AudioInputController::Create() can use the currently registered Factory
  // to create the AudioInputController. Factory is intended for testing only.
  // |user_input_monitor| is used for typing detection and can be NULL.
  class Factory {
   public:
    virtual AudioInputController* Create(
        AudioManager* audio_manager,
        EventHandler* event_handler,
        AudioParameters params,
        UserInputMonitor* user_input_monitor) = 0;

   protected:
    virtual ~Factory() {}
  };

  // Factory method for creating an AudioInputController.
  // The audio device will be created on the audio thread, and when that is
  // done, the event handler will receive an OnCreated() call from that same
  // thread. |device_id| is the unique ID of the audio device to be opened.
  // |user_input_monitor| is used for typing detection and can be NULL.
  static scoped_refptr<AudioInputController> Create(
      AudioManager* audio_manager,
      EventHandler* event_handler,
      const AudioParameters& params,
      const std::string& device_id,
      UserInputMonitor* user_input_monitor);

  // Sets the factory used by the static method Create(). AudioInputController
  // does not take ownership of |factory|. A value of NULL results in an
  // AudioInputController being created directly.
  static void set_factory_for_testing(Factory* factory) { factory_ = factory; }
  AudioInputStream* stream_for_testing() { return stream_; }

  // Factory method for creating an AudioInputController for low-latency mode.
  // The audio device will be created on the audio thread, and when that is
  // done, the event handler will receive an OnCreated() call from that same
  // thread. |user_input_monitor| is used for typing detection and can be NULL.
  static scoped_refptr<AudioInputController> CreateLowLatency(
      AudioManager* audio_manager,
      EventHandler* event_handler,
      const AudioParameters& params,
      const std::string& device_id,
      // External synchronous writer for audio controller.
      SyncWriter* sync_writer,
      UserInputMonitor* user_input_monitor,
      const bool agc_is_enabled);

  // Factory method for creating an AudioInputController with an existing
  // |stream| for low-latency mode, taking ownership of |stream|. The stream
  // will be opened on the audio thread, and when that is done, the event
  // handler will receive an OnCreated() call from that same thread.
  // |user_input_monitor| is used for typing detection and can be NULL.
  static scoped_refptr<AudioInputController> CreateForStream(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      EventHandler* event_handler,
      AudioInputStream* stream,
      // External synchronous writer for audio controller.
      SyncWriter* sync_writer,
      UserInputMonitor* user_input_monitor);

  // Starts recording using the created audio input stream.
  // This method is called on the creator thread.
  virtual void Record();

  // Closes the audio input stream. The state is changed and the resources
  // are freed on the audio thread. |closed_task| is then executed on the thread
  // that called Close().
  // Callbacks (EventHandler and SyncWriter) must exist until |closed_task|
  // is called.
  // It is safe to call this method more than once. Calls after the first one
  // will have no effect.
  // This method trampolines to the audio thread.
  virtual void Close(const base::Closure& closed_task);

  // Sets the capture volume of the input stream. The value 0.0 corresponds
  // to muted and 1.0 to maximum volume.
  virtual void SetVolume(double volume);

  // AudioInputCallback implementation. Threading details depends on the
  // device-specific implementation.
  void OnData(AudioInputStream* stream,
              const AudioBus* source,
              uint32_t hardware_delay_bytes,
              double volume) override;
  void OnError(AudioInputStream* stream) override;

  bool SharedMemoryAndSyncSocketMode() const { return sync_writer_ != NULL; }

  // Enable debug recording of audio input.
  void EnableDebugRecording(AudioInputWriter* input_writer);

  // Disbale debug recording of audio input. Must be called before owner of
  // |input_writer| deletes it.
  void DisableDebugRecording(const base::Closure& callback);

 protected:
  friend class base::RefCountedThreadSafe<AudioInputController>;

  // Used to log the result of capture startup.
  // This was previously logged as a boolean with only the no callback and OK
  // options. The enum order is kept to ensure backwards compatibility.
  // Elements in this enum should not be deleted or rearranged; the only
  // permitted operation is to add new elements before
  // CAPTURE_STARTUP_RESULT_MAX and update CAPTURE_STARTUP_RESULT_MAX.
  //
  // The NO_DATA_CALLBACK enum has been replaced with NEVER_GOT_DATA,
  // and there are also other histograms such as
  // Media.Audio.InputStartupSuccessMac to cover issues similar
  // to the ones the NO_DATA_CALLBACK was intended for.
  enum CaptureStartupResult {
    CAPTURE_STARTUP_NO_DATA_CALLBACK = 0,  // no longer used.
    CAPTURE_STARTUP_OK = 1,
    CAPTURE_STARTUP_CREATE_STREAM_FAILED = 2,
    CAPTURE_STARTUP_OPEN_STREAM_FAILED = 3,
    CAPTURE_STARTUP_NEVER_GOT_DATA = 4,
    CAPTURE_STARTUP_RESULT_MAX = CAPTURE_STARTUP_NEVER_GOT_DATA
  };

  // Internal state of the source.
  enum State {
    CREATED,
    RECORDING,
    CLOSED
  };

#if defined(AUDIO_POWER_MONITORING)
  // Used to log a silence report (see OnData).
  // Elements in this enum should not be deleted or rearranged; the only
  // permitted operation is to add new elements before SILENCE_STATE_MAX and
  // update SILENCE_STATE_MAX.
  // Possible silence state transitions:
  //           SILENCE_STATE_AUDIO_AND_SILENCE
  //               ^                  ^
  // SILENCE_STATE_ONLY_AUDIO   SILENCE_STATE_ONLY_SILENCE
  //               ^                  ^
  //            SILENCE_STATE_NO_MEASUREMENT
  enum SilenceState {
    SILENCE_STATE_NO_MEASUREMENT = 0,
    SILENCE_STATE_ONLY_AUDIO = 1,
    SILENCE_STATE_ONLY_SILENCE = 2,
    SILENCE_STATE_AUDIO_AND_SILENCE = 3,
    SILENCE_STATE_MAX = SILENCE_STATE_AUDIO_AND_SILENCE
  };
#endif

  AudioInputController(EventHandler* handler,
                       SyncWriter* sync_writer,
                       UserInputMonitor* user_input_monitor,
                       const bool agc_is_enabled);
  ~AudioInputController() override;

  // Methods called on the audio thread (owned by the AudioManager).
  void DoCreate(AudioManager* audio_manager,
                const AudioParameters& params,
                const std::string& device_id);
  void DoCreateForLowLatency(AudioManager* audio_manager,
                             const AudioParameters& params,
                             const std::string& device_id);
  void DoCreateForStream(AudioInputStream* stream_to_control);
  void DoRecord();
  void DoClose();
  void DoReportError();
  void DoSetVolume(double volume);
  void DoOnData(std::unique_ptr<AudioBus> data);
  void DoLogAudioLevels(float level_dbfs, int microphone_volume_percent);

  // Helper method that stops, closes, and NULL:s |*stream_|.
  void DoStopCloseAndClearStream();

#if defined(AUDIO_POWER_MONITORING)
  // Updates the silence state, see enum SilenceState above for state
  // transitions.
  void UpdateSilenceState(bool silence);

  // Logs the silence state as UMA stat.
  void LogSilenceState(SilenceState value);
#endif

  // Logs the result of creating an AudioInputController.
  // Only logs once, even if it is called several times.
  void LogCaptureStartupResult(CaptureStartupResult result);

  // Enable and disable debug recording of audio input. Called on the audio
  // thread.
  void DoEnableDebugRecording(AudioInputWriter* input_writer);
  void DoDisableDebugRecording();

  // Called on the audio thread.
  void WriteInputDataForDebugging(std::unique_ptr<AudioBus> data);

  // Called by the stream with log messages.
  void LogMessage(const std::string& message);

  // Gives access to the task runner of the creating thread.
  scoped_refptr<base::SingleThreadTaskRunner> creator_task_runner_;

  // The task runner of audio-manager thread that this object runs on.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Contains the AudioInputController::EventHandler which receives state
  // notifications from this class.
  EventHandler* handler_;

  // Pointer to the audio input stream object.
  AudioInputStream* stream_;

  // Flag for whether CaptureStartupResults shall be reported.
  // A value of 1 means that stats shall be reported,
  // any other value means that stats have already been reported.
  base::AtomicRefCount should_report_stats;

  // |state_| is written on the audio thread and is read on the hardware audio
  // thread. These operations need to be locked. But lock is not required for
  // reading on the audio input controller thread.
  State state_;

  base::Lock lock_;

  // SyncWriter is used only in low-latency mode for synchronous writing.
  SyncWriter* sync_writer_;

  static Factory* factory_;

  double max_volume_;

  UserInputMonitor* user_input_monitor_;

  const bool agc_is_enabled_;

#if defined(AUDIO_POWER_MONITORING)
  // Enabled in DoCrete() but not in DoCreateForStream().
  bool power_measurement_is_enabled_;

  // Updated each time a power measurement is performed.
  base::TimeTicks last_audio_level_log_time_;

  // Whether the silence state should sent as UMA stat.
  bool log_silence_state_;

  // The silence report sent as UMA stat at the end of a session.
  SilenceState silence_state_;
#endif

  size_t prev_key_down_count_;

  // Time when a low-latency stream is created.
  base::TimeTicks low_latency_create_time_;

  // Used for audio debug recordings. Accessed on audio thread.
  AudioInputWriter* input_writer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioInputController);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_INPUT_CONTROLLER_H_

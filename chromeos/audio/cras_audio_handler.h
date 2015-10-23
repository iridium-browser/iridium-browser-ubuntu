// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_
#define CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_

#include <stdint.h>
#include <queue>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/audio/audio_device.h"
#include "chromeos/audio/audio_pref_observer.h"
#include "chromeos/dbus/audio_node.h"
#include "chromeos/dbus/cras_audio_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "chromeos/dbus/volume_state.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

class AudioDevicesPrefHandler;

class CHROMEOS_EXPORT CrasAudioHandler : public CrasAudioClient::Observer,
                                         public AudioPrefObserver,
                                         public SessionManagerClient::Observer {
 public:
  typedef std::priority_queue<AudioDevice,
                              std::vector<AudioDevice>,
                              AudioDeviceCompare> AudioDevicePriorityQueue;
  typedef std::vector<uint64_t> NodeIdList;

  class AudioObserver {
   public:
    // Called when an active output volume changed.
    virtual void OnOutputNodeVolumeChanged(uint64_t node_id, int volume);

    // Called when output mute state changed.
    // |mute_on|: True if output is muted.
    // |system_adjust|: True if the mute state is adjusted by the system
    // automatically(i.e. not by user). UI should reflect the system's mute
    // state, but it should not be too loud, e.g., the volume pop up window
    // should not be triggered.
    virtual void OnOutputMuteChanged(bool mute_on, bool system_adjust);

    // Called when active input node's gain changed.
    virtual void OnInputNodeGainChanged(uint64_t node_id, int gain);

    // Called when input mute state changed.
    virtual void OnInputMuteChanged(bool mute_on);

    // Called when audio nodes changed.
    virtual void OnAudioNodesChanged();

    // Called when active audio node changed.
    virtual void OnActiveOutputNodeChanged();

    // Called when active audio input node changed.
    virtual void OnActiveInputNodeChanged();

   protected:
    AudioObserver();
    virtual ~AudioObserver();
    DISALLOW_COPY_AND_ASSIGN(AudioObserver);
  };

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize(
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);

  // Sets the global instance for testing.
  static void InitializeForTesting();

  // Destroys the global instance.
  static void Shutdown();

  // Returns true if the global instance is initialized.
  static bool IsInitialized();

  // Gets the global instance. Initialize must be called first.
  static CrasAudioHandler* Get();

  // Adds an audio observer.
  virtual void AddAudioObserver(AudioObserver* observer);

  // Removes an audio observer.
  virtual void RemoveAudioObserver(AudioObserver* observer);

  // Returns true if keyboard mic exists.
  virtual bool HasKeyboardMic();

  // Returns true if audio output is muted for the system.
  virtual bool IsOutputMuted();

  // Returns true if audio output is muted for a device.
  virtual bool IsOutputMutedForDevice(uint64_t device_id);

  // Returns true if audio input is muted.
  virtual bool IsInputMuted();

  // Returns true if audio input is muted for a device.
  virtual bool IsInputMutedForDevice(uint64_t device_id);

  // Returns true if the output volume is below the default mute volume level.
  virtual bool IsOutputVolumeBelowDefaultMuteLevel();

  // Returns volume level in 0-100% range at which the volume should be muted.
  virtual int GetOutputDefaultVolumeMuteThreshold();

  // Gets volume level in 0-100% range (0 being pure silence) for the current
  // active node.
  virtual int GetOutputVolumePercent();

  // Gets volume level in 0-100% range (0 being pure silence) for a device.
  virtual int GetOutputVolumePercentForDevice(uint64_t device_id);

  // Gets gain level in 0-100% range (0 being pure silence) for the current
  // active node.
  virtual int GetInputGainPercent();

  // Gets volume level in 0-100% range (0 being pure silence) for a device.
  virtual int GetInputGainPercentForDevice(uint64_t device_id);

  // Returns node_id of the primary active output node.
  virtual uint64_t GetPrimaryActiveOutputNode() const;

  // Returns the node_id of the primary active input node.
  virtual uint64_t GetPrimaryActiveInputNode() const;

  // Gets the audio devices back in |device_list|.
  // This call can be invoked from I/O thread or UI thread because
  // it does not need to access CrasAudioClient on DBus.
  virtual void GetAudioDevices(AudioDeviceList* device_list) const;

  virtual bool GetPrimaryActiveOutputDevice(AudioDevice* device) const;

  // Whether there is alternative input/output audio device.
  virtual bool has_alternative_input() const;
  virtual bool has_alternative_output() const;

  // Sets all active output devices' volume level to |volume_percent|, whose
  // range is from 0-100%.
  virtual void SetOutputVolumePercent(int volume_percent);

  // Sets all active input devices' gain level to |gain_percent|, whose range is
  // from 0-100%.
  virtual void SetInputGainPercent(int gain_percent);

  // Adjusts all active output devices' volume up (positive percentage) or down
  // (negative percentage).
  virtual void AdjustOutputVolumeByPercent(int adjust_by_percent);

  // Adjusts all active output devices' volume to a minimum audible level if it
  // is too low.
  virtual void AdjustOutputVolumeToAudibleLevel();

  // Mutes or unmutes audio output device.
  virtual void SetOutputMute(bool mute_on);

  // Mutes or unmutes audio input device.
  virtual void SetInputMute(bool mute_on);

  // Switches active audio device to |device|.
  virtual void SwitchToDevice(const AudioDevice& device, bool notify);

  // Sets volume/gain level for a device.
  virtual void SetVolumeGainPercentForDevice(uint64_t device_id, int value);

  // Sets the mute for device.
  virtual void SetMuteForDevice(uint64_t device_id, bool mute_on);

  // Activates or deactivates keyboard mic if there's one.
  virtual void SetKeyboardMicActive(bool active);

  // Changes the active nodes to the nodes specified by |new_active_ids|.
  // The caller can pass in the "complete" active node list of either input
  // nodes, or output nodes, or both. If only input nodes are passed in,
  // it will only change the input nodes' active status, output nodes will NOT
  // be changed; similarly for the case if only output nodes are passed.
  // If the nodes specified in |new_active_ids| are already active, they will
  // remain active. Otherwise, the old active nodes will be de-activated before
  // we activate the new nodes with the same type(input/output).
  virtual void ChangeActiveNodes(const NodeIdList& new_active_ids);

  // Swaps the left and right channel of the internal speaker.
  // Swap the left and right channel if |swap| is true; otherwise, swap the left
  // and right channel back to the normal mode.
  // If the feature is not supported on the device, nothing happens.
  virtual void SwapInternalSpeakerLeftRightChannel(bool swap);

  // Enables error logging.
  virtual void LogErrors();

  // If necessary, sets the starting point for re-discovering the active HDMI
  // output device caused by device entering/exiting docking mode, HDMI display
  // changing resolution, or chromeos device suspend/resume. If
  // |force_rediscovering| is true, it will force to set the starting point for
  // re-discovering the active HDMI output device again if it has been in the
  // middle of rediscovering the HDMI active output device.
  virtual void SetActiveHDMIOutoutRediscoveringIfNecessary(
      bool force_rediscovering);

 protected:
  explicit CrasAudioHandler(
      scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler);
  ~CrasAudioHandler() override;

 private:
  friend class CrasAudioHandlerTest;

  // CrasAudioClient::Observer overrides.
  void AudioClientRestarted() override;
  void NodesChanged() override;
  void ActiveOutputNodeChanged(uint64_t node_id) override;
  void ActiveInputNodeChanged(uint64_t node_id) override;

  // AudioPrefObserver overrides.
  void OnAudioPolicyPrefChanged() override;

  // SessionManagerClient::Observer overrides.
  void EmitLoginPromptVisibleCalled() override;

  // Sets the active audio output/input node to the node with |node_id|.
  // If |notify|, notifies Active*NodeChange.
  void SetActiveOutputNode(uint64_t node_id, bool notify);
  void SetActiveInputNode(uint64_t node_id, bool notify);

  // Sets up the audio device state based on audio policy and audio settings
  // saved in prefs.
  void SetupAudioInputState();
  void SetupAudioOutputState();

  // Sets up the additional active audio node's state.
  void SetupAdditionalActiveAudioNodeState(uint64_t node_id);

  const AudioDevice* GetDeviceFromId(uint64_t device_id) const;
  const AudioDevice* GetKeyboardMic() const;

  // Initializes audio state, which should only be called when CrasAudioHandler
  // is created or cras audio client is restarted.
  void InitializeAudioState();

  // Applies the audio muting policies whenever the user logs in or policy
  // change notification is received.
  void ApplyAudioPolicy();

  // Sets output volume of |node_id| to |volume|.
  void SetOutputNodeVolume(uint64_t node_id, int volume);

  void SetOutputNodeVolumePercent(uint64_t node_id, int volume_percent);

  // Sets output mute state to |mute_on| internally, returns true if output mute
  // is set.
  bool SetOutputMuteInternal(bool mute_on);

  // Sets input gain of |node_id| to |gain|.
  void SetInputNodeGain(uint64_t node_id, int gain);

  void SetInputNodeGainPercent(uint64_t node_id, int gain_percent);

  // Sets input mute state to |mute_on| internally.
  void SetInputMuteInternal(bool mute_on);

  // Calling dbus to get nodes data.
  void GetNodes();

  // Updates the current audio nodes list and switches the active device
  // if needed.
  void UpdateDevicesAndSwitchActive(const AudioNodeList& nodes);

  // Returns true if *|current_active_node_id| device is changed to
  // |new_active_device|.
  bool ChangeActiveDevice(const AudioDevice& new_active_device,
                          uint64_t* current_active_node_id);

  // Returns true if the audio nodes change is caused by some non-active
  // audio nodes unplugged.
  bool NonActiveDeviceUnplugged(size_t old_devices_size,
                                size_t new_device_size,
                                uint64_t current_active_node);

  // Returns true if there is any device change for for input or output,
  // specified by |is_input|.
  // The new discovered nodes are returned in |new_discovered|.
  bool HasDeviceChange(const AudioNodeList& new_nodes,
                       bool is_input,
                       AudioNodeList* new_discovered);

  // Handles dbus callback for GetNodes.
  void HandleGetNodes(const chromeos::AudioNodeList& node_list, bool success);

  // Handles the dbus error callback.
  void HandleGetNodesError(const std::string& error_name,
                           const std::string& error_msg);

  // Adds an active node.
  // If there is no active node, |node_id| will be switched to become the
  // primary active node. Otherwise, it will be added as an additional active
  // node.
  void AddActiveNode(uint64_t node_id, bool notify);

  // Adds |node_id| into additional active nodes.
  void AddAdditionalActiveNode(uint64_t node_id, bool notify);

  // Removes |node_id| from additional active nodes.
  void RemoveActiveNodeInternal(uint64_t node_id, bool notify);

  void UpdateAudioAfterHDMIRediscoverGracePeriod();

  bool IsHDMIPrimaryOutputDevice() const;

  void StartHDMIRediscoverGracePeriod();

  bool hdmi_rediscovering() const { return hdmi_rediscovering_; }

  void SetHDMIRediscoverGracePeriodForTesting(int duration_in_ms);

  enum DeviceStatus {
    OLD_DEVICE,
    NEW_DEVICE,
    CHANGED_DEVICE,
  };

  // Checks if |device| is a newly discovered, changed, or existing device for
  // the nodes sent from NodesChanged signal.
  DeviceStatus CheckDeviceStatus(const AudioDevice& device);

  void NotifyActiveNodeChanged(bool is_input);

  scoped_refptr<AudioDevicesPrefHandler> audio_pref_handler_;
  base::ObserverList<AudioObserver> observers_;

  // Audio data and state.
  AudioDeviceMap audio_devices_;

  AudioDevicePriorityQueue input_devices_pq_;
  AudioDevicePriorityQueue output_devices_pq_;

  bool output_mute_on_;
  bool input_mute_on_;
  int output_volume_;
  int input_gain_;
  uint64_t active_output_node_id_;
  uint64_t active_input_node_id_;
  bool has_alternative_input_;
  bool has_alternative_output_;

  bool output_mute_locked_;

  // Failures are not logged at startup, since CRAS may not be running yet.
  bool log_errors_;

  // Timer for HDMI re-discovering grace period.
  base::OneShotTimer<CrasAudioHandler> hdmi_rediscover_timer_;
  int hdmi_rediscover_grace_period_duration_in_ms_;
  bool hdmi_rediscovering_;

  base::WeakPtrFactory<CrasAudioHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrasAudioHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_AUDIO_CRAS_AUDIO_HANDLER_H_

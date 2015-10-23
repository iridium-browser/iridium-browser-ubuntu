// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_cras_audio_client.h"

namespace chromeos {

FakeCrasAudioClient::FakeCrasAudioClient()
    : active_input_node_id_(0),
      active_output_node_id_(0) {
}

FakeCrasAudioClient::~FakeCrasAudioClient() {
}

void FakeCrasAudioClient::Init(dbus::Bus* bus) {
  VLOG(1) << "FakeCrasAudioClient is created";

  // Fake audio output nodes.
  AudioNode output_1;
  output_1.is_input = false;
  output_1.id = 10001;
  output_1.device_name = "Fake Speaker";
  output_1.type = "INTERNAL_SPEAKER";
  output_1.name = "Speaker";
  node_list_.push_back(output_1);

  AudioNode output_2;
  output_2.is_input = false;
  output_2.id = 10002;
  output_2.device_name = "Fake Headphone";
  output_2.type = "HEADPHONE";
  output_2.name = "Headphone";
  node_list_.push_back(output_2);

  AudioNode output_3;
  output_3.is_input = false;
  output_3.id = 10003;
  output_3.device_name = "Fake Bluetooth Headphone";
  output_3.type = "BLUETOOTH";
  output_3.name = "Headphone";
  node_list_.push_back(output_3);

  AudioNode output_4;
  output_4.is_input = false;
  output_4.id = 10004;
  output_4.device_name = "Fake HDMI Speaker";
  output_4.type = "HDMI";
  output_4.name = "HDMI Speaker";
  node_list_.push_back(output_4);

  // Fake audio input nodes
  AudioNode input_1;
  input_1.is_input = true;
  input_1.id = 20001;
  input_1.device_name = "Fake Internal Mic";
  input_1.type = "INTERNAL_MIC";
  input_1.name = "Internal Mic";
  node_list_.push_back(input_1);

  AudioNode input_2;
  input_2.is_input = true;
  input_2.id = 20002;
  input_2.device_name = "Fake USB Mic";
  input_2.type = "USB";
  input_2.name = "Mic";
  node_list_.push_back(input_2);

  AudioNode input_3;
  input_3.is_input = true;
  input_3.id = 20003;
  input_3.device_name = "Fake Mick Jack";
  input_3.type = "MIC";
  input_3.name = "Some type of Mic";
  node_list_.push_back(input_3);
}

void FakeCrasAudioClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeCrasAudioClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeCrasAudioClient::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

void FakeCrasAudioClient::GetVolumeState(
    const GetVolumeStateCallback& callback) {
  callback.Run(volume_state_, true);
}

void FakeCrasAudioClient::GetNodes(const GetNodesCallback& callback,
                                   const ErrorCallback& error_callback) {
  callback.Run(node_list_, true);
}

void FakeCrasAudioClient::SetOutputNodeVolume(uint64 node_id, int32 volume) {}

void FakeCrasAudioClient::SetOutputUserMute(bool mute_on) {
  volume_state_.output_user_mute = mute_on;
  FOR_EACH_OBSERVER(Observer, observers_,
                    OutputMuteChanged(volume_state_.output_user_mute));
}

void FakeCrasAudioClient::SetInputNodeGain(uint64 node_id, int32 input_gain) {}

void FakeCrasAudioClient::SetInputMute(bool mute_on) {
  volume_state_.input_mute = mute_on;
  FOR_EACH_OBSERVER(Observer, observers_,
                    InputMuteChanged(volume_state_.input_mute));
}

void FakeCrasAudioClient::SetActiveOutputNode(uint64 node_id) {
  if (active_output_node_id_ == node_id)
    return;

  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == active_output_node_id_)
      node_list_[i].active = false;
    else if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
  active_output_node_id_ = node_id;
  FOR_EACH_OBSERVER(Observer, observers_, ActiveOutputNodeChanged(node_id));
}

void FakeCrasAudioClient::SetActiveInputNode(uint64 node_id) {
  if (active_input_node_id_ == node_id)
    return;

  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == active_input_node_id_)
      node_list_[i].active = false;
    else if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
  active_input_node_id_ = node_id;
  FOR_EACH_OBSERVER(Observer, observers_, ActiveInputNodeChanged(node_id));
}

void FakeCrasAudioClient::AddActiveInputNode(uint64 node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
}

void FakeCrasAudioClient::RemoveActiveInputNode(uint64 node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = false;
  }
}

void FakeCrasAudioClient::SwapLeftRight(uint64 node_id, bool swap) {
}

void FakeCrasAudioClient::AddActiveOutputNode(uint64 node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = true;
  }
}

void FakeCrasAudioClient::RemoveActiveOutputNode(uint64 node_id) {
  for (size_t i = 0; i < node_list_.size(); ++i) {
    if (node_list_[i].id == node_id)
      node_list_[i].active = false;
  }
}

void FakeCrasAudioClient::InsertAudioNodeToList(const AudioNode& audio_node) {
  auto iter = FindNode(audio_node.id);
  if (iter != node_list_.end())
    (*iter) = audio_node;
  else
    node_list_.push_back(audio_node);
  FOR_EACH_OBSERVER(Observer, observers_, NodesChanged());
}

void FakeCrasAudioClient::RemoveAudioNodeFromList(const uint64& node_id) {
  auto iter = FindNode(node_id);
  if (iter != node_list_.end()) {
    node_list_.erase(iter);
    FOR_EACH_OBSERVER(Observer, observers_, NodesChanged());
  }
}

void FakeCrasAudioClient::SetAudioNodesForTesting(
    const AudioNodeList& audio_nodes) {
  node_list_ = audio_nodes;
}

void FakeCrasAudioClient::SetAudioNodesAndNotifyObserversForTesting(
    const AudioNodeList& new_nodes) {
  SetAudioNodesForTesting(new_nodes);
  FOR_EACH_OBSERVER(Observer, observers_, NodesChanged());
}

AudioNodeList::iterator FakeCrasAudioClient::FindNode(uint64 node_id) {
  return std::find_if(
      node_list_.begin(), node_list_.end(),
      [node_id](const AudioNode& node) { return node_id == node.id; });
}

}  // namespace chromeos

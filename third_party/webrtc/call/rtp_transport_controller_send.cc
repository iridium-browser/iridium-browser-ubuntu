/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <utility>

#include "call/rtp_transport_controller_send.h"
#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "modules/congestion_controller/rtp/include/send_side_congestion_controller.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
const char kTaskQueueExperiment[] = "WebRTC-TaskQueueCongestionControl";
using TaskQueueController = webrtc::webrtc_cc::SendSideCongestionController;

bool TaskQueueExperimentEnabled() {
  std::string trial = webrtc::field_trial::FindFullName(kTaskQueueExperiment);
  return trial.find("Enable") == 0;
}

std::unique_ptr<SendSideCongestionControllerInterface> CreateController(
    Clock* clock,
    webrtc::RtcEventLog* event_log,
    PacedSender* pacer,
    const BitrateConstraints& bitrate_config,
    bool task_queue_controller) {
  if (task_queue_controller) {
    return rtc::MakeUnique<webrtc::webrtc_cc::SendSideCongestionController>(
        clock, event_log, pacer, bitrate_config.start_bitrate_bps,
        bitrate_config.min_bitrate_bps, bitrate_config.max_bitrate_bps);
  }
  auto cc = rtc::MakeUnique<webrtc::SendSideCongestionController>(
      clock, nullptr /* observer */, event_log, pacer);
  cc->SignalNetworkState(kNetworkDown);
  cc->SetBweBitrates(bitrate_config.min_bitrate_bps,
                     bitrate_config.start_bitrate_bps,
                     bitrate_config.max_bitrate_bps);
  return std::move(cc);
}
}  // namespace

RtpTransportControllerSend::RtpTransportControllerSend(
    Clock* clock,
    webrtc::RtcEventLog* event_log,
    const BitrateConstraints& bitrate_config)
    : clock_(clock),
      pacer_(clock, &packet_router_, event_log),
      bitrate_configurator_(bitrate_config),
      process_thread_(ProcessThread::Create("SendControllerThread")),
      observer_(nullptr),
      send_side_cc_(CreateController(clock,
                                     event_log,
                                     &pacer_,
                                     bitrate_config,
                                     TaskQueueExperimentEnabled())) {
  send_side_cc_ptr_ = send_side_cc_.get();
  process_thread_->RegisterModule(&pacer_, RTC_FROM_HERE);
  process_thread_->RegisterModule(send_side_cc_.get(), RTC_FROM_HERE);
  process_thread_->Start();
}

RtpTransportControllerSend::~RtpTransportControllerSend() {
  process_thread_->Stop();
  process_thread_->DeRegisterModule(send_side_cc_.get());
  process_thread_->DeRegisterModule(&pacer_);
}

void RtpTransportControllerSend::OnNetworkChanged(uint32_t bitrate_bps,
                                                  uint8_t fraction_loss,
                                                  int64_t rtt_ms,
                                                  int64_t probing_interval_ms) {
  // TODO(srte): Skip this step when old SendSideCongestionController is
  // deprecated.
  TargetTransferRate msg;
  msg.at_time = Timestamp::ms(clock_->TimeInMilliseconds());
  msg.target_rate = DataRate::bps(bitrate_bps);
  msg.network_estimate.at_time = msg.at_time;
  msg.network_estimate.bwe_period = TimeDelta::ms(probing_interval_ms);
  uint32_t bandwidth_bps;
  if (send_side_cc_ptr_->AvailableBandwidth(&bandwidth_bps))
    msg.network_estimate.bandwidth = DataRate::bps(bandwidth_bps);
  msg.network_estimate.loss_rate_ratio = fraction_loss / 255.0;
  msg.network_estimate.round_trip_time = TimeDelta::ms(rtt_ms);
  rtc::CritScope cs(&observer_crit_);
  // We wont register as observer until we have an observer.
  RTC_DCHECK(observer_ != nullptr);
  observer_->OnTargetTransferRate(msg);
}

PacketRouter* RtpTransportControllerSend::packet_router() {
  return &packet_router_;
}

TransportFeedbackObserver*
RtpTransportControllerSend::transport_feedback_observer() {
  return send_side_cc_.get();
}

RtpPacketSender* RtpTransportControllerSend::packet_sender() {
  return &pacer_;
}

const RtpKeepAliveConfig& RtpTransportControllerSend::keepalive_config() const {
  return keepalive_;
}

void RtpTransportControllerSend::SetAllocatedSendBitrateLimits(
    int min_send_bitrate_bps,
    int max_padding_bitrate_bps,
    int max_total_bitrate_bps) {
  send_side_cc_->SetAllocatedSendBitrateLimits(
      min_send_bitrate_bps, max_padding_bitrate_bps, max_total_bitrate_bps);
}

void RtpTransportControllerSend::SetKeepAliveConfig(
    const RtpKeepAliveConfig& config) {
  keepalive_ = config;
}
void RtpTransportControllerSend::SetPacingFactor(float pacing_factor) {
  send_side_cc_->SetPacingFactor(pacing_factor);
}
void RtpTransportControllerSend::SetQueueTimeLimit(int limit_ms) {
  pacer_.SetQueueTimeLimit(limit_ms);
}
CallStatsObserver* RtpTransportControllerSend::GetCallStatsObserver() {
  return send_side_cc_.get();
}
void RtpTransportControllerSend::RegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  send_side_cc_->RegisterPacketFeedbackObserver(observer);
}
void RtpTransportControllerSend::DeRegisterPacketFeedbackObserver(
    PacketFeedbackObserver* observer) {
  send_side_cc_->DeRegisterPacketFeedbackObserver(observer);
}

void RtpTransportControllerSend::RegisterTargetTransferRateObserver(
    TargetTransferRateObserver* observer) {
  {
    rtc::CritScope cs(&observer_crit_);
    RTC_DCHECK(observer_ == nullptr);
    observer_ = observer;
  }
  send_side_cc_->RegisterNetworkObserver(this);
}
void RtpTransportControllerSend::OnNetworkRouteChanged(
    const std::string& transport_name,
    const rtc::NetworkRoute& network_route) {
  // Check if the network route is connected.
  if (!network_route.connected) {
    RTC_LOG(LS_INFO) << "Transport " << transport_name << " is disconnected";
    // TODO(honghaiz): Perhaps handle this in SignalChannelNetworkState and
    // consider merging these two methods.
    return;
  }

  // Check whether the network route has changed on each transport.
  auto result =
      network_routes_.insert(std::make_pair(transport_name, network_route));
  auto kv = result.first;
  bool inserted = result.second;
  if (inserted) {
    // No need to reset BWE if this is the first time the network connects.
    return;
  }
  if (kv->second != network_route) {
    kv->second = network_route;
    BitrateConstraints bitrate_config = bitrate_configurator_.GetConfig();
    RTC_LOG(LS_INFO) << "Network route changed on transport " << transport_name
                     << ": new local network id "
                     << network_route.local_network_id
                     << " new remote network id "
                     << network_route.remote_network_id
                     << " Reset bitrates to min: "
                     << bitrate_config.min_bitrate_bps
                     << " bps, start: " << bitrate_config.start_bitrate_bps
                     << " bps,  max: " << bitrate_config.max_bitrate_bps
                     << " bps.";
    RTC_DCHECK_GT(bitrate_config.start_bitrate_bps, 0);
    send_side_cc_->OnNetworkRouteChanged(
        network_route, bitrate_config.start_bitrate_bps,
        bitrate_config.min_bitrate_bps, bitrate_config.max_bitrate_bps);
  }
}
void RtpTransportControllerSend::OnNetworkAvailability(bool network_available) {
  send_side_cc_->SignalNetworkState(network_available ? kNetworkUp
                                                      : kNetworkDown);
}
RtcpBandwidthObserver* RtpTransportControllerSend::GetBandwidthObserver() {
  return send_side_cc_->GetBandwidthObserver();
}
int64_t RtpTransportControllerSend::GetPacerQueuingDelayMs() const {
  return pacer_.QueueInMs();
}
int64_t RtpTransportControllerSend::GetFirstPacketTimeMs() const {
  return pacer_.FirstSentPacketTimeMs();
}
void RtpTransportControllerSend::SetPerPacketFeedbackAvailable(bool available) {
  send_side_cc_->SetPerPacketFeedbackAvailable(available);
}
void RtpTransportControllerSend::EnablePeriodicAlrProbing(bool enable) {
  send_side_cc_->EnablePeriodicAlrProbing(enable);
}
void RtpTransportControllerSend::OnSentPacket(
    const rtc::SentPacket& sent_packet) {
  send_side_cc_->OnSentPacket(sent_packet);
}

void RtpTransportControllerSend::SetSdpBitrateParameters(
    const BitrateConstraints& constraints) {
  rtc::Optional<BitrateConstraints> updated =
      bitrate_configurator_.UpdateWithSdpParameters(constraints);
  if (updated.has_value()) {
    send_side_cc_->SetBweBitrates(updated->min_bitrate_bps,
                                  updated->start_bitrate_bps,
                                  updated->max_bitrate_bps);
  } else {
    RTC_LOG(LS_VERBOSE)
        << "WebRTC.RtpTransportControllerSend.SetSdpBitrateParameters: "
        << "nothing to update";
  }
}

void RtpTransportControllerSend::SetClientBitratePreferences(
    const BitrateConstraintsMask& preferences) {
  rtc::Optional<BitrateConstraints> updated =
      bitrate_configurator_.UpdateWithClientPreferences(preferences);
  if (updated.has_value()) {
    send_side_cc_->SetBweBitrates(updated->min_bitrate_bps,
                                  updated->start_bitrate_bps,
                                  updated->max_bitrate_bps);
  } else {
    RTC_LOG(LS_VERBOSE)
        << "WebRTC.RtpTransportControllerSend.SetClientBitratePreferences: "
        << "nothing to update";
  }
}
}  // namespace webrtc

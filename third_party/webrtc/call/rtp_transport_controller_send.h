/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include <map>
#include <memory>
#include <string>

#include "call/rtp_bitrate_configurator.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/congestion_controller/include/send_side_congestion_controller.h"
#include "modules/pacing/packet_router.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/networkroute.h"

namespace webrtc {
class Clock;
class RtcEventLog;

// TODO(nisse): When we get the underlying transports here, we should
// have one object implementing RtpTransportControllerSendInterface
// per transport, sharing the same congestion controller.
class RtpTransportControllerSend : public RtpTransportControllerSendInterface {
 public:
  RtpTransportControllerSend(Clock* clock,
                             RtcEventLog* event_log,
                             const BitrateConstraints& bitrate_config);
  ~RtpTransportControllerSend() override;
  // Implements RtpTransportControllerSendInterface
  PacketRouter* packet_router() override;

  TransportFeedbackObserver* transport_feedback_observer() override;
  RtpPacketSender* packet_sender() override;
  const RtpKeepAliveConfig& keepalive_config() const override;

  void SetAllocatedSendBitrateLimits(int min_send_bitrate_bps,
                                     int max_padding_bitrate_bps) override;

  void SetKeepAliveConfig(const RtpKeepAliveConfig& config);
  void SetPacingFactor(float pacing_factor) override;
  void SetQueueTimeLimit(int limit_ms) override;
  CallStatsObserver* GetCallStatsObserver() override;
  void RegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) override;
  void DeRegisterPacketFeedbackObserver(
      PacketFeedbackObserver* observer) override;
  void RegisterNetworkObserver(NetworkChangedObserver* observer) override;
  void DeRegisterNetworkObserver(NetworkChangedObserver* observer) override;
  void OnNetworkRouteChanged(const std::string& transport_name,
                             const rtc::NetworkRoute& network_route) override;
  void OnNetworkAvailability(bool network_available) override;
  void SetTransportOverhead(
      size_t transport_overhead_bytes_per_packet) override;
  RtcpBandwidthObserver* GetBandwidthObserver() override;
  bool AvailableBandwidth(uint32_t* bandwidth) const override;
  int64_t GetPacerQueuingDelayMs() const override;
  int64_t GetFirstPacketTimeMs() const override;
  RateLimiter* GetRetransmissionRateLimiter() override;
  void EnablePeriodicAlrProbing(bool enable) override;
  void OnSentPacket(const rtc::SentPacket& sent_packet) override;

  void SetSdpBitrateParameters(const BitrateConstraints& constraints) override;
  void SetClientBitratePreferences(
      const BitrateConstraintsMask& preferences) override;

 private:
  PacketRouter packet_router_;
  PacedSender pacer_;
  SendSideCongestionController send_side_cc_;
  RtpKeepAliveConfig keepalive_;
  RtpBitrateConfigurator bitrate_configurator_;
  std::map<std::string, rtc::NetworkRoute> network_routes_;
  const std::unique_ptr<ProcessThread> process_thread_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpTransportControllerSend);
};

}  // namespace webrtc

#endif  // CALL_RTP_TRANSPORT_CONTROLLER_SEND_H_

/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

//  Implementation of Network-Assisted Dynamic Adaptation's (NADA's) proposal.
//  Version according to Draft Document (mentioned in references)
//  http://tools.ietf.org/html/draft-zhu-rmcat-nada-06
//  From March 26, 2015.

#include <math.h>
#include <algorithm>
#include <vector>
#include <iostream>

#include "webrtc/modules/remote_bitrate_estimator/test/estimators/nada.h"
#include "webrtc/modules/rtp_rtcp/interface/receive_statistics.h"

namespace webrtc {
namespace testing {
namespace bwe {

const int NadaBweReceiver::kMedian;

NadaBweReceiver::NadaBweReceiver(int flow_id)
    : BweReceiver(flow_id),
      clock_(0),
      last_feedback_ms_(0),
      recv_stats_(ReceiveStatistics::Create(&clock_)),
      baseline_delay_ms_(0),
      delay_signal_ms_(0),
      last_congestion_signal_ms_(0),
      last_delays_index_(0),
      exp_smoothed_delay_ms_(-1),
      est_queuing_delay_signal_ms_(0) {
}

NadaBweReceiver::~NadaBweReceiver() {
}

void NadaBweReceiver::ReceivePacket(int64_t arrival_time_ms,
                                    const MediaPacket& media_packet) {
  const float kAlpha = 0.9f;                 // Used for exponential smoothing.
  const int64_t kDelayLowThresholdMs = 50;   // Referred as d_th.
  const int64_t kDelayMaxThresholdMs = 400;  // Referred as d_max.

  clock_.AdvanceTimeMilliseconds(arrival_time_ms - clock_.TimeInMilliseconds());
  recv_stats_->IncomingPacket(media_packet.header(),
                              media_packet.payload_size(), false);
  int64_t delay_ms = arrival_time_ms -
                     media_packet.creation_time_us() / 1000;  // Refered as x_n.
  // The min should be updated within the first 10 minutes.
  if (clock_.TimeInMilliseconds() < 10 * 60 * 1000) {
    baseline_delay_ms_ = std::min(baseline_delay_ms_, delay_ms);
  }
  delay_signal_ms_ = delay_ms - baseline_delay_ms_;  // Refered as d_n.
  last_delays_ms_[(last_delays_index_++) % kMedian] = delay_signal_ms_;
  int size = std::min(last_delays_index_, kMedian);
  int64_t median_filtered_delay_ms_ = MedianFilter(last_delays_ms_, size);
  exp_smoothed_delay_ms_ = ExponentialSmoothingFilter(
      median_filtered_delay_ms_, exp_smoothed_delay_ms_, kAlpha);

  if (exp_smoothed_delay_ms_ < kDelayLowThresholdMs) {
    est_queuing_delay_signal_ms_ = exp_smoothed_delay_ms_;
  } else if (exp_smoothed_delay_ms_ < kDelayMaxThresholdMs) {
    est_queuing_delay_signal_ms_ = static_cast<int64_t>(
        pow(0.001 * (kDelayMaxThresholdMs - exp_smoothed_delay_ms_), 4.0) /
        pow(0.001 * (kDelayMaxThresholdMs - kDelayLowThresholdMs), 4.0));
  } else {
    est_queuing_delay_signal_ms_ = 0;
  }

  received_packets_->Insert(media_packet.sequence_number(),
                            media_packet.send_time_ms(), arrival_time_ms,
                            media_packet.payload_size());
}

FeedbackPacket* NadaBweReceiver::GetFeedback(int64_t now_ms) {
  const int64_t kPacketLossPenaltyMs = 1000;  // Referred as d_L.

  if (now_ms - last_feedback_ms_ < 100) {
    return NULL;
  }

  int64_t loss_signal_ms = static_cast<int64_t>(
      RecentPacketLossRatio() * kPacketLossPenaltyMs + 0.5f);
  int64_t congestion_signal_ms = est_queuing_delay_signal_ms_ + loss_signal_ms;

  float derivative = 0.0f;
  if (last_feedback_ms_ > 0) {
    derivative = (congestion_signal_ms - last_congestion_signal_ms_) /
                 static_cast<float>(now_ms - last_feedback_ms_);
  }
  last_feedback_ms_ = now_ms;
  last_congestion_signal_ms_ = congestion_signal_ms;

  PacketIdentifierNode* latest = *(received_packets_->begin());
  int64_t corrected_send_time_ms =
      latest->send_time_ms_ + now_ms - latest->arrival_time_ms_;

  // Sends a tuple containing latest values of <d_hat_n, d_tilde_n, x_n, x'_n,
  // R_r> and additional information.
  return new NadaFeedback(flow_id_, now_ms, exp_smoothed_delay_ms_,
                          est_queuing_delay_signal_ms_, congestion_signal_ms,
                          derivative, RecentReceivingRate(),
                          corrected_send_time_ms);
}

float NadaBweReceiver::GlobalPacketLossRatio() {
  if (received_packets_->empty()) {
    return 0.0f;
  }
  // Possibly there are packets missing.
  const uint16_t kMaxGap = 1.5 * kSetCapacity;
  uint16_t min = received_packets_->find_min();
  uint16_t max = received_packets_->find_max();

  int gap;
  if (max - min < kMaxGap) {
    gap = max - min + 1;
  } else {  // There was an overflow.
    max = received_packets_->upper_bound(kMaxGap);
    min = received_packets_->lower_bound(0xFFFF - kMaxGap);
    gap = max + (0xFFFF - min) + 2;
  }
  return static_cast<float>(received_packets_->size()) / gap;
}

// Go through a fixed time window of most recent packets received and
// counts packets missing to obtain the packet loss ratio. If an unordered
// packet falls out of the timewindow it will be counted as missing.
// E.g.: for a timewindow covering 5 packets of the following arrival sequence
// {10 7 9 5 6} 8 3 2 4 1, the output will be 1/6 (#8 is considered as missing).
float NadaBweReceiver::RecentPacketLossRatio() {
  const int64_t kRecentTimeWindowMs = 500;

  if (received_packets_->empty()) {
    return 0.0f;
  }
  int number_packets_received = 0;

  PacketNodeIt node_it = received_packets_->begin();  // Latest.

  // Lowest timestamp limit, oldest one that should be checked.
  int64_t time_limit_ms = (*node_it)->arrival_time_ms_ - kRecentTimeWindowMs;
  // Oldest and newest values found within the given time window.
  uint16_t oldest_seq_nb = (*node_it)->sequence_number_;
  uint16_t newest_seq_nb = oldest_seq_nb;

  while (node_it != received_packets_->end()) {
    if ((*node_it)->arrival_time_ms_ < time_limit_ms) {
      break;
    }
    uint16_t seq_nb = (*node_it)->sequence_number_;
    if (IsNewerSequenceNumber(seq_nb, newest_seq_nb)) {
      newest_seq_nb = seq_nb;
    }
    if (IsNewerSequenceNumber(oldest_seq_nb, seq_nb)) {
      oldest_seq_nb = seq_nb;
    }
    ++node_it;
    ++number_packets_received;
  }

  // Interval width between oldest and newest sequence number.
  // There was an overflow if newest_seq_nb < oldest_seq_nb.
  int gap = static_cast<uint16_t>(newest_seq_nb - oldest_seq_nb + 1);

  return static_cast<float>(gap - number_packets_received) / gap;
}

size_t NadaBweReceiver::RecentReceivingRate() {
  const int64_t kRecentTimeWindowMs = 500;
  if (received_packets_->empty()) {
    return 0.0f;
  }
  size_t totalSize = 0;
  int64_t time_limit_ms = clock_.TimeInMilliseconds() - kRecentTimeWindowMs;
  PacketNodeIt node_it = received_packets_->begin();
  PacketNodeIt end = received_packets_->end();

  while (node_it != end && (*node_it)->arrival_time_ms_ > time_limit_ms) {
    totalSize += (*node_it)->payload_size_;
    ++node_it;
  }

  return static_cast<size_t>((1000 * totalSize) / kRecentTimeWindowMs);
}

int64_t NadaBweReceiver::MedianFilter(int64_t* last_delays_ms, int size) {
  // Typically size = 5.
  std::vector<int64_t> array_copy(last_delays_ms, last_delays_ms + size);
  std::nth_element(array_copy.begin(), array_copy.begin() + size / 2,
                   array_copy.end());
  return array_copy.at(size / 2);
}

int64_t NadaBweReceiver::ExponentialSmoothingFilter(int64_t new_value,
                                                    int64_t last_smoothed_value,
                                                    float alpha) {
  if (last_smoothed_value < 0) {
    return new_value;  // Handling initial case.
  }
  return static_cast<int64_t>(alpha * new_value +
                              (1.0f - alpha) * last_smoothed_value + 0.5f);
}

NadaBweSender::NadaBweSender(int kbps, BitrateObserver* observer, Clock* clock)
    : clock_(clock), observer_(observer), bitrate_kbps_(kbps) {
}

NadaBweSender::NadaBweSender(BitrateObserver* observer, Clock* clock)
    : clock_(clock), observer_(observer), bitrate_kbps_(kMinRefRateKbps) {
}

NadaBweSender::~NadaBweSender() {
}

int NadaBweSender::GetFeedbackIntervalMs() const {
  return 100;
}

void NadaBweSender::GiveFeedback(const FeedbackPacket& feedback) {
  const NadaFeedback& fb = static_cast<const NadaFeedback&>(feedback);
  // Following parameters might be optimized.
  const int64_t kQueuingDelayUpperBoundMs = 10;
  const float kDerivativeUpperBound = 10.0f * min_feedback_delay_ms_;

  const int kMaxRefRateKbps = 1500;  // Referred as R_max.

  int64_t now_ms = clock_->TimeInMilliseconds();
  float delta_s = now_ms - last_feedback_ms_;
  last_feedback_ms_ = now_ms;
  // Update delta_0.
  min_feedback_delay_ms_ =
      std::min(min_feedback_delay_ms_, static_cast<int64_t>(delta_s));

  // Update RTT_0.
  int64_t rtt = now_ms - fb.latest_send_time_ms();
  min_round_trip_time_ms_ = std::min(min_round_trip_time_ms_, rtt);

  // Independent limits for those variables.
  // There should be no packet losses/marking, hence x_n == d_tilde.
  if (fb.congestion_signal() == fb.est_queuing_delay_signal_ms() &&
      fb.est_queuing_delay_signal_ms() < kQueuingDelayUpperBoundMs &&
      fb.derivative() < kDerivativeUpperBound) {
    AcceleratedRampUp(fb, kMaxRefRateKbps);
  } else {
    GradualRateUpdate(fb, kMaxRefRateKbps, delta_s);
  }
}

int64_t NadaBweSender::TimeUntilNextProcess() {
  return 100;
}

int NadaBweSender::Process() {
  return 0;
}

void NadaBweSender::AcceleratedRampUp(const NadaFeedback& fb,
                                      const int kMaxRefRateKbps) {
  const int kMaxRampUpQueuingDelayMs = 50;  // Referred as T_th.
  const float kGamma0 = 0.5f;               // Referred as gamma_0.

  float gamma =
      std::min(kGamma0, static_cast<float>(kMaxRampUpQueuingDelayMs) /
                            (min_round_trip_time_ms_ + min_feedback_delay_ms_));
  bitrate_kbps_ = static_cast<int>((1.0f + gamma) * fb.receiving_rate() + 0.5f);

  bitrate_kbps_ = std::min(bitrate_kbps_, kMaxRefRateKbps);
  bitrate_kbps_ = std::max(bitrate_kbps_, kMinRefRateKbps);
}

void NadaBweSender::GradualRateUpdate(const NadaFeedback& fb,
                                      const int kMaxRefRateKbps,
                                      const float delta_s) {
  const float kTauOMs = 500.0f;           // Referred as tau_o.
  const float kEta = 2.0f;                // Referred as eta.
  const float kKappa = 1.0f;              // Referred as kappa.
  const float kReferenceDelayMs = 10.0f;  // Referred as x_ref.

  float kPriorityWeight = static_cast<float>(fb.exp_smoothed_delay_ms()) /
                          kReferenceDelayMs;  // Referred as w.

  float kTheta =
      kPriorityWeight * (kMaxRefRateKbps - kMinRefRateKbps) * kReferenceDelayMs;
  float x_hat = fb.congestion_signal() + kEta * kTauOMs * fb.derivative();

  bitrate_kbps_ =
      bitrate_kbps_ +
      static_cast<int>((kKappa * delta_s *
                        (kTheta - (bitrate_kbps_ - kMinRefRateKbps) * x_hat)) /
                           (kTauOMs * kTauOMs) +
                       0.5f);

  bitrate_kbps_ = std::min(bitrate_kbps_, kMaxRefRateKbps);
  bitrate_kbps_ = std::max(bitrate_kbps_, kMinRefRateKbps);

  observer_->OnNetworkChanged(1000 * bitrate_kbps_, 0, 0);
}

void LinkedSet::Insert(uint16_t sequence_number,
                       int64_t send_time_ms,
                       int64_t arrival_time_ms,
                       size_t payload_size) {
  std::map<uint16_t, PacketNodeIt>::iterator it = map_.find(sequence_number);
  if (it != map_.end()) {
    if (it->second != list_.begin()) {
      list_.erase(it->second);
      list_.push_front(*(it->second));
    }
  } else {
    if (size() == capacity_) {
      RemoveTail();
    }
    UpdateHead(new PacketIdentifierNode(sequence_number, send_time_ms,
                                        arrival_time_ms, payload_size));
  }
}
void LinkedSet::RemoveTail() {
  map_.erase(list_.back()->sequence_number_);
  list_.pop_back();
}
void LinkedSet::UpdateHead(PacketIdentifierNode* new_head) {
  list_.push_front(new_head);
  map_[new_head->sequence_number_] = list_.begin();
}

}  // namespace bwe
}  // namespace testing
}  // namespace webrtc

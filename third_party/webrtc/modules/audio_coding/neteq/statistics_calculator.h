/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ_STATISTICS_CALCULATOR_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ_STATISTICS_CALCULATOR_H_

#include <string>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/neteq/interface/neteq.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Forward declarations.
class DecisionLogic;
class DelayManager;

// This class handles various network statistics in NetEq.
class StatisticsCalculator {
 public:
  StatisticsCalculator();

  virtual ~StatisticsCalculator() {}

  // Resets most of the counters.
  void Reset();

  // Resets the counters that are not handled by Reset().
  void ResetMcu();

  // Resets the waiting time statistics.
  void ResetWaitingTimeStatistics();

  // Reports that |num_samples| samples were produced through expansion, and
  // that the expansion produced other than just noise samples.
  void ExpandedVoiceSamples(int num_samples);

  // Reports that |num_samples| samples were produced through expansion, and
  // that the expansion produced only noise samples.
  void ExpandedNoiseSamples(int num_samples);

  // Reports that |num_samples| samples were produced through preemptive
  // expansion.
  void PreemptiveExpandedSamples(int num_samples);

  // Reports that |num_samples| samples were removed through accelerate.
  void AcceleratedSamples(int num_samples);

  // Reports that |num_samples| zeros were inserted into the output.
  void AddZeros(int num_samples);

  // Reports that |num_packets| packets were discarded.
  void PacketsDiscarded(int num_packets);

  // Reports that |num_samples| were lost.
  void LostSamples(int num_samples);

  // Increases the report interval counter with |num_samples| at a sample rate
  // of |fs_hz|. This is how the StatisticsCalculator gets notified that current
  // time is increasing.
  void IncreaseCounter(int num_samples, int fs_hz);

  // Stores new packet waiting time in waiting time statistics.
  void StoreWaitingTime(int waiting_time_ms);

  // Reports that |num_samples| samples were decoded from secondary packets.
  void SecondaryDecodedSamples(int num_samples);

  // Logs a delayed packet outage event of |outage_duration_ms|. A delayed
  // packet outage event is defined as an expand period caused not by an actual
  // packet loss, but by a delayed packet.
  virtual void LogDelayedPacketOutageEvent(int outage_duration_ms);

  // Returns the current network statistics in |stats|. The current sample rate
  // is |fs_hz|, the total number of samples in packet buffer and sync buffer
  // yet to play out is |num_samples_in_buffers|, and the number of samples per
  // packet is |samples_per_packet|.
  void GetNetworkStatistics(int fs_hz,
                            int num_samples_in_buffers,
                            int samples_per_packet,
                            const DelayManager& delay_manager,
                            const DecisionLogic& decision_logic,
                            NetEqNetworkStatistics *stats);

  void WaitingTimes(std::vector<int>* waiting_times);

 private:
  static const int kMaxReportPeriod = 60;  // Seconds before auto-reset.
  static const int kLenWaitingTimes = 100;

  class PeriodicUmaLogger {
   public:
    PeriodicUmaLogger(const std::string& uma_name,
                      int report_interval_ms,
                      int max_value);
    virtual ~PeriodicUmaLogger();
    void AdvanceClock(int step_ms);

   protected:
    void LogToUma(int value) const;
    virtual int Metric() const = 0;
    virtual void Reset() = 0;

    const std::string uma_name_;
    const int report_interval_ms_;
    const int max_value_;
    int timer_ = 0;
  };

  class PeriodicUmaCount final : public PeriodicUmaLogger {
   public:
    PeriodicUmaCount(const std::string& uma_name,
                     int report_interval_ms,
                     int max_value);
    ~PeriodicUmaCount() override;
    void RegisterSample();

   protected:
    int Metric() const override;
    void Reset() override;

   private:
    int counter_ = 0;
  };

  class PeriodicUmaAverage final : public PeriodicUmaLogger {
   public:
    PeriodicUmaAverage(const std::string& uma_name,
                       int report_interval_ms,
                       int max_value);
    ~PeriodicUmaAverage() override;
    void RegisterSample(int value);

   protected:
    int Metric() const override;
    void Reset() override;

   private:
    double sum_ = 0.0;
    int counter_ = 0;
  };

  // Calculates numerator / denominator, and returns the value in Q14.
  static uint16_t CalculateQ14Ratio(uint32_t numerator, uint32_t denominator);

  uint32_t preemptive_samples_;
  uint32_t accelerate_samples_;
  int added_zero_samples_;
  uint32_t expanded_speech_samples_;
  uint32_t expanded_noise_samples_;
  int discarded_packets_;
  uint32_t lost_timestamps_;
  uint32_t timestamps_since_last_report_;
  int waiting_times_[kLenWaitingTimes];  // Used as a circular buffer.
  int len_waiting_times_;
  int next_waiting_time_index_;
  uint32_t secondary_decoded_samples_;
  PeriodicUmaCount delayed_packet_outage_counter_;
  PeriodicUmaAverage excess_buffer_delay_;

  DISALLOW_COPY_AND_ASSIGN(StatisticsCalculator);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ_STATISTICS_CALCULATOR_H_

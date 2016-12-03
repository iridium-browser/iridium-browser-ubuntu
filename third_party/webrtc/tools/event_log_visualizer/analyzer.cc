/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/event_log_visualizer/analyzer.h"

#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include "webrtc/audio_receive_stream.h"
#include "webrtc/audio_send_stream.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/call.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_utility.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {
namespace plotting {

namespace {

std::string SsrcToString(uint32_t ssrc) {
  std::stringstream ss;
  ss << "SSRC " << ssrc;
  return ss.str();
}

// Checks whether an SSRC is contained in the list of desired SSRCs.
// Note that an empty SSRC list matches every SSRC.
bool MatchingSsrc(uint32_t ssrc, const std::vector<uint32_t>& desired_ssrc) {
  if (desired_ssrc.size() == 0)
    return true;
  return std::find(desired_ssrc.begin(), desired_ssrc.end(), ssrc) !=
         desired_ssrc.end();
}

double AbsSendTimeToMicroseconds(int64_t abs_send_time) {
  // The timestamp is a fixed point representation with 6 bits for seconds
  // and 18 bits for fractions of a second. Thus, we divide by 2^18 to get the
  // time in seconds and then multiply by 1000000 to convert to microseconds.
  static constexpr double kTimestampToMicroSec =
      1000000.0 / static_cast<double>(1ul << 18);
  return abs_send_time * kTimestampToMicroSec;
}

// Computes the difference |later| - |earlier| where |later| and |earlier|
// are counters that wrap at |modulus|. The difference is chosen to have the
// least absolute value. For example if |modulus| is 8, then the difference will
// be chosen in the range [-3, 4]. If |modulus| is 9, then the difference will
// be in [-4, 4].
int64_t WrappingDifference(uint32_t later, uint32_t earlier, int64_t modulus) {
  RTC_DCHECK_LE(1, modulus);
  RTC_DCHECK_LT(later, modulus);
  RTC_DCHECK_LT(earlier, modulus);
  int64_t difference =
      static_cast<int64_t>(later) - static_cast<int64_t>(earlier);
  int64_t max_difference = modulus / 2;
  int64_t min_difference = max_difference - modulus + 1;
  if (difference > max_difference) {
    difference -= modulus;
  }
  if (difference < min_difference) {
    difference += modulus;
  }
  return difference;
}

void RegisterHeaderExtensions(
    const std::vector<webrtc::RtpExtension>& extensions,
    webrtc::RtpHeaderExtensionMap* extension_map) {
  extension_map->Erase();
  for (const webrtc::RtpExtension& extension : extensions) {
    extension_map->Register(webrtc::StringToRtpExtensionType(extension.uri),
                            extension.id);
  }
}

constexpr float kLeftMargin = 0.01f;
constexpr float kRightMargin = 0.02f;
constexpr float kBottomMargin = 0.02f;
constexpr float kTopMargin = 0.05f;

class NetworkDelayDiff {
 public:
  class AbsSendTime {
   public:
    using DataType = LoggedRtpPacket;
    using ResultType = double;
    double operator()(const LoggedRtpPacket& old_packet,
                      const LoggedRtpPacket& new_packet) {
      if (old_packet.header.extension.hasAbsoluteSendTime &&
          new_packet.header.extension.hasAbsoluteSendTime) {
        int64_t send_time_diff = WrappingDifference(
            new_packet.header.extension.absoluteSendTime,
            old_packet.header.extension.absoluteSendTime, 1ul << 24);
        int64_t recv_time_diff = new_packet.timestamp - old_packet.timestamp;
        return static_cast<double>(recv_time_diff -
                                   AbsSendTimeToMicroseconds(send_time_diff)) /
               1000;
      } else {
        return 0;
      }
    }
  };

  class CaptureTime {
   public:
    using DataType = LoggedRtpPacket;
    using ResultType = double;
    double operator()(const LoggedRtpPacket& old_packet,
                      const LoggedRtpPacket& new_packet) {
      int64_t send_time_diff = WrappingDifference(
          new_packet.header.timestamp, old_packet.header.timestamp, 1ull << 32);
      int64_t recv_time_diff = new_packet.timestamp - old_packet.timestamp;

      const double kVideoSampleRate = 90000;
      // TODO(terelius): We treat all streams as video for now, even though
      // audio might be sampled at e.g. 16kHz, because it is really difficult to
      // figure out the true sampling rate of a stream. The effect is that the
      // delay will be scaled incorrectly for non-video streams.

      double delay_change =
          static_cast<double>(recv_time_diff) / 1000 -
          static_cast<double>(send_time_diff) / kVideoSampleRate * 1000;
      return delay_change;
    }
  };
};

template <typename Extractor>
class Accumulated {
 public:
  using DataType = typename Extractor::DataType;
  using ResultType = typename Extractor::ResultType;
  ResultType operator()(const DataType& old_packet,
                        const DataType& new_packet) {
    sum += extract(old_packet, new_packet);
    return sum;
  }

 private:
  Extractor extract;
  ResultType sum = 0;
};

template <typename Extractor>
void Pairwise(const std::vector<typename Extractor::DataType>& data,
              uint64_t begin_time,
              TimeSeries* result) {
  Extractor extract;
  for (size_t i = 1; i < data.size(); i++) {
    float x = static_cast<float>(data[i].timestamp - begin_time) / 1000000;
    float y = extract(data[i - 1], data[i]);
    result->points.emplace_back(x, y);
  }
}

}  // namespace

EventLogAnalyzer::EventLogAnalyzer(const ParsedRtcEventLog& log)
    : parsed_log_(log), window_duration_(250000), step_(10000) {
  uint64_t first_timestamp = std::numeric_limits<uint64_t>::max();
  uint64_t last_timestamp = std::numeric_limits<uint64_t>::min();

  // Maps a stream identifier consisting of ssrc and direction
  // to the header extensions used by that stream,
  std::map<StreamId, RtpHeaderExtensionMap> extension_maps;

  PacketDirection direction;
  uint8_t header[IP_PACKET_SIZE];
  size_t header_length;
  size_t total_length;

  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type != ParsedRtcEventLog::VIDEO_RECEIVER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::VIDEO_SENDER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::AUDIO_RECEIVER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::AUDIO_SENDER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::LOG_START &&
        event_type != ParsedRtcEventLog::LOG_END) {
      uint64_t timestamp = parsed_log_.GetTimestamp(i);
      first_timestamp = std::min(first_timestamp, timestamp);
      last_timestamp = std::max(last_timestamp, timestamp);
    }

    switch (parsed_log_.GetEventType(i)) {
      case ParsedRtcEventLog::VIDEO_RECEIVER_CONFIG_EVENT: {
        VideoReceiveStream::Config config(nullptr);
        parsed_log_.GetVideoReceiveConfig(i, &config);
        StreamId stream(config.rtp.remote_ssrc, kIncomingPacket);
        RegisterHeaderExtensions(config.rtp.extensions,
                                 &extension_maps[stream]);
        video_ssrcs_.insert(stream);
        for (auto kv : config.rtp.rtx) {
          StreamId rtx_stream(kv.second.ssrc, kIncomingPacket);
          RegisterHeaderExtensions(config.rtp.extensions,
                                   &extension_maps[rtx_stream]);
          video_ssrcs_.insert(rtx_stream);
          rtx_ssrcs_.insert(rtx_stream);
        }
        break;
      }
      case ParsedRtcEventLog::VIDEO_SENDER_CONFIG_EVENT: {
        VideoSendStream::Config config(nullptr);
        parsed_log_.GetVideoSendConfig(i, &config);
        for (auto ssrc : config.rtp.ssrcs) {
          StreamId stream(ssrc, kOutgoingPacket);
          RegisterHeaderExtensions(config.rtp.extensions,
                                   &extension_maps[stream]);
          video_ssrcs_.insert(stream);
        }
        for (auto ssrc : config.rtp.rtx.ssrcs) {
          StreamId rtx_stream(ssrc, kOutgoingPacket);
          RegisterHeaderExtensions(config.rtp.extensions,
                                   &extension_maps[rtx_stream]);
          video_ssrcs_.insert(rtx_stream);
          rtx_ssrcs_.insert(rtx_stream);
        }
        break;
      }
      case ParsedRtcEventLog::AUDIO_RECEIVER_CONFIG_EVENT: {
        AudioReceiveStream::Config config;
        // TODO(terelius): Parse the audio configs once we have them.
        break;
      }
      case ParsedRtcEventLog::AUDIO_SENDER_CONFIG_EVENT: {
        AudioSendStream::Config config(nullptr);
        // TODO(terelius): Parse the audio configs once we have them.
        break;
      }
      case ParsedRtcEventLog::RTP_EVENT: {
        MediaType media_type;
        parsed_log_.GetRtpHeader(i, &direction, &media_type, header,
                                 &header_length, &total_length);
        // Parse header to get SSRC.
        RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
        RTPHeader parsed_header;
        rtp_parser.Parse(&parsed_header);
        StreamId stream(parsed_header.ssrc, direction);
        // Look up the extension_map and parse it again to get the extensions.
        if (extension_maps.count(stream) == 1) {
          RtpHeaderExtensionMap* extension_map = &extension_maps[stream];
          rtp_parser.Parse(&parsed_header, extension_map);
        }
        uint64_t timestamp = parsed_log_.GetTimestamp(i);
        rtp_packets_[stream].push_back(
            LoggedRtpPacket(timestamp, parsed_header, total_length));
        break;
      }
      case ParsedRtcEventLog::RTCP_EVENT: {
        uint8_t packet[IP_PACKET_SIZE];
        MediaType media_type;
        parsed_log_.GetRtcpPacket(i, &direction, &media_type, packet,
                                  &total_length);

        RtpUtility::RtpHeaderParser rtp_parser(packet, total_length);
        RTPHeader parsed_header;
        RTC_CHECK(rtp_parser.ParseRtcp(&parsed_header));
        uint32_t ssrc = parsed_header.ssrc;

        RTCPUtility::RTCPParserV2 rtcp_parser(packet, total_length, true);
        RTC_CHECK(rtcp_parser.IsValid());

        RTCPUtility::RTCPPacketTypes packet_type = rtcp_parser.Begin();
        while (packet_type != RTCPUtility::RTCPPacketTypes::kInvalid) {
          switch (packet_type) {
            case RTCPUtility::RTCPPacketTypes::kTransportFeedback: {
              // Currently feedback is logged twice, both for audio and video.
              // Only act on one of them.
              if (media_type == MediaType::VIDEO) {
                std::unique_ptr<rtcp::RtcpPacket> rtcp_packet(
                    rtcp_parser.ReleaseRtcpPacket());
                StreamId stream(ssrc, direction);
                uint64_t timestamp = parsed_log_.GetTimestamp(i);
                rtcp_packets_[stream].push_back(LoggedRtcpPacket(
                    timestamp, kRtcpTransportFeedback, std::move(rtcp_packet)));
              }
              break;
            }
            default:
              break;
          }
          rtcp_parser.Iterate();
          packet_type = rtcp_parser.PacketType();
        }
        break;
      }
      case ParsedRtcEventLog::LOG_START: {
        break;
      }
      case ParsedRtcEventLog::LOG_END: {
        break;
      }
      case ParsedRtcEventLog::BWE_PACKET_LOSS_EVENT: {
        BwePacketLossEvent bwe_update;
        bwe_update.timestamp = parsed_log_.GetTimestamp(i);
        parsed_log_.GetBwePacketLossEvent(i, &bwe_update.new_bitrate,
                                             &bwe_update.fraction_loss,
                                             &bwe_update.expected_packets);
        bwe_loss_updates_.push_back(bwe_update);
        break;
      }
      case ParsedRtcEventLog::BWE_PACKET_DELAY_EVENT: {
        break;
      }
      case ParsedRtcEventLog::AUDIO_PLAYOUT_EVENT: {
        break;
      }
      case ParsedRtcEventLog::UNKNOWN_EVENT: {
        break;
      }
    }
  }

  if (last_timestamp < first_timestamp) {
    // No useful events in the log.
    first_timestamp = last_timestamp = 0;
  }
  begin_time_ = first_timestamp;
  end_time_ = last_timestamp;
  call_duration_s_ = static_cast<float>(end_time_ - begin_time_) / 1000000;
}

class BitrateObserver : public CongestionController::Observer,
                        public RemoteBitrateObserver {
 public:
  BitrateObserver() : last_bitrate_bps_(0), bitrate_updated_(false) {}

  void OnNetworkChanged(uint32_t bitrate_bps,
                        uint8_t fraction_loss,
                        int64_t rtt_ms) override {
    last_bitrate_bps_ = bitrate_bps;
    bitrate_updated_ = true;
  }

  void OnReceiveBitrateChanged(const std::vector<uint32_t>& ssrcs,
                               uint32_t bitrate) override {}

  uint32_t last_bitrate_bps() const { return last_bitrate_bps_; }
  bool GetAndResetBitrateUpdated() {
    bool bitrate_updated = bitrate_updated_;
    bitrate_updated_ = false;
    return bitrate_updated;
  }

 private:
  uint32_t last_bitrate_bps_;
  bool bitrate_updated_;
};

bool EventLogAnalyzer::IsRtxSsrc(StreamId stream_id) {
  return rtx_ssrcs_.count(stream_id) == 1;
}

bool EventLogAnalyzer::IsVideoSsrc(StreamId stream_id) {
  return video_ssrcs_.count(stream_id) == 1;
}

bool EventLogAnalyzer::IsAudioSsrc(StreamId stream_id) {
  return audio_ssrcs_.count(stream_id) == 1;
}

void EventLogAnalyzer::CreatePacketGraph(PacketDirection desired_direction,
                                         Plot* plot) {
  std::map<uint32_t, TimeSeries> time_series;

  PacketDirection direction;
  MediaType media_type;
  uint8_t header[IP_PACKET_SIZE];
  size_t header_length, total_length;

  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::RTP_EVENT) {
      parsed_log_.GetRtpHeader(i, &direction, &media_type, header,
                               &header_length, &total_length);
      if (direction == desired_direction) {
        // Parse header to get SSRC.
        RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
        RTPHeader parsed_header;
        rtp_parser.Parse(&parsed_header);
        // Filter on SSRC.
        if (MatchingSsrc(parsed_header.ssrc, desired_ssrc_)) {
          uint64_t timestamp = parsed_log_.GetTimestamp(i);
          float x = static_cast<float>(timestamp - begin_time_) / 1000000;
          float y = total_length;
          time_series[parsed_header.ssrc].points.push_back(
              TimeSeriesPoint(x, y));
        }
      }
    }
  }

  // Set labels and put in graph.
  for (auto& kv : time_series) {
    kv.second.label = SsrcToString(kv.first);
    kv.second.style = BAR_GRAPH;
    plot->series_list_.push_back(std::move(kv.second));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Packet size (bytes)", kBottomMargin,
                          kTopMargin);
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->SetTitle("Incoming RTP packets");
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->SetTitle("Outgoing RTP packets");
  }
}

// For each SSRC, plot the time between the consecutive playouts.
void EventLogAnalyzer::CreatePlayoutGraph(Plot* plot) {
  std::map<uint32_t, TimeSeries> time_series;
  std::map<uint32_t, uint64_t> last_playout;

  uint32_t ssrc;

  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::AUDIO_PLAYOUT_EVENT) {
      parsed_log_.GetAudioPlayout(i, &ssrc);
      uint64_t timestamp = parsed_log_.GetTimestamp(i);
      if (MatchingSsrc(ssrc, desired_ssrc_)) {
        float x = static_cast<float>(timestamp - begin_time_) / 1000000;
        float y = static_cast<float>(timestamp - last_playout[ssrc]) / 1000;
        if (time_series[ssrc].points.size() == 0) {
          // There were no previusly logged playout for this SSRC.
          // Generate a point, but place it on the x-axis.
          y = 0;
        }
        time_series[ssrc].points.push_back(TimeSeriesPoint(x, y));
        last_playout[ssrc] = timestamp;
      }
    }
  }

  // Set labels and put in graph.
  for (auto& kv : time_series) {
    kv.second.label = SsrcToString(kv.first);
    kv.second.style = BAR_GRAPH;
    plot->series_list_.push_back(std::move(kv.second));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Time since last playout (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Audio playout");
}

// For each SSRC, plot the time between the consecutive playouts.
void EventLogAnalyzer::CreateSequenceNumberGraph(Plot* plot) {
  std::map<uint32_t, TimeSeries> time_series;
  std::map<uint32_t, uint16_t> last_seqno;

  PacketDirection direction;
  MediaType media_type;
  uint8_t header[IP_PACKET_SIZE];
  size_t header_length, total_length;

  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::RTP_EVENT) {
      parsed_log_.GetRtpHeader(i, &direction, &media_type, header,
                               &header_length, &total_length);
      uint64_t timestamp = parsed_log_.GetTimestamp(i);
      if (direction == PacketDirection::kIncomingPacket) {
        // Parse header to get SSRC.
        RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
        RTPHeader parsed_header;
        rtp_parser.Parse(&parsed_header);
        // Filter on SSRC.
        if (MatchingSsrc(parsed_header.ssrc, desired_ssrc_)) {
          float x = static_cast<float>(timestamp - begin_time_) / 1000000;
          int y = WrappingDifference(parsed_header.sequenceNumber,
                                     last_seqno[parsed_header.ssrc], 1ul << 16);
          if (time_series[parsed_header.ssrc].points.size() == 0) {
            // There were no previusly logged playout for this SSRC.
            // Generate a point, but place it on the x-axis.
            y = 0;
          }
          time_series[parsed_header.ssrc].points.push_back(
              TimeSeriesPoint(x, y));
          last_seqno[parsed_header.ssrc] = parsed_header.sequenceNumber;
        }
      }
    }
  }

  // Set labels and put in graph.
  for (auto& kv : time_series) {
    kv.second.label = SsrcToString(kv.first);
    kv.second.style = BAR_GRAPH;
    plot->series_list_.push_back(std::move(kv.second));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Difference since last packet", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Sequence number");
}

void EventLogAnalyzer::CreateDelayChangeGraph(Plot* plot) {
  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    uint32_t ssrc = stream_id.GetSsrc();
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != kIncomingPacket ||
        !MatchingSsrc(ssrc, desired_ssrc_) || IsAudioSsrc(stream_id) ||
        !IsVideoSsrc(stream_id) || IsRtxSsrc(stream_id)) {
      continue;
    }

    TimeSeries capture_time_data;
    capture_time_data.label = SsrcToString(ssrc) + " capture-time";
    capture_time_data.style = BAR_GRAPH;
    Pairwise<NetworkDelayDiff::CaptureTime>(packet_stream, begin_time_,
                                            &capture_time_data);
    plot->series_list_.push_back(std::move(capture_time_data));

    TimeSeries send_time_data;
    send_time_data.label = SsrcToString(ssrc) + " abs-send-time";
    send_time_data.style = BAR_GRAPH;
    Pairwise<NetworkDelayDiff::AbsSendTime>(packet_stream, begin_time_,
                                            &send_time_data);
    plot->series_list_.push_back(std::move(send_time_data));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Latency change (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Network latency change between consecutive packets");
}

void EventLogAnalyzer::CreateAccumulatedDelayChangeGraph(Plot* plot) {
  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    uint32_t ssrc = stream_id.GetSsrc();
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != kIncomingPacket ||
        !MatchingSsrc(ssrc, desired_ssrc_) || IsAudioSsrc(stream_id) ||
        !IsVideoSsrc(stream_id) || IsRtxSsrc(stream_id)) {
      continue;
    }

    TimeSeries capture_time_data;
    capture_time_data.label = SsrcToString(ssrc) + " capture-time";
    capture_time_data.style = LINE_GRAPH;
    Pairwise<Accumulated<NetworkDelayDiff::CaptureTime>>(
        packet_stream, begin_time_, &capture_time_data);
    plot->series_list_.push_back(std::move(capture_time_data));

    TimeSeries send_time_data;
    send_time_data.label = SsrcToString(ssrc) + " abs-send-time";
    send_time_data.style = LINE_GRAPH;
    Pairwise<Accumulated<NetworkDelayDiff::AbsSendTime>>(
        packet_stream, begin_time_, &send_time_data);
    plot->series_list_.push_back(std::move(send_time_data));
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Latency change (ms)", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Accumulated network latency change");
}

// Plot the fraction of packets lost (as perceived by the loss-based BWE).
void EventLogAnalyzer::CreateFractionLossGraph(Plot* plot) {
  plot->series_list_.push_back(TimeSeries());
  for (auto& bwe_update : bwe_loss_updates_) {
    float x = static_cast<float>(bwe_update.timestamp - begin_time_) / 1000000;
    float y = static_cast<float>(bwe_update.fraction_loss) / 255 * 100;
    plot->series_list_.back().points.emplace_back(x, y);
  }
  plot->series_list_.back().label = "Fraction lost";
  plot->series_list_.back().style = LINE_DOT_GRAPH;

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Percent lost packets", kBottomMargin,
                          kTopMargin);
  plot->SetTitle("Reported packet loss");
}

// Plot the total bandwidth used by all RTP streams.
void EventLogAnalyzer::CreateTotalBitrateGraph(
    PacketDirection desired_direction,
    Plot* plot) {
  struct TimestampSize {
    TimestampSize(uint64_t t, size_t s) : timestamp(t), size(s) {}
    uint64_t timestamp;
    size_t size;
  };
  std::vector<TimestampSize> packets;

  PacketDirection direction;
  size_t total_length;

  // Extract timestamps and sizes for the relevant packets.
  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::RTP_EVENT) {
      parsed_log_.GetRtpHeader(i, &direction, nullptr, nullptr, nullptr,
                               &total_length);
      if (direction == desired_direction) {
        uint64_t timestamp = parsed_log_.GetTimestamp(i);
        packets.push_back(TimestampSize(timestamp, total_length));
      }
    }
  }

  size_t window_index_begin = 0;
  size_t window_index_end = 0;
  size_t bytes_in_window = 0;

  // Calculate a moving average of the bitrate and store in a TimeSeries.
  plot->series_list_.push_back(TimeSeries());
  for (uint64_t time = begin_time_; time < end_time_ + step_; time += step_) {
    while (window_index_end < packets.size() &&
           packets[window_index_end].timestamp < time) {
      bytes_in_window += packets[window_index_end].size;
      window_index_end++;
    }
    while (window_index_begin < packets.size() &&
           packets[window_index_begin].timestamp < time - window_duration_) {
      RTC_DCHECK_LE(packets[window_index_begin].size, bytes_in_window);
      bytes_in_window -= packets[window_index_begin].size;
      window_index_begin++;
    }
    float window_duration_in_seconds =
        static_cast<float>(window_duration_) / 1000000;
    float x = static_cast<float>(time - begin_time_) / 1000000;
    float y = bytes_in_window * 8 / window_duration_in_seconds / 1000;
    plot->series_list_.back().points.push_back(TimeSeriesPoint(x, y));
  }

  // Set labels.
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->series_list_.back().label = "Incoming bitrate";
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->series_list_.back().label = "Outgoing bitrate";
  }
  plot->series_list_.back().style = LINE_GRAPH;

  // Overlay the send-side bandwidth estimate over the outgoing bitrate.
  if (desired_direction == kOutgoingPacket) {
    plot->series_list_.push_back(TimeSeries());
    for (auto& bwe_update : bwe_loss_updates_) {
      float x =
          static_cast<float>(bwe_update.timestamp - begin_time_) / 1000000;
      float y = static_cast<float>(bwe_update.new_bitrate) / 1000;
      plot->series_list_.back().points.emplace_back(x, y);
    }
    plot->series_list_.back().label = "Loss-based estimate";
    plot->series_list_.back().style = LINE_GRAPH;
  }
  plot->series_list_.back().style = LINE_GRAPH;
  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->SetTitle("Incoming RTP bitrate");
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->SetTitle("Outgoing RTP bitrate");
  }
}

// For each SSRC, plot the bandwidth used by that stream.
void EventLogAnalyzer::CreateStreamBitrateGraph(
    PacketDirection desired_direction,
    Plot* plot) {
  struct TimestampSize {
    TimestampSize(uint64_t t, size_t s) : timestamp(t), size(s) {}
    uint64_t timestamp;
    size_t size;
  };
  std::map<uint32_t, std::vector<TimestampSize>> packets;

  PacketDirection direction;
  MediaType media_type;
  uint8_t header[IP_PACKET_SIZE];
  size_t header_length, total_length;

  // Extract timestamps and sizes for the relevant packets.
  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::RTP_EVENT) {
      parsed_log_.GetRtpHeader(i, &direction, &media_type, header,
                               &header_length, &total_length);
      if (direction == desired_direction) {
        // Parse header to get SSRC.
        RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
        RTPHeader parsed_header;
        rtp_parser.Parse(&parsed_header);
        // Filter on SSRC.
        if (MatchingSsrc(parsed_header.ssrc, desired_ssrc_)) {
          uint64_t timestamp = parsed_log_.GetTimestamp(i);
          packets[parsed_header.ssrc].push_back(
              TimestampSize(timestamp, total_length));
        }
      }
    }
  }

  for (auto& kv : packets) {
    size_t window_index_begin = 0;
    size_t window_index_end = 0;
    size_t bytes_in_window = 0;

    // Calculate a moving average of the bitrate and store in a TimeSeries.
    plot->series_list_.push_back(TimeSeries());
    for (uint64_t time = begin_time_; time < end_time_ + step_; time += step_) {
      while (window_index_end < kv.second.size() &&
             kv.second[window_index_end].timestamp < time) {
        bytes_in_window += kv.second[window_index_end].size;
        window_index_end++;
      }
      while (window_index_begin < kv.second.size() &&
             kv.second[window_index_begin].timestamp <
                 time - window_duration_) {
        RTC_DCHECK_LE(kv.second[window_index_begin].size, bytes_in_window);
        bytes_in_window -= kv.second[window_index_begin].size;
        window_index_begin++;
      }
      float window_duration_in_seconds =
          static_cast<float>(window_duration_) / 1000000;
      float x = static_cast<float>(time - begin_time_) / 1000000;
      float y = bytes_in_window * 8 / window_duration_in_seconds / 1000;
      plot->series_list_.back().points.push_back(TimeSeriesPoint(x, y));
    }

    // Set labels.
    plot->series_list_.back().label = SsrcToString(kv.first);
    plot->series_list_.back().style = LINE_GRAPH;
  }

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 1, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->SetTitle("Incoming bitrate per stream");
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->SetTitle("Outgoing bitrate per stream");
  }
}

void EventLogAnalyzer::CreateBweSimulationGraph(Plot* plot) {
  std::map<uint64_t, const LoggedRtpPacket*> outgoing_rtp;
  std::map<uint64_t, const LoggedRtcpPacket*> incoming_rtcp;

  for (const auto& kv : rtp_packets_) {
    if (kv.first.GetDirection() == PacketDirection::kOutgoingPacket) {
      for (const LoggedRtpPacket& rtp_packet : kv.second)
        outgoing_rtp.insert(std::make_pair(rtp_packet.timestamp, &rtp_packet));
    }
  }

  for (const auto& kv : rtcp_packets_) {
    if (kv.first.GetDirection() == PacketDirection::kIncomingPacket) {
      for (const LoggedRtcpPacket& rtcp_packet : kv.second)
        incoming_rtcp.insert(
            std::make_pair(rtcp_packet.timestamp, &rtcp_packet));
    }
  }

  SimulatedClock clock(0);
  BitrateObserver observer;
  RtcEventLogNullImpl null_event_log;
  CongestionController cc(&clock, &observer, &observer, &null_event_log);
  // TODO(holmer): Log the call config and use that here instead.
  static const uint32_t kDefaultStartBitrateBps = 300000;
  cc.SetBweBitrates(0, kDefaultStartBitrateBps, -1);

  TimeSeries time_series;
  time_series.label = "Delay-based estimate";
  time_series.style = LINE_DOT_GRAPH;

  auto rtp_iterator = outgoing_rtp.begin();
  auto rtcp_iterator = incoming_rtcp.begin();

  auto NextRtpTime = [&]() {
    if (rtp_iterator != outgoing_rtp.end())
      return static_cast<int64_t>(rtp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  auto NextRtcpTime = [&]() {
    if (rtcp_iterator != incoming_rtcp.end())
      return static_cast<int64_t>(rtcp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  auto NextProcessTime = [&]() {
    if (rtcp_iterator != incoming_rtcp.end() ||
        rtp_iterator != outgoing_rtp.end()) {
      return clock.TimeInMicroseconds() +
             std::max<int64_t>(cc.TimeUntilNextProcess() * 1000, 0);
    }
    return std::numeric_limits<int64_t>::max();
  };

  int64_t time_us = std::min(NextRtpTime(), NextRtcpTime());
  while (time_us != std::numeric_limits<int64_t>::max()) {
    clock.AdvanceTimeMicroseconds(time_us - clock.TimeInMicroseconds());
    if (clock.TimeInMicroseconds() >= NextRtcpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtcpTime());
      const LoggedRtcpPacket& rtcp = *rtcp_iterator->second;
      if (rtcp.type == kRtcpTransportFeedback) {
        cc.GetTransportFeedbackObserver()->OnTransportFeedback(
            *static_cast<rtcp::TransportFeedback*>(rtcp.packet.get()));
      }
      ++rtcp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextRtpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtpTime());
      const LoggedRtpPacket& rtp = *rtp_iterator->second;
      if (rtp.header.extension.hasTransportSequenceNumber) {
        RTC_DCHECK(rtp.header.extension.hasTransportSequenceNumber);
        cc.GetTransportFeedbackObserver()->AddPacket(
            rtp.header.extension.transportSequenceNumber, rtp.total_length,
            PacketInfo::kNotAProbe);
        rtc::SentPacket sent_packet(
            rtp.header.extension.transportSequenceNumber, rtp.timestamp / 1000);
        cc.OnSentPacket(sent_packet);
      }
      ++rtp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextProcessTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextProcessTime());
      cc.Process();
    }
    if (observer.GetAndResetBitrateUpdated()) {
      uint32_t y = observer.last_bitrate_bps() / 1000;
      float x = static_cast<float>(clock.TimeInMicroseconds() - begin_time_) /
                1000000;
      time_series.points.emplace_back(x, y);
    }
    time_us = std::min({NextRtpTime(), NextRtcpTime(), NextProcessTime()});
  }
  // Add the data set to the plot.
  plot->series_list_.push_back(std::move(time_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Bitrate (kbps)", kBottomMargin, kTopMargin);
  plot->SetTitle("Simulated BWE behavior");
}

void EventLogAnalyzer::CreateNetworkDelayFeedbackGraph(Plot* plot) {
  std::map<uint64_t, const LoggedRtpPacket*> outgoing_rtp;
  std::map<uint64_t, const LoggedRtcpPacket*> incoming_rtcp;

  for (const auto& kv : rtp_packets_) {
    if (kv.first.GetDirection() == PacketDirection::kOutgoingPacket) {
      for (const LoggedRtpPacket& rtp_packet : kv.second)
        outgoing_rtp.insert(std::make_pair(rtp_packet.timestamp, &rtp_packet));
    }
  }

  for (const auto& kv : rtcp_packets_) {
    if (kv.first.GetDirection() == PacketDirection::kIncomingPacket) {
      for (const LoggedRtcpPacket& rtcp_packet : kv.second)
        incoming_rtcp.insert(
            std::make_pair(rtcp_packet.timestamp, &rtcp_packet));
    }
  }

  SimulatedClock clock(0);
  TransportFeedbackAdapter feedback_adapter(nullptr, &clock);

  TimeSeries time_series;
  time_series.label = "Network Delay Change";
  time_series.style = LINE_DOT_GRAPH;
  int64_t estimated_base_delay_ms = std::numeric_limits<int64_t>::max();

  auto rtp_iterator = outgoing_rtp.begin();
  auto rtcp_iterator = incoming_rtcp.begin();

  auto NextRtpTime = [&]() {
    if (rtp_iterator != outgoing_rtp.end())
      return static_cast<int64_t>(rtp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  auto NextRtcpTime = [&]() {
    if (rtcp_iterator != incoming_rtcp.end())
      return static_cast<int64_t>(rtcp_iterator->first);
    return std::numeric_limits<int64_t>::max();
  };

  int64_t time_us = std::min(NextRtpTime(), NextRtcpTime());
  while (time_us != std::numeric_limits<int64_t>::max()) {
    clock.AdvanceTimeMicroseconds(time_us - clock.TimeInMicroseconds());
    if (clock.TimeInMicroseconds() >= NextRtcpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtcpTime());
      const LoggedRtcpPacket& rtcp = *rtcp_iterator->second;
      if (rtcp.type == kRtcpTransportFeedback) {
        std::vector<PacketInfo> feedback =
            feedback_adapter.GetPacketFeedbackVector(
                *static_cast<rtcp::TransportFeedback*>(rtcp.packet.get()));
        for (const PacketInfo& packet : feedback) {
          int64_t y = packet.arrival_time_ms - packet.send_time_ms;
          float x =
              static_cast<float>(clock.TimeInMicroseconds() - begin_time_) /
              1000000;
          estimated_base_delay_ms = std::min(y, estimated_base_delay_ms);
          time_series.points.emplace_back(x, y);
        }
      }
      ++rtcp_iterator;
    }
    if (clock.TimeInMicroseconds() >= NextRtpTime()) {
      RTC_DCHECK_EQ(clock.TimeInMicroseconds(), NextRtpTime());
      const LoggedRtpPacket& rtp = *rtp_iterator->second;
      if (rtp.header.extension.hasTransportSequenceNumber) {
        RTC_DCHECK(rtp.header.extension.hasTransportSequenceNumber);
        feedback_adapter.AddPacket(rtp.header.extension.transportSequenceNumber,
                                   rtp.total_length, 0);
        feedback_adapter.OnSentPacket(
            rtp.header.extension.transportSequenceNumber, rtp.timestamp / 1000);
      }
      ++rtp_iterator;
    }
    time_us = std::min(NextRtpTime(), NextRtcpTime());
  }
  // We assume that the base network delay (w/o queues) is the min delay
  // observed during the call.
  for (TimeSeriesPoint& point : time_series.points)
    point.y -= estimated_base_delay_ms;
  // Add the data set to the plot.
  plot->series_list_.push_back(std::move(time_series));

  plot->SetXAxis(0, call_duration_s_, "Time (s)", kLeftMargin, kRightMargin);
  plot->SetSuggestedYAxis(0, 10, "Delay (ms)", kBottomMargin, kTopMargin);
  plot->SetTitle("Network Delay Change.");
}
}  // namespace plotting
}  // namespace webrtc

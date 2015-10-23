/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_channel.h"

#include <algorithm>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/common.h"
#include "webrtc/common_video/interface/incoming_video_stream.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/pacing/include/paced_sender.h"
#include "webrtc/modules/pacing/include/packet_router.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/metrics.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/video/receive_statistics_proxy.h"
#include "webrtc/video_engine/call_stats.h"
#include "webrtc/video_engine/payload_router.h"
#include "webrtc/video_engine/report_block_stats.h"
#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {

const int kMaxDecodeWaitTimeMs = 50;
static const int kMaxTargetDelayMs = 10000;
static const float kMaxIncompleteTimeMultiplier = 3.5f;

// Helper class receiving statistics callbacks.
class ChannelStatsObserver : public CallStatsObserver {
 public:
  explicit ChannelStatsObserver(ViEChannel* owner) : owner_(owner) {}
  virtual ~ChannelStatsObserver() {}

  // Implements StatsObserver.
  virtual void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
    owner_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
  }

 private:
  ViEChannel* const owner_;
};

class ViEChannelProtectionCallback : public VCMProtectionCallback {
 public:
  ViEChannelProtectionCallback(ViEChannel* owner) : owner_(owner) {}
  ~ViEChannelProtectionCallback() {}


  int ProtectionRequest(
      const FecProtectionParams* delta_fec_params,
      const FecProtectionParams* key_fec_params,
      uint32_t* sent_video_rate_bps,
      uint32_t* sent_nack_rate_bps,
      uint32_t* sent_fec_rate_bps) override {
    return owner_->ProtectionRequest(delta_fec_params, key_fec_params,
                                     sent_video_rate_bps, sent_nack_rate_bps,
                                     sent_fec_rate_bps);
  }
 private:
  ViEChannel* owner_;
};

ViEChannel::ViEChannel(int32_t channel_id,
                       int32_t engine_id,
                       uint32_t number_of_cores,
                       Transport* transport,
                       ProcessThread* module_process_thread,
                       RtcpIntraFrameObserver* intra_frame_observer,
                       RtcpBandwidthObserver* bandwidth_observer,
                       SendTimeObserver* send_time_observer,
                       RemoteBitrateEstimator* remote_bitrate_estimator,
                       RtcpRttStats* rtt_stats,
                       PacedSender* paced_sender,
                       PacketRouter* packet_router,
                       size_t max_rtp_streams,
                       bool sender)
    : channel_id_(channel_id),
      engine_id_(engine_id),
      number_of_cores_(number_of_cores),
      sender_(sender),
      module_process_thread_(module_process_thread),
      crit_(CriticalSectionWrapper::CreateCriticalSection()),
      send_payload_router_(new PayloadRouter()),
      vcm_protection_callback_(new ViEChannelProtectionCallback(this)),
      vcm_(VideoCodingModule::Create(Clock::GetRealTimeClock(),
                                     nullptr,
                                     nullptr)),
      vie_receiver_(channel_id, vcm_, remote_bitrate_estimator, this),
      vie_sync_(vcm_),
      stats_observer_(new ChannelStatsObserver(this)),
      vcm_receive_stats_callback_(NULL),
      incoming_video_stream_(nullptr),
      codec_observer_(NULL),
      intra_frame_observer_(intra_frame_observer),
      rtt_stats_(rtt_stats),
      paced_sender_(paced_sender),
      packet_router_(packet_router),
      bandwidth_observer_(bandwidth_observer),
      send_time_observer_(send_time_observer),
      decoder_reset_(true),
      nack_history_size_sender_(kSendSidePacketHistorySize),
      max_nack_reordering_threshold_(kMaxPacketAgeToNack),
      pre_render_callback_(NULL),
      report_block_stats_sender_(new ReportBlockStats()),
      time_of_first_rtt_ms_(-1),
      rtt_sum_ms_(0),
      num_rtts_(0),
      rtp_rtcp_modules_(
          CreateRtpRtcpModules(ViEModuleId(engine_id_, channel_id_),
                               !sender,
                               vie_receiver_.GetReceiveStatistics(),
                               transport,
                               sender ? intra_frame_observer_ : nullptr,
                               sender ? bandwidth_observer_.get() : nullptr,
                               sender ? send_time_observer_ : nullptr,
                               rtt_stats_,
                               &rtcp_packet_type_counter_observer_,
                               remote_bitrate_estimator,
                               paced_sender_,
                               sender_ ? packet_router_ : nullptr,
                               &send_bitrate_observer_,
                               &send_frame_count_observer_,
                               &send_side_delay_observer_,
                               max_rtp_streams)),
      num_active_rtp_rtcp_modules_(1) {
  vie_receiver_.SetRtpRtcpModule(rtp_rtcp_modules_[0]);
  vcm_->SetNackSettings(kMaxNackListSize, max_nack_reordering_threshold_, 0);
}

int32_t ViEChannel::Init() {
  module_process_thread_->RegisterModule(vie_receiver_.GetReceiveStatistics());

  // RTP/RTCP initialization.
  module_process_thread_->RegisterModule(rtp_rtcp_modules_[0]);

  rtp_rtcp_modules_[0]->SetKeyFrameRequestMethod(kKeyFrameReqFirRtp);
  if (paced_sender_) {
    for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
      rtp_rtcp->SetStorePacketsStatus(true, nack_history_size_sender_);
  }
  if (sender_) {
    packet_router_->AddRtpModule(rtp_rtcp_modules_[0]);
    std::list<RtpRtcp*> send_rtp_modules(1, rtp_rtcp_modules_[0]);
    send_payload_router_->SetSendingRtpModules(send_rtp_modules);
    DCHECK(!send_payload_router_->active());
  }
  if (vcm_->RegisterReceiveCallback(this) != 0) {
    return -1;
  }
  vcm_->RegisterFrameTypeCallback(this);
  vcm_->RegisterReceiveStatisticsCallback(this);
  vcm_->RegisterDecoderTimingCallback(this);
  vcm_->SetRenderDelay(kViEDefaultRenderDelayMs);

  module_process_thread_->RegisterModule(vcm_);
  module_process_thread_->RegisterModule(&vie_sync_);

  return 0;
}

ViEChannel::~ViEChannel() {
  UpdateHistograms();
  // Make sure we don't get more callbacks from the RTP module.
  module_process_thread_->DeRegisterModule(
      vie_receiver_.GetReceiveStatistics());
  module_process_thread_->DeRegisterModule(vcm_);
  module_process_thread_->DeRegisterModule(&vie_sync_);
  send_payload_router_->SetSendingRtpModules(std::list<RtpRtcp*>());
  if (sender_ && packet_router_) {
    for (size_t i = 0; i < num_active_rtp_rtcp_modules_; ++i)
      packet_router_->RemoveRtpModule(rtp_rtcp_modules_[i]);
  }
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    module_process_thread_->DeRegisterModule(rtp_rtcp);
    delete rtp_rtcp;
  }
  if (decode_thread_) {
    StopDecodeThread();
  }
  // Release modules.
  VideoCodingModule::Destroy(vcm_);
}

void ViEChannel::UpdateHistograms() {
  int64_t now = Clock::GetRealTimeClock()->TimeInMilliseconds();

  {
    CriticalSectionScoped cs(crit_.get());
    int64_t elapsed_sec = (now - time_of_first_rtt_ms_) / 1000;
    if (time_of_first_rtt_ms_ != -1 && num_rtts_ > 0 &&
        elapsed_sec > metrics::kMinRunTimeInSeconds) {
      int64_t avg_rtt_ms = (rtt_sum_ms_ + num_rtts_ / 2) / num_rtts_;
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.AverageRoundTripTimeInMilliseconds", avg_rtt_ms);
    }
  }

  if (sender_) {
    RtcpPacketTypeCounter rtcp_counter;
    GetSendRtcpPacketTypeCounter(&rtcp_counter);
    int64_t elapsed_sec = rtcp_counter.TimeSinceFirstPacketInMs(now) / 1000;
    if (elapsed_sec > metrics::kMinRunTimeInSeconds) {
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.NackPacketsReceivedPerMinute",
                                 rtcp_counter.nack_packets * 60 / elapsed_sec);
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.FirPacketsReceivedPerMinute",
                                 rtcp_counter.fir_packets * 60 / elapsed_sec);
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.PliPacketsReceivedPerMinute",
                                 rtcp_counter.pli_packets * 60 / elapsed_sec);
      if (rtcp_counter.nack_requests > 0) {
        RTC_HISTOGRAM_PERCENTAGE(
            "WebRTC.Video.UniqueNackRequestsReceivedInPercent",
            rtcp_counter.UniqueNackRequestsInPercent());
      }
      int fraction_lost = report_block_stats_sender_->FractionLostInPercent();
      if (fraction_lost != -1) {
        RTC_HISTOGRAM_PERCENTAGE("WebRTC.Video.SentPacketsLostInPercent",
                                 fraction_lost);
      }
    }

    StreamDataCounters rtp;
    StreamDataCounters rtx;
    GetSendStreamDataCounters(&rtp, &rtx);
    StreamDataCounters rtp_rtx = rtp;
    rtp_rtx.Add(rtx);
    elapsed_sec = rtp_rtx.TimeSinceFirstPacketInMs(
                      Clock::GetRealTimeClock()->TimeInMilliseconds()) /
                  1000;
    if (elapsed_sec > metrics::kMinRunTimeInSeconds) {
      RTC_HISTOGRAM_COUNTS_100000(
          "WebRTC.Video.BitrateSentInKbps",
          static_cast<int>(rtp_rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.MediaBitrateSentInKbps",
          static_cast<int>(rtp.MediaPayloadBytes() * 8 / elapsed_sec / 1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.PaddingBitrateSentInKbps",
          static_cast<int>(rtp_rtx.transmitted.padding_bytes * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.RetransmittedBitrateSentInKbps",
          static_cast<int>(rtp_rtx.retransmitted.TotalBytes() * 8 /
                           elapsed_sec / 1000));
      if (rtp_rtcp_modules_[0]->RtxSendStatus() != kRtxOff) {
        RTC_HISTOGRAM_COUNTS_10000(
            "WebRTC.Video.RtxBitrateSentInKbps",
            static_cast<int>(rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                             1000));
      }
      bool fec_enabled = false;
      uint8_t pltype_red;
      uint8_t pltype_fec;
      rtp_rtcp_modules_[0]->GenericFECStatus(fec_enabled, pltype_red,
                                             pltype_fec);
      if (fec_enabled) {
        RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.FecBitrateSentInKbps",
                                   static_cast<int>(rtp_rtx.fec.TotalBytes() *
                                                    8 / elapsed_sec / 1000));
      }
    }
  } else if (vie_receiver_.GetRemoteSsrc() > 0) {
    // Get receive stats if we are receiving packets, i.e. there is a remote
    // ssrc.
    RtcpPacketTypeCounter rtcp_counter;
    GetReceiveRtcpPacketTypeCounter(&rtcp_counter);
    int64_t elapsed_sec = rtcp_counter.TimeSinceFirstPacketInMs(now) / 1000;
    if (elapsed_sec > metrics::kMinRunTimeInSeconds) {
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.NackPacketsSentPerMinute",
          rtcp_counter.nack_packets * 60 / elapsed_sec);
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.FirPacketsSentPerMinute",
          rtcp_counter.fir_packets * 60 / elapsed_sec);
      RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.PliPacketsSentPerMinute",
          rtcp_counter.pli_packets * 60 / elapsed_sec);
      if (rtcp_counter.nack_requests > 0) {
        RTC_HISTOGRAM_PERCENTAGE("WebRTC.Video.UniqueNackRequestsSentInPercent",
            rtcp_counter.UniqueNackRequestsInPercent());
      }
    }

    StreamDataCounters rtp;
    StreamDataCounters rtx;
    GetReceiveStreamDataCounters(&rtp, &rtx);
    StreamDataCounters rtp_rtx = rtp;
    rtp_rtx.Add(rtx);
    elapsed_sec = rtp_rtx.TimeSinceFirstPacketInMs(now) / 1000;
    if (elapsed_sec > metrics::kMinRunTimeInSeconds) {
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.BitrateReceivedInKbps",
          static_cast<int>(rtp_rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.MediaBitrateReceivedInKbps",
          static_cast<int>(rtp.MediaPayloadBytes() * 8 / elapsed_sec / 1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.PaddingBitrateReceivedInKbps",
          static_cast<int>(rtp_rtx.transmitted.padding_bytes * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.RetransmittedBitrateReceivedInKbps",
          static_cast<int>(rtp_rtx.retransmitted.TotalBytes() * 8 /
                           elapsed_sec / 1000));
      uint32_t ssrc = 0;
      if (vie_receiver_.GetRtxSsrc(&ssrc)) {
        RTC_HISTOGRAM_COUNTS_10000(
            "WebRTC.Video.RtxBitrateReceivedInKbps",
            static_cast<int>(rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                             1000));
      }
      if (vie_receiver_.IsFecEnabled()) {
        RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.FecBitrateReceivedInKbps",
                                   static_cast<int>(rtp_rtx.fec.TotalBytes() *
                                                    8 / elapsed_sec / 1000));
      }
    }
  }
}

int32_t ViEChannel::SetSendCodec(const VideoCodec& video_codec,
                                 bool new_stream) {
  DCHECK(sender_);
  if (video_codec.codecType == kVideoCodecRED ||
      video_codec.codecType == kVideoCodecULPFEC) {
    LOG_F(LS_ERROR) << "Not a valid send codec " << video_codec.codecType;
    return -1;
  }
  if (kMaxSimulcastStreams < video_codec.numberOfSimulcastStreams) {
    LOG_F(LS_ERROR) << "Incorrect config "
                    << video_codec.numberOfSimulcastStreams;
    return -1;
  }
  // Update the RTP module with the settings.
  // Stop and Start the RTP module -> trigger new SSRC, if an SSRC hasn't been
  // set explicitly.
  // The first layer is always active, so the first module can be checked for
  // sending status.
  bool is_sending = rtp_rtcp_modules_[0]->Sending();
  bool router_was_active = send_payload_router_->active();
  send_payload_router_->set_active(false);
  send_payload_router_->SetSendingRtpModules(std::list<RtpRtcp*>());

  std::vector<RtpRtcp*> registered_modules;
  std::vector<RtpRtcp*> deregistered_modules;
  size_t num_active_modules = video_codec.numberOfSimulcastStreams > 0
                                   ? video_codec.numberOfSimulcastStreams
                                   : 1;
  size_t num_prev_active_modules;
  {
    // Cache which modules are active so StartSend can know which ones to start.
    CriticalSectionScoped cs(crit_.get());
    num_prev_active_modules = num_active_rtp_rtcp_modules_;
    num_active_rtp_rtcp_modules_ = num_active_modules;
  }
  for (size_t i = 0; i < num_active_modules; ++i)
    registered_modules.push_back(rtp_rtcp_modules_[i]);

  for (size_t i = num_active_modules; i < rtp_rtcp_modules_.size(); ++i)
    deregistered_modules.push_back(rtp_rtcp_modules_[i]);

  // Disable inactive modules.
  for (RtpRtcp* rtp_rtcp : deregistered_modules) {
    rtp_rtcp->SetSendingStatus(false);
    rtp_rtcp->SetSendingMediaStatus(false);
  }

  // Configure active modules.
  for (RtpRtcp* rtp_rtcp : registered_modules) {
    rtp_rtcp->DeRegisterSendPayload(video_codec.plType);
    if (rtp_rtcp->RegisterSendPayload(video_codec) != 0) {
      return -1;
    }
    rtp_rtcp->SetSendingStatus(is_sending);
    rtp_rtcp->SetSendingMediaStatus(is_sending);
  }

  // |RegisterSimulcastRtpRtcpModules| resets all old weak pointers and old
  // modules can be deleted after this step.
  vie_receiver_.RegisterRtpRtcpModules(registered_modules);

  // Update the packet and payload routers with the sending RtpRtcp modules.
  if (sender_) {
    std::list<RtpRtcp*> active_send_modules;
    for (RtpRtcp* rtp_rtcp : registered_modules)
      active_send_modules.push_back(rtp_rtcp);
    send_payload_router_->SetSendingRtpModules(active_send_modules);
  }

  if (router_was_active)
    send_payload_router_->set_active(true);

  // Deregister previously registered modules.
  for (size_t i = num_active_modules; i < num_prev_active_modules; ++i) {
    module_process_thread_->DeRegisterModule(rtp_rtcp_modules_[i]);
    if (sender_ && packet_router_)
      packet_router_->RemoveRtpModule(rtp_rtcp_modules_[i]);
  }
  // Register new active modules.
  for (size_t i = num_prev_active_modules; i < num_active_modules; ++i) {
    module_process_thread_->RegisterModule(rtp_rtcp_modules_[i]);
    if (sender_ && packet_router_)
      packet_router_->AddRtpModule(rtp_rtcp_modules_[i]);
  }
  return 0;
}

int32_t ViEChannel::SetReceiveCodec(const VideoCodec& video_codec) {
  DCHECK(!sender_);
  if (!vie_receiver_.SetReceiveCodec(video_codec)) {
    return -1;
  }

  if (video_codec.codecType != kVideoCodecRED &&
      video_codec.codecType != kVideoCodecULPFEC) {
    // Register codec type with VCM, but do not register RED or ULPFEC.
    if (vcm_->RegisterReceiveCodec(&video_codec, number_of_cores_, false) !=
        VCM_OK) {
      return -1;
    }
  }
  return 0;
}

int32_t ViEChannel::RegisterCodecObserver(ViEDecoderObserver* observer) {
  CriticalSectionScoped cs(crit_.get());
  if (observer) {
    if (codec_observer_) {
      LOG_F(LS_ERROR) << "Observer already registered.";
      return -1;
    }
    codec_observer_ = observer;
  } else {
    codec_observer_ = NULL;
  }
  return 0;
}

int32_t ViEChannel::RegisterExternalDecoder(const uint8_t pl_type,
                                            VideoDecoder* decoder,
                                            bool buffered_rendering,
                                            int32_t render_delay) {
  DCHECK(!sender_);
  int32_t result;
  result = vcm_->RegisterExternalDecoder(decoder, pl_type, buffered_rendering);
  if (result != VCM_OK) {
    return result;
  }
  return vcm_->SetRenderDelay(render_delay);
}

int32_t ViEChannel::DeRegisterExternalDecoder(const uint8_t pl_type) {
  DCHECK(!sender_);
  VideoCodec current_receive_codec;
  int32_t result = 0;
  result = vcm_->ReceiveCodec(&current_receive_codec);
  if (vcm_->RegisterExternalDecoder(NULL, pl_type, false) != VCM_OK) {
    return -1;
  }

  if (result == 0 && current_receive_codec.plType == pl_type) {
    result = vcm_->RegisterReceiveCodec(&current_receive_codec,
                                        number_of_cores_, false);
  }
  return result;
}

int32_t ViEChannel::ReceiveCodecStatistics(uint32_t* num_key_frames,
                                           uint32_t* num_delta_frames) {
  CriticalSectionScoped cs(crit_.get());
  *num_key_frames = receive_frame_counts_.key_frames;
  *num_delta_frames = receive_frame_counts_.delta_frames;
  return 0;
}

uint32_t ViEChannel::DiscardedPackets() const {
  return vcm_->DiscardedPackets();
}

int ViEChannel::ReceiveDelay() const {
  return vcm_->Delay();
}

void ViEChannel::SetRTCPMode(const RTCPMethod rtcp_mode) {
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->SetRTCPStatus(rtcp_mode);
}

void ViEChannel::SetProtectionMode(bool enable_nack,
                                   bool enable_fec,
                                   int payload_type_red,
                                   int payload_type_fec) {
  // Validate payload types.
  if (enable_fec) {
    DCHECK_GE(payload_type_red, 0);
    DCHECK_GE(payload_type_fec, 0);
    DCHECK_LE(payload_type_red, 127);
    DCHECK_LE(payload_type_fec, 127);
  } else {
    DCHECK_EQ(payload_type_red, -1);
    DCHECK_EQ(payload_type_fec, -1);
    // Set to valid uint8_ts to be castable later without signed overflows.
    payload_type_red = 0;
    payload_type_fec = 0;
  }

  VCMVideoProtection protection_method;
  if (enable_nack) {
    protection_method = enable_fec ? kProtectionNackFEC : kProtectionNack;
  } else {
    protection_method = kProtectionNone;
  }

  vcm_->SetVideoProtection(protection_method, true);

  // Set NACK.
  ProcessNACKRequest(enable_nack);

  // Set FEC.
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->SetGenericFECStatus(enable_fec,
                                  static_cast<uint8_t>(payload_type_red),
                                  static_cast<uint8_t>(payload_type_fec));
  }
}

void ViEChannel::ProcessNACKRequest(const bool enable) {
  if (enable) {
    // Turn on NACK.
    if (rtp_rtcp_modules_[0]->RTCP() == kRtcpOff)
      return;
    vie_receiver_.SetNackStatus(true, max_nack_reordering_threshold_);

    for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
      rtp_rtcp->SetStorePacketsStatus(true, nack_history_size_sender_);

    vcm_->RegisterPacketRequestCallback(this);
    // Don't introduce errors when NACK is enabled.
    vcm_->SetDecodeErrorMode(kNoErrors);
  } else {
    vcm_->RegisterPacketRequestCallback(NULL);
    if (paced_sender_ == nullptr) {
      for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
        rtp_rtcp->SetStorePacketsStatus(false, 0);
    }
    vie_receiver_.SetNackStatus(false, max_nack_reordering_threshold_);
    // When NACK is off, allow decoding with errors. Otherwise, the video
    // will freeze, and will only recover with a complete key frame.
    vcm_->SetDecodeErrorMode(kWithErrors);
  }
}

bool ViEChannel::IsSendingFecEnabled() {
  bool fec_enabled = false;
  uint8_t pltype_red = 0;
  uint8_t pltype_fec = 0;

  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->GenericFECStatus(fec_enabled, pltype_red, pltype_fec);
    if (fec_enabled)
      return true;
  }
  return false;
}

int ViEChannel::SetSenderBufferingMode(int target_delay_ms) {
  if ((target_delay_ms < 0) || (target_delay_ms > kMaxTargetDelayMs)) {
    LOG(LS_ERROR) << "Invalid send buffer value.";
    return -1;
  }
  if (target_delay_ms == 0) {
    // Real-time mode.
    nack_history_size_sender_ = kSendSidePacketHistorySize;
  } else {
    nack_history_size_sender_ = GetRequiredNackListSize(target_delay_ms);
    // Don't allow a number lower than the default value.
    if (nack_history_size_sender_ < kSendSidePacketHistorySize) {
      nack_history_size_sender_ = kSendSidePacketHistorySize;
    }
  }
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->SetStorePacketsStatus(true, nack_history_size_sender_);
  return 0;
}

int ViEChannel::SetReceiverBufferingMode(int target_delay_ms) {
  if ((target_delay_ms < 0) || (target_delay_ms > kMaxTargetDelayMs)) {
    LOG(LS_ERROR) << "Invalid receive buffer delay value.";
    return -1;
  }
  int max_nack_list_size;
  int max_incomplete_time_ms;
  if (target_delay_ms == 0) {
    // Real-time mode - restore default settings.
    max_nack_reordering_threshold_ = kMaxPacketAgeToNack;
    max_nack_list_size = kMaxNackListSize;
    max_incomplete_time_ms = 0;
  } else {
    max_nack_list_size =  3 * GetRequiredNackListSize(target_delay_ms) / 4;
    max_nack_reordering_threshold_ = max_nack_list_size;
    // Calculate the max incomplete time and round to int.
    max_incomplete_time_ms = static_cast<int>(kMaxIncompleteTimeMultiplier *
        target_delay_ms + 0.5f);
  }
  vcm_->SetNackSettings(max_nack_list_size, max_nack_reordering_threshold_,
                       max_incomplete_time_ms);
  vcm_->SetMinReceiverDelay(target_delay_ms);
  if (vie_sync_.SetTargetBufferingDelay(target_delay_ms) < 0)
    return -1;
  return 0;
}

int ViEChannel::GetRequiredNackListSize(int target_delay_ms) {
  // The max size of the nack list should be large enough to accommodate the
  // the number of packets (frames) resulting from the increased delay.
  // Roughly estimating for ~40 packets per frame @ 30fps.
  return target_delay_ms * 40 * 30 / 1000;
}

int32_t ViEChannel::SetKeyFrameRequestMethod(
    const KeyFrameRequestMethod method) {
  return rtp_rtcp_modules_[0]->SetKeyFrameRequestMethod(method);
}

void ViEChannel::EnableRemb(bool enable) {
  rtp_rtcp_modules_[0]->SetREMBStatus(enable);
}

int ViEChannel::SetSendTimestampOffsetStatus(bool enable, int id) {
  // Disable any previous registrations of this extension to avoid errors.
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->DeregisterSendRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset);
  }
  if (!enable)
    return 0;
  // Enable the extension.
  int error = 0;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    error |= rtp_rtcp->RegisterSendRtpHeaderExtension(
        kRtpExtensionTransmissionTimeOffset, id);
  }
  return error;
}

int ViEChannel::SetReceiveTimestampOffsetStatus(bool enable, int id) {
  return vie_receiver_.SetReceiveTimestampOffsetStatus(enable, id) ? 0 : -1;
}

int ViEChannel::SetSendAbsoluteSendTimeStatus(bool enable, int id) {
  // Disable any previous registrations of this extension to avoid errors.
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->DeregisterSendRtpHeaderExtension(kRtpExtensionAbsoluteSendTime);
  if (!enable)
    return 0;
  // Enable the extension.
  int error = 0;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    error |= rtp_rtcp->RegisterSendRtpHeaderExtension(
        kRtpExtensionAbsoluteSendTime, id);
  }
  return error;
}

int ViEChannel::SetReceiveAbsoluteSendTimeStatus(bool enable, int id) {
  return vie_receiver_.SetReceiveAbsoluteSendTimeStatus(enable, id) ? 0 : -1;
}

int ViEChannel::SetSendVideoRotationStatus(bool enable, int id) {
  // Disable any previous registrations of this extension to avoid errors.
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->DeregisterSendRtpHeaderExtension(kRtpExtensionVideoRotation);
  if (!enable)
    return 0;
  // Enable the extension.
  int error = 0;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    error |= rtp_rtcp->RegisterSendRtpHeaderExtension(
        kRtpExtensionVideoRotation, id);
  }
  return error;
}

int ViEChannel::SetReceiveVideoRotationStatus(bool enable, int id) {
  return vie_receiver_.SetReceiveVideoRotationStatus(enable, id) ? 0 : -1;
}

int ViEChannel::SetSendTransportSequenceNumber(bool enable, int id) {
  // Disable any previous registrations of this extension to avoid errors.
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->DeregisterSendRtpHeaderExtension(
        kRtpExtensionTransportSequenceNumber);
  }
  if (!enable)
    return 0;
  // Enable the extension.
  int error = 0;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    error |= rtp_rtcp->RegisterSendRtpHeaderExtension(
        kRtpExtensionTransportSequenceNumber, id);
  }
  return error;
}

int ViEChannel::SetReceiveTransportSequenceNumber(bool enable, int id) {
  return vie_receiver_.SetReceiveTransportSequenceNumber(enable, id) ? 0 : -1;
}

void ViEChannel::SetRtcpXrRrtrStatus(bool enable) {
  rtp_rtcp_modules_[0]->SetRtcpXrRrtrStatus(enable);
}

void ViEChannel::SetTransmissionSmoothingStatus(bool enable) {
  DCHECK(paced_sender_ && "No paced sender registered.");
  paced_sender_->SetStatus(enable);
}

void ViEChannel::EnableTMMBR(bool enable) {
  rtp_rtcp_modules_[0]->SetTMMBRStatus(enable);
}

int32_t ViEChannel::SetSSRC(const uint32_t SSRC,
                            const StreamType usage,
                            const uint8_t simulcast_idx) {
  RtpRtcp* rtp_rtcp = rtp_rtcp_modules_[simulcast_idx];
  if (usage == kViEStreamTypeRtx) {
    rtp_rtcp->SetRtxSsrc(SSRC);
  } else {
    rtp_rtcp->SetSSRC(SSRC);
  }
  return 0;
}

int32_t ViEChannel::SetRemoteSSRCType(const StreamType usage,
                                      const uint32_t SSRC) {
  vie_receiver_.SetRtxSsrc(SSRC);
  return 0;
}

int32_t ViEChannel::GetLocalSSRC(uint8_t idx, unsigned int* ssrc) {
  DCHECK_LE(idx, rtp_rtcp_modules_.size());
  *ssrc = rtp_rtcp_modules_[idx]->SSRC();
  return 0;
}

int32_t ViEChannel::GetRemoteSSRC(uint32_t* ssrc) {
  *ssrc = vie_receiver_.GetRemoteSsrc();
  return 0;
}

int ViEChannel::SetRtxSendPayloadType(int payload_type,
                                      int associated_payload_type) {
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->SetRtxSendPayloadType(payload_type, associated_payload_type);
  SetRtxSendStatus(true);
  return 0;
}

void ViEChannel::SetRtxSendStatus(bool enable) {
  int rtx_settings =
      enable ? kRtxRetransmitted | kRtxRedundantPayloads : kRtxOff;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->SetRtxSendStatus(rtx_settings);
}

void ViEChannel::SetRtxReceivePayloadType(int payload_type,
                                          int associated_payload_type) {
  vie_receiver_.SetRtxPayloadType(payload_type, associated_payload_type);
}

void ViEChannel::SetRtpStateForSsrc(uint32_t ssrc, const RtpState& rtp_state) {
  DCHECK(!rtp_rtcp_modules_[0]->Sending());
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    if (rtp_rtcp->SetRtpStateForSsrc(ssrc, rtp_state))
      return;
  }
}

RtpState ViEChannel::GetRtpStateForSsrc(uint32_t ssrc) {
  DCHECK(!rtp_rtcp_modules_[0]->Sending());
  RtpState rtp_state;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    if (rtp_rtcp->GetRtpStateForSsrc(ssrc, &rtp_state))
      return rtp_state;
  }
  LOG(LS_ERROR) << "Couldn't get RTP state for ssrc: " << ssrc;
  return rtp_state;
}

// TODO(pbos): Set CNAME on all modules.
int32_t ViEChannel::SetRTCPCName(const char* rtcp_cname) {
  DCHECK(!rtp_rtcp_modules_[0]->Sending());
  return rtp_rtcp_modules_[0]->SetCNAME(rtcp_cname);
}

int32_t ViEChannel::GetRemoteRTCPCName(char rtcp_cname[]) {
  uint32_t remoteSSRC = vie_receiver_.GetRemoteSsrc();
  return rtp_rtcp_modules_[0]->RemoteCNAME(remoteSSRC, rtcp_cname);
}

int32_t ViEChannel::GetSendRtcpStatistics(uint16_t* fraction_lost,
                                          uint32_t* cumulative_lost,
                                          uint32_t* extended_max,
                                          uint32_t* jitter_samples,
                                          int64_t* rtt_ms) {
  // Aggregate the report blocks associated with streams sent on this channel.
  std::vector<RTCPReportBlock> report_blocks;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->RemoteRTCPStat(&report_blocks);

  if (report_blocks.empty())
    return -1;

  uint32_t remote_ssrc = vie_receiver_.GetRemoteSsrc();
  std::vector<RTCPReportBlock>::const_iterator it = report_blocks.begin();
  for (; it != report_blocks.end(); ++it) {
    if (it->remoteSSRC == remote_ssrc)
      break;
  }
  if (it == report_blocks.end()) {
    // We have not received packets with an SSRC matching the report blocks. To
    // have a chance of calculating an RTT we will try with the SSRC of the
    // first report block received.
    // This is very important for send-only channels where we don't know the
    // SSRC of the other end.
    remote_ssrc = report_blocks[0].remoteSSRC;
  }

  // TODO(asapersson): Change report_block_stats to not rely on
  // GetSendRtcpStatistics to be called.
  RTCPReportBlock report =
      report_block_stats_sender_->AggregateAndStore(report_blocks);
  *fraction_lost = report.fractionLost;
  *cumulative_lost = report.cumulativeLost;
  *extended_max = report.extendedHighSeqNum;
  *jitter_samples = report.jitter;

  int64_t dummy;
  int64_t rtt = 0;
  if (rtp_rtcp_modules_[0]->RTT(remote_ssrc, &rtt, &dummy, &dummy, &dummy) !=
      0) {
    return -1;
  }
  *rtt_ms = rtt;
  return 0;
}

void ViEChannel::RegisterSendChannelRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->RegisterRtcpStatisticsCallback(callback);
}

void ViEChannel::RegisterReceiveChannelRtcpStatisticsCallback(
    RtcpStatisticsCallback* callback) {
  vie_receiver_.GetReceiveStatistics()->RegisterRtcpStatisticsCallback(
      callback);
  rtp_rtcp_modules_[0]->RegisterRtcpStatisticsCallback(callback);
}

void ViEChannel::RegisterRtcpPacketTypeCounterObserver(
    RtcpPacketTypeCounterObserver* observer) {
  rtcp_packet_type_counter_observer_.Set(observer);
}

void ViEChannel::GetSendStreamDataCounters(
    StreamDataCounters* rtp_counters,
    StreamDataCounters* rtx_counters) const {
  *rtp_counters = StreamDataCounters();
  *rtx_counters = StreamDataCounters();
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    StreamDataCounters rtp_data;
    StreamDataCounters rtx_data;
    rtp_rtcp->GetSendStreamDataCounters(&rtp_data, &rtx_data);
    rtp_counters->Add(rtp_data);
    rtx_counters->Add(rtx_data);
  }
}

void ViEChannel::GetReceiveStreamDataCounters(
    StreamDataCounters* rtp_counters,
    StreamDataCounters* rtx_counters) const {
  StreamStatistician* statistician = vie_receiver_.GetReceiveStatistics()->
      GetStatistician(vie_receiver_.GetRemoteSsrc());
  if (statistician) {
    statistician->GetReceiveStreamDataCounters(rtp_counters);
  }
  uint32_t rtx_ssrc = 0;
  if (vie_receiver_.GetRtxSsrc(&rtx_ssrc)) {
    StreamStatistician* statistician =
        vie_receiver_.GetReceiveStatistics()->GetStatistician(rtx_ssrc);
    if (statistician) {
      statistician->GetReceiveStreamDataCounters(rtx_counters);
    }
  }
}

void ViEChannel::RegisterSendChannelRtpStatisticsCallback(
      StreamDataCountersCallback* callback) {
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->RegisterSendChannelRtpStatisticsCallback(callback);
}

void ViEChannel::RegisterReceiveChannelRtpStatisticsCallback(
    StreamDataCountersCallback* callback) {
  vie_receiver_.GetReceiveStatistics()->RegisterRtpStatisticsCallback(callback);
}

void ViEChannel::GetSendRtcpPacketTypeCounter(
    RtcpPacketTypeCounter* packet_counter) const {
  std::map<uint32_t, RtcpPacketTypeCounter> counter_map =
      rtcp_packet_type_counter_observer_.GetPacketTypeCounterMap();

  RtcpPacketTypeCounter counter;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    counter.Add(counter_map[rtp_rtcp->SSRC()]);
  *packet_counter = counter;
}

void ViEChannel::GetReceiveRtcpPacketTypeCounter(
    RtcpPacketTypeCounter* packet_counter) const {
  std::map<uint32_t, RtcpPacketTypeCounter> counter_map =
      rtcp_packet_type_counter_observer_.GetPacketTypeCounterMap();

  RtcpPacketTypeCounter counter;
  counter.Add(counter_map[vie_receiver_.GetRemoteSsrc()]);

  *packet_counter = counter;
}

void ViEChannel::RegisterSendSideDelayObserver(
    SendSideDelayObserver* observer) {
  send_side_delay_observer_.Set(observer);
}

void ViEChannel::RegisterSendBitrateObserver(
    BitrateStatisticsObserver* observer) {
  send_bitrate_observer_.Set(observer);
}

int32_t ViEChannel::StartSend() {
  CriticalSectionScoped cs(crit_.get());

  if (rtp_rtcp_modules_[0]->Sending())
    return -1;

  for (size_t i = 0; i < num_active_rtp_rtcp_modules_; ++i) {
    RtpRtcp* rtp_rtcp = rtp_rtcp_modules_[i];
    rtp_rtcp->SetSendingMediaStatus(true);
    rtp_rtcp->SetSendingStatus(true);
  }
  send_payload_router_->set_active(true);
  return 0;
}

int32_t ViEChannel::StopSend() {
  send_payload_router_->set_active(false);
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->SetSendingMediaStatus(false);

  if (!rtp_rtcp_modules_[0]->Sending()) {
    return -1;
  }

  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->SetSendingStatus(false);
  }
  return 0;
}

bool ViEChannel::Sending() {
  return rtp_rtcp_modules_[0]->Sending();
}

void ViEChannel::StartReceive() {
  if (!sender_)
    StartDecodeThread();
  vie_receiver_.StartReceive();
}

void ViEChannel::StopReceive() {
  vie_receiver_.StopReceive();
  if (!sender_) {
    StopDecodeThread();
    vcm_->ResetDecoder();
  }
}

int32_t ViEChannel::ReceivedRTPPacket(const void* rtp_packet,
                                      size_t rtp_packet_length,
                                      const PacketTime& packet_time) {
  return vie_receiver_.ReceivedRTPPacket(
      rtp_packet, rtp_packet_length, packet_time);
}

int32_t ViEChannel::ReceivedRTCPPacket(const void* rtcp_packet,
                                       size_t rtcp_packet_length) {
  return vie_receiver_.ReceivedRTCPPacket(rtcp_packet, rtcp_packet_length);
}

int32_t ViEChannel::SetMTU(uint16_t mtu) {
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->SetMaxTransferUnit(mtu);
  return 0;
}

RtpRtcp* ViEChannel::rtp_rtcp() {
  return rtp_rtcp_modules_[0];
}

rtc::scoped_refptr<PayloadRouter> ViEChannel::send_payload_router() {
  return send_payload_router_;
}

VCMProtectionCallback* ViEChannel::vcm_protection_callback() {
  return vcm_protection_callback_.get();
}

CallStatsObserver* ViEChannel::GetStatsObserver() {
  return stats_observer_.get();
}

// Do not acquire the lock of |vcm_| in this function. Decode callback won't
// necessarily be called from the decoding thread. The decoding thread may have
// held the lock when calling VideoDecoder::Decode, Reset, or Release. Acquiring
// the same lock in the path of decode callback can deadlock.
int32_t ViEChannel::FrameToRender(VideoFrame& video_frame) {  // NOLINT
  CriticalSectionScoped cs(crit_.get());

  if (decoder_reset_) {
    // Trigger a callback to the user if the incoming codec has changed.
    if (codec_observer_) {
      // The codec set by RegisterReceiveCodec might not be the size we're
      // actually decoding.
      receive_codec_.width = static_cast<uint16_t>(video_frame.width());
      receive_codec_.height = static_cast<uint16_t>(video_frame.height());
      codec_observer_->IncomingCodecChanged(channel_id_, receive_codec_);
    }
    decoder_reset_ = false;
  }

  if (pre_render_callback_ != NULL)
    pre_render_callback_->FrameCallback(&video_frame);

  incoming_video_stream_->RenderFrame(channel_id_, video_frame);
  return 0;
}

int32_t ViEChannel::ReceivedDecodedReferenceFrame(
  const uint64_t picture_id) {
  return rtp_rtcp_modules_[0]->SendRTCPReferencePictureSelection(picture_id);
}

void ViEChannel::IncomingCodecChanged(const VideoCodec& codec) {
  CriticalSectionScoped cs(crit_.get());
  receive_codec_ = codec;
}

void ViEChannel::OnReceiveRatesUpdated(uint32_t bit_rate, uint32_t frame_rate) {
  CriticalSectionScoped cs(crit_.get());
  if (codec_observer_)
    codec_observer_->IncomingRate(channel_id_, frame_rate, bit_rate);
}

void ViEChannel::OnDiscardedPacketsUpdated(int discarded_packets) {
  CriticalSectionScoped cs(crit_.get());
  if (vcm_receive_stats_callback_ != NULL)
    vcm_receive_stats_callback_->OnDiscardedPacketsUpdated(discarded_packets);
}

void ViEChannel::OnFrameCountsUpdated(const FrameCounts& frame_counts) {
  CriticalSectionScoped cs(crit_.get());
  receive_frame_counts_ = frame_counts;
  if (vcm_receive_stats_callback_ != NULL)
    vcm_receive_stats_callback_->OnFrameCountsUpdated(frame_counts);
}

void ViEChannel::OnDecoderTiming(int decode_ms,
                                 int max_decode_ms,
                                 int current_delay_ms,
                                 int target_delay_ms,
                                 int jitter_buffer_ms,
                                 int min_playout_delay_ms,
                                 int render_delay_ms) {
  CriticalSectionScoped cs(crit_.get());
  if (!codec_observer_)
    return;
  codec_observer_->DecoderTiming(decode_ms,
                                 max_decode_ms,
                                 current_delay_ms,
                                 target_delay_ms,
                                 jitter_buffer_ms,
                                 min_playout_delay_ms,
                                 render_delay_ms);
}

int32_t ViEChannel::RequestKeyFrame() {
  return rtp_rtcp_modules_[0]->RequestKeyFrame();
}

int32_t ViEChannel::SliceLossIndicationRequest(
  const uint64_t picture_id) {
  return rtp_rtcp_modules_[0]->SendRTCPSliceLossIndication(
      static_cast<uint8_t>(picture_id));
}

int32_t ViEChannel::ResendPackets(const uint16_t* sequence_numbers,
                                  uint16_t length) {
  return rtp_rtcp_modules_[0]->SendNACK(sequence_numbers, length);
}

bool ViEChannel::ChannelDecodeThreadFunction(void* obj) {
  return static_cast<ViEChannel*>(obj)->ChannelDecodeProcess();
}

bool ViEChannel::ChannelDecodeProcess() {
  vcm_->Decode(kMaxDecodeWaitTimeMs);
  return true;
}

void ViEChannel::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  vcm_->SetReceiveChannelParameters(max_rtt_ms);

  CriticalSectionScoped cs(crit_.get());
  if (time_of_first_rtt_ms_ == -1)
    time_of_first_rtt_ms_ = Clock::GetRealTimeClock()->TimeInMilliseconds();
  rtt_sum_ms_ += avg_rtt_ms;
  ++num_rtts_;
}

int ViEChannel::ProtectionRequest(const FecProtectionParams* delta_fec_params,
                                  const FecProtectionParams* key_fec_params,
                                  uint32_t* video_rate_bps,
                                  uint32_t* nack_rate_bps,
                                  uint32_t* fec_rate_bps) {
  *video_rate_bps = 0;
  *nack_rate_bps = 0;
  *fec_rate_bps = 0;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    uint32_t not_used = 0;
    uint32_t module_video_rate = 0;
    uint32_t module_fec_rate = 0;
    uint32_t module_nack_rate = 0;
    rtp_rtcp->SetFecParameters(delta_fec_params, key_fec_params);
    rtp_rtcp->BitrateSent(&not_used, &module_video_rate, &module_fec_rate,
                          &module_nack_rate);
    *video_rate_bps += module_video_rate;
    *nack_rate_bps += module_nack_rate;
    *fec_rate_bps += module_fec_rate;
  }
  return 0;
}

std::vector<RtpRtcp*> ViEChannel::CreateRtpRtcpModules(
    int32_t id,
    bool receiver_only,
    ReceiveStatistics* receive_statistics,
    Transport* outgoing_transport,
    RtcpIntraFrameObserver* intra_frame_callback,
    RtcpBandwidthObserver* bandwidth_callback,
    SendTimeObserver* send_time_callback,
    RtcpRttStats* rtt_stats,
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer,
    RemoteBitrateEstimator* remote_bitrate_estimator,
    PacedSender* paced_sender,
    PacketRouter* packet_router,
    BitrateStatisticsObserver* send_bitrate_observer,
    FrameCountObserver* send_frame_count_observer,
    SendSideDelayObserver* send_side_delay_observer,
    size_t num_modules) {
  DCHECK_GT(num_modules, 0u);
  RtpRtcp::Configuration configuration;
  ReceiveStatistics* null_receive_statistics = configuration.receive_statistics;
  configuration.id = id;
  configuration.audio = false;
  configuration.receiver_only = receiver_only;
  configuration.receive_statistics = receive_statistics;
  configuration.outgoing_transport = outgoing_transport;
  configuration.intra_frame_callback = intra_frame_callback;
  configuration.rtt_stats = rtt_stats;
  configuration.rtcp_packet_type_counter_observer =
      rtcp_packet_type_counter_observer;
  configuration.paced_sender = paced_sender;
  configuration.packet_router = packet_router;
  configuration.send_bitrate_observer = send_bitrate_observer;
  configuration.send_frame_count_observer = send_frame_count_observer;
  configuration.send_side_delay_observer = send_side_delay_observer;
  configuration.bandwidth_callback = bandwidth_callback;
  configuration.send_time_callback = send_time_callback;

  std::vector<RtpRtcp*> modules;
  for (size_t i = 0; i < num_modules; ++i) {
    RtpRtcp* rtp_rtcp = RtpRtcp::CreateRtpRtcp(configuration);
    rtp_rtcp->SetSendingStatus(false);
    rtp_rtcp->SetSendingMediaStatus(false);
    rtp_rtcp->SetRTCPStatus(kRtcpCompound);
    modules.push_back(rtp_rtcp);
    // Receive statistics and remote bitrate estimator should only be set for
    // the primary (first) module.
    configuration.receive_statistics = null_receive_statistics;
    configuration.remote_bitrate_estimator = nullptr;
  }
  return modules;
}

void ViEChannel::StartDecodeThread() {
  DCHECK(!sender_);
  // Start the decode thread
  if (decode_thread_)
    return;
  decode_thread_ = ThreadWrapper::CreateThread(ChannelDecodeThreadFunction,
                                               this, "DecodingThread");
  decode_thread_->Start();
  decode_thread_->SetPriority(kHighestPriority);
}

void ViEChannel::StopDecodeThread() {
  if (!decode_thread_)
    return;

  vcm_->TriggerDecoderShutdown();

  decode_thread_->Stop();
  decode_thread_.reset();
}

int32_t ViEChannel::SetVoiceChannel(int32_t ve_channel_id,
                                    VoEVideoSync* ve_sync_interface) {
  return vie_sync_.ConfigureSync(ve_channel_id, ve_sync_interface,
                                 rtp_rtcp_modules_[0],
                                 vie_receiver_.GetRtpReceiver());
}

int32_t ViEChannel::VoiceChannel() {
  return vie_sync_.VoiceChannel();
}

void ViEChannel::RegisterPreRenderCallback(
    I420FrameCallback* pre_render_callback) {
  CriticalSectionScoped cs(crit_.get());
  pre_render_callback_ = pre_render_callback;
}

void ViEChannel::RegisterPreDecodeImageCallback(
    EncodedImageCallback* pre_decode_callback) {
  vcm_->RegisterPreDecodeImageCallback(pre_decode_callback);
}

int32_t ViEChannel::OnInitializeDecoder(
    const int32_t id,
    const int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const int frequency,
    const uint8_t channels,
    const uint32_t rate) {
  LOG(LS_INFO) << "OnInitializeDecoder " << static_cast<int>(payload_type)
               << " " << payload_name;
  vcm_->ResetDecoder();

  CriticalSectionScoped cs(crit_.get());
  decoder_reset_ = true;
  return 0;
}

void ViEChannel::OnIncomingSSRCChanged(const int32_t id, const uint32_t ssrc) {
  DCHECK_EQ(channel_id_, ChannelId(id));
  rtp_rtcp_modules_[0]->SetRemoteSSRC(ssrc);
}

void ViEChannel::OnIncomingCSRCChanged(const int32_t id,
                                       const uint32_t CSRC,
                                       const bool added) {
  DCHECK_EQ(channel_id_, ChannelId(id));
  CriticalSectionScoped cs(crit_.get());
}

void ViEChannel::RegisterSendFrameCountObserver(
    FrameCountObserver* observer) {
  send_frame_count_observer_.Set(observer);
}

void ViEChannel::RegisterReceiveStatisticsProxy(
    ReceiveStatisticsProxy* receive_statistics_proxy) {
  CriticalSectionScoped cs(crit_.get());
  vcm_receive_stats_callback_ = receive_statistics_proxy;
}

void ViEChannel::SetIncomingVideoStream(
    IncomingVideoStream* incoming_video_stream) {
  CriticalSectionScoped cs(crit_.get());
  incoming_video_stream_ = incoming_video_stream;
}
}  // namespace webrtc

/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_H_
#define WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_H_

#include <list>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/typedefs.h"
#include "webrtc/video_engine/vie_defines.h"
#include "webrtc/video_engine/vie_receiver.h"
#include "webrtc/video_engine/vie_sync_module.h"

namespace webrtc {

class CallStatsObserver;
class ChannelStatsObserver;
class Config;
class CriticalSectionWrapper;
class EncodedImageCallback;
class I420FrameCallback;
class IncomingVideoStream;
class PacedSender;
class PacketRouter;
class PayloadRouter;
class ProcessThread;
class ReceiveStatisticsProxy;
class ReportBlockStats;
class RtcpRttStats;
class ThreadWrapper;
class ViEChannelProtectionCallback;
class ViEDecoderObserver;
class ViERTPObserver;
class VideoCodingModule;
class VideoDecoder;
class VideoRenderCallback;
class VoEVideoSync;

enum StreamType {
  kViEStreamTypeNormal = 0,  // Normal media stream
  kViEStreamTypeRtx = 1      // Retransmission media stream
};

// This class declares an abstract interface for a user defined observer. It is
// up to the VideoEngine user to implement a derived class which implements the
// observer class. The observer is registered using RegisterDecoderObserver()
// and deregistered using DeregisterDecoderObserver().
class ViEDecoderObserver {
 public:
  // This method is called when a new incoming stream is detected, normally
  // triggered by a new incoming SSRC or payload type.
  virtual void IncomingCodecChanged(const int video_channel,
                                    const VideoCodec& video_codec) = 0;

  // This method is called once per second containing the frame rate and bit
  // rate for the incoming stream
  virtual void IncomingRate(const int video_channel,
                            const unsigned int framerate,
                            const unsigned int bitrate) = 0;

  // Called periodically with decoder timing information.  All values are
  // "current" snapshots unless decorated with a min_/max_ prefix.
  virtual void DecoderTiming(int decode_ms,
                             int max_decode_ms,
                             int current_delay_ms,
                             int target_delay_ms,
                             int jitter_buffer_ms,
                             int min_playout_delay_ms,
                             int render_delay_ms) = 0;

 protected:
  virtual ~ViEDecoderObserver() {}
};
class ViEChannel : public VCMFrameTypeCallback,
                   public VCMReceiveCallback,
                   public VCMReceiveStatisticsCallback,
                   public VCMDecoderTimingCallback,
                   public VCMPacketRequestCallback,
                   public RtpFeedback {
 public:
  friend class ChannelStatsObserver;
  friend class ViEChannelProtectionCallback;

  ViEChannel(int32_t channel_id,
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
             bool sender);
  ~ViEChannel();

  int32_t Init();

  // Sets the encoder to use for the channel. |new_stream| indicates the encoder
  // type has changed and we should start a new RTP stream.
  int32_t SetSendCodec(const VideoCodec& video_codec, bool new_stream = true);
  int32_t SetReceiveCodec(const VideoCodec& video_codec);
  int32_t RegisterCodecObserver(ViEDecoderObserver* observer);
  // Registers an external decoder. |buffered_rendering| means that the decoder
  // will render frames after decoding according to the render timestamp
  // provided by the video coding module. |render_delay| indicates the time
  // needed to decode and render a frame.
  int32_t RegisterExternalDecoder(const uint8_t pl_type,
                                  VideoDecoder* decoder,
                                  bool buffered_rendering,
                                  int32_t render_delay);
  int32_t DeRegisterExternalDecoder(const uint8_t pl_type);
  int32_t ReceiveCodecStatistics(uint32_t* num_key_frames,
                                 uint32_t* num_delta_frames);
  uint32_t DiscardedPackets() const;

  // Returns the estimated delay in milliseconds.
  int ReceiveDelay() const;

  void SetRTCPMode(const RTCPMethod rtcp_mode);
  void SetProtectionMode(bool enable_nack,
                         bool enable_fec,
                         int payload_type_red,
                         int payload_type_fec);
  bool IsSendingFecEnabled();
  int SetSenderBufferingMode(int target_delay_ms);
  int SetReceiverBufferingMode(int target_delay_ms);
  int32_t SetKeyFrameRequestMethod(const KeyFrameRequestMethod method);
  void EnableRemb(bool enable);
  int SetSendTimestampOffsetStatus(bool enable, int id);
  int SetReceiveTimestampOffsetStatus(bool enable, int id);
  int SetSendAbsoluteSendTimeStatus(bool enable, int id);
  int SetReceiveAbsoluteSendTimeStatus(bool enable, int id);
  int SetSendVideoRotationStatus(bool enable, int id);
  int SetReceiveVideoRotationStatus(bool enable, int id);
  int SetSendTransportSequenceNumber(bool enable, int id);
  int SetReceiveTransportSequenceNumber(bool enable, int id);
  void SetRtcpXrRrtrStatus(bool enable);
  void SetTransmissionSmoothingStatus(bool enable);
  void EnableTMMBR(bool enable);

  // Sets SSRC for outgoing stream.
  int32_t SetSSRC(const uint32_t SSRC,
                  const StreamType usage,
                  const unsigned char simulcast_idx);

  // Gets SSRC for outgoing stream number |idx|.
  int32_t GetLocalSSRC(uint8_t idx, unsigned int* ssrc);

  // Gets SSRC for the incoming stream.
  int32_t GetRemoteSSRC(uint32_t* ssrc);

  int SetRtxSendPayloadType(int payload_type, int associated_payload_type);
  void SetRtxReceivePayloadType(int payload_type, int associated_payload_type);

  void SetRtpStateForSsrc(uint32_t ssrc, const RtpState& rtp_state);
  RtpState GetRtpStateForSsrc(uint32_t ssrc);

  // Sets the CName for the outgoing stream on the channel.
  int32_t SetRTCPCName(const char* rtcp_cname);

  // Gets the CName of the incoming stream.
  int32_t GetRemoteRTCPCName(char rtcp_cname[]);

  // Returns statistics reported by the remote client in an RTCP packet.
  // TODO(pbos): Remove this along with VideoSendStream::GetRtt().
  int32_t GetSendRtcpStatistics(uint16_t* fraction_lost,
                                uint32_t* cumulative_lost,
                                uint32_t* extended_max,
                                uint32_t* jitter_samples,
                                int64_t* rtt_ms);

  // Called on receipt of RTCP report block from remote side.
  void RegisterSendChannelRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback);

  // Called on generation of RTCP stats
  void RegisterReceiveChannelRtcpStatisticsCallback(
      RtcpStatisticsCallback* callback);

  // Gets send statistics for the rtp and rtx stream.
  void GetSendStreamDataCounters(StreamDataCounters* rtp_counters,
                                 StreamDataCounters* rtx_counters) const;

  // Gets received stream data counters.
  void GetReceiveStreamDataCounters(StreamDataCounters* rtp_counters,
                                    StreamDataCounters* rtx_counters) const;

  // Called on update of RTP statistics.
  void RegisterSendChannelRtpStatisticsCallback(
      StreamDataCountersCallback* callback);

  // Called on update of RTP statistics.
  void RegisterReceiveChannelRtpStatisticsCallback(
      StreamDataCountersCallback* callback);

  void GetSendRtcpPacketTypeCounter(
      RtcpPacketTypeCounter* packet_counter) const;

  void GetReceiveRtcpPacketTypeCounter(
      RtcpPacketTypeCounter* packet_counter) const;

  void RegisterSendSideDelayObserver(SendSideDelayObserver* observer);

  // Called on any new send bitrate estimate.
  void RegisterSendBitrateObserver(BitrateStatisticsObserver* observer);

  // Implements RtpFeedback.
  virtual int32_t OnInitializeDecoder(
      const int32_t id,
      const int8_t payload_type,
      const char payload_name[RTP_PAYLOAD_NAME_SIZE],
      const int frequency,
      const uint8_t channels,
      const uint32_t rate);
  virtual void OnIncomingSSRCChanged(const int32_t id,
                                     const uint32_t ssrc);
  virtual void OnIncomingCSRCChanged(const int32_t id,
                                     const uint32_t CSRC,
                                     const bool added);

  int32_t SetRemoteSSRCType(const StreamType usage, const uint32_t SSRC);

  int32_t StartSend();
  int32_t StopSend();
  bool Sending();
  void StartReceive();
  void StopReceive();

  int32_t ReceivedRTPPacket(const void* rtp_packet,
                            const size_t rtp_packet_length,
                            const PacketTime& packet_time);
  int32_t ReceivedRTCPPacket(const void* rtcp_packet,
                             const size_t rtcp_packet_length);

  // Sets the maximum transfer unit size for the network link, i.e. including
  // IP, UDP and RTP headers.
  int32_t SetMTU(uint16_t mtu);

  // Gets the modules used by the channel.
  RtpRtcp* rtp_rtcp();
  rtc::scoped_refptr<PayloadRouter> send_payload_router();
  VCMProtectionCallback* vcm_protection_callback();


  CallStatsObserver* GetStatsObserver();

  // Implements VCMReceiveCallback.
  virtual int32_t FrameToRender(VideoFrame& video_frame);  // NOLINT

  // Implements VCMReceiveCallback.
  virtual int32_t ReceivedDecodedReferenceFrame(
      const uint64_t picture_id);

  // Implements VCMReceiveCallback.
  virtual void IncomingCodecChanged(const VideoCodec& codec);

  // Implements VCMReceiveStatisticsCallback.
  void OnReceiveRatesUpdated(uint32_t bit_rate, uint32_t frame_rate) override;
  void OnDiscardedPacketsUpdated(int discarded_packets) override;
  void OnFrameCountsUpdated(const FrameCounts& frame_counts) override;

  // Implements VCMDecoderTimingCallback.
  virtual void OnDecoderTiming(int decode_ms,
                               int max_decode_ms,
                               int current_delay_ms,
                               int target_delay_ms,
                               int jitter_buffer_ms,
                               int min_playout_delay_ms,
                               int render_delay_ms);

  // Implements VideoFrameTypeCallback.
  virtual int32_t RequestKeyFrame();

  // Implements VideoFrameTypeCallback.
  virtual int32_t SliceLossIndicationRequest(
      const uint64_t picture_id);

  // Implements VideoPacketRequestCallback.
  int32_t ResendPackets(const uint16_t* sequence_numbers,
                        uint16_t length) override;

  int32_t SetVoiceChannel(int32_t ve_channel_id,
                          VoEVideoSync* ve_sync_interface);
  int32_t VoiceChannel();

  // New-style callbacks, used by VideoReceiveStream.
  void RegisterPreRenderCallback(I420FrameCallback* pre_render_callback);
  void RegisterPreDecodeImageCallback(
      EncodedImageCallback* pre_decode_callback);

  void RegisterSendFrameCountObserver(FrameCountObserver* observer);
  void RegisterRtcpPacketTypeCounterObserver(
      RtcpPacketTypeCounterObserver* observer);
  void RegisterReceiveStatisticsProxy(
      ReceiveStatisticsProxy* receive_statistics_proxy);
  void SetIncomingVideoStream(IncomingVideoStream* incoming_video_stream);

 protected:
  static bool ChannelDecodeThreadFunction(void* obj);
  bool ChannelDecodeProcess();

  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms);

  int ProtectionRequest(const FecProtectionParams* delta_fec_params,
                        const FecProtectionParams* key_fec_params,
                        uint32_t* sent_video_rate_bps,
                        uint32_t* sent_nack_rate_bps,
                        uint32_t* sent_fec_rate_bps);

 private:
  static std::vector<RtpRtcp*> CreateRtpRtcpModules(
      int32_t id,
      bool receiver_only,
      ReceiveStatistics* receive_statistics,
      Transport* outgoing_transport,
      RtcpIntraFrameObserver* intra_frame_callback,
      RtcpBandwidthObserver* bandwidth_callback,
      SendTimeObserver* send_time_observer,
      RtcpRttStats* rtt_stats,
      RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer,
      RemoteBitrateEstimator* remote_bitrate_estimator,
      PacedSender* paced_sender,
      PacketRouter* packet_router,
      BitrateStatisticsObserver* send_bitrate_observer,
      FrameCountObserver* send_frame_count_observer,
      SendSideDelayObserver* send_side_delay_observer,
      size_t num_modules);

  // Assumed to be protected.
  void StartDecodeThread();
  void StopDecodeThread();

  void ProcessNACKRequest(const bool enable);
  // Compute NACK list parameters for the buffering mode.
  int GetRequiredNackListSize(int target_delay_ms);
  void SetRtxSendStatus(bool enable);

  void UpdateHistograms();

  // ViEChannel exposes methods that allow to modify observers and callbacks
  // to be modified. Such an API-style is cumbersome to implement and maintain
  // at all the levels when comparing to only setting them at construction. As
  // so this class instantiates its children with a wrapper that can be modified
  // at a later time.
  template <class T>
  class RegisterableCallback : public T {
   public:
    RegisterableCallback()
        : critsect_(CriticalSectionWrapper::CreateCriticalSection()),
          callback_(NULL) {}

    void Set(T* callback) {
      CriticalSectionScoped cs(critsect_.get());
      callback_ = callback;
    }

   protected:
    // Note: this should be implemented with a RW-lock to allow simultaneous
    // calls into the callback. However that doesn't seem to be needed for the
    // current type of callbacks covered by this class.
    rtc::scoped_ptr<CriticalSectionWrapper> critsect_;
    T* callback_ GUARDED_BY(critsect_);

   private:
    DISALLOW_COPY_AND_ASSIGN(RegisterableCallback);
  };

  class RegisterableBitrateStatisticsObserver:
    public RegisterableCallback<BitrateStatisticsObserver> {
    virtual void Notify(const BitrateStatistics& total_stats,
                        const BitrateStatistics& retransmit_stats,
                        uint32_t ssrc) {
      CriticalSectionScoped cs(critsect_.get());
      if (callback_)
        callback_->Notify(total_stats, retransmit_stats, ssrc);
    }
  } send_bitrate_observer_;

  class RegisterableFrameCountObserver
      : public RegisterableCallback<FrameCountObserver> {
   public:
    virtual void FrameCountUpdated(const FrameCounts& frame_counts,
                                   uint32_t ssrc) {
      CriticalSectionScoped cs(critsect_.get());
      if (callback_)
        callback_->FrameCountUpdated(frame_counts, ssrc);
    }

   private:
  } send_frame_count_observer_;

  class RegisterableSendSideDelayObserver :
      public RegisterableCallback<SendSideDelayObserver> {
    void SendSideDelayUpdated(int avg_delay_ms,
                              int max_delay_ms,
                              uint32_t ssrc) override {
      CriticalSectionScoped cs(critsect_.get());
      if (callback_)
        callback_->SendSideDelayUpdated(avg_delay_ms, max_delay_ms, ssrc);
    }
  } send_side_delay_observer_;

  class RegisterableRtcpPacketTypeCounterObserver
      : public RegisterableCallback<RtcpPacketTypeCounterObserver> {
   public:
    void RtcpPacketTypesCounterUpdated(
        uint32_t ssrc,
        const RtcpPacketTypeCounter& packet_counter) override {
      CriticalSectionScoped cs(critsect_.get());
      if (callback_)
        callback_->RtcpPacketTypesCounterUpdated(ssrc, packet_counter);
      counter_map_[ssrc] = packet_counter;
    }

    virtual std::map<uint32_t, RtcpPacketTypeCounter> GetPacketTypeCounterMap()
        const {
      CriticalSectionScoped cs(critsect_.get());
      return counter_map_;
    }

   private:
    std::map<uint32_t, RtcpPacketTypeCounter> counter_map_
        GUARDED_BY(critsect_);
  } rtcp_packet_type_counter_observer_;

  const int32_t channel_id_;
  const int32_t engine_id_;
  const uint32_t number_of_cores_;
  const bool sender_;

  ProcessThread* const module_process_thread_;

  // Used for all registered callbacks except rendering.
  rtc::scoped_ptr<CriticalSectionWrapper> crit_;

  // Owned modules/classes.
  rtc::scoped_refptr<PayloadRouter> send_payload_router_;
  rtc::scoped_ptr<ViEChannelProtectionCallback> vcm_protection_callback_;

  VideoCodingModule* const vcm_;
  ViEReceiver vie_receiver_;
  ViESyncModule vie_sync_;

  // Helper to report call statistics.
  rtc::scoped_ptr<ChannelStatsObserver> stats_observer_;

  // Not owned.
  VCMReceiveStatisticsCallback* vcm_receive_stats_callback_
      GUARDED_BY(crit_);
  FrameCounts receive_frame_counts_ GUARDED_BY(crit_);
  IncomingVideoStream* incoming_video_stream_ GUARDED_BY(crit_);
  ViEDecoderObserver* codec_observer_ GUARDED_BY(crit_);
  RtcpIntraFrameObserver* const intra_frame_observer_;
  RtcpRttStats* const rtt_stats_;
  PacedSender* const paced_sender_;
  PacketRouter* const packet_router_;

  const rtc::scoped_ptr<RtcpBandwidthObserver> bandwidth_observer_;
  SendTimeObserver* const send_time_observer_;

  bool decoder_reset_ GUARDED_BY(crit_);
  // Current receive codec used for codec change callback.
  VideoCodec receive_codec_ GUARDED_BY(crit_);
  rtc::scoped_ptr<ThreadWrapper> decode_thread_;

  int nack_history_size_sender_;
  int max_nack_reordering_threshold_;
  I420FrameCallback* pre_render_callback_ GUARDED_BY(crit_);

  const rtc::scoped_ptr<ReportBlockStats> report_block_stats_sender_;

  int64_t time_of_first_rtt_ms_ GUARDED_BY(crit_);
  int64_t rtt_sum_ms_ GUARDED_BY(crit_);
  size_t num_rtts_ GUARDED_BY(crit_);

  // RtpRtcp modules, declared last as they use other members on construction.
  const std::vector<RtpRtcp*> rtp_rtcp_modules_;
  size_t num_active_rtp_rtcp_modules_ GUARDED_BY(crit_);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_CHANNEL_H_

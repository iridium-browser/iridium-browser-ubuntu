/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <vector>

#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/jsep.h"
#include "api/mediastreaminterface.h"
#include "api/peerconnectioninterface.h"
#include "api/umametrics.h"
#include "pc/mediasession.h"
#include "pc/mediastream.h"
#include "pc/mediastreamtrack.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#include "pc/test/fakeaudiocapturemodule.h"
#include "pc/test/mockpeerconnectionobservers.h"
#include "rtc_base/checks.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ptr_util.h"
#include "rtc_base/refcountedobject.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"

// This file contains tests for RTP Media API-related behavior of
// |webrtc::PeerConnection|, see https://w3c.github.io/webrtc-pc/#rtp-media-api.

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

const uint32_t kDefaultTimeout = 10000u;

template <typename MethodFunctor>
class OnSuccessObserver : public rtc::RefCountedObject<
                              webrtc::SetRemoteDescriptionObserverInterface> {
 public:
  explicit OnSuccessObserver(MethodFunctor on_success)
      : on_success_(std::move(on_success)) {}

  // webrtc::SetRemoteDescriptionObserverInterface implementation.
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override {
    RTC_CHECK(error.ok());
    on_success_();
  }

 private:
  MethodFunctor on_success_;
};

class PeerConnectionRtpTest : public testing::Test {
 public:
  PeerConnectionRtpTest()
      : pc_factory_(
            CreatePeerConnectionFactory(rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        FakeAudioCaptureModule::Create(),
                                        CreateBuiltinAudioEncoderFactory(),
                                        CreateBuiltinAudioDecoderFactory(),
                                        nullptr,
                                        nullptr)) {}

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration());
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWithPlanB() {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kPlanB;
    return CreatePeerConnection(config);
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWithUnifiedPlan() {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(config);
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection(
      const RTCConfiguration& config) {
    auto observer = rtc::MakeUnique<MockPeerConnectionObserver>();
    auto pc = pc_factory_->CreatePeerConnection(config, nullptr, nullptr,
                                                observer.get());
    return rtc::MakeUnique<PeerConnectionWrapper>(pc_factory_, pc,
                                                  std::move(observer));
  }

 protected:
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
};

// These tests cover |webrtc::PeerConnectionObserver| callbacks firing upon
// setting the remote description.
class PeerConnectionRtpCallbacksTest : public PeerConnectionRtpTest {};

TEST_F(PeerConnectionRtpCallbacksTest, AddTrackWithoutStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddTrack(caller->CreateAudioTrack("audio_track")));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  // Since we are not supporting the no stream case with Plan B, there should be
  // a generated stream, even though we didn't set one with AddTrack.
  auto& add_track_event = callee->observer()->add_track_events_[0];
  ASSERT_EQ(add_track_event.streams.size(), 1u);
  EXPECT_TRUE(add_track_event.streams[0]->FindAudioTrack("audio_track"));
  EXPECT_EQ(add_track_event.streams, add_track_event.receiver->streams());
}

TEST_F(PeerConnectionRtpCallbacksTest, AddTrackWithStreamFiresOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddTrack(caller->CreateAudioTrack("audio_track"),
                               {"audio_stream"}));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  auto& add_track_event = callee->observer()->add_track_events_[0];
  ASSERT_EQ(add_track_event.streams.size(), 1u);
  EXPECT_EQ("audio_stream", add_track_event.streams[0]->id());
  EXPECT_TRUE(add_track_event.streams[0]->FindAudioTrack("audio_track"));
  EXPECT_EQ(add_track_event.streams, add_track_event.receiver->streams());
}

TEST_F(PeerConnectionRtpCallbacksTest,
       RemoveTrackWithoutStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddTrack(caller->CreateAudioTrack("audio_track"), {});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_F(PeerConnectionRtpCallbacksTest,
       RemoveTrackWithStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddTrack(caller->CreateAudioTrack("audio_track"),
                                 {"audio_stream"});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

TEST_F(PeerConnectionRtpCallbacksTest,
       RemoveTrackWithSharedStreamFiresOnRemoveTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kSharedStreamId[] = "shared_audio_stream";
  auto sender1 = caller->AddTrack(caller->CreateAudioTrack("audio_track1"),
                                  {kSharedStreamId});
  auto sender2 = caller->AddTrack(caller->CreateAudioTrack("audio_track2"),
                                  {kSharedStreamId});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);

  // Remove "audio_track1".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender1));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpReceiverInterface>>{
          callee->observer()->add_track_events_[0].receiver},
      callee->observer()->remove_track_events_);

  // Remove "audio_track2".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender2));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(callee->observer()->GetAddTrackReceivers(),
            callee->observer()->remove_track_events_);
}

// Tests the edge case that if a stream ID changes for a given track that both
// OnRemoveTrack and OnAddTrack is fired.
TEST_F(PeerConnectionRtpCallbacksTest,
       RemoteStreamIdChangesFiresOnRemoveAndOnAddTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kStreamId1[] = "stream1";
  const char kStreamId2[] = "stream2";
  caller->AddTrack(caller->CreateAudioTrack("audio_track1"), {kStreamId1});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(callee->observer()->add_track_events_.size(), 1u);

  // Change the stream ID of the sender in the session description.
  auto offer = caller->CreateOfferAndSetAsLocal();
  auto audio_desc = offer->description()->GetContentDescriptionByName("audio");
  ASSERT_EQ(audio_desc->mutable_streams().size(), 1u);
  audio_desc->mutable_streams()[0].set_stream_ids({kStreamId2});
  ASSERT_TRUE(
      callee->SetRemoteDescription(CloneSessionDescription(offer.get())));

  ASSERT_EQ(callee->observer()->add_track_events_.size(), 2u);
  EXPECT_EQ(callee->observer()->add_track_events_[1].streams[0]->id(),
            kStreamId2);
  ASSERT_EQ(callee->observer()->remove_track_events_.size(), 1u);
  EXPECT_EQ(callee->observer()->remove_track_events_[0]->streams()[0]->id(),
            kStreamId1);
}

// Tests that setting a remote description with sending transceivers will fire
// the OnTrack callback for each transceiver and setting a remote description
// with receive only transceivers will not call OnTrack. One transceiver is
// created without any stream_ids, while the other is created with multiple
// stream_ids.
TEST_F(PeerConnectionRtpCallbacksTest, UnifiedPlanAddTransceiverCallsOnTrack) {
  const std::string kStreamId1 = "video_stream1";
  const std::string kStreamId2 = "video_stream2";
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  auto callee = CreatePeerConnectionWithUnifiedPlan();

  auto audio_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  RtpTransceiverInit video_transceiver_init;
  video_transceiver_init.stream_ids = {kStreamId1, kStreamId2};
  auto video_transceiver =
      caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, video_transceiver_init);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  ASSERT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  ASSERT_EQ(2u, callee->observer()->on_track_transceivers_.size());
  EXPECT_EQ(audio_transceiver->mid(),
            callee->pc()->GetTransceivers()[0]->mid());
  EXPECT_EQ(video_transceiver->mid(),
            callee->pc()->GetTransceivers()[1]->mid());
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> audio_streams =
      callee->pc()->GetTransceivers()[0]->receiver()->streams();
  std::vector<rtc::scoped_refptr<MediaStreamInterface>> video_streams =
      callee->pc()->GetTransceivers()[1]->receiver()->streams();
  ASSERT_EQ(0u, audio_streams.size());
  ASSERT_EQ(2u, video_streams.size());
  EXPECT_EQ(kStreamId1, video_streams[0]->id());
  EXPECT_EQ(kStreamId2, video_streams[1]->id());
}

// Test that doing additional offer/answer exchanges with no changes to tracks
// will cause no additional OnTrack calls after the tracks have been negotiated.
TEST_F(PeerConnectionRtpCallbacksTest, UnifiedPlanReofferDoesNotCallOnTrack) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  auto callee = CreatePeerConnectionWithUnifiedPlan();

  caller->AddAudioTrack("audio");
  callee->AddAudioTrack("audio");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(1u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());

  // If caller reoffers with no changes expect no additional OnTrack calls.
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(1u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());

  // Also if callee reoffers with no changes expect no additional OnTrack calls.
  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));
  EXPECT_EQ(1u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());
}

// Test that OnTrack is called when the transceiver direction changes to send
// the track.
TEST_F(PeerConnectionRtpCallbacksTest, UnifiedPlanSetDirectionCallsOnTrack) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  auto callee = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->SetDirection(RtpTransceiverDirection::kInactive);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(0u, callee->observer()->on_track_transceivers_.size());

  transceiver->SetDirection(RtpTransceiverDirection::kSendOnly);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());

  // If the direction changes but it is still receiving on the remote side, then
  // OnTrack should not be fired again.
  transceiver->SetDirection(RtpTransceiverDirection::kSendRecv);
  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());
}

// Test that OnTrack is called twice when a sendrecv call is started, the callee
// changes the direction to inactive, then changes it back to sendrecv.
TEST_F(PeerConnectionRtpCallbacksTest,
       UnifiedPlanSetDirectionHoldCallsOnTrackTwice) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  auto callee = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());

  // Put the call on hold by no longer receiving the track.
  callee->pc()->GetTransceivers()[0]->SetDirection(
      RtpTransceiverDirection::kInactive);

  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(1u, callee->observer()->on_track_transceivers_.size());

  // Resume the call by changing the direction to recvonly. This should call
  // OnTrack again on the callee side.
  callee->pc()->GetTransceivers()[0]->SetDirection(
      RtpTransceiverDirection::kRecvOnly);

  ASSERT_TRUE(callee->ExchangeOfferAnswerWith(caller.get()));
  EXPECT_EQ(0u, caller->observer()->on_track_transceivers_.size());
  EXPECT_EQ(2u, callee->observer()->on_track_transceivers_.size());
}

// These tests examine the state of the peer connection as a result of
// performing SetRemoteDescription().
class PeerConnectionRtpObserverTest : public PeerConnectionRtpTest {};

TEST_F(PeerConnectionRtpObserverTest, AddSenderWithoutStreamAddsReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddTrack(caller->CreateAudioTrack("audio_track"), {}));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver_added = callee->pc()->GetReceivers()[0];
  EXPECT_EQ("audio_track", receiver_added->track()->id());
  // Since we are not supporting the no stream case with Plan B, there should be
  // a generated stream, even though we didn't set one with AddTrack.
  EXPECT_EQ(receiver_added->streams().size(), 1u);
  EXPECT_TRUE(receiver_added->streams()[0]->FindAudioTrack("audio_track"));
}

TEST_F(PeerConnectionRtpObserverTest, AddSenderWithStreamAddsReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  ASSERT_TRUE(caller->AddTrack(caller->CreateAudioTrack("audio_track"),
                               {"audio_stream"}));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  EXPECT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver_added = callee->pc()->GetReceivers()[0];
  EXPECT_EQ("audio_track", receiver_added->track()->id());
  EXPECT_EQ(receiver_added->streams().size(), 1u);
  EXPECT_EQ("audio_stream", receiver_added->streams()[0]->id());
  EXPECT_TRUE(receiver_added->streams()[0]->FindAudioTrack("audio_track"));
}

TEST_F(PeerConnectionRtpObserverTest,
       RemoveSenderWithoutStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddTrack(caller->CreateAudioTrack("audio_track"), {});
  ASSERT_TRUE(sender);
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver = callee->pc()->GetReceivers()[0];
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(callee->pc()->GetReceivers().size(), 0u);
}

TEST_F(PeerConnectionRtpObserverTest, RemoveSenderWithStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto sender = caller->AddTrack(caller->CreateAudioTrack("audio_track"),
                                 {"audio_stream"});
  ASSERT_TRUE(sender);
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  ASSERT_EQ(callee->pc()->GetReceivers().size(), 1u);
  auto receiver = callee->pc()->GetReceivers()[0];
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(callee->pc()->GetReceivers().size(), 0u);
}

TEST_F(PeerConnectionRtpObserverTest,
       RemoveSenderWithSharedStreamRemovesReceiver) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kSharedStreamId[] = "shared_audio_stream";
  auto sender1 = caller->AddTrack(caller->CreateAudioTrack("audio_track1"),
                                  {kSharedStreamId});
  auto sender2 = caller->AddTrack(caller->CreateAudioTrack("audio_track2"),
                                  {kSharedStreamId});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));

  ASSERT_EQ(callee->pc()->GetReceivers().size(), 2u);
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver1;
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver2;
  if (callee->pc()->GetReceivers()[0]->track()->id() == "audio_track1") {
    receiver1 = callee->pc()->GetReceivers()[0];
    receiver2 = callee->pc()->GetReceivers()[1];
  } else {
    receiver1 = callee->pc()->GetReceivers()[1];
    receiver2 = callee->pc()->GetReceivers()[0];
  }
  EXPECT_EQ("audio_track1", receiver1->track()->id());
  EXPECT_EQ("audio_track2", receiver2->track()->id());

  // Remove "audio_track1".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender1));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  // Only |receiver2| should remain.
  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<webrtc::RtpReceiverInterface>>{receiver2},
      callee->pc()->GetReceivers());

  // Remove "audio_track2".
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender2));
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  EXPECT_EQ(callee->pc()->GetReceivers().size(), 0u);
}

// Invokes SetRemoteDescription() twice in a row without synchronizing the two
// calls and examine the state of the peer connection inside the callbacks to
// ensure that the second call does not occur prematurely, contaminating the
// state of the peer connection of the first callback.
TEST_F(PeerConnectionRtpObserverTest,
       StatesCorrelateWithSetRemoteDescriptionCall) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  // Create SDP for adding a track and for removing it. This will be used in the
  // first and second SetRemoteDescription() calls.
  auto sender = caller->AddTrack(caller->CreateAudioTrack("audio_track"), {});
  auto srd1_sdp = caller->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  auto srd2_sdp = caller->CreateOfferAndSetAsLocal();

  // In the first SetRemoteDescription() callback, check that we have a
  // receiver for the track.
  auto pc = callee->pc();
  bool srd1_callback_called = false;
  auto srd1_callback = [&srd1_callback_called, &pc]() {
    EXPECT_EQ(pc->GetReceivers().size(), 1u);
    srd1_callback_called = true;
  };

  // In the second SetRemoteDescription() callback, check that the receiver has
  // been removed.
  // TODO(hbos): When we implement Unified Plan, receivers will not be removed.
  // Instead, the transceiver owning the receiver will become inactive.
  // https://crbug.com/webrtc/7600
  bool srd2_callback_called = false;
  auto srd2_callback = [&srd2_callback_called, &pc]() {
    EXPECT_TRUE(pc->GetReceivers().empty());
    srd2_callback_called = true;
  };

  // Invoke SetRemoteDescription() twice in a row without synchronizing the two
  // calls. The callbacks verify that the two calls are synchronized, as in, the
  // effects of the second SetRemoteDescription() call must not have happened by
  // the time the first callback is invoked. If it has then the receiver that is
  // added as a result of the first SetRemoteDescription() call will already
  // have been removed as a result of the second SetRemoteDescription() call
  // when the first callback is invoked.
  callee->pc()->SetRemoteDescription(
      std::move(srd1_sdp),
      new OnSuccessObserver<decltype(srd1_callback)>(srd1_callback));
  callee->pc()->SetRemoteDescription(
      std::move(srd2_sdp),
      new OnSuccessObserver<decltype(srd2_callback)>(srd2_callback));
  EXPECT_TRUE_WAIT(srd1_callback_called, kDefaultTimeout);
  EXPECT_TRUE_WAIT(srd2_callback_called, kDefaultTimeout);
}

// Tests that a remote track is created with the signaled MSIDs when they are
// communicated with a=msid and no SSRCs are signaled at all (i.e., no a=ssrc
// lines).
TEST_F(PeerConnectionRtpObserverTest, UnsignaledSsrcCreatesReceiverStreams) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  auto callee = CreatePeerConnectionWithUnifiedPlan();
  const char kStreamId1[] = "stream1";
  const char kStreamId2[] = "stream2";
  caller->AddTrack(caller->CreateAudioTrack("audio_track1"),
                   {kStreamId1, kStreamId2});

  auto offer = caller->CreateOfferAndSetAsLocal();
  // Munge the offer to take out everything but the stream_ids.
  auto contents = offer->description()->contents();
  ASSERT_TRUE(!contents.empty());
  ASSERT_TRUE(!contents[0].media_description()->streams().empty());
  std::vector<std::string> stream_ids =
      contents[0].media_description()->streams()[0].stream_ids();
  contents[0].media_description()->mutable_streams().clear();
  cricket::StreamParams new_stream;
  new_stream.set_stream_ids(stream_ids);
  contents[0].media_description()->AddStream(new_stream);

  // Set the remote description and verify that the streams were added to the
  // receiver correctly.
  ASSERT_TRUE(
      callee->SetRemoteDescription(CloneSessionDescription(offer.get())));
  auto receivers = callee->pc()->GetReceivers();
  ASSERT_EQ(receivers.size(), 1u);
  ASSERT_EQ(receivers[0]->streams().size(), 2u);
  EXPECT_EQ(receivers[0]->streams()[0]->id(), kStreamId1);
  EXPECT_EQ(receivers[0]->streams()[1]->id(), kStreamId2);
}

// Tests that with Unified Plan if the the stream id changes for a track when
// when setting a new remote description, that the media stream is updated
// appropriately for the receiver.
TEST_F(PeerConnectionRtpObserverTest, RemoteStreamIdChangesUpdatesReceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  auto callee = CreatePeerConnectionWithUnifiedPlan();

  const char kStreamId1[] = "stream1";
  const char kStreamId2[] = "stream2";
  caller->AddTrack(caller->CreateAudioTrack("audio_track1"), {kStreamId1});
  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal()));
  EXPECT_EQ(callee->observer()->add_track_events_.size(), 1u);

  // Change the stream id of the sender in the session description.
  auto offer = caller->CreateOfferAndSetAsLocal();
  auto contents = offer->description()->contents();
  ASSERT_EQ(contents.size(), 1u);
  ASSERT_EQ(contents[0].media_description()->mutable_streams().size(), 1u);
  contents[0].media_description()->mutable_streams()[0].set_stream_ids(
      {kStreamId2});

  // Set the remote description and verify that the stream was updated properly.
  ASSERT_TRUE(
      callee->SetRemoteDescription(CloneSessionDescription(offer.get())));
  auto receivers = callee->pc()->GetReceivers();
  ASSERT_EQ(receivers.size(), 1u);
  ASSERT_EQ(receivers[0]->streams().size(), 1u);
  EXPECT_EQ(receivers[0]->streams()[0]->id(), kStreamId2);
}

// This tests a regression caught by a downstream client, that occured when
// applying a remote description with a SessionDescription object that
// contained StreamParams that didn't have ids. Although there were multiple
// remote audio senders, FindSenderInfo didn't find them as unique, because
// it looked up by StreamParam.id, which none had. This meant only one
// AudioRtpReceiver was created, as opposed to one for each remote sender.
TEST_F(PeerConnectionRtpObserverTest,
       MultipleRemoteSendersWithoutStreamParamIdAddsMultipleReceivers) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  const char kStreamId1[] = "stream1";
  const char kStreamId2[] = "stream2";
  caller->AddAudioTrack("audio_track1", {kStreamId1});
  caller->AddAudioTrack("audio_track2", {kStreamId2});

  auto offer = caller->CreateOfferAndSetAsLocal();
  auto mutable_streams =
      cricket::GetFirstAudioContentDescription(offer->description())
          ->mutable_streams();
  ASSERT_EQ(mutable_streams.size(), 2u);
  // Clear the IDs in the StreamParams.
  mutable_streams[0].id.clear();
  mutable_streams[1].id.clear();
  ASSERT_TRUE(
      callee->SetRemoteDescription(CloneSessionDescription(offer.get())));

  auto receivers = callee->pc()->GetReceivers();
  ASSERT_EQ(receivers.size(), 2u);
  ASSERT_EQ(receivers[0]->streams().size(), 1u);
  EXPECT_EQ(kStreamId1, receivers[0]->streams()[0]->id());
  ASSERT_EQ(receivers[1]->streams().size(), 1u);
  EXPECT_EQ(kStreamId2, receivers[1]->streams()[0]->id());
}

// Tests for the legacy SetRemoteDescription() function signature.
class PeerConnectionRtpLegacyObserverTest : public PeerConnectionRtpTest {};

// Sanity test making sure the callback is invoked.
TEST_F(PeerConnectionRtpLegacyObserverTest, OnSuccess) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  std::string error;
  ASSERT_TRUE(
      callee->SetRemoteDescription(caller->CreateOfferAndSetAsLocal(), &error));
}

// Verifies legacy behavior: The observer is not called if if the peer
// connection is destroyed because the asynchronous callback is executed in the
// peer connection's message handler.
TEST_F(PeerConnectionRtpLegacyObserverTest,
       ObserverNotCalledIfPeerConnectionDereferenced) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  rtc::scoped_refptr<webrtc::MockSetSessionDescriptionObserver> observer =
      new rtc::RefCountedObject<webrtc::MockSetSessionDescriptionObserver>();

  auto offer = caller->CreateOfferAndSetAsLocal();
  callee->pc()->SetRemoteDescription(observer, offer.release());
  callee = nullptr;
  rtc::Thread::Current()->ProcessMessages(0);
  EXPECT_FALSE(observer->called());
}

// RtpTransceiver Tests.

// Test that by default there are no transceivers with Unified Plan.
TEST_F(PeerConnectionRtpTest, PeerConnectionHasNoTransceivers) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  EXPECT_THAT(caller->pc()->GetTransceivers(), ElementsAre());
}

// Test that a transceiver created with the audio kind has the correct initial
// properties.
TEST_F(PeerConnectionRtpTest, AddTransceiverHasCorrectInitProperties) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(rtc::nullopt, transceiver->mid());
  EXPECT_FALSE(transceiver->stopped());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceiver->direction());
  EXPECT_EQ(rtc::nullopt, transceiver->current_direction());
}

// Test that adding a transceiver with the audio kind creates an audio sender
// and audio receiver with the receiver having a live audio track.
TEST_F(PeerConnectionRtpTest,
       AddAudioTransceiverCreatesAudioSenderAndReceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceiver->media_type());

  ASSERT_TRUE(transceiver->sender());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceiver->sender()->media_type());

  ASSERT_TRUE(transceiver->receiver());
  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, transceiver->receiver()->media_type());

  auto track = transceiver->receiver()->track();
  ASSERT_TRUE(track);
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind, track->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive, track->state());
}

// Test that adding a transceiver with the video kind creates an video sender
// and video receiver with the receiver having a live video track.
TEST_F(PeerConnectionRtpTest,
       AddAudioTransceiverCreatesVideoSenderAndReceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->media_type());

  ASSERT_TRUE(transceiver->sender());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->sender()->media_type());

  ASSERT_TRUE(transceiver->receiver());
  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, transceiver->receiver()->media_type());

  auto track = transceiver->receiver()->track();
  ASSERT_TRUE(track);
  EXPECT_EQ(MediaStreamTrackInterface::kVideoKind, track->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive, track->state());
}

// Test that after a call to AddTransceiver, the transceiver shows in
// GetTransceivers(), the transceiver's sender shows in GetSenders(), and the
// transceiver's receiver shows in GetReceivers().
TEST_F(PeerConnectionRtpTest, AddTransceiverShowsInLists) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpTransceiverInterface>>{transceiver},
      caller->pc()->GetTransceivers());
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpSenderInterface>>{
          transceiver->sender()},
      caller->pc()->GetSenders());
  EXPECT_EQ(
      std::vector<rtc::scoped_refptr<RtpReceiverInterface>>{
          transceiver->receiver()},
      caller->pc()->GetReceivers());
}

// Test that the direction passed in through the AddTransceiver init parameter
// is set in the returned transceiver.
TEST_F(PeerConnectionRtpTest, AddTransceiverWithDirectionIsReflected) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);
  EXPECT_EQ(RtpTransceiverDirection::kSendOnly, transceiver->direction());
}

// Test that calling AddTransceiver with a track creates a transceiver which has
// its sender's track set to the passed-in track.
TEST_F(PeerConnectionRtpTest, AddTransceiverWithTrackCreatesSenderWithTrack) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("audio track");
  auto transceiver = caller->AddTransceiver(audio_track);

  auto sender = transceiver->sender();
  ASSERT_TRUE(sender->track());
  EXPECT_EQ(audio_track, sender->track());

  auto receiver = transceiver->receiver();
  ASSERT_TRUE(receiver->track());
  EXPECT_EQ(MediaStreamTrackInterface::kAudioKind, receiver->track()->kind());
  EXPECT_EQ(MediaStreamTrackInterface::TrackState::kLive,
            receiver->track()->state());
}

// Test that calling AddTransceiver twice with the same track creates distinct
// transceivers, senders with the same track.
TEST_F(PeerConnectionRtpTest,
       AddTransceiverTwiceWithSameTrackCreatesMultipleTransceivers) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("audio track");

  auto transceiver1 = caller->AddTransceiver(audio_track);
  auto transceiver2 = caller->AddTransceiver(audio_track);

  EXPECT_NE(transceiver1, transceiver2);

  auto sender1 = transceiver1->sender();
  auto sender2 = transceiver2->sender();
  EXPECT_NE(sender1, sender2);
  EXPECT_EQ(audio_track, sender1->track());
  EXPECT_EQ(audio_track, sender2->track());

  EXPECT_THAT(caller->pc()->GetTransceivers(),
              UnorderedElementsAre(transceiver1, transceiver2));
  EXPECT_THAT(caller->pc()->GetSenders(),
              UnorderedElementsAre(sender1, sender2));
}

// RtpTransceiver error handling tests.

TEST_F(PeerConnectionRtpTest, AddTransceiverWithInvalidKindReturnsError) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto result = caller->pc()->AddTransceiver(cricket::MEDIA_TYPE_DATA);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
}

TEST_F(PeerConnectionRtpTest, UnifiedPlanCanClosePeerConnection) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  caller->pc()->Close();
}

// Unified Plan AddTrack tests.

class PeerConnectionRtpUnifiedPlanTest : public PeerConnectionRtpTest {};

// Test that adding an audio track creates a new audio RtpSender with the given
// track.
TEST_F(PeerConnectionRtpUnifiedPlanTest, AddAudioTrackCreatesAudioSender) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("a");
  auto sender = caller->AddTrack(audio_track);
  ASSERT_TRUE(sender);

  EXPECT_EQ(cricket::MEDIA_TYPE_AUDIO, sender->media_type());
  EXPECT_EQ(audio_track, sender->track());
}

// Test that adding a video track creates a new video RtpSender with the given
// track.
TEST_F(PeerConnectionRtpUnifiedPlanTest, AddVideoTrackCreatesVideoSender) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto video_track = caller->CreateVideoTrack("a");
  auto sender = caller->AddTrack(video_track);
  ASSERT_TRUE(sender);

  EXPECT_EQ(cricket::MEDIA_TYPE_VIDEO, sender->media_type());
  EXPECT_EQ(video_track, sender->track());
}

// Test that adding a track to a new PeerConnection creates an RtpTransceiver
// with the sender that AddTrack returns and in the sendrecv direction.
TEST_F(PeerConnectionRtpUnifiedPlanTest, AddFirstTrackCreatesTransceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender = caller->AddAudioTrack("a");
  ASSERT_TRUE(sender);

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  EXPECT_EQ(sender, transceivers[0]->sender());
  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceivers[0]->direction());
}

// Test that if a transceiver of the same type but no track had been added to
// the PeerConnection and later a call to AddTrack is made, the resulting sender
// is the transceiver's sender and the sender's track is the newly-added track.
TEST_F(PeerConnectionRtpUnifiedPlanTest, AddTrackReusesTransceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto audio_track = caller->CreateAudioTrack("a");
  auto sender = caller->AddTrack(audio_track);
  ASSERT_TRUE(sender);

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(1u, transceivers.size());
  EXPECT_EQ(transceiver, transceivers[0]);
  EXPECT_EQ(sender, transceiver->sender());
  EXPECT_EQ(audio_track, sender->track());
}

// Test that adding two tracks to a new PeerConnection creates two
// RtpTransceivers in the same order.
TEST_F(PeerConnectionRtpUnifiedPlanTest, TwoAddTrackCreatesTwoTransceivers) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender1 = caller->AddAudioTrack("a");
  auto sender2 = caller->AddVideoTrack("v");
  ASSERT_TRUE(sender2);

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(sender1, transceivers[0]->sender());
  EXPECT_EQ(sender2, transceivers[1]->sender());
}

// Test that if there are multiple transceivers with no sending track then a
// later call to AddTrack will use the one of the same type as the newly-added
// track.
TEST_F(PeerConnectionRtpUnifiedPlanTest, AddTrackReusesTransceiverOfType) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto video_transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_VIDEO);
  auto sender = caller->AddVideoTrack("v");

  ASSERT_EQ(2u, caller->pc()->GetTransceivers().size());
  EXPECT_NE(sender, audio_transceiver->sender());
  EXPECT_EQ(sender, video_transceiver->sender());
}

// Test that if the only transceivers that do not have a sending track have a
// different type from the added track, then AddTrack will create a new
// transceiver for the track.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       AddTrackDoesNotReuseTransceiverOfWrongType) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto sender = caller->AddVideoTrack("v");

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_NE(sender, transceivers[0]->sender());
  EXPECT_EQ(sender, transceivers[1]->sender());
}

// Test that the first available transceiver is reused by AddTrack when multiple
// are available.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       AddTrackReusesFirstMatchingTransceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  auto sender = caller->AddAudioTrack("a");

  auto transceivers = caller->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  EXPECT_EQ(sender, transceivers[0]->sender());
  EXPECT_NE(sender, transceivers[1]->sender());
}

// Test that a call to AddTrack that reuses a transceiver will change the
// direction from inactive to sendonly.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       AddTrackChangesDirectionFromInactiveToSendOnly) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kInactive;
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);

  caller->observer()->clear_negotiation_needed();
  ASSERT_TRUE(caller->AddAudioTrack("a"));
  EXPECT_TRUE(caller->observer()->negotiation_needed());

  EXPECT_EQ(RtpTransceiverDirection::kSendOnly, transceiver->direction());
}

// Test that a call to AddTrack that reuses a transceiver will change the
// direction from recvonly to sendrecv.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       AddTrackChangesDirectionFromRecvOnlyToSendRecv) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kRecvOnly;
  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO, init);

  caller->observer()->clear_negotiation_needed();
  ASSERT_TRUE(caller->AddAudioTrack("a"));
  EXPECT_TRUE(caller->observer()->negotiation_needed());

  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceiver->direction());
}

TEST_F(PeerConnectionRtpUnifiedPlanTest, AddTrackCreatesSenderWithTrackId) {
  const std::string kTrackId = "audio_track";

  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack(kTrackId);
  auto sender = caller->AddTrack(audio_track);

  EXPECT_EQ(kTrackId, sender->id());
}

// Unified Plan AddTrack error handling.

TEST_F(PeerConnectionRtpUnifiedPlanTest, AddTrackErrorIfClosed) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("a");
  caller->pc()->Close();

  caller->observer()->clear_negotiation_needed();
  auto result = caller->pc()
                ->AddTrack(audio_track, std::vector<std::string>());
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.error().type());
  EXPECT_FALSE(caller->observer()->negotiation_needed());
}

TEST_F(PeerConnectionRtpUnifiedPlanTest, AddTrackErrorIfTrackAlreadyHasSender) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto audio_track = caller->CreateAudioTrack("a");
  ASSERT_TRUE(caller->AddTrack(audio_track));

  caller->observer()->clear_negotiation_needed();
  auto result = caller->pc()
                ->AddTrack(audio_track, std::vector<std::string>());
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, result.error().type());
  EXPECT_FALSE(caller->observer()->negotiation_needed());
}

// Unified Plan RemoveTrack tests.

// Test that calling RemoveTrack on a sender with a previously-added track
// clears the sender's track.
TEST_F(PeerConnectionRtpUnifiedPlanTest, RemoveTrackClearsSenderTrack) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender = caller->AddAudioTrack("a");
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));

  EXPECT_FALSE(sender->track());
}

// Test that calling RemoveTrack on a sender where the transceiver is configured
// in the sendrecv direction changes the transceiver's direction to recvonly.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       RemoveTrackChangesDirectionFromSendRecvToRecvOnly) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendRecv;
  auto transceiver =
      caller->AddTransceiver(caller->CreateAudioTrack("a"), init);

  caller->observer()->clear_negotiation_needed();
  ASSERT_TRUE(caller->pc()->RemoveTrack(transceiver->sender()));
  EXPECT_TRUE(caller->observer()->negotiation_needed());

  EXPECT_EQ(RtpTransceiverDirection::kRecvOnly, transceiver->direction());
  EXPECT_TRUE(caller->observer()->renegotiation_needed_);
}

// Test that calling RemoveTrack on a sender where the transceiver is configured
// in the sendonly direction changes the transceiver's direction to inactive.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       RemoveTrackChangesDirectionFromSendOnlyToInactive) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  RtpTransceiverInit init;
  init.direction = RtpTransceiverDirection::kSendOnly;
  auto transceiver =
      caller->AddTransceiver(caller->CreateAudioTrack("a"), init);

  caller->observer()->clear_negotiation_needed();
  ASSERT_TRUE(caller->pc()->RemoveTrack(transceiver->sender()));
  EXPECT_TRUE(caller->observer()->negotiation_needed());

  EXPECT_EQ(RtpTransceiverDirection::kInactive, transceiver->direction());
}

// Test that calling RemoveTrack with a sender that has a null track results in
// no change in state.
TEST_F(PeerConnectionRtpUnifiedPlanTest, RemoveTrackWithNullSenderTrackIsNoOp) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender = caller->AddAudioTrack("a");
  auto transceiver = caller->pc()->GetTransceivers()[0];
  ASSERT_TRUE(sender->SetTrack(nullptr));

  caller->observer()->clear_negotiation_needed();
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));
  EXPECT_FALSE(caller->observer()->negotiation_needed());

  EXPECT_EQ(RtpTransceiverDirection::kSendRecv, transceiver->direction());
}

// Unified Plan RemoveTrack error handling.

TEST_F(PeerConnectionRtpUnifiedPlanTest, RemoveTrackErrorIfClosed) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender = caller->AddAudioTrack("a");
  caller->pc()->Close();

  caller->observer()->clear_negotiation_needed();
  EXPECT_FALSE(caller->pc()->RemoveTrack(sender));
  EXPECT_FALSE(caller->observer()->negotiation_needed());
}

TEST_F(PeerConnectionRtpUnifiedPlanTest,
       RemoveTrackNoErrorIfTrackAlreadyRemoved) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto sender = caller->AddAudioTrack("a");
  ASSERT_TRUE(caller->pc()->RemoveTrack(sender));

  caller->observer()->clear_negotiation_needed();
  EXPECT_TRUE(caller->pc()->RemoveTrack(sender));
  EXPECT_FALSE(caller->observer()->negotiation_needed());
}

// Test that OnRenegotiationNeeded is fired if SetDirection is called on an
// active RtpTransceiver with a new direction.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       RenegotiationNeededAfterTransceiverSetDirection) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  caller->observer()->clear_negotiation_needed();
  transceiver->SetDirection(RtpTransceiverDirection::kInactive);
  EXPECT_TRUE(caller->observer()->negotiation_needed());
}

// Test that OnRenegotiationNeeded is not fired if SetDirection is called on an
// active RtpTransceiver with current direction.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       NoRenegotiationNeededAfterTransceiverSetSameDirection) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);

  caller->observer()->clear_negotiation_needed();
  transceiver->SetDirection(transceiver->direction());
  EXPECT_FALSE(caller->observer()->negotiation_needed());
}

// Test that OnRenegotiationNeeded is not fired if SetDirection is called on a
// stopped RtpTransceiver.
TEST_F(PeerConnectionRtpUnifiedPlanTest,
       NoRenegotiationNeededAfterSetDirectionOnStoppedTransceiver) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();

  auto transceiver = caller->AddTransceiver(cricket::MEDIA_TYPE_AUDIO);
  transceiver->Stop();

  caller->observer()->clear_negotiation_needed();
  transceiver->SetDirection(RtpTransceiverDirection::kInactive);
  EXPECT_FALSE(caller->observer()->negotiation_needed());
}

// Test MSID signaling between Unified Plan and Plan B endpoints. There are two
// options for this kind of signaling: media section based (a=msid) and ssrc
// based (a=ssrc MSID). While JSEP only specifies media section MSID signaling,
// we want to ensure compatibility with older Plan B endpoints that might expect
// ssrc based MSID signaling. Thus we test here that Unified Plan offers both
// types but answers with the same type as the offer.

class PeerConnectionMsidSignalingTest : public PeerConnectionRtpTest {};

TEST_F(PeerConnectionMsidSignalingTest, UnifiedPlanTalkingToOurself) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  caller->AddAudioTrack("caller_audio");
  auto callee = CreatePeerConnectionWithUnifiedPlan();
  callee->AddAudioTrack("callee_audio");
  auto caller_observer = caller->RegisterFakeMetricsObserver();

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  // Offer should have had both a=msid and a=ssrc MSID lines.
  auto* offer = callee->pc()->remote_description();
  EXPECT_EQ((cricket::kMsidSignalingMediaSection |
             cricket::kMsidSignalingSsrcAttribute),
            offer->description()->msid_signaling());

  // Answer should have had only a=msid lines.
  auto* answer = caller->pc()->remote_description();
  EXPECT_EQ(cricket::kMsidSignalingMediaSection,
            answer->description()->msid_signaling());
  // Check that this is counted correctly
  EXPECT_TRUE(caller_observer->ExpectOnlySingleEnumCount(
      kEnumCounterSdpSemanticNegotiated, kSdpSemanticNegotiatedUnifiedPlan));
}

TEST_F(PeerConnectionMsidSignalingTest, PlanBOfferToUnifiedPlanAnswer) {
  auto caller = CreatePeerConnectionWithPlanB();
  caller->AddAudioTrack("caller_audio");
  auto callee = CreatePeerConnectionWithUnifiedPlan();
  callee->AddAudioTrack("callee_audio");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  // Offer should have only a=ssrc MSID lines.
  auto* offer = callee->pc()->remote_description();
  EXPECT_EQ(cricket::kMsidSignalingSsrcAttribute,
            offer->description()->msid_signaling());

  // Answer should have only a=ssrc MSID lines to match the offer.
  auto* answer = caller->pc()->remote_description();
  EXPECT_EQ(cricket::kMsidSignalingSsrcAttribute,
            answer->description()->msid_signaling());
}

// This tests that a Plan B endpoint appropriately sets the remote description
// from a Unified Plan offer. When the Unified Plan offer contains a=msid lines
// that signal no stream ids or multiple stream ids we expect that the Plan B
// endpoint always has exactly one media stream per track.
TEST_F(PeerConnectionMsidSignalingTest, UnifiedPlanToPlanBAnswer) {
  const std::string kStreamId1 = "audio_stream_1";
  const std::string kStreamId2 = "audio_stream_2";

  auto caller = CreatePeerConnectionWithUnifiedPlan();
  caller->AddAudioTrack("caller_audio", {kStreamId1, kStreamId2});
  caller->AddVideoTrack("caller_video", {});
  auto callee = CreatePeerConnectionWithPlanB();
  callee->AddAudioTrack("callee_audio");
  caller->AddVideoTrack("callee_video");

  ASSERT_TRUE(caller->ExchangeOfferAnswerWith(callee.get()));

  // Offer should have had both a=msid and a=ssrc MSID lines.
  auto* offer = callee->pc()->remote_description();
  EXPECT_EQ((cricket::kMsidSignalingMediaSection |
             cricket::kMsidSignalingSsrcAttribute),
            offer->description()->msid_signaling());

  // Callee should always have 1 stream for all of it's receivers.
  const auto& track_events = callee->observer()->add_track_events_;
  ASSERT_EQ(2u, track_events.size());
  ASSERT_EQ(1u, track_events[0].streams.size());
  EXPECT_EQ(kStreamId1, track_events[0].streams[0]->id());
  ASSERT_EQ(1u, track_events[1].streams.size());
  // This autogenerated a stream id for the empty one signalled.
  EXPECT_FALSE(track_events[1].streams[0]->id().empty());
}

TEST_F(PeerConnectionMsidSignalingTest, PureUnifiedPlanToUs) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  caller->AddAudioTrack("caller_audio");
  auto callee = CreatePeerConnectionWithUnifiedPlan();
  callee->AddAudioTrack("callee_audio");

  auto offer = caller->CreateOffer();
  // Simulate a pure Unified Plan offerer by setting the MSID signaling to media
  // section only.
  offer->description()->set_msid_signaling(cricket::kMsidSignalingMediaSection);

  ASSERT_TRUE(
      caller->SetLocalDescription(CloneSessionDescription(offer.get())));
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  // Answer should have only a=msid to match the offer.
  auto answer = callee->CreateAnswer();
  EXPECT_EQ(cricket::kMsidSignalingMediaSection,
            answer->description()->msid_signaling());
}

// Test that the correct UMA metrics are reported for simple/complex SDP.

class SdpFormatReceivedTest : public PeerConnectionRtpTest {};

#ifdef HAVE_SCTP
TEST_F(SdpFormatReceivedTest, DataChannelOnlyIsReportedAsNoTracks) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  caller->CreateDataChannel("dc");
  auto callee = CreatePeerConnectionWithUnifiedPlan();
  auto callee_metrics = callee->RegisterFakeMetricsObserver();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));

  EXPECT_TRUE(callee_metrics->ExpectOnlySingleEnumCount(
      kEnumCounterSdpFormatReceived, kSdpFormatReceivedNoTracks));
}
#endif  // HAVE_SCTP

TEST_F(SdpFormatReceivedTest, SimpleUnifiedPlanIsReportedAsSimple) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  caller->AddAudioTrack("audio");
  caller->AddVideoTrack("video");
  auto callee = CreatePeerConnectionWithPlanB();
  auto callee_metrics = callee->RegisterFakeMetricsObserver();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));

  EXPECT_TRUE(callee_metrics->ExpectOnlySingleEnumCount(
      kEnumCounterSdpFormatReceived, kSdpFormatReceivedSimple));
}

TEST_F(SdpFormatReceivedTest, SimplePlanBIsReportedAsSimple) {
  auto caller = CreatePeerConnectionWithPlanB();
  caller->AddVideoTrack("video");  // Video only.
  auto callee = CreatePeerConnectionWithUnifiedPlan();
  auto callee_metrics = callee->RegisterFakeMetricsObserver();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));

  EXPECT_TRUE(callee_metrics->ExpectOnlySingleEnumCount(
      kEnumCounterSdpFormatReceived, kSdpFormatReceivedSimple));
}

TEST_F(SdpFormatReceivedTest, ComplexUnifiedIsReportedAsComplexUnifiedPlan) {
  auto caller = CreatePeerConnectionWithUnifiedPlan();
  caller->AddAudioTrack("audio1");
  caller->AddAudioTrack("audio2");
  caller->AddVideoTrack("video");
  auto callee = CreatePeerConnectionWithPlanB();
  auto callee_metrics = callee->RegisterFakeMetricsObserver();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));

  EXPECT_TRUE(callee_metrics->ExpectOnlySingleEnumCount(
      kEnumCounterSdpFormatReceived, kSdpFormatReceivedComplexUnifiedPlan));
}

TEST_F(SdpFormatReceivedTest, ComplexPlanBIsReportedAsComplexPlanB) {
  auto caller = CreatePeerConnectionWithPlanB();
  caller->AddVideoTrack("video1");
  caller->AddVideoTrack("video2");
  auto callee = CreatePeerConnectionWithUnifiedPlan();
  auto callee_metrics = callee->RegisterFakeMetricsObserver();

  ASSERT_TRUE(callee->SetRemoteDescription(caller->CreateOffer()));

  EXPECT_TRUE(callee_metrics->ExpectOnlySingleEnumCount(
      kEnumCounterSdpFormatReceived, kSdpFormatReceivedComplexPlanB));
}

// Sender setups in a call.

class PeerConnectionSenderTest : public PeerConnectionRtpTest {};

TEST_F(PeerConnectionSenderTest, CreateTwoSendersWithSameTrack) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();

  auto track = caller->CreateAudioTrack("audio_track");
  auto sender1 = caller->AddTrack(track);
  ASSERT_TRUE(sender1);
  // We need to temporarily reset the track for the subsequent AddTrack() to
  // succeed.
  EXPECT_TRUE(sender1->SetTrack(nullptr));
  auto sender2 = caller->AddTrack(track);
  EXPECT_TRUE(sender2);
  EXPECT_TRUE(sender1->SetTrack(track));

  // TODO(hbos): When https://crbug.com/webrtc/8734 is resolved, this should
  // return true, and doing |callee->SetRemoteDescription()| should work.
  EXPECT_FALSE(caller->CreateOfferAndSetAsLocal());
}

}  // namespace webrtc

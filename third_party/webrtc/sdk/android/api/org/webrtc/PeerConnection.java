/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Java-land version of the PeerConnection APIs; wraps the C++ API
 * http://www.webrtc.org/reference/native-apis, which in turn is inspired by the
 * JS APIs: http://dev.w3.org/2011/webrtc/editor/webrtc.html and
 * http://www.w3.org/TR/mediacapture-streams/
 */
@JNINamespace("webrtc::jni")
public class PeerConnection {
  /** Tracks PeerConnectionInterface::IceGatheringState */
  public enum IceGatheringState {
    NEW,
    GATHERING,
    COMPLETE;

    @CalledByNative("IceGatheringState")
    static IceGatheringState fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  /** Tracks PeerConnectionInterface::IceConnectionState */
  public enum IceConnectionState {
    NEW,
    CHECKING,
    CONNECTED,
    COMPLETED,
    FAILED,
    DISCONNECTED,
    CLOSED;

    @CalledByNative("IceConnectionState")
    static IceConnectionState fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  /** Tracks PeerConnectionInterface::TlsCertPolicy */
  public enum TlsCertPolicy {
    TLS_CERT_POLICY_SECURE,
    TLS_CERT_POLICY_INSECURE_NO_CHECK,
  }

  /** Tracks PeerConnectionInterface::SignalingState */
  public enum SignalingState {
    STABLE,
    HAVE_LOCAL_OFFER,
    HAVE_LOCAL_PRANSWER,
    HAVE_REMOTE_OFFER,
    HAVE_REMOTE_PRANSWER,
    CLOSED;

    @CalledByNative("SignalingState")
    static SignalingState fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  /** Java version of PeerConnectionObserver. */
  public static interface Observer {
    /** Triggered when the SignalingState changes. */
    @CalledByNative("Observer") void onSignalingChange(SignalingState newState);

    /** Triggered when the IceConnectionState changes. */
    @CalledByNative("Observer") void onIceConnectionChange(IceConnectionState newState);

    /** Triggered when the ICE connection receiving status changes. */
    @CalledByNative("Observer") void onIceConnectionReceivingChange(boolean receiving);

    /** Triggered when the IceGatheringState changes. */
    @CalledByNative("Observer") void onIceGatheringChange(IceGatheringState newState);

    /** Triggered when a new ICE candidate has been found. */
    @CalledByNative("Observer") void onIceCandidate(IceCandidate candidate);

    /** Triggered when some ICE candidates have been removed. */
    @CalledByNative("Observer") void onIceCandidatesRemoved(IceCandidate[] candidates);

    /** Triggered when media is received on a new stream from remote peer. */
    @CalledByNative("Observer") void onAddStream(MediaStream stream);

    /** Triggered when a remote peer close a stream. */
    @CalledByNative("Observer") void onRemoveStream(MediaStream stream);

    /** Triggered when a remote peer opens a DataChannel. */
    @CalledByNative("Observer") void onDataChannel(DataChannel dataChannel);

    /** Triggered when renegotiation is necessary. */
    @CalledByNative("Observer") void onRenegotiationNeeded();

    /**
     * Triggered when a new track is signaled by the remote peer, as a result of
     * setRemoteDescription.
     */
    @CalledByNative("Observer") void onAddTrack(RtpReceiver receiver, MediaStream[] mediaStreams);
  }

  /** Java version of PeerConnectionInterface.IceServer. */
  public static class IceServer {
    // List of URIs associated with this server. Valid formats are described
    // in RFC7064 and RFC7065, and more may be added in the future. The "host"
    // part of the URI may contain either an IP address or a hostname.
    @Deprecated public final String uri;
    public final List<String> urls;
    public final String username;
    public final String password;
    public final TlsCertPolicy tlsCertPolicy;

    // If the URIs in |urls| only contain IP addresses, this field can be used
    // to indicate the hostname, which may be necessary for TLS (using the SNI
    // extension). If |urls| itself contains the hostname, this isn't
    // necessary.
    public final String hostname;

    // List of protocols to be used in the TLS ALPN extension.
    public final List<String> tlsAlpnProtocols;

    // List of elliptic curves to be used in the TLS elliptic curves extension.
    // Only curve names supported by OpenSSL should be used (eg. "P-256","X25519").
    public final List<String> tlsEllipticCurves;

    /** Convenience constructor for STUN servers. */
    @Deprecated
    public IceServer(String uri) {
      this(uri, "", "");
    }

    @Deprecated
    public IceServer(String uri, String username, String password) {
      this(uri, username, password, TlsCertPolicy.TLS_CERT_POLICY_SECURE);
    }

    @Deprecated
    public IceServer(String uri, String username, String password, TlsCertPolicy tlsCertPolicy) {
      this(uri, username, password, tlsCertPolicy, "");
    }

    @Deprecated
    public IceServer(String uri, String username, String password, TlsCertPolicy tlsCertPolicy,
        String hostname) {
      this(uri, Collections.singletonList(uri), username, password, tlsCertPolicy, hostname, null,
          null);
    }

    private IceServer(String uri, List<String> urls, String username, String password,
        TlsCertPolicy tlsCertPolicy, String hostname, List<String> tlsAlpnProtocols,
        List<String> tlsEllipticCurves) {
      if (uri == null || urls == null || urls.isEmpty()) {
        throw new IllegalArgumentException("uri == null || urls == null || urls.isEmpty()");
      }
      for (String it : urls) {
        if (it == null) {
          throw new IllegalArgumentException("urls element is null: " + urls);
        }
      }
      if (username == null) {
        throw new IllegalArgumentException("username == null");
      }
      if (password == null) {
        throw new IllegalArgumentException("password == null");
      }
      if (hostname == null) {
        throw new IllegalArgumentException("hostname == null");
      }
      this.uri = uri;
      this.urls = urls;
      this.username = username;
      this.password = password;
      this.tlsCertPolicy = tlsCertPolicy;
      this.hostname = hostname;
      this.tlsAlpnProtocols = tlsAlpnProtocols;
      this.tlsEllipticCurves = tlsEllipticCurves;
    }

    @Override
    public String toString() {
      return urls + " [" + username + ":" + password + "] [" + tlsCertPolicy + "] [" + hostname
          + "] [" + tlsAlpnProtocols + "] [" + tlsEllipticCurves + "]";
    }

    public static Builder builder(String uri) {
      return new Builder(Collections.singletonList(uri));
    }

    public static Builder builder(List<String> urls) {
      return new Builder(urls);
    }

    public static class Builder {
      private final List<String> urls;
      private String username = "";
      private String password = "";
      private TlsCertPolicy tlsCertPolicy = TlsCertPolicy.TLS_CERT_POLICY_SECURE;
      private String hostname = "";
      private List<String> tlsAlpnProtocols;
      private List<String> tlsEllipticCurves;

      private Builder(List<String> urls) {
        if (urls == null || urls.isEmpty()) {
          throw new IllegalArgumentException("urls == null || urls.isEmpty(): " + urls);
        }
        this.urls = urls;
      }

      public Builder setUsername(String username) {
        this.username = username;
        return this;
      }

      public Builder setPassword(String password) {
        this.password = password;
        return this;
      }

      public Builder setTlsCertPolicy(TlsCertPolicy tlsCertPolicy) {
        this.tlsCertPolicy = tlsCertPolicy;
        return this;
      }

      public Builder setHostname(String hostname) {
        this.hostname = hostname;
        return this;
      }

      public Builder setTlsAlpnProtocols(List<String> tlsAlpnProtocols) {
        this.tlsAlpnProtocols = tlsAlpnProtocols;
        return this;
      }

      public Builder setTlsEllipticCurves(List<String> tlsEllipticCurves) {
        this.tlsEllipticCurves = tlsEllipticCurves;
        return this;
      }

      public IceServer createIceServer() {
        return new IceServer(urls.get(0), urls, username, password, tlsCertPolicy, hostname,
            tlsAlpnProtocols, tlsEllipticCurves);
      }
    }

    @CalledByNative("IceServer")
    List<String> getUrls() {
      return urls;
    }

    @CalledByNative("IceServer")
    String getUsername() {
      return username;
    }

    @CalledByNative("IceServer")
    String getPassword() {
      return password;
    }

    @CalledByNative("IceServer")
    TlsCertPolicy getTlsCertPolicy() {
      return tlsCertPolicy;
    }

    @CalledByNative("IceServer")
    String getHostname() {
      return hostname;
    }

    @CalledByNative("IceServer")
    List<String> getTlsAlpnProtocols() {
      return tlsAlpnProtocols;
    }

    @CalledByNative("IceServer")
    List<String> getTlsEllipticCurves() {
      return tlsEllipticCurves;
    }
  }

  /** Java version of PeerConnectionInterface.IceTransportsType */
  public enum IceTransportsType { NONE, RELAY, NOHOST, ALL }

  /** Java version of PeerConnectionInterface.BundlePolicy */
  public enum BundlePolicy { BALANCED, MAXBUNDLE, MAXCOMPAT }

  /** Java version of PeerConnectionInterface.RtcpMuxPolicy */
  public enum RtcpMuxPolicy { NEGOTIATE, REQUIRE }

  /** Java version of PeerConnectionInterface.TcpCandidatePolicy */
  public enum TcpCandidatePolicy { ENABLED, DISABLED }

  /** Java version of PeerConnectionInterface.CandidateNetworkPolicy */
  public enum CandidateNetworkPolicy { ALL, LOW_COST }

  // Keep in sync with webrtc/rtc_base/network_constants.h.
  public enum AdapterType {
    UNKNOWN,
    ETHERNET,
    WIFI,
    CELLULAR,
    VPN,
    LOOPBACK,
  }

  /** Java version of rtc::KeyType */
  public enum KeyType { RSA, ECDSA }

  /** Java version of PeerConnectionInterface.ContinualGatheringPolicy */
  public enum ContinualGatheringPolicy { GATHER_ONCE, GATHER_CONTINUALLY }

  /** Java version of rtc::IntervalRange */
  public static class IntervalRange {
    private final int min;
    private final int max;

    public IntervalRange(int min, int max) {
      this.min = min;
      this.max = max;
    }

    @CalledByNative("IntervalRange")
    public int getMin() {
      return min;
    }

    @CalledByNative("IntervalRange")
    public int getMax() {
      return max;
    }
  }

  /** Java version of PeerConnectionInterface.RTCConfiguration */
  // TODO(qingsi): Resolve the naming inconsistency of fields with/without units.
  public static class RTCConfiguration {
    public IceTransportsType iceTransportsType;
    public List<IceServer> iceServers;
    public BundlePolicy bundlePolicy;
    public RtcpMuxPolicy rtcpMuxPolicy;
    public TcpCandidatePolicy tcpCandidatePolicy;
    public CandidateNetworkPolicy candidateNetworkPolicy;
    public int audioJitterBufferMaxPackets;
    public boolean audioJitterBufferFastAccelerate;
    public int iceConnectionReceivingTimeout;
    public int iceBackupCandidatePairPingInterval;
    public KeyType keyType;
    public ContinualGatheringPolicy continualGatheringPolicy;
    public int iceCandidatePoolSize;
    public boolean pruneTurnPorts;
    public boolean presumeWritableWhenFullyRelayed;
    public Integer iceCheckMinInterval;
    // The interval in milliseconds at which STUN candidates will resend STUN binding requests
    // to keep NAT bindings open.
    // The default value in the implementation is used if this field is null.
    public Integer stunCandidateKeepaliveIntervalMs;
    public boolean disableIPv6OnWifi;
    // By default, PeerConnection will use a limited number of IPv6 network
    // interfaces, in order to avoid too many ICE candidate pairs being created
    // and delaying ICE completion.
    //
    // Can be set to Integer.MAX_VALUE to effectively disable the limit.
    public int maxIPv6Networks;
    public IntervalRange iceRegatherIntervalRange;

    // These values will be overridden by MediaStream constraints if deprecated constraints-based
    // create peerconnection interface is used.
    public boolean disableIpv6;
    public boolean enableDscp;
    public boolean enableCpuOveruseDetection;
    public boolean enableRtpDataChannel;
    public boolean suspendBelowMinBitrate;
    public Integer screencastMinBitrate;
    public Boolean combinedAudioVideoBwe;
    public Boolean enableDtlsSrtp;
    // Use "Unknown" to represent no preference of adapter types, not the
    // preference of adapters of unknown types.
    public AdapterType networkPreference;

    // This is an optional wrapper for the C++ webrtc::TurnCustomizer.
    public TurnCustomizer turnCustomizer;

    // TODO(deadbeef): Instead of duplicating the defaults here, we should do
    // something to pick up the defaults from C++. The Objective-C equivalent
    // of RTCConfiguration does that.
    public RTCConfiguration(List<IceServer> iceServers) {
      iceTransportsType = IceTransportsType.ALL;
      bundlePolicy = BundlePolicy.BALANCED;
      rtcpMuxPolicy = RtcpMuxPolicy.REQUIRE;
      tcpCandidatePolicy = TcpCandidatePolicy.ENABLED;
      candidateNetworkPolicy = CandidateNetworkPolicy.ALL;
      this.iceServers = iceServers;
      audioJitterBufferMaxPackets = 50;
      audioJitterBufferFastAccelerate = false;
      iceConnectionReceivingTimeout = -1;
      iceBackupCandidatePairPingInterval = -1;
      keyType = KeyType.ECDSA;
      continualGatheringPolicy = ContinualGatheringPolicy.GATHER_ONCE;
      iceCandidatePoolSize = 0;
      pruneTurnPorts = false;
      presumeWritableWhenFullyRelayed = false;
      iceCheckMinInterval = null;
      stunCandidateKeepaliveIntervalMs = null;
      disableIPv6OnWifi = false;
      maxIPv6Networks = 5;
      iceRegatherIntervalRange = null;
      disableIpv6 = false;
      enableDscp = false;
      enableCpuOveruseDetection = true;
      enableRtpDataChannel = false;
      suspendBelowMinBitrate = false;
      screencastMinBitrate = null;
      combinedAudioVideoBwe = null;
      enableDtlsSrtp = null;
      networkPreference = AdapterType.UNKNOWN;
    }

    @CalledByNative("RTCConfiguration")
    IceTransportsType getIceTransportsType() {
      return iceTransportsType;
    }

    @CalledByNative("RTCConfiguration")
    List<IceServer> getIceServers() {
      return iceServers;
    }

    @CalledByNative("RTCConfiguration")
    BundlePolicy getBundlePolicy() {
      return bundlePolicy;
    }

    @CalledByNative("RTCConfiguration")
    RtcpMuxPolicy getRtcpMuxPolicy() {
      return rtcpMuxPolicy;
    }

    @CalledByNative("RTCConfiguration")
    TcpCandidatePolicy getTcpCandidatePolicy() {
      return tcpCandidatePolicy;
    }

    @CalledByNative("RTCConfiguration")
    CandidateNetworkPolicy getCandidateNetworkPolicy() {
      return candidateNetworkPolicy;
    }

    @CalledByNative("RTCConfiguration")
    int getAudioJitterBufferMaxPackets() {
      return audioJitterBufferMaxPackets;
    }

    @CalledByNative("RTCConfiguration")
    boolean getAudioJitterBufferFastAccelerate() {
      return audioJitterBufferFastAccelerate;
    }

    @CalledByNative("RTCConfiguration")
    int getIceConnectionReceivingTimeout() {
      return iceConnectionReceivingTimeout;
    }

    @CalledByNative("RTCConfiguration")
    int getIceBackupCandidatePairPingInterval() {
      return iceBackupCandidatePairPingInterval;
    }

    @CalledByNative("RTCConfiguration")
    KeyType getKeyType() {
      return keyType;
    }

    @CalledByNative("RTCConfiguration")
    ContinualGatheringPolicy getContinualGatheringPolicy() {
      return continualGatheringPolicy;
    }

    @CalledByNative("RTCConfiguration")
    int getIceCandidatePoolSize() {
      return iceCandidatePoolSize;
    }

    @CalledByNative("RTCConfiguration")
    boolean getPruneTurnPorts() {
      return pruneTurnPorts;
    }

    @CalledByNative("RTCConfiguration")
    boolean getPresumeWritableWhenFullyRelayed() {
      return presumeWritableWhenFullyRelayed;
    }

    @CalledByNative("RTCConfiguration")
    Integer getIceCheckMinInterval() {
      return iceCheckMinInterval;
    }

    @CalledByNative("RTCConfiguration")
    Integer getStunCandidateKeepaliveInterval() {
      return stunCandidateKeepaliveIntervalMs;
    }

    @CalledByNative("RTCConfiguration")
    boolean getDisableIPv6OnWifi() {
      return disableIPv6OnWifi;
    }

    @CalledByNative("RTCConfiguration")
    int getMaxIPv6Networks() {
      return maxIPv6Networks;
    }

    @CalledByNative("RTCConfiguration")
    IntervalRange getIceRegatherIntervalRange() {
      return iceRegatherIntervalRange;
    }

    @CalledByNative("RTCConfiguration")
    TurnCustomizer getTurnCustomizer() {
      return turnCustomizer;
    }

    @CalledByNative("RTCConfiguration")
    boolean getDisableIpv6() {
      return disableIpv6;
    }

    @CalledByNative("RTCConfiguration")
    boolean getEnableDscp() {
      return enableDscp;
    }

    @CalledByNative("RTCConfiguration")
    boolean getEnableCpuOveruseDetection() {
      return enableCpuOveruseDetection;
    }

    @CalledByNative("RTCConfiguration")
    boolean getEnableRtpDataChannel() {
      return enableRtpDataChannel;
    }

    @CalledByNative("RTCConfiguration")
    boolean getSuspendBelowMinBitrate() {
      return suspendBelowMinBitrate;
    }

    @CalledByNative("RTCConfiguration")
    Integer getScreencastMinBitrate() {
      return screencastMinBitrate;
    }

    @CalledByNative("RTCConfiguration")
    Boolean getCombinedAudioVideoBwe() {
      return combinedAudioVideoBwe;
    }

    @CalledByNative("RTCConfiguration")
    Boolean getEnableDtlsSrtp() {
      return enableDtlsSrtp;
    }

    @CalledByNative("RTCConfiguration")
    AdapterType getNetworkPreference() {
      return networkPreference;
    }
  };

  private final List<MediaStream> localStreams = new ArrayList<>();
  private final long nativePeerConnection;
  private List<RtpSender> senders = new ArrayList<>();
  private List<RtpReceiver> receivers = new ArrayList<>();

  /**
   * Wraps a PeerConnection created by the factory. Can be used by clients that want to implement
   * their PeerConnection creation in JNI.
   */
  public PeerConnection(NativePeerConnectionFactory factory) {
    this(factory.createNativePeerConnection());
  }

  PeerConnection(long nativePeerConnection) {
    this.nativePeerConnection = nativePeerConnection;
  }

  // JsepInterface.
  public SessionDescription getLocalDescription() {
    return nativeGetLocalDescription();
  }

  public SessionDescription getRemoteDescription() {
    return nativeGetRemoteDescription();
  }

  public DataChannel createDataChannel(String label, DataChannel.Init init) {
    return nativeCreateDataChannel(label, init);
  }

  public void createOffer(SdpObserver observer, MediaConstraints constraints) {
    nativeCreateOffer(observer, constraints);
  }

  public void createAnswer(SdpObserver observer, MediaConstraints constraints) {
    nativeCreateAnswer(observer, constraints);
  }

  public void setLocalDescription(SdpObserver observer, SessionDescription sdp) {
    nativeSetLocalDescription(observer, sdp);
  }

  public void setRemoteDescription(SdpObserver observer, SessionDescription sdp) {
    nativeSetRemoteDescription(observer, sdp);
  }

  // True if remote audio should be played out. Defaults to true.
  // Note that even if playout is enabled, streams will only be played out if
  // the appropriate SDP is also applied. The main purpose of this API is to
  // be able to control the exact time when audio playout starts.
  public void setAudioPlayout(boolean playout) {
    nativeSetAudioPlayout(playout);
  }

  // True if local audio shall be recorded. Defaults to true.
  // Note that even if recording is enabled, streams will only be recorded if
  // the appropriate SDP is also applied. The main purpose of this API is to
  // be able to control the exact time when audio recording starts.
  public void setAudioRecording(boolean recording) {
    nativeSetAudioRecording(recording);
  }

  public boolean setConfiguration(RTCConfiguration config) {
    return nativeSetConfiguration(config);
  }

  public boolean addIceCandidate(IceCandidate candidate) {
    return nativeAddIceCandidate(candidate.sdpMid, candidate.sdpMLineIndex, candidate.sdp);
  }

  public boolean removeIceCandidates(final IceCandidate[] candidates) {
    return nativeRemoveIceCandidates(candidates);
  }

  public boolean addStream(MediaStream stream) {
    boolean ret = nativeAddLocalStream(stream.nativeStream);
    if (!ret) {
      return false;
    }
    localStreams.add(stream);
    return true;
  }

  public void removeStream(MediaStream stream) {
    nativeRemoveLocalStream(stream.nativeStream);
    localStreams.remove(stream);
  }

  /**
   * Creates an RtpSender without a track.
   * <p>
   * This method allows an application to cause the PeerConnection to negotiate
   * sending/receiving a specific media type, but without having a track to
   * send yet.
   * <p>
   * When the application does want to begin sending a track, it can call
   * RtpSender.setTrack, which doesn't require any additional SDP negotiation.
   * <p>
   * Example use:
   * <pre>
   * {@code
   * audioSender = pc.createSender("audio", "stream1");
   * videoSender = pc.createSender("video", "stream1");
   * // Do normal SDP offer/answer, which will kick off ICE/DTLS and negotiate
   * // media parameters....
   * // Later, when the endpoint is ready to actually begin sending:
   * audioSender.setTrack(audioTrack, false);
   * videoSender.setTrack(videoTrack, false);
   * }
   * </pre>
   * Note: This corresponds most closely to "addTransceiver" in the official
   * WebRTC API, in that it creates a sender without a track. It was
   * implemented before addTransceiver because it provides useful
   * functionality, and properly implementing transceivers would have required
   * a great deal more work.
   *
   * @param kind      Corresponds to MediaStreamTrack kinds (must be "audio" or
   *                  "video").
   * @param stream_id The ID of the MediaStream that this sender's track will
   *                  be associated with when SDP is applied to the remote
   *                  PeerConnection. If createSender is used to create an
   *                  audio and video sender that should be synchronized, they
   *                  should use the same stream ID.
   * @return          A new RtpSender object if successful, or null otherwise.
   */
  public RtpSender createSender(String kind, String stream_id) {
    RtpSender new_sender = nativeCreateSender(kind, stream_id);
    if (new_sender != null) {
      senders.add(new_sender);
    }
    return new_sender;
  }

  // Note that calling getSenders will dispose of the senders previously
  // returned (and same goes for getReceivers).
  public List<RtpSender> getSenders() {
    for (RtpSender sender : senders) {
      sender.dispose();
    }
    senders = nativeGetSenders();
    return Collections.unmodifiableList(senders);
  }

  public List<RtpReceiver> getReceivers() {
    for (RtpReceiver receiver : receivers) {
      receiver.dispose();
    }
    receivers = nativeGetReceivers();
    return Collections.unmodifiableList(receivers);
  }

  // Older, non-standard implementation of getStats.
  @Deprecated
  public boolean getStats(StatsObserver observer, MediaStreamTrack track) {
    return nativeOldGetStats(observer, (track == null) ? 0 : track.nativeTrack);
  }

  // Gets stats using the new stats collection API, see webrtc/api/stats/. These
  // will replace old stats collection API when the new API has matured enough.
  public void getStats(RTCStatsCollectorCallback callback) {
    nativeNewGetStats(callback);
  }

  // Limits the bandwidth allocated for all RTP streams sent by this
  // PeerConnection. Pass null to leave a value unchanged.
  public boolean setBitrate(Integer min, Integer current, Integer max) {
    return nativeSetBitrate(min, current, max);
  }

  // Starts recording an RTC event log. Ownership of the file is transfered to
  // the native code. If an RTC event log is already being recorded, it will be
  // stopped and a new one will start using the provided file. Logging will
  // continue until the stopRtcEventLog function is called. The max_size_bytes
  // argument is ignored, it is added for future use.
  public boolean startRtcEventLog(int file_descriptor, int max_size_bytes) {
    return nativeStartRtcEventLog(file_descriptor, max_size_bytes);
  }

  // Stops recording an RTC event log. If no RTC event log is currently being
  // recorded, this call will have no effect.
  public void stopRtcEventLog() {
    nativeStopRtcEventLog();
  }

  // TODO(fischman): add support for DTMF-related methods once that API
  // stabilizes.
  public SignalingState signalingState() {
    return nativeSignalingState();
  }

  public IceConnectionState iceConnectionState() {
    return nativeIceConnectionState();
  }

  public IceGatheringState iceGatheringState() {
    return nativeIceGatheringState();
  }

  public void close() {
    nativeClose();
  }

  /**
   * Free native resources associated with this PeerConnection instance.
   * <p>
   * This method removes a reference count from the C++ PeerConnection object,
   * which should result in it being destroyed. It also calls equivalent
   * "dispose" methods on the Java objects attached to this PeerConnection
   * (streams, senders, receivers), such that their associated C++ objects
   * will also be destroyed.
   * <p>
   * Note that this method cannot be safely called from an observer callback
   * (PeerConnection.Observer, DataChannel.Observer, etc.). If you want to, for
   * example, destroy the PeerConnection after an "ICE failed" callback, you
   * must do this asynchronously (in other words, unwind the stack first). See
   * <a href="https://bugs.chromium.org/p/webrtc/issues/detail?id=3721">bug
   * 3721</a> for more details.
   */
  public void dispose() {
    close();
    for (MediaStream stream : localStreams) {
      nativeRemoveLocalStream(stream.nativeStream);
      stream.dispose();
    }
    localStreams.clear();
    for (RtpSender sender : senders) {
      sender.dispose();
    }
    senders.clear();
    for (RtpReceiver receiver : receivers) {
      receiver.dispose();
    }
    receivers.clear();
    nativeFreeOwnedPeerConnection(nativePeerConnection);
  }

  /** Returns a pointer to the native webrtc::PeerConnectionInterface. */
  public long getNativePeerConnection() {
    return nativeGetNativePeerConnection();
  }

  @CalledByNative
  long getNativeOwnedPeerConnection() {
    return nativePeerConnection;
  }

  public static long createNativePeerConnectionObserver(Observer observer) {
    return nativeCreatePeerConnectionObserver(observer);
  }

  private native long nativeGetNativePeerConnection();
  private native SessionDescription nativeGetLocalDescription();
  private native SessionDescription nativeGetRemoteDescription();
  private native DataChannel nativeCreateDataChannel(String label, DataChannel.Init init);
  private native void nativeCreateOffer(SdpObserver observer, MediaConstraints constraints);
  private native void nativeCreateAnswer(SdpObserver observer, MediaConstraints constraints);
  private native void nativeSetLocalDescription(SdpObserver observer, SessionDescription sdp);
  private native void nativeSetRemoteDescription(SdpObserver observer, SessionDescription sdp);
  private native void nativeSetAudioPlayout(boolean playout);
  private native void nativeSetAudioRecording(boolean recording);
  private native boolean nativeSetBitrate(Integer min, Integer current, Integer max);
  private native SignalingState nativeSignalingState();
  private native IceConnectionState nativeIceConnectionState();
  private native IceGatheringState nativeIceGatheringState();
  private native void nativeClose();
  private static native long nativeCreatePeerConnectionObserver(Observer observer);
  private static native void nativeFreeOwnedPeerConnection(long ownedPeerConnection);
  private native boolean nativeSetConfiguration(RTCConfiguration config);
  private native boolean nativeAddIceCandidate(
      String sdpMid, int sdpMLineIndex, String iceCandidateSdp);
  private native boolean nativeRemoveIceCandidates(final IceCandidate[] candidates);
  private native boolean nativeAddLocalStream(long stream);
  private native void nativeRemoveLocalStream(long stream);
  private native boolean nativeOldGetStats(StatsObserver observer, long nativeTrack);
  private native void nativeNewGetStats(RTCStatsCollectorCallback callback);
  private native RtpSender nativeCreateSender(String kind, String stream_id);
  private native List<RtpSender> nativeGetSenders();
  private native List<RtpReceiver> nativeGetReceivers();
  private native boolean nativeStartRtcEventLog(int file_descriptor, int max_size_bytes);
  private native void nativeStopRtcEventLog();
}

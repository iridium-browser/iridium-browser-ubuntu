/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "WebRTC/RTCVideoCodecFactory.h"

#import "WebRTC/RTCVideoCodec.h"
#import "WebRTC/RTCVideoCodecH264.h"
#if defined(USE_BUILTIN_SW_CODECS)
#import "WebRTC/RTCVideoEncoderVP8.h"  // nogncheck
#if !defined(RTC_DISABLE_VP9)
#import "WebRTC/RTCVideoEncoderVP9.h"  // nogncheck
#endif
#endif

@implementation RTCDefaultVideoEncoderFactory

@synthesize preferredCodec;

+ (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  NSDictionary<NSString *, NSString *> *constrainedHighParams = @{
    @"profile-level-id" : kRTCLevel31ConstrainedHigh,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTCVideoCodecInfo *constrainedHighInfo =
      [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH264Name
                                   parameters:constrainedHighParams];

  NSDictionary<NSString *, NSString *> *constrainedBaselineParams = @{
    @"profile-level-id" : kRTCLevel31ConstrainedBaseline,
    @"level-asymmetry-allowed" : @"1",
    @"packetization-mode" : @"1",
  };
  RTCVideoCodecInfo *constrainedBaselineInfo =
      [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecH264Name
                                   parameters:constrainedBaselineParams];

#if defined(USE_BUILTIN_SW_CODECS)
  RTCVideoCodecInfo *vp8Info = [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp8Name];

#if !defined(RTC_DISABLE_VP9)
  RTCVideoCodecInfo *vp9Info = [[RTCVideoCodecInfo alloc] initWithName:kRTCVideoCodecVp9Name];
#endif
#endif

  return @[
    constrainedHighInfo,
    constrainedBaselineInfo,
#if defined(USE_BUILTIN_SW_CODECS)
    vp8Info,
#if !defined(RTC_DISABLE_VP9)
    vp9Info,
#endif
#endif
  ];
}

- (id<RTCVideoEncoder>)createEncoder:(RTCVideoCodecInfo *)info {
  if ([info.name isEqualToString:kRTCVideoCodecH264Name]) {
    return [[RTCVideoEncoderH264 alloc] initWithCodecInfo:info];
#if defined(USE_BUILTIN_SW_CODECS)
  } else if ([info.name isEqualToString:kRTCVideoCodecVp8Name]) {
    return [RTCVideoEncoderVP8 vp8Encoder];
#if !defined(RTC_DISABLE_VP9)
  } else if ([info.name isEqualToString:kRTCVideoCodecVp9Name]) {
    return [RTCVideoEncoderVP9 vp9Encoder];
#endif
#endif
  }

  return nil;
}

- (NSArray<RTCVideoCodecInfo *> *)supportedCodecs {
  NSMutableArray<RTCVideoCodecInfo *> *codecs = [[[self class] supportedCodecs] mutableCopy];

  NSMutableArray<RTCVideoCodecInfo *> *orderedCodecs = [NSMutableArray array];
  NSUInteger index = [codecs indexOfObject:self.preferredCodec];
  if (index != NSNotFound) {
    [orderedCodecs addObject:[codecs objectAtIndex:index]];
    [codecs removeObjectAtIndex:index];
  }
  [orderedCodecs addObjectsFromArray:codecs];

  return [orderedCodecs copy];
}

@end

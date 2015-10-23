/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDSignalingMessage.h"

#import "RTCLogging.h"

#import "ARDUtilities.h"
#import "RTCICECandidate+JSON.h"
#import "RTCSessionDescription+JSON.h"

static NSString const *kARDSignalingMessageTypeKey = @"type";

@implementation ARDSignalingMessage

@synthesize type = _type;

- (instancetype)initWithType:(ARDSignalingMessageType)type {
  if (self = [super init]) {
    _type = type;
  }
  return self;
}

- (NSString *)description {
  return [[NSString alloc] initWithData:[self JSONData]
                               encoding:NSUTF8StringEncoding];
}

+ (ARDSignalingMessage *)messageFromJSONString:(NSString *)jsonString {
  NSDictionary *values = [NSDictionary dictionaryWithJSONString:jsonString];
  if (!values) {
    RTCLogError(@"Error parsing signaling message JSON.");
    return nil;
  }

  NSString *typeString = values[kARDSignalingMessageTypeKey];
  ARDSignalingMessage *message = nil;
  if ([typeString isEqualToString:@"candidate"]) {
    RTCICECandidate *candidate =
        [RTCICECandidate candidateFromJSONDictionary:values];
    message = [[ARDICECandidateMessage alloc] initWithCandidate:candidate];
  } else if ([typeString isEqualToString:@"offer"] ||
             [typeString isEqualToString:@"answer"]) {
    RTCSessionDescription *description =
        [RTCSessionDescription descriptionFromJSONDictionary:values];
    message =
        [[ARDSessionDescriptionMessage alloc] initWithDescription:description];
  } else if ([typeString isEqualToString:@"bye"]) {
    message = [[ARDByeMessage alloc] init];
  } else {
    RTCLogError(@"Unexpected type: %@", typeString);
  }
  return message;
}

- (NSData *)JSONData {
  return nil;
}

@end

@implementation ARDICECandidateMessage

@synthesize candidate = _candidate;

- (instancetype)initWithCandidate:(RTCICECandidate *)candidate {
  if (self = [super initWithType:kARDSignalingMessageTypeCandidate]) {
    _candidate = candidate;
  }
  return self;
}

- (NSData *)JSONData {
  return [_candidate JSONData];
}

@end

@implementation ARDSessionDescriptionMessage

@synthesize sessionDescription = _sessionDescription;

- (instancetype)initWithDescription:(RTCSessionDescription *)description {
  ARDSignalingMessageType type = kARDSignalingMessageTypeOffer;
  NSString *typeString = description.type;
  if ([typeString isEqualToString:@"offer"]) {
    type = kARDSignalingMessageTypeOffer;
  } else if ([typeString isEqualToString:@"answer"]) {
    type = kARDSignalingMessageTypeAnswer;
  } else {
    NSAssert(NO, @"Unexpected type: %@", typeString);
  }
  if (self = [super initWithType:type]) {
    _sessionDescription = description;
  }
  return self;
}

- (NSData *)JSONData {
  return [_sessionDescription JSONData];
}

@end

@implementation ARDByeMessage

- (instancetype)init {
  return [super initWithType:kARDSignalingMessageTypeBye];
}

- (NSData *)JSONData {
  NSDictionary *message = @{
    @"type": @"bye"
  };
  return [NSJSONSerialization dataWithJSONObject:message
                                         options:NSJSONWritingPrettyPrinted
                                           error:NULL];
}

@end

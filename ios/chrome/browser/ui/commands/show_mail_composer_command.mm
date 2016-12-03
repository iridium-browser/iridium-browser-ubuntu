// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/show_mail_composer_command.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"

@implementation ShowMailComposerCommand {
  base::scoped_nsobject<NSArray> _toRecipients;
  base::scoped_nsobject<NSString> _subject;
  base::scoped_nsobject<NSString> _body;
  base::FilePath _textFileToAttach;
}

@synthesize emailNotConfiguredAlertTitleId = _emailNotConfiguredAlertTitleId;
@synthesize emailNotConfiguredAlertMessageId =
    _emailNotConfiguredAlertMessageId;

- (instancetype)initWithTag:(NSInteger)tag {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithToRecipient:(NSString*)toRecipient
                             subject:(NSString*)subject
                                body:(NSString*)body
      emailNotConfiguredAlertTitleId:(int)alertTitleId
    emailNotConfiguredAlertMessageId:(int)alertMessageId {
  DCHECK(alertTitleId);
  DCHECK(alertMessageId);
  self = [super initWithTag:IDC_SHOW_MAIL_COMPOSER];
  if (self) {
    _toRecipients.reset([@[ toRecipient ] retain]);
    _subject.reset([subject copy]);
    _body.reset([body copy]);
    _emailNotConfiguredAlertTitleId = alertTitleId;
    _emailNotConfiguredAlertMessageId = alertMessageId;
  }
  return self;
}

- (NSArray*)toRecipients {
  return _toRecipients.get();
}

- (NSString*)subject {
  return _subject.get();
}

- (NSString*)body {
  return _body.get();
}

- (const base::FilePath&)textFileToAttach {
  return _textFileToAttach;
}

- (void)setTextFileToAttach:(const base::FilePath&)textFileToAttach {
  _textFileToAttach = textFileToAttach;
}

@end

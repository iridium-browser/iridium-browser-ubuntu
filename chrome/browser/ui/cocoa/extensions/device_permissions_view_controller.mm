// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/extensions/device_permissions_dialog_controller.h"
#import "chrome/browser/ui/cocoa/extensions/device_permissions_view_controller.h"
#include "chrome/grit/generated_resources.h"
#import "ui/base/l10n/l10n_util_mac.h"

using extensions::DevicePermissionsPrompt;

@implementation DevicePermissionsViewController

- (id)initWithController:(DevicePermissionsDialogController*)controller
                  prompt:
                      (scoped_refptr<DevicePermissionsPrompt::Prompt>)prompt {
  if ((self = [super initWithNibName:@"DevicePermissionsPrompt"
                              bundle:base::mac::FrameworkBundle()])) {
    controller_ = controller;
    prompt_ = prompt;
  }
  return self;
}

- (IBAction)cancel:(id)sender {
  controller_->Dismissed();
}

- (IBAction)ok:(id)sender {
  [[tableView_ selectedRowIndexes]
      enumerateIndexesUsingBlock:^(NSUInteger index, BOOL* stop) {
          prompt_->GrantDevicePermission(index);
      }];
  controller_->Dismissed();
}

- (void)devicesChanged {
  [tableView_ reloadData];
}

- (void)awakeFromNib {
  [titleField_ setStringValue:base::SysUTF16ToNSString(prompt_->GetHeading())];
  [promptField_
      setStringValue:base::SysUTF16ToNSString(prompt_->GetPromptMessage())];
  [tableView_ setAllowsMultipleSelection:prompt_->multiple()];
  [[deviceNameColumn_ headerCell]
      setStringValue:l10n_util::GetNSString(
                         IDS_DEVICE_PERMISSIONS_DIALOG_DEVICE_NAME_COLUMN)];
  [[serialNumberColumn_ headerCell]
      setStringValue:l10n_util::GetNSString(
                         IDS_DEVICE_PERMISSIONS_DIALOG_SERIAL_NUMBER_COLUMN)];
  [okButton_
      setTitle:l10n_util::GetNSString(IDS_DEVICE_PERMISSIONS_DIALOG_SELECT)];
  [cancelButton_ setTitle:l10n_util::GetNSString(IDS_CANCEL)];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView*)tableView {
  DCHECK_EQ(tableView_, tableView);
  return prompt_->GetDeviceCount();
}

- (id)tableView:(NSTableView*)tableView
    objectValueForTableColumn:(NSTableColumn*)tableColumn
                          row:(NSInteger)rowIndex {
  if (tableColumn == deviceNameColumn_) {
    return base::SysUTF16ToNSString(prompt_->GetDeviceName(rowIndex));
  } else if (tableColumn == serialNumberColumn_) {
    return base::SysUTF16ToNSString(prompt_->GetDeviceSerialNumber(rowIndex));
  } else {
    NOTREACHED();
    return @"";
  }
}

@end

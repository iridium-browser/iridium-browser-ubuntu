// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SUGGESTIONS_SUGGESTIONS_COLLECTION_UPDATER_H_
#define IOS_CHROME_BROWSER_UI_SUGGESTIONS_SUGGESTIONS_COLLECTION_UPDATER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"

@class SuggestionsViewController;

// Enum defining the ItemType of this SuggestionsCollectionUpdater.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeText = kItemTypeEnumZero,
  ItemTypeArticle,
  ItemTypeExpand,
  ItemTypeStack,
};

// Updater for a CollectionViewController populating it with some items and
// handling the items addition.
@interface SuggestionsCollectionUpdater : NSObject

// |collectionViewController| this Updater will update. Needs to be set before
// adding items.
@property(nonatomic, assign)
    SuggestionsViewController* collectionViewController;

// Adds a text item with a |title| and a |subtitle| in the section numbered
// |section|. If |section| is greater than the current number of section, it
// will add a new section at the end.
- (void)addTextItem:(NSString*)title
           subtitle:(NSString*)subtitle
          toSection:(NSInteger)inputSection;

// Returns whether the section should use the default, non-card style.
- (BOOL)shouldUseCustomStyleForSection:(NSInteger)section;

@end

#endif  // IOS_CHROME_BROWSER_UI_SUGGESTIONS_SUGGESTIONS_COLLECTION_UPDATER_H_

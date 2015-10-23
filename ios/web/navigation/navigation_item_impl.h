// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_
#define IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_

#import <Foundation/Foundation.h>

#include "base/basictypes.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ios/web/navigation/navigation_item_facade_delegate.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/ssl_status.h"
#include "url/gurl.h"

namespace web {

class NavigationItemFacadeDelegate;

// Implementation of NavigationItem.
class NavigationItemImpl : public web::NavigationItem {
 public:
  // Creates a default NavigationItemImpl.
  NavigationItemImpl();
  ~NavigationItemImpl() override;

  // Since NavigationItemImpls own their facade delegates, there is no implicit
  // copy constructor (scoped_ptrs can't be copied), so one is defined here.
  NavigationItemImpl(const NavigationItemImpl& item);

  // Accessors for the delegate used to drive the navigation entry facade.
  // NOTE: to minimize facade synchronization code, NavigationItems take
  // ownership of their facade delegates.
  void SetFacadeDelegate(
      scoped_ptr<NavigationItemFacadeDelegate> facade_delegate);
  NavigationItemFacadeDelegate* GetFacadeDelegate() const;

  // NavigationItem implementation:
  int GetUniqueID() const override;
  void SetURL(const GURL& url) override;
  const GURL& GetURL() const override;
  void SetReferrer(const web::Referrer& referrer) override;
  const web::Referrer& GetReferrer() const override;
  void SetVirtualURL(const GURL& url) override;
  const GURL& GetVirtualURL() const override;
  void SetTitle(const base::string16& title) override;
  const base::string16& GetTitle() const override;
  void SetPageDisplayState(const PageDisplayState& display_state) override;
  const PageDisplayState& GetPageDisplayState() const override;
  const base::string16& GetTitleForDisplay(
      const std::string& languages) const override;
  void SetTransitionType(ui::PageTransition transition_type) override;
  ui::PageTransition GetTransitionType() const override;
  const FaviconStatus& GetFavicon() const override;
  FaviconStatus& GetFavicon() override;
  const SSLStatus& GetSSL() const override;
  SSLStatus& GetSSL() override;
  void SetTimestamp(base::Time timestamp) override;
  base::Time GetTimestamp() const override;
  void SetUnsafe(bool is_unsafe) override;
  bool IsUnsafe() const override;
  void SetIsOverridingUserAgent(bool is_overriding_user_agent) override;
  bool IsOverridingUserAgent() const override;
  bool HasPostData() const override;
  NSDictionary* GetHttpRequestHeaders() const override;
  void AddHttpRequestHeaders(NSDictionary* additional_headers) override;

  // Serialized representation of the state object that was used in conjunction
  // with a JavaScript window.history.pushState() or
  // window.history.replaceState() call that created or modified this
  // CRWSessionEntry. Intended to be used for JavaScript history operations and
  // will be nil in most cases.
  void SetSerializedStateObject(NSString* serialized_state_object);
  NSString* GetSerializedStateObject() const;

  // Whether or not this item was created by calling history.pushState().
  void SetIsCreatedFromPushState(bool push_state);
  bool IsCreatedFromPushState() const;

  // Whether or not to bypass showing the resubmit data confirmation when
  // loading a POST request. Set to YES for browser-generated POST requests.
  void SetShouldSkipResubmitDataConfirmation(bool skip);
  bool ShouldSkipResubmitDataConfirmation() const;

  // Data submitted with a POST request, persisted for resubmits.
  void SetPostData(NSData* post_data);
  NSData* GetPostData() const;

  // Removes the header for |key| from |http_request_headers_|.
  void RemoveHttpRequestHeaderForKey(NSString* key);

  // Removes all http headers from |http_request_headers_|.
  void ResetHttpRequestHeaders();

  // Once a navigation item is committed, we should no longer track
  // non-persisted state, as documented on the members below.
  void ResetForCommit();

  // Whether this (pending) navigation is renderer-initiated.  Resets to false
  // for all types of navigations after commit.
  void set_is_renderer_initiated(bool is_renderer_initiated) {
    is_renderer_initiated_ = is_renderer_initiated;
  }
  bool is_renderer_initiated() const { return is_renderer_initiated_; }

 private:
  int unique_id_;
  GURL url_;
  Referrer referrer_;
  GURL virtual_url_;
  base::string16 title_;
  PageDisplayState page_display_state_;
  ui::PageTransition transition_type_;
  FaviconStatus favicon_;
  SSLStatus ssl_;
  base::Time timestamp_;
  bool is_overriding_user_agent_;
  base::scoped_nsobject<NSMutableDictionary> http_request_headers_;

  base::scoped_nsobject<NSString> serialized_state_object_;
  bool is_created_from_push_state_;
  bool should_skip_resubmit_data_confirmation_;
  base::scoped_nsobject<NSData> post_data_;

  // Whether the item, while loading, was created for a renderer-initiated
  // navigation.  This dictates whether the URL should be displayed before the
  // navigation commits.  It is cleared in |ResetForCommit| and not persisted.
  bool is_renderer_initiated_;

  // Whether the navigation contains unsafe resources.
  bool is_unsafe_;

  // This is a cached version of the result of GetTitleForDisplay. When the URL,
  // virtual URL, or title is set, this should be cleared to force a refresh.
  mutable base::string16 cached_display_title_;

  // Weak pointer to the facade delegate.
  scoped_ptr<NavigationItemFacadeDelegate> facade_delegate_;

  // Copy and assignment is explicitly allowed for this class.
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_NAVIGATION_ITEM_IMPL_H_

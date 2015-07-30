// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_DATA_H_
#define EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_DATA_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/coalesced_permission_message.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permission_set.h"

class GURL;

namespace extensions {
class Extension;
class URLPatternSet;

// A container for the permissions state of an extension, including active,
// withheld, and tab-specific permissions.
class PermissionsData {
 public:
  // The possible types of access for a given frame.
  enum AccessType {
    ACCESS_DENIED,   // The extension is not allowed to access the given page.
    ACCESS_ALLOWED,  // The extension is allowed to access the given page.
    ACCESS_WITHHELD  // The browser must determine if the extension can access
                     // the given page.
  };

  using TabPermissionsMap = std::map<int, scoped_refptr<const PermissionSet>>;

  // Delegate class to allow different contexts (e.g. browser vs renderer) to
  // have control over policy decisions.
  class PolicyDelegate {
   public:
    virtual ~PolicyDelegate() {}

    // Returns false if script access should be blocked on this page.
    // Otherwise, default policy should decide.
    virtual bool CanExecuteScriptOnPage(const Extension* extension,
                                        const GURL& document_url,
                                        const GURL& top_document_url,
                                        int tab_id,
                                        int process_id,
                                        std::string* error) = 0;
  };

  static void SetPolicyDelegate(PolicyDelegate* delegate);

  PermissionsData(const Extension* extension);
  virtual ~PermissionsData();

  // Returns true if the extension is a COMPONENT extension or is on the
  // whitelist of extensions that can script all pages.
  static bool CanExecuteScriptEverywhere(const Extension* extension);

  // Returns true if the --scripts-require-action flag would possibly affect
  // the given |extension| and |permissions|. We pass in the |permissions|
  // explicitly, as we may need to check with permissions other than the ones
  // that are currently on the extension's PermissionsData.
  static bool ScriptsMayRequireActionForExtension(
      const Extension* extension,
      const PermissionSet* permissions);

  // Returns true if we should skip the permissions warning for the extension
  // with the given |extension_id|.
  static bool ShouldSkipPermissionWarnings(const std::string& extension_id);

  // Returns true if the given |url| is restricted for the given |extension|,
  // as is commonly the case for chrome:// urls.
  // NOTE: You probably want to use CanAccessPage().
  static bool IsRestrictedUrl(const GURL& document_url,
                              const GURL& top_frame_url,
                              const Extension* extension,
                              std::string* error);

  // Sets the runtime permissions of the given |extension| to |active| and
  // |withheld|.
  void SetPermissions(const scoped_refptr<const PermissionSet>& active,
                      const scoped_refptr<const PermissionSet>& withheld) const;

  // Updates the tab-specific permissions of |tab_id| to include those from
  // |permissions|.
  void UpdateTabSpecificPermissions(
      int tab_id,
      scoped_refptr<const PermissionSet> permissions) const;

  // Clears the tab-specific permissions of |tab_id|.
  void ClearTabSpecificPermissions(int tab_id) const;

  // Returns true if the |extension| has the given |permission|. Prefer
  // IsExtensionWithPermissionOrSuggestInConsole when developers may be using an
  // api that requires a permission they didn't know about, e.g. open web apis.
  // Note this does not include APIs with no corresponding permission, like
  // "runtime" or "browserAction".
  // TODO(mpcomplete): drop the "API" from these names, it's confusing.
  bool HasAPIPermission(APIPermission::ID permission) const;
  bool HasAPIPermission(const std::string& permission_name) const;
  bool HasAPIPermissionForTab(int tab_id, APIPermission::ID permission) const;
  bool CheckAPIPermissionWithParam(
      APIPermission::ID permission,
      const APIPermission::CheckParam* param) const;

  // Returns the hosts this extension effectively has access to, including
  // explicit and scriptable hosts, and any hosts on tabs the extension has
  // active tab permissions for.
  URLPatternSet GetEffectiveHostPermissions() const;

  // TODO(rdevlin.cronin): HasHostPermission() and
  // HasEffectiveAccessToAllHosts() are just forwards for the active
  // permissions. We should either get rid of these, and have callers use
  // active_permissions(), or should get rid of active_permissions(), and make
  // callers use PermissionsData for everything. We should not do both.

  // Whether the extension has access to the given |url|.
  bool HasHostPermission(const GURL& url) const;

  // Whether the extension has effective access to all hosts. This is true if
  // there is a content script that matches all hosts, if there is a host
  // permission grants access to all hosts (like <all_urls>) or an api
  // permission that effectively grants access to all hosts (e.g. proxy,
  // network, etc.)
  bool HasEffectiveAccessToAllHosts() const;

  // Returns the full list of legacy permission message IDs.
  // Deprecated. You DO NOT want to call this!
  // TODO(treib): Remove once we've switched to the new system.
  PermissionMessageIDs GetLegacyPermissionMessageIDs() const;

  // Returns the full list of permission messages that should display at install
  // time, including their submessages, as strings.
  // TODO(treib): Remove this and move callers over to
  // GetCoalescedPermissionMessages once we've fully switched to the new system.
  PermissionMessageStrings GetPermissionMessageStrings() const;

  // Returns the full list of permission details for messages that should
  // display at install time, in a nested format ready for display.
  CoalescedPermissionMessages GetCoalescedPermissionMessages() const;

  // Returns true if the extension has requested all-hosts permissions (or
  // something close to it), but has had it withheld.
  bool HasWithheldImpliedAllHosts() const;

  // Returns true if the |extension| has permission to access and interact with
  // the specified page, in order to do things like inject scripts or modify
  // the content.
  // If this returns false and |error| is non-NULL, |error| will be popualted
  // with the reason the extension cannot access the page.
  bool CanAccessPage(const Extension* extension,
                     const GURL& document_url,
                     const GURL& top_document_url,
                     int tab_id,
                     int process_id,
                     std::string* error) const;
  // Like CanAccessPage, but also takes withheld permissions into account.
  // TODO(rdevlin.cronin) We shouldn't have two functions, but not all callers
  // know how to wait for permission.
  AccessType GetPageAccess(const Extension* extension,
                           const GURL& document_url,
                           const GURL& top_document_url,
                           int tab_id,
                           int process_id,
                           std::string* error) const;

  // Returns true if the |extension| has permission to inject a content script
  // on the page.
  // If this returns false and |error| is non-NULL, |error| will be popualted
  // with the reason the extension cannot script the page.
  // NOTE: You almost certainly want to use CanAccessPage() instead of this
  // method.
  bool CanRunContentScriptOnPage(const Extension* extension,
                                 const GURL& document_url,
                                 const GURL& top_document_url,
                                 int tab_id,
                                 int process_id,
                                 std::string* error) const;
  // Like CanRunContentScriptOnPage, but also takes withheld permissions into
  // account.
  // TODO(rdevlin.cronin) We shouldn't have two functions, but not all callers
  // know how to wait for permission.
  AccessType GetContentScriptAccess(const Extension* extension,
                                    const GURL& document_url,
                                    const GURL& top_document_url,
                                    int tab_id,
                                    int process_id,
                                    std::string* error) const;

  // Returns true if extension is allowed to obtain the contents of a page as
  // an image. Since a page may contain sensitive information, this is
  // restricted to the extension's host permissions as well as the extension
  // page itself.
  bool CanCaptureVisiblePage(int tab_id, std::string* error) const;

  // Returns the tab permissions map.
  TabPermissionsMap CopyTabSpecificPermissionsMap() const;

  scoped_refptr<const PermissionSet> active_permissions() const {
    // We lock so that we can't also be setting the permissions while returning.
    base::AutoLock auto_lock(runtime_lock_);
    return active_permissions_unsafe_;
  }

  scoped_refptr<const PermissionSet> withheld_permissions() const {
    // We lock so that we can't also be setting the permissions while returning.
    base::AutoLock auto_lock(runtime_lock_);
    return withheld_permissions_unsafe_;
  }

#if defined(UNIT_TEST)
  scoped_refptr<const PermissionSet> GetTabSpecificPermissionsForTesting(
      int tab_id) const {
    return GetTabSpecificPermissions(tab_id);
  }
#endif

 private:
  // Gets the tab-specific host permissions of |tab_id|, or NULL if there
  // aren't any.
  scoped_refptr<const PermissionSet> GetTabSpecificPermissions(
      int tab_id) const;

  // Returns true if the |extension| has tab-specific permission to operate on
  // the tab specified by |tab_id| with the given |url|.
  // Note that if this returns false, it doesn't mean the extension can't run on
  // the given tab, only that it does not have tab-specific permission to do so.
  bool HasTabSpecificPermissionToExecuteScript(int tab_id,
                                               const GURL& url) const;

  // Returns whether or not the extension is permitted to run on the given page,
  // checking against |permitted_url_patterns| in addition to blocking special
  // sites (like the webstore or chrome:// urls).
  AccessType CanRunOnPage(const Extension* extension,
                          const GURL& document_url,
                          const GURL& top_document_url,
                          int tab_id,
                          int process_id,
                          const URLPatternSet& permitted_url_patterns,
                          const URLPatternSet& withheld_url_patterns,
                          std::string* error) const;

  // The associated extension's id.
  std::string extension_id_;

  // The associated extension's manifest type.
  Manifest::Type manifest_type_;

  mutable base::Lock runtime_lock_;

  // The permission's which are currently active on the extension during
  // runtime.
  // Unsafe indicates that we must lock anytime this is directly accessed.
  // Unless you need to change |active_permissions_unsafe_|, use the (safe)
  // active_permissions() accessor.
  mutable scoped_refptr<const PermissionSet> active_permissions_unsafe_;

  // The permissions the extension requested, but was not granted due because
  // they are too powerful. This includes things like all_hosts.
  // Unsafe indicates that we must lock anytime this is directly accessed.
  // Unless you need to change |withheld_permissions_unsafe_|, use the (safe)
  // withheld_permissions() accessor.
  mutable scoped_refptr<const PermissionSet> withheld_permissions_unsafe_;

  mutable TabPermissionsMap tab_specific_permissions_;

  DISALLOW_COPY_AND_ASSIGN(PermissionsData);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_PERMISSIONS_DATA_H_

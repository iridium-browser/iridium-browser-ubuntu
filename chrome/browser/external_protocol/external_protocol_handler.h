// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/shell_integration.h"
#include "ui/base/page_transition_types.h"

class GURL;
class PrefRegistrySimple;

namespace base {
class DictionaryValue;
}

class ExternalProtocolHandler {
 public:
  enum BlockState {
    DONT_BLOCK,
    BLOCK,
    UNKNOWN,
  };

  // Delegate to allow unit testing to provide different behavior.
  class Delegate {
   public:
    virtual scoped_refptr<shell_integration::DefaultProtocolClientWorker>
    CreateShellWorker(
        const shell_integration::DefaultWebClientWorkerCallback& callback,
        const std::string& protocol) = 0;
    virtual BlockState GetBlockState(const std::string& scheme) = 0;
    virtual void BlockRequest() = 0;
    virtual void RunExternalProtocolDialog(
        const GURL& url,
        int render_process_host_id,
        int routing_id,
        ui::PageTransition page_transition,
        bool has_user_gesture) = 0;
    virtual void LaunchUrlWithoutSecurityCheck(const GURL& url) = 0;
    virtual void FinishedProcessingCheck() = 0;
    virtual ~Delegate() {}
  };

  // Returns whether we should block a given scheme.
  static BlockState GetBlockState(const std::string& scheme);

  // Sets whether we should block a given scheme.
  static void SetBlockState(const std::string& scheme, BlockState state);

  // Checks to see if the protocol is allowed, if it is whitelisted,
  // the application associated with the protocol is launched on the io thread,
  // if it is blacklisted, returns silently. Otherwise, an
  // ExternalProtocolDialog is created asking the user. If the user accepts,
  // LaunchUrlWithoutSecurityCheck is called on the io thread and the
  // application is launched.
  // Must run on the UI thread.
  // Allowing use of a delegate to facilitate unit testing.
  static void LaunchUrlWithDelegate(const GURL& url,
                                    int render_process_host_id,
                                    int tab_contents_id,
                                    ui::PageTransition page_transition,
                                    bool has_user_gesture,
                                    Delegate* delegate);

  // Creates and runs a External Protocol dialog box.
  // |url| - The url of the request.
  // |render_process_host_id| and |routing_id| are used by
  // tab_util::GetWebContentsByID to aquire the tab contents associated with
  // this dialog.
  // NOTE: There is a race between the Time of Check and the Time Of Use for
  //       the command line. Since the caller (web page) does not have access
  //       to change the command line by itself, we do not do anything special
  //       to protect against this scenario.
  // This is implemented separately on each platform.
  static void RunExternalProtocolDialog(const GURL& url,
                                        int render_process_host_id,
                                        int routing_id,
                                        ui::PageTransition page_transition,
                                        bool has_user_gesture);

  // Register the ExcludedSchemes preference.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Starts a url using the external protocol handler with the help
  // of shellexecute. Should only be called if the protocol is whitelisted
  // (checked in LaunchUrl) or if the user explicitly allows it. (By selecting
  // "Launch Application" in an ExternalProtocolDialog.) It is assumed that the
  // url has already been escaped, which happens in LaunchUrl.
  // NOTE: You should Not call this function directly unless you are sure the
  // url you have has been checked against the blacklist, and has been escaped.
  // All calls to this function should originate in some way from LaunchUrl.
  static void LaunchUrlWithoutSecurityCheck(const GURL& url,
                                            int render_process_host_id,
                                            int tab_contents_id);

  // Prepopulates the dictionary with known protocols to deny or allow, if
  // preferences for them do not already exist.
  static void PrepopulateDictionary(base::DictionaryValue* win_pref);

  // Allows LaunchUrl to proceed with launching an external protocol handler.
  // This is typically triggered by a user gesture, but is also called for
  // each extension API function. Note that each call to LaunchUrl resets
  // the state to false (not allowed).
  static void PermitLaunchUrl();

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolHandler);
};

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_EXTERNAL_PROTOCOL_HANDLER_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_WEBUI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_WEBUI_MESSAGE_HANDLER_H_

#include <vector>

#include "base/macros.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/ui/webui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/media_router/media_sink_with_cast_modes.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

namespace media_router {

class Issue;
class MediaRoute;
class MediaRouterUI;

// The handler for Javascript messages related to the media router dialog.
class MediaRouterWebUIMessageHandler : public content::WebUIMessageHandler {
 public:
  explicit MediaRouterWebUIMessageHandler(MediaRouterUI* media_router_ui);
  ~MediaRouterWebUIMessageHandler() override;

  // Methods to update the status displayed by the dialog.
  void UpdateSinks(const std::vector<MediaSinkWithCastModes>& sinks);
  void UpdateRoutes(const std::vector<MediaRoute>& routes);
  void UpdateCastModes(const CastModeSet& cast_modes,
                       const std::string& source_host);
  void OnCreateRouteResponseReceived(const MediaSink::Id& sink_id,
                                     const MediaRoute* route);

  // Does not take ownership of |issue|. Note that |issue| can be nullptr, when
  // there are no more issues.
  void UpdateIssue(const Issue* issue);

 private:
  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Handlers for JavaScript messages.
  // In all cases, |args| consists of a single DictionaryValue containing the
  // actual parameters.
  // See media_router_ui_interface.js for documentation on parameters.
  void OnRequestInitialData(const base::ListValue* args);
  void OnCreateRoute(const base::ListValue* args);
  void OnActOnIssue(const base::ListValue* args);
  void OnCloseRoute(const base::ListValue* args);
  void OnCloseDialog(const base::ListValue* args);

  // Performs an action for an Issue of |type|.
  // |args| contains additional parameter that varies based on |type|.
  // Returns |true| if the action was successfully performed.
  bool ActOnIssueType(const IssueAction::Type& type,
                      const base::DictionaryValue* args);

  // Keeps track of whether a command to close the dialog has been issued.
  bool dialog_closing_;

  MediaRouterUI* media_router_ui_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterWebUIMessageHandler);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_WEBUI_MESSAGE_HANDLER_H_

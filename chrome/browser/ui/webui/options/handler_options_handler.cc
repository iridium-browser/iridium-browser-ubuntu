// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/handler_options_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

namespace options {

HandlerOptionsHandler::HandlerOptionsHandler() {
}

HandlerOptionsHandler::~HandlerOptionsHandler() {
}

void HandlerOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
      { "handlersTabLabel", IDS_HANDLERS_TAB_LABEL },
      { "handlersAllow", IDS_HANDLERS_ALLOW_RADIO },
      { "handlersBlock", IDS_HANDLERS_DONOTALLOW_RADIO },
      { "handlersTypeColumnHeader", IDS_HANDLERS_TYPE_COLUMN_HEADER },
      { "handlersSiteColumnHeader", IDS_HANDLERS_SITE_COLUMN_HEADER },
      { "handlersRemoveLink", IDS_HANDLERS_REMOVE_HANDLER_LINK },
      { "handlersNoneHandler", IDS_HANDLERS_NONE_HANDLER },
      { "handlersActiveHeading", IDS_HANDLERS_ACTIVE_HEADING },
      { "handlersIgnoredHeading", IDS_HANDLERS_IGNORED_HEADING },
  };
  RegisterTitle(localized_strings, "handlersPage",
                IDS_HANDLER_OPTIONS_WINDOW_TITLE);
  RegisterStrings(localized_strings, resources, arraysize(resources));

  localized_strings->SetString("handlersLearnMoreUrl",
                               chrome::kLearnMoreRegisterProtocolHandlerURL);
}

void HandlerOptionsHandler::InitializeHandler() {
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED,
      content::Source<Profile>(Profile::FromWebUI(web_ui())));
}

void HandlerOptionsHandler::InitializePage() {
  UpdateHandlerList();
}

void HandlerOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("clearDefault",
      base::Bind(&HandlerOptionsHandler::ClearDefault,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeHandler",
      base::Bind(&HandlerOptionsHandler::RemoveHandler,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setHandlersEnabled",
      base::Bind(&HandlerOptionsHandler::SetHandlersEnabled,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setDefault",
      base::Bind(&HandlerOptionsHandler::SetDefault,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("removeIgnoredHandler",
      base::Bind(&HandlerOptionsHandler::RemoveIgnoredHandler,
                 base::Unretained(this)));
}

ProtocolHandlerRegistry* HandlerOptionsHandler::GetProtocolHandlerRegistry() {
  return ProtocolHandlerRegistryFactory::GetForBrowserContext(
      Profile::FromWebUI(web_ui()));
}

static void GetHandlersAsListValue(
    const ProtocolHandlerRegistry::ProtocolHandlerList& handlers,
    base::ListValue* handler_list) {
  ProtocolHandlerRegistry::ProtocolHandlerList::const_iterator handler;
  for (handler = handlers.begin(); handler != handlers.end(); ++handler) {
    std::unique_ptr<base::ListValue> handler_value(new base::ListValue());
    handler_value->AppendString(handler->protocol());
    handler_value->AppendString(handler->url().spec());
    handler_value->AppendString(handler->url().host());
    handler_list->Append(std::move(handler_value));
  }
}

void HandlerOptionsHandler::GetHandlersForProtocol(
    const std::string& protocol,
    base::DictionaryValue* handlers_value) {
  ProtocolHandlerRegistry* registry = GetProtocolHandlerRegistry();
  // The items which are to be written into |handlers_value| are also described
  // in chrome/browser/resources/options/handler_options.js in @typedef
  // for Handlers. Please update them whenever you add or remove any keys here.
  handlers_value->SetString("protocol", protocol);
  handlers_value->SetInteger("default_handler",
      registry->GetHandlerIndex(protocol));
  handlers_value->SetBoolean(
      "is_default_handler_set_by_user",
      registry->IsRegisteredByUser(registry->GetHandlerFor(protocol)));
  handlers_value->SetBoolean("has_policy_recommendations",
                             registry->HasPolicyRegisteredHandler(protocol));

  base::ListValue* handlers_list = new base::ListValue();
  GetHandlersAsListValue(registry->GetHandlersFor(protocol), handlers_list);
  handlers_value->Set("handlers", handlers_list);
}

void HandlerOptionsHandler::GetIgnoredHandlers(base::ListValue* handlers) {
  ProtocolHandlerRegistry* registry = GetProtocolHandlerRegistry();
  ProtocolHandlerRegistry::ProtocolHandlerList ignored_handlers =
      registry->GetIgnoredHandlers();
  return GetHandlersAsListValue(ignored_handlers, handlers);
}

void HandlerOptionsHandler::UpdateHandlerList() {
  ProtocolHandlerRegistry* registry = GetProtocolHandlerRegistry();
  std::vector<std::string> protocols;
  registry->GetRegisteredProtocols(&protocols);

  base::ListValue handlers;
  for (std::vector<std::string>::iterator protocol = protocols.begin();
       protocol != protocols.end(); protocol++) {
    std::unique_ptr<base::DictionaryValue> handler_value(
        new base::DictionaryValue());
    GetHandlersForProtocol(*protocol, handler_value.get());
    handlers.Append(std::move(handler_value));
  }

  std::unique_ptr<base::ListValue> ignored_handlers(new base::ListValue());
  GetIgnoredHandlers(ignored_handlers.get());
  web_ui()->CallJavascriptFunctionUnsafe("HandlerOptions.setHandlers",
                                         handlers);
  web_ui()->CallJavascriptFunctionUnsafe("HandlerOptions.setIgnoredHandlers",
                                         *ignored_handlers);
}

void HandlerOptionsHandler::RemoveHandler(const base::ListValue* args) {
  const base::ListValue* list;
  if (!args->GetList(0, &list)) {
    NOTREACHED();
    return;
  }

  ProtocolHandler handler(ParseHandlerFromArgs(list));
  GetProtocolHandlerRegistry()->RemoveHandler(handler);

  // No need to call UpdateHandlerList() - we should receive a notification
  // that the ProtocolHandlerRegistry has changed and we will update the view
  // then.
}

void HandlerOptionsHandler::RemoveIgnoredHandler(const base::ListValue* args) {
  const base::ListValue* list;
  if (!args->GetList(0, &list)) {
    NOTREACHED();
    return;
  }

  ProtocolHandler handler(ParseHandlerFromArgs(list));
  GetProtocolHandlerRegistry()->RemoveIgnoredHandler(handler);
}

void HandlerOptionsHandler::SetHandlersEnabled(const base::ListValue* args) {
  bool enabled = true;
  CHECK(args->GetBoolean(0, &enabled));
  if (enabled)
    GetProtocolHandlerRegistry()->Enable();
  else
    GetProtocolHandlerRegistry()->Disable();
}

void HandlerOptionsHandler::ClearDefault(const base::ListValue* args) {
  const base::Value* value;
  CHECK(args->Get(0, &value));
  std::string protocol_to_clear;
  CHECK(value->GetAsString(&protocol_to_clear));
  GetProtocolHandlerRegistry()->ClearDefault(protocol_to_clear);
}

void HandlerOptionsHandler::SetDefault(const base::ListValue* args) {
  const base::ListValue* list;
  CHECK(args->GetList(0, &list));
  const ProtocolHandler& handler(ParseHandlerFromArgs(list));
  CHECK(!handler.IsEmpty());
  GetProtocolHandlerRegistry()->OnAcceptRegisterProtocolHandler(handler);
}

ProtocolHandler HandlerOptionsHandler::ParseHandlerFromArgs(
    const base::ListValue* args) const {
  base::string16 protocol;
  base::string16 url;
  bool ok = args->GetString(0, &protocol) && args->GetString(1, &url);
  if (!ok)
    return ProtocolHandler::EmptyProtocolHandler();
  return ProtocolHandler::CreateProtocolHandler(base::UTF16ToUTF8(protocol),
                                                GURL(base::UTF16ToUTF8(url)));
}

void HandlerOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PROTOCOL_HANDLER_REGISTRY_CHANGED, type);
  UpdateHandlerList();
}

}  // namespace options

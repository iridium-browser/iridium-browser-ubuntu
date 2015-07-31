// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "chrome/common/extensions/api/input_ime/input_components_handler.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

namespace input_ime = extensions::api::input_ime;
namespace KeyEventHandled = extensions::api::input_ime::KeyEventHandled;
namespace DeleteSurroundingText =
    extensions::api::input_ime::DeleteSurroundingText;
namespace UpdateMenuItems = extensions::api::input_ime::UpdateMenuItems;
namespace SendKeyEvents = extensions::api::input_ime::SendKeyEvents;
namespace HideInputView = extensions::api::input_ime::HideInputView;
namespace SetMenuItems = extensions::api::input_ime::SetMenuItems;
namespace SetCursorPosition = extensions::api::input_ime::SetCursorPosition;
namespace SetCandidates = extensions::api::input_ime::SetCandidates;
namespace SetCandidateWindowProperties =
    extensions::api::input_ime::SetCandidateWindowProperties;
namespace CommitText = extensions::api::input_ime::CommitText;
namespace ClearComposition = extensions::api::input_ime::ClearComposition;
namespace SetComposition = extensions::api::input_ime::SetComposition;
using chromeos::InputMethodEngineInterface;

namespace {

const char kErrorEngineNotAvailable[] = "Engine is not available";
const char kErrorSetMenuItemsFail[] = "Could not create menu Items";
const char kErrorUpdateMenuItemsFail[] = "Could not update menu Items";
const char kOnCompositionBoundsChangedEventName[] =
    "inputMethodPrivate.onCompositionBoundsChanged";

void SetMenuItemToMenu(const input_ime::MenuItem& input,
                       InputMethodEngineInterface::MenuItem* out) {
  out->modified = 0;
  out->id = input.id;
  if (input.label) {
    out->modified |= InputMethodEngineInterface::MENU_ITEM_MODIFIED_LABEL;
    out->label = *input.label;
  }

  if (input.style != input_ime::MENU_ITEM_STYLE_NONE) {
    out->modified |= InputMethodEngineInterface::MENU_ITEM_MODIFIED_STYLE;
    out->style = static_cast<InputMethodEngineInterface::MenuItemStyle>(
        input.style);
  }

  if (input.visible)
    out->modified |= InputMethodEngineInterface::MENU_ITEM_MODIFIED_VISIBLE;
  out->visible = input.visible ? *input.visible : true;

  if (input.checked)
    out->modified |= InputMethodEngineInterface::MENU_ITEM_MODIFIED_CHECKED;
  out->checked = input.checked ? *input.checked : false;

  if (input.enabled)
    out->modified |= InputMethodEngineInterface::MENU_ITEM_MODIFIED_ENABLED;
  out->enabled = input.enabled ? *input.enabled : true;
}

void CallbackKeyEventHandle(chromeos::input_method::KeyEventHandle* key_data,
                            bool handled) {
  base::Callback<void(bool consumed)>* callback =
      reinterpret_cast<base::Callback<void(bool consumed)>*>(key_data);
  callback->Run(handled);
  delete callback;
}

extensions::InputImeEventRouter* GetInputImeEventRouter(Profile* profile) {
  if (profile->HasOffTheRecordProfile())
    profile = profile->GetOffTheRecordProfile();
  return extensions::InputImeEventRouterFactory::GetInstance()->GetRouter(
      profile);
}

}  // namespace

namespace chromeos {
class ImeObserver : public InputMethodEngineInterface::Observer {
 public:
  explicit ImeObserver(const std::string& extension_id, Profile* profile)
      : extension_id_(extension_id), profile_(profile) {}

  ~ImeObserver() override {}

  void OnActivate(const std::string& component_id) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnActivate::kEventName))
      return;

    scoped_ptr<base::ListValue> args(input_ime::OnActivate::Create(
        component_id,
        input_ime::ParseScreenType(GetCurrentScreenType())));

    DispatchEventToExtension(input_ime::OnActivate::kEventName, args.Pass());
  }

  void OnDeactivated(const std::string& component_id) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnDeactivated::kEventName))
      return;

    scoped_ptr<base::ListValue> args(
        input_ime::OnDeactivated::Create(component_id));

    DispatchEventToExtension(input_ime::OnDeactivated::kEventName, args.Pass());
  }

  void OnFocus(
      const InputMethodEngineInterface::InputContext& context) override {
    if (extension_id_.empty() || !HasListener(input_ime::OnFocus::kEventName))
      return;

    input_ime::InputContext context_value;
    context_value.context_id = context.id;
    context_value.type = input_ime::ParseInputContextType(context.type);
    context_value.auto_correct = context.auto_correct;
    context_value.auto_complete = context.auto_complete;
    context_value.spell_check = context.spell_check;

    scoped_ptr<base::ListValue> args(input_ime::OnFocus::Create(context_value));

    DispatchEventToExtension(input_ime::OnFocus::kEventName, args.Pass());
  }

  void OnBlur(int context_id) override {
    if (extension_id_.empty() || !HasListener(input_ime::OnBlur::kEventName))
      return;

    scoped_ptr<base::ListValue> args(input_ime::OnBlur::Create(context_id));

    DispatchEventToExtension(input_ime::OnBlur::kEventName, args.Pass());
  }

  void OnInputContextUpdate(
      const InputMethodEngineInterface::InputContext& context) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnInputContextUpdate::kEventName))
      return;

    input_ime::InputContext context_value;
    context_value.context_id = context.id;
    context_value.type = input_ime::ParseInputContextType(context.type);

    scoped_ptr<base::ListValue> args(
        input_ime::OnInputContextUpdate::Create(context_value));

    DispatchEventToExtension(input_ime::OnInputContextUpdate::kEventName,
                             args.Pass());
  }

  bool IsInterestedInKeyEvent() const override {
    return ShouldForwardKeyEvent();
  }

  void OnKeyEvent(const std::string& component_id,
                  const InputMethodEngineInterface::KeyboardEvent& event,
                  chromeos::input_method::KeyEventHandle* key_data) override {
    if (extension_id_.empty())
      return;

    // If there is no listener for the event, no need to dispatch the event to
    // extension. Instead, releases the key event for default system behavior.
    if (!ShouldForwardKeyEvent()) {
      // Continue processing the key event so that the physical keyboard can
      // still work.
      CallbackKeyEventHandle(key_data, false);
      return;
    }

    const std::string request_id =
        GetInputImeEventRouter(profile_)->AddRequest(component_id, key_data);

    input_ime::KeyboardEvent key_data_value;
    key_data_value.type = input_ime::ParseKeyboardEventType(event.type);
    key_data_value.request_id = request_id;
    if (!event.extension_id.empty())
      key_data_value.extension_id.reset(new std::string(event.extension_id));
    key_data_value.key = event.key;
    key_data_value.code = event.code;
    key_data_value.alt_key.reset(new bool(event.alt_key));
    key_data_value.ctrl_key.reset(new bool(event.ctrl_key));
    key_data_value.shift_key.reset(new bool(event.shift_key));
    key_data_value.caps_lock.reset(new bool(event.caps_lock));

    scoped_ptr<base::ListValue> args(
        input_ime::OnKeyEvent::Create(component_id, key_data_value));

    DispatchEventToExtension(input_ime::OnKeyEvent::kEventName, args.Pass());
  }

  void OnCandidateClicked(
      const std::string& component_id,
      int candidate_id,
      InputMethodEngineInterface::MouseButtonEvent button) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnCandidateClicked::kEventName))
      return;

    input_ime::MouseButton button_enum = input_ime::MOUSE_BUTTON_NONE;
    switch (button) {
      case InputMethodEngineInterface::MOUSE_BUTTON_MIDDLE:
        button_enum = input_ime::MOUSE_BUTTON_MIDDLE;
        break;

      case InputMethodEngineInterface::MOUSE_BUTTON_RIGHT:
        button_enum = input_ime::MOUSE_BUTTON_RIGHT;
        break;

      case InputMethodEngineInterface::MOUSE_BUTTON_LEFT:
      // Default to left.
      default:
        button_enum = input_ime::MOUSE_BUTTON_LEFT;
        break;
    }

    scoped_ptr<base::ListValue> args(input_ime::OnCandidateClicked::Create(
        component_id, candidate_id, button_enum));

    DispatchEventToExtension(input_ime::OnCandidateClicked::kEventName,
                             args.Pass());
  }

  void OnMenuItemActivated(const std::string& component_id,
                           const std::string& menu_id) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnMenuItemActivated::kEventName))
      return;

    scoped_ptr<base::ListValue> args(
        input_ime::OnMenuItemActivated::Create(component_id, menu_id));

    DispatchEventToExtension(input_ime::OnMenuItemActivated::kEventName,
                             args.Pass());
  }

  void OnSurroundingTextChanged(const std::string& component_id,
                                const std::string& text,
                                int cursor_pos,
                                int anchor_pos) override {
    if (extension_id_.empty() ||
        !HasListener(input_ime::OnSurroundingTextChanged::kEventName))
      return;

    input_ime::OnSurroundingTextChanged::SurroundingInfo info;
    info.text = text;
    info.focus = cursor_pos;
    info.anchor = anchor_pos;
    scoped_ptr<base::ListValue> args(
        input_ime::OnSurroundingTextChanged::Create(component_id, info));

    DispatchEventToExtension(input_ime::OnSurroundingTextChanged::kEventName,
                             args.Pass());
  }

  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {
    if (extension_id_.empty() ||
        !HasListener(kOnCompositionBoundsChangedEventName))
      return;

    // Note: this is a private API event.
    base::ListValue* bounds_list = new base::ListValue();
    for (size_t i = 0; i < bounds.size(); ++i) {
      base::DictionaryValue* bounds_value = new base::DictionaryValue();
      bounds_value->SetInteger("x", bounds[i].x());
      bounds_value->SetInteger("y", bounds[i].y());
      bounds_value->SetInteger("w", bounds[i].width());
      bounds_value->SetInteger("h", bounds[i].height());
      bounds_list->Append(bounds_value);
    }

    if (bounds_list->GetSize() <= 0)
      return;
    scoped_ptr<base::ListValue> args(new base::ListValue());

    // The old extension code uses the first parameter to get the bounds of the
    // first composition character, so for backward compatibility, add it here.
    base::Value* first_value = NULL;
    if (bounds_list->Get(0, &first_value))
      args->Append(first_value->DeepCopy());
    args->Append(bounds_list);

    DispatchEventToExtension(kOnCompositionBoundsChangedEventName, args.Pass());
  }

  void OnReset(const std::string& component_id) override {
    if (extension_id_.empty() || !HasListener(input_ime::OnReset::kEventName))
      return;

    scoped_ptr<base::ListValue> args(input_ime::OnReset::Create(component_id));

    DispatchEventToExtension(input_ime::OnReset::kEventName, args.Pass());
  }

 private:
  void DispatchEventToExtension(const std::string& event_name,
                                scoped_ptr<base::ListValue> args) {
    if (event_name != input_ime::OnActivate::kEventName) {
      // For suspended IME extension (e.g. XKB extension), don't awake it by IME
      // events except onActivate. The IME extension should be awake by other
      // events (e.g. runtime.onMessage) from its other pages.
      // This is to save memory for steady state Chrome OS on which the users
      // don't want any IME features.
      extensions::ExtensionSystem* extension_system =
          extensions::ExtensionSystem::Get(profile_);
      if (extension_system) {
        const extensions::Extension* extension =
            extension_system->extension_service()->GetExtensionById(
                extension_id_, false /* include_disabled */);
        if (!extension)
          return;
        extensions::ProcessManager* process_manager =
            extensions::ProcessManager::Get(profile_);
        if (extensions::BackgroundInfo::HasBackgroundPage(extension) &&
            !process_manager->GetBackgroundHostForExtension(extension_id_)) {
          return;
        }
      }
    }

    scoped_ptr<extensions::Event> event(
        new extensions::Event(event_name, args.Pass()));
    event->restrict_to_browser_context = profile_;
    extensions::EventRouter::Get(profile_)
        ->DispatchEventToExtension(extension_id_, event.Pass());
  }

  // Returns true if the extension is ready to accept key event, otherwise
  // returns false.
  bool ShouldForwardKeyEvent() const {
    // Only forward key events to extension if there are non-lazy listeners
    // for onKeyEvent. Because if something wrong with the lazy background
    // page which doesn't register listener for onKeyEvent, it will not handle
    // the key events, and therefore, all key events will be eaten.
    // This is for error-tolerance, and it means that onKeyEvent will never wake
    // up lazy background page.
    const extensions::EventListenerMap::ListenerList& listener_list =
        extensions::EventRouter::Get(profile_)
            ->listeners()
            .GetEventListenersByName(input_ime::OnKeyEvent::kEventName);
    for (extensions::EventListenerMap::ListenerList::const_iterator it =
             listener_list.begin();
         it != listener_list.end(); ++it) {
      if ((*it)->extension_id() == extension_id_ && !(*it)->IsLazy())
        return true;
    }
    return false;
  }

  bool HasListener(const std::string& event_name) const {
    return extensions::EventRouter::Get(profile_)->HasEventListener(event_name);
  }

  // The component IME extensions need to know the current screen type (e.g.
  // lock screen, login screen, etc.) so that its on-screen keyboard page
  // won't open new windows/pages. See crbug.com/395621.
  std::string GetCurrentScreenType() {
    switch (chromeos::input_method::InputMethodManager::Get()
                ->GetUISessionState()) {
      case chromeos::input_method::InputMethodManager::STATE_LOGIN_SCREEN:
        return "login";
      case chromeos::input_method::InputMethodManager::STATE_LOCK_SCREEN:
        return "lock";
      case chromeos::input_method::InputMethodManager::STATE_BROWSER_SCREEN:
        return UserAddingScreen::Get()->IsRunning() ? "secondary-login"
                                                    : "normal";
      case chromeos::input_method::InputMethodManager::STATE_TERMINATING:
        return "normal";
    }
    NOTREACHED() << "New screen type is added. Please add new entry above.";
    return "normal";
  }

  std::string extension_id_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ImeObserver);
};

}  // namespace chromeos

namespace extensions {

InputImeEventRouterFactory* InputImeEventRouterFactory::GetInstance() {
  return Singleton<InputImeEventRouterFactory>::get();
}

InputImeEventRouterFactory::InputImeEventRouterFactory() {
}

InputImeEventRouterFactory::~InputImeEventRouterFactory() {
}

InputImeEventRouter* InputImeEventRouterFactory::GetRouter(Profile* profile) {
  InputImeEventRouter* router = router_map_[profile];
  if (!router) {
    router = new InputImeEventRouter(profile);
    router_map_[profile] = router;
  }
  return router;
}

InputImeEventRouter::InputImeEventRouter(Profile* profile)
    : next_request_id_(1), profile_(profile) {
}

InputImeEventRouter::~InputImeEventRouter() {
}

bool InputImeEventRouter::RegisterImeExtension(
    const std::string& extension_id,
    const std::vector<extensions::InputComponentInfo>& input_components) {
  VLOG(1) << "RegisterImeExtension: " << extension_id;

  if (engine_map_[extension_id])
    return false;

  chromeos::input_method::InputMethodManager* manager =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::ComponentExtensionIMEManager* comp_ext_ime_manager =
      manager->GetComponentExtensionIMEManager();

  chromeos::input_method::InputMethodDescriptors descriptors;
  // Only creates descriptors for 3rd party IME extension, because the
  // descriptors for component IME extensions are managed by InputMethodUtil.
  if (!comp_ext_ime_manager->IsWhitelistedExtension(extension_id)) {
    for (std::vector<extensions::InputComponentInfo>::const_iterator it =
             input_components.begin();
         it != input_components.end();
         ++it) {
      const extensions::InputComponentInfo& component = *it;
      DCHECK(component.type == extensions::INPUT_COMPONENT_TYPE_IME);

      std::vector<std::string> layouts;
      layouts.assign(component.layouts.begin(), component.layouts.end());
      std::vector<std::string> languages;
      languages.assign(component.languages.begin(), component.languages.end());

      const std::string& input_method_id =
          chromeos::extension_ime_util::GetInputMethodID(extension_id,
                                                         component.id);
      descriptors.push_back(chromeos::input_method::InputMethodDescriptor(
          input_method_id,
          component.name,
          std::string(),  // TODO(uekawa): Set short name.
          layouts,
          languages,
          false,  // 3rd party IMEs are always not for login.
          component.options_page_url,
          component.input_view_url));
    }
  }

  scoped_ptr<chromeos::InputMethodEngineInterface::Observer> observer(
      new chromeos::ImeObserver(extension_id, profile_));
  chromeos::InputMethodEngine* engine = new chromeos::InputMethodEngine();
  engine->Initialize(observer.Pass(), extension_id.c_str(), profile_);
  engine_map_[extension_id] = engine;
  chromeos::UserSessionManager::GetInstance()
      ->GetDefaultIMEState(profile_)
      ->AddInputMethodExtension(extension_id, descriptors, engine);

  return true;
}

void InputImeEventRouter::UnregisterAllImes(const std::string& extension_id) {
  std::map<std::string, InputMethodEngineInterface*>::iterator it =
      engine_map_.find(extension_id);
  if (it != engine_map_.end()) {
    chromeos::input_method::InputMethodManager::Get()
        ->GetActiveIMEState()
        ->RemoveInputMethodExtension(extension_id);
    delete it->second;
    engine_map_.erase(it);
  }
}

InputMethodEngineInterface* InputImeEventRouter::GetEngine(
    const std::string& extension_id,
    const std::string& component_id) {
  std::map<std::string, InputMethodEngineInterface*>::iterator it =
      engine_map_.find(extension_id);
  if (it != engine_map_.end())
    return it->second;
  return NULL;
}

InputMethodEngineInterface* InputImeEventRouter::GetActiveEngine(
    const std::string& extension_id) {
  std::map<std::string, InputMethodEngineInterface*>::iterator it =
      engine_map_.find(extension_id);
  if (it != engine_map_.end() && it->second->IsActive())
    return it->second;
  return NULL;
}

void InputImeEventRouter::OnKeyEventHandled(
    const std::string& extension_id,
    const std::string& request_id,
    bool handled) {
  RequestMap::iterator request = request_map_.find(request_id);
  if (request == request_map_.end()) {
    LOG(ERROR) << "Request ID not found: " << request_id;
    return;
  }

  std::string component_id = request->second.first;
  chromeos::input_method::KeyEventHandle* key_data = request->second.second;
  request_map_.erase(request);

  CallbackKeyEventHandle(key_data, handled);
}

std::string InputImeEventRouter::AddRequest(
    const std::string& component_id,
    chromeos::input_method::KeyEventHandle* key_data) {
  std::string request_id = base::IntToString(next_request_id_);
  ++next_request_id_;

  request_map_[request_id] = std::make_pair(component_id, key_data);

  return request_id;
}

bool InputImeSetCompositionFunction::RunSync() {
  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetActiveEngine(extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<SetComposition::Params> parent_params(
      SetComposition::Params::Create(*args_));
  const SetComposition::Params::Parameters& params = parent_params->parameters;
  std::vector<InputMethodEngineInterface::SegmentInfo> segments;
  if (params.segments) {
    const std::vector<linked_ptr<
        SetComposition::Params::Parameters::SegmentsType> >&
            segments_args = *params.segments;
    for (size_t i = 0; i < segments_args.size(); ++i) {
      EXTENSION_FUNCTION_VALIDATE(
          segments_args[i]->style !=
          input_ime::UNDERLINE_STYLE_NONE);
      segments.push_back(InputMethodEngineInterface::SegmentInfo());
      segments.back().start = segments_args[i]->start;
      segments.back().end = segments_args[i]->end;
      if (segments_args[i]->style ==
          input_ime::UNDERLINE_STYLE_UNDERLINE) {
        segments.back().style =
            InputMethodEngineInterface::SEGMENT_STYLE_UNDERLINE;
      } else if (segments_args[i]->style ==
                 input_ime::UNDERLINE_STYLE_DOUBLEUNDERLINE) {
        segments.back().style =
            InputMethodEngineInterface::SEGMENT_STYLE_DOUBLE_UNDERLINE;
      } else {
        segments.back().style =
            InputMethodEngineInterface::SEGMENT_STYLE_NO_UNDERLINE;
      }
    }
  }

  int selection_start =
      params.selection_start ? *params.selection_start : params.cursor;
  int selection_end =
      params.selection_end ? *params.selection_end : params.cursor;

  SetResult(new base::FundamentalValue(
      engine->SetComposition(params.context_id, params.text.c_str(),
                             selection_start, selection_end, params.cursor,
                             segments, &error_)));
  return true;
}

bool InputImeClearCompositionFunction::RunSync() {
  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetActiveEngine(extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<ClearComposition::Params> parent_params(
      ClearComposition::Params::Create(*args_));
  const ClearComposition::Params::Parameters& params =
      parent_params->parameters;

  SetResult(new base::FundamentalValue(
      engine->ClearComposition(params.context_id, &error_)));
  return true;
}

bool InputImeCommitTextFunction::RunSync() {
  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetActiveEngine(extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<CommitText::Params> parent_params(
      CommitText::Params::Create(*args_));
  const CommitText::Params::Parameters& params =
      parent_params->parameters;

  SetResult(new base::FundamentalValue(
      engine->CommitText(params.context_id, params.text.c_str(), &error_)));
  return true;
}

bool InputImeHideInputViewFunction::RunAsync() {
  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetActiveEngine(extension_id());
  if (!engine) {
    return true;
  }
  engine->HideInputView();
  return true;
}

bool InputImeSendKeyEventsFunction::RunAsync() {
  scoped_ptr<SendKeyEvents::Params> parent_params(
      SendKeyEvents::Params::Create(*args_));
  const SendKeyEvents::Params::Parameters& params =
      parent_params->parameters;
  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetActiveEngine(extension_id());
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  const std::vector<linked_ptr<input_ime::KeyboardEvent> >& key_data =
      params.key_data;
  std::vector<chromeos::InputMethodEngineInterface::KeyboardEvent> key_data_out;

  for (size_t i = 0; i < key_data.size(); ++i) {
    chromeos::InputMethodEngineInterface::KeyboardEvent event;
    event.type = input_ime::ToString(key_data[i]->type);
    event.key = key_data[i]->key;
    event.code = key_data[i]->code;
    event.key_code = key_data[i]->key_code.get() ? *(key_data[i]->key_code) : 0;
    if (key_data[i]->alt_key)
      event.alt_key = *(key_data[i]->alt_key);
    if (key_data[i]->ctrl_key)
      event.ctrl_key = *(key_data[i]->ctrl_key);
    if (key_data[i]->shift_key)
      event.shift_key = *(key_data[i]->shift_key);
    if (key_data[i]->caps_lock)
      event.caps_lock = *(key_data[i]->caps_lock);
    key_data_out.push_back(event);
  }

  engine->SendKeyEvents(params.context_id, key_data_out);
  return true;
}

bool InputImeSetCandidateWindowPropertiesFunction::RunSync() {
  scoped_ptr<SetCandidateWindowProperties::Params> parent_params(
      SetCandidateWindowProperties::Params::Create(*args_));
  const SetCandidateWindowProperties::Params::Parameters&
      params = parent_params->parameters;

  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetEngine(extension_id(), params.engine_id);
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  const SetCandidateWindowProperties::Params::Parameters::Properties&
      properties = params.properties;

  if (properties.visible &&
      !engine->SetCandidateWindowVisible(*properties.visible, &error_)) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  InputMethodEngineInterface::CandidateWindowProperty properties_out =
    engine->GetCandidateWindowProperty();
  bool modified = false;

  if (properties.cursor_visible) {
    properties_out.is_cursor_visible = *properties.cursor_visible;
    modified = true;
  }

  if (properties.vertical) {
    properties_out.is_vertical = *properties.vertical;
    modified = true;
  }

  if (properties.page_size) {
    properties_out.page_size = *properties.page_size;
    modified = true;
  }

  if (properties.window_position == input_ime::WINDOW_POSITION_COMPOSITION) {
    properties_out.show_window_at_composition = true;
    modified = true;
  } else if (properties.window_position == input_ime::WINDOW_POSITION_CURSOR) {
    properties_out.show_window_at_composition = false;
    modified = true;
  }

  if (properties.auxiliary_text) {
    properties_out.auxiliary_text = *properties.auxiliary_text;
    modified = true;
  }

  if (properties.auxiliary_text_visible) {
    properties_out.is_auxiliary_text_visible =
        *properties.auxiliary_text_visible;
    modified = true;
  }

  if (modified) {
    engine->SetCandidateWindowProperty(properties_out);
  }

  SetResult(new base::FundamentalValue(true));

  return true;
}

bool InputImeSetCandidatesFunction::RunSync() {
  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetActiveEngine(extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<SetCandidates::Params> parent_params(
      SetCandidates::Params::Create(*args_));
  const SetCandidates::Params::Parameters& params =
      parent_params->parameters;

  std::vector<InputMethodEngineInterface::Candidate> candidates_out;
  const std::vector<linked_ptr<
      SetCandidates::Params::Parameters::CandidatesType> >& candidates_in =
          params.candidates;
  for (size_t i = 0; i < candidates_in.size(); ++i) {
    candidates_out.push_back(InputMethodEngineInterface::Candidate());
    candidates_out.back().value = candidates_in[i]->candidate;
    candidates_out.back().id = candidates_in[i]->id;
    if (candidates_in[i]->label)
      candidates_out.back().label = *candidates_in[i]->label;
    if (candidates_in[i]->annotation)
      candidates_out.back().annotation = *candidates_in[i]->annotation;
    if (candidates_in[i]->usage) {
      candidates_out.back().usage.title = candidates_in[i]->usage->title;
      candidates_out.back().usage.body = candidates_in[i]->usage->body;
    }
  }

  SetResult(new base::FundamentalValue(
      engine->SetCandidates(params.context_id, candidates_out, &error_)));
  return true;
}

bool InputImeSetCursorPositionFunction::RunSync() {
  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetActiveEngine(extension_id());
  if (!engine) {
    SetResult(new base::FundamentalValue(false));
    return true;
  }

  scoped_ptr<SetCursorPosition::Params> parent_params(
      SetCursorPosition::Params::Create(*args_));
  const SetCursorPosition::Params::Parameters& params =
      parent_params->parameters;

  SetResult(new base::FundamentalValue(
      engine->SetCursorPosition(params.context_id, params.candidate_id,
                                &error_)));
  return true;
}

bool InputImeSetMenuItemsFunction::RunSync() {
  scoped_ptr<SetMenuItems::Params> parent_params(
      SetMenuItems::Params::Create(*args_));
  const SetMenuItems::Params::Parameters& params =
      parent_params->parameters;

  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetEngine(extension_id(), params.engine_id);
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  const std::vector<linked_ptr<input_ime::MenuItem> >& items = params.items;
  std::vector<InputMethodEngineInterface::MenuItem> items_out;

  for (size_t i = 0; i < items.size(); ++i) {
    items_out.push_back(InputMethodEngineInterface::MenuItem());
    SetMenuItemToMenu(*items[i], &items_out.back());
  }

  if (!engine->SetMenuItems(items_out))
    error_ = kErrorSetMenuItemsFail;
  return true;
}

bool InputImeUpdateMenuItemsFunction::RunSync() {
  scoped_ptr<UpdateMenuItems::Params> parent_params(
      UpdateMenuItems::Params::Create(*args_));
  const UpdateMenuItems::Params::Parameters& params =
      parent_params->parameters;

  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetEngine(extension_id(), params.engine_id);
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  const std::vector<linked_ptr<input_ime::MenuItem> >& items = params.items;
  std::vector<InputMethodEngineInterface::MenuItem> items_out;

  for (size_t i = 0; i < items.size(); ++i) {
    items_out.push_back(InputMethodEngineInterface::MenuItem());
    SetMenuItemToMenu(*items[i], &items_out.back());
  }

  if (!engine->UpdateMenuItems(items_out))
    error_ = kErrorUpdateMenuItemsFail;
  return true;
}

bool InputImeDeleteSurroundingTextFunction::RunSync() {
  scoped_ptr<DeleteSurroundingText::Params> parent_params(
      DeleteSurroundingText::Params::Create(*args_));
  const DeleteSurroundingText::Params::Parameters& params =
      parent_params->parameters;

  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
          ->GetEngine(extension_id(), params.engine_id);
  if (!engine) {
    error_ = kErrorEngineNotAvailable;
    return false;
  }

  engine->DeleteSurroundingText(params.context_id, params.offset, params.length,
                                &error_);
  return true;
}

bool InputImeKeyEventHandledFunction::RunAsync() {
  scoped_ptr<KeyEventHandled::Params> params(
      KeyEventHandled::Params::Create(*args_));
  GetInputImeEventRouter(Profile::FromBrowserContext(browser_context()))
      ->OnKeyEventHandled(extension_id(), params->request_id, params->response);
  return true;
}

InputImeAPI::InputImeAPI(content::BrowserContext* context)
    : browser_context_(context), extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));

  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, input_ime::OnFocus::kEventName);
}

InputImeAPI::~InputImeAPI() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<InputImeAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<InputImeAPI>* InputImeAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void InputImeAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  const std::vector<InputComponentInfo>* input_components =
      extensions::InputComponents::GetInputComponents(extension);
  if (input_components)
    GetInputImeEventRouter(Profile::FromBrowserContext(browser_context))
        ->RegisterImeExtension(extension->id(), *input_components);
}

void InputImeAPI::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionInfo::Reason reason) {
  const std::vector<InputComponentInfo>* input_components =
      extensions::InputComponents::GetInputComponents(extension);
  if (!input_components)
    return;
  if (input_components->size() > 0) {
    GetInputImeEventRouter(Profile::FromBrowserContext(browser_context))
        ->UnregisterAllImes(extension->id());
  }
}

void InputImeAPI::OnListenerAdded(const EventListenerInfo& details) {
  if (!details.browser_context)
    return;
  InputMethodEngineInterface* engine =
      GetInputImeEventRouter(
          Profile::FromBrowserContext(details.browser_context))
          ->GetActiveEngine(details.extension_id);
  // Notifies the IME extension for IME ready with onActivate/onFocus events.
  if (engine)
    engine->Enable(engine->GetActiveComponentId());
}

}  // namespace extensions

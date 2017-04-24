/*
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/events/UIEventWithKeyState.h"

namespace blink {

UIEventWithKeyState::UIEventWithKeyState(
    const AtomicString& type,
    bool canBubble,
    bool cancelable,
    AbstractView* view,
    int detail,
    WebInputEvent::Modifiers modifiers,
    TimeTicks platformTimeStamp,
    InputDeviceCapabilities* sourceCapabilities)
    : UIEvent(type,
              canBubble,
              cancelable,
              ComposedMode::Composed,
              platformTimeStamp,
              view,
              detail,
              sourceCapabilities),
      m_modifiers(modifiers) {}

UIEventWithKeyState::UIEventWithKeyState(const AtomicString& type,
                                         const EventModifierInit& initializer)
    : UIEvent(type, initializer), m_modifiers(0) {
  if (initializer.ctrlKey())
    m_modifiers |= WebInputEvent::ControlKey;
  if (initializer.shiftKey())
    m_modifiers |= WebInputEvent::ShiftKey;
  if (initializer.altKey())
    m_modifiers |= WebInputEvent::AltKey;
  if (initializer.metaKey())
    m_modifiers |= WebInputEvent::MetaKey;
  if (initializer.modifierAltGraph())
    m_modifiers |= WebInputEvent::AltGrKey;
  if (initializer.modifierFn())
    m_modifiers |= WebInputEvent::FnKey;
  if (initializer.modifierCapsLock())
    m_modifiers |= WebInputEvent::CapsLockOn;
  if (initializer.modifierScrollLock())
    m_modifiers |= WebInputEvent::ScrollLockOn;
  if (initializer.modifierNumLock())
    m_modifiers |= WebInputEvent::NumLockOn;
  if (initializer.modifierSymbol())
    m_modifiers |= WebInputEvent::SymbolKey;
}

bool UIEventWithKeyState::s_newTabModifierSetFromIsolatedWorld = false;

void UIEventWithKeyState::didCreateEventInIsolatedWorld(bool ctrlKey,
                                                        bool shiftKey,
                                                        bool altKey,
                                                        bool metaKey) {
#if OS(MACOSX)
  const bool newTabModifierSet = metaKey;
#else
  const bool newTabModifierSet = ctrlKey;
#endif
  s_newTabModifierSetFromIsolatedWorld |= newTabModifierSet;
}

void UIEventWithKeyState::setFromWebInputEventModifiers(
    EventModifierInit& initializer,
    WebInputEvent::Modifiers modifiers) {
  if (modifiers & WebInputEvent::ControlKey)
    initializer.setCtrlKey(true);
  if (modifiers & WebInputEvent::ShiftKey)
    initializer.setShiftKey(true);
  if (modifiers & WebInputEvent::AltKey)
    initializer.setAltKey(true);
  if (modifiers & WebInputEvent::MetaKey)
    initializer.setMetaKey(true);
  if (modifiers & WebInputEvent::AltGrKey)
    initializer.setModifierAltGraph(true);
  if (modifiers & WebInputEvent::FnKey)
    initializer.setModifierFn(true);
  if (modifiers & WebInputEvent::CapsLockOn)
    initializer.setModifierCapsLock(true);
  if (modifiers & WebInputEvent::ScrollLockOn)
    initializer.setModifierScrollLock(true);
  if (modifiers & WebInputEvent::NumLockOn)
    initializer.setModifierNumLock(true);
  if (modifiers & WebInputEvent::SymbolKey)
    initializer.setModifierSymbol(true);
}

bool UIEventWithKeyState::getModifierState(const String& keyIdentifier) const {
  struct Identifier {
    const char* identifier;
    WebInputEvent::Modifiers mask;
  };
  static const Identifier kIdentifiers[] = {
      {"Shift", WebInputEvent::ShiftKey},
      {"Control", WebInputEvent::ControlKey},
      {"Alt", WebInputEvent::AltKey},
      {"Meta", WebInputEvent::MetaKey},
      {"AltGraph", WebInputEvent::AltGrKey},
      {"Accel",
#if OS(MACOSX)
       WebInputEvent::MetaKey
#else
       WebInputEvent::ControlKey
#endif
      },
      {"Fn", WebInputEvent::FnKey},
      {"CapsLock", WebInputEvent::CapsLockOn},
      {"ScrollLock", WebInputEvent::ScrollLockOn},
      {"NumLock", WebInputEvent::NumLockOn},
      {"Symbol", WebInputEvent::SymbolKey},
  };
  for (const auto& identifier : kIdentifiers) {
    if (keyIdentifier == identifier.identifier)
      return m_modifiers & identifier.mask;
  }
  return false;
}

void UIEventWithKeyState::initModifiers(bool ctrlKey,
                                        bool altKey,
                                        bool shiftKey,
                                        bool metaKey) {
  m_modifiers = 0;
  if (ctrlKey)
    m_modifiers |= WebInputEvent::ControlKey;
  if (altKey)
    m_modifiers |= WebInputEvent::AltKey;
  if (shiftKey)
    m_modifiers |= WebInputEvent::ShiftKey;
  if (metaKey)
    m_modifiers |= WebInputEvent::MetaKey;
}

UIEventWithKeyState* findEventWithKeyState(Event* event) {
  for (Event* e = event; e; e = e->underlyingEvent())
    if (e->isKeyboardEvent() || e->isMouseEvent())
      return static_cast<UIEventWithKeyState*>(e);
  return nullptr;
}

}  // namespace blink

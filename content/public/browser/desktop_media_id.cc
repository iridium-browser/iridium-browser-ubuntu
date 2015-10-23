// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/desktop_media_id.h"

#include <vector>

#include "base/id_map.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#endif  // defined(USE_AURA)

namespace  {

#if defined(USE_AURA)

class AuraWindowRegistry : public aura::WindowObserver {
 public:
  static AuraWindowRegistry* GetInstance() {
    return Singleton<AuraWindowRegistry>::get();
  }

  int RegisterWindow(aura::Window* window) {
    IDMap<aura::Window>::const_iterator it(&registered_windows_);
    for (; !it.IsAtEnd(); it.Advance()) {
      if (it.GetCurrentValue() == window)
        return it.GetCurrentKey();
    }

    window->AddObserver(this);
    return registered_windows_.Add(window);
  }

  aura::Window* GetWindowById(int id) {
    return registered_windows_.Lookup(id);
  }

 private:
  friend struct DefaultSingletonTraits<AuraWindowRegistry>;

  AuraWindowRegistry() {}
  ~AuraWindowRegistry() override {}

  // WindowObserver overrides.
  void OnWindowDestroying(aura::Window* window) override {
    IDMap<aura::Window>::iterator it(&registered_windows_);
    for (; !it.IsAtEnd(); it.Advance()) {
      if (it.GetCurrentValue() == window) {
        registered_windows_.Remove(it.GetCurrentKey());
        return;
      }
    }
    NOTREACHED();
  }

  IDMap<aura::Window> registered_windows_;

  DISALLOW_COPY_AND_ASSIGN(AuraWindowRegistry);
};

#endif  // defined(USE_AURA)

}  // namespace

namespace content {

const char kScreenPrefix[] = "screen";
const char kWindowPrefix[] = "window";

#if defined(USE_AURA)

// static
DesktopMediaID DesktopMediaID::RegisterAuraWindow(DesktopMediaID::Type type,
                                                  aura::Window* window) {
  DCHECK(type == TYPE_SCREEN || type == TYPE_WINDOW);
  DCHECK(window);
  DesktopMediaID media_id(type, kNullId);
  media_id.aura_id = AuraWindowRegistry::GetInstance()->RegisterWindow(window);
  return media_id;
}

// static
aura::Window* DesktopMediaID::GetAuraWindowById(const DesktopMediaID& id) {
  return AuraWindowRegistry::GetInstance()->GetWindowById(id.aura_id);
}

#endif  // defined(USE_AURA)

DesktopMediaID::DesktopMediaID() = default;

// static
DesktopMediaID DesktopMediaID::Parse(const std::string& str) {
  std::vector<std::string> parts = base::SplitString(
      str, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

#if defined(USE_AURA)
  if (parts.size() != 3)
    return DesktopMediaID();
#else
  if (parts.size() != 2)
    return DesktopMediaID();
#endif

  Type type = TYPE_NONE;
  if (parts[0] == kScreenPrefix) {
    type = TYPE_SCREEN;
  } else if (parts[0] == kWindowPrefix) {
    type = TYPE_WINDOW;
  } else {
    return DesktopMediaID();
  }

  int64 id;
  if (!base::StringToInt64(parts[1], &id))
    return DesktopMediaID();

  DesktopMediaID media_id(type, id);

#if defined(USE_AURA)
  int64 aura_id;
  if (!base::StringToInt64(parts[2], &aura_id))
    return DesktopMediaID();
  media_id.aura_id = aura_id;
#endif  // defined(USE_AURA)

  return media_id;
}

std::string DesktopMediaID::ToString() {
  std::string prefix;
  switch (type) {
    case TYPE_NONE:
      NOTREACHED();
      return std::string();
    case TYPE_SCREEN:
      prefix = kScreenPrefix;
      break;
    case TYPE_WINDOW:
      prefix = kWindowPrefix;
      break;
  }
  DCHECK(!prefix.empty());

  prefix.append(":");
  prefix.append(base::Int64ToString(id));

#if defined(USE_AURA)
  prefix.append(":");
  prefix.append(base::Int64ToString(aura_id));
#endif  // defined(USE_AURA)

  return prefix;
}

}  // namespace content

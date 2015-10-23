// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_page_prefs.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/prefs/pref_service.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace {

// Path to the integer corresponding to user's preference theme.
const char kFontPref[] = "dom_distiller.font_family";
// Path to the integer corresponding to user's preference font family.
const char kThemePref[] = "dom_distiller.theme";
}

namespace dom_distiller {

DistilledPagePrefs::DistilledPagePrefs(PrefService* pref_service)
    : pref_service_(pref_service), weak_ptr_factory_(this) {
}

DistilledPagePrefs::~DistilledPagePrefs() {
}

// static
void DistilledPagePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      kThemePref,
      DistilledPagePrefs::LIGHT,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      kFontPref,
      DistilledPagePrefs::SANS_SERIF,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void DistilledPagePrefs::SetFontFamily(
    DistilledPagePrefs::FontFamily new_font_family) {
  pref_service_->SetInteger(kFontPref, new_font_family);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DistilledPagePrefs::NotifyOnChangeFontFamily,
                            weak_ptr_factory_.GetWeakPtr(), new_font_family));
}

DistilledPagePrefs::FontFamily DistilledPagePrefs::GetFontFamily() {
  int font_family = pref_service_->GetInteger(kFontPref);
  if (font_family < 0 || font_family >= DistilledPagePrefs::FONT_FAMILY_COUNT) {
    // Persisted data was incorrect, trying to clean it up by storing the
    // default.
    SetFontFamily(DistilledPagePrefs::SANS_SERIF);
    return DistilledPagePrefs::SANS_SERIF;
  }
  return static_cast<FontFamily>(font_family);
}

void DistilledPagePrefs::SetTheme(DistilledPagePrefs::Theme new_theme) {
  pref_service_->SetInteger(kThemePref, new_theme);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DistilledPagePrefs::NotifyOnChangeTheme,
                            weak_ptr_factory_.GetWeakPtr(), new_theme));
}

DistilledPagePrefs::Theme DistilledPagePrefs::GetTheme() {
  int theme = pref_service_->GetInteger(kThemePref);
  if (theme < 0 || theme >= DistilledPagePrefs::THEME_COUNT) {
    // Persisted data was incorrect, trying to clean it up by storing the
    // default.
    SetTheme(DistilledPagePrefs::LIGHT);
    return DistilledPagePrefs::LIGHT;
  }
  return static_cast<Theme>(theme);
}

void DistilledPagePrefs::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void DistilledPagePrefs::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void DistilledPagePrefs::NotifyOnChangeFontFamily(
    DistilledPagePrefs::FontFamily new_font_family) {
  FOR_EACH_OBSERVER(Observer, observers_, OnChangeFontFamily(new_font_family));
}

void DistilledPagePrefs::NotifyOnChangeTheme(
    DistilledPagePrefs::Theme new_theme) {
  FOR_EACH_OBSERVER(Observer, observers_, OnChangeTheme(new_theme));
}

}  // namespace dom_distiller

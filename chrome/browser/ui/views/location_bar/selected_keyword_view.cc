// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/location_bar/location_bar_util.h"
#include "chrome/browser/ui/views/location_bar/keyword_hint_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/template_url_service.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/painter.h"


namespace {
const int kBackgroundImages[] = IMAGE_GRID(IDR_OMNIBOX_SELECTED_KEYWORD_BUBBLE);
}


SelectedKeywordView::SelectedKeywordView(const gfx::FontList& font_list,
                                         SkColor text_color,
                                         SkColor parent_background_color,
                                         Profile* profile)
    : IconLabelBubbleView(kBackgroundImages, NULL, IDR_KEYWORD_SEARCH_MAGNIFIER,
                          font_list, text_color, parent_background_color,
                          false),
      profile_(profile) {
  full_label_.SetFontList(font_list);
  full_label_.SetVisible(false);
  partial_label_.SetFontList(font_list);
  partial_label_.SetVisible(false);
}

SelectedKeywordView::~SelectedKeywordView() {
}

gfx::Size SelectedKeywordView::GetPreferredSize() const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(full_label_.GetPreferredSize().width());
}

gfx::Size SelectedKeywordView::GetMinimumSize() const {
  // Height will be ignored by the LocationBarView.
  return GetSizeForLabelWidth(partial_label_.GetMinimumSize().width());
}

void SelectedKeywordView::Layout() {
  SetLabel(((width() == GetPreferredSize().width()) ?
      full_label_ : partial_label_).text());
  IconLabelBubbleView::Layout();
}

void SelectedKeywordView::SetKeyword(const base::string16& keyword) {
  keyword_ = keyword;
  if (keyword.empty())
    return;
  DCHECK(profile_);
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!model)
    return;

  bool is_extension_keyword;
  const base::string16 short_name =
      model->GetKeywordShortName(keyword, &is_extension_keyword);
  const base::string16 full_name = is_extension_keyword ?
      short_name :
      l10n_util::GetStringFUTF16(IDS_OMNIBOX_KEYWORD_TEXT, short_name);
  full_label_.SetText(full_name);

  const base::string16 min_string(
      location_bar_util::CalculateMinString(short_name));
  const base::string16 partial_name = is_extension_keyword ?
      min_string :
      l10n_util::GetStringFUTF16(IDS_OMNIBOX_KEYWORD_TEXT, min_string);
  partial_label_.SetText(min_string.empty() ?
      full_label_.text() : partial_name);
}

const char* SelectedKeywordView::GetClassName() const {
  return "SelectedKeywordView";
}

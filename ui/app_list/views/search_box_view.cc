// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/search_box_view.h"

#include <algorithm>

#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/app_list/views/app_list_menu_views.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/shadow_value.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/shadow_border.h"

namespace app_list {

namespace {

const int kPadding = 16;
const int kInnerPadding = 24;
const int kPreferredWidth = 360;
const int kPreferredHeight = 48;

const SkColor kHintTextColor = SkColorSetRGB(0xA0, 0xA0, 0xA0);

// Menu offset relative to the bottom-right corner of the menu button.
const int kMenuYOffsetFromButton = -4;
const int kMenuXOffsetFromButton = -7;

const int kBackgroundBorderCornerRadius = 2;

// A background that paints a solid white rounded rect with a thin grey border.
class ExperimentalSearchBoxBackground : public views::Background {
 public:
  ExperimentalSearchBoxBackground() {}
  ~ExperimentalSearchBoxBackground() override {}

 private:
  // views::Background overrides:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::Rect bounds = view->GetContentsBounds();

    SkPaint paint;
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    paint.setColor(kSearchBoxBackground);
    canvas->DrawRoundRect(bounds, kBackgroundBorderCornerRadius, paint);
  }

  DISALLOW_COPY_AND_ASSIGN(ExperimentalSearchBoxBackground);
};

}  // namespace

// To paint grey background on mic and back buttons
class SearchBoxImageButton : public views::ImageButton {
 public:
  explicit SearchBoxImageButton(views::ButtonListener* listener)
      : ImageButton(listener), selected_(false) {}
  ~SearchBoxImageButton() override {}

  bool selected() { return selected_; }
  void SetSelected(bool selected) {
    if (selected_ == selected)
      return;

    selected_ = selected;
    SchedulePaint();
    if (selected)
      NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, true);
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    // Disable space key to press the button. The keyboard events received
    // by this view are forwarded from a Textfield (SearchBoxView) and key
    // released events are not forwarded. This leaves the button in pressed
    // state.
    if (event.key_code() == ui::VKEY_SPACE)
      return false;

    return CustomButton::OnKeyPressed(event);
  }

 private:
  // views::View overrides:
  void OnPaintBackground(gfx::Canvas* canvas) override {
    if (state_ == STATE_HOVERED || state_ == STATE_PRESSED || selected_)
      canvas->FillRect(gfx::Rect(size()), kSelectedColor);
  }

  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(SearchBoxImageButton);
};

SearchBoxView::SearchBoxView(SearchBoxViewDelegate* delegate,
                             AppListViewDelegate* view_delegate)
    : delegate_(delegate),
      view_delegate_(view_delegate),
      model_(NULL),
      content_container_(new views::View),
      icon_view_(NULL),
      back_button_(NULL),
      speech_button_(NULL),
      menu_button_(NULL),
      search_box_(new views::Textfield),
      contents_view_(NULL),
      focused_view_(FOCUS_SEARCH_BOX) {
  SetLayoutManager(new views::FillLayout);
  AddChildView(content_container_);

  if (switches::IsExperimentalAppListEnabled()) {
    SetShadow(GetShadowForZHeight(2));
    back_button_ = new SearchBoxImageButton(this);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    back_button_->SetImage(
        views::ImageButton::STATE_NORMAL,
        rb.GetImageSkiaNamed(IDR_APP_LIST_FOLDER_BACK_NORMAL));
    back_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                    views::ImageButton::ALIGN_MIDDLE);
    base::string16 back_title(l10n_util::GetStringUTF16(IDS_APP_LIST_BACK));
    back_button_->SetAccessibleName(back_title);
    back_button_->SetTooltipText(back_title);
    content_container_->AddChildView(back_button_);

    content_container_->set_background(new ExperimentalSearchBoxBackground());
  } else {
    set_background(
        views::Background::CreateSolidBackground(kSearchBoxBackground));
    SetBorder(
        views::Border::CreateSolidSidedBorder(0, 0, 1, 0, kTopSeparatorColor));
    icon_view_ = new views::ImageView;
    content_container_->AddChildView(icon_view_);
  }

  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, kPadding, 0,
                           kInnerPadding - views::Textfield::kTextPadding);
  content_container_->SetLayoutManager(layout);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  layout->set_minimum_cross_axis_size(kPreferredHeight);

  search_box_->SetBorder(views::Border::NullBorder());
  search_box_->SetTextColor(kSearchTextColor);
  search_box_->SetBackgroundColor(kSearchBoxBackground);
  search_box_->set_placeholder_text_color(kHintTextColor);
  search_box_->set_controller(this);
  search_box_->SetTextInputType(ui::TEXT_INPUT_TYPE_SEARCH);
  search_box_->SetTextInputFlags(ui::TEXT_INPUT_FLAG_AUTOCORRECT_OFF);
  content_container_->AddChildView(search_box_);
  layout->SetFlexForView(search_box_, 1);

#if !defined(OS_CHROMEOS)
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  menu_button_ = new views::MenuButton(NULL, base::string16(), this, false);
  menu_button_->SetBorder(views::Border::NullBorder());
  menu_button_->SetImage(views::Button::STATE_NORMAL,
                         *rb.GetImageSkiaNamed(IDR_APP_LIST_TOOLS_NORMAL));
  menu_button_->SetImage(views::Button::STATE_HOVERED,
                         *rb.GetImageSkiaNamed(IDR_APP_LIST_TOOLS_HOVER));
  menu_button_->SetImage(views::Button::STATE_PRESSED,
                         *rb.GetImageSkiaNamed(IDR_APP_LIST_TOOLS_PRESSED));
  content_container_->AddChildView(menu_button_);
#endif

  view_delegate_->GetSpeechUI()->AddObserver(this);
  ModelChanged();
}

SearchBoxView::~SearchBoxView() {
  view_delegate_->GetSpeechUI()->RemoveObserver(this);
  model_->search_box()->RemoveObserver(this);
}

void SearchBoxView::ModelChanged() {
  if (model_)
    model_->search_box()->RemoveObserver(this);

  model_ = view_delegate_->GetModel();
  DCHECK(model_);
  model_->search_box()->AddObserver(this);
  IconChanged();
  SpeechRecognitionButtonPropChanged();
  HintTextChanged();
}

bool SearchBoxView::HasSearch() const {
  return !search_box_->text().empty();
}

void SearchBoxView::ClearSearch() {
  search_box_->SetText(base::string16());
  view_delegate_->AutoLaunchCanceled();
  // Updates model and fires query changed manually because SetText() above
  // does not generate ContentsChanged() notification.
  UpdateModel();
  NotifyQueryChanged();
}

void SearchBoxView::InvalidateMenu() {
  menu_.reset();
}

void SearchBoxView::SetShadow(const gfx::ShadowValue& shadow) {
  SetBorder(make_scoped_ptr(new views::ShadowBorder(shadow)));
  Layout();
}

gfx::Rect SearchBoxView::GetViewBoundsForSearchBoxContentsBounds(
    const gfx::Rect& rect) const {
  gfx::Rect view_bounds = rect;
  view_bounds.Inset(-GetInsets());
  return view_bounds;
}

views::ImageButton* SearchBoxView::back_button() {
  return static_cast<views::ImageButton*>(back_button_);
}

// Returns true if set internally, i.e. if focused_view_ != CONTENTS_VIEW.
// Note: because we always want to be able to type in the edit box, this is only
// a faux-focus so that buttons can respond to the ENTER key.
bool SearchBoxView::MoveTabFocus(bool move_backwards) {
  if (back_button_)
    back_button_->SetSelected(false);
  if (speech_button_)
    speech_button_->SetSelected(false);

  switch (focused_view_) {
    case FOCUS_BACK_BUTTON:
      focused_view_ = move_backwards ? FOCUS_BACK_BUTTON : FOCUS_SEARCH_BOX;
      break;
    case FOCUS_SEARCH_BOX:
      if (move_backwards) {
        focused_view_ = back_button_ && back_button_->visible()
            ? FOCUS_BACK_BUTTON : FOCUS_SEARCH_BOX;
      } else {
        focused_view_ = speech_button_ && speech_button_->visible()
            ? FOCUS_MIC_BUTTON : FOCUS_CONTENTS_VIEW;
      }
      break;
    case FOCUS_MIC_BUTTON:
      focused_view_ = move_backwards ? FOCUS_SEARCH_BOX : FOCUS_CONTENTS_VIEW;
      break;
    case FOCUS_CONTENTS_VIEW:
      focused_view_ = move_backwards
          ? (speech_button_ && speech_button_->visible() ?
              FOCUS_MIC_BUTTON : FOCUS_SEARCH_BOX)
          : FOCUS_CONTENTS_VIEW;
      break;
    default:
      DCHECK(false);
  }

  switch (focused_view_) {
    case FOCUS_BACK_BUTTON:
      if (back_button_)
        back_button_->SetSelected(true);
      break;
    case FOCUS_SEARCH_BOX:
      // Set the ChromeVox focus to the search box. However, DO NOT do this if
      // we are in the search results state (i.e., if the search box has text in
      // it), because the focus is about to be shifted to the first search
      // result and we do not want to read out the name of the search box as
      // well.
      if (search_box_->text().empty())
        search_box_->NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, true);
      break;
    case FOCUS_MIC_BUTTON:
      if (speech_button_)
        speech_button_->SetSelected(true);
      break;
    default:
      break;
  }

  if (focused_view_ < FOCUS_CONTENTS_VIEW)
    delegate_->SetSearchResultSelection(focused_view_ == FOCUS_SEARCH_BOX);

  return (focused_view_ < FOCUS_CONTENTS_VIEW);
}

void SearchBoxView::ResetTabFocus(bool on_contents) {
  if (back_button_)
    back_button_->SetSelected(false);
  if (speech_button_)
    speech_button_->SetSelected(false);
  focused_view_ = on_contents ? FOCUS_CONTENTS_VIEW : FOCUS_SEARCH_BOX;
}

gfx::Size SearchBoxView::GetPreferredSize() const {
  return gfx::Size(kPreferredWidth, kPreferredHeight);
}

bool SearchBoxView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (contents_view_)
    return contents_view_->OnMouseWheel(event);

  return false;
}

void SearchBoxView::OnEnabledChanged() {
  search_box_->SetEnabled(enabled());
  if (menu_button_)
    menu_button_->SetEnabled(enabled());
  if (speech_button_)
    speech_button_->SetEnabled(enabled());
}

void SearchBoxView::UpdateModel() {
  // Temporarily remove from observer to ignore notifications caused by us.
  model_->search_box()->RemoveObserver(this);
  model_->search_box()->SetText(search_box_->text());
  model_->search_box()->SetSelectionModel(search_box_->GetSelectionModel());
  model_->search_box()->AddObserver(this);
}

void SearchBoxView::NotifyQueryChanged() {
  DCHECK(delegate_);
  delegate_->QueryChanged(this);
}

void SearchBoxView::ContentsChanged(views::Textfield* sender,
                                    const base::string16& new_contents) {
  UpdateModel();
  view_delegate_->AutoLaunchCanceled();
  NotifyQueryChanged();
}

bool SearchBoxView::HandleKeyEvent(views::Textfield* sender,
                                   const ui::KeyEvent& key_event) {
  bool handled = false;
  if (key_event.key_code() == ui::VKEY_TAB) {
    if (focused_view_ != FOCUS_CONTENTS_VIEW &&
        MoveTabFocus(key_event.IsShiftDown()))
      return true;
  }

  if (focused_view_ == FOCUS_BACK_BUTTON && back_button_ &&
      back_button_->OnKeyPressed(key_event))
    return true;

  if (focused_view_ == FOCUS_MIC_BUTTON && speech_button_ &&
      speech_button_->OnKeyPressed(key_event))
    return true;

  if (contents_view_ && contents_view_->visible())
    handled = contents_view_->OnKeyPressed(key_event);

  // Arrow keys may have selected an item.  If they did, move focus off buttons.
  // If they didn't, we still select the first search item, in case they're
  // moving the caret through typed search text.  The UP arrow never moves
  // focus from text/buttons to app list/results, so ignore it.
  if (focused_view_ < FOCUS_CONTENTS_VIEW &&
      (key_event.key_code() == ui::VKEY_LEFT ||
       key_event.key_code() == ui::VKEY_RIGHT ||
       key_event.key_code() == ui::VKEY_DOWN)) {
    if (!handled)
      delegate_->SetSearchResultSelection(true);
    ResetTabFocus(handled);
  }

  return handled;
}

void SearchBoxView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  if (back_button_ && sender == back_button_)
    delegate_->BackButtonPressed();
  else if (speech_button_ && sender == speech_button_)
    view_delegate_->ToggleSpeechRecognition();
  else
    NOTREACHED();
}

void SearchBoxView::OnMenuButtonClicked(View* source, const gfx::Point& point) {
  if (!menu_)
    menu_.reset(new AppListMenuViews(view_delegate_));

  const gfx::Point menu_location =
      menu_button_->GetBoundsInScreen().bottom_right() +
      gfx::Vector2d(kMenuXOffsetFromButton, kMenuYOffsetFromButton);
  menu_->RunMenuAt(menu_button_, menu_location);
}

void SearchBoxView::IconChanged() {
  if (icon_view_)
    icon_view_->SetImage(model_->search_box()->icon());
}

void SearchBoxView::SpeechRecognitionButtonPropChanged() {
  const SearchBoxModel::SpeechButtonProperty* speech_button_prop =
      model_->search_box()->speech_button();
  if (speech_button_prop) {
    if (!speech_button_) {
      speech_button_ = new SearchBoxImageButton(this);
      content_container_->AddChildView(speech_button_);
    }

    speech_button_->SetAccessibleName(speech_button_prop->accessible_name);
    if (view_delegate_->GetSpeechUI()->state() ==
        SPEECH_RECOGNITION_HOTWORD_LISTENING) {
      speech_button_->SetImage(
          views::Button::STATE_NORMAL, &speech_button_prop->on_icon);
      speech_button_->SetTooltipText(speech_button_prop->on_tooltip);
    } else {
      speech_button_->SetImage(
          views::Button::STATE_NORMAL, &speech_button_prop->off_icon);
      speech_button_->SetTooltipText(speech_button_prop->off_tooltip);
    }
  } else {
    if (speech_button_) {
      // Deleting a view will detach it from its parent.
      delete speech_button_;
      speech_button_ = NULL;
    }
  }
  Layout();
}

void SearchBoxView::HintTextChanged() {
  const app_list::SearchBoxModel* search_box = model_->search_box();
  search_box_->set_placeholder_text(search_box->hint_text());
  search_box_->SetAccessibleName(search_box->accessible_name());
}

void SearchBoxView::SelectionModelChanged() {
  search_box_->SelectSelectionModel(model_->search_box()->selection_model());
}

void SearchBoxView::TextChanged() {
  search_box_->SetText(model_->search_box()->text());
  NotifyQueryChanged();
}

void SearchBoxView::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
  SpeechRecognitionButtonPropChanged();
  SchedulePaint();
}

}  // namespace app_list

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/open_pdf_in_reader_view.h"

#include "chrome/browser/ui/views/open_pdf_in_reader_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/pdf/browser/open_pdf_in_reader_prompt_client.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/widget/widget.h"

OpenPDFInReaderView::OpenPDFInReaderView() : bubble_(NULL), model_(NULL) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  if (!ui::MaterialDesignController::IsModeMaterial()) {
    SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_OMNIBOX_PDF_ICON));
  }
  SetTooltipText(l10n_util::GetStringUTF16(IDS_PDF_BUBBLE_OPEN_IN_READER_LINK));
}

OpenPDFInReaderView::~OpenPDFInReaderView() {
  if (bubble_)
    bubble_->GetWidget()->RemoveObserver(this);
}

void OpenPDFInReaderView::Update(content::WebContents* web_contents) {
  model_ = NULL;
  if (web_contents) {
    pdf::PDFWebContentsHelper* pdf_tab_helper =
        pdf::PDFWebContentsHelper::FromWebContents(web_contents);
    model_ = pdf_tab_helper->open_in_reader_prompt();
  }

  SetVisible(!!model_);

  // Hide the bubble if it is currently shown and the icon is hidden.
  if (!model_ && bubble_)
    bubble_->GetWidget()->Hide();
}

void OpenPDFInReaderView::ShowBubble() {
  if (bubble_)
    return;

  DCHECK(model_);
  bubble_ = new OpenPDFInReaderBubbleView(this, model_);
  views::BubbleDialogDelegateView::CreateBubble(bubble_);
  bubble_->GetWidget()->AddObserver(this);
  bubble_->GetWidget()->Show();
}

void OpenPDFInReaderView::GetAccessibleState(ui::AXViewState* state) {
  ImageView::GetAccessibleState(state);
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_OPEN_PDF_IN_READER);
  state->role = ui::AX_ROLE_BUTTON;
}

bool OpenPDFInReaderView::OnMousePressed(const ui::MouseEvent& event) {
  // Show the bubble on mouse release; that is standard button behavior.
  return true;
}

void OpenPDFInReaderView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()))
    ShowBubble();
}

bool OpenPDFInReaderView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_SPACE &&
      event.key_code() != ui::VKEY_RETURN) {
    return false;
  }

  ShowBubble();
  return true;
}

void OpenPDFInReaderView::OnNativeThemeChanged(
    const ui::NativeTheme* native_theme) {
  if (ui::MaterialDesignController::IsModeMaterial()) {
    SetImage(gfx::CreateVectorIcon(
        gfx::VectorIconId::PDF,
        color_utils::DeriveDefaultIconColor(native_theme->GetSystemColor(
            ui::NativeTheme::kColorId_TextfieldDefaultColor))));
  }
}

void OpenPDFInReaderView::OnWidgetDestroying(views::Widget* widget) {
  if (!bubble_)
    return;

  bubble_->GetWidget()->RemoveObserver(this);
  bubble_ = NULL;
}

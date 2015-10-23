// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_base.h"

#ifndef MANDOLINE_UI_AURA_INPUT_METHOD_MANDOLINE_H_
#define MANDOLINE_UI_AURA_INPUT_METHOD_MANDOLINE_H_

namespace mojo {
class View;
}  // namespace mojo

namespace mandoline {

class InputMethodMandoline : public ui::InputMethodBase {
 public:
  InputMethodMandoline(ui::internal::InputMethodDelegate* delegate,
                       mojo::View* view);
  ~InputMethodMandoline() override;

 private:
  // Overridden from ui::InputMethod:
  void OnFocus() override;
  void OnBlur() override;
  bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                NativeEventResult* result) override;
  void DispatchKeyEvent(ui::KeyEvent* event) override;
  void OnTextInputTypeChanged(const ui::TextInputClient* client) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void CancelComposition(const ui::TextInputClient* client) override;
  void OnInputLocaleChanged() override;
  std::string GetInputLocale() override;
  bool IsCandidatePopupOpen() const override;

  // Overridden from ui::InputMethodBase:
  void OnDidChangeFocusedClient(ui::TextInputClient* focused_before,
                                ui::TextInputClient* focused) override;

  void UpdateTextInputType();

  // The toplevel view which is not owned by this class.
  mojo::View* view_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodMandoline);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_INPUT_METHOD_MANDOLINE_H_

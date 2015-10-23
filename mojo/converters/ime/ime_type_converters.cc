// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/converters/ime/ime_type_converters.h"

#include "base/basictypes.h"
#include "base/macros.h"

namespace mojo {

#define TEXT_INPUT_TYPE_ASSERT(NAME) \
  COMPILE_ASSERT(static_cast<int32>(TEXT_INPUT_TYPE_##NAME) == \
                 static_cast<int32>(ui::TEXT_INPUT_TYPE_##NAME), \
                 text_input_type_should_match)
TEXT_INPUT_TYPE_ASSERT(NONE);
TEXT_INPUT_TYPE_ASSERT(TEXT);
TEXT_INPUT_TYPE_ASSERT(PASSWORD);
TEXT_INPUT_TYPE_ASSERT(SEARCH);
TEXT_INPUT_TYPE_ASSERT(EMAIL);
TEXT_INPUT_TYPE_ASSERT(NUMBER);
TEXT_INPUT_TYPE_ASSERT(TELEPHONE);
TEXT_INPUT_TYPE_ASSERT(URL);
TEXT_INPUT_TYPE_ASSERT(DATE);
TEXT_INPUT_TYPE_ASSERT(DATE_TIME);
TEXT_INPUT_TYPE_ASSERT(DATE_TIME_LOCAL);
TEXT_INPUT_TYPE_ASSERT(MONTH);
TEXT_INPUT_TYPE_ASSERT(TIME);
TEXT_INPUT_TYPE_ASSERT(WEEK);
TEXT_INPUT_TYPE_ASSERT(TEXT_AREA);

#define TEXT_INPUT_FLAG_ASSERT(NAME) \
  COMPILE_ASSERT(static_cast<int32>(TEXT_INPUT_FLAG_##NAME) == \
                 static_cast<int32>(ui::TEXT_INPUT_FLAG_##NAME), \
                 text_input_flag_should_match)
TEXT_INPUT_FLAG_ASSERT(NONE);
TEXT_INPUT_FLAG_ASSERT(AUTOCOMPLETE_ON);
TEXT_INPUT_FLAG_ASSERT(AUTOCOMPLETE_OFF);
TEXT_INPUT_FLAG_ASSERT(AUTOCORRECT_ON);
TEXT_INPUT_FLAG_ASSERT(AUTOCORRECT_OFF);
TEXT_INPUT_FLAG_ASSERT(SPELLCHECK_ON);
TEXT_INPUT_FLAG_ASSERT(SPELLCHECK_OFF);
TEXT_INPUT_FLAG_ASSERT(AUTOCAPITALIZE_NONE);
TEXT_INPUT_FLAG_ASSERT(AUTOCAPITALIZE_CHARACTERS);
TEXT_INPUT_FLAG_ASSERT(AUTOCAPITALIZE_WORDS);
TEXT_INPUT_FLAG_ASSERT(AUTOCAPITALIZE_SENTENCES);

// static
TextInputType TypeConverter<TextInputType, ui::TextInputType>::Convert(
    const ui::TextInputType& input) {
  return static_cast<TextInputType>(input);
}

// static
ui::TextInputType TypeConverter<ui::TextInputType, TextInputType>::Convert(
    const TextInputType& input) {
  return static_cast<ui::TextInputType>(input);
}

// static
ui::TextInputState
TypeConverter<ui::TextInputState, TextInputStatePtr>::Convert(
    const TextInputStatePtr& input) {
  return ui::TextInputState(ConvertTo<ui::TextInputType>(input->type),
                            input->flags,
                            input->text.To<std::string>(),
                            input->selection_start,
                            input->selection_end,
                            input->composition_start,
                            input->composition_end,
                            input->can_compose_inline);
}

}  // namespace mojo

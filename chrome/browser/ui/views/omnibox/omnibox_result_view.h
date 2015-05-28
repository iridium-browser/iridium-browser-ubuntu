// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "components/omnibox/autocomplete_match.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

class LocationBarView;
class OmniboxPopupContentsView;

namespace gfx {
class Canvas;
class RenderText;
}

class OmniboxResultView : public views::View,
                          private gfx::AnimationDelegate {
 public:
  // Keep these ordered from least dominant (normal) to most dominant
  // (selected).
  enum ResultViewState {
    NORMAL = 0,
    HOVERED,
    SELECTED,
    NUM_STATES
  };

  enum ColorKind {
    BACKGROUND = 0,
    TEXT,
    DIMMED_TEXT,
    URL,
    DIVIDER,
    NUM_KINDS
  };

  // The minimum distance between the top and bottom of the text and the
  // top or bottom of the row.
  static const int kMinimumTextVerticalPadding = 3;

  OmniboxResultView(OmniboxPopupContentsView* model,
                    int model_index,
                    LocationBarView* location_bar_view,
                    const gfx::FontList& font_list);
  ~OmniboxResultView() override;

  SkColor GetColor(ResultViewState state, ColorKind kind) const;

  // Updates the match used to paint the contents of this result view. We copy
  // the match so that we can continue to paint the last result even after the
  // model has changed.
  void SetMatch(const AutocompleteMatch& match);

  void ShowKeyword(bool show_keyword);

  void Invalidate();

  // views::View:
  gfx::Size GetPreferredSize() const override;

  ResultViewState GetState() const;

  // Returns the height of the text portion of the result view. In the base
  // class, this is the height of one line of text.
  virtual int GetTextHeight() const;

  // Returns the display width required for the match contents.
  int GetMatchContentsWidth() const;

  // Stores the image in a local data member and schedules a repaint.
  void SetAnswerImage(const gfx::ImageSkia& image);

 protected:
  // Paints the given |match| using the RenderText instances |contents| and
  // |description| at offset |x| in the bounds of this view.
  virtual void PaintMatch(const AutocompleteMatch& match,
                          gfx::RenderText* contents,
                          gfx::RenderText* description,
                          gfx::Canvas* canvas,
                          int x) const;

  // Draws given |render_text| on |canvas| at given location (|x|, |y|).
  // |contents| indicates whether the |render_text| is for the match contents
  // (rather than the separator or the description).  Additional properties from
  // |match| are used to render Infinite suggestions correctly.  If |max_width|
  // is a non-negative number, the text will be elided to fit within
  // |max_width|.  Returns the x position to the right of the string.
  int DrawRenderText(const AutocompleteMatch& match,
                     gfx::RenderText* render_text,
                     bool contents,
                     gfx::Canvas* canvas,
                     int x,
                     int y,
                     int max_width) const;

  // Creates a RenderText with given |text| and rendering defaults.
  scoped_ptr<gfx::RenderText> CreateRenderText(
      const base::string16& text) const;

  // Creates a RenderText with default rendering for the given |text|. The
  // |classifications| and |force_dim| are used to style the text.
  scoped_ptr<gfx::RenderText> CreateClassifiedRenderText(
      const base::string16& text,
      const ACMatchClassifications& classifications,
      bool force_dim) const;

  const gfx::Rect& text_bounds() const { return text_bounds_; }

  void set_edge_item_padding(int value) { edge_item_padding_ = value; }
  void set_item_padding(int value) { item_padding_ = value; }

 private:
  // views::View:
  const char* GetClassName() const override;

  gfx::ImageSkia GetIcon() const;
  const gfx::ImageSkia* GetKeywordIcon() const;

  // Whether to render only the keyword match.  Returns true if |match_| has an
  // associated keyword match that has been animated so close to the start that
  // the keyword match will hide even the icon of the regular match.
  bool ShowOnlyKeywordMatch() const;

  // Resets all RenderTexts for contents and description of the |match_| and its
  // associated keyword match.
  void ResetRenderTexts() const;

  // Initializes |contents_rendertext_| if it is NULL.
  void InitContentsRenderTextIfNecessary() const;

  // views::View:
  void Layout() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  // Returns the offset at which the contents of the |match| should be displayed
  // within the text bounds. The directionality of UI and match contents is used
  // to determine the offset relative to the correct edge.
  int GetDisplayOffset(const AutocompleteMatch& match,
                       bool is_ui_rtl,
                       bool is_match_contents_rtl) const;

  int GetAnswerLineHeight() const;
  int GetContentLineHeight() const;

  // Adds |text| to |description_rendertext_|.  |text_type| is an index into the
  // kTextStyles constant defined in the .cc file and is used to style the text,
  // including setting the font size, color, and baseline style.  See the
  // TextStyle struct in the .cc file for more.
  void AppendAnswerText(const base::string16& text, int text_type);

  static int default_icon_size_;

  // Default values cached here, may be overridden using the setters above.
  int edge_item_padding_;
  int item_padding_;

  // This row's model and model index.
  OmniboxPopupContentsView* model_;
  size_t model_index_;

  LocationBarView* location_bar_view_;
  // Note: image_service_ may be null in some unit tests.
  BitmapFetcherService* image_service_;

  const gfx::FontList font_list_;
  int font_height_;

  // A context used for mirroring regions.
  class MirroringContext;
  scoped_ptr<MirroringContext> mirroring_context_;

  AutocompleteMatch match_;

  gfx::Rect text_bounds_;
  gfx::Rect icon_bounds_;

  gfx::Rect keyword_text_bounds_;
  scoped_ptr<views::ImageView> keyword_icon_;

  scoped_ptr<gfx::SlideAnimation> animation_;

  // If the answer has an icon, these control the fetching and updating
  // of the icon.
  BitmapFetcherService::RequestId request_id_;
  gfx::ImageSkia answer_image_;

  // We preserve these RenderTexts so that we won't recreate them on every call
  // to GetMatchContentsWidth() or OnPaint().
  mutable scoped_ptr<gfx::RenderText> contents_rendertext_;
  mutable scoped_ptr<gfx::RenderText> description_rendertext_;
  mutable scoped_ptr<gfx::RenderText> separator_rendertext_;
  mutable scoped_ptr<gfx::RenderText> keyword_contents_rendertext_;
  mutable scoped_ptr<gfx::RenderText> keyword_description_rendertext_;

  mutable int separator_width_;

  base::WeakPtrFactory<OmniboxResultView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxResultView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_RESULT_VIEW_H_

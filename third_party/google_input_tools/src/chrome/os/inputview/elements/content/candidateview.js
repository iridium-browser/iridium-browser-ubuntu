// Copyright 2014 The ChromeOS IME Authors. All Rights Reserved.
// limitations under the License.
// See the License for the specific language governing permissions and
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// distributed under the License is distributed on an "AS-IS" BASIS,
// Unless required by applicable law or agreed to in writing, software
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// You may obtain a copy of the License at
// you may not use this file except in compliance with the License.
// Licensed under the Apache License, Version 2.0 (the "License");
//
goog.provide('i18n.input.chrome.inputview.elements.content.CandidateView');

goog.require('goog.a11y.aria');
goog.require('goog.a11y.aria.State');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.object');
goog.require('goog.style');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.GlobalFlags');
goog.require('i18n.input.chrome.inputview.elements.Element');
goog.require('i18n.input.chrome.inputview.elements.ElementType');
goog.require('i18n.input.chrome.inputview.elements.content.Candidate');
goog.require('i18n.input.chrome.inputview.elements.content.CandidateButton');
goog.require('i18n.input.chrome.inputview.elements.content.ToolbarButton');
goog.require('i18n.input.chrome.inputview.util');
goog.require('i18n.input.chrome.message.Name');



goog.scope(function() {
var Css = i18n.input.chrome.inputview.Css;
var TagName = goog.dom.TagName;
var Candidate = i18n.input.chrome.inputview.elements.content.Candidate;
var Type = i18n.input.chrome.inputview.elements.content.Candidate.Type;
var ElementType = i18n.input.chrome.inputview.elements.ElementType;
var content = i18n.input.chrome.inputview.elements.content;
var Name = i18n.input.chrome.message.Name;
var util = i18n.input.chrome.inputview.util;



/**
 * The candidate view.
 *
 * @param {string} id The id.
 * @param {!i18n.input.chrome.inputview.Adapter} adapter .
 * @param {goog.events.EventTarget=} opt_eventTarget The event target.
 * @constructor
 * @extends {i18n.input.chrome.inputview.elements.Element}
 */
i18n.input.chrome.inputview.elements.content.CandidateView = function(id,
    adapter, opt_eventTarget) {
  goog.base(this, id, ElementType.CANDIDATE_VIEW, opt_eventTarget);

  /**
   * The bus channel to communicate with background.
   *
   * @private {!i18n.input.chrome.inputview.Adapter}
   */
  this.adapter_ = adapter;

  /**
   * The icons.
   *
   * @private {!Array.<!i18n.input.chrome.inputview.elements.Element>}
   */
  this.iconButtons_ = [];

  this.iconButtons_[IconType.BACK] = new content.CandidateButton(
      '', ElementType.BACK_BUTTON, '',
      chrome.i18n.getMessage('HANDWRITING_BACK'), this);
  this.iconButtons_[IconType.SHRINK_CANDIDATES] = new content.
      CandidateButton('', ElementType.SHRINK_CANDIDATES,
          Css.SHRINK_CANDIDATES_ICON, '', this);

  this.iconButtons_[IconType.EXPAND_CANDIDATES] = new content.
      CandidateButton('', ElementType.EXPAND_CANDIDATES,
          Css.EXPAND_CANDIDATES_ICON, '', this);
  this.iconButtons_[IconType.VOICE] = new content.CandidateButton('',
      ElementType.VOICE_BTN, Css.VOICE_MIC_BAR, '', this, true);

  /**
   * Toolbar buttons.
   *
   * @private {!Array.<!i18n.input.chrome.inputview.elements.Element>}
   */
  this.toolbarButtons_ = [];

  this.toolbarButtons_.push(new content.
      ToolbarButton('', ElementType.UNDO, Css.UNDO_ICON, '', this, true));
  this.toolbarButtons_.push(new content.
      ToolbarButton('', ElementType.REDO, Css.REDO_ICON, '', this, true));
  this.toolbarButtons_.push(new content.
      ToolbarButton('', ElementType.BOLD, Css.BOLD_ICON, '', this, true));
  this.toolbarButtons_.push(new content.
      ToolbarButton('', ElementType.ITALICS, Css.ITALICS_ICON, '', this, true));
  this.toolbarButtons_.push(new content.ToolbarButton(
      '', ElementType.UNDERLINE, Css.UNDERLINE_ICON, '', this, true));
  this.toolbarButtons_.push(new content.
      ToolbarButton('', ElementType.CUT, Css.CUT_ICON, '', this));
  this.toolbarButtons_.push(new content.
      ToolbarButton('', ElementType.COPY, Css.COPY_ICON, '', this));
  this.toolbarButtons_.push(new content.
      ToolbarButton('', ElementType.PASTE, Css.PASTE_ICON, '', this));
  this.toolbarButtons_.push(new content.
      ToolbarButton('', ElementType.SELECT_ALL, Css.SELECT_ALL_ICON, '', this));
};
goog.inherits(i18n.input.chrome.inputview.elements.content.CandidateView,
    i18n.input.chrome.inputview.elements.Element);
var CandidateView = i18n.input.chrome.inputview.elements.content.CandidateView;


/**
 * The icon type at the right of the candidate view.
 *
 * @enum {number}
 */
CandidateView.IconType = {
  BACK: 0,
  SHRINK_CANDIDATES: 1,
  EXPAND_CANDIDATES: 2,
  VOICE: 3
};
var IconType = CandidateView.IconType;


/**
 * How many candidates in this view.
 *
 * @type {number}
 */
CandidateView.prototype.candidateCount = 0;


/**
 * The padding between candidates.
 *
 * @type {number}
 * @private
 */
CandidateView.PADDING_ = 50;


/**
 * The width in weight which stands for the entire row. It is used for the
 * alignment of the number row.
 *
 * @private {number}
 */
CandidateView.prototype.widthInWeight_ = 0;


/**
 * The width in weight of the backspace key.
 *
 * @private {number}
 */
CandidateView.prototype.backspaceWeight_ = 0;


/**
 * The width of the icon like voice/down/up arrow.
 *
 * @private {number}
 */
CandidateView.prototype.iconWidth_ = 120;


/**
 * True if it is showing candidate.
 *
 * @type {boolean}
 */
CandidateView.prototype.showingCandidates = false;


/**
 * True if it is showing number row.
 *
 * @type {boolean}
 */
CandidateView.prototype.showingNumberRow = false;


/**
 * True if showing the toolbar row.
 *
 * @type {boolean}
 */
CandidateView.prototype.showingToolbar = false;


/**
 * The width for a candidate when showing in THREE_CANDIDATE mode.
 *
 * @type {number}
 * @private
 */
CandidateView.WIDTH_FOR_THREE_CANDIDATES_ = 200;


/**
 * The width of icons in the toolbar.
 *
 * @private {number}
 */
CandidateView.TOOLBAR_ICON_WIDTH_ = 40;


/**
 * The handwriting keyset code.
 *
 * @private {string}
 */
CandidateView.HANDWRITING_VIEW_CODE_ = 'hwt';


/**
 * The emoji keyset code.
 *
 * @private {string}
 */
CandidateView.EMOJI_VIEW_CODE_ = 'emoji';


/** @private {string} */
CandidateView.prototype.keyset_ = '';


/**
 * The width of the inter container.
 *
 * @private {number}
 */
CandidateView.prototype.interContainerWidth_ = 0;


/** @private {boolean} */
CandidateView.prototype.navigation_ = false;


/** @private {number} */
CandidateView.prototype.sumOfCandidates_ = 0;


/** @override */
CandidateView.prototype.createDom = function() {
  goog.base(this, 'createDom');

  var dom = this.getDomHelper();
  var elem = this.getElement();
  goog.dom.classlist.add(elem, Css.CANDIDATE_VIEW);

  for (var i = 0; i < this.toolbarButtons_.length; i++) {
    var button = this.toolbarButtons_[i];
    button.render(elem);
    button.setVisible(false);
  }

  this.interContainer_ = dom.createDom(TagName.DIV,
      Css.CANDIDATE_INTER_CONTAINER);
  dom.appendChild(elem, this.interContainer_);

  for (var i = 0; i < this.iconButtons_.length; i++) {
    var button = this.iconButtons_[i];
    button.render(elem);
    button.setVisible(false);
    if (button.type == ElementType.VOICE_BTN) {
      goog.dom.classlist.add(button.getElement(), Css.VOICE_BUTTON);
    }
  }

  goog.a11y.aria.setState(/** @type {!Element} */
      (this.iconButtons_[IconType.SHRINK_CANDIDATES].getElement()),
      goog.a11y.aria.State.LABEL,
      chrome.i18n.getMessage('SHRINK_CANDIDATES'));

  goog.a11y.aria.setState(/** @type {!Element} */
      (this.iconButtons_[IconType.EXPAND_CANDIDATES].getElement()),
      goog.a11y.aria.State.LABEL,
      chrome.i18n.getMessage('EXPAND_CANDIDATES'));
};


/**
 * Hides the number row.
 */
CandidateView.prototype.hideNumberRow = function() {
  if (this.showingNumberRow) {
    this.getDomHelper().removeChildren(this.interContainer_);
    this.showingNumberRow = false;
  }
};


/**
 * Shows the number row.
 */
CandidateView.prototype.showNumberRow = function() {
  goog.dom.classlist.remove(this.getElement(),
      i18n.input.chrome.inputview.Css.THREE_CANDIDATES);
  var dom = this.getDomHelper();
  dom.removeChildren(this.interContainer_);
  var weightArray = [];
  for (var i = 0; i < 10; i++) {
    weightArray.push(1);
  }
  weightArray.push(this.widthInWeight_ - 10);
  var values = util.splitValue(weightArray, this.width);
  for (var i = 0; i < 10; i++) {
    var candidateElem = new Candidate(String(i), goog.object.create(
        Name.CANDIDATE, String((i + 1) % 10)),
        Type.NUMBER, this.height, false, values[i], this);
    candidateElem.render(this.interContainer_);
  }
  this.showingNumberRow = true;
  this.showingCandidates = false;
};


/**
 * Shows the candidates.
 *
 * @param {!Array.<!Object>} candidates The candidate list.
 * @param {boolean} showThreeCandidates .
 * @param {boolean=} opt_expandable True if the candidates would be shown
 *     in expanded view.
 */
CandidateView.prototype.showCandidates = function(candidates,
    showThreeCandidates, opt_expandable) {
  this.clearCandidates();
  this.sumOfCandidates_ = candidates.length;
  if (candidates.length > 0) {
    if (showThreeCandidates) {
      this.addThreeCandidates_(candidates);
    } else {
      this.addFullCandidates_(candidates);
      if (!this.iconButtons_[IconType.BACK].isVisible()) {
        this.switchToIcon(IconType.EXPAND_CANDIDATES,
            !!opt_expandable && this.candidateCount < candidates.length);
      }
    }
    this.showingCandidates = true;
    this.showingNumberRow = false;
  }
};


/**
 * Adds the candidates in THREE-CANDIDATE mode.
 *
 * @param {!Array.<!Object>} candidates The candidate list.
 * @private
 */
CandidateView.prototype.addThreeCandidates_ = function(candidates) {
  goog.dom.classlist.add(this.getElement(),
      i18n.input.chrome.inputview.Css.THREE_CANDIDATES);
  this.interContainer_.style.width = 'auto';
  var num = Math.min(3, candidates.length);
  var width = CandidateView.WIDTH_FOR_THREE_CANDIDATES_;
  if (this.showingToolbar) {
    width -= this.iconWidth_ / 3;
  }
  for (var i = 0; i < num; i++) {
    var candidateElem = new Candidate(String(i), candidates[i], Type.CANDIDATE,
        this.height, i == 1 || num == 1, width, this);
    candidateElem.render(this.interContainer_);
  }
  this.candidateCount = num;
};


/**
 * Clears the candidates.
 */
CandidateView.prototype.clearCandidates = function() {
  this.sumOfCandidates_ = 0;
  if (this.showingCandidates) {
    this.candidateCount = 0;
    this.getDomHelper().removeChildren(this.interContainer_);
    this.showingCandidates = false;
  }
};


/**
 * Adds candidates into the view, as many as the candidate bar can support.
 *
 * @param {!Array.<!Object>} candidates The candidate list.
 * @private
 */
CandidateView.prototype.addFullCandidates_ = function(candidates) {
  goog.dom.classlist.remove(this.getElement(),
      i18n.input.chrome.inputview.Css.THREE_CANDIDATES);
  var totalWidth = Math.floor(this.width - this.iconWidth_);
  var w = 0;
  var i;
  for (i = 0; i < candidates.length; i++) {
    var candidateElem = new Candidate(String(i), candidates[i], Type.CANDIDATE,
        this.height, false, undefined, this);
    candidateElem.render(this.interContainer_);
    var size = goog.style.getSize(candidateElem.getElement());
    var candidateWidth = size.width + CandidateView.PADDING_ * 2;
    // 1px is the width of the separator.
    w += candidateWidth + 1;

    if (w >= totalWidth) {
      if (i == 0) {
        // Make sure have one at least.
        candidateElem.setSize(totalWidth);
        ++i;
      } else {
        this.interContainer_.removeChild(candidateElem.getElement());
      }
      break;
    }
    candidateElem.setSize(candidateWidth);
  }
  this.candidateCount = i;
};


/**
 * Sets the widthInWeight which equals to a total line in the
 * keyset view and it is used for alignment of number row.
 *
 * @param {number} widthInWeight .
 * @param {number} backspaceWeight .
 */
CandidateView.prototype.setWidthInWeight = function(widthInWeight,
    backspaceWeight) {
  this.widthInWeight_ = widthInWeight;
  this.backspaceWeight_ = backspaceWeight;
};


/** @override */
CandidateView.prototype.resize = function(width, height) {
  if (this.backspaceWeight_ > 0) {
    var weightArray = [Math.round(this.widthInWeight_ - this.backspaceWeight_)];
    weightArray.push(this.backspaceWeight_);
    var values = util.splitValue(weightArray, width);
    this.iconWidth_ = values[values.length - 1];
  }
  goog.style.setSize(this.getElement(), width, height);
  if (!goog.dom.classlist.contains(this.getElement(),
      i18n.input.chrome.inputview.Css.THREE_CANDIDATES)) {
    goog.style.setSize(this.interContainer_, (width - this.iconWidth_), height);
  }
  for (var i = 0; i < this.iconButtons_.length; i++) {
    var button = this.iconButtons_[i];
    button.resize(this.iconWidth_, height);
  }

  for (var i = 0; i < this.toolbarButtons_.length; i++) {
    var button = this.toolbarButtons_[i];
    button.resize(CandidateView.TOOLBAR_ICON_WIDTH_, height);
  }

  // Resets the candidates elements visibility.
  if (this.candidateCount > 0) {
    var totalWidth = Math.floor(width - this.iconWidth_);
    var w = 0;
    for (i = 0; i < this.candidateCount; i++) {
      if (w <= totalWidth) {
        w += goog.style.getSize(this.interContainer_.children[i]).width;
      }
      goog.style.setElementShown(this.interContainer_.children[i],
          w <= totalWidth);
    }
  }

  goog.base(this, 'resize', width, height);

  if (this.showingNumberRow) {
    this.showNumberRow();
  }
};


/**
 * Switches to the icon, or hide it.
 *
 * @param {number} type .
 * @param {boolean} visible The visibility of back button.
 */
CandidateView.prototype.switchToIcon = function(type, visible) {
  if (visible) {
    for (var i = 0; i < this.iconButtons_.length; i++) {
      if (type != IconType.VOICE) {
        this.iconButtons_[i].setVisible(i == type);
      } else {
        this.iconButtons_[i].setVisible(i == type &&
            this.needToShowVoiceIcon_());
      }
    }
  } else {
    this.iconButtons_[type].setVisible(false);
    // When some icon turn to invisible, need to show voice icon.
    if (!visible && type != IconType.VOICE && this.needToShowVoiceIcon_()) {
      this.iconButtons_[IconType.VOICE].setVisible(true);
    }
  }
};


/**
 * Changes the visibility of the toolbar and it's icons.
 *
 * @param {boolean} visible The target visibility.
 */
CandidateView.prototype.setToolbarVisible = function(visible) {
  this.showingToolbar = visible;
  for (var i = 0; i < this.toolbarButtons_.length; i++) {
    this.toolbarButtons_[i].setVisible(visible);
  }
};


/**
 * Updates the candidate view by key set changing. Whether to show voice icon
 * or not.
 *
 * @param {string} keyset .
 * @param {boolean} isPasswordBox .
 * @param {boolean} isRTL .
 */
CandidateView.prototype.updateByKeyset = function(
    keyset, isPasswordBox, isRTL) {
  this.keyset_ = keyset;
  if (keyset == CandidateView.HANDWRITING_VIEW_CODE_ ||
      keyset == CandidateView.EMOJI_VIEW_CODE_) {
    // Handwriting and emoji keyset do not allow to show voice icon.
    // When it's not material design style, need to show BACK icon.
    if (!i18n.input.chrome.inputview.GlobalFlags.isQPInputView) {
      this.switchToIcon(IconType.BACK, true);
    } else {
      this.switchToIcon(IconType.VOICE, false);
    }
  } else {
    this.switchToIcon(IconType.VOICE, this.needToShowVoiceIcon_());
  }

  if (isPasswordBox && (keyset.indexOf('compact') != -1 &&
      keyset.indexOf('compact.symbol') == -1)) {
    this.showNumberRow();
  } else {
    this.hideNumberRow();
  }
  this.interContainer_.style.direction = isRTL ? 'rtl' : 'ltr';
};


/** @override */
CandidateView.prototype.disposeInternal = function() {
  goog.disposeAll(this.toolbarButtons_);
  goog.disposeAll(this.iconButtons_);

  goog.base(this, 'disposeInternal');
};


/**
 * Whether need to show voice icon on candidate view bar.
 *
 * @return {boolean}
 * @private
 */
CandidateView.prototype.needToShowVoiceIcon_ = function() {
  return this.adapter_.isVoiceInputEnabled &&
      this.adapter_.contextType != 'password' &&
      this.keyset_ != CandidateView.HANDWRITING_VIEW_CODE_ &&
      this.keyset_ != CandidateView.EMOJI_VIEW_CODE_ &&
      (!this.navigation_ || this.candidateCount == this.sumOfCandidates_);
};


/**
 * Sets the navigation value.
 *
 * @param {boolean} navigation .
 */
CandidateView.prototype.setNavigation = function(navigation) {
  this.navigation_ = navigation;
};
});  // goog.scope

/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @unrestricted
 */
UI.Dialog = class extends UI.Widget {
  constructor() {
    super(true);
    this.markAsRoot();
    this.registerRequiredCSS('ui/dialog.css');

    this.contentElement.createChild('content');
    this.contentElement.tabIndex = 0;
    this.contentElement.addEventListener('focus', this._onFocus.bind(this), false);
    this._keyDownBound = this._onKeyDown.bind(this);

    this._wrapsContent = false;
    this._dimmed = false;
    /** @type {!Map<!HTMLElement, number>} */
    this._tabIndexMap = new Map();
  }

  /**
   * @return {boolean}
   */
  static hasInstance() {
    return !!UI.Dialog._instance;
  }

  /**
   * @param {!UI.Widget} view
   */
  static setModalHostView(view) {
    UI.Dialog._modalHostView = view;
  }

  /**
   * FIXME: make utility method in Dialog, so clients use it instead of this getter.
   * Method should be like Dialog.showModalElement(position params, reposition callback).
   * @return {?UI.Widget}
   */
  static modalHostView() {
    return UI.Dialog._modalHostView;
  }

  static modalHostRepositioned() {
    if (UI.Dialog._instance)
      UI.Dialog._instance._position();
  }

  /**
   * @override
   */
  show() {
    if (UI.Dialog._instance)
      UI.Dialog._instance.detach();
    UI.Dialog._instance = this;

    var document = /** @type {!Document} */ (UI.Dialog._modalHostView.element.ownerDocument);
    this._disableTabIndexOnElements(document);

    this._glassPane = new UI.GlassPane(document, this._dimmed);
    this._glassPane.element.addEventListener('click', this._onGlassPaneClick.bind(this), false);
    this.element.ownerDocument.body.addEventListener('keydown', this._keyDownBound, false);

    super.show(this._glassPane.element);

    this._position();
    this._focusRestorer = new UI.WidgetFocusRestorer(this);
  }

  /**
   * @override
   */
  detach() {
    this._focusRestorer.restore();

    this.element.ownerDocument.body.removeEventListener('keydown', this._keyDownBound, false);
    super.detach();

    this._glassPane.dispose();
    delete this._glassPane;

    this._restoreTabIndexOnElements();

    delete UI.Dialog._instance;
  }

  addCloseButton() {
    var closeButton = this.contentElement.createChild('div', 'dialog-close-button', 'dt-close-button');
    closeButton.gray = true;
    closeButton.addEventListener('click', () => this.detach(), false);
  }

  /**
   * @param {number=} positionX
   * @param {number=} positionY
   */
  setPosition(positionX, positionY) {
    this._defaultPositionX = positionX;
    this._defaultPositionY = positionY;
  }

  /**
   * @param {!UI.Size} size
   */
  setMaxSize(size) {
    this._maxSize = size;
  }

  /**
   * @param {boolean} wraps
   */
  setWrapsContent(wraps) {
    this.element.classList.toggle('wraps-content', wraps);
    this._wrapsContent = wraps;
  }

  /**
   * @param {boolean} dimmed
   */
  setDimmed(dimmed) {
    this._dimmed = dimmed;
  }

  contentResized() {
    if (this._wrapsContent)
      this._position();
  }

  /**
   * @param {!Document} document
   */
  _disableTabIndexOnElements(document) {
    this._tabIndexMap.clear();
    for (var node = document; node; node = node.traverseNextNode(document)) {
      if (node instanceof HTMLElement) {
        var element = /** @type {!HTMLElement} */ (node);
        var tabIndex = element.tabIndex;
        if (tabIndex >= 0) {
          this._tabIndexMap.set(element, tabIndex);
          element.tabIndex = -1;
        }
      }
    }
  }

  _restoreTabIndexOnElements() {
    for (var element of this._tabIndexMap.keys())
      element.tabIndex = /** @type {number} */ (this._tabIndexMap.get(element));
    this._tabIndexMap.clear();
  }

  /**
   * @param {!Event} event
   */
  _onFocus(event) {
    this.focus();
  }

  /**
   * @param {!Event} event
   */
  _onGlassPaneClick(event) {
    if (!this.element.isSelfOrAncestor(/** @type {?Node} */ (event.target)))
      this.detach();
  }

  _position() {
    var container = UI.Dialog._modalHostView.element;

    var width = container.offsetWidth - 10;
    var height = container.offsetHeight - 10;

    if (this._wrapsContent) {
      width = Math.min(width, this.contentElement.offsetWidth);
      height = Math.min(height, this.contentElement.offsetHeight);
    }

    if (this._maxSize) {
      width = Math.min(width, this._maxSize.width);
      height = Math.min(height, this._maxSize.height);
    }

    var positionX;
    if (typeof this._defaultPositionX === 'number') {
      positionX = this._defaultPositionX;
    } else {
      positionX = (container.offsetWidth - width) / 2;
      positionX = Number.constrain(positionX, 0, container.offsetWidth - width);
    }

    var positionY;
    if (typeof this._defaultPositionY === 'number') {
      positionY = this._defaultPositionY;
    } else {
      positionY = (container.offsetHeight - height) / 2;
      positionY = Number.constrain(positionY, 0, container.offsetHeight - height);
    }

    this.element.style.width = width + 'px';
    this.element.style.height = height + 'px';
    this.element.positionAt(positionX, positionY, container);
  }

  /**
   * @param {!Event} event
   */
  _onKeyDown(event) {
    if (event.keyCode === UI.KeyboardShortcut.Keys.Esc.code) {
      event.consume(true);
      this.detach();
    }
  }
};


/** @type {?Element} */
UI.Dialog._previousFocusedElement = null;

/** @type {?UI.Widget} */
UI.Dialog._modalHostView = null;

"use strict";
// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = {
    argumentsReceived: false,
    params: null,
    picker: null
};

/**
 * @param {Event} event
 */
function handleMessage(event) {
    window.removeEventListener("message", handleMessage, false);
    initialize(JSON.parse(event.data));
    global.argumentsReceived = true;
}

/**
 * @param {!Object} args
 */
function initialize(args) {
    global.params = args;
    var main = $("main");
    main.innerHTML = "";
    global.picker = new ListPicker(main, args);
}

function handleArgumentsTimeout() {
    if (global.argumentsReceived)
        return;
    initialize({});
}

/**
 * @constructor
 * @param {!Element} element
 * @param {!Object} config
 */
function ListPicker(element, config) {
    Picker.call(this, element, config);
    window.pagePopupController.selectFontsFromOwnerDocument(document);
    this._selectElement = createElement("select");
    this._selectElement.size = 20;
    this._element.appendChild(this._selectElement);
    this._layout();
    this._selectElement.addEventListener("mouseup", this._handleMouseUp.bind(this), false);
    this._selectElement.addEventListener("touchstart", this._handleTouchStart.bind(this), false);
    this._selectElement.addEventListener("keydown", this._handleKeyDown.bind(this), false);
    this._selectElement.addEventListener("change", this._handleChange.bind(this), false);
    window.addEventListener("message", this._handleWindowMessage.bind(this), false);
    window.addEventListener("mousemove", this._handleWindowMouseMove.bind(this), false);
    window.addEventListener("touchmove", this._handleWindowTouchMove.bind(this), false);
    window.addEventListener("touchend", this._handleWindowTouchEnd.bind(this), false);
    this.lastMousePositionX = Infinity;
    this.lastMousePositionY = Infinity;
    this._selectionSetByMouseHover = false;

    this._trackingTouchId = null;

    this._handleWindowDidHide();
    this._selectElement.focus();
    this._selectElement.value = this._config.selectedIndex;
}
ListPicker.prototype = Object.create(Picker.prototype);

ListPicker.prototype._handleWindowDidHide = function() {
    this._fixWindowSize();
    var selectedOption = this._selectElement.options[this._selectElement.selectedIndex];
    if (selectedOption)
        selectedOption.scrollIntoView(false);
    window.removeEventListener("didHide", this._handleWindowDidHideBound, false);
};

ListPicker.prototype._handleWindowMessage = function(event) {
    eval(event.data);
    if (window.updateData.type === "update") {
        this._config.children = window.updateData.children;
        this._update();
    }
    delete window.updateData;
};

ListPicker.prototype._handleWindowMouseMove = function (event) {
    this.lastMousePositionX = event.clientX;
    this.lastMousePositionY = event.clientY;
    this._highlightOption(event.target);
    this._selectionSetByMouseHover = true;
    // Prevent the select element from firing change events for mouse input.
    event.preventDefault();
};

ListPicker.prototype._handleMouseUp = function(event) {
    if (event.target.tagName !== "OPTION")
        return;
    window.pagePopupController.setValueAndClosePopup(0, this._selectElement.value);
};

ListPicker.prototype._handleTouchStart = function(event) {
    if (this._trackingTouchId !== null)
        return;
    var touch = event.touches[0];
    this._trackingTouchId = touch.identifier;
    this._highlightOption(touch.target);
    this._selectionSetByMouseHover = false;
};

ListPicker.prototype._handleWindowTouchMove = function(event) {
    if (this._trackingTouchId === null)
        return;
    var touch = this._getTouchForId(event.touches, this._trackingTouchId);
    if (!touch)
        return;
    this._highlightOption(document.elementFromPoint(touch.clientX, touch.clientY));
    this._selectionSetByMouseHover = false;
};

ListPicker.prototype._handleWindowTouchEnd = function(event) {
    if (this._trackingTouchId === null)
        return;
    var touch = this._getTouchForId(event.changedTouches, this._trackingTouchId);
    if (!touch)
        return;
    var target = document.elementFromPoint(touch.clientX, touch.clientY)
    if (target.tagName === "OPTION")
        window.pagePopupController.setValueAndClosePopup(0, this._selectElement.value);
    this._trackingTouchId = null;
};

ListPicker.prototype._getTouchForId = function (touchList, id) {
    for (var i = 0; i < touchList.length; i++) {
        if (touchList[i].identifier === id)
            return touchList[i];
    }
    return null;
};

ListPicker.prototype._highlightOption = function(target) {
    if (target.tagName !== "OPTION" || target.selected)
        return;
    var savedScrollTop = this._selectElement.scrollTop;
    target.selected = true;
    this._selectElement.scrollTop = savedScrollTop;
};

ListPicker.prototype._handleChange = function(event) {
    window.pagePopupController.setValue(this._selectElement.value);
    this._selectionSetByMouseHover = false;
};

ListPicker.prototype._handleKeyDown = function(event) {
    var key = event.keyIdentifier;
    if (key === "U+001B") { // ESC
        window.pagePopupController.closePopup();
        event.preventDefault();
    } else if (key === "U+0009" /* TAB */ || key === "Enter") {
        window.pagePopupController.setValueAndClosePopup(0, this._selectElement.value);
        event.preventDefault();
    } else if (event.altKey && (key === "Down" || key === "Up")) {
        // We need to add a delay here because, if we do it immediately the key
        // press event will be handled by HTMLSelectElement and this popup will
        // be reopened.
        setTimeout(function () {
            window.pagePopupController.closePopup();
        }, 0);
        event.preventDefault();
    } else {
        // After a key press, we need to call setValue to reflect the selection
        // to the owner element. We can handle most cases with the change
        // event. But we need to call setValue even when the selection hasn't
        // changed. So we call it here too. setValue will be called twice for
        // some key presses but it won't matter.
        window.pagePopupController.setValue(this._selectElement.value);
    }
};

ListPicker.prototype._fixWindowSize = function() {
    this._selectElement.style.height = "";
    var maxHeight = this._selectElement.offsetHeight;
    // heightOutsideOfContent should be matched to border widths of the listbox
    // SELECT. See listPicker.css and html.css.
    var heightOutsideOfContent = 2;
    var noScrollHeight = Math.round(this._calculateScrollHeight() + heightOutsideOfContent);
    var desiredWindowHeight = noScrollHeight;
    var desiredWindowWidth = this._selectElement.offsetWidth;
    var expectingScrollbar = false;
    if (desiredWindowHeight > maxHeight) {
        desiredWindowHeight = maxHeight;
        // Setting overflow to auto does not increase width for the scrollbar
        // so we need to do it manually.
        desiredWindowWidth += getScrollbarWidth();
        expectingScrollbar = true;
    }
    desiredWindowWidth = Math.max(this._config.anchorRectInScreen.width, desiredWindowWidth);
    var windowRect = adjustWindowRect(desiredWindowWidth, desiredWindowHeight, this._selectElement.offsetWidth, 0);
    // If the available screen space is smaller than maxHeight, we will get an unexpected scrollbar.
    if (!expectingScrollbar && windowRect.height < noScrollHeight) {
        desiredWindowWidth = windowRect.width + getScrollbarWidth();
        windowRect = adjustWindowRect(desiredWindowWidth, windowRect.height, windowRect.width, windowRect.height);
    }
    this._selectElement.style.width = windowRect.width + "px";
    this._selectElement.style.height = windowRect.height + "px";
    this._element.style.height = windowRect.height + "px";
    setWindowRect(windowRect);
};

ListPicker.prototype._calculateScrollHeight = function() {
    // Element.scrollHeight returns an integer value but this calculate the
    // actual fractional value.
    var top = Infinity;
    var bottom = -Infinity;
    for (var i = 0; i < this._selectElement.children.length; i++) {
        var rect = this._selectElement.children[i].getBoundingClientRect();
        // Skip hidden elements.
        if (rect.width === 0 && rect.height === 0)
            continue;
        top = Math.min(top, rect.top);
        bottom = Math.max(bottom, rect.bottom);
    }
    return Math.max(bottom - top, 0);
};

ListPicker.prototype._listItemCount = function() {
    return this._selectElement.querySelectorAll("option,optgroup,hr").length;
};

ListPicker.prototype._layout = function() {
    if (this._config.isRTL)
        this._element.classList.add("rtl");
    this._selectElement.style.backgroundColor = this._config.backgroundColor;
    this._updateChildren(this._selectElement, this._config);
};

ListPicker.prototype._update = function() {
    var scrollPosition = this._selectElement.scrollTop;
    var oldValue = this._selectElement.value;
    this._layout();
    this._selectElement.value = this._config.selectedIndex;
    this._selectElement.scrollTop = scrollPosition;
    var optionUnderMouse = null;
    if (this._selectionSetByMouseHover) {
        var elementUnderMouse = document.elementFromPoint(this.lastMousePositionX, this.lastMousePositionY);
        optionUnderMouse = elementUnderMouse && elementUnderMouse.closest("option");
    }
    if (optionUnderMouse)
        optionUnderMouse.selected = true;
    else
        this._selectElement.value = oldValue;
    this._selectElement.scrollTop = scrollPosition;
    this.dispatchEvent("didUpdate");
};

/**
 * @param {!Element} parent Select element or optgroup element.
 * @param {!Object} config
 */
ListPicker.prototype._updateChildren = function(parent, config) {
    var outOfDateIndex = 0;
    var fragment = null;
    var inGroup = parent.tagName === "OPTGROUP";
    for (var i = 0; i < config.children.length; ++i) {
        var childConfig = config.children[i];
        var item = this._findReusableItem(parent, childConfig, outOfDateIndex) || this._createItemElement(childConfig);
        this._configureItem(item, childConfig, inGroup);
        if (outOfDateIndex < parent.children.length) {
            parent.insertBefore(item, parent.children[outOfDateIndex]);
        } else {
            if (!fragment)
                fragment = document.createDocumentFragment();
            fragment.appendChild(item);
        }
        outOfDateIndex++;
    }
    if (fragment) {
        parent.appendChild(fragment);
        return;
    }
    var unused = parent.children.length - outOfDateIndex;
    for (var i = 0; i < unused; i++) {
        parent.removeChild(parent.lastElementChild);
    }
};

ListPicker.prototype._findReusableItem = function(parent, config, startIndex) {
    if (startIndex >= parent.children.length)
        return null;
    var tagName = "OPTION";
    if (config.type === "optgroup")
        tagName = "OPTGROUP";
    else if (config.type === "separator")
        tagName = "HR";
    for (var i = startIndex; i < parent.children.length; i++) {
        var child = parent.children[i];
        if (tagName === child.tagName) {
            return child;
        }
    }
    return null;
};

ListPicker.prototype._createItemElement = function(config) {
    var element;
    if (config.type === "option")
        element = createElement("option");
    else if (config.type === "optgroup")
        element = createElement("optgroup");
    else if (config.type === "separator")
        element = createElement("hr");
    return element;
};

ListPicker.prototype._applyItemStyle = function(element, styleConfig) {
    if (!styleConfig)
        return;
    var style = element.style;
    style.visibility = styleConfig.visibility;
    style.display = styleConfig.display;
    style.direction = styleConfig.direction;
    style.unicodeBidi = styleConfig.unicodeBidi;
    if (!styleConfig.color)
        return;
    style.color = styleConfig.color;
    style.backgroundColor = styleConfig.backgroundColor;
    style.fontSize = styleConfig.fontSize + "px";
    style.fontWeight = styleConfig.fontWeight;
    style.fontFamily = styleConfig.fontFamily.join(",");
    style.fontStyle = styleConfig.fontStyle;
    style.fontVariant = styleConfig.fontVariant;
    style.textTransform = styleConfig.textTransform;
};

ListPicker.prototype._configureItem = function(element, config, inGroup) {
    if (config.type === "option") {
        element.label = config.label;
        element.value = config.value;
        element.title = config.title;
        element.disabled = config.disabled;
        element.setAttribute("aria-label", config.ariaLabel);
        element.style.webkitPaddingStart = this._config.paddingStart + "px";
        if (inGroup) {
            element.style.webkitMarginStart = (- this._config.paddingStart) + "px";
            // Should be synchronized with padding-end in listPicker.css.
            element.style.webkitMarginEnd = "-2px";
        }
    } else if (config.type === "optgroup") {
        element.label = config.label;
        element.title = config.title;
        element.disabled = config.disabled;
        element.setAttribute("aria-label", config.ariaLabel);
        this._updateChildren(element, config);
        element.style.webkitPaddingStart = this._config.paddingStart + "px";
    } else if (config.type === "separator") {
        element.title = config.title;
        element.disabled = config.disabled;
        element.setAttribute("aria-label", config.ariaLabel);
        if (inGroup) {
            element.style.webkitMarginStart = (- this._config.paddingStart) + "px";
            // Should be synchronized with padding-end in listPicker.css.
            element.style.webkitMarginEnd = "-2px";
        }
    }
    this._applyItemStyle(element, config.style);
};

if (window.dialogArguments) {
    initialize(dialogArguments);
} else {
    window.addEventListener("message", handleMessage, false);
    window.setTimeout(handleArgumentsTimeout, 1000);
}

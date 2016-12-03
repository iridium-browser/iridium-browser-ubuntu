// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
function PromiseResolver() {
  this.resolve_;
  this.reject_;
  this.promise_ = new Promise(function(resolve, reject) {
    this.resolve_ = resolve;
    this.reject_ = reject;
  }.bind(this));
}

PromiseResolver.prototype = {
  get promise() {
    return this.promise_;
  },
  set promise(p) {
    assertNotReached();
  },
  get resolve() {
    return this.resolve_;
  },
  set resolve(r) {
    assertNotReached();
  },
  get reject() {
    return this.reject_;
  },
  set reject(s) {
    assertNotReached();
  }
};

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var global = this;

var WebUIListener;

var cr = cr || function() {
  'use strict';
  function exportPath(name, opt_object, opt_objectToExportTo) {
    var parts = name.split('.');
    var cur = opt_objectToExportTo || global;
    for (var part; parts.length && (part = parts.shift()); ) {
      if (!parts.length && opt_object !== undefined) {
        cur[part] = opt_object;
      } else if (part in cur) {
        cur = cur[part];
      } else {
        cur = cur[part] = {};
      }
    }
    return cur;
  }
  function dispatchPropertyChange(target, propertyName, newValue, oldValue) {
    var e = new Event(propertyName + 'Change');
    e.propertyName = propertyName;
    e.newValue = newValue;
    e.oldValue = oldValue;
    target.dispatchEvent(e);
  }
  function getAttributeName(jsName) {
    return jsName.replace(/([A-Z])/g, '-$1').toLowerCase();
  }
  var PropertyKind = {
    JS: 'js',
    ATTR: 'attr',
    BOOL_ATTR: 'boolAttr'
  };
  function getGetter(name, kind) {
    switch (kind) {
     case PropertyKind.JS:
      var privateName = name + '_';
      return function() {
        return this[privateName];
      };

     case PropertyKind.ATTR:
      var attributeName = getAttributeName(name);
      return function() {
        return this.getAttribute(attributeName);
      };

     case PropertyKind.BOOL_ATTR:
      var attributeName = getAttributeName(name);
      return function() {
        return this.hasAttribute(attributeName);
      };
    }
    throw 'not reached';
  }
  function getSetter(name, kind, opt_setHook) {
    switch (kind) {
     case PropertyKind.JS:
      var privateName = name + '_';
      return function(value) {
        var oldValue = this[name];
        if (value !== oldValue) {
          this[privateName] = value;
          if (opt_setHook) opt_setHook.call(this, value, oldValue);
          dispatchPropertyChange(this, name, value, oldValue);
        }
      };

     case PropertyKind.ATTR:
      var attributeName = getAttributeName(name);
      return function(value) {
        var oldValue = this[name];
        if (value !== oldValue) {
          if (value == undefined) this.removeAttribute(attributeName); else this.setAttribute(attributeName, value);
          if (opt_setHook) opt_setHook.call(this, value, oldValue);
          dispatchPropertyChange(this, name, value, oldValue);
        }
      };

     case PropertyKind.BOOL_ATTR:
      var attributeName = getAttributeName(name);
      return function(value) {
        var oldValue = this[name];
        if (value !== oldValue) {
          if (value) this.setAttribute(attributeName, name); else this.removeAttribute(attributeName);
          if (opt_setHook) opt_setHook.call(this, value, oldValue);
          dispatchPropertyChange(this, name, value, oldValue);
        }
      };
    }
    throw 'not reached';
  }
  function defineProperty(obj, name, opt_kind, opt_setHook) {
    if (typeof obj == 'function') obj = obj.prototype;
    var kind = opt_kind || PropertyKind.JS;
    if (!obj.__lookupGetter__(name)) obj.__defineGetter__(name, getGetter(name, kind));
    if (!obj.__lookupSetter__(name)) obj.__defineSetter__(name, getSetter(name, kind, opt_setHook));
  }
  var uidCounter = 1;
  function createUid() {
    return uidCounter++;
  }
  function getUid(item) {
    if (item.hasOwnProperty('uid')) return item.uid;
    return item.uid = createUid();
  }
  function dispatchSimpleEvent(target, type, opt_bubbles, opt_cancelable) {
    var e = new Event(type, {
      bubbles: opt_bubbles,
      cancelable: opt_cancelable === undefined || opt_cancelable
    });
    return target.dispatchEvent(e);
  }
  function define(name, fun) {
    var obj = exportPath(name);
    var exports = fun();
    for (var propertyName in exports) {
      var propertyDescriptor = Object.getOwnPropertyDescriptor(exports, propertyName);
      if (propertyDescriptor) Object.defineProperty(obj, propertyName, propertyDescriptor);
    }
  }
  function addSingletonGetter(ctor) {
    ctor.getInstance = function() {
      return ctor.instance_ || (ctor.instance_ = new ctor());
    };
  }
  function makePublic(ctor, methods, opt_target) {
    methods.forEach(function(method) {
      ctor[method] = function() {
        var target = opt_target ? document.getElementById(opt_target) : ctor.getInstance();
        return target[method + '_'].apply(target, arguments);
      };
    });
  }
  var chromeSendResolverMap = {};
  function webUIResponse(id, isSuccess, response) {
    var resolver = chromeSendResolverMap[id];
    delete chromeSendResolverMap[id];
    if (isSuccess) resolver.resolve(response); else resolver.reject(response);
  }
  function sendWithPromise(methodName, var_args) {
    var args = Array.prototype.slice.call(arguments, 1);
    var promiseResolver = new PromiseResolver();
    var id = methodName + '_' + createUid();
    chromeSendResolverMap[id] = promiseResolver;
    chrome.send(methodName, [ id ].concat(args));
    return promiseResolver.promise;
  }
  var webUIListenerMap = {};
  function webUIListenerCallback(event, var_args) {
    var eventListenersMap = webUIListenerMap[event];
    if (!eventListenersMap) {
      return;
    }
    var args = Array.prototype.slice.call(arguments, 1);
    for (var listenerId in eventListenersMap) {
      eventListenersMap[listenerId].apply(null, args);
    }
  }
  function addWebUIListener(eventName, callback) {
    webUIListenerMap[eventName] = webUIListenerMap[eventName] || {};
    var uid = createUid();
    webUIListenerMap[eventName][uid] = callback;
    return {
      eventName: eventName,
      uid: uid
    };
  }
  function removeWebUIListener(listener) {
    var listenerExists = webUIListenerMap[listener.eventName] && webUIListenerMap[listener.eventName][listener.uid];
    if (listenerExists) {
      delete webUIListenerMap[listener.eventName][listener.uid];
      return true;
    }
    return false;
  }
  return {
    addSingletonGetter: addSingletonGetter,
    createUid: createUid,
    define: define,
    defineProperty: defineProperty,
    dispatchPropertyChange: dispatchPropertyChange,
    dispatchSimpleEvent: dispatchSimpleEvent,
    exportPath: exportPath,
    getUid: getUid,
    makePublic: makePublic,
    PropertyKind: PropertyKind,
    addWebUIListener: addWebUIListener,
    removeWebUIListener: removeWebUIListener,
    sendWithPromise: sendWithPromise,
    webUIListenerCallback: webUIListenerCallback,
    webUIResponse: webUIResponse,
    get doc() {
      return document;
    },
    get isMac() {
      return /Mac/.test(navigator.platform);
    },
    get isWindows() {
      return /Win/.test(navigator.platform);
    },
    get isChromeOS() {
      return /CrOS/.test(navigator.userAgent);
    },
    get isLinux() {
      return /Linux/.test(navigator.userAgent);
    },
    get isAndroid() {
      return /Android/.test(navigator.userAgent);
    },
    get isIOS() {
      return /iPad|iPhone|iPod/.test(navigator.platform);
    }
  };
}();

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('cr.ui', function() {
  function decorate(source, constr) {
    var elements;
    if (typeof source == 'string') elements = cr.doc.querySelectorAll(source); else elements = [ source ];
    for (var i = 0, el; el = elements[i]; i++) {
      if (!(el instanceof constr)) constr.decorate(el);
    }
  }
  function createElementHelper(tagName, opt_bag) {
    var doc;
    if (opt_bag && opt_bag.ownerDocument) doc = opt_bag.ownerDocument; else doc = cr.doc;
    return doc.createElement(tagName);
  }
  function define(tagNameOrFunction) {
    var createFunction, tagName;
    if (typeof tagNameOrFunction == 'function') {
      createFunction = tagNameOrFunction;
      tagName = '';
    } else {
      createFunction = createElementHelper;
      tagName = tagNameOrFunction;
    }
    function f(opt_propertyBag) {
      var el = createFunction(tagName, opt_propertyBag);
      f.decorate(el);
      for (var propertyName in opt_propertyBag) {
        el[propertyName] = opt_propertyBag[propertyName];
      }
      return el;
    }
    f.decorate = function(el) {
      el.__proto__ = f.prototype;
      el.decorate();
    };
    return f;
  }
  function limitInputWidth(el, parentEl, min, opt_scale) {
    el.style.width = '10px';
    var doc = el.ownerDocument;
    var win = doc.defaultView;
    var computedStyle = win.getComputedStyle(el);
    var parentComputedStyle = win.getComputedStyle(parentEl);
    var rtl = computedStyle.direction == 'rtl';
    var inputRect = el.getBoundingClientRect();
    var parentRect = parentEl.getBoundingClientRect();
    var startPos = rtl ? parentRect.right - inputRect.right : inputRect.left - parentRect.left;
    var inner = parseInt(computedStyle.borderLeftWidth, 10) + parseInt(computedStyle.paddingLeft, 10) + parseInt(computedStyle.paddingRight, 10) + parseInt(computedStyle.borderRightWidth, 10);
    var parentPadding = rtl ? parseInt(parentComputedStyle.paddingLeft, 10) : parseInt(parentComputedStyle.paddingRight, 10);
    var max = parentEl.clientWidth - startPos - inner - parentPadding;
    if (opt_scale) max *= opt_scale;
    function limit() {
      if (el.scrollWidth > max) {
        el.style.width = max + 'px';
      } else {
        el.style.width = 0;
        var sw = el.scrollWidth;
        if (sw < min) {
          el.style.width = min + 'px';
        } else {
          el.style.width = sw + 'px';
        }
      }
    }
    el.addEventListener('input', limit);
    limit();
  }
  function toCssPx(pixels) {
    if (!window.isFinite(pixels)) console.error('Pixel value is not a number: ' + pixels);
    return Math.round(pixels) + 'px';
  }
  function swallowDoubleClick(e) {
    var doc = e.target.ownerDocument;
    var counter = Math.min(1, e.detail);
    function swallow(e) {
      e.stopPropagation();
      e.preventDefault();
    }
    function onclick(e) {
      if (e.detail > counter) {
        counter = e.detail;
        swallow(e);
      } else {
        doc.removeEventListener('dblclick', swallow, true);
        doc.removeEventListener('click', onclick, true);
      }
    }
    setTimeout(function() {
      doc.addEventListener('click', onclick, true);
      doc.addEventListener('dblclick', swallow, true);
    }, 0);
  }
  return {
    decorate: decorate,
    define: define,
    limitInputWidth: limitInputWidth,
    toCssPx: toCssPx,
    swallowDoubleClick: swallowDoubleClick
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('cr.ui', function() {
  function KeyboardShortcut(shortcut) {
    var mods = {};
    var ident = '';
    shortcut.split('|').forEach(function(part) {
      var partLc = part.toLowerCase();
      switch (partLc) {
       case 'alt':
       case 'ctrl':
       case 'meta':
       case 'shift':
        mods[partLc + 'Key'] = true;
        break;

       default:
        if (ident) throw Error('Invalid shortcut');
        ident = part;
      }
    });
    this.ident_ = ident;
    this.mods_ = mods;
  }
  KeyboardShortcut.prototype = {
    matchesEvent: function(e) {
      if (e.key == this.ident_) {
        var mods = this.mods_;
        return [ 'altKey', 'ctrlKey', 'metaKey', 'shiftKey' ].every(function(k) {
          return e[k] == !!mods[k];
        });
      }
      return false;
    }
  };
  var Command = cr.ui.define('command');
  Command.prototype = {
    __proto__: HTMLElement.prototype,
    decorate: function() {
      CommandManager.init(assert(this.ownerDocument));
      if (this.hasAttribute('shortcut')) this.shortcut = this.getAttribute('shortcut');
    },
    execute: function(opt_element) {
      if (this.disabled) return;
      var doc = this.ownerDocument;
      if (doc.activeElement) {
        var e = new Event('command', {
          bubbles: true
        });
        e.command = this;
        (opt_element || doc.activeElement).dispatchEvent(e);
      }
    },
    canExecuteChange: function(opt_node) {
      dispatchCanExecuteEvent(this, opt_node || this.ownerDocument.activeElement);
    },
    shortcut_: '',
    get shortcut() {
      return this.shortcut_;
    },
    set shortcut(shortcut) {
      var oldShortcut = this.shortcut_;
      if (shortcut !== oldShortcut) {
        this.keyboardShortcuts_ = shortcut.split(/\s+/).map(function(shortcut) {
          return new KeyboardShortcut(shortcut);
        });
        this.shortcut_ = shortcut;
        cr.dispatchPropertyChange(this, 'shortcut', this.shortcut_, oldShortcut);
      }
    },
    matchesEvent: function(e) {
      if (!this.keyboardShortcuts_) return false;
      return this.keyboardShortcuts_.some(function(keyboardShortcut) {
        return keyboardShortcut.matchesEvent(e);
      });
    }
  };
  cr.defineProperty(Command, 'label', cr.PropertyKind.ATTR);
  cr.defineProperty(Command, 'disabled', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(Command, 'hidden', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(Command, 'checked', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(Command, 'hideShortcutText', cr.PropertyKind.BOOL_ATTR);
  function dispatchCanExecuteEvent(command, target) {
    var e = new CanExecuteEvent(command);
    target.dispatchEvent(e);
    command.disabled = !e.canExecute;
  }
  var commandManagers = {};
  function CommandManager(doc) {
    doc.addEventListener('focus', this.handleFocus_.bind(this), true);
    doc.addEventListener('keydown', this.handleKeyDown_.bind(this), false);
  }
  CommandManager.init = function(doc) {
    var uid = cr.getUid(doc);
    if (!(uid in commandManagers)) {
      commandManagers[uid] = new CommandManager(doc);
    }
  };
  CommandManager.prototype = {
    handleFocus_: function(e) {
      var target = e.target;
      if (target.menu || target.command) return;
      var commands = Array.prototype.slice.call(target.ownerDocument.querySelectorAll('command'));
      commands.forEach(function(command) {
        dispatchCanExecuteEvent(command, target);
      });
    },
    handleKeyDown_: function(e) {
      var target = e.target;
      var commands = Array.prototype.slice.call(target.ownerDocument.querySelectorAll('command'));
      for (var i = 0, command; command = commands[i]; i++) {
        if (command.matchesEvent(e)) {
          command.canExecuteChange();
          if (!command.disabled) {
            e.preventDefault();
            e.stopPropagation();
            command.execute();
            return;
          }
        }
      }
    }
  };
  function CanExecuteEvent(command) {
    var e = new Event('canExecute', {
      bubbles: true,
      cancelable: true
    });
    e.__proto__ = CanExecuteEvent.prototype;
    e.command = command;
    return e;
  }
  CanExecuteEvent.prototype = {
    __proto__: Event.prototype,
    command: null,
    canExecute_: false,
    get canExecute() {
      return this.canExecute_;
    },
    set canExecute(canExecute) {
      this.canExecute_ = !!canExecute;
      this.stopPropagation();
      this.preventDefault();
    }
  };
  return {
    Command: Command,
    CanExecuteEvent: CanExecuteEvent
  };
});

Polymer({
  is: 'app-drawer',
  properties: {
    opened: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true
    },
    persistent: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },
    align: {
      type: String,
      value: 'left'
    },
    position: {
      type: String,
      readOnly: true,
      value: 'left',
      reflectToAttribute: true
    },
    swipeOpen: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },
    noFocusTrap: {
      type: Boolean,
      value: false
    }
  },
  observers: [ 'resetLayout(position)', '_resetPosition(align, isAttached)' ],
  _translateOffset: 0,
  _trackDetails: null,
  _drawerState: 0,
  _boundEscKeydownHandler: null,
  _firstTabStop: null,
  _lastTabStop: null,
  ready: function() {
    this.setScrollDirection('y');
    this._setTransitionDuration('0s');
  },
  attached: function() {
    Polymer.RenderStatus.afterNextRender(this, function() {
      this._setTransitionDuration('');
      this._boundEscKeydownHandler = this._escKeydownHandler.bind(this);
      this._resetDrawerState();
      this.listen(this, 'track', '_track');
      this.addEventListener('transitionend', this._transitionend.bind(this));
      this.addEventListener('keydown', this._tabKeydownHandler.bind(this));
    });
  },
  detached: function() {
    document.removeEventListener('keydown', this._boundEscKeydownHandler);
  },
  open: function() {
    this.opened = true;
  },
  close: function() {
    this.opened = false;
  },
  toggle: function() {
    this.opened = !this.opened;
  },
  getWidth: function() {
    return this.$.contentContainer.offsetWidth;
  },
  resetLayout: function() {
    this.debounce('_resetLayout', function() {
      this.fire('app-drawer-reset-layout');
    }, 1);
  },
  _isRTL: function() {
    return window.getComputedStyle(this).direction === 'rtl';
  },
  _resetPosition: function() {
    switch (this.align) {
     case 'start':
      this._setPosition(this._isRTL() ? 'right' : 'left');
      return;

     case 'end':
      this._setPosition(this._isRTL() ? 'left' : 'right');
      return;
    }
    this._setPosition(this.align);
  },
  _escKeydownHandler: function(event) {
    var ESC_KEYCODE = 27;
    if (event.keyCode === ESC_KEYCODE) {
      event.preventDefault();
      this.close();
    }
  },
  _track: function(event) {
    if (this.persistent) {
      return;
    }
    event.preventDefault();
    switch (event.detail.state) {
     case 'start':
      this._trackStart(event);
      break;

     case 'track':
      this._trackMove(event);
      break;

     case 'end':
      this._trackEnd(event);
      break;
    }
  },
  _trackStart: function(event) {
    this._drawerState = this._DRAWER_STATE.TRACKING;
    this._setTransitionDuration('0s');
    this.style.visibility = 'visible';
    var rect = this.$.contentContainer.getBoundingClientRect();
    if (this.position === 'left') {
      this._translateOffset = rect.left;
    } else {
      this._translateOffset = rect.right - window.innerWidth;
    }
    this._trackDetails = [];
  },
  _trackMove: function(event) {
    this._translateDrawer(event.detail.dx + this._translateOffset);
    this._trackDetails.push({
      dx: event.detail.dx,
      timeStamp: Date.now()
    });
  },
  _trackEnd: function(event) {
    var x = event.detail.dx + this._translateOffset;
    var drawerWidth = this.getWidth();
    var isPositionLeft = this.position === 'left';
    var isInEndState = isPositionLeft ? x >= 0 || x <= -drawerWidth : x <= 0 || x >= drawerWidth;
    if (!isInEndState) {
      var trackDetails = this._trackDetails;
      this._trackDetails = null;
      this._flingDrawer(event, trackDetails);
      if (this._drawerState === this._DRAWER_STATE.FLINGING) {
        return;
      }
    }
    var halfWidth = drawerWidth / 2;
    if (event.detail.dx < -halfWidth) {
      this.opened = this.position === 'right';
    } else if (event.detail.dx > halfWidth) {
      this.opened = this.position === 'left';
    }
    if (isInEndState) {
      this._resetDrawerState();
    }
    this._setTransitionDuration('');
    this._resetDrawerTranslate();
    this.style.visibility = '';
  },
  _calculateVelocity: function(event, trackDetails) {
    var now = Date.now();
    var timeLowerBound = now - 100;
    var trackDetail;
    var min = 0;
    var max = trackDetails.length - 1;
    while (min <= max) {
      var mid = min + max >> 1;
      var d = trackDetails[mid];
      if (d.timeStamp >= timeLowerBound) {
        trackDetail = d;
        max = mid - 1;
      } else {
        min = mid + 1;
      }
    }
    if (trackDetail) {
      var dx = event.detail.dx - trackDetail.dx;
      var dt = now - trackDetail.timeStamp || 1;
      return dx / dt;
    }
    return 0;
  },
  _flingDrawer: function(event, trackDetails) {
    var velocity = this._calculateVelocity(event, trackDetails);
    if (Math.abs(velocity) < this._MIN_FLING_THRESHOLD) {
      return;
    }
    this._drawerState = this._DRAWER_STATE.FLINGING;
    var x = event.detail.dx + this._translateOffset;
    var drawerWidth = this.getWidth();
    var isPositionLeft = this.position === 'left';
    var isVelocityPositive = velocity > 0;
    var isClosingLeft = !isVelocityPositive && isPositionLeft;
    var isClosingRight = isVelocityPositive && !isPositionLeft;
    var dx;
    if (isClosingLeft) {
      dx = -(x + drawerWidth);
    } else if (isClosingRight) {
      dx = drawerWidth - x;
    } else {
      dx = -x;
    }
    if (isVelocityPositive) {
      velocity = Math.max(velocity, this._MIN_TRANSITION_VELOCITY);
      this.opened = this.position === 'left';
    } else {
      velocity = Math.min(velocity, -this._MIN_TRANSITION_VELOCITY);
      this.opened = this.position === 'right';
    }
    this._setTransitionDuration(this._FLING_INITIAL_SLOPE * dx / velocity + 'ms');
    this._setTransitionTimingFunction(this._FLING_TIMING_FUNCTION);
    this._resetDrawerTranslate();
  },
  _transitionend: function(event) {
    var target = Polymer.dom(event).rootTarget;
    if (target === this.$.contentContainer || target === this.$.scrim) {
      if (this._drawerState === this._DRAWER_STATE.FLINGING) {
        this._setTransitionDuration('');
        this._setTransitionTimingFunction('');
        this.style.visibility = '';
      }
      this._resetDrawerState();
    }
  },
  _setTransitionDuration: function(duration) {
    this.$.contentContainer.style.transitionDuration = duration;
    this.$.scrim.style.transitionDuration = duration;
  },
  _setTransitionTimingFunction: function(timingFunction) {
    this.$.contentContainer.style.transitionTimingFunction = timingFunction;
    this.$.scrim.style.transitionTimingFunction = timingFunction;
  },
  _translateDrawer: function(x) {
    var drawerWidth = this.getWidth();
    if (this.position === 'left') {
      x = Math.max(-drawerWidth, Math.min(x, 0));
      this.$.scrim.style.opacity = 1 + x / drawerWidth;
    } else {
      x = Math.max(0, Math.min(x, drawerWidth));
      this.$.scrim.style.opacity = 1 - x / drawerWidth;
    }
    this.translate3d(x + 'px', '0', '0', this.$.contentContainer);
  },
  _resetDrawerTranslate: function() {
    this.$.scrim.style.opacity = '';
    this.transform('', this.$.contentContainer);
  },
  _resetDrawerState: function() {
    var oldState = this._drawerState;
    if (this.opened) {
      this._drawerState = this.persistent ? this._DRAWER_STATE.OPENED_PERSISTENT : this._DRAWER_STATE.OPENED;
    } else {
      this._drawerState = this._DRAWER_STATE.CLOSED;
    }
    if (oldState !== this._drawerState) {
      if (this._drawerState === this._DRAWER_STATE.OPENED) {
        this._setKeyboardFocusTrap();
        document.addEventListener('keydown', this._boundEscKeydownHandler);
        document.body.style.overflow = 'hidden';
      } else {
        document.removeEventListener('keydown', this._boundEscKeydownHandler);
        document.body.style.overflow = '';
      }
      if (oldState !== this._DRAWER_STATE.INIT) {
        this.fire('app-drawer-transitioned');
      }
    }
  },
  _setKeyboardFocusTrap: function() {
    if (this.noFocusTrap) {
      return;
    }
    var focusableElementsSelector = [ 'a[href]:not([tabindex="-1"])', 'area[href]:not([tabindex="-1"])', 'input:not([disabled]):not([tabindex="-1"])', 'select:not([disabled]):not([tabindex="-1"])', 'textarea:not([disabled]):not([tabindex="-1"])', 'button:not([disabled]):not([tabindex="-1"])', 'iframe:not([tabindex="-1"])', '[tabindex]:not([tabindex="-1"])', '[contentEditable=true]:not([tabindex="-1"])' ].join(',');
    var focusableElements = Polymer.dom(this).querySelectorAll(focusableElementsSelector);
    if (focusableElements.length > 0) {
      this._firstTabStop = focusableElements[0];
      this._lastTabStop = focusableElements[focusableElements.length - 1];
    } else {
      this._firstTabStop = null;
      this._lastTabStop = null;
    }
    var tabindex = this.getAttribute('tabindex');
    if (tabindex && parseInt(tabindex, 10) > -1) {
      this.focus();
    } else if (this._firstTabStop) {
      this._firstTabStop.focus();
    }
  },
  _tabKeydownHandler: function(event) {
    if (this.noFocusTrap) {
      return;
    }
    var TAB_KEYCODE = 9;
    if (this._drawerState === this._DRAWER_STATE.OPENED && event.keyCode === TAB_KEYCODE) {
      if (event.shiftKey) {
        if (this._firstTabStop && Polymer.dom(event).localTarget === this._firstTabStop) {
          event.preventDefault();
          this._lastTabStop.focus();
        }
      } else {
        if (this._lastTabStop && Polymer.dom(event).localTarget === this._lastTabStop) {
          event.preventDefault();
          this._firstTabStop.focus();
        }
      }
    }
  },
  _MIN_FLING_THRESHOLD: .2,
  _MIN_TRANSITION_VELOCITY: 1.2,
  _FLING_TIMING_FUNCTION: 'cubic-bezier(0.667, 1, 0.667, 1)',
  _FLING_INITIAL_SLOPE: 1.5,
  _DRAWER_STATE: {
    INIT: 0,
    OPENED: 1,
    OPENED_PERSISTENT: 2,
    CLOSED: 3,
    TRACKING: 4,
    FLINGING: 5
  }
});

(function() {
  'use strict';
  Polymer({
    is: 'iron-location',
    properties: {
      path: {
        type: String,
        notify: true,
        value: function() {
          return window.decodeURIComponent(window.location.pathname);
        }
      },
      query: {
        type: String,
        notify: true,
        value: function() {
          return window.decodeURIComponent(window.location.search.slice(1));
        }
      },
      hash: {
        type: String,
        notify: true,
        value: function() {
          return window.decodeURIComponent(window.location.hash.slice(1));
        }
      },
      dwellTime: {
        type: Number,
        value: 2e3
      },
      urlSpaceRegex: {
        type: String,
        value: ''
      },
      _urlSpaceRegExp: {
        computed: '_makeRegExp(urlSpaceRegex)'
      },
      _lastChangedAt: {
        type: Number
      },
      _initialized: {
        type: Boolean,
        value: false
      }
    },
    hostAttributes: {
      hidden: true
    },
    observers: [ '_updateUrl(path, query, hash)' ],
    attached: function() {
      this.listen(window, 'hashchange', '_hashChanged');
      this.listen(window, 'location-changed', '_urlChanged');
      this.listen(window, 'popstate', '_urlChanged');
      this.listen(document.body, 'click', '_globalOnClick');
      this._lastChangedAt = window.performance.now() - (this.dwellTime - 200);
      this._initialized = true;
      this._urlChanged();
    },
    detached: function() {
      this.unlisten(window, 'hashchange', '_hashChanged');
      this.unlisten(window, 'location-changed', '_urlChanged');
      this.unlisten(window, 'popstate', '_urlChanged');
      this.unlisten(document.body, 'click', '_globalOnClick');
      this._initialized = false;
    },
    _hashChanged: function() {
      this.hash = window.decodeURIComponent(window.location.hash.substring(1));
    },
    _urlChanged: function() {
      this._dontUpdateUrl = true;
      this._hashChanged();
      this.path = window.decodeURIComponent(window.location.pathname);
      this.query = window.decodeURIComponent(window.location.search.substring(1));
      this._dontUpdateUrl = false;
      this._updateUrl();
    },
    _getUrl: function() {
      var partiallyEncodedPath = window.encodeURI(this.path).replace(/\#/g, '%23').replace(/\?/g, '%3F');
      var partiallyEncodedQuery = '';
      if (this.query) {
        partiallyEncodedQuery = '?' + window.encodeURI(this.query).replace(/\#/g, '%23');
      }
      var partiallyEncodedHash = '';
      if (this.hash) {
        partiallyEncodedHash = '#' + window.encodeURI(this.hash);
      }
      return partiallyEncodedPath + partiallyEncodedQuery + partiallyEncodedHash;
    },
    _updateUrl: function() {
      if (this._dontUpdateUrl || !this._initialized) {
        return;
      }
      if (this.path === window.decodeURIComponent(window.location.pathname) && this.query === window.decodeURIComponent(window.location.search.substring(1)) && this.hash === window.decodeURIComponent(window.location.hash.substring(1))) {
        return;
      }
      var newUrl = this._getUrl();
      var fullNewUrl = new URL(newUrl, window.location.protocol + '//' + window.location.host).href;
      var now = window.performance.now();
      var shouldReplace = this._lastChangedAt + this.dwellTime > now;
      this._lastChangedAt = now;
      if (shouldReplace) {
        window.history.replaceState({}, '', fullNewUrl);
      } else {
        window.history.pushState({}, '', fullNewUrl);
      }
      this.fire('location-changed', {}, {
        node: window
      });
    },
    _globalOnClick: function(event) {
      if (event.defaultPrevented) {
        return;
      }
      var href = this._getSameOriginLinkHref(event);
      if (!href) {
        return;
      }
      event.preventDefault();
      if (href === window.location.href) {
        return;
      }
      window.history.pushState({}, '', href);
      this.fire('location-changed', {}, {
        node: window
      });
    },
    _getSameOriginLinkHref: function(event) {
      if (event.button !== 0) {
        return null;
      }
      if (event.metaKey || event.ctrlKey) {
        return null;
      }
      var eventPath = Polymer.dom(event).path;
      var anchor = null;
      for (var i = 0; i < eventPath.length; i++) {
        var element = eventPath[i];
        if (element.tagName === 'A' && element.href) {
          anchor = element;
          break;
        }
      }
      if (!anchor) {
        return null;
      }
      if (anchor.target === '_blank') {
        return null;
      }
      if ((anchor.target === '_top' || anchor.target === '_parent') && window.top !== window) {
        return null;
      }
      var href = anchor.href;
      var url;
      if (document.baseURI != null) {
        url = new URL(href, document.baseURI);
      } else {
        url = new URL(href);
      }
      var origin;
      if (window.location.origin) {
        origin = window.location.origin;
      } else {
        origin = window.location.protocol + '//' + window.location.hostname;
        if (window.location.port) {
          origin += ':' + window.location.port;
        }
      }
      if (url.origin !== origin) {
        return null;
      }
      var normalizedHref = url.pathname + url.search + url.hash;
      if (this._urlSpaceRegExp && !this._urlSpaceRegExp.test(normalizedHref)) {
        return null;
      }
      var fullNormalizedHref = new URL(normalizedHref, window.location.href).href;
      return fullNormalizedHref;
    },
    _makeRegExp: function(urlSpaceRegex) {
      return RegExp(urlSpaceRegex);
    }
  });
})();

'use strict';

Polymer({
  is: 'iron-query-params',
  properties: {
    paramsString: {
      type: String,
      notify: true,
      observer: 'paramsStringChanged'
    },
    paramsObject: {
      type: Object,
      notify: true,
      value: function() {
        return {};
      }
    },
    _dontReact: {
      type: Boolean,
      value: false
    }
  },
  hostAttributes: {
    hidden: true
  },
  observers: [ 'paramsObjectChanged(paramsObject.*)' ],
  paramsStringChanged: function() {
    this._dontReact = true;
    this.paramsObject = this._decodeParams(this.paramsString);
    this._dontReact = false;
  },
  paramsObjectChanged: function() {
    if (this._dontReact) {
      return;
    }
    this.paramsString = this._encodeParams(this.paramsObject);
  },
  _encodeParams: function(params) {
    var encodedParams = [];
    for (var key in params) {
      var value = params[key];
      if (value === '') {
        encodedParams.push(encodeURIComponent(key));
      } else if (value) {
        encodedParams.push(encodeURIComponent(key) + '=' + encodeURIComponent(value.toString()));
      }
    }
    return encodedParams.join('&');
  },
  _decodeParams: function(paramString) {
    var params = {};
    paramString = (paramString || '').replace(/\+/g, '%20');
    var paramList = paramString.split('&');
    for (var i = 0; i < paramList.length; i++) {
      var param = paramList[i].split('=');
      if (param[0]) {
        params[decodeURIComponent(param[0])] = decodeURIComponent(param[1] || '');
      }
    }
    return params;
  }
});

'use strict';

Polymer.AppRouteConverterBehavior = {
  properties: {
    route: {
      type: Object,
      notify: true
    },
    queryParams: {
      type: Object,
      notify: true
    },
    path: {
      type: String,
      notify: true
    }
  },
  observers: [ '_locationChanged(path, queryParams)', '_routeChanged(route.prefix, route.path)', '_routeQueryParamsChanged(route.__queryParams)' ],
  created: function() {
    this.linkPaths('route.__queryParams', 'queryParams');
    this.linkPaths('queryParams', 'route.__queryParams');
  },
  _locationChanged: function() {
    if (this.route && this.route.path === this.path && this.queryParams === this.route.__queryParams) {
      return;
    }
    this.route = {
      prefix: '',
      path: this.path,
      __queryParams: this.queryParams
    };
  },
  _routeChanged: function() {
    if (!this.route) {
      return;
    }
    this.path = this.route.prefix + this.route.path;
  },
  _routeQueryParamsChanged: function(queryParams) {
    if (!this.route) {
      return;
    }
    this.queryParams = queryParams;
  }
};

'use strict';

Polymer({
  is: 'app-location',
  properties: {
    route: {
      type: Object,
      notify: true
    },
    useHashAsPath: {
      type: Boolean,
      value: false
    },
    urlSpaceRegex: {
      type: String,
      notify: true
    },
    __queryParams: {
      type: Object
    },
    __path: {
      type: String
    },
    __query: {
      type: String
    },
    __hash: {
      type: String
    },
    path: {
      type: String,
      observer: '__onPathChanged'
    }
  },
  behaviors: [ Polymer.AppRouteConverterBehavior ],
  observers: [ '__computeRoutePath(useHashAsPath, __hash, __path)' ],
  __computeRoutePath: function() {
    this.path = this.useHashAsPath ? this.__hash : this.__path;
  },
  __onPathChanged: function() {
    if (!this._readied) {
      return;
    }
    if (this.useHashAsPath) {
      this.__hash = this.path;
    } else {
      this.__path = this.path;
    }
  }
});

'use strict';

Polymer({
  is: 'app-route',
  properties: {
    route: {
      type: Object,
      notify: true
    },
    pattern: {
      type: String
    },
    data: {
      type: Object,
      value: function() {
        return {};
      },
      notify: true
    },
    queryParams: {
      type: Object,
      value: function() {
        return {};
      },
      notify: true
    },
    tail: {
      type: Object,
      value: function() {
        return {
          path: null,
          prefix: null,
          __queryParams: null
        };
      },
      notify: true
    },
    active: {
      type: Boolean,
      notify: true,
      readOnly: true
    },
    _queryParamsUpdating: {
      type: Boolean,
      value: false
    },
    _matched: {
      type: String,
      value: ''
    }
  },
  observers: [ '__tryToMatch(route.path, pattern)', '__updatePathOnDataChange(data.*)', '__tailPathChanged(tail.path)', '__routeQueryParamsChanged(route.__queryParams)', '__tailQueryParamsChanged(tail.__queryParams)', '__queryParamsChanged(queryParams.*)' ],
  created: function() {
    this.linkPaths('route.__queryParams', 'tail.__queryParams');
    this.linkPaths('tail.__queryParams', 'route.__queryParams');
  },
  __routeQueryParamsChanged: function(queryParams) {
    if (queryParams && this.tail) {
      this.set('tail.__queryParams', queryParams);
      if (!this.active || this._queryParamsUpdating) {
        return;
      }
      var copyOfQueryParams = {};
      var anythingChanged = false;
      for (var key in queryParams) {
        copyOfQueryParams[key] = queryParams[key];
        if (anythingChanged || !this.queryParams || queryParams[key] !== this.queryParams[key]) {
          anythingChanged = true;
        }
      }
      for (var key in this.queryParams) {
        if (anythingChanged || !(key in queryParams)) {
          anythingChanged = true;
          break;
        }
      }
      if (!anythingChanged) {
        return;
      }
      this._queryParamsUpdating = true;
      this.set('queryParams', copyOfQueryParams);
      this._queryParamsUpdating = false;
    }
  },
  __tailQueryParamsChanged: function(queryParams) {
    if (queryParams && this.route) {
      this.set('route.__queryParams', queryParams);
    }
  },
  __queryParamsChanged: function(changes) {
    if (!this.active || this._queryParamsUpdating) {
      return;
    }
    this.set('route.__' + changes.path, changes.value);
  },
  __resetProperties: function() {
    this._setActive(false);
    this._matched = null;
  },
  __tryToMatch: function() {
    if (!this.route) {
      return;
    }
    var path = this.route.path;
    var pattern = this.pattern;
    if (!pattern) {
      return;
    }
    if (!path) {
      this.__resetProperties();
      return;
    }
    var remainingPieces = path.split('/');
    var patternPieces = pattern.split('/');
    var matched = [];
    var namedMatches = {};
    for (var i = 0; i < patternPieces.length; i++) {
      var patternPiece = patternPieces[i];
      if (!patternPiece && patternPiece !== '') {
        break;
      }
      var pathPiece = remainingPieces.shift();
      if (!pathPiece && pathPiece !== '') {
        this.__resetProperties();
        return;
      }
      matched.push(pathPiece);
      if (patternPiece.charAt(0) == ':') {
        namedMatches[patternPiece.slice(1)] = pathPiece;
      } else if (patternPiece !== pathPiece) {
        this.__resetProperties();
        return;
      }
    }
    this._matched = matched.join('/');
    var propertyUpdates = {};
    if (!this.active) {
      propertyUpdates.active = true;
    }
    var tailPrefix = this.route.prefix + this._matched;
    var tailPath = remainingPieces.join('/');
    if (remainingPieces.length > 0) {
      tailPath = '/' + tailPath;
    }
    if (!this.tail || this.tail.prefix !== tailPrefix || this.tail.path !== tailPath) {
      propertyUpdates.tail = {
        prefix: tailPrefix,
        path: tailPath,
        __queryParams: this.route.__queryParams
      };
    }
    propertyUpdates.data = namedMatches;
    this._dataInUrl = {};
    for (var key in namedMatches) {
      this._dataInUrl[key] = namedMatches[key];
    }
    this.__setMulti(propertyUpdates);
  },
  __tailPathChanged: function() {
    if (!this.active) {
      return;
    }
    var tailPath = this.tail.path;
    var newPath = this._matched;
    if (tailPath) {
      if (tailPath.charAt(0) !== '/') {
        tailPath = '/' + tailPath;
      }
      newPath += tailPath;
    }
    this.set('route.path', newPath);
  },
  __updatePathOnDataChange: function() {
    if (!this.route || !this.active) {
      return;
    }
    var newPath = this.__getLink({});
    var oldPath = this.__getLink(this._dataInUrl);
    if (newPath === oldPath) {
      return;
    }
    this.set('route.path', newPath);
  },
  __getLink: function(overrideValues) {
    var values = {
      tail: null
    };
    for (var key in this.data) {
      values[key] = this.data[key];
    }
    for (var key in overrideValues) {
      values[key] = overrideValues[key];
    }
    var patternPieces = this.pattern.split('/');
    var interp = patternPieces.map(function(value) {
      if (value[0] == ':') {
        value = values[value.slice(1)];
      }
      return value;
    }, this);
    if (values.tail && values.tail.path) {
      if (interp.length > 0 && values.tail.path.charAt(0) === '/') {
        interp.push(values.tail.path.slice(1));
      } else {
        interp.push(values.tail.path);
      }
    }
    return interp.join('/');
  },
  __setMulti: function(setObj) {
    for (var property in setObj) {
      this._propertySetter(property, setObj[property]);
    }
    for (var property in setObj) {
      this._pathEffector(property, this[property]);
      this._notifyPathUp(property, this[property]);
    }
  }
});

Polymer({
  is: 'iron-media-query',
  properties: {
    queryMatches: {
      type: Boolean,
      value: false,
      readOnly: true,
      notify: true
    },
    query: {
      type: String,
      observer: 'queryChanged'
    },
    full: {
      type: Boolean,
      value: false
    },
    _boundMQHandler: {
      value: function() {
        return this.queryHandler.bind(this);
      }
    },
    _mq: {
      value: null
    }
  },
  attached: function() {
    this.style.display = 'none';
    this.queryChanged();
  },
  detached: function() {
    this._remove();
  },
  _add: function() {
    if (this._mq) {
      this._mq.addListener(this._boundMQHandler);
    }
  },
  _remove: function() {
    if (this._mq) {
      this._mq.removeListener(this._boundMQHandler);
    }
    this._mq = null;
  },
  queryChanged: function() {
    this._remove();
    var query = this.query;
    if (!query) {
      return;
    }
    if (!this.full && query[0] !== '(') {
      query = '(' + query + ')';
    }
    this._mq = window.matchMedia(query);
    this._add();
    this.queryHandler(this._mq);
  },
  queryHandler: function(mq) {
    this._setQueryMatches(mq.matches);
  }
});

Polymer.IronResizableBehavior = {
  properties: {
    _parentResizable: {
      type: Object,
      observer: '_parentResizableChanged'
    },
    _notifyingDescendant: {
      type: Boolean,
      value: false
    }
  },
  listeners: {
    'iron-request-resize-notifications': '_onIronRequestResizeNotifications'
  },
  created: function() {
    this._interestedResizables = [];
    this._boundNotifyResize = this.notifyResize.bind(this);
  },
  attached: function() {
    this.fire('iron-request-resize-notifications', null, {
      node: this,
      bubbles: true,
      cancelable: true
    });
    if (!this._parentResizable) {
      window.addEventListener('resize', this._boundNotifyResize);
      this.notifyResize();
    }
  },
  detached: function() {
    if (this._parentResizable) {
      this._parentResizable.stopResizeNotificationsFor(this);
    } else {
      window.removeEventListener('resize', this._boundNotifyResize);
    }
    this._parentResizable = null;
  },
  notifyResize: function() {
    if (!this.isAttached) {
      return;
    }
    this._interestedResizables.forEach(function(resizable) {
      if (this.resizerShouldNotify(resizable)) {
        this._notifyDescendant(resizable);
      }
    }, this);
    this._fireResize();
  },
  assignParentResizable: function(parentResizable) {
    this._parentResizable = parentResizable;
  },
  stopResizeNotificationsFor: function(target) {
    var index = this._interestedResizables.indexOf(target);
    if (index > -1) {
      this._interestedResizables.splice(index, 1);
      this.unlisten(target, 'iron-resize', '_onDescendantIronResize');
    }
  },
  resizerShouldNotify: function(element) {
    return true;
  },
  _onDescendantIronResize: function(event) {
    if (this._notifyingDescendant) {
      event.stopPropagation();
      return;
    }
    if (!Polymer.Settings.useShadow) {
      this._fireResize();
    }
  },
  _fireResize: function() {
    this.fire('iron-resize', null, {
      node: this,
      bubbles: false
    });
  },
  _onIronRequestResizeNotifications: function(event) {
    var target = event.path ? event.path[0] : event.target;
    if (target === this) {
      return;
    }
    if (this._interestedResizables.indexOf(target) === -1) {
      this._interestedResizables.push(target);
      this.listen(target, 'iron-resize', '_onDescendantIronResize');
    }
    target.assignParentResizable(this);
    this._notifyDescendant(target);
    event.stopPropagation();
  },
  _parentResizableChanged: function(parentResizable) {
    if (parentResizable) {
      window.removeEventListener('resize', this._boundNotifyResize);
    }
  },
  _notifyDescendant: function(descendant) {
    if (!this.isAttached) {
      return;
    }
    this._notifyingDescendant = true;
    descendant.notifyResize();
    this._notifyingDescendant = false;
  }
};

Polymer.IronSelection = function(selectCallback) {
  this.selection = [];
  this.selectCallback = selectCallback;
};

Polymer.IronSelection.prototype = {
  get: function() {
    return this.multi ? this.selection.slice() : this.selection[0];
  },
  clear: function(excludes) {
    this.selection.slice().forEach(function(item) {
      if (!excludes || excludes.indexOf(item) < 0) {
        this.setItemSelected(item, false);
      }
    }, this);
  },
  isSelected: function(item) {
    return this.selection.indexOf(item) >= 0;
  },
  setItemSelected: function(item, isSelected) {
    if (item != null) {
      if (isSelected !== this.isSelected(item)) {
        if (isSelected) {
          this.selection.push(item);
        } else {
          var i = this.selection.indexOf(item);
          if (i >= 0) {
            this.selection.splice(i, 1);
          }
        }
        if (this.selectCallback) {
          this.selectCallback(item, isSelected);
        }
      }
    }
  },
  select: function(item) {
    if (this.multi) {
      this.toggle(item);
    } else if (this.get() !== item) {
      this.setItemSelected(this.get(), false);
      this.setItemSelected(item, true);
    }
  },
  toggle: function(item) {
    this.setItemSelected(item, !this.isSelected(item));
  }
};

Polymer.IronSelectableBehavior = {
  properties: {
    attrForSelected: {
      type: String,
      value: null
    },
    selected: {
      type: String,
      notify: true
    },
    selectedItem: {
      type: Object,
      readOnly: true,
      notify: true
    },
    activateEvent: {
      type: String,
      value: 'tap',
      observer: '_activateEventChanged'
    },
    selectable: String,
    selectedClass: {
      type: String,
      value: 'iron-selected'
    },
    selectedAttribute: {
      type: String,
      value: null
    },
    fallbackSelection: {
      type: String,
      value: null
    },
    items: {
      type: Array,
      readOnly: true,
      notify: true,
      value: function() {
        return [];
      }
    },
    _excludedLocalNames: {
      type: Object,
      value: function() {
        return {
          template: 1
        };
      }
    }
  },
  observers: [ '_updateAttrForSelected(attrForSelected)', '_updateSelected(selected)', '_checkFallback(fallbackSelection)' ],
  created: function() {
    this._bindFilterItem = this._filterItem.bind(this);
    this._selection = new Polymer.IronSelection(this._applySelection.bind(this));
  },
  attached: function() {
    this._observer = this._observeItems(this);
    this._updateItems();
    if (!this._shouldUpdateSelection) {
      this._updateSelected();
    }
    this._addListener(this.activateEvent);
  },
  detached: function() {
    if (this._observer) {
      Polymer.dom(this).unobserveNodes(this._observer);
    }
    this._removeListener(this.activateEvent);
  },
  indexOf: function(item) {
    return this.items.indexOf(item);
  },
  select: function(value) {
    this.selected = value;
  },
  selectPrevious: function() {
    var length = this.items.length;
    var index = (Number(this._valueToIndex(this.selected)) - 1 + length) % length;
    this.selected = this._indexToValue(index);
  },
  selectNext: function() {
    var index = (Number(this._valueToIndex(this.selected)) + 1) % this.items.length;
    this.selected = this._indexToValue(index);
  },
  selectIndex: function(index) {
    this.select(this._indexToValue(index));
  },
  forceSynchronousItemUpdate: function() {
    this._updateItems();
  },
  get _shouldUpdateSelection() {
    return this.selected != null;
  },
  _checkFallback: function() {
    if (this._shouldUpdateSelection) {
      this._updateSelected();
    }
  },
  _addListener: function(eventName) {
    this.listen(this, eventName, '_activateHandler');
  },
  _removeListener: function(eventName) {
    this.unlisten(this, eventName, '_activateHandler');
  },
  _activateEventChanged: function(eventName, old) {
    this._removeListener(old);
    this._addListener(eventName);
  },
  _updateItems: function() {
    var nodes = Polymer.dom(this).queryDistributedElements(this.selectable || '*');
    nodes = Array.prototype.filter.call(nodes, this._bindFilterItem);
    this._setItems(nodes);
  },
  _updateAttrForSelected: function() {
    if (this._shouldUpdateSelection) {
      this.selected = this._indexToValue(this.indexOf(this.selectedItem));
    }
  },
  _updateSelected: function() {
    this._selectSelected(this.selected);
  },
  _selectSelected: function(selected) {
    this._selection.select(this._valueToItem(this.selected));
    if (this.fallbackSelection && this.items.length && this._selection.get() === undefined) {
      this.selected = this.fallbackSelection;
    }
  },
  _filterItem: function(node) {
    return !this._excludedLocalNames[node.localName];
  },
  _valueToItem: function(value) {
    return value == null ? null : this.items[this._valueToIndex(value)];
  },
  _valueToIndex: function(value) {
    if (this.attrForSelected) {
      for (var i = 0, item; item = this.items[i]; i++) {
        if (this._valueForItem(item) == value) {
          return i;
        }
      }
    } else {
      return Number(value);
    }
  },
  _indexToValue: function(index) {
    if (this.attrForSelected) {
      var item = this.items[index];
      if (item) {
        return this._valueForItem(item);
      }
    } else {
      return index;
    }
  },
  _valueForItem: function(item) {
    var propValue = item[Polymer.CaseMap.dashToCamelCase(this.attrForSelected)];
    return propValue != undefined ? propValue : item.getAttribute(this.attrForSelected);
  },
  _applySelection: function(item, isSelected) {
    if (this.selectedClass) {
      this.toggleClass(this.selectedClass, isSelected, item);
    }
    if (this.selectedAttribute) {
      this.toggleAttribute(this.selectedAttribute, isSelected, item);
    }
    this._selectionChange();
    this.fire('iron-' + (isSelected ? 'select' : 'deselect'), {
      item: item
    });
  },
  _selectionChange: function() {
    this._setSelectedItem(this._selection.get());
  },
  _observeItems: function(node) {
    return Polymer.dom(node).observeNodes(function(mutation) {
      this._updateItems();
      if (this._shouldUpdateSelection) {
        this._updateSelected();
      }
      this.fire('iron-items-changed', mutation, {
        bubbles: false,
        cancelable: false
      });
    });
  },
  _activateHandler: function(e) {
    var t = e.target;
    var items = this.items;
    while (t && t != this) {
      var i = items.indexOf(t);
      if (i >= 0) {
        var value = this._indexToValue(i);
        this._itemActivate(value, t);
        return;
      }
      t = t.parentNode;
    }
  },
  _itemActivate: function(value, item) {
    if (!this.fire('iron-activate', {
      selected: value,
      item: item
    }, {
      cancelable: true
    }).defaultPrevented) {
      this.select(value);
    }
  }
};

Polymer({
  is: 'iron-pages',
  behaviors: [ Polymer.IronResizableBehavior, Polymer.IronSelectableBehavior ],
  properties: {
    activateEvent: {
      type: String,
      value: null
    }
  },
  observers: [ '_selectedPageChanged(selected)' ],
  _selectedPageChanged: function(selected, old) {
    this.async(this.notifyResize);
  }
});

Polymer.IronScrollTargetBehavior = {
  properties: {
    scrollTarget: {
      type: HTMLElement,
      value: function() {
        return this._defaultScrollTarget;
      }
    }
  },
  observers: [ '_scrollTargetChanged(scrollTarget, isAttached)' ],
  _scrollTargetChanged: function(scrollTarget, isAttached) {
    var eventTarget;
    if (this._oldScrollTarget) {
      eventTarget = this._oldScrollTarget === this._doc ? window : this._oldScrollTarget;
      eventTarget.removeEventListener('scroll', this._boundScrollHandler);
      this._oldScrollTarget = null;
    }
    if (!isAttached) {
      return;
    }
    if (scrollTarget === 'document') {
      this.scrollTarget = this._doc;
    } else if (typeof scrollTarget === 'string') {
      this.scrollTarget = this.domHost ? this.domHost.$[scrollTarget] : Polymer.dom(this.ownerDocument).querySelector('#' + scrollTarget);
    } else if (this._isValidScrollTarget()) {
      eventTarget = scrollTarget === this._doc ? window : scrollTarget;
      this._boundScrollHandler = this._boundScrollHandler || this._scrollHandler.bind(this);
      this._oldScrollTarget = scrollTarget;
      eventTarget.addEventListener('scroll', this._boundScrollHandler);
    }
  },
  _scrollHandler: function scrollHandler() {},
  get _defaultScrollTarget() {
    return this._doc;
  },
  get _doc() {
    return this.ownerDocument.documentElement;
  },
  get _scrollTop() {
    if (this._isValidScrollTarget()) {
      return this.scrollTarget === this._doc ? window.pageYOffset : this.scrollTarget.scrollTop;
    }
    return 0;
  },
  get _scrollLeft() {
    if (this._isValidScrollTarget()) {
      return this.scrollTarget === this._doc ? window.pageXOffset : this.scrollTarget.scrollLeft;
    }
    return 0;
  },
  set _scrollTop(top) {
    if (this.scrollTarget === this._doc) {
      window.scrollTo(window.pageXOffset, top);
    } else if (this._isValidScrollTarget()) {
      this.scrollTarget.scrollTop = top;
    }
  },
  set _scrollLeft(left) {
    if (this.scrollTarget === this._doc) {
      window.scrollTo(left, window.pageYOffset);
    } else if (this._isValidScrollTarget()) {
      this.scrollTarget.scrollLeft = left;
    }
  },
  scroll: function(left, top) {
    if (this.scrollTarget === this._doc) {
      window.scrollTo(left, top);
    } else if (this._isValidScrollTarget()) {
      this.scrollTarget.scrollLeft = left;
      this.scrollTarget.scrollTop = top;
    }
  },
  get _scrollTargetWidth() {
    if (this._isValidScrollTarget()) {
      return this.scrollTarget === this._doc ? window.innerWidth : this.scrollTarget.offsetWidth;
    }
    return 0;
  },
  get _scrollTargetHeight() {
    if (this._isValidScrollTarget()) {
      return this.scrollTarget === this._doc ? window.innerHeight : this.scrollTarget.offsetHeight;
    }
    return 0;
  },
  _isValidScrollTarget: function() {
    return this.scrollTarget instanceof HTMLElement;
  }
};

(function() {
  'use strict';
  var KEY_IDENTIFIER = {
    'U+0008': 'backspace',
    'U+0009': 'tab',
    'U+001B': 'esc',
    'U+0020': 'space',
    'U+007F': 'del'
  };
  var KEY_CODE = {
    8: 'backspace',
    9: 'tab',
    13: 'enter',
    27: 'esc',
    33: 'pageup',
    34: 'pagedown',
    35: 'end',
    36: 'home',
    32: 'space',
    37: 'left',
    38: 'up',
    39: 'right',
    40: 'down',
    46: 'del',
    106: '*'
  };
  var MODIFIER_KEYS = {
    shift: 'shiftKey',
    ctrl: 'ctrlKey',
    alt: 'altKey',
    meta: 'metaKey'
  };
  var KEY_CHAR = /[a-z0-9*]/;
  var IDENT_CHAR = /U\+/;
  var ARROW_KEY = /^arrow/;
  var SPACE_KEY = /^space(bar)?/;
  var ESC_KEY = /^escape$/;
  function transformKey(key, noSpecialChars) {
    var validKey = '';
    if (key) {
      var lKey = key.toLowerCase();
      if (lKey === ' ' || SPACE_KEY.test(lKey)) {
        validKey = 'space';
      } else if (ESC_KEY.test(lKey)) {
        validKey = 'esc';
      } else if (lKey.length == 1) {
        if (!noSpecialChars || KEY_CHAR.test(lKey)) {
          validKey = lKey;
        }
      } else if (ARROW_KEY.test(lKey)) {
        validKey = lKey.replace('arrow', '');
      } else if (lKey == 'multiply') {
        validKey = '*';
      } else {
        validKey = lKey;
      }
    }
    return validKey;
  }
  function transformKeyIdentifier(keyIdent) {
    var validKey = '';
    if (keyIdent) {
      if (keyIdent in KEY_IDENTIFIER) {
        validKey = KEY_IDENTIFIER[keyIdent];
      } else if (IDENT_CHAR.test(keyIdent)) {
        keyIdent = parseInt(keyIdent.replace('U+', '0x'), 16);
        validKey = String.fromCharCode(keyIdent).toLowerCase();
      } else {
        validKey = keyIdent.toLowerCase();
      }
    }
    return validKey;
  }
  function transformKeyCode(keyCode) {
    var validKey = '';
    if (Number(keyCode)) {
      if (keyCode >= 65 && keyCode <= 90) {
        validKey = String.fromCharCode(32 + keyCode);
      } else if (keyCode >= 112 && keyCode <= 123) {
        validKey = 'f' + (keyCode - 112);
      } else if (keyCode >= 48 && keyCode <= 57) {
        validKey = String(keyCode - 48);
      } else if (keyCode >= 96 && keyCode <= 105) {
        validKey = String(keyCode - 96);
      } else {
        validKey = KEY_CODE[keyCode];
      }
    }
    return validKey;
  }
  function normalizedKeyForEvent(keyEvent, noSpecialChars) {
    return transformKey(keyEvent.key, noSpecialChars) || transformKeyIdentifier(keyEvent.keyIdentifier) || transformKeyCode(keyEvent.keyCode) || transformKey(keyEvent.detail ? keyEvent.detail.key : keyEvent.detail, noSpecialChars) || '';
  }
  function keyComboMatchesEvent(keyCombo, event) {
    var keyEvent = normalizedKeyForEvent(event, keyCombo.hasModifiers);
    return keyEvent === keyCombo.key && (!keyCombo.hasModifiers || !!event.shiftKey === !!keyCombo.shiftKey && !!event.ctrlKey === !!keyCombo.ctrlKey && !!event.altKey === !!keyCombo.altKey && !!event.metaKey === !!keyCombo.metaKey);
  }
  function parseKeyComboString(keyComboString) {
    if (keyComboString.length === 1) {
      return {
        combo: keyComboString,
        key: keyComboString,
        event: 'keydown'
      };
    }
    return keyComboString.split('+').reduce(function(parsedKeyCombo, keyComboPart) {
      var eventParts = keyComboPart.split(':');
      var keyName = eventParts[0];
      var event = eventParts[1];
      if (keyName in MODIFIER_KEYS) {
        parsedKeyCombo[MODIFIER_KEYS[keyName]] = true;
        parsedKeyCombo.hasModifiers = true;
      } else {
        parsedKeyCombo.key = keyName;
        parsedKeyCombo.event = event || 'keydown';
      }
      return parsedKeyCombo;
    }, {
      combo: keyComboString.split(':').shift()
    });
  }
  function parseEventString(eventString) {
    return eventString.trim().split(' ').map(function(keyComboString) {
      return parseKeyComboString(keyComboString);
    });
  }
  Polymer.IronA11yKeysBehavior = {
    properties: {
      keyEventTarget: {
        type: Object,
        value: function() {
          return this;
        }
      },
      stopKeyboardEventPropagation: {
        type: Boolean,
        value: false
      },
      _boundKeyHandlers: {
        type: Array,
        value: function() {
          return [];
        }
      },
      _imperativeKeyBindings: {
        type: Object,
        value: function() {
          return {};
        }
      }
    },
    observers: [ '_resetKeyEventListeners(keyEventTarget, _boundKeyHandlers)' ],
    keyBindings: {},
    registered: function() {
      this._prepKeyBindings();
    },
    attached: function() {
      this._listenKeyEventListeners();
    },
    detached: function() {
      this._unlistenKeyEventListeners();
    },
    addOwnKeyBinding: function(eventString, handlerName) {
      this._imperativeKeyBindings[eventString] = handlerName;
      this._prepKeyBindings();
      this._resetKeyEventListeners();
    },
    removeOwnKeyBindings: function() {
      this._imperativeKeyBindings = {};
      this._prepKeyBindings();
      this._resetKeyEventListeners();
    },
    keyboardEventMatchesKeys: function(event, eventString) {
      var keyCombos = parseEventString(eventString);
      for (var i = 0; i < keyCombos.length; ++i) {
        if (keyComboMatchesEvent(keyCombos[i], event)) {
          return true;
        }
      }
      return false;
    },
    _collectKeyBindings: function() {
      var keyBindings = this.behaviors.map(function(behavior) {
        return behavior.keyBindings;
      });
      if (keyBindings.indexOf(this.keyBindings) === -1) {
        keyBindings.push(this.keyBindings);
      }
      return keyBindings;
    },
    _prepKeyBindings: function() {
      this._keyBindings = {};
      this._collectKeyBindings().forEach(function(keyBindings) {
        for (var eventString in keyBindings) {
          this._addKeyBinding(eventString, keyBindings[eventString]);
        }
      }, this);
      for (var eventString in this._imperativeKeyBindings) {
        this._addKeyBinding(eventString, this._imperativeKeyBindings[eventString]);
      }
      for (var eventName in this._keyBindings) {
        this._keyBindings[eventName].sort(function(kb1, kb2) {
          var b1 = kb1[0].hasModifiers;
          var b2 = kb2[0].hasModifiers;
          return b1 === b2 ? 0 : b1 ? -1 : 1;
        });
      }
    },
    _addKeyBinding: function(eventString, handlerName) {
      parseEventString(eventString).forEach(function(keyCombo) {
        this._keyBindings[keyCombo.event] = this._keyBindings[keyCombo.event] || [];
        this._keyBindings[keyCombo.event].push([ keyCombo, handlerName ]);
      }, this);
    },
    _resetKeyEventListeners: function() {
      this._unlistenKeyEventListeners();
      if (this.isAttached) {
        this._listenKeyEventListeners();
      }
    },
    _listenKeyEventListeners: function() {
      if (!this.keyEventTarget) {
        return;
      }
      Object.keys(this._keyBindings).forEach(function(eventName) {
        var keyBindings = this._keyBindings[eventName];
        var boundKeyHandler = this._onKeyBindingEvent.bind(this, keyBindings);
        this._boundKeyHandlers.push([ this.keyEventTarget, eventName, boundKeyHandler ]);
        this.keyEventTarget.addEventListener(eventName, boundKeyHandler);
      }, this);
    },
    _unlistenKeyEventListeners: function() {
      var keyHandlerTuple;
      var keyEventTarget;
      var eventName;
      var boundKeyHandler;
      while (this._boundKeyHandlers.length) {
        keyHandlerTuple = this._boundKeyHandlers.pop();
        keyEventTarget = keyHandlerTuple[0];
        eventName = keyHandlerTuple[1];
        boundKeyHandler = keyHandlerTuple[2];
        keyEventTarget.removeEventListener(eventName, boundKeyHandler);
      }
    },
    _onKeyBindingEvent: function(keyBindings, event) {
      if (this.stopKeyboardEventPropagation) {
        event.stopPropagation();
      }
      if (event.defaultPrevented) {
        return;
      }
      for (var i = 0; i < keyBindings.length; i++) {
        var keyCombo = keyBindings[i][0];
        var handlerName = keyBindings[i][1];
        if (keyComboMatchesEvent(keyCombo, event)) {
          this._triggerKeyHandler(keyCombo, handlerName, event);
          if (event.defaultPrevented) {
            return;
          }
        }
      }
    },
    _triggerKeyHandler: function(keyCombo, handlerName, keyboardEvent) {
      var detail = Object.create(keyCombo);
      detail.keyboardEvent = keyboardEvent;
      var event = new CustomEvent(keyCombo.event, {
        detail: detail,
        cancelable: true
      });
      this[handlerName].call(this, event);
      if (event.defaultPrevented) {
        keyboardEvent.preventDefault();
      }
    }
  };
})();

Polymer.IronControlState = {
  properties: {
    focused: {
      type: Boolean,
      value: false,
      notify: true,
      readOnly: true,
      reflectToAttribute: true
    },
    disabled: {
      type: Boolean,
      value: false,
      notify: true,
      observer: '_disabledChanged',
      reflectToAttribute: true
    },
    _oldTabIndex: {
      type: Number
    },
    _boundFocusBlurHandler: {
      type: Function,
      value: function() {
        return this._focusBlurHandler.bind(this);
      }
    }
  },
  observers: [ '_changedControlState(focused, disabled)' ],
  ready: function() {
    this.addEventListener('focus', this._boundFocusBlurHandler, true);
    this.addEventListener('blur', this._boundFocusBlurHandler, true);
  },
  _focusBlurHandler: function(event) {
    if (event.target === this) {
      this._setFocused(event.type === 'focus');
    } else if (!this.shadowRoot) {
      var target = Polymer.dom(event).localTarget;
      if (!this.isLightDescendant(target)) {
        this.fire(event.type, {
          sourceEvent: event
        }, {
          node: this,
          bubbles: event.bubbles,
          cancelable: event.cancelable
        });
      }
    }
  },
  _disabledChanged: function(disabled, old) {
    this.setAttribute('aria-disabled', disabled ? 'true' : 'false');
    this.style.pointerEvents = disabled ? 'none' : '';
    if (disabled) {
      this._oldTabIndex = this.tabIndex;
      this._setFocused(false);
      this.tabIndex = -1;
      this.blur();
    } else if (this._oldTabIndex !== undefined) {
      this.tabIndex = this._oldTabIndex;
    }
  },
  _changedControlState: function() {
    if (this._controlStateChanged) {
      this._controlStateChanged();
    }
  }
};

Polymer.IronButtonStateImpl = {
  properties: {
    pressed: {
      type: Boolean,
      readOnly: true,
      value: false,
      reflectToAttribute: true,
      observer: '_pressedChanged'
    },
    toggles: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },
    active: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true
    },
    pointerDown: {
      type: Boolean,
      readOnly: true,
      value: false
    },
    receivedFocusFromKeyboard: {
      type: Boolean,
      readOnly: true
    },
    ariaActiveAttribute: {
      type: String,
      value: 'aria-pressed',
      observer: '_ariaActiveAttributeChanged'
    }
  },
  listeners: {
    down: '_downHandler',
    up: '_upHandler',
    tap: '_tapHandler'
  },
  observers: [ '_detectKeyboardFocus(focused)', '_activeChanged(active, ariaActiveAttribute)' ],
  keyBindings: {
    'enter:keydown': '_asyncClick',
    'space:keydown': '_spaceKeyDownHandler',
    'space:keyup': '_spaceKeyUpHandler'
  },
  _mouseEventRe: /^mouse/,
  _tapHandler: function() {
    if (this.toggles) {
      this._userActivate(!this.active);
    } else {
      this.active = false;
    }
  },
  _detectKeyboardFocus: function(focused) {
    this._setReceivedFocusFromKeyboard(!this.pointerDown && focused);
  },
  _userActivate: function(active) {
    if (this.active !== active) {
      this.active = active;
      this.fire('change');
    }
  },
  _downHandler: function(event) {
    this._setPointerDown(true);
    this._setPressed(true);
    this._setReceivedFocusFromKeyboard(false);
  },
  _upHandler: function() {
    this._setPointerDown(false);
    this._setPressed(false);
  },
  _spaceKeyDownHandler: function(event) {
    var keyboardEvent = event.detail.keyboardEvent;
    var target = Polymer.dom(keyboardEvent).localTarget;
    if (this.isLightDescendant(target)) return;
    keyboardEvent.preventDefault();
    keyboardEvent.stopImmediatePropagation();
    this._setPressed(true);
  },
  _spaceKeyUpHandler: function(event) {
    var keyboardEvent = event.detail.keyboardEvent;
    var target = Polymer.dom(keyboardEvent).localTarget;
    if (this.isLightDescendant(target)) return;
    if (this.pressed) {
      this._asyncClick();
    }
    this._setPressed(false);
  },
  _asyncClick: function() {
    this.async(function() {
      this.click();
    }, 1);
  },
  _pressedChanged: function(pressed) {
    this._changedButtonState();
  },
  _ariaActiveAttributeChanged: function(value, oldValue) {
    if (oldValue && oldValue != value && this.hasAttribute(oldValue)) {
      this.removeAttribute(oldValue);
    }
  },
  _activeChanged: function(active, ariaActiveAttribute) {
    if (this.toggles) {
      this.setAttribute(this.ariaActiveAttribute, active ? 'true' : 'false');
    } else {
      this.removeAttribute(this.ariaActiveAttribute);
    }
    this._changedButtonState();
  },
  _controlStateChanged: function() {
    if (this.disabled) {
      this._setPressed(false);
    } else {
      this._changedButtonState();
    }
  },
  _changedButtonState: function() {
    if (this._buttonStateChanged) {
      this._buttonStateChanged();
    }
  }
};

Polymer.IronButtonState = [ Polymer.IronA11yKeysBehavior, Polymer.IronButtonStateImpl ];

(function() {
  var Utility = {
    distance: function(x1, y1, x2, y2) {
      var xDelta = x1 - x2;
      var yDelta = y1 - y2;
      return Math.sqrt(xDelta * xDelta + yDelta * yDelta);
    },
    now: window.performance && window.performance.now ? window.performance.now.bind(window.performance) : Date.now
  };
  function ElementMetrics(element) {
    this.element = element;
    this.width = this.boundingRect.width;
    this.height = this.boundingRect.height;
    this.size = Math.max(this.width, this.height);
  }
  ElementMetrics.prototype = {
    get boundingRect() {
      return this.element.getBoundingClientRect();
    },
    furthestCornerDistanceFrom: function(x, y) {
      var topLeft = Utility.distance(x, y, 0, 0);
      var topRight = Utility.distance(x, y, this.width, 0);
      var bottomLeft = Utility.distance(x, y, 0, this.height);
      var bottomRight = Utility.distance(x, y, this.width, this.height);
      return Math.max(topLeft, topRight, bottomLeft, bottomRight);
    }
  };
  function Ripple(element) {
    this.element = element;
    this.color = window.getComputedStyle(element).color;
    this.wave = document.createElement('div');
    this.waveContainer = document.createElement('div');
    this.wave.style.backgroundColor = this.color;
    this.wave.classList.add('wave');
    this.waveContainer.classList.add('wave-container');
    Polymer.dom(this.waveContainer).appendChild(this.wave);
    this.resetInteractionState();
  }
  Ripple.MAX_RADIUS = 300;
  Ripple.prototype = {
    get recenters() {
      return this.element.recenters;
    },
    get center() {
      return this.element.center;
    },
    get mouseDownElapsed() {
      var elapsed;
      if (!this.mouseDownStart) {
        return 0;
      }
      elapsed = Utility.now() - this.mouseDownStart;
      if (this.mouseUpStart) {
        elapsed -= this.mouseUpElapsed;
      }
      return elapsed;
    },
    get mouseUpElapsed() {
      return this.mouseUpStart ? Utility.now() - this.mouseUpStart : 0;
    },
    get mouseDownElapsedSeconds() {
      return this.mouseDownElapsed / 1e3;
    },
    get mouseUpElapsedSeconds() {
      return this.mouseUpElapsed / 1e3;
    },
    get mouseInteractionSeconds() {
      return this.mouseDownElapsedSeconds + this.mouseUpElapsedSeconds;
    },
    get initialOpacity() {
      return this.element.initialOpacity;
    },
    get opacityDecayVelocity() {
      return this.element.opacityDecayVelocity;
    },
    get radius() {
      var width2 = this.containerMetrics.width * this.containerMetrics.width;
      var height2 = this.containerMetrics.height * this.containerMetrics.height;
      var waveRadius = Math.min(Math.sqrt(width2 + height2), Ripple.MAX_RADIUS) * 1.1 + 5;
      var duration = 1.1 - .2 * (waveRadius / Ripple.MAX_RADIUS);
      var timeNow = this.mouseInteractionSeconds / duration;
      var size = waveRadius * (1 - Math.pow(80, -timeNow));
      return Math.abs(size);
    },
    get opacity() {
      if (!this.mouseUpStart) {
        return this.initialOpacity;
      }
      return Math.max(0, this.initialOpacity - this.mouseUpElapsedSeconds * this.opacityDecayVelocity);
    },
    get outerOpacity() {
      var outerOpacity = this.mouseUpElapsedSeconds * .3;
      var waveOpacity = this.opacity;
      return Math.max(0, Math.min(outerOpacity, waveOpacity));
    },
    get isOpacityFullyDecayed() {
      return this.opacity < .01 && this.radius >= Math.min(this.maxRadius, Ripple.MAX_RADIUS);
    },
    get isRestingAtMaxRadius() {
      return this.opacity >= this.initialOpacity && this.radius >= Math.min(this.maxRadius, Ripple.MAX_RADIUS);
    },
    get isAnimationComplete() {
      return this.mouseUpStart ? this.isOpacityFullyDecayed : this.isRestingAtMaxRadius;
    },
    get translationFraction() {
      return Math.min(1, this.radius / this.containerMetrics.size * 2 / Math.sqrt(2));
    },
    get xNow() {
      if (this.xEnd) {
        return this.xStart + this.translationFraction * (this.xEnd - this.xStart);
      }
      return this.xStart;
    },
    get yNow() {
      if (this.yEnd) {
        return this.yStart + this.translationFraction * (this.yEnd - this.yStart);
      }
      return this.yStart;
    },
    get isMouseDown() {
      return this.mouseDownStart && !this.mouseUpStart;
    },
    resetInteractionState: function() {
      this.maxRadius = 0;
      this.mouseDownStart = 0;
      this.mouseUpStart = 0;
      this.xStart = 0;
      this.yStart = 0;
      this.xEnd = 0;
      this.yEnd = 0;
      this.slideDistance = 0;
      this.containerMetrics = new ElementMetrics(this.element);
    },
    draw: function() {
      var scale;
      var translateString;
      var dx;
      var dy;
      this.wave.style.opacity = this.opacity;
      scale = this.radius / (this.containerMetrics.size / 2);
      dx = this.xNow - this.containerMetrics.width / 2;
      dy = this.yNow - this.containerMetrics.height / 2;
      this.waveContainer.style.webkitTransform = 'translate(' + dx + 'px, ' + dy + 'px)';
      this.waveContainer.style.transform = 'translate3d(' + dx + 'px, ' + dy + 'px, 0)';
      this.wave.style.webkitTransform = 'scale(' + scale + ',' + scale + ')';
      this.wave.style.transform = 'scale3d(' + scale + ',' + scale + ',1)';
    },
    downAction: function(event) {
      var xCenter = this.containerMetrics.width / 2;
      var yCenter = this.containerMetrics.height / 2;
      this.resetInteractionState();
      this.mouseDownStart = Utility.now();
      if (this.center) {
        this.xStart = xCenter;
        this.yStart = yCenter;
        this.slideDistance = Utility.distance(this.xStart, this.yStart, this.xEnd, this.yEnd);
      } else {
        this.xStart = event ? event.detail.x - this.containerMetrics.boundingRect.left : this.containerMetrics.width / 2;
        this.yStart = event ? event.detail.y - this.containerMetrics.boundingRect.top : this.containerMetrics.height / 2;
      }
      if (this.recenters) {
        this.xEnd = xCenter;
        this.yEnd = yCenter;
        this.slideDistance = Utility.distance(this.xStart, this.yStart, this.xEnd, this.yEnd);
      }
      this.maxRadius = this.containerMetrics.furthestCornerDistanceFrom(this.xStart, this.yStart);
      this.waveContainer.style.top = (this.containerMetrics.height - this.containerMetrics.size) / 2 + 'px';
      this.waveContainer.style.left = (this.containerMetrics.width - this.containerMetrics.size) / 2 + 'px';
      this.waveContainer.style.width = this.containerMetrics.size + 'px';
      this.waveContainer.style.height = this.containerMetrics.size + 'px';
    },
    upAction: function(event) {
      if (!this.isMouseDown) {
        return;
      }
      this.mouseUpStart = Utility.now();
    },
    remove: function() {
      Polymer.dom(this.waveContainer.parentNode).removeChild(this.waveContainer);
    }
  };
  Polymer({
    is: 'paper-ripple',
    behaviors: [ Polymer.IronA11yKeysBehavior ],
    properties: {
      initialOpacity: {
        type: Number,
        value: .25
      },
      opacityDecayVelocity: {
        type: Number,
        value: .8
      },
      recenters: {
        type: Boolean,
        value: false
      },
      center: {
        type: Boolean,
        value: false
      },
      ripples: {
        type: Array,
        value: function() {
          return [];
        }
      },
      animating: {
        type: Boolean,
        readOnly: true,
        reflectToAttribute: true,
        value: false
      },
      holdDown: {
        type: Boolean,
        value: false,
        observer: '_holdDownChanged'
      },
      noink: {
        type: Boolean,
        value: false
      },
      _animating: {
        type: Boolean
      },
      _boundAnimate: {
        type: Function,
        value: function() {
          return this.animate.bind(this);
        }
      }
    },
    get target() {
      return this.keyEventTarget;
    },
    keyBindings: {
      'enter:keydown': '_onEnterKeydown',
      'space:keydown': '_onSpaceKeydown',
      'space:keyup': '_onSpaceKeyup'
    },
    attached: function() {
      if (this.parentNode.nodeType == 11) {
        this.keyEventTarget = Polymer.dom(this).getOwnerRoot().host;
      } else {
        this.keyEventTarget = this.parentNode;
      }
      var keyEventTarget = this.keyEventTarget;
      this.listen(keyEventTarget, 'up', 'uiUpAction');
      this.listen(keyEventTarget, 'down', 'uiDownAction');
    },
    detached: function() {
      this.unlisten(this.keyEventTarget, 'up', 'uiUpAction');
      this.unlisten(this.keyEventTarget, 'down', 'uiDownAction');
      this.keyEventTarget = null;
    },
    get shouldKeepAnimating() {
      for (var index = 0; index < this.ripples.length; ++index) {
        if (!this.ripples[index].isAnimationComplete) {
          return true;
        }
      }
      return false;
    },
    simulatedRipple: function() {
      this.downAction(null);
      this.async(function() {
        this.upAction();
      }, 1);
    },
    uiDownAction: function(event) {
      if (!this.noink) {
        this.downAction(event);
      }
    },
    downAction: function(event) {
      if (this.holdDown && this.ripples.length > 0) {
        return;
      }
      var ripple = this.addRipple();
      ripple.downAction(event);
      if (!this._animating) {
        this._animating = true;
        this.animate();
      }
    },
    uiUpAction: function(event) {
      if (!this.noink) {
        this.upAction(event);
      }
    },
    upAction: function(event) {
      if (this.holdDown) {
        return;
      }
      this.ripples.forEach(function(ripple) {
        ripple.upAction(event);
      });
      this._animating = true;
      this.animate();
    },
    onAnimationComplete: function() {
      this._animating = false;
      this.$.background.style.backgroundColor = null;
      this.fire('transitionend');
    },
    addRipple: function() {
      var ripple = new Ripple(this);
      Polymer.dom(this.$.waves).appendChild(ripple.waveContainer);
      this.$.background.style.backgroundColor = ripple.color;
      this.ripples.push(ripple);
      this._setAnimating(true);
      return ripple;
    },
    removeRipple: function(ripple) {
      var rippleIndex = this.ripples.indexOf(ripple);
      if (rippleIndex < 0) {
        return;
      }
      this.ripples.splice(rippleIndex, 1);
      ripple.remove();
      if (!this.ripples.length) {
        this._setAnimating(false);
      }
    },
    animate: function() {
      if (!this._animating) {
        return;
      }
      var index;
      var ripple;
      for (index = 0; index < this.ripples.length; ++index) {
        ripple = this.ripples[index];
        ripple.draw();
        this.$.background.style.opacity = ripple.outerOpacity;
        if (ripple.isOpacityFullyDecayed && !ripple.isRestingAtMaxRadius) {
          this.removeRipple(ripple);
        }
      }
      if (!this.shouldKeepAnimating && this.ripples.length === 0) {
        this.onAnimationComplete();
      } else {
        window.requestAnimationFrame(this._boundAnimate);
      }
    },
    _onEnterKeydown: function() {
      this.uiDownAction();
      this.async(this.uiUpAction, 1);
    },
    _onSpaceKeydown: function() {
      this.uiDownAction();
    },
    _onSpaceKeyup: function() {
      this.uiUpAction();
    },
    _holdDownChanged: function(newVal, oldVal) {
      if (oldVal === undefined) {
        return;
      }
      if (newVal) {
        this.downAction();
      } else {
        this.upAction();
      }
    }
  });
})();

Polymer.PaperRippleBehavior = {
  properties: {
    noink: {
      type: Boolean,
      observer: '_noinkChanged'
    },
    _rippleContainer: {
      type: Object
    }
  },
  _buttonStateChanged: function() {
    if (this.focused) {
      this.ensureRipple();
    }
  },
  _downHandler: function(event) {
    Polymer.IronButtonStateImpl._downHandler.call(this, event);
    if (this.pressed) {
      this.ensureRipple(event);
    }
  },
  ensureRipple: function(optTriggeringEvent) {
    if (!this.hasRipple()) {
      this._ripple = this._createRipple();
      this._ripple.noink = this.noink;
      var rippleContainer = this._rippleContainer || this.root;
      if (rippleContainer) {
        Polymer.dom(rippleContainer).appendChild(this._ripple);
      }
      if (optTriggeringEvent) {
        var domContainer = Polymer.dom(this._rippleContainer || this);
        var target = Polymer.dom(optTriggeringEvent).rootTarget;
        if (domContainer.deepContains(target)) {
          this._ripple.uiDownAction(optTriggeringEvent);
        }
      }
    }
  },
  getRipple: function() {
    this.ensureRipple();
    return this._ripple;
  },
  hasRipple: function() {
    return Boolean(this._ripple);
  },
  _createRipple: function() {
    return document.createElement('paper-ripple');
  },
  _noinkChanged: function(noink) {
    if (this.hasRipple()) {
      this._ripple.noink = noink;
    }
  }
};

Polymer.PaperButtonBehaviorImpl = {
  properties: {
    elevation: {
      type: Number,
      reflectToAttribute: true,
      readOnly: true
    }
  },
  observers: [ '_calculateElevation(focused, disabled, active, pressed, receivedFocusFromKeyboard)', '_computeKeyboardClass(receivedFocusFromKeyboard)' ],
  hostAttributes: {
    role: 'button',
    tabindex: '0',
    animated: true
  },
  _calculateElevation: function() {
    var e = 1;
    if (this.disabled) {
      e = 0;
    } else if (this.active || this.pressed) {
      e = 4;
    } else if (this.receivedFocusFromKeyboard) {
      e = 3;
    }
    this._setElevation(e);
  },
  _computeKeyboardClass: function(receivedFocusFromKeyboard) {
    this.toggleClass('keyboard-focus', receivedFocusFromKeyboard);
  },
  _spaceKeyDownHandler: function(event) {
    Polymer.IronButtonStateImpl._spaceKeyDownHandler.call(this, event);
    if (this.hasRipple() && this.getRipple().ripples.length < 1) {
      this._ripple.uiDownAction();
    }
  },
  _spaceKeyUpHandler: function(event) {
    Polymer.IronButtonStateImpl._spaceKeyUpHandler.call(this, event);
    if (this.hasRipple()) {
      this._ripple.uiUpAction();
    }
  }
};

Polymer.PaperButtonBehavior = [ Polymer.IronButtonState, Polymer.IronControlState, Polymer.PaperRippleBehavior, Polymer.PaperButtonBehaviorImpl ];

Polymer({
  is: 'paper-button',
  behaviors: [ Polymer.PaperButtonBehavior ],
  properties: {
    raised: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      observer: '_calculateElevation'
    }
  },
  _calculateElevation: function() {
    if (!this.raised) {
      this._setElevation(0);
    } else {
      Polymer.PaperButtonBehaviorImpl._calculateElevation.apply(this);
    }
  }
});

(function() {
  var metaDatas = {};
  var metaArrays = {};
  var singleton = null;
  Polymer.IronMeta = Polymer({
    is: 'iron-meta',
    properties: {
      type: {
        type: String,
        value: 'default',
        observer: '_typeChanged'
      },
      key: {
        type: String,
        observer: '_keyChanged'
      },
      value: {
        type: Object,
        notify: true,
        observer: '_valueChanged'
      },
      self: {
        type: Boolean,
        observer: '_selfChanged'
      },
      list: {
        type: Array,
        notify: true
      }
    },
    hostAttributes: {
      hidden: true
    },
    factoryImpl: function(config) {
      if (config) {
        for (var n in config) {
          switch (n) {
           case 'type':
           case 'key':
           case 'value':
            this[n] = config[n];
            break;
          }
        }
      }
    },
    created: function() {
      this._metaDatas = metaDatas;
      this._metaArrays = metaArrays;
    },
    _keyChanged: function(key, old) {
      this._resetRegistration(old);
    },
    _valueChanged: function(value) {
      this._resetRegistration(this.key);
    },
    _selfChanged: function(self) {
      if (self) {
        this.value = this;
      }
    },
    _typeChanged: function(type) {
      this._unregisterKey(this.key);
      if (!metaDatas[type]) {
        metaDatas[type] = {};
      }
      this._metaData = metaDatas[type];
      if (!metaArrays[type]) {
        metaArrays[type] = [];
      }
      this.list = metaArrays[type];
      this._registerKeyValue(this.key, this.value);
    },
    byKey: function(key) {
      return this._metaData && this._metaData[key];
    },
    _resetRegistration: function(oldKey) {
      this._unregisterKey(oldKey);
      this._registerKeyValue(this.key, this.value);
    },
    _unregisterKey: function(key) {
      this._unregister(key, this._metaData, this.list);
    },
    _registerKeyValue: function(key, value) {
      this._register(key, value, this._metaData, this.list);
    },
    _register: function(key, value, data, list) {
      if (key && data && value !== undefined) {
        data[key] = value;
        list.push(value);
      }
    },
    _unregister: function(key, data, list) {
      if (key && data) {
        if (key in data) {
          var value = data[key];
          delete data[key];
          this.arrayDelete(list, value);
        }
      }
    }
  });
  Polymer.IronMeta.getIronMeta = function getIronMeta() {
    if (singleton === null) {
      singleton = new Polymer.IronMeta();
    }
    return singleton;
  };
  Polymer.IronMetaQuery = Polymer({
    is: 'iron-meta-query',
    properties: {
      type: {
        type: String,
        value: 'default',
        observer: '_typeChanged'
      },
      key: {
        type: String,
        observer: '_keyChanged'
      },
      value: {
        type: Object,
        notify: true,
        readOnly: true
      },
      list: {
        type: Array,
        notify: true
      }
    },
    factoryImpl: function(config) {
      if (config) {
        for (var n in config) {
          switch (n) {
           case 'type':
           case 'key':
            this[n] = config[n];
            break;
          }
        }
      }
    },
    created: function() {
      this._metaDatas = metaDatas;
      this._metaArrays = metaArrays;
    },
    _keyChanged: function(key) {
      this._setValue(this._metaData && this._metaData[key]);
    },
    _typeChanged: function(type) {
      this._metaData = metaDatas[type];
      this.list = metaArrays[type];
      if (this.key) {
        this._keyChanged(this.key);
      }
    },
    byKey: function(key) {
      return this._metaData && this._metaData[key];
    }
  });
})();

Polymer({
  is: 'iron-icon',
  properties: {
    icon: {
      type: String,
      observer: '_iconChanged'
    },
    theme: {
      type: String,
      observer: '_updateIcon'
    },
    src: {
      type: String,
      observer: '_srcChanged'
    },
    _meta: {
      value: Polymer.Base.create('iron-meta', {
        type: 'iconset'
      }),
      observer: '_updateIcon'
    }
  },
  _DEFAULT_ICONSET: 'icons',
  _iconChanged: function(icon) {
    var parts = (icon || '').split(':');
    this._iconName = parts.pop();
    this._iconsetName = parts.pop() || this._DEFAULT_ICONSET;
    this._updateIcon();
  },
  _srcChanged: function(src) {
    this._updateIcon();
  },
  _usesIconset: function() {
    return this.icon || !this.src;
  },
  _updateIcon: function() {
    if (this._usesIconset()) {
      if (this._img && this._img.parentNode) {
        Polymer.dom(this.root).removeChild(this._img);
      }
      if (this._iconName === "") {
        if (this._iconset) {
          this._iconset.removeIcon(this);
        }
      } else if (this._iconsetName && this._meta) {
        this._iconset = this._meta.byKey(this._iconsetName);
        if (this._iconset) {
          this._iconset.applyIcon(this, this._iconName, this.theme);
          this.unlisten(window, 'iron-iconset-added', '_updateIcon');
        } else {
          this.listen(window, 'iron-iconset-added', '_updateIcon');
        }
      }
    } else {
      if (this._iconset) {
        this._iconset.removeIcon(this);
      }
      if (!this._img) {
        this._img = document.createElement('img');
        this._img.style.width = '100%';
        this._img.style.height = '100%';
        this._img.draggable = false;
      }
      this._img.src = this.src;
      Polymer.dom(this.root).appendChild(this._img);
    }
  }
});

Polymer.PaperInkyFocusBehaviorImpl = {
  observers: [ '_focusedChanged(receivedFocusFromKeyboard)' ],
  _focusedChanged: function(receivedFocusFromKeyboard) {
    if (receivedFocusFromKeyboard) {
      this.ensureRipple();
    }
    if (this.hasRipple()) {
      this._ripple.holdDown = receivedFocusFromKeyboard;
    }
  },
  _createRipple: function() {
    var ripple = Polymer.PaperRippleBehavior._createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('center', '');
    ripple.classList.add('circle');
    return ripple;
  }
};

Polymer.PaperInkyFocusBehavior = [ Polymer.IronButtonState, Polymer.IronControlState, Polymer.PaperRippleBehavior, Polymer.PaperInkyFocusBehaviorImpl ];

Polymer({
  is: 'paper-icon-button',
  hostAttributes: {
    role: 'button',
    tabindex: '0'
  },
  behaviors: [ Polymer.PaperInkyFocusBehavior ],
  properties: {
    src: {
      type: String
    },
    icon: {
      type: String
    },
    alt: {
      type: String,
      observer: "_altChanged"
    }
  },
  _altChanged: function(newValue, oldValue) {
    var label = this.getAttribute('aria-label');
    if (!label || oldValue == label) {
      this.setAttribute('aria-label', newValue);
    }
  }
});

Polymer({
  is: 'paper-tab',
  behaviors: [ Polymer.IronControlState, Polymer.IronButtonState, Polymer.PaperRippleBehavior ],
  properties: {
    link: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    }
  },
  hostAttributes: {
    role: 'tab'
  },
  listeners: {
    down: '_updateNoink',
    tap: '_onTap'
  },
  attached: function() {
    this._updateNoink();
  },
  get _parentNoink() {
    var parent = Polymer.dom(this).parentNode;
    return !!parent && !!parent.noink;
  },
  _updateNoink: function() {
    this.noink = !!this.noink || !!this._parentNoink;
  },
  _onTap: function(event) {
    if (this.link) {
      var anchor = this.queryEffectiveChildren('a');
      if (!anchor) {
        return;
      }
      if (event.target === anchor) {
        return;
      }
      anchor.click();
    }
  }
});

Polymer.IronMultiSelectableBehaviorImpl = {
  properties: {
    multi: {
      type: Boolean,
      value: false,
      observer: 'multiChanged'
    },
    selectedValues: {
      type: Array,
      notify: true
    },
    selectedItems: {
      type: Array,
      readOnly: true,
      notify: true
    }
  },
  observers: [ '_updateSelected(selectedValues.splices)' ],
  select: function(value) {
    if (this.multi) {
      if (this.selectedValues) {
        this._toggleSelected(value);
      } else {
        this.selectedValues = [ value ];
      }
    } else {
      this.selected = value;
    }
  },
  multiChanged: function(multi) {
    this._selection.multi = multi;
  },
  get _shouldUpdateSelection() {
    return this.selected != null || this.selectedValues != null && this.selectedValues.length;
  },
  _updateAttrForSelected: function() {
    if (!this.multi) {
      Polymer.IronSelectableBehavior._updateAttrForSelected.apply(this);
    } else if (this._shouldUpdateSelection) {
      this.selectedValues = this.selectedItems.map(function(selectedItem) {
        return this._indexToValue(this.indexOf(selectedItem));
      }, this).filter(function(unfilteredValue) {
        return unfilteredValue != null;
      }, this);
    }
  },
  _updateSelected: function() {
    if (this.multi) {
      this._selectMulti(this.selectedValues);
    } else {
      this._selectSelected(this.selected);
    }
  },
  _selectMulti: function(values) {
    if (values) {
      var selectedItems = this._valuesToItems(values);
      this._selection.clear(selectedItems);
      for (var i = 0; i < selectedItems.length; i++) {
        this._selection.setItemSelected(selectedItems[i], true);
      }
      if (this.fallbackSelection && this.items.length && !this._selection.get().length) {
        var fallback = this._valueToItem(this.fallbackSelection);
        if (fallback) {
          this.selectedValues = [ this.fallbackSelection ];
        }
      }
    } else {
      this._selection.clear();
    }
  },
  _selectionChange: function() {
    var s = this._selection.get();
    if (this.multi) {
      this._setSelectedItems(s);
    } else {
      this._setSelectedItems([ s ]);
      this._setSelectedItem(s);
    }
  },
  _toggleSelected: function(value) {
    var i = this.selectedValues.indexOf(value);
    var unselected = i < 0;
    if (unselected) {
      this.push('selectedValues', value);
    } else {
      this.splice('selectedValues', i, 1);
    }
  },
  _valuesToItems: function(values) {
    return values == null ? null : values.map(function(value) {
      return this._valueToItem(value);
    }, this);
  }
};

Polymer.IronMultiSelectableBehavior = [ Polymer.IronSelectableBehavior, Polymer.IronMultiSelectableBehaviorImpl ];

Polymer.IronMenuBehaviorImpl = {
  properties: {
    focusedItem: {
      observer: '_focusedItemChanged',
      readOnly: true,
      type: Object
    },
    attrForItemTitle: {
      type: String
    }
  },
  hostAttributes: {
    role: 'menu',
    tabindex: '0'
  },
  observers: [ '_updateMultiselectable(multi)' ],
  listeners: {
    focus: '_onFocus',
    keydown: '_onKeydown',
    'iron-items-changed': '_onIronItemsChanged'
  },
  keyBindings: {
    up: '_onUpKey',
    down: '_onDownKey',
    esc: '_onEscKey',
    'shift+tab:keydown': '_onShiftTabDown'
  },
  attached: function() {
    this._resetTabindices();
  },
  select: function(value) {
    if (this._defaultFocusAsync) {
      this.cancelAsync(this._defaultFocusAsync);
      this._defaultFocusAsync = null;
    }
    var item = this._valueToItem(value);
    if (item && item.hasAttribute('disabled')) return;
    this._setFocusedItem(item);
    Polymer.IronMultiSelectableBehaviorImpl.select.apply(this, arguments);
  },
  _resetTabindices: function() {
    var selectedItem = this.multi ? this.selectedItems && this.selectedItems[0] : this.selectedItem;
    this.items.forEach(function(item) {
      item.setAttribute('tabindex', item === selectedItem ? '0' : '-1');
    }, this);
  },
  _updateMultiselectable: function(multi) {
    if (multi) {
      this.setAttribute('aria-multiselectable', 'true');
    } else {
      this.removeAttribute('aria-multiselectable');
    }
  },
  _focusWithKeyboardEvent: function(event) {
    for (var i = 0, item; item = this.items[i]; i++) {
      var attr = this.attrForItemTitle || 'textContent';
      var title = item[attr] || item.getAttribute(attr);
      if (!item.hasAttribute('disabled') && title && title.trim().charAt(0).toLowerCase() === String.fromCharCode(event.keyCode).toLowerCase()) {
        this._setFocusedItem(item);
        break;
      }
    }
  },
  _focusPrevious: function() {
    var length = this.items.length;
    var curFocusIndex = Number(this.indexOf(this.focusedItem));
    for (var i = 1; i < length + 1; i++) {
      var item = this.items[(curFocusIndex - i + length) % length];
      if (!item.hasAttribute('disabled')) {
        this._setFocusedItem(item);
        return;
      }
    }
  },
  _focusNext: function() {
    var length = this.items.length;
    var curFocusIndex = Number(this.indexOf(this.focusedItem));
    for (var i = 1; i < length + 1; i++) {
      var item = this.items[(curFocusIndex + i) % length];
      if (!item.hasAttribute('disabled')) {
        this._setFocusedItem(item);
        return;
      }
    }
  },
  _applySelection: function(item, isSelected) {
    if (isSelected) {
      item.setAttribute('aria-selected', 'true');
    } else {
      item.removeAttribute('aria-selected');
    }
    Polymer.IronSelectableBehavior._applySelection.apply(this, arguments);
  },
  _focusedItemChanged: function(focusedItem, old) {
    old && old.setAttribute('tabindex', '-1');
    if (focusedItem) {
      focusedItem.setAttribute('tabindex', '0');
      focusedItem.focus();
    }
  },
  _onIronItemsChanged: function(event) {
    if (event.detail.addedNodes.length) {
      this._resetTabindices();
    }
  },
  _onShiftTabDown: function(event) {
    var oldTabIndex = this.getAttribute('tabindex');
    Polymer.IronMenuBehaviorImpl._shiftTabPressed = true;
    this._setFocusedItem(null);
    this.setAttribute('tabindex', '-1');
    this.async(function() {
      this.setAttribute('tabindex', oldTabIndex);
      Polymer.IronMenuBehaviorImpl._shiftTabPressed = false;
    }, 1);
  },
  _onFocus: function(event) {
    if (Polymer.IronMenuBehaviorImpl._shiftTabPressed) {
      return;
    }
    var rootTarget = Polymer.dom(event).rootTarget;
    if (rootTarget !== this && typeof rootTarget.tabIndex !== "undefined" && !this.isLightDescendant(rootTarget)) {
      return;
    }
    this._defaultFocusAsync = this.async(function() {
      var selectedItem = this.multi ? this.selectedItems && this.selectedItems[0] : this.selectedItem;
      this._setFocusedItem(null);
      if (selectedItem) {
        this._setFocusedItem(selectedItem);
      } else if (this.items[0]) {
        this._focusNext();
      }
    });
  },
  _onUpKey: function(event) {
    this._focusPrevious();
    event.detail.keyboardEvent.preventDefault();
  },
  _onDownKey: function(event) {
    this._focusNext();
    event.detail.keyboardEvent.preventDefault();
  },
  _onEscKey: function(event) {
    this.focusedItem.blur();
  },
  _onKeydown: function(event) {
    if (!this.keyboardEventMatchesKeys(event, 'up down esc')) {
      this._focusWithKeyboardEvent(event);
    }
    event.stopPropagation();
  },
  _activateHandler: function(event) {
    Polymer.IronSelectableBehavior._activateHandler.call(this, event);
    event.stopPropagation();
  }
};

Polymer.IronMenuBehaviorImpl._shiftTabPressed = false;

Polymer.IronMenuBehavior = [ Polymer.IronMultiSelectableBehavior, Polymer.IronA11yKeysBehavior, Polymer.IronMenuBehaviorImpl ];

Polymer.IronMenubarBehaviorImpl = {
  hostAttributes: {
    role: 'menubar'
  },
  keyBindings: {
    left: '_onLeftKey',
    right: '_onRightKey'
  },
  _onUpKey: function(event) {
    this.focusedItem.click();
    event.detail.keyboardEvent.preventDefault();
  },
  _onDownKey: function(event) {
    this.focusedItem.click();
    event.detail.keyboardEvent.preventDefault();
  },
  get _isRTL() {
    return window.getComputedStyle(this)['direction'] === 'rtl';
  },
  _onLeftKey: function(event) {
    if (this._isRTL) {
      this._focusNext();
    } else {
      this._focusPrevious();
    }
    event.detail.keyboardEvent.preventDefault();
  },
  _onRightKey: function(event) {
    if (this._isRTL) {
      this._focusPrevious();
    } else {
      this._focusNext();
    }
    event.detail.keyboardEvent.preventDefault();
  },
  _onKeydown: function(event) {
    if (this.keyboardEventMatchesKeys(event, 'up down left right esc')) {
      return;
    }
    this._focusWithKeyboardEvent(event);
  }
};

Polymer.IronMenubarBehavior = [ Polymer.IronMenuBehavior, Polymer.IronMenubarBehaviorImpl ];

Polymer({
  is: 'iron-iconset-svg',
  properties: {
    name: {
      type: String,
      observer: '_nameChanged'
    },
    size: {
      type: Number,
      value: 24
    }
  },
  attached: function() {
    this.style.display = 'none';
  },
  getIconNames: function() {
    this._icons = this._createIconMap();
    return Object.keys(this._icons).map(function(n) {
      return this.name + ':' + n;
    }, this);
  },
  applyIcon: function(element, iconName) {
    element = element.root || element;
    this.removeIcon(element);
    var svg = this._cloneIcon(iconName);
    if (svg) {
      var pde = Polymer.dom(element);
      pde.insertBefore(svg, pde.childNodes[0]);
      return element._svgIcon = svg;
    }
    return null;
  },
  removeIcon: function(element) {
    if (element._svgIcon) {
      Polymer.dom(element).removeChild(element._svgIcon);
      element._svgIcon = null;
    }
  },
  _nameChanged: function() {
    new Polymer.IronMeta({
      type: 'iconset',
      key: this.name,
      value: this
    });
    this.async(function() {
      this.fire('iron-iconset-added', this, {
        node: window
      });
    });
  },
  _createIconMap: function() {
    var icons = Object.create(null);
    Polymer.dom(this).querySelectorAll('[id]').forEach(function(icon) {
      icons[icon.id] = icon;
    });
    return icons;
  },
  _cloneIcon: function(id) {
    this._icons = this._icons || this._createIconMap();
    return this._prepareSvgClone(this._icons[id], this.size);
  },
  _prepareSvgClone: function(sourceSvg, size) {
    if (sourceSvg) {
      var content = sourceSvg.cloneNode(true), svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg'), viewBox = content.getAttribute('viewBox') || '0 0 ' + size + ' ' + size;
      svg.setAttribute('viewBox', viewBox);
      svg.setAttribute('preserveAspectRatio', 'xMidYMid meet');
      svg.style.cssText = 'pointer-events: none; display: block; width: 100%; height: 100%;';
      svg.appendChild(content).removeAttribute('id');
      return svg;
    }
    return null;
  }
});

Polymer({
  is: 'paper-tabs',
  behaviors: [ Polymer.IronResizableBehavior, Polymer.IronMenubarBehavior ],
  properties: {
    noink: {
      type: Boolean,
      value: false,
      observer: '_noinkChanged'
    },
    noBar: {
      type: Boolean,
      value: false
    },
    noSlide: {
      type: Boolean,
      value: false
    },
    scrollable: {
      type: Boolean,
      value: false
    },
    fitContainer: {
      type: Boolean,
      value: false
    },
    disableDrag: {
      type: Boolean,
      value: false
    },
    hideScrollButtons: {
      type: Boolean,
      value: false
    },
    alignBottom: {
      type: Boolean,
      value: false
    },
    selectable: {
      type: String,
      value: 'paper-tab'
    },
    autoselect: {
      type: Boolean,
      value: false
    },
    autoselectDelay: {
      type: Number,
      value: 0
    },
    _step: {
      type: Number,
      value: 10
    },
    _holdDelay: {
      type: Number,
      value: 1
    },
    _leftHidden: {
      type: Boolean,
      value: false
    },
    _rightHidden: {
      type: Boolean,
      value: false
    },
    _previousTab: {
      type: Object
    }
  },
  hostAttributes: {
    role: 'tablist'
  },
  listeners: {
    'iron-resize': '_onTabSizingChanged',
    'iron-items-changed': '_onTabSizingChanged',
    'iron-select': '_onIronSelect',
    'iron-deselect': '_onIronDeselect'
  },
  keyBindings: {
    'left:keyup right:keyup': '_onArrowKeyup'
  },
  created: function() {
    this._holdJob = null;
    this._pendingActivationItem = undefined;
    this._pendingActivationTimeout = undefined;
    this._bindDelayedActivationHandler = this._delayedActivationHandler.bind(this);
    this.addEventListener('blur', this._onBlurCapture.bind(this), true);
  },
  ready: function() {
    this.setScrollDirection('y', this.$.tabsContainer);
  },
  detached: function() {
    this._cancelPendingActivation();
  },
  _noinkChanged: function(noink) {
    var childTabs = Polymer.dom(this).querySelectorAll('paper-tab');
    childTabs.forEach(noink ? this._setNoinkAttribute : this._removeNoinkAttribute);
  },
  _setNoinkAttribute: function(element) {
    element.setAttribute('noink', '');
  },
  _removeNoinkAttribute: function(element) {
    element.removeAttribute('noink');
  },
  _computeScrollButtonClass: function(hideThisButton, scrollable, hideScrollButtons) {
    if (!scrollable || hideScrollButtons) {
      return 'hidden';
    }
    if (hideThisButton) {
      return 'not-visible';
    }
    return '';
  },
  _computeTabsContentClass: function(scrollable, fitContainer) {
    return scrollable ? 'scrollable' + (fitContainer ? ' fit-container' : '') : ' fit-container';
  },
  _computeSelectionBarClass: function(noBar, alignBottom) {
    if (noBar) {
      return 'hidden';
    } else if (alignBottom) {
      return 'align-bottom';
    }
    return '';
  },
  _onTabSizingChanged: function() {
    this.debounce('_onTabSizingChanged', function() {
      this._scroll();
      this._tabChanged(this.selectedItem);
    }, 10);
  },
  _onIronSelect: function(event) {
    this._tabChanged(event.detail.item, this._previousTab);
    this._previousTab = event.detail.item;
    this.cancelDebouncer('tab-changed');
  },
  _onIronDeselect: function(event) {
    this.debounce('tab-changed', function() {
      this._tabChanged(null, this._previousTab);
      this._previousTab = null;
    }, 1);
  },
  _activateHandler: function() {
    this._cancelPendingActivation();
    Polymer.IronMenuBehaviorImpl._activateHandler.apply(this, arguments);
  },
  _scheduleActivation: function(item, delay) {
    this._pendingActivationItem = item;
    this._pendingActivationTimeout = this.async(this._bindDelayedActivationHandler, delay);
  },
  _delayedActivationHandler: function() {
    var item = this._pendingActivationItem;
    this._pendingActivationItem = undefined;
    this._pendingActivationTimeout = undefined;
    item.fire(this.activateEvent, null, {
      bubbles: true,
      cancelable: true
    });
  },
  _cancelPendingActivation: function() {
    if (this._pendingActivationTimeout !== undefined) {
      this.cancelAsync(this._pendingActivationTimeout);
      this._pendingActivationItem = undefined;
      this._pendingActivationTimeout = undefined;
    }
  },
  _onArrowKeyup: function(event) {
    if (this.autoselect) {
      this._scheduleActivation(this.focusedItem, this.autoselectDelay);
    }
  },
  _onBlurCapture: function(event) {
    if (event.target === this._pendingActivationItem) {
      this._cancelPendingActivation();
    }
  },
  get _tabContainerScrollSize() {
    return Math.max(0, this.$.tabsContainer.scrollWidth - this.$.tabsContainer.offsetWidth);
  },
  _scroll: function(e, detail) {
    if (!this.scrollable) {
      return;
    }
    var ddx = detail && -detail.ddx || 0;
    this._affectScroll(ddx);
  },
  _down: function(e) {
    this.async(function() {
      if (this._defaultFocusAsync) {
        this.cancelAsync(this._defaultFocusAsync);
        this._defaultFocusAsync = null;
      }
    }, 1);
  },
  _affectScroll: function(dx) {
    this.$.tabsContainer.scrollLeft += dx;
    var scrollLeft = this.$.tabsContainer.scrollLeft;
    this._leftHidden = scrollLeft === 0;
    this._rightHidden = scrollLeft === this._tabContainerScrollSize;
  },
  _onLeftScrollButtonDown: function() {
    this._scrollToLeft();
    this._holdJob = setInterval(this._scrollToLeft.bind(this), this._holdDelay);
  },
  _onRightScrollButtonDown: function() {
    this._scrollToRight();
    this._holdJob = setInterval(this._scrollToRight.bind(this), this._holdDelay);
  },
  _onScrollButtonUp: function() {
    clearInterval(this._holdJob);
    this._holdJob = null;
  },
  _scrollToLeft: function() {
    this._affectScroll(-this._step);
  },
  _scrollToRight: function() {
    this._affectScroll(this._step);
  },
  _tabChanged: function(tab, old) {
    if (!tab) {
      this.$.selectionBar.classList.remove('expand');
      this.$.selectionBar.classList.remove('contract');
      this._positionBar(0, 0);
      return;
    }
    var r = this.$.tabsContent.getBoundingClientRect();
    var w = r.width;
    var tabRect = tab.getBoundingClientRect();
    var tabOffsetLeft = tabRect.left - r.left;
    this._pos = {
      width: this._calcPercent(tabRect.width, w),
      left: this._calcPercent(tabOffsetLeft, w)
    };
    if (this.noSlide || old == null) {
      this.$.selectionBar.classList.remove('expand');
      this.$.selectionBar.classList.remove('contract');
      this._positionBar(this._pos.width, this._pos.left);
      return;
    }
    var oldRect = old.getBoundingClientRect();
    var oldIndex = this.items.indexOf(old);
    var index = this.items.indexOf(tab);
    var m = 5;
    this.$.selectionBar.classList.add('expand');
    var moveRight = oldIndex < index;
    var isRTL = this._isRTL;
    if (isRTL) {
      moveRight = !moveRight;
    }
    if (moveRight) {
      this._positionBar(this._calcPercent(tabRect.left + tabRect.width - oldRect.left, w) - m, this._left);
    } else {
      this._positionBar(this._calcPercent(oldRect.left + oldRect.width - tabRect.left, w) - m, this._calcPercent(tabOffsetLeft, w) + m);
    }
    if (this.scrollable) {
      this._scrollToSelectedIfNeeded(tabRect.width, tabOffsetLeft);
    }
  },
  _scrollToSelectedIfNeeded: function(tabWidth, tabOffsetLeft) {
    var l = tabOffsetLeft - this.$.tabsContainer.scrollLeft;
    if (l < 0) {
      this.$.tabsContainer.scrollLeft += l;
    } else {
      l += tabWidth - this.$.tabsContainer.offsetWidth;
      if (l > 0) {
        this.$.tabsContainer.scrollLeft += l;
      }
    }
  },
  _calcPercent: function(w, w0) {
    return 100 * w / w0;
  },
  _positionBar: function(width, left) {
    width = width || 0;
    left = left || 0;
    this._width = width;
    this._left = left;
    this.transform('translateX(' + left + '%) scaleX(' + width / 100 + ')', this.$.selectionBar);
  },
  _onBarTransitionEnd: function(e) {
    var cl = this.$.selectionBar.classList;
    if (cl.contains('expand')) {
      cl.remove('expand');
      cl.add('contract');
      this._positionBar(this._pos.width, this._pos.left);
    } else if (cl.contains('contract')) {
      cl.remove('contract');
    }
  }
});

(function() {
  'use strict';
  Polymer.IronA11yAnnouncer = Polymer({
    is: 'iron-a11y-announcer',
    properties: {
      mode: {
        type: String,
        value: 'polite'
      },
      _text: {
        type: String,
        value: ''
      }
    },
    created: function() {
      if (!Polymer.IronA11yAnnouncer.instance) {
        Polymer.IronA11yAnnouncer.instance = this;
      }
      document.body.addEventListener('iron-announce', this._onIronAnnounce.bind(this));
    },
    announce: function(text) {
      this._text = '';
      this.async(function() {
        this._text = text;
      }, 100);
    },
    _onIronAnnounce: function(event) {
      if (event.detail && event.detail.text) {
        this.announce(event.detail.text);
      }
    }
  });
  Polymer.IronA11yAnnouncer.instance = null;
  Polymer.IronA11yAnnouncer.requestAvailability = function() {
    if (!Polymer.IronA11yAnnouncer.instance) {
      Polymer.IronA11yAnnouncer.instance = document.createElement('iron-a11y-announcer');
    }
    document.body.appendChild(Polymer.IronA11yAnnouncer.instance);
  };
})();

Polymer.IronValidatableBehaviorMeta = null;

Polymer.IronValidatableBehavior = {
  properties: {
    validator: {
      type: String
    },
    invalid: {
      notify: true,
      reflectToAttribute: true,
      type: Boolean,
      value: false
    },
    _validatorMeta: {
      type: Object
    },
    validatorType: {
      type: String,
      value: 'validator'
    },
    _validator: {
      type: Object,
      computed: '__computeValidator(validator)'
    }
  },
  observers: [ '_invalidChanged(invalid)' ],
  registered: function() {
    Polymer.IronValidatableBehaviorMeta = new Polymer.IronMeta({
      type: 'validator'
    });
  },
  _invalidChanged: function() {
    if (this.invalid) {
      this.setAttribute('aria-invalid', 'true');
    } else {
      this.removeAttribute('aria-invalid');
    }
  },
  hasValidator: function() {
    return this._validator != null;
  },
  validate: function(value) {
    this.invalid = !this._getValidity(value);
    return !this.invalid;
  },
  _getValidity: function(value) {
    if (this.hasValidator()) {
      return this._validator.validate(value);
    }
    return true;
  },
  __computeValidator: function() {
    return Polymer.IronValidatableBehaviorMeta && Polymer.IronValidatableBehaviorMeta.byKey(this.validator);
  }
};

Polymer({
  is: 'iron-input',
  "extends": 'input',
  behaviors: [ Polymer.IronValidatableBehavior ],
  properties: {
    bindValue: {
      observer: '_bindValueChanged',
      type: String
    },
    preventInvalidInput: {
      type: Boolean
    },
    allowedPattern: {
      type: String,
      observer: "_allowedPatternChanged"
    },
    _previousValidInput: {
      type: String,
      value: ''
    },
    _patternAlreadyChecked: {
      type: Boolean,
      value: false
    }
  },
  listeners: {
    input: '_onInput',
    keypress: '_onKeypress'
  },
  registered: function() {
    if (!this._canDispatchEventOnDisabled()) {
      this._origDispatchEvent = this.dispatchEvent;
      this.dispatchEvent = this._dispatchEventFirefoxIE;
    }
  },
  created: function() {
    Polymer.IronA11yAnnouncer.requestAvailability();
  },
  _canDispatchEventOnDisabled: function() {
    var input = document.createElement('input');
    var canDispatch = false;
    input.disabled = true;
    input.addEventListener('feature-check-dispatch-event', function() {
      canDispatch = true;
    });
    try {
      input.dispatchEvent(new Event('feature-check-dispatch-event'));
    } catch (e) {}
    return canDispatch;
  },
  _dispatchEventFirefoxIE: function() {
    var disabled = this.disabled;
    this.disabled = false;
    this._origDispatchEvent.apply(this, arguments);
    this.disabled = disabled;
  },
  get _patternRegExp() {
    var pattern;
    if (this.allowedPattern) {
      pattern = new RegExp(this.allowedPattern);
    } else {
      switch (this.type) {
       case 'number':
        pattern = /[0-9.,e-]/;
        break;
      }
    }
    return pattern;
  },
  ready: function() {
    this.bindValue = this.value;
  },
  _bindValueChanged: function() {
    if (this.value !== this.bindValue) {
      this.value = !(this.bindValue || this.bindValue === 0 || this.bindValue === false) ? '' : this.bindValue;
    }
    this.fire('bind-value-changed', {
      value: this.bindValue
    });
  },
  _allowedPatternChanged: function() {
    this.preventInvalidInput = this.allowedPattern ? true : false;
  },
  _onInput: function() {
    if (this.preventInvalidInput && !this._patternAlreadyChecked) {
      var valid = this._checkPatternValidity();
      if (!valid) {
        this._announceInvalidCharacter('Invalid string of characters not entered.');
        this.value = this._previousValidInput;
      }
    }
    this.bindValue = this.value;
    this._previousValidInput = this.value;
    this._patternAlreadyChecked = false;
  },
  _isPrintable: function(event) {
    var anyNonPrintable = event.keyCode == 8 || event.keyCode == 9 || event.keyCode == 13 || event.keyCode == 27;
    var mozNonPrintable = event.keyCode == 19 || event.keyCode == 20 || event.keyCode == 45 || event.keyCode == 46 || event.keyCode == 144 || event.keyCode == 145 || event.keyCode > 32 && event.keyCode < 41 || event.keyCode > 111 && event.keyCode < 124;
    return !anyNonPrintable && !(event.charCode == 0 && mozNonPrintable);
  },
  _onKeypress: function(event) {
    if (!this.preventInvalidInput && this.type !== 'number') {
      return;
    }
    var regexp = this._patternRegExp;
    if (!regexp) {
      return;
    }
    if (event.metaKey || event.ctrlKey || event.altKey) return;
    this._patternAlreadyChecked = true;
    var thisChar = String.fromCharCode(event.charCode);
    if (this._isPrintable(event) && !regexp.test(thisChar)) {
      event.preventDefault();
      this._announceInvalidCharacter('Invalid character ' + thisChar + ' not entered.');
    }
  },
  _checkPatternValidity: function() {
    var regexp = this._patternRegExp;
    if (!regexp) {
      return true;
    }
    for (var i = 0; i < this.value.length; i++) {
      if (!regexp.test(this.value[i])) {
        return false;
      }
    }
    return true;
  },
  validate: function() {
    var valid = this.checkValidity();
    if (valid) {
      if (this.required && this.value === '') {
        valid = false;
      } else if (this.hasValidator()) {
        valid = Polymer.IronValidatableBehavior.validate.call(this, this.value);
      }
    }
    this.invalid = !valid;
    this.fire('iron-input-validate');
    return valid;
  },
  _announceInvalidCharacter: function(message) {
    this.fire('iron-announce', {
      text: message
    });
  }
});

Polymer({
  is: 'paper-input-container',
  properties: {
    noLabelFloat: {
      type: Boolean,
      value: false
    },
    alwaysFloatLabel: {
      type: Boolean,
      value: false
    },
    attrForValue: {
      type: String,
      value: 'bind-value'
    },
    autoValidate: {
      type: Boolean,
      value: false
    },
    invalid: {
      observer: '_invalidChanged',
      type: Boolean,
      value: false
    },
    focused: {
      readOnly: true,
      type: Boolean,
      value: false,
      notify: true
    },
    _addons: {
      type: Array
    },
    _inputHasContent: {
      type: Boolean,
      value: false
    },
    _inputSelector: {
      type: String,
      value: 'input,textarea,.paper-input-input'
    },
    _boundOnFocus: {
      type: Function,
      value: function() {
        return this._onFocus.bind(this);
      }
    },
    _boundOnBlur: {
      type: Function,
      value: function() {
        return this._onBlur.bind(this);
      }
    },
    _boundOnInput: {
      type: Function,
      value: function() {
        return this._onInput.bind(this);
      }
    },
    _boundValueChanged: {
      type: Function,
      value: function() {
        return this._onValueChanged.bind(this);
      }
    }
  },
  listeners: {
    'addon-attached': '_onAddonAttached',
    'iron-input-validate': '_onIronInputValidate'
  },
  get _valueChangedEvent() {
    return this.attrForValue + '-changed';
  },
  get _propertyForValue() {
    return Polymer.CaseMap.dashToCamelCase(this.attrForValue);
  },
  get _inputElement() {
    return Polymer.dom(this).querySelector(this._inputSelector);
  },
  get _inputElementValue() {
    return this._inputElement[this._propertyForValue] || this._inputElement.value;
  },
  ready: function() {
    if (!this._addons) {
      this._addons = [];
    }
    this.addEventListener('focus', this._boundOnFocus, true);
    this.addEventListener('blur', this._boundOnBlur, true);
  },
  attached: function() {
    if (this.attrForValue) {
      this._inputElement.addEventListener(this._valueChangedEvent, this._boundValueChanged);
    } else {
      this.addEventListener('input', this._onInput);
    }
    if (this._inputElementValue != '') {
      this._handleValueAndAutoValidate(this._inputElement);
    } else {
      this._handleValue(this._inputElement);
    }
  },
  _onAddonAttached: function(event) {
    if (!this._addons) {
      this._addons = [];
    }
    var target = event.target;
    if (this._addons.indexOf(target) === -1) {
      this._addons.push(target);
      if (this.isAttached) {
        this._handleValue(this._inputElement);
      }
    }
  },
  _onFocus: function() {
    this._setFocused(true);
  },
  _onBlur: function() {
    this._setFocused(false);
    this._handleValueAndAutoValidate(this._inputElement);
  },
  _onInput: function(event) {
    this._handleValueAndAutoValidate(event.target);
  },
  _onValueChanged: function(event) {
    this._handleValueAndAutoValidate(event.target);
  },
  _handleValue: function(inputElement) {
    var value = this._inputElementValue;
    if (value || value === 0 || inputElement.type === 'number' && !inputElement.checkValidity()) {
      this._inputHasContent = true;
    } else {
      this._inputHasContent = false;
    }
    this.updateAddons({
      inputElement: inputElement,
      value: value,
      invalid: this.invalid
    });
  },
  _handleValueAndAutoValidate: function(inputElement) {
    if (this.autoValidate) {
      var valid;
      if (inputElement.validate) {
        valid = inputElement.validate(this._inputElementValue);
      } else {
        valid = inputElement.checkValidity();
      }
      this.invalid = !valid;
    }
    this._handleValue(inputElement);
  },
  _onIronInputValidate: function(event) {
    this.invalid = this._inputElement.invalid;
  },
  _invalidChanged: function() {
    if (this._addons) {
      this.updateAddons({
        invalid: this.invalid
      });
    }
  },
  updateAddons: function(state) {
    for (var addon, index = 0; addon = this._addons[index]; index++) {
      addon.update(state);
    }
  },
  _computeInputContentClass: function(noLabelFloat, alwaysFloatLabel, focused, invalid, _inputHasContent) {
    var cls = 'input-content';
    if (!noLabelFloat) {
      var label = this.querySelector('label');
      if (alwaysFloatLabel || _inputHasContent) {
        cls += ' label-is-floating';
        this.$.labelAndInputContainer.style.position = 'static';
        if (invalid) {
          cls += ' is-invalid';
        } else if (focused) {
          cls += " label-is-highlighted";
        }
      } else {
        if (label) {
          this.$.labelAndInputContainer.style.position = 'relative';
        }
      }
    } else {
      if (_inputHasContent) {
        cls += ' label-is-hidden';
      }
    }
    return cls;
  },
  _computeUnderlineClass: function(focused, invalid) {
    var cls = 'underline';
    if (invalid) {
      cls += ' is-invalid';
    } else if (focused) {
      cls += ' is-highlighted';
    }
    return cls;
  },
  _computeAddOnContentClass: function(focused, invalid) {
    var cls = 'add-on-content';
    if (invalid) {
      cls += ' is-invalid';
    } else if (focused) {
      cls += ' is-highlighted';
    }
    return cls;
  }
});

Polymer.PaperSpinnerBehavior = {
  listeners: {
    animationend: '__reset',
    webkitAnimationEnd: '__reset'
  },
  properties: {
    active: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: '__activeChanged'
    },
    alt: {
      type: String,
      value: 'loading',
      observer: '__altChanged'
    },
    __coolingDown: {
      type: Boolean,
      value: false
    }
  },
  __computeContainerClasses: function(active, coolingDown) {
    return [ active || coolingDown ? 'active' : '', coolingDown ? 'cooldown' : '' ].join(' ');
  },
  __activeChanged: function(active, old) {
    this.__setAriaHidden(!active);
    this.__coolingDown = !active && old;
  },
  __altChanged: function(alt) {
    if (alt === this.getPropertyInfo('alt').value) {
      this.alt = this.getAttribute('aria-label') || alt;
    } else {
      this.__setAriaHidden(alt === '');
      this.setAttribute('aria-label', alt);
    }
  },
  __setAriaHidden: function(hidden) {
    var attr = 'aria-hidden';
    if (hidden) {
      this.setAttribute(attr, 'true');
    } else {
      this.removeAttribute(attr);
    }
  },
  __reset: function() {
    this.active = false;
    this.__coolingDown = false;
  }
};

Polymer({
  is: 'paper-spinner-lite',
  behaviors: [ Polymer.PaperSpinnerBehavior ]
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var CrSearchFieldBehavior = {
  properties: {
    label: {
      type: String,
      value: ''
    },
    clearLabel: {
      type: String,
      value: ''
    },
    showingSearch: {
      type: Boolean,
      value: false,
      notify: true,
      observer: 'showingSearchChanged_',
      reflectToAttribute: true
    },
    lastValue_: {
      type: String,
      value: ''
    }
  },
  getSearchInput: function() {},
  getValue: function() {
    return this.getSearchInput().value;
  },
  setValue: function(value) {
    this.getSearchInput().bindValue = value;
    this.onValueChanged_(value);
  },
  showAndFocus: function() {
    this.showingSearch = true;
    this.focus_();
  },
  focus_: function() {
    this.getSearchInput().focus();
  },
  onSearchTermSearch: function() {
    this.onValueChanged_(this.getValue());
  },
  onValueChanged_: function(newValue) {
    if (newValue == this.lastValue_) return;
    this.fire('search-changed', newValue);
    this.lastValue_ = newValue;
  },
  onSearchTermKeydown: function(e) {
    if (e.key == 'Escape') this.showingSearch = false;
  },
  showingSearchChanged_: function() {
    if (this.showingSearch) {
      this.focus_();
      return;
    }
    this.setValue('');
    this.getSearchInput().blur();
  }
};

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'cr-toolbar-search-field',
  behaviors: [ CrSearchFieldBehavior ],
  properties: {
    narrow: {
      type: Boolean,
      reflectToAttribute: true
    },
    label: String,
    clearLabel: String,
    spinnerActive: {
      type: Boolean,
      reflectToAttribute: true
    },
    hasSearchText_: Boolean
  },
  listeners: {
    tap: 'showSearch_',
    'searchInput.bind-value-changed': 'onBindValueChanged_'
  },
  getSearchInput: function() {
    return this.$.searchInput;
  },
  isSearchFocused: function() {
    return this.$.searchTerm.focused;
  },
  computeIconTabIndex_: function(narrow) {
    return narrow ? 0 : -1;
  },
  isSpinnerShown_: function(spinnerActive, showingSearch) {
    return spinnerActive && showingSearch;
  },
  onInputBlur_: function() {
    if (!this.hasSearchText_) this.showingSearch = false;
  },
  onBindValueChanged_: function() {
    var newValue = this.$.searchInput.bindValue;
    this.hasSearchText_ = newValue != '';
    if (newValue != '') this.showingSearch = true;
  },
  showSearch_: function(e) {
    if (e.target != this.$.clearSearch) this.showingSearch = true;
  },
  hideSearch_: function(e) {
    this.showingSearch = false;
    e.stopPropagation();
  }
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'cr-toolbar',
  properties: {
    pageName: String,
    searchPrompt: String,
    clearLabel: String,
    menuLabel: String,
    spinnerActive: Boolean,
    showMenu: {
      type: Boolean,
      value: false
    },
    narrow_: {
      type: Boolean,
      reflectToAttribute: true
    },
    showingSearch_: {
      type: Boolean,
      reflectToAttribute: true
    }
  },
  getSearchField: function() {
    return this.$.search;
  },
  onMenuTap_: function(e) {
    this.fire('cr-menu-tap');
  }
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'history-toolbar',
  properties: {
    count: {
      type: Number,
      value: 0,
      observer: 'changeToolbarView_'
    },
    itemsSelected_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },
    searchTerm: {
      type: String,
      notify: true
    },
    spinnerActive: {
      type: Boolean,
      value: false
    },
    hasDrawer: {
      type: Boolean,
      observer: 'hasDrawerChanged_',
      reflectToAttribute: true
    },
    isGroupedMode: {
      type: Boolean,
      reflectToAttribute: true
    },
    groupedRange: {
      type: Number,
      value: 0,
      reflectToAttribute: true,
      notify: true
    },
    queryStartTime: String,
    queryEndTime: String
  },
  changeToolbarView_: function() {
    this.itemsSelected_ = this.count > 0;
  },
  setSearchTerm: function(search) {
    if (this.searchTerm == search) return;
    this.searchTerm = search;
    var searchField = this.$['main-toolbar'].getSearchField();
    searchField.showAndFocus();
    searchField.setValue(search);
  },
  onSearchChanged_: function(event) {
    this.searchTerm = event.detail;
  },
  onClearSelectionTap_: function() {
    this.fire('unselect-all');
  },
  onDeleteTap_: function() {
    this.fire('delete-selected');
  },
  get searchBar() {
    return this.$['main-toolbar'].getSearchField();
  },
  showSearchField: function() {
    this.$['main-toolbar'].getSearchField().showAndFocus();
  },
  deletingAllowed_: function() {
    return loadTimeData.getBoolean('allowDeletingHistory');
  },
  numberOfItemsSelected_: function(count) {
    return count > 0 ? loadTimeData.getStringF('itemsSelected', count) : '';
  },
  getHistoryInterval_: function(queryStartTime, queryEndTime) {
    return loadTimeData.getStringF('historyInterval', queryStartTime, queryEndTime);
  },
  hasDrawerChanged_: function() {
    this.updateStyles();
  }
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'cr-dialog',
  "extends": 'dialog',
  created: function() {
    window.addEventListener('popstate', function() {
      if (this.open) this.cancel();
    }.bind(this));
  },
  cancel: function() {
    this.fire('cancel');
    HTMLDialogElement.prototype.close.call(this, '');
  },
  close: function(opt_returnValue) {
    HTMLDialogElement.prototype.close.call(this, 'success');
  },
  getCloseButton: function() {
    return this.$.close;
  }
});

Polymer.IronFitBehavior = {
  properties: {
    sizingTarget: {
      type: Object,
      value: function() {
        return this;
      }
    },
    fitInto: {
      type: Object,
      value: window
    },
    noOverlap: {
      type: Boolean
    },
    positionTarget: {
      type: Element
    },
    horizontalAlign: {
      type: String
    },
    verticalAlign: {
      type: String
    },
    dynamicAlign: {
      type: Boolean
    },
    horizontalOffset: {
      type: Number,
      value: 0,
      notify: true
    },
    verticalOffset: {
      type: Number,
      value: 0,
      notify: true
    },
    autoFitOnAttach: {
      type: Boolean,
      value: false
    },
    _fitInfo: {
      type: Object
    }
  },
  get _fitWidth() {
    var fitWidth;
    if (this.fitInto === window) {
      fitWidth = this.fitInto.innerWidth;
    } else {
      fitWidth = this.fitInto.getBoundingClientRect().width;
    }
    return fitWidth;
  },
  get _fitHeight() {
    var fitHeight;
    if (this.fitInto === window) {
      fitHeight = this.fitInto.innerHeight;
    } else {
      fitHeight = this.fitInto.getBoundingClientRect().height;
    }
    return fitHeight;
  },
  get _fitLeft() {
    var fitLeft;
    if (this.fitInto === window) {
      fitLeft = 0;
    } else {
      fitLeft = this.fitInto.getBoundingClientRect().left;
    }
    return fitLeft;
  },
  get _fitTop() {
    var fitTop;
    if (this.fitInto === window) {
      fitTop = 0;
    } else {
      fitTop = this.fitInto.getBoundingClientRect().top;
    }
    return fitTop;
  },
  get _defaultPositionTarget() {
    var parent = Polymer.dom(this).parentNode;
    if (parent && parent.nodeType === Node.DOCUMENT_FRAGMENT_NODE) {
      parent = parent.host;
    }
    return parent;
  },
  get _localeHorizontalAlign() {
    if (this._isRTL) {
      if (this.horizontalAlign === 'right') {
        return 'left';
      }
      if (this.horizontalAlign === 'left') {
        return 'right';
      }
    }
    return this.horizontalAlign;
  },
  attached: function() {
    this._isRTL = window.getComputedStyle(this).direction == 'rtl';
    this.positionTarget = this.positionTarget || this._defaultPositionTarget;
    if (this.autoFitOnAttach) {
      if (window.getComputedStyle(this).display === 'none') {
        setTimeout(function() {
          this.fit();
        }.bind(this));
      } else {
        this.fit();
      }
    }
  },
  fit: function() {
    this.position();
    this.constrain();
    this.center();
  },
  _discoverInfo: function() {
    if (this._fitInfo) {
      return;
    }
    var target = window.getComputedStyle(this);
    var sizer = window.getComputedStyle(this.sizingTarget);
    this._fitInfo = {
      inlineStyle: {
        top: this.style.top || '',
        left: this.style.left || '',
        position: this.style.position || ''
      },
      sizerInlineStyle: {
        maxWidth: this.sizingTarget.style.maxWidth || '',
        maxHeight: this.sizingTarget.style.maxHeight || '',
        boxSizing: this.sizingTarget.style.boxSizing || ''
      },
      positionedBy: {
        vertically: target.top !== 'auto' ? 'top' : target.bottom !== 'auto' ? 'bottom' : null,
        horizontally: target.left !== 'auto' ? 'left' : target.right !== 'auto' ? 'right' : null
      },
      sizedBy: {
        height: sizer.maxHeight !== 'none',
        width: sizer.maxWidth !== 'none',
        minWidth: parseInt(sizer.minWidth, 10) || 0,
        minHeight: parseInt(sizer.minHeight, 10) || 0
      },
      margin: {
        top: parseInt(target.marginTop, 10) || 0,
        right: parseInt(target.marginRight, 10) || 0,
        bottom: parseInt(target.marginBottom, 10) || 0,
        left: parseInt(target.marginLeft, 10) || 0
      }
    };
    if (this.verticalOffset) {
      this._fitInfo.margin.top = this._fitInfo.margin.bottom = this.verticalOffset;
      this._fitInfo.inlineStyle.marginTop = this.style.marginTop || '';
      this._fitInfo.inlineStyle.marginBottom = this.style.marginBottom || '';
      this.style.marginTop = this.style.marginBottom = this.verticalOffset + 'px';
    }
    if (this.horizontalOffset) {
      this._fitInfo.margin.left = this._fitInfo.margin.right = this.horizontalOffset;
      this._fitInfo.inlineStyle.marginLeft = this.style.marginLeft || '';
      this._fitInfo.inlineStyle.marginRight = this.style.marginRight || '';
      this.style.marginLeft = this.style.marginRight = this.horizontalOffset + 'px';
    }
  },
  resetFit: function() {
    var info = this._fitInfo || {};
    for (var property in info.sizerInlineStyle) {
      this.sizingTarget.style[property] = info.sizerInlineStyle[property];
    }
    for (var property in info.inlineStyle) {
      this.style[property] = info.inlineStyle[property];
    }
    this._fitInfo = null;
  },
  refit: function() {
    var scrollLeft = this.sizingTarget.scrollLeft;
    var scrollTop = this.sizingTarget.scrollTop;
    this.resetFit();
    this.fit();
    this.sizingTarget.scrollLeft = scrollLeft;
    this.sizingTarget.scrollTop = scrollTop;
  },
  position: function() {
    if (!this.horizontalAlign && !this.verticalAlign) {
      return;
    }
    this._discoverInfo();
    this.style.position = 'fixed';
    this.sizingTarget.style.boxSizing = 'border-box';
    this.style.left = '0px';
    this.style.top = '0px';
    var rect = this.getBoundingClientRect();
    var positionRect = this.__getNormalizedRect(this.positionTarget);
    var fitRect = this.__getNormalizedRect(this.fitInto);
    var margin = this._fitInfo.margin;
    var size = {
      width: rect.width + margin.left + margin.right,
      height: rect.height + margin.top + margin.bottom
    };
    var position = this.__getPosition(this._localeHorizontalAlign, this.verticalAlign, size, positionRect, fitRect);
    var left = position.left + margin.left;
    var top = position.top + margin.top;
    var right = Math.min(fitRect.right - margin.right, left + rect.width);
    var bottom = Math.min(fitRect.bottom - margin.bottom, top + rect.height);
    var minWidth = this._fitInfo.sizedBy.minWidth;
    var minHeight = this._fitInfo.sizedBy.minHeight;
    if (left < margin.left) {
      left = margin.left;
      if (right - left < minWidth) {
        left = right - minWidth;
      }
    }
    if (top < margin.top) {
      top = margin.top;
      if (bottom - top < minHeight) {
        top = bottom - minHeight;
      }
    }
    this.sizingTarget.style.maxWidth = right - left + 'px';
    this.sizingTarget.style.maxHeight = bottom - top + 'px';
    this.style.left = left - rect.left + 'px';
    this.style.top = top - rect.top + 'px';
  },
  constrain: function() {
    if (this.horizontalAlign || this.verticalAlign) {
      return;
    }
    this._discoverInfo();
    var info = this._fitInfo;
    if (!info.positionedBy.vertically) {
      this.style.position = 'fixed';
      this.style.top = '0px';
    }
    if (!info.positionedBy.horizontally) {
      this.style.position = 'fixed';
      this.style.left = '0px';
    }
    this.sizingTarget.style.boxSizing = 'border-box';
    var rect = this.getBoundingClientRect();
    if (!info.sizedBy.height) {
      this.__sizeDimension(rect, info.positionedBy.vertically, 'top', 'bottom', 'Height');
    }
    if (!info.sizedBy.width) {
      this.__sizeDimension(rect, info.positionedBy.horizontally, 'left', 'right', 'Width');
    }
  },
  _sizeDimension: function(rect, positionedBy, start, end, extent) {
    this.__sizeDimension(rect, positionedBy, start, end, extent);
  },
  __sizeDimension: function(rect, positionedBy, start, end, extent) {
    var info = this._fitInfo;
    var fitRect = this.__getNormalizedRect(this.fitInto);
    var max = extent === 'Width' ? fitRect.width : fitRect.height;
    var flip = positionedBy === end;
    var offset = flip ? max - rect[end] : rect[start];
    var margin = info.margin[flip ? start : end];
    var offsetExtent = 'offset' + extent;
    var sizingOffset = this[offsetExtent] - this.sizingTarget[offsetExtent];
    this.sizingTarget.style['max' + extent] = max - margin - offset - sizingOffset + 'px';
  },
  center: function() {
    if (this.horizontalAlign || this.verticalAlign) {
      return;
    }
    this._discoverInfo();
    var positionedBy = this._fitInfo.positionedBy;
    if (positionedBy.vertically && positionedBy.horizontally) {
      return;
    }
    this.style.position = 'fixed';
    if (!positionedBy.vertically) {
      this.style.top = '0px';
    }
    if (!positionedBy.horizontally) {
      this.style.left = '0px';
    }
    var rect = this.getBoundingClientRect();
    var fitRect = this.__getNormalizedRect(this.fitInto);
    if (!positionedBy.vertically) {
      var top = fitRect.top - rect.top + (fitRect.height - rect.height) / 2;
      this.style.top = top + 'px';
    }
    if (!positionedBy.horizontally) {
      var left = fitRect.left - rect.left + (fitRect.width - rect.width) / 2;
      this.style.left = left + 'px';
    }
  },
  __getNormalizedRect: function(target) {
    if (target === document.documentElement || target === window) {
      return {
        top: 0,
        left: 0,
        width: window.innerWidth,
        height: window.innerHeight,
        right: window.innerWidth,
        bottom: window.innerHeight
      };
    }
    return target.getBoundingClientRect();
  },
  __getCroppedArea: function(position, size, fitRect) {
    var verticalCrop = Math.min(0, position.top) + Math.min(0, fitRect.bottom - (position.top + size.height));
    var horizontalCrop = Math.min(0, position.left) + Math.min(0, fitRect.right - (position.left + size.width));
    return Math.abs(verticalCrop) * size.width + Math.abs(horizontalCrop) * size.height;
  },
  __getPosition: function(hAlign, vAlign, size, positionRect, fitRect) {
    var positions = [ {
      verticalAlign: 'top',
      horizontalAlign: 'left',
      top: positionRect.top,
      left: positionRect.left
    }, {
      verticalAlign: 'top',
      horizontalAlign: 'right',
      top: positionRect.top,
      left: positionRect.right - size.width
    }, {
      verticalAlign: 'bottom',
      horizontalAlign: 'left',
      top: positionRect.bottom - size.height,
      left: positionRect.left
    }, {
      verticalAlign: 'bottom',
      horizontalAlign: 'right',
      top: positionRect.bottom - size.height,
      left: positionRect.right - size.width
    } ];
    if (this.noOverlap) {
      for (var i = 0, l = positions.length; i < l; i++) {
        var copy = {};
        for (var key in positions[i]) {
          copy[key] = positions[i][key];
        }
        positions.push(copy);
      }
      positions[0].top = positions[1].top += positionRect.height;
      positions[2].top = positions[3].top -= positionRect.height;
      positions[4].left = positions[6].left += positionRect.width;
      positions[5].left = positions[7].left -= positionRect.width;
    }
    vAlign = vAlign === 'auto' ? null : vAlign;
    hAlign = hAlign === 'auto' ? null : hAlign;
    var position;
    for (var i = 0; i < positions.length; i++) {
      var pos = positions[i];
      if (!this.dynamicAlign && !this.noOverlap && pos.verticalAlign === vAlign && pos.horizontalAlign === hAlign) {
        position = pos;
        break;
      }
      var alignOk = (!vAlign || pos.verticalAlign === vAlign) && (!hAlign || pos.horizontalAlign === hAlign);
      if (!this.dynamicAlign && !alignOk) {
        continue;
      }
      position = position || pos;
      pos.croppedArea = this.__getCroppedArea(pos, size, fitRect);
      var diff = pos.croppedArea - position.croppedArea;
      if (diff < 0 || diff === 0 && alignOk) {
        position = pos;
      }
      if (position.croppedArea === 0 && alignOk) {
        break;
      }
    }
    return position;
  }
};

(function() {
  'use strict';
  Polymer({
    is: 'iron-overlay-backdrop',
    properties: {
      opened: {
        reflectToAttribute: true,
        type: Boolean,
        value: false,
        observer: '_openedChanged'
      }
    },
    listeners: {
      transitionend: '_onTransitionend'
    },
    created: function() {
      this.__openedRaf = null;
    },
    attached: function() {
      this.opened && this._openedChanged(this.opened);
    },
    prepare: function() {
      if (this.opened && !this.parentNode) {
        Polymer.dom(document.body).appendChild(this);
      }
    },
    open: function() {
      this.opened = true;
    },
    close: function() {
      this.opened = false;
    },
    complete: function() {
      if (!this.opened && this.parentNode === document.body) {
        Polymer.dom(this.parentNode).removeChild(this);
      }
    },
    _onTransitionend: function(event) {
      if (event && event.target === this) {
        this.complete();
      }
    },
    _openedChanged: function(opened) {
      if (opened) {
        this.prepare();
      } else {
        var cs = window.getComputedStyle(this);
        if (cs.transitionDuration === '0s' || cs.opacity == 0) {
          this.complete();
        }
      }
      if (!this.isAttached) {
        return;
      }
      if (this.__openedRaf) {
        window.cancelAnimationFrame(this.__openedRaf);
        this.__openedRaf = null;
      }
      this.scrollTop = this.scrollTop;
      this.__openedRaf = window.requestAnimationFrame(function() {
        this.__openedRaf = null;
        this.toggleClass('opened', this.opened);
      }.bind(this));
    }
  });
})();

Polymer.IronOverlayManagerClass = function() {
  this._overlays = [];
  this._minimumZ = 101;
  this._backdropElement = null;
  Polymer.Gestures.add(document, 'tap', this._onCaptureClick.bind(this));
  document.addEventListener('focus', this._onCaptureFocus.bind(this), true);
  document.addEventListener('keydown', this._onCaptureKeyDown.bind(this), true);
};

Polymer.IronOverlayManagerClass.prototype = {
  constructor: Polymer.IronOverlayManagerClass,
  get backdropElement() {
    if (!this._backdropElement) {
      this._backdropElement = document.createElement('iron-overlay-backdrop');
    }
    return this._backdropElement;
  },
  get deepActiveElement() {
    var active = document.activeElement || document.body;
    while (active.root && Polymer.dom(active.root).activeElement) {
      active = Polymer.dom(active.root).activeElement;
    }
    return active;
  },
  _bringOverlayAtIndexToFront: function(i) {
    var overlay = this._overlays[i];
    if (!overlay) {
      return;
    }
    var lastI = this._overlays.length - 1;
    var currentOverlay = this._overlays[lastI];
    if (currentOverlay && this._shouldBeBehindOverlay(overlay, currentOverlay)) {
      lastI--;
    }
    if (i >= lastI) {
      return;
    }
    var minimumZ = Math.max(this.currentOverlayZ(), this._minimumZ);
    if (this._getZ(overlay) <= minimumZ) {
      this._applyOverlayZ(overlay, minimumZ);
    }
    while (i < lastI) {
      this._overlays[i] = this._overlays[i + 1];
      i++;
    }
    this._overlays[lastI] = overlay;
  },
  addOrRemoveOverlay: function(overlay) {
    if (overlay.opened) {
      this.addOverlay(overlay);
    } else {
      this.removeOverlay(overlay);
    }
  },
  addOverlay: function(overlay) {
    var i = this._overlays.indexOf(overlay);
    if (i >= 0) {
      this._bringOverlayAtIndexToFront(i);
      this.trackBackdrop();
      return;
    }
    var insertionIndex = this._overlays.length;
    var currentOverlay = this._overlays[insertionIndex - 1];
    var minimumZ = Math.max(this._getZ(currentOverlay), this._minimumZ);
    var newZ = this._getZ(overlay);
    if (currentOverlay && this._shouldBeBehindOverlay(overlay, currentOverlay)) {
      this._applyOverlayZ(currentOverlay, minimumZ);
      insertionIndex--;
      var previousOverlay = this._overlays[insertionIndex - 1];
      minimumZ = Math.max(this._getZ(previousOverlay), this._minimumZ);
    }
    if (newZ <= minimumZ) {
      this._applyOverlayZ(overlay, minimumZ);
    }
    this._overlays.splice(insertionIndex, 0, overlay);
    this.trackBackdrop();
  },
  removeOverlay: function(overlay) {
    var i = this._overlays.indexOf(overlay);
    if (i === -1) {
      return;
    }
    this._overlays.splice(i, 1);
    this.trackBackdrop();
  },
  currentOverlay: function() {
    var i = this._overlays.length - 1;
    return this._overlays[i];
  },
  currentOverlayZ: function() {
    return this._getZ(this.currentOverlay());
  },
  ensureMinimumZ: function(minimumZ) {
    this._minimumZ = Math.max(this._minimumZ, minimumZ);
  },
  focusOverlay: function() {
    var current = this.currentOverlay();
    if (current) {
      current._applyFocus();
    }
  },
  trackBackdrop: function() {
    var overlay = this._overlayWithBackdrop();
    if (!overlay && !this._backdropElement) {
      return;
    }
    this.backdropElement.style.zIndex = this._getZ(overlay) - 1;
    this.backdropElement.opened = !!overlay;
  },
  getBackdrops: function() {
    var backdrops = [];
    for (var i = 0; i < this._overlays.length; i++) {
      if (this._overlays[i].withBackdrop) {
        backdrops.push(this._overlays[i]);
      }
    }
    return backdrops;
  },
  backdropZ: function() {
    return this._getZ(this._overlayWithBackdrop()) - 1;
  },
  _overlayWithBackdrop: function() {
    for (var i = 0; i < this._overlays.length; i++) {
      if (this._overlays[i].withBackdrop) {
        return this._overlays[i];
      }
    }
  },
  _getZ: function(overlay) {
    var z = this._minimumZ;
    if (overlay) {
      var z1 = Number(overlay.style.zIndex || window.getComputedStyle(overlay).zIndex);
      if (z1 === z1) {
        z = z1;
      }
    }
    return z;
  },
  _setZ: function(element, z) {
    element.style.zIndex = z;
  },
  _applyOverlayZ: function(overlay, aboveZ) {
    this._setZ(overlay, aboveZ + 2);
  },
  _overlayInPath: function(path) {
    path = path || [];
    for (var i = 0; i < path.length; i++) {
      if (path[i]._manager === this) {
        return path[i];
      }
    }
  },
  _onCaptureClick: function(event) {
    var overlay = this.currentOverlay();
    if (overlay && this._overlayInPath(Polymer.dom(event).path) !== overlay) {
      overlay._onCaptureClick(event);
    }
  },
  _onCaptureFocus: function(event) {
    var overlay = this.currentOverlay();
    if (overlay) {
      overlay._onCaptureFocus(event);
    }
  },
  _onCaptureKeyDown: function(event) {
    var overlay = this.currentOverlay();
    if (overlay) {
      if (Polymer.IronA11yKeysBehavior.keyboardEventMatchesKeys(event, 'esc')) {
        overlay._onCaptureEsc(event);
      } else if (Polymer.IronA11yKeysBehavior.keyboardEventMatchesKeys(event, 'tab')) {
        overlay._onCaptureTab(event);
      }
    }
  },
  _shouldBeBehindOverlay: function(overlay1, overlay2) {
    return !overlay1.alwaysOnTop && overlay2.alwaysOnTop;
  }
};

Polymer.IronOverlayManager = new Polymer.IronOverlayManagerClass();

(function() {
  'use strict';
  Polymer.IronOverlayBehaviorImpl = {
    properties: {
      opened: {
        observer: '_openedChanged',
        type: Boolean,
        value: false,
        notify: true
      },
      canceled: {
        observer: '_canceledChanged',
        readOnly: true,
        type: Boolean,
        value: false
      },
      withBackdrop: {
        observer: '_withBackdropChanged',
        type: Boolean
      },
      noAutoFocus: {
        type: Boolean,
        value: false
      },
      noCancelOnEscKey: {
        type: Boolean,
        value: false
      },
      noCancelOnOutsideClick: {
        type: Boolean,
        value: false
      },
      closingReason: {
        type: Object
      },
      restoreFocusOnClose: {
        type: Boolean,
        value: false
      },
      alwaysOnTop: {
        type: Boolean
      },
      _manager: {
        type: Object,
        value: Polymer.IronOverlayManager
      },
      _focusedChild: {
        type: Object
      }
    },
    listeners: {
      'iron-resize': '_onIronResize'
    },
    get backdropElement() {
      return this._manager.backdropElement;
    },
    get _focusNode() {
      return this._focusedChild || Polymer.dom(this).querySelector('[autofocus]') || this;
    },
    get _focusableNodes() {
      var FOCUSABLE_WITH_DISABLED = [ 'a[href]', 'area[href]', 'iframe', '[tabindex]', '[contentEditable=true]' ];
      var FOCUSABLE_WITHOUT_DISABLED = [ 'input', 'select', 'textarea', 'button' ];
      var selector = FOCUSABLE_WITH_DISABLED.join(':not([tabindex="-1"]),') + ':not([tabindex="-1"]),' + FOCUSABLE_WITHOUT_DISABLED.join(':not([disabled]):not([tabindex="-1"]),') + ':not([disabled]):not([tabindex="-1"])';
      var focusables = Polymer.dom(this).querySelectorAll(selector);
      if (this.tabIndex >= 0) {
        focusables.splice(0, 0, this);
      }
      return focusables.sort(function(a, b) {
        if (a.tabIndex === b.tabIndex) {
          return 0;
        }
        if (a.tabIndex === 0 || a.tabIndex > b.tabIndex) {
          return 1;
        }
        return -1;
      });
    },
    ready: function() {
      this.__isAnimating = false;
      this.__shouldRemoveTabIndex = false;
      this.__firstFocusableNode = this.__lastFocusableNode = null;
      this.__raf = null;
      this.__restoreFocusNode = null;
      this._ensureSetup();
    },
    attached: function() {
      if (this.opened) {
        this._openedChanged(this.opened);
      }
      this._observer = Polymer.dom(this).observeNodes(this._onNodesChange);
    },
    detached: function() {
      Polymer.dom(this).unobserveNodes(this._observer);
      this._observer = null;
      if (this.__raf) {
        window.cancelAnimationFrame(this.__raf);
        this.__raf = null;
      }
      this._manager.removeOverlay(this);
    },
    toggle: function() {
      this._setCanceled(false);
      this.opened = !this.opened;
    },
    open: function() {
      this._setCanceled(false);
      this.opened = true;
    },
    close: function() {
      this._setCanceled(false);
      this.opened = false;
    },
    cancel: function(event) {
      var cancelEvent = this.fire('iron-overlay-canceled', event, {
        cancelable: true
      });
      if (cancelEvent.defaultPrevented) {
        return;
      }
      this._setCanceled(true);
      this.opened = false;
    },
    _ensureSetup: function() {
      if (this._overlaySetup) {
        return;
      }
      this._overlaySetup = true;
      this.style.outline = 'none';
      this.style.display = 'none';
    },
    _openedChanged: function(opened) {
      if (opened) {
        this.removeAttribute('aria-hidden');
      } else {
        this.setAttribute('aria-hidden', 'true');
      }
      if (!this.isAttached) {
        return;
      }
      this.__isAnimating = true;
      this.__onNextAnimationFrame(this.__openedChanged);
    },
    _canceledChanged: function() {
      this.closingReason = this.closingReason || {};
      this.closingReason.canceled = this.canceled;
    },
    _withBackdropChanged: function() {
      if (this.withBackdrop && !this.hasAttribute('tabindex')) {
        this.setAttribute('tabindex', '-1');
        this.__shouldRemoveTabIndex = true;
      } else if (this.__shouldRemoveTabIndex) {
        this.removeAttribute('tabindex');
        this.__shouldRemoveTabIndex = false;
      }
      if (this.opened && this.isAttached) {
        this._manager.trackBackdrop();
      }
    },
    _prepareRenderOpened: function() {
      this.__restoreFocusNode = this._manager.deepActiveElement;
      this._preparePositioning();
      this.refit();
      this._finishPositioning();
      if (this.noAutoFocus && document.activeElement === this._focusNode) {
        this._focusNode.blur();
        this.__restoreFocusNode.focus();
      }
    },
    _renderOpened: function() {
      this._finishRenderOpened();
    },
    _renderClosed: function() {
      this._finishRenderClosed();
    },
    _finishRenderOpened: function() {
      this.notifyResize();
      this.__isAnimating = false;
      var focusableNodes = this._focusableNodes;
      this.__firstFocusableNode = focusableNodes[0];
      this.__lastFocusableNode = focusableNodes[focusableNodes.length - 1];
      this.fire('iron-overlay-opened');
    },
    _finishRenderClosed: function() {
      this.style.display = 'none';
      this.style.zIndex = '';
      this.notifyResize();
      this.__isAnimating = false;
      this.fire('iron-overlay-closed', this.closingReason);
    },
    _preparePositioning: function() {
      this.style.transition = this.style.webkitTransition = 'none';
      this.style.transform = this.style.webkitTransform = 'none';
      this.style.display = '';
    },
    _finishPositioning: function() {
      this.style.display = 'none';
      this.scrollTop = this.scrollTop;
      this.style.transition = this.style.webkitTransition = '';
      this.style.transform = this.style.webkitTransform = '';
      this.style.display = '';
      this.scrollTop = this.scrollTop;
    },
    _applyFocus: function() {
      if (this.opened) {
        if (!this.noAutoFocus) {
          this._focusNode.focus();
        }
      } else {
        this._focusNode.blur();
        this._focusedChild = null;
        if (this.restoreFocusOnClose && this.__restoreFocusNode) {
          this.__restoreFocusNode.focus();
        }
        this.__restoreFocusNode = null;
        var currentOverlay = this._manager.currentOverlay();
        if (currentOverlay && this !== currentOverlay) {
          currentOverlay._applyFocus();
        }
      }
    },
    _onCaptureClick: function(event) {
      if (!this.noCancelOnOutsideClick) {
        this.cancel(event);
      }
    },
    _onCaptureFocus: function(event) {
      if (!this.withBackdrop) {
        return;
      }
      var path = Polymer.dom(event).path;
      if (path.indexOf(this) === -1) {
        event.stopPropagation();
        this._applyFocus();
      } else {
        this._focusedChild = path[0];
      }
    },
    _onCaptureEsc: function(event) {
      if (!this.noCancelOnEscKey) {
        this.cancel(event);
      }
    },
    _onCaptureTab: function(event) {
      if (!this.withBackdrop) {
        return;
      }
      var shift = event.shiftKey;
      var nodeToCheck = shift ? this.__firstFocusableNode : this.__lastFocusableNode;
      var nodeToSet = shift ? this.__lastFocusableNode : this.__firstFocusableNode;
      var shouldWrap = false;
      if (nodeToCheck === nodeToSet) {
        shouldWrap = true;
      } else {
        var focusedNode = this._manager.deepActiveElement;
        shouldWrap = focusedNode === nodeToCheck || focusedNode === this;
      }
      if (shouldWrap) {
        event.preventDefault();
        this._focusedChild = nodeToSet;
        this._applyFocus();
      }
    },
    _onIronResize: function() {
      if (this.opened && !this.__isAnimating) {
        this.__onNextAnimationFrame(this.refit);
      }
    },
    _onNodesChange: function() {
      if (this.opened && !this.__isAnimating) {
        this.notifyResize();
      }
    },
    __openedChanged: function() {
      if (this.opened) {
        this._prepareRenderOpened();
        this._manager.addOverlay(this);
        this._applyFocus();
        this._renderOpened();
      } else {
        this._manager.removeOverlay(this);
        this._applyFocus();
        this._renderClosed();
      }
    },
    __onNextAnimationFrame: function(callback) {
      if (this.__raf) {
        window.cancelAnimationFrame(this.__raf);
      }
      var self = this;
      this.__raf = window.requestAnimationFrame(function nextAnimationFrame() {
        self.__raf = null;
        callback.call(self);
      });
    }
  };
  Polymer.IronOverlayBehavior = [ Polymer.IronFitBehavior, Polymer.IronResizableBehavior, Polymer.IronOverlayBehaviorImpl ];
})();

Polymer.NeonAnimatableBehavior = {
  properties: {
    animationConfig: {
      type: Object
    },
    entryAnimation: {
      observer: '_entryAnimationChanged',
      type: String
    },
    exitAnimation: {
      observer: '_exitAnimationChanged',
      type: String
    }
  },
  _entryAnimationChanged: function() {
    this.animationConfig = this.animationConfig || {};
    this.animationConfig['entry'] = [ {
      name: this.entryAnimation,
      node: this
    } ];
  },
  _exitAnimationChanged: function() {
    this.animationConfig = this.animationConfig || {};
    this.animationConfig['exit'] = [ {
      name: this.exitAnimation,
      node: this
    } ];
  },
  _copyProperties: function(config1, config2) {
    for (var property in config2) {
      config1[property] = config2[property];
    }
  },
  _cloneConfig: function(config) {
    var clone = {
      isClone: true
    };
    this._copyProperties(clone, config);
    return clone;
  },
  _getAnimationConfigRecursive: function(type, map, allConfigs) {
    if (!this.animationConfig) {
      return;
    }
    if (this.animationConfig.value && typeof this.animationConfig.value === 'function') {
      this._warn(this._logf('playAnimation', "Please put 'animationConfig' inside of your components 'properties' object instead of outside of it."));
      return;
    }
    var thisConfig;
    if (type) {
      thisConfig = this.animationConfig[type];
    } else {
      thisConfig = this.animationConfig;
    }
    if (!Array.isArray(thisConfig)) {
      thisConfig = [ thisConfig ];
    }
    if (thisConfig) {
      for (var config, index = 0; config = thisConfig[index]; index++) {
        if (config.animatable) {
          config.animatable._getAnimationConfigRecursive(config.type || type, map, allConfigs);
        } else {
          if (config.id) {
            var cachedConfig = map[config.id];
            if (cachedConfig) {
              if (!cachedConfig.isClone) {
                map[config.id] = this._cloneConfig(cachedConfig);
                cachedConfig = map[config.id];
              }
              this._copyProperties(cachedConfig, config);
            } else {
              map[config.id] = config;
            }
          } else {
            allConfigs.push(config);
          }
        }
      }
    }
  },
  getAnimationConfig: function(type) {
    var map = {};
    var allConfigs = [];
    this._getAnimationConfigRecursive(type, map, allConfigs);
    for (var key in map) {
      allConfigs.push(map[key]);
    }
    return allConfigs;
  }
};

Polymer.NeonAnimationRunnerBehaviorImpl = {
  _configureAnimations: function(configs) {
    var results = [];
    if (configs.length > 0) {
      for (var config, index = 0; config = configs[index]; index++) {
        var neonAnimation = document.createElement(config.name);
        if (neonAnimation.isNeonAnimation) {
          var result = null;
          try {
            result = neonAnimation.configure(config);
            if (typeof result.cancel != 'function') {
              result = document.timeline.play(result);
            }
          } catch (e) {
            result = null;
            console.warn('Couldnt play', '(', config.name, ').', e);
          }
          if (result) {
            results.push({
              neonAnimation: neonAnimation,
              config: config,
              animation: result
            });
          }
        } else {
          console.warn(this.is + ':', config.name, 'not found!');
        }
      }
    }
    return results;
  },
  _shouldComplete: function(activeEntries) {
    var finished = true;
    for (var i = 0; i < activeEntries.length; i++) {
      if (activeEntries[i].animation.playState != 'finished') {
        finished = false;
        break;
      }
    }
    return finished;
  },
  _complete: function(activeEntries) {
    for (var i = 0; i < activeEntries.length; i++) {
      activeEntries[i].neonAnimation.complete(activeEntries[i].config);
    }
    for (var i = 0; i < activeEntries.length; i++) {
      activeEntries[i].animation.cancel();
    }
  },
  playAnimation: function(type, cookie) {
    var configs = this.getAnimationConfig(type);
    if (!configs) {
      return;
    }
    this._active = this._active || {};
    if (this._active[type]) {
      this._complete(this._active[type]);
      delete this._active[type];
    }
    var activeEntries = this._configureAnimations(configs);
    if (activeEntries.length == 0) {
      this.fire('neon-animation-finish', cookie, {
        bubbles: false
      });
      return;
    }
    this._active[type] = activeEntries;
    for (var i = 0; i < activeEntries.length; i++) {
      activeEntries[i].animation.onfinish = function() {
        if (this._shouldComplete(activeEntries)) {
          this._complete(activeEntries);
          delete this._active[type];
          this.fire('neon-animation-finish', cookie, {
            bubbles: false
          });
        }
      }.bind(this);
    }
  },
  cancelAnimation: function() {
    for (var k in this._animations) {
      this._animations[k].cancel();
    }
    this._animations = {};
  }
};

Polymer.NeonAnimationRunnerBehavior = [ Polymer.NeonAnimatableBehavior, Polymer.NeonAnimationRunnerBehaviorImpl ];

Polymer.NeonAnimationBehavior = {
  properties: {
    animationTiming: {
      type: Object,
      value: function() {
        return {
          duration: 500,
          easing: 'cubic-bezier(0.4, 0, 0.2, 1)',
          fill: 'both'
        };
      }
    }
  },
  isNeonAnimation: true,
  timingFromConfig: function(config) {
    if (config.timing) {
      for (var property in config.timing) {
        this.animationTiming[property] = config.timing[property];
      }
    }
    return this.animationTiming;
  },
  setPrefixedProperty: function(node, property, value) {
    var map = {
      transform: [ 'webkitTransform' ],
      transformOrigin: [ 'mozTransformOrigin', 'webkitTransformOrigin' ]
    };
    var prefixes = map[property];
    for (var prefix, index = 0; prefix = prefixes[index]; index++) {
      node.style[prefix] = value;
    }
    node.style[property] = value;
  },
  complete: function() {}
};

Polymer({
  is: 'opaque-animation',
  behaviors: [ Polymer.NeonAnimationBehavior ],
  configure: function(config) {
    var node = config.node;
    this._effect = new KeyframeEffect(node, [ {
      opacity: '1'
    }, {
      opacity: '1'
    } ], this.timingFromConfig(config));
    node.style.opacity = '0';
    return this._effect;
  },
  complete: function(config) {
    config.node.style.opacity = '';
  }
});

(function() {
  'use strict';
  var LAST_TOUCH_POSITION = {
    pageX: 0,
    pageY: 0
  };
  var ROOT_TARGET = null;
  var SCROLLABLE_NODES = [];
  Polymer.IronDropdownScrollManager = {
    get currentLockingElement() {
      return this._lockingElements[this._lockingElements.length - 1];
    },
    elementIsScrollLocked: function(element) {
      var currentLockingElement = this.currentLockingElement;
      if (currentLockingElement === undefined) return false;
      var scrollLocked;
      if (this._hasCachedLockedElement(element)) {
        return true;
      }
      if (this._hasCachedUnlockedElement(element)) {
        return false;
      }
      scrollLocked = !!currentLockingElement && currentLockingElement !== element && !this._composedTreeContains(currentLockingElement, element);
      if (scrollLocked) {
        this._lockedElementCache.push(element);
      } else {
        this._unlockedElementCache.push(element);
      }
      return scrollLocked;
    },
    pushScrollLock: function(element) {
      if (this._lockingElements.indexOf(element) >= 0) {
        return;
      }
      if (this._lockingElements.length === 0) {
        this._lockScrollInteractions();
      }
      this._lockingElements.push(element);
      this._lockedElementCache = [];
      this._unlockedElementCache = [];
    },
    removeScrollLock: function(element) {
      var index = this._lockingElements.indexOf(element);
      if (index === -1) {
        return;
      }
      this._lockingElements.splice(index, 1);
      this._lockedElementCache = [];
      this._unlockedElementCache = [];
      if (this._lockingElements.length === 0) {
        this._unlockScrollInteractions();
      }
    },
    _lockingElements: [],
    _lockedElementCache: null,
    _unlockedElementCache: null,
    _hasCachedLockedElement: function(element) {
      return this._lockedElementCache.indexOf(element) > -1;
    },
    _hasCachedUnlockedElement: function(element) {
      return this._unlockedElementCache.indexOf(element) > -1;
    },
    _composedTreeContains: function(element, child) {
      var contentElements;
      var distributedNodes;
      var contentIndex;
      var nodeIndex;
      if (element.contains(child)) {
        return true;
      }
      contentElements = Polymer.dom(element).querySelectorAll('content');
      for (contentIndex = 0; contentIndex < contentElements.length; ++contentIndex) {
        distributedNodes = Polymer.dom(contentElements[contentIndex]).getDistributedNodes();
        for (nodeIndex = 0; nodeIndex < distributedNodes.length; ++nodeIndex) {
          if (this._composedTreeContains(distributedNodes[nodeIndex], child)) {
            return true;
          }
        }
      }
      return false;
    },
    _scrollInteractionHandler: function(event) {
      if (event.cancelable && this._shouldPreventScrolling(event)) {
        event.preventDefault();
      }
      if (event.targetTouches) {
        var touch = event.targetTouches[0];
        LAST_TOUCH_POSITION.pageX = touch.pageX;
        LAST_TOUCH_POSITION.pageY = touch.pageY;
      }
    },
    _lockScrollInteractions: function() {
      this._boundScrollHandler = this._boundScrollHandler || this._scrollInteractionHandler.bind(this);
      document.addEventListener('wheel', this._boundScrollHandler, true);
      document.addEventListener('mousewheel', this._boundScrollHandler, true);
      document.addEventListener('DOMMouseScroll', this._boundScrollHandler, true);
      document.addEventListener('touchstart', this._boundScrollHandler, true);
      document.addEventListener('touchmove', this._boundScrollHandler, true);
    },
    _unlockScrollInteractions: function() {
      document.removeEventListener('wheel', this._boundScrollHandler, true);
      document.removeEventListener('mousewheel', this._boundScrollHandler, true);
      document.removeEventListener('DOMMouseScroll', this._boundScrollHandler, true);
      document.removeEventListener('touchstart', this._boundScrollHandler, true);
      document.removeEventListener('touchmove', this._boundScrollHandler, true);
    },
    _shouldPreventScrolling: function(event) {
      var target = Polymer.dom(event).rootTarget;
      if (event.type !== 'touchmove' && ROOT_TARGET !== target) {
        ROOT_TARGET = target;
        SCROLLABLE_NODES = this._getScrollableNodes(Polymer.dom(event).path);
      }
      if (!SCROLLABLE_NODES.length) {
        return true;
      }
      if (event.type === 'touchstart') {
        return false;
      }
      var info = this._getScrollInfo(event);
      return !this._getScrollingNode(SCROLLABLE_NODES, info.deltaX, info.deltaY);
    },
    _getScrollableNodes: function(nodes) {
      var scrollables = [];
      var lockingIndex = nodes.indexOf(this.currentLockingElement);
      for (var i = 0; i <= lockingIndex; i++) {
        var node = nodes[i];
        if (node.nodeType === 11) {
          continue;
        }
        var style = node.style;
        if (style.overflow !== 'scroll' && style.overflow !== 'auto') {
          style = window.getComputedStyle(node);
        }
        if (style.overflow === 'scroll' || style.overflow === 'auto') {
          scrollables.push(node);
        }
      }
      return scrollables;
    },
    _getScrollingNode: function(nodes, deltaX, deltaY) {
      if (!deltaX && !deltaY) {
        return;
      }
      var verticalScroll = Math.abs(deltaY) >= Math.abs(deltaX);
      for (var i = 0; i < nodes.length; i++) {
        var node = nodes[i];
        var canScroll = false;
        if (verticalScroll) {
          canScroll = deltaY < 0 ? node.scrollTop > 0 : node.scrollTop < node.scrollHeight - node.clientHeight;
        } else {
          canScroll = deltaX < 0 ? node.scrollLeft > 0 : node.scrollLeft < node.scrollWidth - node.clientWidth;
        }
        if (canScroll) {
          return node;
        }
      }
    },
    _getScrollInfo: function(event) {
      var info = {
        deltaX: event.deltaX,
        deltaY: event.deltaY
      };
      if ('deltaX' in event) {} else if ('wheelDeltaX' in event) {
        info.deltaX = -event.wheelDeltaX;
        info.deltaY = -event.wheelDeltaY;
      } else if ('axis' in event) {
        info.deltaX = event.axis === 1 ? event.detail : 0;
        info.deltaY = event.axis === 2 ? event.detail : 0;
      } else if (event.targetTouches) {
        var touch = event.targetTouches[0];
        info.deltaX = LAST_TOUCH_POSITION.pageX - touch.pageX;
        info.deltaY = LAST_TOUCH_POSITION.pageY - touch.pageY;
      }
      return info;
    }
  };
})();

(function() {
  'use strict';
  Polymer({
    is: 'iron-dropdown',
    behaviors: [ Polymer.IronControlState, Polymer.IronA11yKeysBehavior, Polymer.IronOverlayBehavior, Polymer.NeonAnimationRunnerBehavior ],
    properties: {
      horizontalAlign: {
        type: String,
        value: 'left',
        reflectToAttribute: true
      },
      verticalAlign: {
        type: String,
        value: 'top',
        reflectToAttribute: true
      },
      openAnimationConfig: {
        type: Object
      },
      closeAnimationConfig: {
        type: Object
      },
      focusTarget: {
        type: Object
      },
      noAnimations: {
        type: Boolean,
        value: false
      },
      allowOutsideScroll: {
        type: Boolean,
        value: false
      },
      _boundOnCaptureScroll: {
        type: Function,
        value: function() {
          return this._onCaptureScroll.bind(this);
        }
      }
    },
    listeners: {
      'neon-animation-finish': '_onNeonAnimationFinish'
    },
    observers: [ '_updateOverlayPosition(positionTarget, verticalAlign, horizontalAlign, verticalOffset, horizontalOffset)' ],
    get containedElement() {
      return Polymer.dom(this.$.content).getDistributedNodes()[0];
    },
    get _focusTarget() {
      return this.focusTarget || this.containedElement;
    },
    ready: function() {
      this._scrollTop = 0;
      this._scrollLeft = 0;
      this._refitOnScrollRAF = null;
    },
    detached: function() {
      this.cancelAnimation();
      Polymer.IronDropdownScrollManager.removeScrollLock(this);
    },
    _openedChanged: function() {
      if (this.opened && this.disabled) {
        this.cancel();
      } else {
        this.cancelAnimation();
        this.sizingTarget = this.containedElement || this.sizingTarget;
        this._updateAnimationConfig();
        this._saveScrollPosition();
        if (this.opened) {
          document.addEventListener('scroll', this._boundOnCaptureScroll);
          !this.allowOutsideScroll && Polymer.IronDropdownScrollManager.pushScrollLock(this);
        } else {
          document.removeEventListener('scroll', this._boundOnCaptureScroll);
          Polymer.IronDropdownScrollManager.removeScrollLock(this);
        }
        Polymer.IronOverlayBehaviorImpl._openedChanged.apply(this, arguments);
      }
    },
    _renderOpened: function() {
      if (!this.noAnimations && this.animationConfig.open) {
        this.$.contentWrapper.classList.add('animating');
        this.playAnimation('open');
      } else {
        Polymer.IronOverlayBehaviorImpl._renderOpened.apply(this, arguments);
      }
    },
    _renderClosed: function() {
      if (!this.noAnimations && this.animationConfig.close) {
        this.$.contentWrapper.classList.add('animating');
        this.playAnimation('close');
      } else {
        Polymer.IronOverlayBehaviorImpl._renderClosed.apply(this, arguments);
      }
    },
    _onNeonAnimationFinish: function() {
      this.$.contentWrapper.classList.remove('animating');
      if (this.opened) {
        this._finishRenderOpened();
      } else {
        this._finishRenderClosed();
      }
    },
    _onCaptureScroll: function() {
      if (!this.allowOutsideScroll) {
        this._restoreScrollPosition();
      } else {
        this._refitOnScrollRAF && window.cancelAnimationFrame(this._refitOnScrollRAF);
        this._refitOnScrollRAF = window.requestAnimationFrame(this.refit.bind(this));
      }
    },
    _saveScrollPosition: function() {
      if (document.scrollingElement) {
        this._scrollTop = document.scrollingElement.scrollTop;
        this._scrollLeft = document.scrollingElement.scrollLeft;
      } else {
        this._scrollTop = Math.max(document.documentElement.scrollTop, document.body.scrollTop);
        this._scrollLeft = Math.max(document.documentElement.scrollLeft, document.body.scrollLeft);
      }
    },
    _restoreScrollPosition: function() {
      if (document.scrollingElement) {
        document.scrollingElement.scrollTop = this._scrollTop;
        document.scrollingElement.scrollLeft = this._scrollLeft;
      } else {
        document.documentElement.scrollTop = this._scrollTop;
        document.documentElement.scrollLeft = this._scrollLeft;
        document.body.scrollTop = this._scrollTop;
        document.body.scrollLeft = this._scrollLeft;
      }
    },
    _updateAnimationConfig: function() {
      var animations = (this.openAnimationConfig || []).concat(this.closeAnimationConfig || []);
      for (var i = 0; i < animations.length; i++) {
        animations[i].node = this.containedElement;
      }
      this.animationConfig = {
        open: this.openAnimationConfig,
        close: this.closeAnimationConfig
      };
    },
    _updateOverlayPosition: function() {
      if (this.isAttached) {
        this.notifyResize();
      }
    },
    _applyFocus: function() {
      var focusTarget = this.focusTarget || this.containedElement;
      if (focusTarget && this.opened && !this.noAutoFocus) {
        focusTarget.focus();
      } else {
        Polymer.IronOverlayBehaviorImpl._applyFocus.apply(this, arguments);
      }
    }
  });
})();

Polymer({
  is: 'fade-in-animation',
  behaviors: [ Polymer.NeonAnimationBehavior ],
  configure: function(config) {
    var node = config.node;
    this._effect = new KeyframeEffect(node, [ {
      opacity: '0'
    }, {
      opacity: '1'
    } ], this.timingFromConfig(config));
    return this._effect;
  }
});

Polymer({
  is: 'fade-out-animation',
  behaviors: [ Polymer.NeonAnimationBehavior ],
  configure: function(config) {
    var node = config.node;
    this._effect = new KeyframeEffect(node, [ {
      opacity: '1'
    }, {
      opacity: '0'
    } ], this.timingFromConfig(config));
    return this._effect;
  }
});

Polymer({
  is: 'paper-menu-grow-height-animation',
  behaviors: [ Polymer.NeonAnimationBehavior ],
  configure: function(config) {
    var node = config.node;
    var rect = node.getBoundingClientRect();
    var height = rect.height;
    this._effect = new KeyframeEffect(node, [ {
      height: height / 2 + 'px'
    }, {
      height: height + 'px'
    } ], this.timingFromConfig(config));
    return this._effect;
  }
});

Polymer({
  is: 'paper-menu-grow-width-animation',
  behaviors: [ Polymer.NeonAnimationBehavior ],
  configure: function(config) {
    var node = config.node;
    var rect = node.getBoundingClientRect();
    var width = rect.width;
    this._effect = new KeyframeEffect(node, [ {
      width: width / 2 + 'px'
    }, {
      width: width + 'px'
    } ], this.timingFromConfig(config));
    return this._effect;
  }
});

Polymer({
  is: 'paper-menu-shrink-width-animation',
  behaviors: [ Polymer.NeonAnimationBehavior ],
  configure: function(config) {
    var node = config.node;
    var rect = node.getBoundingClientRect();
    var width = rect.width;
    this._effect = new KeyframeEffect(node, [ {
      width: width + 'px'
    }, {
      width: width - width / 20 + 'px'
    } ], this.timingFromConfig(config));
    return this._effect;
  }
});

Polymer({
  is: 'paper-menu-shrink-height-animation',
  behaviors: [ Polymer.NeonAnimationBehavior ],
  configure: function(config) {
    var node = config.node;
    var rect = node.getBoundingClientRect();
    var height = rect.height;
    var top = rect.top;
    this.setPrefixedProperty(node, 'transformOrigin', '0 0');
    this._effect = new KeyframeEffect(node, [ {
      height: height + 'px',
      transform: 'translateY(0)'
    }, {
      height: height / 2 + 'px',
      transform: 'translateY(-20px)'
    } ], this.timingFromConfig(config));
    return this._effect;
  }
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var SLIDE_CUBIC_BEZIER = 'cubic-bezier(0.3, 0.95, 0.5, 1)';

Polymer({
  is: 'cr-shared-menu',
  behaviors: [ Polymer.IronA11yKeysBehavior ],
  properties: {
    menuOpen: {
      type: Boolean,
      observer: 'menuOpenChanged_',
      value: false,
      notify: true
    },
    itemData: {
      type: Object,
      value: null
    },
    keyEventTarget: {
      type: Object,
      value: function() {
        return this.$.menu;
      }
    },
    openAnimationConfig: {
      type: Object,
      value: function() {
        return [ {
          name: 'fade-in-animation',
          timing: {
            delay: 50,
            duration: 200
          }
        }, {
          name: 'paper-menu-grow-width-animation',
          timing: {
            delay: 50,
            duration: 150,
            easing: SLIDE_CUBIC_BEZIER
          }
        }, {
          name: 'paper-menu-grow-height-animation',
          timing: {
            delay: 100,
            duration: 275,
            easing: SLIDE_CUBIC_BEZIER
          }
        } ];
      }
    },
    closeAnimationConfig: {
      type: Object,
      value: function() {
        return [ {
          name: 'fade-out-animation',
          timing: {
            duration: 150
          }
        } ];
      }
    }
  },
  keyBindings: {
    tab: 'onTabPressed_'
  },
  listeners: {
    'dropdown.iron-overlay-canceled': 'onOverlayCanceled_'
  },
  lastAnchor_: null,
  firstFocus_: null,
  lastFocus_: null,
  attached: function() {
    window.addEventListener('resize', this.closeMenu.bind(this));
  },
  closeMenu: function() {
    if (this.root.activeElement == null) {
      this.$.dropdown.restoreFocusOnClose = false;
    }
    this.menuOpen = false;
  },
  openMenu: function(anchor, itemData) {
    if (this.lastAnchor_ == anchor && this.menuOpen) return;
    if (this.menuOpen) this.closeMenu();
    this.itemData = itemData;
    this.lastAnchor_ = anchor;
    this.$.dropdown.restoreFocusOnClose = true;
    var focusableChildren = Polymer.dom(this).querySelectorAll('[tabindex]:not([hidden]),button:not([hidden])');
    if (focusableChildren.length > 0) {
      this.$.dropdown.focusTarget = focusableChildren[0];
      this.firstFocus_ = focusableChildren[0];
      this.lastFocus_ = focusableChildren[focusableChildren.length - 1];
    }
    this.$.dropdown.positionTarget = anchor;
    this.menuOpen = true;
  },
  toggleMenu: function(anchor, itemData) {
    if (anchor == this.lastAnchor_ && this.menuOpen) this.closeMenu(); else this.openMenu(anchor, itemData);
  },
  onTabPressed_: function(e) {
    if (!this.firstFocus_ || !this.lastFocus_) return;
    var toFocus;
    var keyEvent = e.detail.keyboardEvent;
    if (keyEvent.shiftKey && keyEvent.target == this.firstFocus_) toFocus = this.lastFocus_; else if (keyEvent.target == this.lastFocus_) toFocus = this.firstFocus_;
    if (!toFocus) return;
    e.preventDefault();
    toFocus.focus();
  },
  menuOpenChanged_: function() {
    if (!this.menuOpen) {
      this.itemData = null;
      this.lastAnchor_ = null;
    }
  },
  onOverlayCanceled_: function(e) {
    if (e.detail.type == 'tap') this.$.dropdown.restoreFocusOnClose = false;
  }
});

Polymer.PaperItemBehaviorImpl = {
  hostAttributes: {
    role: 'option',
    tabindex: '0'
  }
};

Polymer.PaperItemBehavior = [ Polymer.IronButtonState, Polymer.IronControlState, Polymer.PaperItemBehaviorImpl ];

Polymer({
  is: 'paper-item',
  behaviors: [ Polymer.PaperItemBehavior ]
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('md_history', function() {
  function BrowserService() {
    this.pendingDeleteItems_ = null;
    this.pendingDeletePromise_ = null;
  }
  BrowserService.prototype = {
    deleteItems: function(items) {
      if (this.pendingDeleteItems_ != null) {
        return new Promise(function(resolve, reject) {
          reject(items);
        });
      }
      var removalList = items.map(function(item) {
        return {
          url: item.url,
          timestamps: item.allTimestamps
        };
      });
      this.pendingDeleteItems_ = items;
      this.pendingDeletePromise_ = new PromiseResolver();
      chrome.send('removeVisits', removalList);
      return this.pendingDeletePromise_.promise;
    },
    removeBookmark: function(url) {
      chrome.send('removeBookmark', [ url ]);
    },
    openForeignSessionAllTabs: function(sessionTag) {
      chrome.send('openForeignSession', [ sessionTag ]);
    },
    openForeignSessionTab: function(sessionTag, windowId, tabId, e) {
      chrome.send('openForeignSession', [ sessionTag, String(windowId), String(tabId), e.button || 0, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey ]);
    },
    deleteForeignSession: function(sessionTag) {
      chrome.send('deleteForeignSession', [ sessionTag ]);
    },
    openClearBrowsingData: function() {
      chrome.send('clearBrowsingData');
    },
    recordHistogram: function(histogram, value, max) {
      chrome.send('metricsHandler:recordInHistogram', [ histogram, value, max ]);
    },
    recordAction: function(action) {
      if (action.indexOf('_') == -1) action = 'HistoryPage_' + action;
      chrome.send('metricsHandler:recordAction', [ action ]);
    },
    resolveDelete_: function(successful) {
      if (this.pendingDeleteItems_ == null || this.pendingDeletePromise_ == null) {
        return;
      }
      if (successful) this.pendingDeletePromise_.resolve(this.pendingDeleteItems_); else this.pendingDeletePromise_.reject(this.pendingDeleteItems_);
      this.pendingDeleteItems_ = null;
      this.pendingDeletePromise_ = null;
    }
  };
  cr.addSingletonGetter(BrowserService);
  return {
    BrowserService: BrowserService
  };
});

function deleteComplete() {
  md_history.BrowserService.getInstance().resolveDelete_(true);
}

function deleteFailed() {
  md_history.BrowserService.getInstance().resolveDelete_(false);
}

Polymer({
  is: 'iron-collapse',
  behaviors: [ Polymer.IronResizableBehavior ],
  properties: {
    horizontal: {
      type: Boolean,
      value: false,
      observer: '_horizontalChanged'
    },
    opened: {
      type: Boolean,
      value: false,
      notify: true,
      observer: '_openedChanged'
    },
    noAnimation: {
      type: Boolean
    }
  },
  get dimension() {
    return this.horizontal ? 'width' : 'height';
  },
  get _dimensionMax() {
    return this.horizontal ? 'maxWidth' : 'maxHeight';
  },
  get _dimensionMaxCss() {
    return this.horizontal ? 'max-width' : 'max-height';
  },
  hostAttributes: {
    role: 'group',
    'aria-hidden': 'true',
    'aria-expanded': 'false'
  },
  listeners: {
    transitionend: '_transitionEnd'
  },
  attached: function() {
    this._transitionEnd();
  },
  toggle: function() {
    this.opened = !this.opened;
  },
  show: function() {
    this.opened = true;
  },
  hide: function() {
    this.opened = false;
  },
  updateSize: function(size, animated) {
    var curSize = this.style[this._dimensionMax];
    if (curSize === size || size === 'auto' && !curSize) {
      return;
    }
    this._updateTransition(false);
    if (animated && !this.noAnimation && this._isDisplayed) {
      var startSize = this._calcSize();
      if (size === 'auto') {
        this.style[this._dimensionMax] = '';
        size = this._calcSize();
      }
      this.style[this._dimensionMax] = startSize;
      this.scrollTop = this.scrollTop;
      this._updateTransition(true);
    }
    if (size === 'auto') {
      this.style[this._dimensionMax] = '';
    } else {
      this.style[this._dimensionMax] = size;
    }
  },
  enableTransition: function(enabled) {
    Polymer.Base._warn('`enableTransition()` is deprecated, use `noAnimation` instead.');
    this.noAnimation = !enabled;
  },
  _updateTransition: function(enabled) {
    this.style.transitionDuration = enabled && !this.noAnimation ? '' : '0s';
  },
  _horizontalChanged: function() {
    this.style.transitionProperty = this._dimensionMaxCss;
    var otherDimension = this._dimensionMax === 'maxWidth' ? 'maxHeight' : 'maxWidth';
    this.style[otherDimension] = '';
    this.updateSize(this.opened ? 'auto' : '0px', false);
  },
  _openedChanged: function() {
    this.setAttribute('aria-expanded', this.opened);
    this.setAttribute('aria-hidden', !this.opened);
    this.toggleClass('iron-collapse-closed', false);
    this.toggleClass('iron-collapse-opened', false);
    this.updateSize(this.opened ? 'auto' : '0px', true);
    if (this.opened) {
      this.focus();
    }
    if (this.noAnimation) {
      this._transitionEnd();
    }
  },
  _transitionEnd: function() {
    if (this.opened) {
      this.style[this._dimensionMax] = '';
    }
    this.toggleClass('iron-collapse-closed', !this.opened);
    this.toggleClass('iron-collapse-opened', this.opened);
    this._updateTransition(false);
    this.notifyResize();
  },
  get _isDisplayed() {
    var rect = this.getBoundingClientRect();
    for (var prop in rect) {
      if (rect[prop] !== 0) return true;
    }
    return false;
  },
  _calcSize: function() {
    return this.getBoundingClientRect()[this.dimension] + 'px';
  }
});

Polymer.IronFormElementBehavior = {
  properties: {
    name: {
      type: String
    },
    value: {
      notify: true,
      type: String
    },
    required: {
      type: Boolean,
      value: false
    },
    _parentForm: {
      type: Object
    }
  },
  attached: function() {
    this.fire('iron-form-element-register');
  },
  detached: function() {
    if (this._parentForm) {
      this._parentForm.fire('iron-form-element-unregister', {
        target: this
      });
    }
  }
};

Polymer.IronCheckedElementBehaviorImpl = {
  properties: {
    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      notify: true,
      observer: '_checkedChanged'
    },
    toggles: {
      type: Boolean,
      value: true,
      reflectToAttribute: true
    },
    value: {
      type: String,
      value: 'on',
      observer: '_valueChanged'
    }
  },
  observers: [ '_requiredChanged(required)' ],
  created: function() {
    this._hasIronCheckedElementBehavior = true;
  },
  _getValidity: function(_value) {
    return this.disabled || !this.required || this.checked;
  },
  _requiredChanged: function() {
    if (this.required) {
      this.setAttribute('aria-required', 'true');
    } else {
      this.removeAttribute('aria-required');
    }
  },
  _checkedChanged: function() {
    this.active = this.checked;
    this.fire('iron-change');
  },
  _valueChanged: function() {
    if (this.value === undefined || this.value === null) {
      this.value = 'on';
    }
  }
};

Polymer.IronCheckedElementBehavior = [ Polymer.IronFormElementBehavior, Polymer.IronValidatableBehavior, Polymer.IronCheckedElementBehaviorImpl ];

Polymer.PaperCheckedElementBehaviorImpl = {
  _checkedChanged: function() {
    Polymer.IronCheckedElementBehaviorImpl._checkedChanged.call(this);
    if (this.hasRipple()) {
      if (this.checked) {
        this._ripple.setAttribute('checked', '');
      } else {
        this._ripple.removeAttribute('checked');
      }
    }
  },
  _buttonStateChanged: function() {
    Polymer.PaperRippleBehavior._buttonStateChanged.call(this);
    if (this.disabled) {
      return;
    }
    if (this.isAttached) {
      this.checked = this.active;
    }
  }
};

Polymer.PaperCheckedElementBehavior = [ Polymer.PaperInkyFocusBehavior, Polymer.IronCheckedElementBehavior, Polymer.PaperCheckedElementBehaviorImpl ];

Polymer({
  is: 'paper-checkbox',
  behaviors: [ Polymer.PaperCheckedElementBehavior ],
  hostAttributes: {
    role: 'checkbox',
    'aria-checked': false,
    tabindex: 0
  },
  properties: {
    ariaActiveAttribute: {
      type: String,
      value: 'aria-checked'
    }
  },
  _computeCheckboxClass: function(checked, invalid) {
    var className = '';
    if (checked) {
      className += 'checked ';
    }
    if (invalid) {
      className += 'invalid';
    }
    return className;
  },
  _computeCheckmarkClass: function(checked) {
    return checked ? '' : 'hidden';
  },
  _createRipple: function() {
    this._rippleContainer = this.$.checkboxContainer;
    return Polymer.PaperInkyFocusBehaviorImpl._createRipple.call(this);
  }
});

Polymer({
  is: 'paper-icon-button-light',
  "extends": 'button',
  behaviors: [ Polymer.PaperRippleBehavior ],
  listeners: {
    down: '_rippleDown',
    up: '_rippleUp',
    focus: '_rippleDown',
    blur: '_rippleUp'
  },
  _rippleDown: function() {
    this.getRipple().downAction();
  },
  _rippleUp: function() {
    this.getRipple().upAction();
  },
  ensureRipple: function(var_args) {
    var lastRipple = this._ripple;
    Polymer.PaperRippleBehavior.ensureRipple.apply(this, arguments);
    if (this._ripple && this._ripple !== lastRipple) {
      this._ripple.center = true;
      this._ripple.classList.add('circle');
    }
  }
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('cr.icon', function() {
  function getSupportedScaleFactors() {
    var supportedScaleFactors = [];
    if (cr.isMac || cr.isChromeOS || cr.isWindows || cr.isLinux) {
      supportedScaleFactors.push(1);
      supportedScaleFactors.push(2);
    } else {
      supportedScaleFactors.push(window.devicePixelRatio);
    }
    return supportedScaleFactors;
  }
  function getProfileAvatarIcon(path) {
    var chromeThemePath = 'chrome://theme';
    var isDefaultAvatar = path.slice(0, chromeThemePath.length) == chromeThemePath;
    return isDefaultAvatar ? imageset(path + '@scalefactorx') : url(path);
  }
  function imageset(path) {
    var supportedScaleFactors = getSupportedScaleFactors();
    var replaceStartIndex = path.indexOf('scalefactor');
    if (replaceStartIndex < 0) return url(path);
    var s = '';
    for (var i = 0; i < supportedScaleFactors.length; ++i) {
      var scaleFactor = supportedScaleFactors[i];
      var pathWithScaleFactor = path.substr(0, replaceStartIndex) + scaleFactor + path.substr(replaceStartIndex + 'scalefactor'.length);
      s += url(pathWithScaleFactor) + ' ' + scaleFactor + 'x';
      if (i != supportedScaleFactors.length - 1) s += ', ';
    }
    return '-webkit-image-set(' + s + ')';
  }
  var FAVICON_URL_REGEX = /\.ico$/i;
  function getFaviconImageSet(url, opt_size, opt_type) {
    var size = opt_size || 16;
    var type = opt_type || 'favicon';
    return imageset('chrome://' + type + '/size/' + size + '@scalefactorx/' + (FAVICON_URL_REGEX.test(url) ? 'iconurl/' : '') + url);
  }
  return {
    getSupportedScaleFactors: getSupportedScaleFactors,
    getProfileAvatarIcon: getProfileAvatarIcon,
    getFaviconImageSet: getFaviconImageSet
  };
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'history-searched-label',
  properties: {
    title: String,
    searchTerm: String
  },
  observers: [ 'setSearchedTextToBold_(title, searchTerm)' ],
  setSearchedTextToBold_: function() {
    var i = 0;
    var titleElem = this.$.container;
    var titleText = this.title;
    if (this.searchTerm == '' || this.searchTerm == null) {
      titleElem.textContent = titleText;
      return;
    }
    var re = new RegExp(quoteString(this.searchTerm), 'gim');
    var match;
    titleElem.textContent = '';
    while (match = re.exec(titleText)) {
      if (match.index > i) titleElem.appendChild(document.createTextNode(titleText.slice(i, match.index)));
      i = re.lastIndex;
      var b = document.createElement('b');
      b.textContent = titleText.substring(match.index, i);
      titleElem.appendChild(b);
    }
    if (i < titleText.length) titleElem.appendChild(document.createTextNode(titleText.slice(i)));
  }
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
cr.define('md_history', function() {
  var HistoryItem = Polymer({
    is: 'history-item',
    properties: {
      item: {
        type: Object,
        observer: 'showIcon_'
      },
      searchTerm: {
        type: String
      },
      selected: {
        type: Boolean,
        notify: true
      },
      isFirstItem: {
        type: Boolean,
        reflectToAttribute: true
      },
      isCardStart: {
        type: Boolean,
        reflectToAttribute: true
      },
      isCardEnd: {
        type: Boolean,
        reflectToAttribute: true
      },
      embedded: {
        type: Boolean,
        reflectToAttribute: true
      },
      hasTimeGap: {
        type: Boolean
      },
      numberOfItems: {
        type: Number
      },
      path: String,
      index: Number
    },
    onCheckboxSelected_: function(e) {
      this.fire('history-checkbox-select', {
        element: this,
        shiftKey: e.shiftKey
      });
      e.preventDefault();
    },
    onCheckboxMousedown_: function(e) {
      if (e.shiftKey) e.preventDefault();
    },
    onRemoveBookmarkTap_: function() {
      if (!this.item.starred) return;
      if (this.$$('#bookmark-star') == this.root.activeElement) this.$['menu-button'].focus();
      var browserService = md_history.BrowserService.getInstance();
      browserService.removeBookmark(this.item.url);
      browserService.recordAction('BookmarkStarClicked');
      this.fire('remove-bookmark-stars', this.item.url);
    },
    onMenuButtonTap_: function(e) {
      this.fire('toggle-menu', {
        target: Polymer.dom(e).localTarget,
        index: this.index,
        item: this.item,
        path: this.path
      });
      e.stopPropagation();
    },
    onLinkClick_: function() {
      var browserService = md_history.BrowserService.getInstance();
      browserService.recordAction('EntryLinkClick');
      if (this.searchTerm) browserService.recordAction('SearchResultClick');
      if (this.index == undefined) return;
      browserService.recordHistogram('HistoryPage.ClickPosition', this.index, UMA_MAX_BUCKET_VALUE);
      if (this.index <= UMA_MAX_SUBSET_BUCKET_VALUE) {
        browserService.recordHistogram('HistoryPage.ClickPositionSubset', this.index, UMA_MAX_SUBSET_BUCKET_VALUE);
      }
    },
    onLinkRightClick_: function() {
      md_history.BrowserService.getInstance().recordAction('EntryLinkRightClick');
    },
    showIcon_: function() {
      this.$.icon.style.backgroundImage = cr.icon.getFaviconImageSet(this.item.url);
    },
    selectionNotAllowed_: function() {
      return !loadTimeData.getBoolean('allowDeletingHistory');
    },
    cardTitle_: function(numberOfItems, historyDate, search) {
      if (!search) return this.item.dateRelativeDay;
      var resultId = numberOfItems == 1 ? 'searchResult' : 'searchResults';
      return loadTimeData.getStringF('foundSearchResults', numberOfItems, loadTimeData.getString(resultId), search);
    },
    cropItemTitle_: function(title) {
      return title.length > TITLE_MAX_LENGTH ? title.substr(0, TITLE_MAX_LENGTH) : title;
    }
  });
  HistoryItem.needsTimeGap = function(visits, currentIndex, searchedTerm) {
    if (currentIndex >= visits.length - 1 || visits.length == 0) return false;
    var currentItem = visits[currentIndex];
    var nextItem = visits[currentIndex + 1];
    if (searchedTerm) return currentItem.dateShort != nextItem.dateShort;
    return currentItem.time - nextItem.time > BROWSING_GAP_TIME && currentItem.dateRelativeDay == nextItem.dateRelativeDay;
  };
  return {
    HistoryItem: HistoryItem
  };
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var SelectionTreeNode = function(currentPath) {
  this.currentPath = currentPath;
  this.leaf = false;
  this.indexes = [];
  this.children = [];
};

SelectionTreeNode.prototype.addChild = function(index, path) {
  this.indexes.push(index);
  this.children[index] = new SelectionTreeNode(path);
};

var HistoryListBehavior = {
  properties: {
    selectedPaths: {
      type: Object,
      value: function() {
        return new Set();
      }
    },
    lastSelectedPath: String
  },
  listeners: {
    'history-checkbox-select': 'itemSelected_'
  },
  hasResults: function(historyDataLength) {
    return historyDataLength > 0;
  },
  noResultsMessage: function(searchedTerm, isLoading) {
    if (isLoading) return '';
    var messageId = searchedTerm !== '' ? 'noSearchResults' : 'noResults';
    return loadTimeData.getString(messageId);
  },
  unselectAllItems: function() {
    this.selectedPaths.forEach(function(path) {
      this.set(path + '.selected', false);
    }.bind(this));
    this.selectedPaths.clear();
  },
  deleteSelected: function() {
    var toBeRemoved = Array.from(this.selectedPaths.values()).map(function(path) {
      return this.get(path);
    }.bind(this));
    md_history.BrowserService.getInstance().deleteItems(toBeRemoved).then(function() {
      this.removeItemsByPath(Array.from(this.selectedPaths));
      this.fire('unselect-all');
    }.bind(this));
  },
  removeItemsByPath: function(paths) {
    if (paths.length == 0) return;
    this.removeItemsBeneathNode_(this.buildRemovalTree_(paths));
  },
  buildRemovalTree_: function(paths) {
    var rootNode = new SelectionTreeNode(paths[0].split('.')[0]);
    paths.forEach(function(path) {
      var components = path.split('.');
      var node = rootNode;
      components.shift();
      while (components.length > 1) {
        var index = Number(components.shift());
        var arrayName = components.shift();
        if (!node.children[index]) node.addChild(index, [ node.currentPath, index, arrayName ].join('.'));
        node = node.children[index];
      }
      node.leaf = true;
      node.indexes.push(Number(components.shift()));
    });
    return rootNode;
  },
  removeItemsBeneathNode_: function(node) {
    var array = this.get(node.currentPath);
    var splices = [];
    node.indexes.sort(function(a, b) {
      return b - a;
    });
    node.indexes.forEach(function(index) {
      if (node.leaf || this.removeItemsBeneathNode_(node.children[index])) {
        var item = array.splice(index, 1)[0];
        splices.push({
          index: index,
          removed: [ item ],
          addedCount: 0,
          object: array,
          type: 'splice'
        });
      }
    }.bind(this));
    if (array.length == 0 && node.currentPath.indexOf('.') != -1) return true;
    this.notifySplices(node.currentPath, splices);
    return false;
  },
  itemSelected_: function(e) {
    var item = e.detail.element;
    var paths = [];
    var itemPath = item.path;
    if (e.detail.shiftKey && this.lastSelectedPath) {
      var itemPathComponents = itemPath.split('.');
      var itemIndex = Number(itemPathComponents.pop());
      var itemArrayPath = itemPathComponents.join('.');
      var lastItemPathComponents = this.lastSelectedPath.split('.');
      var lastItemIndex = Number(lastItemPathComponents.pop());
      if (itemArrayPath == lastItemPathComponents.join('.')) {
        for (var i = Math.min(itemIndex, lastItemIndex); i <= Math.max(itemIndex, lastItemIndex); i++) {
          paths.push(itemArrayPath + '.' + i);
        }
      }
    }
    if (paths.length == 0) paths.push(item.path);
    paths.forEach(function(path) {
      this.set(path + '.selected', item.selected);
      if (item.selected) {
        this.selectedPaths.add(path);
        return;
      }
      this.selectedPaths.delete(path);
    }.bind(this));
    this.lastSelectedPath = itemPath;
  }
};

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var HistoryDomain;

var HistoryGroup;

Polymer({
  is: 'history-grouped-list',
  behaviors: [ HistoryListBehavior ],
  properties: {
    historyData: {
      type: Array
    },
    groupedHistoryData_: {
      type: Array
    },
    searchedTerm: {
      type: String,
      value: ''
    },
    range: {
      type: Number
    },
    queryStartTime: String,
    queryEndTime: String
  },
  observers: [ 'updateGroupedHistoryData_(range, historyData)' ],
  createHistoryDomains_: function(visits) {
    var domainIndexes = {};
    var domains = [];
    for (var i = 0, visit; visit = visits[i]; i++) {
      var domain = visit.domain;
      if (domainIndexes[domain] == undefined) {
        domainIndexes[domain] = domains.length;
        domains.push({
          domain: domain,
          visits: [],
          expanded: false,
          rendered: false
        });
      }
      domains[domainIndexes[domain]].visits.push(visit);
    }
    var sortByVisits = function(a, b) {
      return b.visits.length - a.visits.length;
    };
    domains.sort(sortByVisits);
    return domains;
  },
  updateGroupedHistoryData_: function() {
    if (this.historyData.length == 0) {
      this.groupedHistoryData_ = [];
      return;
    }
    if (this.range == HistoryRange.WEEK) {
      var days = [];
      var currentDayVisits = [ this.historyData[0] ];
      var pushCurrentDay = function() {
        days.push({
          title: this.searchedTerm ? currentDayVisits[0].dateShort : currentDayVisits[0].dateRelativeDay,
          domains: this.createHistoryDomains_(currentDayVisits)
        });
      }.bind(this);
      var visitsSameDay = function(a, b) {
        if (this.searchedTerm) return a.dateShort == b.dateShort;
        return a.dateRelativeDay == b.dateRelativeDay;
      }.bind(this);
      for (var i = 1; i < this.historyData.length; i++) {
        var visit = this.historyData[i];
        if (!visitsSameDay(visit, currentDayVisits[0])) {
          pushCurrentDay();
          currentDayVisits = [];
        }
        currentDayVisits.push(visit);
      }
      pushCurrentDay();
      this.groupedHistoryData_ = days;
    } else if (this.range == HistoryRange.MONTH) {
      this.groupedHistoryData_ = [ {
        title: this.queryStartTime + ' – ' + this.queryEndTime,
        domains: this.createHistoryDomains_(this.historyData)
      } ];
    }
  },
  toggleDomainExpanded_: function(e) {
    var collapse = e.currentTarget.parentNode.querySelector('iron-collapse');
    e.model.set('domain.rendered', true);
    setTimeout(function() {
      collapse.toggle();
    }, 0);
  },
  needsTimeGap_: function(groupIndex, domainIndex, itemIndex) {
    var visits = this.groupedHistoryData_[groupIndex].domains[domainIndex].visits;
    return md_history.HistoryItem.needsTimeGap(visits, itemIndex, this.searchedTerm);
  },
  pathForItem_: function(groupIndex, domainIndex, itemIndex) {
    return [ 'groupedHistoryData_', groupIndex, 'domains', domainIndex, 'visits', itemIndex ].join('.');
  },
  getWebsiteIconStyle_: function(domain) {
    return 'background-image: ' + cr.icon.getFaviconImageSet(domain.visits[0].url);
  },
  getDropdownIcon_: function(expanded) {
    return expanded ? 'cr:expand-less' : 'cr:expand-more';
  }
});

(function() {
  var IOS = navigator.userAgent.match(/iP(?:hone|ad;(?: U;)? CPU) OS (\d+)/);
  var IOS_TOUCH_SCROLLING = IOS && IOS[1] >= 8;
  var DEFAULT_PHYSICAL_COUNT = 3;
  var HIDDEN_Y = '-10000px';
  var DEFAULT_GRID_SIZE = 200;
  var SECRET_TABINDEX = -100;
  Polymer({
    is: 'iron-list',
    properties: {
      items: {
        type: Array
      },
      maxPhysicalCount: {
        type: Number,
        value: 500
      },
      as: {
        type: String,
        value: 'item'
      },
      indexAs: {
        type: String,
        value: 'index'
      },
      selectedAs: {
        type: String,
        value: 'selected'
      },
      grid: {
        type: Boolean,
        value: false,
        reflectToAttribute: true
      },
      selectionEnabled: {
        type: Boolean,
        value: false
      },
      selectedItem: {
        type: Object,
        notify: true
      },
      selectedItems: {
        type: Object,
        notify: true
      },
      multiSelection: {
        type: Boolean,
        value: false
      }
    },
    observers: [ '_itemsChanged(items.*)', '_selectionEnabledChanged(selectionEnabled)', '_multiSelectionChanged(multiSelection)', '_setOverflow(scrollTarget)' ],
    behaviors: [ Polymer.Templatizer, Polymer.IronResizableBehavior, Polymer.IronA11yKeysBehavior, Polymer.IronScrollTargetBehavior ],
    keyBindings: {
      up: '_didMoveUp',
      down: '_didMoveDown',
      enter: '_didEnter'
    },
    _ratio: .5,
    _scrollerPaddingTop: 0,
    _scrollPosition: 0,
    _physicalSize: 0,
    _physicalAverage: 0,
    _physicalAverageCount: 0,
    _physicalTop: 0,
    _virtualCount: 0,
    _physicalIndexForKey: null,
    _estScrollHeight: 0,
    _scrollHeight: 0,
    _viewportHeight: 0,
    _viewportWidth: 0,
    _physicalItems: null,
    _physicalSizes: null,
    _firstVisibleIndexVal: null,
    _lastVisibleIndexVal: null,
    _collection: null,
    _itemsRendered: false,
    _lastPage: null,
    _maxPages: 3,
    _focusedItem: null,
    _focusedIndex: -1,
    _offscreenFocusedItem: null,
    _focusBackfillItem: null,
    _itemsPerRow: 1,
    _itemWidth: 0,
    _rowHeight: 0,
    get _physicalBottom() {
      return this._physicalTop + this._physicalSize;
    },
    get _scrollBottom() {
      return this._scrollPosition + this._viewportHeight;
    },
    get _virtualEnd() {
      return this._virtualStart + this._physicalCount - 1;
    },
    get _hiddenContentSize() {
      var size = this.grid ? this._physicalRows * this._rowHeight : this._physicalSize;
      return size - this._viewportHeight;
    },
    get _maxScrollTop() {
      return this._estScrollHeight - this._viewportHeight + this._scrollerPaddingTop;
    },
    _minVirtualStart: 0,
    get _maxVirtualStart() {
      return Math.max(0, this._virtualCount - this._physicalCount);
    },
    _virtualStartVal: 0,
    set _virtualStart(val) {
      this._virtualStartVal = Math.min(this._maxVirtualStart, Math.max(this._minVirtualStart, val));
    },
    get _virtualStart() {
      return this._virtualStartVal || 0;
    },
    _physicalStartVal: 0,
    set _physicalStart(val) {
      this._physicalStartVal = val % this._physicalCount;
      if (this._physicalStartVal < 0) {
        this._physicalStartVal = this._physicalCount + this._physicalStartVal;
      }
      this._physicalEnd = (this._physicalStart + this._physicalCount - 1) % this._physicalCount;
    },
    get _physicalStart() {
      return this._physicalStartVal || 0;
    },
    _physicalCountVal: 0,
    set _physicalCount(val) {
      this._physicalCountVal = val;
      this._physicalEnd = (this._physicalStart + this._physicalCount - 1) % this._physicalCount;
    },
    get _physicalCount() {
      return this._physicalCountVal;
    },
    _physicalEnd: 0,
    get _optPhysicalSize() {
      if (this.grid) {
        return this._estRowsInView * this._rowHeight * this._maxPages;
      }
      return this._viewportHeight * this._maxPages;
    },
    get _optPhysicalCount() {
      return this._estRowsInView * this._itemsPerRow * this._maxPages;
    },
    get _isVisible() {
      return this.scrollTarget && Boolean(this.scrollTarget.offsetWidth || this.scrollTarget.offsetHeight);
    },
    get firstVisibleIndex() {
      if (this._firstVisibleIndexVal === null) {
        var physicalOffset = Math.floor(this._physicalTop + this._scrollerPaddingTop);
        this._firstVisibleIndexVal = this._iterateItems(function(pidx, vidx) {
          physicalOffset += this._getPhysicalSizeIncrement(pidx);
          if (physicalOffset > this._scrollPosition) {
            return this.grid ? vidx - vidx % this._itemsPerRow : vidx;
          }
          if (this.grid && this._virtualCount - 1 === vidx) {
            return vidx - vidx % this._itemsPerRow;
          }
        }) || 0;
      }
      return this._firstVisibleIndexVal;
    },
    get lastVisibleIndex() {
      if (this._lastVisibleIndexVal === null) {
        if (this.grid) {
          var lastIndex = this.firstVisibleIndex + this._estRowsInView * this._itemsPerRow - 1;
          this._lastVisibleIndexVal = Math.min(this._virtualCount, lastIndex);
        } else {
          var physicalOffset = this._physicalTop;
          this._iterateItems(function(pidx, vidx) {
            if (physicalOffset < this._scrollBottom) {
              this._lastVisibleIndexVal = vidx;
            } else {
              return true;
            }
            physicalOffset += this._getPhysicalSizeIncrement(pidx);
          });
        }
      }
      return this._lastVisibleIndexVal;
    },
    get _defaultScrollTarget() {
      return this;
    },
    get _virtualRowCount() {
      return Math.ceil(this._virtualCount / this._itemsPerRow);
    },
    get _estRowsInView() {
      return Math.ceil(this._viewportHeight / this._rowHeight);
    },
    get _physicalRows() {
      return Math.ceil(this._physicalCount / this._itemsPerRow);
    },
    ready: function() {
      this.addEventListener('focus', this._didFocus.bind(this), true);
    },
    attached: function() {
      this.updateViewportBoundaries();
      this._render();
      this.listen(this, 'iron-resize', '_resizeHandler');
    },
    detached: function() {
      this._itemsRendered = false;
      this.unlisten(this, 'iron-resize', '_resizeHandler');
    },
    _setOverflow: function(scrollTarget) {
      this.style.webkitOverflowScrolling = scrollTarget === this ? 'touch' : '';
      this.style.overflow = scrollTarget === this ? 'auto' : '';
    },
    updateViewportBoundaries: function() {
      this._scrollerPaddingTop = this.scrollTarget === this ? 0 : parseInt(window.getComputedStyle(this)['padding-top'], 10);
      this._viewportHeight = this._scrollTargetHeight;
      if (this.grid) {
        this._updateGridMetrics();
      }
    },
    _scrollHandler: function() {
      var scrollTop = Math.max(0, Math.min(this._maxScrollTop, this._scrollTop));
      var delta = scrollTop - this._scrollPosition;
      var tileHeight, tileTop, kth, recycledTileSet, scrollBottom, physicalBottom;
      var ratio = this._ratio;
      var recycledTiles = 0;
      var hiddenContentSize = this._hiddenContentSize;
      var currentRatio = ratio;
      var movingUp = [];
      this._scrollPosition = scrollTop;
      this._firstVisibleIndexVal = null;
      this._lastVisibleIndexVal = null;
      scrollBottom = this._scrollBottom;
      physicalBottom = this._physicalBottom;
      if (Math.abs(delta) > this._physicalSize) {
        this._physicalTop += delta;
        recycledTiles = Math.round(delta / this._physicalAverage);
      } else if (delta < 0) {
        var topSpace = scrollTop - this._physicalTop;
        var virtualStart = this._virtualStart;
        recycledTileSet = [];
        kth = this._physicalEnd;
        currentRatio = topSpace / hiddenContentSize;
        while (currentRatio < ratio && recycledTiles < this._physicalCount && virtualStart - recycledTiles > 0 && physicalBottom - this._getPhysicalSizeIncrement(kth) > scrollBottom) {
          tileHeight = this._getPhysicalSizeIncrement(kth);
          currentRatio += tileHeight / hiddenContentSize;
          physicalBottom -= tileHeight;
          recycledTileSet.push(kth);
          recycledTiles++;
          kth = kth === 0 ? this._physicalCount - 1 : kth - 1;
        }
        movingUp = recycledTileSet;
        recycledTiles = -recycledTiles;
      } else if (delta > 0) {
        var bottomSpace = physicalBottom - scrollBottom;
        var virtualEnd = this._virtualEnd;
        var lastVirtualItemIndex = this._virtualCount - 1;
        recycledTileSet = [];
        kth = this._physicalStart;
        currentRatio = bottomSpace / hiddenContentSize;
        while (currentRatio < ratio && recycledTiles < this._physicalCount && virtualEnd + recycledTiles < lastVirtualItemIndex && this._physicalTop + this._getPhysicalSizeIncrement(kth) < scrollTop) {
          tileHeight = this._getPhysicalSizeIncrement(kth);
          currentRatio += tileHeight / hiddenContentSize;
          this._physicalTop += tileHeight;
          recycledTileSet.push(kth);
          recycledTiles++;
          kth = (kth + 1) % this._physicalCount;
        }
      }
      if (recycledTiles === 0) {
        if (physicalBottom < scrollBottom || this._physicalTop > scrollTop) {
          this._increasePoolIfNeeded();
        }
      } else {
        this._virtualStart = this._virtualStart + recycledTiles;
        this._physicalStart = this._physicalStart + recycledTiles;
        this._update(recycledTileSet, movingUp);
      }
    },
    _update: function(itemSet, movingUp) {
      this._manageFocus();
      this._assignModels(itemSet);
      this._updateMetrics(itemSet);
      if (movingUp) {
        while (movingUp.length) {
          var idx = movingUp.pop();
          this._physicalTop -= this._getPhysicalSizeIncrement(idx);
        }
      }
      this._positionItems();
      this._updateScrollerSize();
      this._increasePoolIfNeeded();
    },
    _createPool: function(size) {
      var physicalItems = new Array(size);
      this._ensureTemplatized();
      for (var i = 0; i < size; i++) {
        var inst = this.stamp(null);
        physicalItems[i] = inst.root.querySelector('*');
        Polymer.dom(this).appendChild(inst.root);
      }
      return physicalItems;
    },
    _increasePoolIfNeeded: function() {
      if (this._viewportHeight === 0) {
        return false;
      }
      var isClientHeightFull = this._physicalBottom >= this._scrollBottom && this._physicalTop <= this._scrollPosition;
      if (this._physicalSize >= this._optPhysicalSize && isClientHeightFull) {
        return false;
      }
      var currentPage = Math.floor(this._physicalSize / this._viewportHeight);
      if (currentPage === 0) {
        this._debounceTemplate(this._increasePool.bind(this, Math.round(this._physicalCount * .5)));
      } else if (this._lastPage !== currentPage && isClientHeightFull) {
        Polymer.dom.addDebouncer(this.debounce('_debounceTemplate', this._increasePool.bind(this, this._itemsPerRow), 16));
      } else {
        this._debounceTemplate(this._increasePool.bind(this, this._itemsPerRow));
      }
      this._lastPage = currentPage;
      return true;
    },
    _increasePool: function(missingItems) {
      var nextPhysicalCount = Math.min(this._physicalCount + missingItems, this._virtualCount - this._virtualStart, Math.max(this.maxPhysicalCount, DEFAULT_PHYSICAL_COUNT));
      var prevPhysicalCount = this._physicalCount;
      var delta = nextPhysicalCount - prevPhysicalCount;
      if (delta <= 0) {
        return;
      }
      [].push.apply(this._physicalItems, this._createPool(delta));
      [].push.apply(this._physicalSizes, new Array(delta));
      this._physicalCount = prevPhysicalCount + delta;
      if (this._physicalStart > this._physicalEnd && this._isIndexRendered(this._focusedIndex) && this._getPhysicalIndex(this._focusedIndex) < this._physicalEnd) {
        this._physicalStart = this._physicalStart + delta;
      }
      this._update();
    },
    _render: function() {
      var requiresUpdate = this._virtualCount > 0 || this._physicalCount > 0;
      if (this.isAttached && !this._itemsRendered && this._isVisible && requiresUpdate) {
        this._lastPage = 0;
        this._update();
        this._itemsRendered = true;
      }
    },
    _ensureTemplatized: function() {
      if (!this.ctor) {
        var props = {};
        props.__key__ = true;
        props[this.as] = true;
        props[this.indexAs] = true;
        props[this.selectedAs] = true;
        props.tabIndex = true;
        this._instanceProps = props;
        this._userTemplate = Polymer.dom(this).querySelector('template');
        if (this._userTemplate) {
          this.templatize(this._userTemplate);
        } else {
          console.warn('iron-list requires a template to be provided in light-dom');
        }
      }
    },
    _getStampedChildren: function() {
      return this._physicalItems;
    },
    _forwardInstancePath: function(inst, path, value) {
      if (path.indexOf(this.as + '.') === 0) {
        this.notifyPath('items.' + inst.__key__ + '.' + path.slice(this.as.length + 1), value);
      }
    },
    _forwardParentProp: function(prop, value) {
      if (this._physicalItems) {
        this._physicalItems.forEach(function(item) {
          item._templateInstance[prop] = value;
        }, this);
      }
    },
    _forwardParentPath: function(path, value) {
      if (this._physicalItems) {
        this._physicalItems.forEach(function(item) {
          item._templateInstance.notifyPath(path, value, true);
        }, this);
      }
    },
    _forwardItemPath: function(path, value) {
      if (!this._physicalIndexForKey) {
        return;
      }
      var dot = path.indexOf('.');
      var key = path.substring(0, dot < 0 ? path.length : dot);
      var idx = this._physicalIndexForKey[key];
      var offscreenItem = this._offscreenFocusedItem;
      var el = offscreenItem && offscreenItem._templateInstance.__key__ === key ? offscreenItem : this._physicalItems[idx];
      if (!el || el._templateInstance.__key__ !== key) {
        return;
      }
      if (dot >= 0) {
        path = this.as + '.' + path.substring(dot + 1);
        el._templateInstance.notifyPath(path, value, true);
      } else {
        var currentItem = el._templateInstance[this.as];
        if (Array.isArray(this.selectedItems)) {
          for (var i = 0; i < this.selectedItems.length; i++) {
            if (this.selectedItems[i] === currentItem) {
              this.set('selectedItems.' + i, value);
              break;
            }
          }
        } else if (this.selectedItem === currentItem) {
          this.set('selectedItem', value);
        }
        el._templateInstance[this.as] = value;
      }
    },
    _itemsChanged: function(change) {
      if (change.path === 'items') {
        this._virtualStart = 0;
        this._physicalTop = 0;
        this._virtualCount = this.items ? this.items.length : 0;
        this._collection = this.items ? Polymer.Collection.get(this.items) : null;
        this._physicalIndexForKey = {};
        this._firstVisibleIndexVal = null;
        this._lastVisibleIndexVal = null;
        this._resetScrollPosition(0);
        this._removeFocusedItem();
        if (!this._physicalItems) {
          this._physicalCount = Math.max(1, Math.min(DEFAULT_PHYSICAL_COUNT, this._virtualCount));
          this._physicalItems = this._createPool(this._physicalCount);
          this._physicalSizes = new Array(this._physicalCount);
        }
        this._physicalStart = 0;
      } else if (change.path === 'items.splices') {
        this._adjustVirtualIndex(change.value.indexSplices);
        this._virtualCount = this.items ? this.items.length : 0;
      } else {
        this._forwardItemPath(change.path.split('.').slice(1).join('.'), change.value);
        return;
      }
      this._itemsRendered = false;
      this._debounceTemplate(this._render);
    },
    _adjustVirtualIndex: function(splices) {
      splices.forEach(function(splice) {
        splice.removed.forEach(this._removeItem, this);
        if (splice.index < this._virtualStart) {
          var delta = Math.max(splice.addedCount - splice.removed.length, splice.index - this._virtualStart);
          this._virtualStart = this._virtualStart + delta;
          if (this._focusedIndex >= 0) {
            this._focusedIndex = this._focusedIndex + delta;
          }
        }
      }, this);
    },
    _removeItem: function(item) {
      this.$.selector.deselect(item);
      if (this._focusedItem && this._focusedItem._templateInstance[this.as] === item) {
        this._removeFocusedItem();
      }
    },
    _iterateItems: function(fn, itemSet) {
      var pidx, vidx, rtn, i;
      if (arguments.length === 2 && itemSet) {
        for (i = 0; i < itemSet.length; i++) {
          pidx = itemSet[i];
          vidx = this._computeVidx(pidx);
          if ((rtn = fn.call(this, pidx, vidx)) != null) {
            return rtn;
          }
        }
      } else {
        pidx = this._physicalStart;
        vidx = this._virtualStart;
        for (;pidx < this._physicalCount; pidx++, vidx++) {
          if ((rtn = fn.call(this, pidx, vidx)) != null) {
            return rtn;
          }
        }
        for (pidx = 0; pidx < this._physicalStart; pidx++, vidx++) {
          if ((rtn = fn.call(this, pidx, vidx)) != null) {
            return rtn;
          }
        }
      }
    },
    _computeVidx: function(pidx) {
      if (pidx >= this._physicalStart) {
        return this._virtualStart + (pidx - this._physicalStart);
      }
      return this._virtualStart + (this._physicalCount - this._physicalStart) + pidx;
    },
    _assignModels: function(itemSet) {
      this._iterateItems(function(pidx, vidx) {
        var el = this._physicalItems[pidx];
        var inst = el._templateInstance;
        var item = this.items && this.items[vidx];
        if (item != null) {
          inst[this.as] = item;
          inst.__key__ = this._collection.getKey(item);
          inst[this.selectedAs] = this.$.selector.isSelected(item);
          inst[this.indexAs] = vidx;
          inst.tabIndex = this._focusedIndex === vidx ? 0 : -1;
          this._physicalIndexForKey[inst.__key__] = pidx;
          el.removeAttribute('hidden');
        } else {
          inst.__key__ = null;
          el.setAttribute('hidden', '');
        }
      }, itemSet);
    },
    _updateMetrics: function(itemSet) {
      Polymer.dom.flush();
      var newPhysicalSize = 0;
      var oldPhysicalSize = 0;
      var prevAvgCount = this._physicalAverageCount;
      var prevPhysicalAvg = this._physicalAverage;
      this._iterateItems(function(pidx, vidx) {
        oldPhysicalSize += this._physicalSizes[pidx] || 0;
        this._physicalSizes[pidx] = this._physicalItems[pidx].offsetHeight;
        newPhysicalSize += this._physicalSizes[pidx];
        this._physicalAverageCount += this._physicalSizes[pidx] ? 1 : 0;
      }, itemSet);
      this._viewportHeight = this._scrollTargetHeight;
      if (this.grid) {
        this._updateGridMetrics();
        this._physicalSize = Math.ceil(this._physicalCount / this._itemsPerRow) * this._rowHeight;
      } else {
        this._physicalSize = this._physicalSize + newPhysicalSize - oldPhysicalSize;
      }
      if (this._physicalAverageCount !== prevAvgCount) {
        this._physicalAverage = Math.round((prevPhysicalAvg * prevAvgCount + newPhysicalSize) / this._physicalAverageCount);
      }
    },
    _updateGridMetrics: function() {
      this._viewportWidth = this.$.items.offsetWidth;
      this._itemWidth = this._physicalCount > 0 ? this._physicalItems[0].getBoundingClientRect().width : DEFAULT_GRID_SIZE;
      this._rowHeight = this._physicalCount > 0 ? this._physicalItems[0].offsetHeight : DEFAULT_GRID_SIZE;
      this._itemsPerRow = this._itemWidth ? Math.floor(this._viewportWidth / this._itemWidth) : this._itemsPerRow;
    },
    _positionItems: function() {
      this._adjustScrollPosition();
      var y = this._physicalTop;
      if (this.grid) {
        var totalItemWidth = this._itemsPerRow * this._itemWidth;
        var rowOffset = (this._viewportWidth - totalItemWidth) / 2;
        this._iterateItems(function(pidx, vidx) {
          var modulus = vidx % this._itemsPerRow;
          var x = Math.floor(modulus * this._itemWidth + rowOffset);
          this.translate3d(x + 'px', y + 'px', 0, this._physicalItems[pidx]);
          if (this._shouldRenderNextRow(vidx)) {
            y += this._rowHeight;
          }
        });
      } else {
        this._iterateItems(function(pidx, vidx) {
          this.translate3d(0, y + 'px', 0, this._physicalItems[pidx]);
          y += this._physicalSizes[pidx];
        });
      }
    },
    _getPhysicalSizeIncrement: function(pidx) {
      if (!this.grid) {
        return this._physicalSizes[pidx];
      }
      if (this._computeVidx(pidx) % this._itemsPerRow !== this._itemsPerRow - 1) {
        return 0;
      }
      return this._rowHeight;
    },
    _shouldRenderNextRow: function(vidx) {
      return vidx % this._itemsPerRow === this._itemsPerRow - 1;
    },
    _adjustScrollPosition: function() {
      var deltaHeight = this._virtualStart === 0 ? this._physicalTop : Math.min(this._scrollPosition + this._physicalTop, 0);
      if (deltaHeight) {
        this._physicalTop = this._physicalTop - deltaHeight;
        if (!IOS_TOUCH_SCROLLING && this._physicalTop !== 0) {
          this._resetScrollPosition(this._scrollTop - deltaHeight);
        }
      }
    },
    _resetScrollPosition: function(pos) {
      if (this.scrollTarget) {
        this._scrollTop = pos;
        this._scrollPosition = this._scrollTop;
      }
    },
    _updateScrollerSize: function(forceUpdate) {
      if (this.grid) {
        this._estScrollHeight = this._virtualRowCount * this._rowHeight;
      } else {
        this._estScrollHeight = this._physicalBottom + Math.max(this._virtualCount - this._physicalCount - this._virtualStart, 0) * this._physicalAverage;
      }
      forceUpdate = forceUpdate || this._scrollHeight === 0;
      forceUpdate = forceUpdate || this._scrollPosition >= this._estScrollHeight - this._physicalSize;
      forceUpdate = forceUpdate || this.grid && this.$.items.style.height < this._estScrollHeight;
      if (forceUpdate || Math.abs(this._estScrollHeight - this._scrollHeight) >= this._optPhysicalSize) {
        this.$.items.style.height = this._estScrollHeight + 'px';
        this._scrollHeight = this._estScrollHeight;
      }
    },
    scrollToItem: function(item) {
      return this.scrollToIndex(this.items.indexOf(item));
    },
    scrollToIndex: function(idx) {
      if (typeof idx !== 'number' || idx < 0 || idx > this.items.length - 1) {
        return;
      }
      Polymer.dom.flush();
      idx = Math.min(Math.max(idx, 0), this._virtualCount - 1);
      if (!this._isIndexRendered(idx) || idx >= this._maxVirtualStart) {
        this._virtualStart = this.grid ? idx - this._itemsPerRow * 2 : idx - 1;
      }
      this._manageFocus();
      this._assignModels();
      this._updateMetrics();
      var estPhysicalTop = Math.floor(this._virtualStart / this._itemsPerRow) * this._physicalAverage;
      this._physicalTop = estPhysicalTop;
      var currentTopItem = this._physicalStart;
      var currentVirtualItem = this._virtualStart;
      var targetOffsetTop = 0;
      var hiddenContentSize = this._hiddenContentSize;
      while (currentVirtualItem < idx && targetOffsetTop <= hiddenContentSize) {
        targetOffsetTop = targetOffsetTop + this._getPhysicalSizeIncrement(currentTopItem);
        currentTopItem = (currentTopItem + 1) % this._physicalCount;
        currentVirtualItem++;
      }
      this._updateScrollerSize(true);
      this._positionItems();
      this._resetScrollPosition(this._physicalTop + this._scrollerPaddingTop + targetOffsetTop);
      this._increasePoolIfNeeded();
      this._firstVisibleIndexVal = null;
      this._lastVisibleIndexVal = null;
    },
    _resetAverage: function() {
      this._physicalAverage = 0;
      this._physicalAverageCount = 0;
    },
    _resizeHandler: function() {
      if (IOS && Math.abs(this._viewportHeight - this._scrollTargetHeight) < 100) {
        return;
      }
      Polymer.dom.addDebouncer(this.debounce('_debounceTemplate', function() {
        this.updateViewportBoundaries();
        this._render();
        if (this._itemsRendered && this._physicalItems && this._isVisible) {
          this._resetAverage();
          this.scrollToIndex(this.firstVisibleIndex);
        }
      }.bind(this), 1));
    },
    _getModelFromItem: function(item) {
      var key = this._collection.getKey(item);
      var pidx = this._physicalIndexForKey[key];
      if (pidx != null) {
        return this._physicalItems[pidx]._templateInstance;
      }
      return null;
    },
    _getNormalizedItem: function(item) {
      if (this._collection.getKey(item) === undefined) {
        if (typeof item === 'number') {
          item = this.items[item];
          if (!item) {
            throw new RangeError('<item> not found');
          }
          return item;
        }
        throw new TypeError('<item> should be a valid item');
      }
      return item;
    },
    selectItem: function(item) {
      item = this._getNormalizedItem(item);
      var model = this._getModelFromItem(item);
      if (!this.multiSelection && this.selectedItem) {
        this.deselectItem(this.selectedItem);
      }
      if (model) {
        model[this.selectedAs] = true;
      }
      this.$.selector.select(item);
      this.updateSizeForItem(item);
    },
    deselectItem: function(item) {
      item = this._getNormalizedItem(item);
      var model = this._getModelFromItem(item);
      if (model) {
        model[this.selectedAs] = false;
      }
      this.$.selector.deselect(item);
      this.updateSizeForItem(item);
    },
    toggleSelectionForItem: function(item) {
      item = this._getNormalizedItem(item);
      if (this.$.selector.isSelected(item)) {
        this.deselectItem(item);
      } else {
        this.selectItem(item);
      }
    },
    clearSelection: function() {
      function unselect(item) {
        var model = this._getModelFromItem(item);
        if (model) {
          model[this.selectedAs] = false;
        }
      }
      if (Array.isArray(this.selectedItems)) {
        this.selectedItems.forEach(unselect, this);
      } else if (this.selectedItem) {
        unselect.call(this, this.selectedItem);
      }
      this.$.selector.clearSelection();
    },
    _selectionEnabledChanged: function(selectionEnabled) {
      var handler = selectionEnabled ? this.listen : this.unlisten;
      handler.call(this, this, 'tap', '_selectionHandler');
    },
    _selectionHandler: function(e) {
      var model = this.modelForElement(e.target);
      if (!model) {
        return;
      }
      var modelTabIndex, activeElTabIndex;
      var target = Polymer.dom(e).path[0];
      var activeEl = Polymer.dom(this.domHost ? this.domHost.root : document).activeElement;
      var physicalItem = this._physicalItems[this._getPhysicalIndex(model[this.indexAs])];
      if (target.localName === 'input' || target.localName === 'button' || target.localName === 'select') {
        return;
      }
      modelTabIndex = model.tabIndex;
      model.tabIndex = SECRET_TABINDEX;
      activeElTabIndex = activeEl ? activeEl.tabIndex : -1;
      model.tabIndex = modelTabIndex;
      if (activeEl && physicalItem.contains(activeEl) && activeElTabIndex !== SECRET_TABINDEX) {
        return;
      }
      this.toggleSelectionForItem(model[this.as]);
    },
    _multiSelectionChanged: function(multiSelection) {
      this.clearSelection();
      this.$.selector.multi = multiSelection;
    },
    updateSizeForItem: function(item) {
      item = this._getNormalizedItem(item);
      var key = this._collection.getKey(item);
      var pidx = this._physicalIndexForKey[key];
      if (pidx != null) {
        this._updateMetrics([ pidx ]);
        this._positionItems();
      }
    },
    _manageFocus: function() {
      var fidx = this._focusedIndex;
      if (fidx >= 0 && fidx < this._virtualCount) {
        if (this._isIndexRendered(fidx)) {
          this._restoreFocusedItem();
        } else {
          this._createFocusBackfillItem();
        }
      } else if (this._virtualCount > 0 && this._physicalCount > 0) {
        this._focusedIndex = this._virtualStart;
        this._focusedItem = this._physicalItems[this._physicalStart];
      }
    },
    _isIndexRendered: function(idx) {
      return idx >= this._virtualStart && idx <= this._virtualEnd;
    },
    _isIndexVisible: function(idx) {
      return idx >= this.firstVisibleIndex && idx <= this.lastVisibleIndex;
    },
    _getPhysicalIndex: function(idx) {
      return this._physicalIndexForKey[this._collection.getKey(this._getNormalizedItem(idx))];
    },
    _focusPhysicalItem: function(idx) {
      if (idx < 0 || idx >= this._virtualCount) {
        return;
      }
      this._restoreFocusedItem();
      if (!this._isIndexRendered(idx)) {
        this.scrollToIndex(idx);
      }
      var physicalItem = this._physicalItems[this._getPhysicalIndex(idx)];
      var model = physicalItem._templateInstance;
      var focusable;
      model.tabIndex = SECRET_TABINDEX;
      if (physicalItem.tabIndex === SECRET_TABINDEX) {
        focusable = physicalItem;
      }
      if (!focusable) {
        focusable = Polymer.dom(physicalItem).querySelector('[tabindex="' + SECRET_TABINDEX + '"]');
      }
      model.tabIndex = 0;
      this._focusedIndex = idx;
      focusable && focusable.focus();
    },
    _removeFocusedItem: function() {
      if (this._offscreenFocusedItem) {
        Polymer.dom(this).removeChild(this._offscreenFocusedItem);
      }
      this._offscreenFocusedItem = null;
      this._focusBackfillItem = null;
      this._focusedItem = null;
      this._focusedIndex = -1;
    },
    _createFocusBackfillItem: function() {
      var pidx, fidx = this._focusedIndex;
      if (this._offscreenFocusedItem || fidx < 0) {
        return;
      }
      if (!this._focusBackfillItem) {
        var stampedTemplate = this.stamp(null);
        this._focusBackfillItem = stampedTemplate.root.querySelector('*');
        Polymer.dom(this).appendChild(stampedTemplate.root);
      }
      pidx = this._getPhysicalIndex(fidx);
      if (pidx != null) {
        this._offscreenFocusedItem = this._physicalItems[pidx];
        this._physicalItems[pidx] = this._focusBackfillItem;
        this.translate3d(0, HIDDEN_Y, 0, this._offscreenFocusedItem);
      }
    },
    _restoreFocusedItem: function() {
      var pidx, fidx = this._focusedIndex;
      if (!this._offscreenFocusedItem || this._focusedIndex < 0) {
        return;
      }
      this._assignModels();
      pidx = this._getPhysicalIndex(fidx);
      if (pidx != null) {
        this._focusBackfillItem = this._physicalItems[pidx];
        this._physicalItems[pidx] = this._offscreenFocusedItem;
        this._offscreenFocusedItem = null;
        this.translate3d(0, HIDDEN_Y, 0, this._focusBackfillItem);
      }
    },
    _didFocus: function(e) {
      var targetModel = this.modelForElement(e.target);
      var focusedModel = this._focusedItem ? this._focusedItem._templateInstance : null;
      var hasOffscreenFocusedItem = this._offscreenFocusedItem !== null;
      var fidx = this._focusedIndex;
      if (!targetModel || !focusedModel) {
        return;
      }
      if (focusedModel === targetModel) {
        if (!this._isIndexVisible(fidx)) {
          this.scrollToIndex(fidx);
        }
      } else {
        this._restoreFocusedItem();
        focusedModel.tabIndex = -1;
        targetModel.tabIndex = 0;
        fidx = targetModel[this.indexAs];
        this._focusedIndex = fidx;
        this._focusedItem = this._physicalItems[this._getPhysicalIndex(fidx)];
        if (hasOffscreenFocusedItem && !this._offscreenFocusedItem) {
          this._update();
        }
      }
    },
    _didMoveUp: function() {
      this._focusPhysicalItem(this._focusedIndex - 1);
    },
    _didMoveDown: function(e) {
      e.detail.keyboardEvent.preventDefault();
      this._focusPhysicalItem(this._focusedIndex + 1);
    },
    _didEnter: function(e) {
      this._focusPhysicalItem(this._focusedIndex);
      this._selectionHandler(e.detail.keyboardEvent);
    }
  });
})();

Polymer({
  is: 'iron-scroll-threshold',
  properties: {
    upperThreshold: {
      type: Number,
      value: 100
    },
    lowerThreshold: {
      type: Number,
      value: 100
    },
    upperTriggered: {
      type: Boolean,
      value: false,
      notify: true,
      readOnly: true
    },
    lowerTriggered: {
      type: Boolean,
      value: false,
      notify: true,
      readOnly: true
    },
    horizontal: {
      type: Boolean,
      value: false
    }
  },
  behaviors: [ Polymer.IronScrollTargetBehavior ],
  observers: [ '_setOverflow(scrollTarget)', '_initCheck(horizontal, isAttached)' ],
  get _defaultScrollTarget() {
    return this;
  },
  _setOverflow: function(scrollTarget) {
    this.style.overflow = scrollTarget === this ? 'auto' : '';
  },
  _scrollHandler: function() {
    var THROTTLE_THRESHOLD = 200;
    if (!this.isDebouncerActive('_checkTheshold')) {
      this.debounce('_checkTheshold', function() {
        this.checkScrollThesholds();
      }, THROTTLE_THRESHOLD);
    }
  },
  _initCheck: function(horizontal, isAttached) {
    if (isAttached) {
      this.debounce('_init', function() {
        this.clearTriggers();
        this.checkScrollThesholds();
      });
    }
  },
  checkScrollThesholds: function() {
    if (!this.scrollTarget || this.lowerTriggered && this.upperTriggered) {
      return;
    }
    var upperScrollValue = this.horizontal ? this._scrollLeft : this._scrollTop;
    var lowerScrollValue = this.horizontal ? this.scrollTarget.scrollWidth - this._scrollTargetWidth - this._scrollLeft : this.scrollTarget.scrollHeight - this._scrollTargetHeight - this._scrollTop;
    if (upperScrollValue <= this.upperThreshold && !this.upperTriggered) {
      this._setUpperTriggered(true);
      this.fire('upper-threshold');
    }
    if (lowerScrollValue <= this.lowerThreshold && !this.lowerTriggered) {
      this._setLowerTriggered(true);
      this.fire('lower-threshold');
    }
  },
  clearTriggers: function() {
    this._setUpperTriggered(false);
    this._setLowerTriggered(false);
  }
});

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'history-list',
  behaviors: [ HistoryListBehavior ],
  properties: {
    searchedTerm: {
      type: String,
      value: ''
    },
    querying: Boolean,
    historyData_: Array,
    resultLoadingDisabled_: {
      type: Boolean,
      value: false
    }
  },
  listeners: {
    scroll: 'notifyListScroll_',
    'remove-bookmark-stars': 'removeBookmarkStars_'
  },
  attached: function() {
    this.$['infinite-list'].notifyResize();
    this.$['infinite-list'].scrollTarget = this;
    this.$['scroll-threshold'].scrollTarget = this;
  },
  removeBookmarkStars_: function(e) {
    var url = e.detail;
    if (this.historyData_ === undefined) return;
    for (var i = 0; i < this.historyData_.length; i++) {
      if (this.historyData_[i].url == url) this.set('historyData_.' + i + '.starred', false);
    }
  },
  disableResultLoading: function() {
    this.resultLoadingDisabled_ = true;
  },
  addNewResults: function(historyResults, incremental) {
    var results = historyResults.slice();
    this.$['scroll-threshold'].clearTriggers();
    if (!incremental) {
      this.resultLoadingDisabled_ = false;
      if (this.historyData_) this.splice('historyData_', 0, this.historyData_.length);
      this.fire('unselect-all');
    }
    if (this.historyData_) {
      results.unshift('historyData_');
      this.push.apply(this, results);
    } else {
      this.set('historyData_', results);
    }
  },
  loadMoreData_: function() {
    if (this.resultLoadingDisabled_ || this.querying) return;
    this.fire('load-more-history');
  },
  needsTimeGap_: function(item, index, length) {
    return md_history.HistoryItem.needsTimeGap(this.historyData_, index, this.searchedTerm);
  },
  isCardStart_: function(item, i, length) {
    if (length == 0 || i > length - 1) return false;
    return i == 0 || this.historyData_[i].dateRelativeDay != this.historyData_[i - 1].dateRelativeDay;
  },
  isCardEnd_: function(item, i, length) {
    if (length == 0 || i > length - 1) return false;
    return i == length - 1 || this.historyData_[i].dateRelativeDay != this.historyData_[i + 1].dateRelativeDay;
  },
  isFirstItem_: function(index) {
    return index == 0;
  },
  notifyListScroll_: function() {
    this.fire('history-list-scrolled');
  },
  pathForItem_: function(index) {
    return 'historyData_.' + index;
  }
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'history-lazy-render',
  "extends": 'template',
  behaviors: [ Polymer.Templatizer ],
  _renderPromise: null,
  _instance: null,
  get: function() {
    if (!this._renderPromise) {
      this._renderPromise = new Promise(function(resolve) {
        this._debounceTemplate(function() {
          this._render();
          this._renderPromise = null;
          resolve(this.getIfExists());
        }.bind(this));
      }.bind(this));
    }
    return this._renderPromise;
  },
  getIfExists: function() {
    if (this._instance) {
      var children = this._instance._children;
      for (var i = 0; i < children.length; i++) {
        if (children[i].nodeType == Node.ELEMENT_NODE) return children[i];
      }
    }
    return null;
  },
  _render: function() {
    if (!this.ctor) this.templatize(this);
    var parentNode = this.parentNode;
    if (parentNode && !this._instance) {
      this._instance = this.stamp({});
      var root = this._instance.root;
      parentNode.insertBefore(root, this);
    }
  },
  _forwardParentProp: function(prop, value) {
    if (this._instance) this._instance.__setProperty(prop, value, true);
  },
  _forwardParentPath: function(path, value) {
    if (this._instance) this._instance._notifyPath(path, value, true);
  }
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'history-list-container',
  properties: {
    selectedPage_: String,
    grouped: Boolean,
    groupedRange: {
      type: Number,
      observer: 'groupedRangeChanged_'
    },
    queryState: Object,
    queryResult: Object
  },
  listeners: {
    'history-list-scrolled': 'closeMenu_',
    'load-more-history': 'loadMoreHistory_',
    'toggle-menu': 'toggleMenu_'
  },
  historyResult: function(info, results) {
    this.initializeResults_(info, results);
    this.closeMenu_();
    if (this.selectedPage_ == 'grouped-list') {
      this.$$('#grouped-list').historyData = results;
      return;
    }
    var list = this.$['infinite-list'];
    list.addNewResults(results, this.queryState.incremental);
    if (info.finished) list.disableResultLoading();
  },
  queryHistory: function(incremental) {
    var queryState = this.queryState;
    var noResults = !this.queryResult || this.queryResult.results == null;
    if (queryState.queryingDisabled || !this.queryState.searchTerm && noResults) {
      return;
    }
    var dialog = this.$.dialog.getIfExists();
    if (!incremental && dialog && dialog.open) dialog.close();
    this.set('queryState.querying', true);
    this.set('queryState.incremental', incremental);
    var lastVisitTime = 0;
    if (incremental) {
      var lastVisit = this.queryResult.results.slice(-1)[0];
      lastVisitTime = lastVisit ? lastVisit.time : 0;
    }
    var maxResults = this.groupedRange == HistoryRange.ALL_TIME ? RESULTS_PER_PAGE : 0;
    chrome.send('queryHistory', [ queryState.searchTerm, queryState.groupedOffset, queryState.range, lastVisitTime, maxResults ]);
  },
  historyDeleted: function() {
    if (this.getSelectedItemCount() > 0) return;
    this.queryHistory(false);
  },
  getContentScrollTarget: function() {
    return this.getSelectedList_();
  },
  getSelectedItemCount: function() {
    return this.getSelectedList_().selectedPaths.size;
  },
  unselectAllItems: function(count) {
    var selectedList = this.getSelectedList_();
    if (selectedList) selectedList.unselectAllItems(count);
  },
  deleteSelectedWithPrompt: function() {
    if (!loadTimeData.getBoolean('allowDeletingHistory')) return;
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordAction('RemoveSelected');
    if (this.queryState.searchTerm != '') browserService.recordAction('SearchResultRemove');
    this.$.dialog.get().then(function(dialog) {
      dialog.showModal();
    });
  },
  groupedRangeChanged_: function(range, oldRange) {
    this.selectedPage_ = range == HistoryRange.ALL_TIME ? 'infinite-list' : 'grouped-list';
    if (oldRange == undefined) return;
    this.queryHistory(false);
    this.fire('history-view-changed');
  },
  loadMoreHistory_: function() {
    this.queryHistory(true);
  },
  initializeResults_: function(info, results) {
    if (results.length == 0) return;
    var currentDate = results[0].dateRelativeDay;
    for (var i = 0; i < results.length; i++) {
      results[i].selected = false;
      results[i].readableTimestamp = info.term == '' ? results[i].dateTimeOfDay : results[i].dateShort;
      if (results[i].dateRelativeDay != currentDate) {
        currentDate = results[i].dateRelativeDay;
      }
    }
  },
  onDialogConfirmTap_: function() {
    md_history.BrowserService.getInstance().recordAction('ConfirmRemoveSelected');
    this.getSelectedList_().deleteSelected();
    var dialog = assert(this.$.dialog.getIfExists());
    dialog.close();
  },
  onDialogCancelTap_: function() {
    md_history.BrowserService.getInstance().recordAction('CancelRemoveSelected');
    var dialog = assert(this.$.dialog.getIfExists());
    dialog.close();
  },
  closeMenu_: function() {
    var menu = this.$.sharedMenu.getIfExists();
    if (menu) menu.closeMenu();
  },
  toggleMenu_: function(e) {
    var target = e.detail.target;
    return this.$.sharedMenu.get().then(function(menu) {
      menu.toggleMenu(target, e.detail);
    });
  },
  onMoreFromSiteTap_: function() {
    md_history.BrowserService.getInstance().recordAction('EntryMenuShowMoreFromSite');
    var menu = assert(this.$.sharedMenu.getIfExists());
    this.fire('search-domain', {
      domain: menu.itemData.item.domain
    });
    menu.closeMenu();
  },
  onRemoveFromHistoryTap_: function() {
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordAction('EntryMenuRemoveFromHistory');
    var menu = assert(this.$.sharedMenu.getIfExists());
    var itemData = menu.itemData;
    browserService.deleteItems([ itemData.item ]).then(function(items) {
      this.getSelectedList_().removeItemsByPath([ itemData.path ]);
      this.fire('unselect-all');
      var index = itemData.index;
      if (index == undefined) return;
      var browserService = md_history.BrowserService.getInstance();
      browserService.recordHistogram('HistoryPage.RemoveEntryPosition', index, UMA_MAX_BUCKET_VALUE);
      if (index <= UMA_MAX_SUBSET_BUCKET_VALUE) {
        browserService.recordHistogram('HistoryPage.RemoveEntryPositionSubset', index, UMA_MAX_SUBSET_BUCKET_VALUE);
      }
    }.bind(this));
    menu.closeMenu();
  },
  getSelectedList_: function() {
    return this.$.content.selectedItem;
  }
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'history-synced-device-card',
  properties: {
    device: String,
    lastUpdateTime: String,
    tabs: {
      type: Array,
      value: function() {
        return [];
      },
      observer: 'updateIcons_'
    },
    separatorIndexes: Array,
    opened: Boolean,
    searchTerm: String,
    sessionTag: String
  },
  openTab_: function(e) {
    var tab = e.model.tab;
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordHistogram(SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.LINK_CLICKED, SyncedTabsHistogram.LIMIT);
    browserService.openForeignSessionTab(this.sessionTag, tab.windowId, tab.sessionId, e);
    e.preventDefault();
  },
  toggleTabCard: function() {
    var histogramValue = this.$.collapse.opened ? SyncedTabsHistogram.COLLAPSE_SESSION : SyncedTabsHistogram.EXPAND_SESSION;
    md_history.BrowserService.getInstance().recordHistogram(SYNCED_TABS_HISTOGRAM_NAME, histogramValue, SyncedTabsHistogram.LIMIT);
    this.$.collapse.toggle();
    this.$['dropdown-indicator'].icon = this.$.collapse.opened ? 'cr:expand-less' : 'cr:expand-more';
  },
  updateIcons_: function() {
    this.async(function() {
      var icons = Polymer.dom(this.root).querySelectorAll('.website-icon');
      for (var i = 0; i < this.tabs.length; i++) {
        icons[i].style.backgroundImage = cr.icon.getFaviconImageSet(this.tabs[i].url);
      }
    });
  },
  isWindowSeparatorIndex_: function(index, separatorIndexes) {
    return this.separatorIndexes.indexOf(index) != -1;
  },
  getCollapseIcon_: function(opened) {
    return opened ? 'cr:expand-less' : 'cr:expand-more';
  },
  getCollapseTitle_: function(opened) {
    return opened ? loadTimeData.getString('collapseSessionButton') : loadTimeData.getString('expandSessionButton');
  },
  onMenuButtonTap_: function(e) {
    this.fire('toggle-menu', {
      target: Polymer.dom(e).localTarget,
      tag: this.sessionTag
    });
    e.stopPropagation();
  },
  onLinkRightClick_: function() {
    md_history.BrowserService.getInstance().recordHistogram(SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.LINK_RIGHT_CLICKED, SyncedTabsHistogram.LIMIT);
  }
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var ForeignDeviceInternal;

Polymer({
  is: 'history-synced-device-manager',
  properties: {
    sessionList: {
      type: Array,
      observer: 'updateSyncedDevices'
    },
    searchTerm: {
      type: String,
      observer: 'searchTermChanged'
    },
    syncedDevices_: {
      type: Array,
      value: function() {
        return [];
      }
    },
    signInState: {
      type: Boolean,
      observer: 'signInStateChanged_'
    },
    guestSession_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isGuestSession')
    },
    fetchingSyncedTabs_: {
      type: Boolean,
      value: false
    },
    hasSeenForeignData_: Boolean
  },
  listeners: {
    'toggle-menu': 'onToggleMenu_',
    scroll: 'onListScroll_'
  },
  attached: function() {
    chrome.send('otherDevicesInitialized');
    md_history.BrowserService.getInstance().recordHistogram(SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.INITIALIZED, SyncedTabsHistogram.LIMIT);
  },
  getContentScrollTarget: function() {
    return this;
  },
  createInternalDevice_: function(session) {
    var tabs = [];
    var separatorIndexes = [];
    for (var i = 0; i < session.windows.length; i++) {
      var windowId = session.windows[i].sessionId;
      var newTabs = session.windows[i].tabs;
      if (newTabs.length == 0) continue;
      newTabs.forEach(function(tab) {
        tab.windowId = windowId;
      });
      var windowAdded = false;
      if (!this.searchTerm) {
        tabs = tabs.concat(newTabs);
        windowAdded = true;
      } else {
        var searchText = this.searchTerm.toLowerCase();
        for (var j = 0; j < newTabs.length; j++) {
          var tab = newTabs[j];
          if (tab.title.toLowerCase().indexOf(searchText) != -1) {
            tabs.push(tab);
            windowAdded = true;
          }
        }
      }
      if (windowAdded && i != session.windows.length - 1) separatorIndexes.push(tabs.length - 1);
    }
    return {
      device: session.name,
      lastUpdateTime: '– ' + session.modifiedTime,
      opened: true,
      separatorIndexes: separatorIndexes,
      timestamp: session.timestamp,
      tabs: tabs,
      tag: session.tag
    };
  },
  onSignInTap_: function() {
    chrome.send('startSignInFlow');
  },
  onListScroll_: function() {
    var menu = this.$.menu.getIfExists();
    if (menu) menu.closeMenu();
  },
  onToggleMenu_: function(e) {
    this.$.menu.get().then(function(menu) {
      menu.toggleMenu(e.detail.target, e.detail.tag);
      if (menu.menuOpen) {
        md_history.BrowserService.getInstance().recordHistogram(SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.SHOW_SESSION_MENU, SyncedTabsHistogram.LIMIT);
      }
    });
  },
  onOpenAllTap_: function() {
    var menu = assert(this.$.menu.getIfExists());
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordHistogram(SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.OPEN_ALL, SyncedTabsHistogram.LIMIT);
    browserService.openForeignSessionAllTabs(menu.itemData);
    menu.closeMenu();
  },
  onDeleteSessionTap_: function() {
    var menu = assert(this.$.menu.getIfExists());
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordHistogram(SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.HIDE_FOR_NOW, SyncedTabsHistogram.LIMIT);
    browserService.deleteForeignSession(menu.itemData);
    menu.closeMenu();
  },
  clearDisplayedSyncedDevices_: function() {
    this.syncedDevices_ = [];
  },
  showNoSyncedMessage: function(signInState, syncedDevicesLength, guestSession) {
    if (guestSession) return true;
    return signInState && syncedDevicesLength == 0;
  },
  showSignInGuide: function(signInState, guestSession) {
    var show = !signInState && !guestSession;
    if (show) {
      md_history.BrowserService.getInstance().recordAction('Signin_Impression_FromRecentTabs');
    }
    return show;
  },
  noSyncedTabsMessage: function(fetchingSyncedTabs) {
    return loadTimeData.getString(fetchingSyncedTabs ? 'loading' : 'noSyncedResults');
  },
  updateSyncedDevices: function(sessionList) {
    this.fetchingSyncedTabs_ = false;
    if (!sessionList) return;
    if (sessionList.length > 0 && !this.hasSeenForeignData_) {
      this.hasSeenForeignData_ = true;
      md_history.BrowserService.getInstance().recordHistogram(SYNCED_TABS_HISTOGRAM_NAME, SyncedTabsHistogram.HAS_FOREIGN_DATA, SyncedTabsHistogram.LIMIT);
    }
    var updateCount = Math.min(sessionList.length, this.syncedDevices_.length);
    for (var i = 0; i < updateCount; i++) {
      var oldDevice = this.syncedDevices_[i];
      if (oldDevice.tag != sessionList[i].tag || oldDevice.timestamp != sessionList[i].timestamp) {
        this.splice('syncedDevices_', i, 1, this.createInternalDevice_(sessionList[i]));
      }
    }
    if (sessionList.length >= this.syncedDevices_.length) {
      for (var i = updateCount; i < sessionList.length; i++) {
        this.push('syncedDevices_', this.createInternalDevice_(sessionList[i]));
      }
    } else {
      this.splice('syncedDevices_', updateCount, this.syncedDevices_.length - updateCount);
    }
  },
  tabSyncDisabled: function() {
    this.fetchingSyncedTabs_ = false;
    this.clearDisplayedSyncedDevices_();
  },
  signInStateChanged_: function() {
    this.fire('history-view-changed');
    if (!this.signInState) {
      this.clearDisplayedSyncedDevices_();
      return;
    }
    this.fetchingSyncedTabs_ = true;
  },
  searchTermChanged: function(searchTerm) {
    this.clearDisplayedSyncedDevices_();
    this.updateSyncedDevices(this.sessionList);
  }
});

Polymer({
  is: 'iron-selector',
  behaviors: [ Polymer.IronMultiSelectableBehavior ]
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'history-side-bar',
  behaviors: [ Polymer.IronA11yKeysBehavior ],
  properties: {
    selectedPage: {
      type: String,
      notify: true
    },
    route: Object,
    showFooter: Boolean,
    drawer: {
      type: Boolean,
      reflectToAttribute: true
    }
  },
  keyBindings: {
    'space:keydown': 'onSpacePressed_'
  },
  onSpacePressed_: function(e) {
    e.detail.keyboardEvent.path[0].click();
  },
  onSelectorActivate_: function() {
    this.fire('history-close-drawer');
  },
  onClearBrowsingDataTap_: function(e) {
    var browserService = md_history.BrowserService.getInstance();
    browserService.recordAction('InitClearBrowsingData');
    browserService.openClearBrowsingData();
    e.preventDefault();
  },
  getQueryString_: function(route) {
    return window.location.search;
  }
});

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'history-app',
  behaviors: [ Polymer.IronScrollTargetBehavior ],
  properties: {
    showSidebarFooter: Boolean,
    selectedPage_: {
      type: String,
      observer: 'unselectAll'
    },
    grouped_: {
      type: Boolean,
      reflectToAttribute: true
    },
    queryState_: {
      type: Object,
      value: function() {
        return {
          incremental: false,
          querying: true,
          queryingDisabled: false,
          _range: HistoryRange.ALL_TIME,
          searchTerm: '',
          groupedOffset: 0,
          set range(val) {
            this._range = Number(val);
          },
          get range() {
            return this._range;
          }
        };
      }
    },
    queryResult_: {
      type: Object,
      value: function() {
        return {
          info: null,
          results: null,
          sessionList: null
        };
      }
    },
    routeData_: Object,
    queryParams_: Object,
    hasDrawer_: Boolean,
    isUserSignedIn_: {
      type: Boolean,
      value: loadTimeData.getBoolean('isUserSignedIn')
    },
    toolbarShadow_: {
      type: Boolean,
      reflectToAttribute: true,
      notify: true
    }
  },
  observers: [ 'routeDataChanged_(routeData_.page)', 'selectedPageChanged_(selectedPage_)', 'searchTermChanged_(queryState_.searchTerm)', 'searchQueryParamChanged_(queryParams_.q)' ],
  listeners: {
    'cr-menu-tap': 'onMenuTap_',
    'history-checkbox-select': 'checkboxSelected',
    'unselect-all': 'unselectAll',
    'delete-selected': 'deleteSelected',
    'search-domain': 'searchDomain_',
    'history-close-drawer': 'closeDrawer_',
    'history-view-changed': 'historyViewChanged_'
  },
  ready: function() {
    this.grouped_ = loadTimeData.getBoolean('groupByDomain');
    cr.ui.decorate('command', cr.ui.Command);
    document.addEventListener('canExecute', this.onCanExecute_.bind(this));
    document.addEventListener('command', this.onCommand_.bind(this));
    if (window.location.hash) {
      window.location.href = window.location.href.split('#')[0] + '?' + window.location.hash.substr(1);
    }
  },
  onFirstRender: function() {
    requestAnimationFrame(function() {
      chrome.send('metricsHandler:recordTime', [ 'History.ResultsRenderedTime', window.performance.now() ]);
    });
    if (!this.hasDrawer_) {
      this.focusToolbarSearchField();
    }
  },
  _scrollHandler: function() {
    this.toolbarShadow_ = this.scrollTarget.scrollTop != 0;
  },
  onMenuTap_: function() {
    var drawer = this.$$('#drawer');
    if (drawer) drawer.toggle();
  },
  checkboxSelected: function(e) {
    var toolbar = this.$.toolbar;
    toolbar.count = this.$.history.getSelectedItemCount();
  },
  unselectAll: function() {
    var listContainer = this.$.history;
    var toolbar = this.$.toolbar;
    listContainer.unselectAllItems(toolbar.count);
    toolbar.count = 0;
  },
  deleteSelected: function() {
    this.$.history.deleteSelectedWithPrompt();
  },
  historyResult: function(info, results) {
    this.set('queryState_.querying', false);
    this.set('queryResult_.info', info);
    this.set('queryResult_.results', results);
    var listContainer = this.$['history'];
    listContainer.historyResult(info, results);
  },
  focusToolbarSearchField: function() {
    this.$.toolbar.showSearchField();
  },
  searchDomain_: function(e) {
    this.$.toolbar.setSearchTerm(e.detail.domain);
  },
  onCanExecute_: function(e) {
    e = e;
    switch (e.command.id) {
     case 'find-command':
      e.canExecute = true;
      break;

     case 'slash-command':
      e.canExecute = !this.$.toolbar.searchBar.isSearchFocused();
      break;

     case 'delete-command':
      e.canExecute = this.$.toolbar.count > 0;
      break;
    }
  },
  searchTermChanged_: function(searchTerm) {
    this.set('queryParams_.q', searchTerm || null);
    this.$['history'].queryHistory(false);
    if (this.queryState_.searchTerm) md_history.BrowserService.getInstance().recordAction('Search');
  },
  searchQueryParamChanged_: function(searchQuery) {
    this.$.toolbar.setSearchTerm(searchQuery || '');
  },
  onCommand_: function(e) {
    if (e.command.id == 'find-command' || e.command.id == 'slash-command') this.focusToolbarSearchField();
    if (e.command.id == 'delete-command') this.deleteSelected();
  },
  setForeignSessions: function(sessionList, isTabSyncEnabled) {
    if (!isTabSyncEnabled) {
      var syncedDeviceManagerElem = this.$$('history-synced-device-manager');
      if (syncedDeviceManagerElem) syncedDeviceManagerElem.tabSyncDisabled();
      return;
    }
    this.set('queryResult_.sessionList', sessionList);
  },
  historyDeleted: function() {
    this.$.history.historyDeleted();
  },
  updateSignInState: function(isUserSignedIn) {
    this.isUserSignedIn_ = isUserSignedIn;
  },
  syncedTabsSelected_: function(selectedPage) {
    return selectedPage == 'syncedTabs';
  },
  shouldShowSpinner_: function(querying, incremental, searchTerm) {
    return querying && !incremental && searchTerm != '';
  },
  routeDataChanged_: function(page) {
    this.selectedPage_ = page;
  },
  selectedPageChanged_: function(selectedPage) {
    this.set('routeData_.page', selectedPage);
    this.historyViewChanged_();
  },
  historyViewChanged_: function() {
    requestAnimationFrame(function() {
      this.scrollTarget = this.$.content.selectedItem.getContentScrollTarget();
      this._scrollHandler();
    }.bind(this));
    this.recordHistoryPageView_();
  },
  getSelectedPage_: function(selectedPage, items) {
    return selectedPage;
  },
  closeDrawer_: function() {
    var drawer = this.$$('#drawer');
    if (drawer) drawer.close();
  },
  recordHistoryPageView_: function() {
    var histogramValue = HistoryPageViewHistogram.END;
    switch (this.selectedPage_) {
     case 'syncedTabs':
      histogramValue = this.isUserSignedIn_ ? HistoryPageViewHistogram.SYNCED_TABS : HistoryPageViewHistogram.SIGNIN_PROMO;
      break;

     default:
      switch (this.queryState_.range) {
       case HistoryRange.ALL_TIME:
        histogramValue = HistoryPageViewHistogram.HISTORY;
        break;

       case HistoryRange.WEEK:
        histogramValue = HistoryPageViewHistogram.GROUPED_WEEK;
        break;

       case HistoryRange.MONTH:
        histogramValue = HistoryPageViewHistogram.GROUPED_MONTH;
        break;
      }
      break;
    }
    md_history.BrowserService.getInstance().recordHistogram('History.HistoryPageView', histogramValue, HistoryPageViewHistogram.END);
  }
});
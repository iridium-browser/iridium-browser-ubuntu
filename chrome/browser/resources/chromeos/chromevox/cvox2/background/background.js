// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * background page.
 */

goog.provide('Background');

goog.require('AutomationPredicate');
goog.require('AutomationUtil');
goog.require('BackgroundKeyboardHandler');
goog.require('ChromeVoxState');
goog.require('CommandHandler');
goog.require('FindHandler');
goog.require('LiveRegions');
goog.require('NextEarcons');
goog.require('Notifications');
goog.require('Output');
goog.require('Output.EventType');
goog.require('PanelCommand');
goog.require('Stubs');
goog.require('constants');
goog.require('cursors.Cursor');
goog.require('cvox.BrailleKeyCommand');
goog.require('cvox.ChromeVoxBackground');
goog.require('cvox.ChromeVoxEditableTextBase');
goog.require('cvox.ClassicEarcons');
goog.require('cvox.ExtensionBridge');
goog.require('cvox.NavBraille');

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

/**
 * ChromeVox2 background page.
 * @constructor
 * @extends {ChromeVoxState}
 */
Background = function() {
  ChromeVoxState.call(this);

  /**
   * A list of site substring patterns to use with ChromeVox next. Keep these
   * strings relatively specific.
   * @type {!Array<string>}
   * @private
   */
  this.whitelist_ = ['chromevox_next_test'];

  /**
   * A list of site substring patterns to blacklist ChromeVox Classic,
   * putting ChromeVox into Compat mode.
   * @type {!Set<string>}
   * @private
   */
  this.classicBlacklist_ = new Set();

  /**
   * Regular expression for blacklisting classic.
   * @type {RegExp}
   * @private
   */
  this.classicBlacklistRegExp_ = Background.globsToRegExp_(
      chrome.runtime.getManifest()['content_scripts'][0]['exclude_globs']);

  /**
   * @type {cursors.Range}
   * @private
   */
  this.currentRange_ = null;

  /**
   * @type {cursors.Range}
   * @private
   */
  this.savedRange_ = null;

  // Manually bind all functions to |this|.
  for (var func in this) {
    if (typeof(this[func]) == 'function')
      this[func] = this[func].bind(this);
  }

  /** @type {!cvox.AbstractEarcons} @private */
  this.classicEarcons_ = cvox.ChromeVox.earcons || new cvox.ClassicEarcons();

  /** @type {!cvox.AbstractEarcons} @private */
  this.nextEarcons_ = new NextEarcons();

  // Turn cvox.ChromeVox.earcons into a getter that returns either the
  // Next earcons or the Classic earcons depending on the current mode.
  Object.defineProperty(cvox.ChromeVox, 'earcons', {
    get: (function() {
      if (this.mode === ChromeVoxMode.FORCE_NEXT ||
          this.mode === ChromeVoxMode.NEXT) {
        return this.nextEarcons_;
      } else {
        return this.classicEarcons_;
      }
    }).bind(this)
  });

  if (cvox.ChromeVox.isChromeOS) {
    Object.defineProperty(cvox.ChromeVox, 'modKeyStr', {
      get: function() {
        return (this.mode == ChromeVoxMode.CLASSIC ||
            this.mode == ChromeVoxMode.COMPAT) ?
                'Search+Shift' : 'Search';
      }.bind(this)
    });
  }

  Object.defineProperty(cvox.ChromeVox, 'isActive', {
    get: function() {
      return localStorage['active'] !== 'false';
    },
    set: function(value) {
      localStorage['active'] = value;
    }
  });

  cvox.ExtensionBridge.addMessageListener(this.onMessage_);

  /** @type {!BackgroundKeyboardHandler} */
  this.keyboardHandler_ = new BackgroundKeyboardHandler();

  /** @type {!LiveRegions} */
  this.liveRegions_ = new LiveRegions(this);

  /** @type {boolean} @private */
  this.inExcursion_ = false;

  /**
   * Stores the mode as computed the last time a current range was set.
   * @type {?ChromeVoxMode}
   */
  this.mode_ = null;

  chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
      this.onAccessibilityGesture_);
};

/**
 * @const {string}
 */
Background.ISSUE_URL = 'https://code.google.com/p/chromium/issues/entry?' +
    'labels=Type-Bug,Pri-2,cvox2,OS-Chrome&' +
    'components=UI>accessibility&' +
    'description=';

/**
 * Map from gesture names (AXGesture defined in ui/accessibility/ax_enums.idl)
 *     to commands when in Classic mode.
 * @type {Object<string, string>}
 * @const
 */
Background.GESTURE_CLASSIC_COMMAND_MAP = {
  'click': 'forceClickOnCurrentItem',
  'swipeUp1': 'backward',
  'swipeDown1': 'forward',
  'swipeLeft1': 'left',
  'swipeRight1': 'right',
  'swipeUp2': 'jumpToTop',
  'swipeDown2': 'readFromHere',
};

/**
 * Map from gesture names (AXGesture defined in ui/accessibility/ax_enums.idl)
 *     to commands when in Classic mode.
 * @type {Object<string, string>}
 * @const
 */
Background.GESTURE_NEXT_COMMAND_MAP = {
  'click': 'forceClickOnCurrentItem',
  'swipeUp1': 'previousLine',
  'swipeDown1': 'nextLine',
  'swipeLeft1': 'previousObject',
  'swipeRight1': 'nextObject',
  'swipeUp2': 'jumpToTop',
  'swipeDown2': 'readFromHere',
};

Background.prototype = {
  __proto__: ChromeVoxState.prototype,

  /**
   * @override
   */
  getMode: function() {
    if (localStorage['useNext'] == 'true')
      return ChromeVoxMode.FORCE_NEXT;

    var target;
    if (!this.getCurrentRange()) {
      chrome.automation.getFocus(function(focus) {
        target = focus;
      });
    } else {
      target = this.getCurrentRange().start.node;
    }

    if (!target)
      return ChromeVoxMode.CLASSIC;

    var root = target.root;
    if (root && this.isWhitelistedForCompat_(root.docUrl))
      return ChromeVoxMode.COMPAT;

    // Closure complains, but clearly, |target| is not null.
    var topLevelRoot =
        AutomationUtil.getTopLevelRoot(/** @type {!AutomationNode} */(target));
    if (!topLevelRoot)
      return ChromeVoxMode.COMPAT;
    if (this.isWhitelistedForCompat_(topLevelRoot.docUrl))
      return ChromeVoxMode.COMPAT;
    else if (this.isWhitelistedForNext_(topLevelRoot.docUrl))
      return ChromeVoxMode.NEXT;
    else
      return ChromeVoxMode.CLASSIC;
  },

  /**
   * Handles a mode change.
   * @param {ChromeVoxMode} newMode
   * @param {?ChromeVoxMode} oldMode Can be null at startup when no range was
   *  previously set.
   * @private
   */
  onModeChanged_: function(newMode, oldMode) {
    this.keyboardHandler_.onModeChanged(newMode, oldMode);
    CommandHandler.onModeChanged(newMode, oldMode);
    FindHandler.onModeChanged(newMode, oldMode);
    Notifications.onModeChange(newMode, oldMode);

    if (newMode == ChromeVoxMode.CLASSIC)
      chrome.accessibilityPrivate.setFocusRing([]);

    // note that |this.currentRange_| can *change* because the request is
    // async. Save it to ensure we're looking at the currentRange at this moment
    // in time.
    var cur = this.currentRange_;
    chrome.tabs.query({active: true,
                       lastFocusedWindow: true}, function(tabs) {
      if (newMode === ChromeVoxMode.CLASSIC) {
        // Generally, we don't want to inject classic content scripts as it is
        // done by the extension system at document load. The exception is when
        // we toggle classic on manually as part of a user command.
        if (oldMode == ChromeVoxMode.FORCE_NEXT)
          cvox.ChromeVox.injectChromeVoxIntoTabs(tabs);
      } else if (newMode === ChromeVoxMode.FORCE_NEXT) {
        // Disable ChromeVox everywhere.
        this.disableClassicChromeVox_();
      } else {
        // If we're focused in the desktop tree, do nothing.
        if (cur && !cur.isWebRange())
          return;

        // If we're entering compat mode or next mode for just one tab,
        // disable Classic for that tab only.
        this.disableClassicChromeVox_(tabs);
      }
    }.bind(this));

    // Switching into either compat or classic.
    if (oldMode === ChromeVoxMode.NEXT ||
        oldMode === ChromeVoxMode.FORCE_NEXT) {
      // Make sure we cancel the progress loading sound just in case.
      cvox.ChromeVox.earcons.cancelEarcon(cvox.Earcon.PAGE_START_LOADING);
      (new PanelCommand(PanelCommandType.DISABLE_MENUS)).send();
    }

    // Switching out of next, force next, or uninitialized (on startup).
    if (newMode === ChromeVoxMode.NEXT ||
        newMode === ChromeVoxMode.FORCE_NEXT) {
      (new PanelCommand(PanelCommandType.ENABLE_MENUS)).send();
      if (cvox.TabsApiHandler)
        cvox.TabsApiHandler.shouldOutputSpeechAndBraille = false;
    } else {
      // |newMode| is either classic or compat.
      if (cvox.TabsApiHandler)
        cvox.TabsApiHandler.shouldOutputSpeechAndBraille = true;
    }
  },

  /**
   * Toggles between force next and classic/compat modes.
   * This toggle automatically handles deciding between classic/compat based on
   * the start of the current range.
   * @param {boolean=} opt_setValue Directly set to force next (true) or
   *                                classic/compat (false).
   * @return {boolean} True to announce current position.
   */
  toggleNext: function(opt_setValue) {
    var useNext;
    if (opt_setValue !== undefined)
      useNext = opt_setValue;
    else
      useNext = localStorage['useNext'] != 'true';

    localStorage['useNext'] = useNext;
    if (useNext)
      this.setCurrentRangeToFocus_();
    else
      this.setCurrentRange(null);

    var announce = Msgs.getMsg(useNext ?
        'switch_to_next' : 'switch_to_classic');
    cvox.ChromeVox.tts.speak(
        announce, cvox.QueueMode.FLUSH, {doNotInterrupt: true});

    // If the new mode is Classic, return false now so we don't announce
    // anything more.
    return useNext;
  },

  /**
   * @override
   */
  getCurrentRange: function() {
    if (this.currentRange_ && this.currentRange_.isValid())
      return this.currentRange_;
    return null;
  },

  /**
   * @override
   */
  setCurrentRange: function(newRange) {
    if (!this.inExcursion_ && newRange)
      this.savedRange_ = new cursors.Range(newRange.start, newRange.end);

    this.currentRange_ = newRange;
    var oldMode = this.mode_;
    var newMode = this.getMode();
    if (oldMode != newMode) {
      this.onModeChanged_(newMode, oldMode);
      this.mode_ = newMode;
    }

    if (this.currentRange_) {
      var start = this.currentRange_.start.node;
      start.makeVisible();

      var root = start.root;
      if (!root || root.role == RoleType.desktop)
        return;

      var position = {};
      var loc = start.location;
      position.x = loc.left + loc.width / 2;
      position.y = loc.top + loc.height / 2;
      var url = root.docUrl;
      url = url.substring(0, url.indexOf('#')) || url;
      cvox.ChromeVox.position[url] = position;
    }
  },

  /**
   * Navigate to the given range - it both sets the range and outputs it.
   * @param {!cursors.Range} range The new range.
   * @param {boolean=} opt_focus Focus the range; defaults to true.
   * @param {Object=} opt_speechProps Speech properties.
   * @private
   */
  navigateToRange: function(range, opt_focus, opt_speechProps) {
    opt_focus = opt_focus === undefined ? true : opt_focus;
    opt_speechProps = opt_speechProps || {};

    if (opt_focus) {
      // TODO(dtseng): Figure out what it means to focus a range.
      var actionNode = range.start.node;
      if (actionNode.role == RoleType.inlineTextBox)
        actionNode = actionNode.parent;

      // Iframes, when focused, causes the child webArea to fire focus event.
      // This can result in getting stuck when navigating backward.
      if (actionNode.role != RoleType.iframe && !actionNode.state.focused &&
          !AutomationPredicate.container(actionNode))
        actionNode.focus();
    }
    var prevRange = this.currentRange_;
    this.setCurrentRange(range);

    var o = new Output();
    var selectedRange;
    if (this.pageSel_ &&
        this.pageSel_.isValid() &&
        range.isValid()) {
      // Compute the direction of the endpoints of each range.

      // Casts are ok because isValid checks node start and end nodes are
      // non-null; Closure just doesn't eval enough to see it.
      var startDir =
          AutomationUtil.getDirection(this.pageSel_.start.node,
              /** @type {!AutomationNode} */ (range.start.node));
      var endDir =
          AutomationUtil.getDirection(this.pageSel_.end.node,
              /** @type {!AutomationNode} */ (range.end.node));

      // Selection across roots isn't supported.
      var pageRootStart = this.pageSel_.start.node.root;
      var pageRootEnd = this.pageSel_.end.node.root;
      var curRootStart = range.start.node.root;
      var curRootEnd = range.end.node.root;

      // Disallow crossing over the start of the page selection and roots.
      if (startDir == Dir.BACKWARD ||
          pageRootStart != pageRootEnd ||
          pageRootStart != curRootStart ||
          pageRootEnd != curRootEnd) {
        o.format('@end_selection');
        this.pageSel_ = null;
      } else {
        // Expand or shrink requires different feedback.
        var msg;
        if (endDir == Dir.FORWARD &&
            (this.pageSel_.end.node != range.end.node ||
                this.pageSel_.end.index <= range.end.index)) {
          msg = '@selected';
        } else {
          msg = '@unselected';
          selectedRange = prevRange;
        }
        this.pageSel_ = new cursors.Range(
            this.pageSel_.start,
            range.end
            );
        if (this.pageSel_)
          this.pageSel_.select();
      }
    } else {
      range.select();
    }

    o.withRichSpeechAndBraille(
        selectedRange || range, prevRange, Output.EventType.NAVIGATE)
            .withQueueMode(cvox.QueueMode.FLUSH);

    if (msg)
      o.format(msg);

    for (var prop in opt_speechProps)
      o.format('!' + prop);

    o.go();
  },

  /**
   * Open the options page in a new tab.
   */
  showOptionsPage: function() {
    var optionsPage = {url: 'chromevox/background/options.html'};
    chrome.tabs.create(optionsPage);
  },

  /**
   * Handles a braille command.
   * @param {!cvox.BrailleKeyEvent} evt
   * @param {!cvox.NavBraille} content
   * @return {boolean} True if evt was processed.
   */
  onBrailleKeyEvent: function(evt, content) {
    if (this.mode === ChromeVoxMode.CLASSIC)
      return false;

    switch (evt.command) {
      case cvox.BrailleKeyCommand.PAN_LEFT:
        CommandHandler.onCommand('previousObject');
        break;
      case cvox.BrailleKeyCommand.PAN_RIGHT:
        CommandHandler.onCommand('nextObject');
        break;
      case cvox.BrailleKeyCommand.LINE_UP:
        CommandHandler.onCommand('previousLine');
        break;
      case cvox.BrailleKeyCommand.LINE_DOWN:
        CommandHandler.onCommand('nextLine');
        break;
      case cvox.BrailleKeyCommand.TOP:
        CommandHandler.onCommand('jumpToTop');
        break;
      case cvox.BrailleKeyCommand.BOTTOM:
        CommandHandler.onCommand('jumpToBottom');
        break;
      case cvox.BrailleKeyCommand.ROUTING:
        this.brailleRoutingCommand_(
            content.text,
            // Cast ok since displayPosition is always defined in this case.
            /** @type {number} */ (evt.displayPosition));
        break;
      default:
        return false;
    }
    return true;
  },

  /**
   * Returns true if the url should have Classic running.
   * @return {boolean}
   * @private
   */
  shouldEnableClassicForUrl_: function(url) {
    return this.mode != ChromeVoxMode.FORCE_NEXT &&
        !this.isBlacklistedForClassic_(url) &&
        !this.isWhitelistedForNext_(url);
  },

  /**
   * Compat mode is on if any of the following are true:
   * 1. a url is blacklisted for Classic.
   * 2. the current range is not within web content.
   * @param {string} url
   */
  isWhitelistedForCompat_: function(url) {
    return this.isBlacklistedForClassic_(url) || (this.getCurrentRange() &&
        !this.getCurrentRange().isWebRange() &&
        this.getCurrentRange().start.node.state.focused);
  },

  /**
   * @param {string} url
   * @return {boolean}
   * @private
   */
  isBlacklistedForClassic_: function(url) {
    if (this.classicBlacklistRegExp_.test(url))
      return true;
    url = url.substring(0, url.indexOf('#')) || url;
    return this.classicBlacklist_.has(url);
  },

  /**
   * @param {string} url
   * @return {boolean} Whether the given |url| is whitelisted.
   * @private
   */
  isWhitelistedForNext_: function(url) {
    return this.whitelist_.some(function(item) {
      return url.indexOf(item) != -1;
    });
  },

  /**
   * Disables classic ChromeVox in current web content.
   * @param {Array<Tab>=} opt_tabs The tabs where ChromeVox scripts should
   *     be disabled. If null, will disable ChromeVox everywhere.
   */
  disableClassicChromeVox_: function(opt_tabs) {
    var disableChromeVoxCommand = {
      message: 'SYSTEM_COMMAND',
      command: 'killChromeVox'
    };

    if (opt_tabs) {
      for (var i = 0, tab; tab = opt_tabs[i]; i++)
        chrome.tabs.sendMessage(tab.id, disableChromeVoxCommand);
    } else {
      // Send to all ChromeVox clients.
      cvox.ExtensionBridge.send(disableChromeVoxCommand);
    }
  },

  /**
   * @param {!Spannable} text
   * @param {number} position
   * @private
   */
  brailleRoutingCommand_: function(text, position) {
    var actionNodeSpan = null;
    var selectionSpan = null;
    text.getSpans(position).forEach(function(span) {
      if (span instanceof Output.SelectionSpan) {
        selectionSpan = span;
      } else if (span instanceof Output.NodeSpan) {
        if (!actionNodeSpan ||
            text.getSpanLength(span) <= text.getSpanLength(actionNodeSpan)) {
          actionNodeSpan = span;
        }
      }
    });
    if (!actionNodeSpan)
      return;
    var actionNode = actionNodeSpan.node;
    if (actionNode.role === RoleType.inlineTextBox)
      actionNode = actionNode.parent;
    actionNode.doDefault();
    if (selectionSpan) {
      var start = text.getSpanStart(selectionSpan);
      var targetPosition = position - start + selectionSpan.offset;
      actionNode.setSelection(targetPosition, targetPosition);
    }
  },

  /**
   * @param {Object} msg A message sent from a content script.
   * @param {Port} port
   * @private
   */
  onMessage_: function(msg, port) {
    var target = msg['target'];
    var action = msg['action'];

    switch (target) {
      case 'next':
        if (action == 'getIsClassicEnabled') {
          var url = msg['url'];
          var isClassicEnabled = this.shouldEnableClassicForUrl_(url);
          port.postMessage({
            target: 'next',
            isClassicEnabled: isClassicEnabled
          });
        } else if (action == 'enableCompatForUrl') {
          var url = msg['url'];
          this.classicBlacklist_.add(url);
          if (this.currentRange_ && this.currentRange_.start.node)
            this.setCurrentRange(this.currentRange_);
        } else if (action == 'onCommand') {
          CommandHandler.onCommand(msg['command']);
        } else if (action == 'flushNextUtterance') {
          Output.flushNextSpeechUtterance();
        }
        break;
    }
  },

  /**
   * Restore the range to the last range that was *not* in the ChromeVox
   * panel. This is used when the ChromeVox Panel closes.
   * @param {function()=} opt_callback
   * @private
   */
  restoreCurrentRange_: function(opt_callback) {
    if (this.savedRange_) {
      var node = this.savedRange_.start.node;
      var containingWebView = node;
      while (containingWebView && containingWebView.role != RoleType.webView)
        containingWebView = containingWebView.parent;

      if (containingWebView) {
        // Focusing the webView causes a focus change event which steals focus
        // away from the saved range.
        var saved = this.savedRange_;
        var setSavedRange = function(e) {
          if (e.target.root == saved.start.node.root) {
          this.navigateToRange(saved, false);
          opt_callback && opt_callback();
        }
          node.root.removeEventListener(EventType.focus, setSavedRange, true);
        }.bind(this);
        node.root.addEventListener(EventType.focus, setSavedRange, true);
        containingWebView.focus();
      }
      this.navigateToRange(this.savedRange_);
      this.savedRange_ = null;
    }
  },

  /**
   * Move ChromeVox without saving any ranges.
   */
  startExcursion: function() {
    this.inExcursion_ = true;
  },

  /**
   * Move ChromeVox back to the last saved range.
   * @param {function()=} opt_callback Called when range has been restored.
   */
  endExcursion: function(opt_callback) {
    this.inExcursion_ = false;
    this.restoreCurrentRange_(opt_callback);
  },

  /**
   * Move ChromeVox back to the last saved range.
   */
  saveExcursion: function() {
    this.savedRange_ =
        new cursors.Range(this.currentRange_.start, this.currentRange_.end);
  },

  /**
   * Handles accessibility gestures from the touch screen.
   * @param {string} gesture The gesture to handle, based on the AXGesture enum
   *     defined in ui/accessibility/ax_enums.idl
   * @private
   */
  onAccessibilityGesture_: function(gesture) {
    // If we're in classic mode, some gestures need to be handled by the
    // content script. Other gestures are universal and will be handled in
    // this function.
    if (this.mode == ChromeVoxMode.CLASSIC) {
      if (this.handleClassicGesture_(gesture))
        return;
    }

    var command = Background.GESTURE_NEXT_COMMAND_MAP[gesture];
    if (command)
      CommandHandler.onCommand(command);
  },

  /**
   * Handles accessibility gestures from the touch screen when in CLASSIC
   * mode, by forwarding a command to the content script.
   * @param {string} gesture The gesture to handle, based on the AXGesture enum
   *     defined in ui/accessibility/ax_enums.idl
   * @return {boolean} True if this gesture was handled.
   * @private
   */
  handleClassicGesture_: function(gesture) {
    var command = Background.GESTURE_CLASSIC_COMMAND_MAP[gesture];
    if (!command)
      return false;

    var msg = {
      'message': 'USER_COMMAND',
      'command': command
    };
    cvox.ExtensionBridge.send(msg);
    return true;
  },

  /** @private */
  setCurrentRangeToFocus_: function() {
    chrome.automation.getFocus(function(focus) {
      if (focus)
        this.setCurrentRange(cursors.Range.fromNode(focus));
      else
        this.setCurrentRange(null);
    }.bind(this));
  },
};

/**
 * Converts a list of globs, as used in the extension manifest, to a regular
 * expression that matches if and only if any of the globs in the list matches.
 * @param {!Array<string>} globs
 * @return {!RegExp}
 * @private
 */
Background.globsToRegExp_ = function(globs) {
  return new RegExp('^(' + globs.map(function(glob) {
    return glob.replace(/[.+^$(){}|[\]\\]/g, '\\$&')
        .replace(/\*/g, '.*')
        .replace(/\?/g, '.');
  }).join('|') + ')$');
};

new Background();

});  // goog.scope

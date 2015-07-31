// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Definitions for the Chromium extensions API used by ChromeVox.
 *
 * @externs
 */


/**
 * @const
 */
chrome.app = {};


/**
 * @const
 */
chrome.extension = {};


/** @type {!Object|undefined} */
chrome.extension.lastError = {};


/**
 * @type {string|undefined}
 */
chrome.extension.lastError.message;


/** @type {boolean|undefined} */
chrome.extension.inIncognitoContext;


/**
 * @param {string|Object<string>=} opt_extensionIdOrConnectInfo Either the
 *     extensionId to connect to, in which case connectInfo params can be
 *     passed in the next optional argument, or the connectInfo params.
 * @param {Object<string>=} opt_connectInfo The connectInfo object,
 *     if arg1 was the extensionId to connect to.
 * @return {Port} New port.
 */
chrome.extension.connect = function(
    opt_extensionIdOrConnectInfo, opt_connectInfo) {};


/**
 * @return {Window} The global JS object for the background page.
 */
chrome.extension.getBackgroundPage = function() {};


/**
 * @param {string} path A path to a resource within an extension expressed
 *     relative to it's install directory.
 * @return {string} The fully-qualified URL to the resource.
 */
chrome.extension.getURL = function(path) {};


/**
 * @param {function(boolean): void} callback Callback function.
 */
chrome.extension.isAllowedIncognitoAccess = function(callback) {};


/**
 * @param {string|*} extensionIdOrRequest Either the extensionId to send the
 *     request to, in which case the request is passed as the next arg, or the
 *     request.
 * @param {*=} opt_request The request value, if arg1 was the extensionId.
 * @param {function(*): void=} opt_callback The callback function which
 *     takes a JSON response object sent by the handler of the request.
 */
chrome.extension.sendMessage = function(
    extensionIdOrRequest, opt_request, opt_callback) {};


/** @type {ChromeEvent} */
chrome.extension.onConnect;


/** @type {ChromeEvent} */
chrome.extension.onConnectExternal;


/** @type {ChromeEvent} */
chrome.extension.onMessage;


/**
 * @const
 */
chrome.runtime = {};


/** @type {!Object|undefined} */
chrome.runtime.lastError = {};


/**
 * @type {string|undefined}
 */
chrome.runtime.lastError.message;


/** @type {string} */
chrome.runtime.id;


/**
 * @param {function(!Window=): void} callback Callback function.
 */
chrome.runtime.getBackgroundPage = function(callback) {};



/**
 * Manifest information returned from chrome.runtime.getManifest. See
 * http://developer.chrome.com/extensions/manifest.html. Note that there are
 * several other fields not included here. They should be added to these externs
 * as needed.
 * @constructor
 */
chrome.runtime.Manifest = function() {};


/** @type {string} */
chrome.runtime.Manifest.prototype.name;


/** @type {string} */
chrome.runtime.Manifest.prototype.version;


/** @type {number|undefined} */
chrome.runtime.Manifest.prototype.manifest_version;


/** @type {string|undefined} */
chrome.runtime.Manifest.prototype.description;


/** @type {!chrome.runtime.Manifest.Oauth2|undefined} */
chrome.runtime.Manifest.prototype.oauth2;



/**
 * Oauth2 info in the manifest.
 * See http://developer.chrome.com/apps/app_identity.html#update_manifest.
 * @constructor
 */
chrome.runtime.Manifest.Oauth2 = function() {};


/** @type {string} */
chrome.runtime.Manifest.Oauth2.prototype.client_id;

/**@type {!Array<string>} */
chrome.runtime.Manifest.Oauth2.prototype.scopes;


/**
 * http://developer.chrome.com/extensions/runtime.html#method-getManifest
 * @return {!chrome.runtime.Manifest} The full manifest file of the app or
 *     extension.
 */
chrome.runtime.getManifest = function() {};


/**
 * @param {string} path A path to a resource within an extension expressed
 *     relative to it's install directory.
 * @return {string} The fully-qualified URL to the resource.
 */
chrome.runtime.getURL = function(path) {};

/**
 * @param {string|!Object<string>=} opt_extensionIdOrConnectInfo Either the
 *     extensionId to connect to, in which case connectInfo params can be
 *     passed in the next optional argument, or the connectInfo params.
 * @param {!Object<string>=} opt_connectInfo The connectInfo object,
 *     if arg1 was the extensionId to connect to.
 * @return {!Port} New port.
 */
chrome.runtime.connect = function(
    opt_extensionIdOrConnectInfo, opt_connectInfo) {};


/**
 * @param {string|*} extensionIdOrMessage Either the extensionId to send the
 *     message to, in which case the message is passed as the next arg, or the
 *     message itself.
 * @param {(*|Object|function(*): void)=} opt_messageOrOptsOrCallback
 *     One of:
 *     The message, if arg1 was the extensionId.
 *     The options for message sending, if arg1 was the message and this
 *     argument is not a function.
 *     The callback, if arg1 was the message and this argument is a function.
 * @param {(Object|function(*): void)=} opt_optsOrCallback
 *     Either the options for message sending, if arg2 was the message,
 *     or the callback.
 * @param {function(*): void=} opt_callback The callback function which
 *     takes a JSON response object sent by the handler of the request.
 */
chrome.runtime.sendMessage = function(
    extensionIdOrMessage, opt_messageOrOptsOrCallback, opt_optsOrCallback,
    opt_callback) {};


/**
 *
 * @param {function(!Object)} callback
 */
chrome.runtime.getPlatformInfo = function(callback) {};


/** @type {!chrome.runtime.PortEvent} */
chrome.runtime.onConnect;


/** @type {!chrome.runtime.PortEvent} */
chrome.runtime.onConnectExternal;


/** @type {!chrome.runtime.MessageSenderEvent} */
chrome.runtime.onMessage;


/** @type {!chrome.runtime.MessageSenderEvent} */
chrome.runtime.onMessageExternal;


/**
 * Event whose listeners take a Port parameter.
 * @constructor
 */
chrome.runtime.PortEvent = function() {};


/**
 * @param {function(!Port): void} callback Callback.
 */
chrome.runtime.PortEvent.prototype.addListener = function(callback) {};


/**
 * @param {function(!Port): void} callback Callback.
 */
chrome.runtime.PortEvent.prototype.removeListener = function(callback) {};


/**
 * @param {function(!Port): void} callback Callback.
 * @return {boolean}
 */
chrome.runtime.PortEvent.prototype.hasListener = function(callback) {};


/**
 * @return {boolean}
 */
chrome.runtime.PortEvent.prototype.hasListeners = function() {};



/**
 * Event whose listeners take a MessageSender and additional parameters.
 * @constructor
 */
chrome.runtime.MessageSenderEvent = function() {};


/**
 * @param {function(*, !MessageSender, function(*): void): (boolean|undefined)}
 *     callback Callback.
 */
chrome.runtime.MessageSenderEvent.prototype.addListener = function(callback) {};


/**
 * @param {function(*, !MessageSender, function(*): void): (boolean|undefined)}
 *     callback Callback.
 */
chrome.runtime.MessageSenderEvent.prototype.removeListener = function(callback)
    {};


/**
 * @param {function(*, !MessageSender, function(*): void): (boolean|undefined)}
 *     callback Callback.
 * @return {boolean}
 */
chrome.runtime.MessageSenderEvent.prototype.hasListener = function(callback) {};


/**
 * @return {boolean}
 */
chrome.runtime.MessageSenderEvent.prototype.hasListeners = function() {};


/**
 * @const
 */
chrome.tabs = {};


/**
 * @param {number?} windowId Window Id.
 * @param {Object?} options parameters of image capture, such as the format of
 *    the resulting image.
 * @param {function(string): void} callback Callback function which accepts
 *    the data URL string of a JPEG encoding of the visible area of the
 *    captured tab. May be assigned to the 'src' property of an HTML Image
 *    element for display.
 */
chrome.tabs.captureVisibleTab = function(windowId, options, callback) {};


/**
 * @param {number} tabId Tab Id.
 * @param {Object<string>=} opt_connectInfo Info Object.
 */
chrome.tabs.connect = function(tabId, opt_connectInfo) {};


/**
 * @param {Object} createProperties Info object.
 * @param {function(Tab): void=} opt_callback The callback function.
 */
chrome.tabs.create = function(createProperties, opt_callback) {};


/**
 * @param {number?} tabId Tab id.
 * @param {function(string): void} callback Callback function.
 */
chrome.tabs.detectLanguage = function(tabId, callback) {};


/**
 * @param {number?} tabId Tab id.
 * @param {Object?} details An object which may have 'code', 'file',
 *    or 'allFrames' keys.
 * @param {function(): void=} opt_callback Callback function.
 */
chrome.tabs.executeScript = function(tabId, details, opt_callback) {};


/**
 * @param {number} tabId Tab id.
 * @param {function(Tab): void} callback Callback.
 */
chrome.tabs.get = function(tabId, callback) {};


/**
 * Note: as of 2012-04-12, this function is no longer documented on
 * the public web pages, but there are still existing usages
 *
 * @param {number?} windowId Window id.
 * @param {function(Array<Tab>): void} callback Callback.
 */
chrome.tabs.getAllInWindow = function(windowId, callback) {};


/**
 * @param {function(Tab): void} callback Callback.
 */
chrome.tabs.getCurrent = function(callback) {};


/**
 * Note: as of 2012-04-12, this function is no longer documented on
 * the public web pages, but there are still existing usages.
 *
 * @param {number?} windowId Window id.
 * @param {function(Tab): void} callback Callback.
 */
chrome.tabs.getSelected = function(windowId, callback) {};


/**
 * @param {Object<string, (number|Array<number>)>} highlightInfo
 *     An object with 'windowId' (number) and 'tabs'
 *     (number or array of numbers) keys.
 * @param {function(Window): void} callback Callback function invoked
 *    with each appropriate Window.
 */
chrome.tabs.highlight = function(highlightInfo, callback) {};


/**
 * @param {number?} tabId Tab id.
 * @param {Object?} details An object which may have 'code', 'file',
 *     or 'allFrames' keys.
 * @param {function(): void=} opt_callback Callback function.
 */
chrome.tabs.insertCSS = function(tabId, details, opt_callback) {};


/**
 * @param {number} tabId Tab id.
 * @param {Object<string, number>} moveProperties An object with 'index'
 *     and optional 'windowId' keys.
 * @param {function(Tab): void=} opt_callback Callback.
 */
chrome.tabs.move = function(tabId, moveProperties, opt_callback) {};


/**
 * @param {Object<string, (number|string)>} queryInfo An object which may have
 *     'active', 'pinned', 'highlighted', 'status', 'title', 'url', 'windowId',
 *     and 'windowType' keys.
 * @param {function(Array<Tab>): void=} opt_callback Callback.
 * @return {!Array<Tab>}
 */
chrome.tabs.query = function(queryInfo, opt_callback) {};


/**
 * @param {number=} opt_tabId Tab id.
 * @param {Object<string, boolean>=} opt_reloadProperties An object which
 *   may have a 'bypassCache' key.
 * @param {function(): void=} opt_callback The callback function invoked
 *    after the tab has been reloaded.
 */
chrome.tabs.reload = function(opt_tabId, opt_reloadProperties, opt_callback) {};


/**
 * @param {number|Array<number>} tabIds A tab ID or an array of tab IDs.
 * @param {function(Tab): void=} opt_callback Callback.
 */
chrome.tabs.remove = function(tabIds, opt_callback) {};


/**
 * @param {number} tabId Tab id.
 * @param {*} request The request value of any type.
 * @param {function(*): void=} opt_callback The callback function which
 *     takes a JSON response object sent by the handler of the request.
 */
chrome.tabs.sendMessage = function(tabId, request, opt_callback) {};


/**
 * @param {number} tabId Tab id.
 * @param {*} request The request value of any type.
 * @param {function(*): void=} opt_callback The callback function which
 *     takes a JSON response object sent by the handler of the request.
 */
chrome.tabs.sendRequest = function(tabId, request, opt_callback) {};


/**
 * @param {number} tabId Tab id.
 * @param {Object<string, (string|boolean)>} updateProperties An object which
 *     may have 'url' or 'selected' key.
 * @param {function(Tab): void=} opt_callback Callback.
 */
chrome.tabs.update = function(tabId, updateProperties, opt_callback) {};


/** @type {ChromeEvent} */
chrome.tabs.onActiveChanged;


/** @type {ChromeEvent} */
chrome.tabs.onActivated;


/** @type {ChromeEvent} */
chrome.tabs.onAttached;


/** @type {ChromeEvent} */
chrome.tabs.onCreated;


/** @type {ChromeEvent} */
chrome.tabs.onDetached;


/** @type {ChromeEvent} */
chrome.tabs.onHighlightChanged;


/** @type {ChromeEvent} */
chrome.tabs.onMoved;


/** @type {ChromeEvent} */
chrome.tabs.onRemoved;


/** @type {ChromeEvent} */
chrome.tabs.onUpdated;


/** @type {ChromeEvent} */
chrome.tabs.onReplaced;

/**
 * @const
 */
chrome.windows = {};


/**
 * @param {Object=} opt_createData May have many keys to specify parameters.
 *     Or the callback.
 * @param {function(ChromeWindow): void=} opt_callback Callback.
 */
chrome.windows.create = function(opt_createData, opt_callback) {};


/**
 * @param {number} id Window id.
 * @param {Object=} opt_getInfo May have 'populate' key. Or the callback.
 * @param {function(!ChromeWindow): void=} opt_callback Callback when
 *     opt_getInfo is an object.
 */
chrome.windows.get = function(id, opt_getInfo, opt_callback) {};


/**
 * @param {Object=} opt_getInfo May have 'populate' key. Or the callback.
 * @param {function(!Array<!ChromeWindow>): void=} opt_callback Callback.
 */
chrome.windows.getAll = function(opt_getInfo, opt_callback) {};


/**
 * @param {Object=} opt_getInfo May have 'populate' key. Or the callback.
 * @param {function(ChromeWindow): void=} opt_callback Callback.
 */
chrome.windows.getCurrent = function(opt_getInfo, opt_callback) { };


/**
 * @param {Object=} opt_getInfo May have 'populate' key. Or the callback.
 * @param {function(ChromeWindow): void=} opt_callback Callback.
 */
chrome.windows.getLastFocused = function(opt_getInfo, opt_callback) { };


/**
 * @param {number} tabId Tab Id.
 * @param {function(): void=} opt_callback Callback.
 */
chrome.windows.remove = function(tabId, opt_callback) {};


/**
 * @param {number} tabId Tab Id.
 * @param {Object} updateProperties An object which may have many keys for
 *     various options.
 * @param {function(): void=} opt_callback Callback.
 */
chrome.windows.update = function(tabId, updateProperties, opt_callback) {};


/** @type {ChromeEvent} */
chrome.windows.onCreated;


/** @type {ChromeEvent} */
chrome.windows.onFocusChanged;


/** @type {ChromeEvent} */
chrome.windows.onRemoved;


/**
 * @type {number}
 */
chrome.windows.WINDOW_ID_NONE;


/**
 * @type {number}
 */
chrome.windows.WINDOW_ID_CURRENT;


/**
 * @const
 */
chrome.i18n = {};


/**
 * @param {function(Array<string>): void} callback The callback function which
 *     accepts an array of the accept languages of the browser, such as
 *     'en-US','en','zh-CN'.
 */
chrome.i18n.getAcceptLanguages = function(callback) {};


/**
 * @param {string} messageName
 * @param {(string|Array<string>)=} opt_args
 * @return {string}
 */
chrome.i18n.getMessage = function(messageName, opt_args) {};


/**
 * Chrome Text-to-Speech API.
 * @const
 */
chrome.tts = {};



/**
 * An event from the TTS engine to communicate the status of an utterance.
 * @constructor
 */
function TtsEvent() {}


/** @type {string} */
TtsEvent.prototype.type;


/** @type {number} */
TtsEvent.prototype.charIndex;


/** @type {string} */
TtsEvent.prototype.errorMessage;



/**
 * A description of a voice available for speech synthesis.
 * @constructor
 */
function TtsVoice() {}


/** @type {string} */
TtsVoice.prototype.voiceName;


/** @type {string} */
TtsVoice.prototype.lang;


/** @type {string} */
TtsVoice.prototype.gender;


/** @type {string} */
TtsVoice.prototype.extensionId;


/** @type {Array<string>} */
TtsVoice.prototype.eventTypes;


/**
 * Gets an array of all available voices.
 * @param {function(Array<TtsVoice>)=} opt_callback An optional callback
 *     function.
 */
chrome.tts.getVoices = function(opt_callback) {};


/**
 * Checks if the engine is currently speaking.
 * @param {function(boolean)=} opt_callback The callback function.
 */
chrome.tts.isSpeaking = function(opt_callback) {};


/**
 * Speaks text using a text-to-speech engine.
 * @param {string} utterance The text to speak, either plain text or a complete,
 *     well-formed SSML document. Speech engines that do not support SSML will
 *     strip away the tags and speak the text. The maximum length of the text is
 *     32,768 characters.
 * @param {Object=} opt_options The speech options.
 * @param {function()=} opt_callback Called right away, before speech finishes.
 */
chrome.tts.speak = function(utterance, opt_options, opt_callback) {};


/**
 * Stops any current speech.
 */
chrome.tts.stop = function() {};


/**
 * @const
 */
chrome.history = {};


/**
 * @param {Object<string, string>} details Object with a 'url' key.
 */
chrome.history.addUrl = function(details) {};


/**
 * @param {function(): void} callback Callback function.
 */
chrome.history.deleteAll = function(callback) {};


/**
 * @param {Object<string, string>} range Object with 'startTime'
 *     and 'endTime' keys.
 * @param {function(): void} callback Callback function.
 */
chrome.history.deleteRange = function(range, callback) {};


/**
 * @param {Object<string, string>} details Object with a 'url' key.
 */
chrome.history.deleteUrl = function(details) {};


/**
 * @param {Object<string, string>} details Object with a 'url' key.
 * @param {function(!Array<!VisitItem>): void} callback Callback function.
 * @return {!Array<!VisitItem>}
 */
chrome.history.getVisits = function(details, callback) {};


/**
 * @param {Object<string, string>} query Object with a 'text' (string)
 *     key and optional 'startTime' (number), 'endTime' (number) and
 *     'maxResults' keys.
 * @param {function(!Array<!HistoryItem>): void} callback Callback function.
 * @return {!Array<!HistoryItem>}
 */
chrome.history.search = function(query, callback) {};


/** @type {ChromeEvent} */
chrome.history.onVisitRemoved;


/** @type {ChromeEvent} */
chrome.history.onVisited;


/**
 * @const
 */
chrome.permissions = {};


/**
 * @typedef {{
 *   permissions: (Array<string>|undefined),
 *   origins: (Array<string>|undefined)
 * }}
* @see http://developer.chrome.com/extensions/permissions.html#type-Permissions
*/
chrome.permissions.Permissions;


/**
 * @param {!chrome.permissions.Permissions} permissions
 * @param {function(boolean): void} callback Callback function.
 */
chrome.permissions.contains = function(permissions, callback) {};


/**
 * @param {function(!chrome.permissions.Permissions): void} callback
 *     Callback function.
 */
chrome.permissions.getAll = function(callback) {};


/**
 * @param {!chrome.permissions.Permissions} permissions
 * @param {function(boolean): void=} opt_callback Callback function.
 */
chrome.permissions.remove = function(permissions, opt_callback) {};


/**
 * @param {!chrome.permissions.Permissions} permissions
 * @param {function(boolean): void=} opt_callback Callback function.
 */
chrome.permissions.request = function(permissions, opt_callback) {};


/** @type {!ChromeEvent} */
chrome.permissions.onAdded;


/** @type {!ChromeEvent} */
chrome.permissions.onRemoved;


/**
 */
chrome.power = {};


/**
 * @param {string} level A string describing the degree to which power
 *     management should be disabled, should be either "system" or "display".
 */
chrome.power.requestKeepAwake = function(level) {};


/**
 * Releases a request previously made via requestKeepAwake().
 */
chrome.power.releaseKeepAwake = function() {};


/**
 * @constructor
 */
function Tab() {}


/** @type {number} */
Tab.prototype.id;


/** @type {number} */
Tab.prototype.index;


/** @type {number} */
Tab.prototype.windowId;


/** @type {number} */
Tab.prototype.openerTabId;


/** @type {boolean} */
Tab.prototype.highlighted;


/** @type {boolean} */
Tab.prototype.active;


/** @type {boolean} */
Tab.prototype.pinned;


/** @type {string} */
Tab.prototype.url;


/** @type {string} */
Tab.prototype.title;


/** @type {string} */
Tab.prototype.favIconUrl;


/** @type {string} */
Tab.prototype.status;


/** @type {boolean} */
Tab.prototype.incognito;



/**
 * @constructor
 */
function ChromeWindow() {}


/** @type {number} */
ChromeWindow.prototype.id;


/** @type {boolean} */
ChromeWindow.prototype.focused;


/** @type {number} */
ChromeWindow.prototype.top;


/** @type {number} */
ChromeWindow.prototype.left;


/** @type {number} */
ChromeWindow.prototype.width;


/** @type {number} */
ChromeWindow.prototype.height;


/** @type {Array<Tab>} */
ChromeWindow.prototype.tabs;


/** @type {boolean} */
ChromeWindow.prototype.incognito;


/** @type {string} */
ChromeWindow.prototype.type;


/** @type {string} */
ChromeWindow.prototype.state;


/** @type {boolean} */
ChromeWindow.prototype.alwaysOnTop;



/**
 * @constructor
 */
function ChromeEvent() {}


/** @param {Function} callback */
ChromeEvent.prototype.addListener = function(callback) {};


/** @param {Function} callback */
ChromeEvent.prototype.removeListener = function(callback) {};


/**
 * @param {Function} callback
 * @return {boolean}
 */
ChromeEvent.prototype.hasListener = function(callback) {};


/** @return {boolean} */
ChromeEvent.prototype.hasListeners = function() {};


/**
 * @constructor
 */
function Port() {}


/** @type {string} */
Port.prototype.name;


/** @type {ChromeEvent} */
Port.prototype.onDisconnect;


/** @type {ChromeEvent} */
Port.prototype.onMessage;


/** @type {MessageSender} */
Port.prototype.sender;


/**
 * @param {Object<string>} obj Message object.
 */
Port.prototype.postMessage = function(obj) {};


/**
 * Note: as of 2012-04-12, this function is no longer documented on
 * the public web pages, but there are still existing usages.
 */
Port.prototype.disconnect = function() {};



/**
 * @constructor
 */
function MessageSender() {}


/** @type {!Tab|undefined} */
MessageSender.prototype.tab;


/** @type {string|undefined} */
MessageSender.prototype.id;


/** @type {string|undefined} */
MessageSender.prototype.url;


/** @type {string|undefined} */
MessageSender.prototype.tlsChannelId;



/**
 * @constructor
 */
function BookmarkTreeNode() {}


/** @type {string} */
BookmarkTreeNode.prototype.id;


/** @type {string} */
BookmarkTreeNode.prototype.parentId;


/** @type {number} */
BookmarkTreeNode.prototype.index;


/** @type {string} */
BookmarkTreeNode.prototype.url;


/** @type {string} */
BookmarkTreeNode.prototype.title;


/** @type {number} */
BookmarkTreeNode.prototype.dateAdded;


/** @type {number} */
BookmarkTreeNode.prototype.dateGroupModified;


/** @type {Array<BookmarkTreeNode>} */
BookmarkTreeNode.prototype.children;



/**
 * @constructor
 */
function Cookie() {}


/** @type {string} */
Cookie.prototype.name;


/** @type {string} */
Cookie.prototype.value;


/** @type {string} */
Cookie.prototype.domain;


/** @type {boolean} */
Cookie.prototype.hostOnly;


/** @type {string} */
Cookie.prototype.path;


/** @type {boolean} */
Cookie.prototype.secure;


/** @type {boolean} */
Cookie.prototype.httpOnly;


/** @type {boolean} */
Cookie.prototype.session;


/** @type {number} */
Cookie.prototype.expirationDate;


/** @type {string} */
Cookie.prototype.storeId;



/**
 * @constructor
 */
function Debuggee() {}


/** @type {number} */
Debuggee.prototype.tabId;


/**
 * @constructor
 */
function HistoryItem() {}


/** @type {string} */
HistoryItem.prototype.id;


/** @type {string} */
HistoryItem.prototype.url;


/** @type {string} */
HistoryItem.prototype.title;


/** @type {number} */
HistoryItem.prototype.lastVisitTime;


/** @type {number} */
HistoryItem.prototype.visitCount;


/** @type {number} */
HistoryItem.prototype.typedCount;



/**
 * @constructor
 */
function VisitItem() {}


/** @type {string} */
VisitItem.prototype.id;


/** @type {string} */
VisitItem.prototype.visitId;


/** @type {number} */
VisitItem.prototype.visitTime;


/** @type {string} */
VisitItem.prototype.referringVisitId;


/** @type {string} */
VisitItem.prototype.transition;


/**
 * @const
 */
chrome.storage = {};


/**
 * @const
 */
chrome.storage.local = {};


/**
 * @param {string|!Object|null} keys
 * @param {function(Object, string)} callback
 */
chrome.storage.local.get = function(keys, callback) {};


/**
 * @param {Object} items
 * @param {function()=} opt_callback
 */
chrome.storage.local.set = function(items, opt_callback) {};


/**
 * @param {string|!Object|null} keys
 * @param {function()=} opt_callback
 */
chrome.storage.local.remove = function(keys, opt_callback) {};


/**
 * @type {ChromeEvent}
 */
chrome.storage.onChanged;


// Begin auto generated externs; do not edit.
// The following was generated from:
//
// python tools/json_schema_compiler/compiler.py
//     -g externs
//     chrome/common/extensions/api/automation.idl

/**
 * @const
 */
chrome.automation = {};

/**
 * @enum {string}
 */
chrome.automation.EventType = {
  activedescendantchanged: 'activedescendantchanged',
  alert: 'alert',
  ariaAttributeChanged: 'ariaAttributeChanged',
  autocorrectionOccured: 'autocorrectionOccured',
  blur: 'blur',
  checkedStateChanged: 'checkedStateChanged',
  childrenChanged: 'childrenChanged',
  focus: 'focus',
  hide: 'hide',
  hover: 'hover',
  invalidStatusChanged: 'invalidStatusChanged',
  layoutComplete: 'layoutComplete',
  liveRegionChanged: 'liveRegionChanged',
  loadComplete: 'loadComplete',
  locationChanged: 'locationChanged',
  menuEnd: 'menuEnd',
  menuListItemSelected: 'menuListItemSelected',
  menuListValueChanged: 'menuListValueChanged',
  menuPopupEnd: 'menuPopupEnd',
  menuPopupStart: 'menuPopupStart',
  menuStart: 'menuStart',
  rowCollapsed: 'rowCollapsed',
  rowCountChanged: 'rowCountChanged',
  rowExpanded: 'rowExpanded',
  scrollPositionChanged: 'scrollPositionChanged',
  scrolledToAnchor: 'scrolledToAnchor',
  selectedChildrenChanged: 'selectedChildrenChanged',
  selection: 'selection',
  selectionAdd: 'selectionAdd',
  selectionRemove: 'selectionRemove',
  show: 'show',
  textChanged: 'textChanged',
  textSelectionChanged: 'textSelectionChanged',
  treeChanged: 'treeChanged',
  valueChanged: 'valueChanged',
};

/**
 * @enum {string}
 */
chrome.automation.RoleType = {
  alertDialog: 'alertDialog',
  alert: 'alert',
  annotation: 'annotation',
  application: 'application',
  article: 'article',
  banner: 'banner',
  blockquote: 'blockquote',
  busyIndicator: 'busyIndicator',
  button: 'button',
  buttonDropDown: 'buttonDropDown',
  canvas: 'canvas',
  caption: 'caption',
  cell: 'cell',
  checkBox: 'checkBox',
  client: 'client',
  colorWell: 'colorWell',
  columnHeader: 'columnHeader',
  column: 'column',
  comboBox: 'comboBox',
  complementary: 'complementary',
  contentInfo: 'contentInfo',
  date: 'date',
  dateTime: 'dateTime',
  definition: 'definition',
  descriptionListDetail: 'descriptionListDetail',
  descriptionList: 'descriptionList',
  descriptionListTerm: 'descriptionListTerm',
  desktop: 'desktop',
  details: 'details',
  dialog: 'dialog',
  directory: 'directory',
  disclosureTriangle: 'disclosureTriangle',
  div: 'div',
  document: 'document',
  embeddedObject: 'embeddedObject',
  figcaption: 'figcaption',
  figure: 'figure',
  footer: 'footer',
  form: 'form',
  grid: 'grid',
  group: 'group',
  heading: 'heading',
  iframe: 'iframe',
  iframePresentational: 'iframePresentational',
  ignored: 'ignored',
  imageMapLink: 'imageMapLink',
  imageMap: 'imageMap',
  image: 'image',
  inlineTextBox: 'inlineTextBox',
  labelText: 'labelText',
  legend: 'legend',
  lineBreak: 'lineBreak',
  link: 'link',
  listBoxOption: 'listBoxOption',
  listBox: 'listBox',
  listItem: 'listItem',
  listMarker: 'listMarker',
  list: 'list',
  locationBar: 'locationBar',
  log: 'log',
  main: 'main',
  marquee: 'marquee',
  math: 'math',
  menuBar: 'menuBar',
  menuButton: 'menuButton',
  menuItem: 'menuItem',
  menuItemCheckBox: 'menuItemCheckBox',
  menuItemRadio: 'menuItemRadio',
  menuListOption: 'menuListOption',
  menuListPopup: 'menuListPopup',
  menu: 'menu',
  meter: 'meter',
  navigation: 'navigation',
  note: 'note',
  outline: 'outline',
  pane: 'pane',
  paragraph: 'paragraph',
  popUpButton: 'popUpButton',
  pre: 'pre',
  presentational: 'presentational',
  progressIndicator: 'progressIndicator',
  radioButton: 'radioButton',
  radioGroup: 'radioGroup',
  region: 'region',
  rootWebArea: 'rootWebArea',
  rowHeader: 'rowHeader',
  row: 'row',
  ruby: 'ruby',
  ruler: 'ruler',
  svgRoot: 'svgRoot',
  scrollArea: 'scrollArea',
  scrollBar: 'scrollBar',
  seamlessWebArea: 'seamlessWebArea',
  search: 'search',
  searchBox: 'searchBox',
  slider: 'slider',
  sliderThumb: 'sliderThumb',
  spinButtonPart: 'spinButtonPart',
  spinButton: 'spinButton',
  splitter: 'splitter',
  staticText: 'staticText',
  status: 'status',
  switch: 'switch',
  tabGroup: 'tabGroup',
  tabList: 'tabList',
  tabPanel: 'tabPanel',
  tab: 'tab',
  tableHeaderContainer: 'tableHeaderContainer',
  table: 'table',
  textField: 'textField',
  time: 'time',
  timer: 'timer',
  titleBar: 'titleBar',
  toggleButton: 'toggleButton',
  toolbar: 'toolbar',
  treeGrid: 'treeGrid',
  treeItem: 'treeItem',
  tree: 'tree',
  unknown: 'unknown',
  tooltip: 'tooltip',
  webArea: 'webArea',
  webView: 'webView',
  window: 'window',
};

/**
 * @enum {string}
 */
chrome.automation.StateType = {
  busy: 'busy',
  checked: 'checked',
  collapsed: 'collapsed',
  default: 'default',
  disabled: 'disabled',
  editable: 'editable',
  enabled: 'enabled',
  expanded: 'expanded',
  focusable: 'focusable',
  focused: 'focused',
  haspopup: 'haspopup',
  horizontal: 'horizontal',
  hovered: 'hovered',
  indeterminate: 'indeterminate',
  invisible: 'invisible',
  linked: 'linked',
  multiselectable: 'multiselectable',
  offscreen: 'offscreen',
  pressed: 'pressed',
  protected: 'protected',
  readOnly: 'readOnly',
  required: 'required',
  selectable: 'selectable',
  selected: 'selected',
  vertical: 'vertical',
  visited: 'visited',
};

/**
 * @enum {string}
 */
chrome.automation.TreeChangeType = {
  nodeCreated: 'nodeCreated',
  subtreeCreated: 'subtreeCreated',
  nodeChanged: 'nodeChanged',
  nodeRemoved: 'nodeRemoved',
};

/**
 * @typedef {{
 *   left: number,
 *   top: number,
 *   width: number,
 *   height: number
 * }}
 */
chrome.automation.Rect;

/**
 * @typedef {{
 *   role: (!chrome.automation.RoleType|undefined),
 *   state: (Object|undefined),
 *   attributes: (Object|undefined)
 * }}
 */
chrome.automation.FindParams;

/**
 * @constructor
 */
chrome.automation.AutomationEvent = function() {};

/**
 * @typedef {{
 *   target: chrome.automation.AutomationNode,
 *   type: !chrome.automation.TreeChangeType
 * }}
 */
chrome.automation.TreeChange;

/**
 * @constructor
 */
chrome.automation.AutomationNode = function() {};

/**
 * @typedef {{
 *   activedescendant: chrome.automation.AutomationNode
 * }}
 */
chrome.automation.ActiveDescendantMixin;

/**
 * @typedef {{
 *   url: string
 * }}
 */
chrome.automation.LinkMixins;

/**
 * @typedef {{
 *   docUrl: string,
 *   docTitle: string,
 *   docLoaded: boolean,
 *   docLoadingProgress: number
 * }}
 */
chrome.automation.DocumentMixins;

/**
 * @typedef {{
 *   scrollX: number,
 *   scrollXMin: number,
 *   scrollXMax: number,
 *   scrollY: number,
 *   scrollYMin: number,
 *   scrollYMax: number
 * }}
 */
chrome.automation.ScrollableMixins;

/**
 * @typedef {{
 *   textSelStart: number,
 *   textSelEnd: number
 * }}
 */
chrome.automation.EditableTextMixins;

/**
 * @typedef {{
 *   valueForRange: number,
 *   minValueForRange: number,
 *   maxValueForRange: number
 * }}
 */
chrome.automation.RangeMixins;

/**
 * @typedef {{
 *   tableRowCount: number,
 *   tableColumnCount: number
 * }}
 */
chrome.automation.TableMixins;

/**
 * @typedef {{
 *   tableCellColumnIndex: number,
 *   tableCellColumnSpan: number,
 *   tableCellRowIndex: number,
 *   tableCellRowSpan: number
 * }}
 */
chrome.automation.TableCellMixins;

/**
 * Get the automation tree for the tab with the given tabId, or the current tab
 * if no tabID is given, enabling automation if necessary. Returns a tree with a
 * placeholder root node; listen for the "loadComplete" event to get a
 * notification that the tree has fully loaded (the previous root node reference
 * will stop working at or before this point).
 * @param {number} tabId
 * @param {function(chrome.automation.AutomationNode):void} callback
 *     Called when the <code>AutomationNode</code> for the page is available.
 */
chrome.automation.getTree = function(tabId, callback) {};

/**
 * Get the automation tree for the whole desktop which consists of all on screen
 * views. Note this API is currently only supported on Chrome OS.
 * @param {function(chrome.automation.AutomationNode):void} callback
 *     Called when the <code>AutomationNode</code> for the page is available.
 */
chrome.automation.getDesktop = function(callback) {};

/**
 * Add a tree change observer. Tree change observers are static/global,
 * they listen to tree changes across all trees.
 * @param {function(chrome.automation.TreeChange):void} observer
 *     A listener for tree changes on the <code>AutomationNode</code> tree.
 */
chrome.automation.addTreeChangeObserver = function(observer) {};

/**
 * Remove a tree change observer.
 * @param {function(chrome.automation.TreeChange):void} observer
 *     A listener for tree changes on the <code>AutomationNode</code> tree.
 */
chrome.automation.removeTreeChangeObserver = function(observer) {};

//
// End auto generated externs; do not edit.
//



/**
 * @type {chrome.automation.RoleType}
 */
chrome.automation.AutomationNode.prototype.role;


/**
 * @type {!Object<chrome.automation.StateType, boolean>}
 */
chrome.automation.AutomationNode.prototype.state;


/**
 * @type {number}
 */
chrome.automation.AutomationNode.prototype.indexInParent;


/**
 * @type {{
 *     name: string,
 *     url: string,
 *     value: string,
 *     textSelStart: number,
 *     textSelEnd: number,
 *     wordStarts: Array<number>,
 *     wordEnds: Array<number>
 * }}
 */
chrome.automation.AutomationNode.prototype.attributes;


/**
 * @type {!chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.root;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.firstChild;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.lastChild;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.nextSibling;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.previousSibling;


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.AutomationNode.prototype.parent;


/**
 * @type {!Array<chrome.automation.AutomationNode>}
 */
chrome.automation.AutomationNode.prototype.children;


/**
 * @type {{top: number, left: number, height: number, width: number}}
 */
chrome.automation.AutomationNode.prototype.location;


/**
 * @param {chrome.automation.EventType} eventType
 * @param {function(chrome.automation.AutomationNode) : void} callback
 * @param {boolean} capture
 */
chrome.automation.AutomationNode.prototype.addEventListener =
    function(eventType, callback, capture) {};


/**
 * @param {chrome.automation.EventType} eventType
 * @param {function(chrome.automation.AutomationNode) : void} callback
 * @param {boolean} capture
 */
chrome.automation.AutomationNode.prototype.removeEventListener =
    function(eventType, callback, capture) {};


/**
 * @type {chrome.automation.AutomationNode}
 */
chrome.automation.TreeChange.prototype.target;


/**
 * @type {chrome.automation.TreeChangeType}
 */
chrome.automation.TreeChange.prototype.type;


/**
 * @param {function(chrome.automation.TreeChange) : void}
 *    callback
 */
chrome.automation.AutomationNode.prototype.addTreeChangeObserver =
    function(callback) {};


/**
 * @param {function(chrome.automation.TreeChange) : void}
 *    callback
 */
chrome.automation.AutomationNode.prototype.removeTreeChangeObserver =
    function(callback) {};


chrome.automation.AutomationNode.prototype.doDefault = function() {};


chrome.automation.AutomationNode.prototype.focus = function() {};

/** @type {string} */
chrome.automation.AutomationNode.prototype.containerLiveStatus;

/** @type {string} */
chrome.automation.AutomationNode.prototype.containerLiveRelevant;

/** @type {boolean} */
chrome.automation.AutomationNode.prototype.containerLiveAtomic;

/** @type {boolean} */
chrome.automation.AutomationNode.prototype.containerLiveBusy;


/**
 * @param {Object} findParams
 */
chrome.automation.AutomationNode.prototype.find = function(findParams) {};


/**
 * @const
 */
chrome.commands = {};


/**
 * @type {ChromeEvent}
 */
chrome.commands.onCommand;

/**
 * @param {function(Array<{description: string,
 *                         name: string,
 *                         shortcut: string}>): void} callback
 */
chrome.commands.getAll = function(callback) {};

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various hacks needed to inform JSCompiler of various
// WebKit- and Chrome-specific properties and methods. It is used only with
// JSCompiler to verify the type-correctness of our code.

/** @type {Object} */
chrome.app = {};

/** @type {Object} */
chrome.app.runtime = {
  /** @type {chrome.Event} */
  onLaunched: null
};


/** @type {Object} */
chrome.app.window = {
  /**
   * @param {string} name
   * @param {Object} parameters
   * @param {function(AppWindow)=} opt_callback
   */
  create: function(name, parameters, opt_callback) {},
  /**
   * @return {AppWindow}
   */
  current: function() {},
  /**
   * @param {string} id
   * @return {AppWindow}
   */
  get: function(id) {},
  /**
   * @return {Array<AppWindow>}
   */
  getAll: function() {}
};


/** @type {Object} */
chrome.runtime = {
  /** @type {Object} */
  lastError: {
    /** @type {string} */
    message: ''
  },
  /** @type {string} */
  id: '',
  /** @return {{name: string, version: string, app: {background: Object}}} */
  getManifest: function() {},
  /** @param {function(Window):void} callback */
  getBackgroundPage: function(callback) {},
  /** @type {chrome.Event} */
  onSuspend: null,
  /** @type {chrome.Event} */
  onSuspendCanceled: null,
  /** @type {chrome.Event} */
  onConnect: null,
  /** @type {chrome.Event} */
  onConnectExternal: null,
  /** @type {chrome.Event} */
  onMessage: null,
  /** @type {chrome.Event} */
  onMessageExternal: null
};

/**
 * @type {?function(string):chrome.runtime.Port}
 */
chrome.runtime.connectNative = function(name) {};

/**
 * @param {{ name: string}} config
 * @return {chrome.runtime.Port}
 */
chrome.runtime.connect = function(config) {};

/**
 * @param {string?} extensionId
 * @param {*} message
 * @param {Object=} opt_options
 * @param {function(*)=} opt_callback
 */
chrome.runtime.sendMessage = function(
    extensionId, message, opt_options, opt_callback) {};

/** @constructor */
chrome.runtime.MessageSender = function(){
  /** @type {chrome.Tab} */
  this.tab = null;
  /** @type {string} */
  this.id = '';
  /** @type {string} */
  this.url = '';
};

/** @constructor */
chrome.runtime.Port = function() {
  this.onMessage = new chrome.Event();
  this.onDisconnect = new chrome.Event();

  /** @type {string} */
  this.name = '';

  /** @type {chrome.runtime.MessageSender} */
  this.sender = null;
};

/** @type {chrome.Event} */
chrome.runtime.Port.prototype.onMessage = null;

/** @type {chrome.Event} */
chrome.runtime.Port.prototype.onDisconnect = null;

chrome.runtime.Port.prototype.disconnect = function() {};

/**
 * @param {Object} message
 */
chrome.runtime.Port.prototype.postMessage = function(message) {};


/** @type {Object} */
chrome.extension = {};

/**
 * @param {*} message
 * @param {function(*)=} opt_callback
 */
chrome.extension.sendMessage = function(message, opt_callback) {};

/** @type {chrome.Event} */
chrome.extension.onMessage;


/** @type {Object} */
chrome.i18n = {};

/**
 * @param {string} messageName
 * @param {(string|Array<string>)=} opt_args
 * @return {string}
 */
chrome.i18n.getMessage = function(messageName, opt_args) {};

/**
 * @return {string}
 */
chrome.i18n.getUILanguage = function() {};


/** @type {Object} */
chrome.storage = {};

/** @type {chrome.Storage} */
chrome.storage.local;

/** @type {chrome.Storage} */
chrome.storage.sync;

/** @constructor */
chrome.Storage = function() {};

/**
 * @param {string|Array<string>|Object<string>} items
 * @param {function(Object<string>):void} callback
 * @return {void}
 */
chrome.Storage.prototype.get = function(items, callback) {};

/**
 * @param {Object<string>} items
 * @param {function():void=} opt_callback
 * @return {void}
 */
chrome.Storage.prototype.set = function(items, opt_callback) {};

/**
 * @param {string|Array<string>} items
 * @param {function():void=} opt_callback
 * @return {void}
 */
chrome.Storage.prototype.remove = function(items, opt_callback) {};

/**
 * @param {function():void=} opt_callback
 * @return {void}
 */
chrome.Storage.prototype.clear = function(opt_callback) {};


/**
 * @type {Object}
 * src/chrome/common/extensions/api/context_menus.json
 */
chrome.contextMenus = {};
/** @type {chrome.Event} */
chrome.contextMenus.onClicked;
/**
 * @param {!Object} createProperties
 * @param {function()=} opt_callback
 */
chrome.contextMenus.create = function(createProperties, opt_callback) {};
/**
 * @param {string|number} id
 * @param {!Object} updateProperties
 * @param {function()=} opt_callback
 */
chrome.contextMenus.update = function(id, updateProperties, opt_callback) {};
/**
 * @param {string|number} menuItemId
 * @param {function()=} opt_callback
 */
chrome.contextMenus.remove = function(menuItemId, opt_callback) {};
/**
 * @param {function()=} opt_callback
 */
chrome.contextMenus.removeAll = function(opt_callback) {};

/** @constructor */
function OnClickData() {};
/** @type {string|number} */
OnClickData.prototype.menuItemId;
/** @type {string|number} */
OnClickData.prototype.parentMenuItemId;
/** @type {string} */
OnClickData.prototype.mediaType;
/** @type {string} */
OnClickData.prototype.linkUrl;
/** @type {string} */
OnClickData.prototype.srcUrl;
/** @type {string} */
OnClickData.prototype.pageUrl;
/** @type {string} */
OnClickData.prototype.frameUrl;
/** @type {string} */
OnClickData.prototype.selectionText;
/** @type {boolean} */
OnClickData.prototype.editable;
/** @type {boolean} */
OnClickData.prototype.wasChecked;
/** @type {boolean} */
OnClickData.prototype.checked;


/** @type {Object} */
chrome.fileSystem = {
  /**
   * @param {Object<string>?} options
   * @param {function(Entry, Array<FileEntry>):void} callback
   */
  chooseEntry: function(options, callback) {},
  /**
   * @param {Entry} entry
   * @param {function(string):void} callback
   */
  getDisplayPath: function(entry, callback) {}
};

/** @param {function(FileWriter):void} callback */
Entry.prototype.createWriter = function(callback) {};

/** @type {Object} */
chrome.identity = {
  /**
   * @param {Object<string>} parameters
   * @param {function(string):void} callback
   */
  getAuthToken: function(parameters, callback) {},
  /**
   * @param {Object<string>} parameters
   * @param {function():void} callback
   */
  removeCachedAuthToken: function(parameters, callback) {},
  /**
   * @param {Object<string>} parameters
   * @param {function(string):void} callback
   */
  launchWebAuthFlow: function(parameters, callback) {}
};


/** @type {Object} */
chrome.permissions = {
  /**
   * @param {Object<string>} permissions
   * @param {function(boolean):void} callback
   */
  contains: function(permissions, callback) {},
  /**
   * @param {Object<string>} permissions
   * @param {function(boolean):void} callback
   */
  request: function(permissions, callback) {}
};


/** @type {Object} */
chrome.tabs = {};

/** @param {function(chrome.Tab):void} callback */
chrome.tabs.getCurrent = function(callback) {};

/**
 * @param {Object?} options
 * @param {function(chrome.Tab)=} opt_callback
 */
chrome.tabs.create = function(options, opt_callback) {};

/**
 * @param {string} id
 * @param {function(chrome.Tab)} callback
 */
chrome.tabs.get = function(id, callback) {};

/**
 * @param {string} id
 * @param {function(*=):void=} opt_callback
 */
chrome.tabs.remove = function(id, opt_callback) {};


/** @constructor */
chrome.Tab = function() {
  /** @type {boolean} */
  this.pinned = false;
  /** @type {number} */
  this.windowId = 0;
  /** @type {string} */
  this.id = '';
};


/** @type {Object} */
chrome.windows = {};

/** @param {number} id
 *  @param {Object?} getInfo
 *  @param {function(chrome.Window):void} callback */
chrome.windows.get = function(id, getInfo, callback) {};

/** @constructor */
chrome.Window = function() {
  /** @type {string} */
  this.state = '';
  /** @type {string} */
  this.type = '';
};

/** @constructor */
var AppWindow = function() {
  /** @type {Window} */
  this.contentWindow = null;
  /** @type {chrome.Event} */
  this.onClosed = null;
  /** @type {chrome.Event} */
  this.onRestored = null;
  /** @type {chrome.Event} */
  this.onMaximized = null;
  /** @type {chrome.Event} */
  this.onMinimized = null;
  /** @type {chrome.Event} */
  this.onFullscreened = null;
  /** @type {string} */
  this.id = '';
  /** @type {Bounds} */
  this.outerBounds = null;
  /** @type {Bounds} */
  this.innerBounds = null;
};

AppWindow.prototype.close = function() {};
AppWindow.prototype.drawAttention = function() {};
AppWindow.prototype.focus = function() {};
AppWindow.prototype.maximize = function() {};
AppWindow.prototype.minimize = function() {};
/**
 * @param {number} left
 * @param {number} top
 */
AppWindow.prototype.moveTo = function(left, top) {};
/**
 * @param {number} width
 * @param {number} height
 */
AppWindow.prototype.resizeTo = function(width, height) {};

AppWindow.prototype.restore = function() {};
AppWindow.prototype.show = function() {};
/** @return {boolean} */
AppWindow.prototype.isMinimized = function() {};
AppWindow.prototype.fullscreen = function() {};
/** @return {boolean} */
AppWindow.prototype.isFullscreen = function() {};
/** @return {boolean} */
AppWindow.prototype.isMaximized = function() {};

/**
 * @param {{rects: Array<ClientRect>}} rects
 */
AppWindow.prototype.setShape = function(rects) {};

/**
 * @param {{rects: Array<ClientRect>}} rects
 */
AppWindow.prototype.setInputRegion = function(rects) {};

/** @constructor */
var LaunchData = function() {
  /** @type {string} */
  this.id = '';
  /** @type {Array<{type: string, entry: FileEntry}>} */
  this.items = [];
};

/** @constructor */
function Bounds() {
  /** @type {number} */
  this.left = 0;
  /** @type {number} */
  this.top = 0;
  /** @type {number} */
  this.width = 0;
  /** @type {number} */
  this.height = 0;
}

/** @type {Object} */
chrome.sockets = {};

/** @type {Object} */
chrome.sockets.tcp = {};

/** @constructor */
chrome.sockets.tcp.CreateInfo = function() {
  /** @type {number} */
  this.socketId = 0;
}

/**
 * @param {Object} properties
 * @param {function(chrome.sockets.tcp.CreateInfo):void} callback
 */
chrome.sockets.tcp.create = function(properties, callback) {};


/**
 * @param {number} socketId
 * @param {string} peerAddress
 * @param {number} peerPort
 * @param {function(number):void} callback
 */
chrome.sockets.tcp.connect =
    function(socketId, peerAddress, peerPort, callback) {};


/** @constructor */
chrome.sockets.tcp.SendInfo = function() {
  /** @type {number} */
  this.resultCode = 0;

  /** @type {number} */
  this.bytesSent = 0;
}

/**
 * @param {number} socketId
 * @param {ArrayBuffer} data
 * @param {function(chrome.sockets.tcp.SendInfo):void} callback
 */
chrome.sockets.tcp.send = function(socketId, data, callback) {};


/**
 * @param {number} socketId
 */
chrome.sockets.tcp.close = function(socketId) {};

/**
 * @param {number} socketId
 * @param {boolean} paused
 * @param {function(number):void=} callback
 */
chrome.sockets.tcp.setPaused = function(socketId, paused, callback) {};

/**
 * @param {number} socketId
 * @param {Object} options
 * @param {function(number):void} callback
 */
chrome.sockets.tcp.secure = function(socketId, options, callback) {};

/** @constructor */
chrome.sockets.tcp.ReceiveInfo = function() {
  /** @type {number} */
  this.socketId = 0;

  /** @type {ArrayBuffer} */
  this.data = null;
}

/** @type {chrome.Event} */
chrome.sockets.tcp.onReceive = null;

/** @constructor */
chrome.sockets.tcp.ReceiveErrorInfo = function() {
  /** @type {number} */
  this.socketId = 0;

  /** @type {number} */
  this.resultCode = 0;
}

/** @type {chrome.Event} */
chrome.sockets.tcp.onReceiveError = null;

/** @type {Object} */
chrome.socket = {};

/** @constructor */
chrome.socket.CreateInfo = function() {
  /** @type {number} */
  this.socketId = 0;
}

/**
 * @param {string} socketType
 * @param {Object} options
 * @param {function(chrome.socket.CreateInfo):void} callback
 */
chrome.socket.create = function(socketType, options, callback) {};

/**
 * @param {number} socketId
 * @param {string} hostname
 * @param {number} port
 * @param {function(number):void} callback
 */
chrome.socket.connect =
    function(socketId, hostname, port, callback) {};

/** @constructor */
chrome.socket.WriteInfo = function() {
  /** @type {number} */
  this.bytesWritten = 0;
}

/**
 * @param {number} socketId
 * @param {ArrayBuffer} data
 * @param {function(chrome.socket.WriteInfo):void} callback
 */
chrome.socket.write = function(socketId, data, callback) {};

/** @constructor */
chrome.socket.ReadInfo = function() {
  /** @type {number} */
  this.resultCode = 0;

  /** @type {ArrayBuffer} */
  this.data = null;
}

/**
 * @param {number} socketId
 * @param {function(chrome.socket.ReadInfo):void} callback
 */
chrome.socket.read = function(socketId, callback) {};

/**
 * @param {number} socketId
 */
chrome.socket.destroy = function(socketId) {};

/**
 * @param {number} socketId
 * @param {Object} options
 * @param {function(number):void} callback
 */
chrome.socket.secure = function(socketId, options, callback) {};

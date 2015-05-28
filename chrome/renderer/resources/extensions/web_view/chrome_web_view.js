// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <webview> API.
// See web_view_api_methods.js for details.

var ChromeWebView = require('chromeWebViewInternal').ChromeWebView;
var ChromeWebViewSchema =
    requireNative('schema_registry').GetSchema('chromeWebViewInternal');
var CreateEvent = require('guestViewEvents').CreateEvent;
var EventBindings = require('event_bindings');
var idGeneratorNatives = requireNative('id_generator');
var Utils = require('utils');
var WebViewImpl = require('webView').WebViewImpl;

// This is the only "webViewInternal.onClicked" named event for this renderer.
//
// Since we need an event per <webview>, we define events with suffix
// (subEventName) in each of the <webview>. Behind the scenes, this event is
// registered as a ContextMenusEvent, with filter set to the webview's
// |viewInstanceId|. Any time a ContextMenusEvent is dispatched, we re-dispatch
// it to the subEvent's listeners. This way
// <webview>.contextMenus.onClicked behave as a regular chrome Event type.
var ContextMenusEvent = CreateEvent('chromeWebViewInternal.onClicked');
// See comment above.
var ContextMenusHandlerEvent =
    CreateEvent('chromeWebViewInternal.onContextMenuShow');

// -----------------------------------------------------------------------------
// ContextMenusOnClickedEvent object.

// This event is exposed as <webview>.contextMenus.onClicked.
function ContextMenusOnClickedEvent(opt_eventName,
                                    opt_argSchemas,
                                    opt_eventOptions,
                                    opt_webViewInstanceId) {
  var subEventName = GetUniqueSubEventName(opt_eventName);
  EventBindings.Event.call(this,
                           subEventName,
                           opt_argSchemas,
                           opt_eventOptions,
                           opt_webViewInstanceId);

  // TODO(lazyboy): When do we dispose this listener?
  ContextMenusEvent.addListener(function() {
    // Re-dispatch to subEvent's listeners.
    $Function.apply(this.dispatch, this, $Array.slice(arguments));
  }.bind(this), {instanceId: opt_webViewInstanceId || 0});
}

ContextMenusOnClickedEvent.prototype.__proto__ = EventBindings.Event.prototype;

function ContextMenusOnContextMenuEvent(webViewImpl,
                                        opt_eventName,
                                        opt_argSchemas,
                                        opt_eventOptions,
                                        opt_webViewInstanceId) {
  var subEventName = GetUniqueSubEventName(opt_eventName);
  EventBindings.Event.call(this,
                           subEventName,
                           opt_argSchemas,
                           opt_eventOptions,
                           opt_webViewInstanceId);
  var defaultPrevented = false;
  ContextMenusHandlerEvent.addListener(function(e) {
    var defaultPrevented = false;
    var event = {
      'preventDefault': function() { defaultPrevented = true; }
    };

    // Re-dispatch to subEvent's listeners.
    $Function.apply(this.dispatch, this, [event]);

    if (!defaultPrevented) {
      // TODO(lazyboy): Remove |items| parameter completely from
      // ChromeWebView.showContextMenu as we don't do anything useful with it
      // currently.
      var items = [];
      ChromeWebView.showContextMenu(
          webViewImpl.guest.getId(), e.requestId, items);
    }
  }.bind(this), {instanceId: opt_webViewInstanceId || 0});
}

ContextMenusOnContextMenuEvent.prototype.__proto__ =
    EventBindings.Event.prototype;

// -----------------------------------------------------------------------------
// WebViewContextMenusImpl object.

// An instance of this class is exposed as <webview>.contextMenus.
function WebViewContextMenusImpl(viewInstanceId) {
  this.viewInstanceId_ = viewInstanceId;
}

WebViewContextMenusImpl.prototype.create = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusCreate, null, args);
};

WebViewContextMenusImpl.prototype.remove = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusRemove, null, args);
};

WebViewContextMenusImpl.prototype.removeAll = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusRemoveAll, null, args);
};

WebViewContextMenusImpl.prototype.update = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusUpdate, null, args);
};

var WebViewContextMenus = Utils.expose(
    'WebViewContextMenus', WebViewContextMenusImpl,
    { functions: ['create', 'remove', 'removeAll', 'update'] });

// -----------------------------------------------------------------------------

WebViewImpl.prototype.maybeSetupContextMenus = function() {
  if (!this.contextMenusOnContextMenuEvent_) {
    var eventName = 'chromeWebViewInternal.onContextMenuShow';
    var eventSchema =
        Utils.lookup(ChromeWebViewSchema.events, 'name', 'onShow');
    var eventOptions = {supportsListeners: true};
    this.contextMenusOnContextMenuEvent_ = new ContextMenusOnContextMenuEvent(
        this, eventName, eventSchema, eventOptions, this.viewInstanceId);
  }

  var createContextMenus = function() {
    return function() {
      if (this.contextMenus_) {
        return this.contextMenus_;
      }

      this.contextMenus_ = new WebViewContextMenus(this.viewInstanceId);

      // Define 'onClicked' event property on |this.contextMenus_|.
      var getOnClickedEvent = function() {
        return function() {
          if (!this.contextMenusOnClickedEvent_) {
            var eventName = 'chromeWebViewInternal.onClicked';
            var eventSchema =
                Utils.lookup(ChromeWebViewSchema.events, 'name', 'onClicked');
            var eventOptions = {supportsListeners: true};
            var onClickedEvent = new ContextMenusOnClickedEvent(
                eventName, eventSchema, eventOptions, this.viewInstanceId);
            this.contextMenusOnClickedEvent_ = onClickedEvent;
            return onClickedEvent;
          }
          return this.contextMenusOnClickedEvent_;
        }.bind(this);
      }.bind(this);
      $Object.defineProperty(
          this.contextMenus_,
          'onClicked',
          {get: getOnClickedEvent(), enumerable: true});
      $Object.defineProperty(
          this.contextMenus_,
          'onShow',
          {
            get: function() {
              return this.contextMenusOnContextMenuEvent_;
            }.bind(this),
            enumerable: true
          });
      return this.contextMenus_;
    }.bind(this);
  }.bind(this);

  // Expose <webview>.contextMenus object.
  // TODO(lazyboy): Add documentation for contextMenus:
  // http://crbug.com/470979.
  $Object.defineProperty(
      this.element,
      'contextMenus',
      {
        get: createContextMenus(),
        enumerable: true
      });
};

function GetUniqueSubEventName(eventName) {
  return eventName + '/' + idGeneratorNatives.GetNextId();
}

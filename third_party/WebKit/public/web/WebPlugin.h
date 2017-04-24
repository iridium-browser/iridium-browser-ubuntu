/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2014 Opera Software ASA. All rights reserved.
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

#ifndef WebPlugin_h
#define WebPlugin_h

#include "WebDragStatus.h"
#include "WebInputMethodController.h"
#include "public/platform/WebCanvas.h"
#include "public/platform/WebDragOperation.h"
#include "public/platform/WebFocusType.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "v8/include/v8.h"

namespace blink {

class WebDragData;
class WebInputEvent;
class WebPluginContainer;
class WebURLResponse;
struct WebCompositionUnderline;
struct WebCursorInfo;
struct WebPrintParams;
struct WebPrintPresetOptions;
struct WebPoint;
struct WebRect;
struct WebURLError;
template <typename T>
class WebVector;

class WebPlugin {
 public:
  // Initializes the plugin using |container| to communicate with the renderer
  // code. |container| must own this plugin. |container| must not be nullptr.
  //
  // Returns true if a plugin (not necessarily this one) has been successfully
  // initialized into |container|.
  //
  // NOTE: This method is subtle. This plugin may be marked for deletion via
  // destroy() during initialization. When this occurs, container() will
  // return nullptr. Because deletions during initialize() must be
  // asynchronous, this object is still alive immediately after initialize().
  //   1) If container() == nullptr and this method returns true, this plugin
  //      has been replaced by another during initialization. This new plugin
  //      may be accessed via container->plugin().
  //   2) If container() == nullptr and this method returns false, this plugin
  //      and the container have both been marked for deletion.
  virtual bool initialize(WebPluginContainer*) = 0;

  // Plugins must arrange for themselves to be deleted sometime during or after
  // this method is called. This method is only called by the owning
  // WebPluginContainer.
  // The exception is if the plugin has been detached by a WebPluginContainer,
  // i.e. been replaced by another plugin. Then it must be destroyed separately.
  // Once this method has been called, container() must return nullptr.
  virtual void destroy() = 0;

  // Returns the container that this plugin has been initialized with.
  // Must return nullptr if this plugin is scheduled for deletion.
  //
  // NOTE: This container doesn't necessarily own this plugin. For example,
  // if the container has been assigned a new plugin, then the container will
  // own the new plugin, not this old plugin.
  virtual WebPluginContainer* container() const { return nullptr; }

  virtual v8::Local<v8::Object> v8ScriptableObject(v8::Isolate*) {
    return v8::Local<v8::Object>();
  }

  virtual bool supportsKeyboardFocus() const { return false; }
  virtual bool supportsEditCommands() const { return false; }
  // Returns true if this plugin supports input method, which implements
  // setComposition(), commitText() and finishComposingText() below.
  virtual bool supportsInputMethod() const { return false; }

  virtual bool canProcessDrag() const { return false; }

  virtual void updateAllLifecyclePhases() = 0;
  virtual void paint(WebCanvas*, const WebRect&) = 0;

  // Coordinates are relative to the containing window.
  virtual void updateGeometry(const WebRect& windowRect,
                              const WebRect& clipRect,
                              const WebRect& unobscuredRect,
                              const WebVector<WebRect>& cutOutsRects,
                              bool isVisible) = 0;

  virtual void updateFocus(bool focused, WebFocusType) = 0;

  virtual void updateVisibility(bool) = 0;

  virtual WebInputEventResult handleInputEvent(const WebInputEvent&,
                                               WebCursorInfo&) = 0;

  virtual bool handleDragStatusUpdate(WebDragStatus,
                                      const WebDragData&,
                                      WebDragOperationsMask,
                                      const WebPoint& position,
                                      const WebPoint& screenPosition) {
    return false;
  }

  virtual void didReceiveResponse(const WebURLResponse&) = 0;
  virtual void didReceiveData(const char* data, int dataLength) = 0;
  virtual void didFinishLoading() = 0;
  virtual void didFailLoading(const WebURLError&) = 0;

  // Printing interface.
  // Whether the plugin supports its own paginated print. The other print
  // interface methods are called only if this method returns true.
  virtual bool supportsPaginatedPrint() { return false; }
  // Returns true if the printed content should not be scaled to
  // the printer's printable area.
  virtual bool isPrintScalingDisabled() { return false; }
  // Returns true on success and sets the out parameter to the print preset
  // options for the document.
  virtual bool getPrintPresetOptionsFromDocument(WebPrintPresetOptions*) {
    return false;
  }

  // Sets up printing with the specified printParams. Returns the number of
  // pages to be printed at these settings.
  virtual int printBegin(const WebPrintParams& printParams) { return 0; }

  virtual void printPage(int pageNumber, WebCanvas* canvas) {}

  // Ends the print operation.
  virtual void printEnd() {}

  virtual bool hasSelection() const { return false; }
  virtual WebString selectionAsText() const { return WebString(); }
  virtual WebString selectionAsMarkup() const { return WebString(); }

  virtual bool executeEditCommand(const WebString& name) { return false; }
  virtual bool executeEditCommand(const WebString& name,
                                  const WebString& value) {
    return false;
  }

  // Sets composition text from input method, and returns true if the
  // composition is set successfully.
  virtual bool setComposition(
      const WebString& text,
      const WebVector<WebCompositionUnderline>& underlines,
      int selectionStart,
      int selectionEnd) {
    return false;
  }

  // Deletes the ongoing composition if any, inserts the specified text, and
  // moves the caret according to relativeCaretPosition.
  virtual bool commitText(const WebString& text,
                          const WebVector<WebCompositionUnderline>& underlines,
                          int relativeCaretPosition) {
    return false;
  }

  // Confirms an ongoing composition; holds or moves selections accroding to
  // selectionBehavior.
  virtual bool finishComposingText(
      WebInputMethodController::ConfirmCompositionBehavior selectionBehavior) {
    return false;
  }

  // Deletes the current selection plus the specified number of characters
  // before and after the selection or caret.
  virtual void extendSelectionAndDelete(int before, int after) {}
  // Deletes text before and after the current cursor position, excluding the
  // selection. The lengths are supplied in UTF-16 Code Unit, not in code points
  // or in glyphs.
  virtual void deleteSurroundingText(int before, int after) {}
  // Deletes text before and after the current cursor position, excluding the
  // selection. The lengths are supplied in code points, not in UTF-16 Code Unit
  // or in glyphs. Do nothing if there are one or more invalid surrogate pairs
  // in the requested range.
  virtual void deleteSurroundingTextInCodePoints(int before, int after) {}
  // If the given position is over a link, returns the absolute url.
  // Otherwise an empty url is returned.
  virtual WebURL linkAtPosition(const WebPoint& position) const {
    return WebURL();
  }

  // Find interface.
  // Start a new search.  The plugin should search for a little bit at a time so
  // that it doesn't block the thread in case of a large document.  The results,
  // along with the find's identifier, should be sent asynchronously to
  // WebFrameClient's reportFindInPage* methods.
  // Returns true if the search started, or false if the plugin doesn't support
  // search.
  virtual bool startFind(const WebString& searchText,
                         bool caseSensitive,
                         int identifier) {
    return false;
  }
  // Tells the plugin to jump forward or backward in the list of find results.
  virtual void selectFindResult(bool forward, int identifier) {}
  // Tells the plugin that the user has stopped the find operation.
  virtual void stopFind() {}

  // View rotation types.
  enum RotationType { RotationType90Clockwise, RotationType90Counterclockwise };
  // Whether the plugin can rotate the view of its content.
  virtual bool canRotateView() { return false; }
  // Rotates the plugin's view of its content.
  virtual void rotateView(RotationType type) {}

  virtual bool isPlaceholder() { return true; }

 protected:
  ~WebPlugin() {}
};

}  // namespace blink

#endif

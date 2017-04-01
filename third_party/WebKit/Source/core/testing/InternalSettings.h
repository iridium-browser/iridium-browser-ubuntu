/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InternalSettings_h
#define InternalSettings_h

#include "core/editing/EditingBehaviorTypes.h"
#include "core/page/Page.h"
#include "core/testing/InternalSettingsGenerated.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/ImageAnimationPolicy.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebDisplayMode.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class Page;
class Settings;

class InternalSettings final : public InternalSettingsGenerated,
                               public Supplement<Page> {
  USING_GARBAGE_COLLECTED_MIXIN(InternalSettings);
  DEFINE_WRAPPERTYPEINFO();

 public:
  class Backup {
    DISALLOW_NEW();

   public:
    explicit Backup(Settings*);
    void restoreTo(Settings*);

    bool m_originalCSP;
    bool m_originalCSSStickyPositionEnabled;
    bool m_originalOverlayScrollbarsEnabled;
    EditingBehaviorType m_originalEditingBehavior;
    bool m_originalTextAutosizingEnabled;
    IntSize m_originalTextAutosizingWindowSizeOverride;
    float m_originalAccessibilityFontScaleFactor;
    String m_originalMediaTypeOverride;
    WebDisplayMode m_originalDisplayModeOverride;
    bool m_originalMockScrollbarsEnabled;
    bool m_originalMockGestureTapHighlightsEnabled;
    bool m_langAttributeAwareFormControlUIEnabled;
    bool m_imagesEnabled;
    String m_defaultVideoPosterURL;
    bool m_originalLayerSquashingEnabled;
    ImageAnimationPolicy m_originalImageAnimationPolicy;
    bool m_originalScrollTopLeftInteropEnabled;
    bool m_originalCompositorWorkerEnabled;
  };

  static InternalSettings* create(Page& page) {
    return new InternalSettings(page);
  }
  static InternalSettings* from(Page&);

  ~InternalSettings() override;
  void resetToConsistentState();

  void setStandardFontFamily(const AtomicString& family,
                             const String& script,
                             ExceptionState&);
  void setSerifFontFamily(const AtomicString& family,
                          const String& script,
                          ExceptionState&);
  void setSansSerifFontFamily(const AtomicString& family,
                              const String& script,
                              ExceptionState&);
  void setFixedFontFamily(const AtomicString& family,
                          const String& script,
                          ExceptionState&);
  void setCursiveFontFamily(const AtomicString& family,
                            const String& script,
                            ExceptionState&);
  void setFantasyFontFamily(const AtomicString& family,
                            const String& script,
                            ExceptionState&);
  void setPictographFontFamily(const AtomicString& family,
                               const String& script,
                               ExceptionState&);

  void setDefaultVideoPosterURL(const String& url, ExceptionState&);
  void setEditingBehavior(const String&, ExceptionState&);
  void setImagesEnabled(bool, ExceptionState&);
  void setMediaTypeOverride(const String& mediaType, ExceptionState&);
  void setDisplayModeOverride(const String& displayMode, ExceptionState&);
  void setMockScrollbarsEnabled(bool, ExceptionState&);
  void setHideScrollbars(bool, ExceptionState&);
  void setMockGestureTapHighlightsEnabled(bool, ExceptionState&);
  void setTextAutosizingEnabled(bool, ExceptionState&);
  void setTextTrackKindUserPreference(const String& preference,
                                      ExceptionState&);
  void setAccessibilityFontScaleFactor(float fontScaleFactor, ExceptionState&);
  void setTextAutosizingWindowSizeOverride(int width,
                                           int height,
                                           ExceptionState&);
  void setViewportEnabled(bool, ExceptionState&);
  void setViewportMetaEnabled(bool, ExceptionState&);
  void setViewportStyle(const String& preference, ExceptionState&);
  void setCompositorWorkerEnabled(bool, ExceptionState&);
  void setPresentationReceiver(bool, ExceptionState&);

  // FIXME: The following are RuntimeEnabledFeatures and likely
  // cannot be changed after process start. These setters should
  // be removed or moved onto internals.runtimeFlags:
  void setCSSStickyPositionEnabled(bool);
  void setLangAttributeAwareFormControlUIEnabled(bool);
  void setOverlayScrollbarsEnabled(bool);
  void setExperimentalContentSecurityPolicyFeaturesEnabled(bool);
  void setImageAnimationPolicy(const String&, ExceptionState&);
  void setScrollTopLeftInteropEnabled(bool);

  DECLARE_VIRTUAL_TRACE();

  void setAvailablePointerTypes(const String&, ExceptionState&);
  void setPrimaryPointerType(const String&, ExceptionState&);
  void setAvailableHoverTypes(const String&, ExceptionState&);
  void setPrimaryHoverType(const String&, ExceptionState&);
  void setDnsPrefetchLogging(bool, ExceptionState&);
  void setPreloadLogging(bool, ExceptionState&);

 private:
  explicit InternalSettings(Page&);

  Settings* settings() const;
  Page* page() const { return m_page; }
  static const char* supplementName();

  WeakMember<Page> m_page;
  Backup m_backup;
};

}  // namespace blink

#endif  // InternalSettings_h

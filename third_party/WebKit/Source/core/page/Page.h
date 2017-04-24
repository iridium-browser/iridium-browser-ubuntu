/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef Page_h
#define Page_h

#include "core/CoreExport.h"
#include "core/dom/ViewportDescription.h"
#include "core/frame/Deprecation.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/SettingsDelegate.h"
#include "core/frame/UseCounter.h"
#include "core/page/Page.h"
#include "core/page/PageAnimator.h"
#include "core/page/PageVisibilityNotifier.h"
#include "core/page/PageVisibilityObserver.h"
#include "core/page/PageVisibilityState.h"
#include "platform/Supplementable.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/Region.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"

namespace blink {

class AutoscrollController;
class ChromeClient;
class ClientRectList;
class ContextMenuClient;
class ContextMenuController;
class Document;
class DragCaret;
class DragController;
class EditorClient;
class FocusController;
class Frame;
class FrameHost;
struct PageScaleConstraints;
class PageScaleConstraintsSet;
class PluginData;
class PointerLockController;
class ScopedPageSuspender;
class ScrollingCoordinator;
class Settings;
class SpellCheckerClient;
class ValidationMessageClient;
class WebLayerTreeView;

typedef uint64_t LinkHash;

float deviceScaleFactorDeprecated(LocalFrame*);

class CORE_EXPORT Page final : public GarbageCollectedFinalized<Page>,
                               public Supplementable<Page>,
                               public PageVisibilityNotifier,
                               public SettingsDelegate {
  USING_GARBAGE_COLLECTED_MIXIN(Page);
  WTF_MAKE_NONCOPYABLE(Page);
  friend class Settings;

 public:
  // It is up to the platform to ensure that non-null clients are provided where
  // required.
  struct CORE_EXPORT PageClients final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(PageClients);

   public:
    PageClients();
    ~PageClients();

    Member<ChromeClient> chromeClient;
    ContextMenuClient* contextMenuClient;
    EditorClient* editorClient;
    SpellCheckerClient* spellCheckerClient;
  };

  static Page* create(PageClients& pageClients) {
    return new Page(pageClients);
  }

  // An "ordinary" page is a fully-featured page owned by a web view.
  static Page* createOrdinary(PageClients&);

  ~Page() override;

  void closeSoon();
  bool isClosing() const { return m_isClosing; }

  using PageSet = PersistentHeapHashSet<WeakMember<Page>>;

  // Return the current set of full-fledged, ordinary pages.
  // Each created and owned by a WebView.
  //
  // This set does not include Pages created for other, internal purposes
  // (SVGImages, inspector overlays, page popups etc.)
  static PageSet& ordinaryPages();

  static void platformColorsChanged();

  // TODO(sashab): Remove this.
  FrameHost& frameHost() const { return *m_frameHost; }

  void setNeedsRecalcStyleInAllFrames();
  void updateAcceleratedCompositingSettings();

  ViewportDescription viewportDescription() const;

  static void refreshPlugins();
  PluginData* pluginData(SecurityOrigin* mainFrameOrigin) const;

  EditorClient& editorClient() const { return *m_editorClient; }
  SpellCheckerClient& spellCheckerClient() const {
    return *m_spellCheckerClient;
  }

  void setMainFrame(Frame*);
  Frame* mainFrame() const { return m_mainFrame; }
  // Escape hatch for existing code that assumes that the root frame is
  // always a LocalFrame. With OOPI, this is not always the case. Code that
  // depends on this will generally have to be rewritten to propagate any
  // necessary state through all renderer processes for that page and/or
  // coordinate/rely on the browser process to help dispatch/coordinate work.
  LocalFrame* deprecatedLocalMainFrame() const {
    return toLocalFrame(m_mainFrame);
  }

  void willUnloadDocument(const Document&);
  void documentDetached(Document*);

  bool openedByDOM() const;
  void setOpenedByDOM();

  PageAnimator& animator() { return *m_animator; }
  ChromeClient& chromeClient() const { return *m_chromeClient; }
  AutoscrollController& autoscrollController() const {
    return *m_autoscrollController;
  }
  DragCaret& dragCaret() const { return *m_dragCaret; }
  DragController& dragController() const { return *m_dragController; }
  FocusController& focusController() const { return *m_focusController; }
  ContextMenuController& contextMenuController() const {
    return *m_contextMenuController;
  }
  PointerLockController& pointerLockController() const {
    return *m_pointerLockController;
  }
  ValidationMessageClient& validationMessageClient() const {
    return *m_validationMessageClient;
  }
  void setValidationMessageClient(ValidationMessageClient*);

  ScrollingCoordinator* scrollingCoordinator();

  ClientRectList* nonFastScrollableRects(const LocalFrame*);

  Settings& settings() const { return *m_settings; }

  UseCounter& useCounter() { return m_useCounter; }
  Deprecation& deprecation() { return m_deprecation; }
  HostsUsingFeatures& hostsUsingFeatures() { return m_hostsUsingFeatures; }

  PageScaleConstraintsSet& pageScaleConstraintsSet();
  const PageScaleConstraintsSet& pageScaleConstraintsSet() const;

  void setTabKeyCyclesThroughElements(bool b) {
    m_tabKeyCyclesThroughElements = b;
  }
  bool tabKeyCyclesThroughElements() const {
    return m_tabKeyCyclesThroughElements;
  }

  // Suspension is used to implement the "Optionally, pause while waiting for
  // the user to acknowledge the message" step of simple dialog processing:
  // https://html.spec.whatwg.org/multipage/webappapis.html#simple-dialogs
  //
  // Per https://html.spec.whatwg.org/multipage/webappapis.html#pause, no loads
  // are allowed to start/continue in this state, and all background processing
  // is also suspended.
  bool suspended() const { return m_suspended; }

  void setPageScaleFactor(float);
  float pageScaleFactor() const;

  // Corresponds to pixel density of the device where this Page is
  // being displayed. In multi-monitor setups this can vary between pages.
  // This value does not account for Page zoom, use LocalFrame::devicePixelRatio
  // instead.  This is to be deprecated. Use this with caution.
  // 1) If you need to scale the content per device scale factor, this is still
  //    valid.  In use-zoom-for-dsf mode, this is always 1, and will be remove
  //    when transition is complete.
  // 2) If you want to compute the device related measure (such as device pixel
  //    height, or the scale factor for drag image), use
  //    ChromeClient::screenInfo() instead.
  float deviceScaleFactorDeprecated() const { return m_deviceScaleFactor; }
  void setDeviceScaleFactorDeprecated(float);

  static void allVisitedStateChanged(bool invalidateVisitedLinkHashes);
  static void visitedStateChanged(LinkHash visitedHash);

  void setVisibilityState(PageVisibilityState, bool);
  PageVisibilityState visibilityState() const;
  bool isPageVisible() const;

  bool isCursorVisible() const;
  void setIsCursorVisible(bool isVisible) { m_isCursorVisible = isVisible; }

  void setDefaultPageScaleLimits(float minScale, float maxScale);
  void setUserAgentPageScaleConstraints(
      const PageScaleConstraints& newConstraints);

#if DCHECK_IS_ON()
  void setIsPainting(bool painting) { m_isPainting = painting; }
  bool isPainting() const { return m_isPainting; }
#endif

  void didCommitLoad(LocalFrame*);

  void acceptLanguagesChanged();

  DECLARE_TRACE();

  void layerTreeViewInitialized(WebLayerTreeView&, FrameView*);
  void willCloseLayerTreeView(WebLayerTreeView&, FrameView*);

  void willBeDestroyed();

 private:
  friend class ScopedPageSuspender;

  explicit Page(PageClients&);

  void initGroup();

  // SettingsDelegate overrides.
  void settingsChanged(SettingsDelegate::ChangeType) override;

  // ScopedPageSuspender helpers.
  void setSuspended(bool);

  Member<PageAnimator> m_animator;
  const Member<AutoscrollController> m_autoscrollController;
  Member<ChromeClient> m_chromeClient;
  const Member<DragCaret> m_dragCaret;
  const Member<DragController> m_dragController;
  const Member<FocusController> m_focusController;
  const Member<ContextMenuController> m_contextMenuController;
  const std::unique_ptr<PageScaleConstraintsSet> m_pageScaleConstraintsSet;
  const Member<PointerLockController> m_pointerLockController;
  Member<ScrollingCoordinator> m_scrollingCoordinator;

  // Typically, the main frame and Page should both be owned by the embedder,
  // which must call Page::willBeDestroyed() prior to destroying Page. This
  // call detaches the main frame and clears this pointer, thus ensuring that
  // this field only references a live main frame.
  //
  // However, there are several locations (InspectorOverlay, SVGImage, and
  // WebPagePopupImpl) which don't hold a reference to the main frame at all
  // after creating it. These are still safe because they always create a
  // Frame with a FrameView. FrameView and Frame hold references to each
  // other, thus keeping each other alive. The call to willBeDestroyed()
  // breaks this cycle, so the frame is still properly destroyed once no
  // longer needed.
  Member<Frame> m_mainFrame;

  mutable RefPtr<PluginData> m_pluginData;

  EditorClient* const m_editorClient;
  SpellCheckerClient* const m_spellCheckerClient;
  Member<ValidationMessageClient> m_validationMessageClient;

  UseCounter m_useCounter;
  Deprecation m_deprecation;
  HostsUsingFeatures m_hostsUsingFeatures;

  bool m_openedByDOM;
  // Set to true when window.close() has been called and the Page will be
  // destroyed. The browsing contexts in this page should no longer be
  // discoverable via JS.
  // TODO(dcheng): Try to remove |DOMWindow::m_windowIsClosing| in favor of
  // this. However, this depends on resolving https://crbug.com/674641
  bool m_isClosing;

  bool m_tabKeyCyclesThroughElements;
  bool m_suspended;

  float m_deviceScaleFactor;

  PageVisibilityState m_visibilityState;

  bool m_isCursorVisible;

#if DCHECK_IS_ON()
  bool m_isPainting = false;
#endif

  // A pointer to all the interfaces provided to in-process Frames for this
  // Page.
  // FIXME: Most of the members of Page should move onto FrameHost.
  Member<FrameHost> m_frameHost;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<Page>;

}  // namespace blink

#endif  // Page_h

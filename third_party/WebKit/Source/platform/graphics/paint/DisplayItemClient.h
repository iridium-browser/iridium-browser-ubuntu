// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayItemClient_h
#define DisplayItemClient_h

#include "platform/PlatformExport.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Assertions.h"
#include "wtf/text/WTFString.h"

#define CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS DCHECK_IS_ON()

namespace blink {

// The class for objects that can be associated with display items. A
// DisplayItemClient object should live at least longer than the document cycle
// in which its display items are created during painting. After the document
// cycle, a pointer/reference to DisplayItemClient should be no longer
// dereferenced unless we can make sure the client is still valid.
class PLATFORM_EXPORT DisplayItemClient {
 public:
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  DisplayItemClient();
  virtual ~DisplayItemClient();

  // Tests if this DisplayItemClient object has been created and has not been
  // deleted yet.
  bool isAlive() const;

  // Called when any DisplayItem of this DisplayItemClient is added into
  // PaintController using PaintController::createAndAppend() or into a cached
  // subsequence.
  void beginShouldKeepAlive(const void* owner) const;

  // Called when the DisplayItemClient is sure that it can safely die before its
  // owners have chance to remove it from the aliveness control.
  void endShouldKeepAlive() const;

  // Clears all should-keep-alive DisplayItemClients of a PaintController.
  // Called after PaintController commits new display items or the subsequence
  // owner is invalidated.
  static void endShouldKeepAliveAllClients(const void* owner);
  static void endShouldKeepAliveAllClients();
#else
  DisplayItemClient() {}
  virtual ~DisplayItemClient() {}
#endif

  virtual String debugName() const = 0;

  // The visual rect of this DisplayItemClient, in the object space of the
  // object that owns the GraphicsLayer, i.e. offset by
  // offsetFromLayoutObjectWithSubpixelAccumulation().
  virtual LayoutRect visualRect() const = 0;

  // This is declared here instead of in LayoutObject for verifying the
  // condition in DrawingRecorder.
  // Returns true if the object itself will not generate any effective painted
  // output no matter what size the object is. For example, this function can
  // return false for an object whose size is currently 0x0 but would have
  // effective painted output if it was set a non-empty size. It's used to skip
  // unforced paint invalidation of LayoutObjects (which is when
  // shouldDoFullPaintInvalidation is false, but mayNeedPaintInvalidation or
  // childShouldCheckForPaintInvalidation is true) to avoid unnecessary paint
  // invalidations of empty areas covered by such objects.
  virtual bool paintedOutputOfObjectHasNoEffectRegardlessOfSize() const {
    return false;
  }

  void setDisplayItemsUncached(
      PaintInvalidationReason reason = PaintInvalidationFull) const {
    m_cacheGenerationOrInvalidationReason.invalidate(reason);
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
    // Clear should-keep-alive of DisplayItemClients in a subsequence if this
    // object is a subsequence.
    endShouldKeepAliveAllClients(this);
#endif
  }

  PaintInvalidationReason getPaintInvalidationReason() const {
    return m_cacheGenerationOrInvalidationReason.getPaintInvalidationReason();
  }

  // A client is considered "just created" if its display items have never been
  // committed.
  bool isJustCreated() const {
    return m_cacheGenerationOrInvalidationReason.isJustCreated();
  }
  void clearIsJustCreated() const {
    m_cacheGenerationOrInvalidationReason.clearIsJustCreated();
  }

 private:
  friend class FakeDisplayItemClient;
  friend class PaintController;

  // Holds a unique cache generation id of DisplayItemClients and
  // PaintControllers, or PaintInvalidationReason if the DisplayItemClient or
  // PaintController is invalidated.
  //
  // A paint controller sets its cache generation to
  // DisplayItemCacheGeneration::next() at the end of each
  // commitNewDisplayItems, and updates the cache generation of each client with
  // cached drawings by calling DisplayItemClient::setDisplayItemsCached(). A
  // display item is treated as validly cached in a paint controller if its
  // cache generation matches the paint controller's cache generation.
  //
  // SPv1 only: If a display item is painted on multiple paint controllers,
  // because cache generations are unique, the client's cache generation matches
  // the last paint controller only. The client will be treated as invalid on
  // other paint controllers regardless if it's validly cached by these paint
  // controllers. This situation is very rare (about 0.07% of clients were
  // painted on multiple paint controllers) so the performance penalty is
  // trivial.
  class PLATFORM_EXPORT CacheGenerationOrInvalidationReason {
    DISALLOW_NEW();

   public:
    CacheGenerationOrInvalidationReason() : m_value(kJustCreated) {}

    void invalidate(PaintInvalidationReason reason = PaintInvalidationFull) {
      if (m_value != kJustCreated)
        m_value = static_cast<ValueType>(reason);
    }

    static CacheGenerationOrInvalidationReason next() {
      // In case the value overflowed in the previous call.
      if (s_nextGeneration < kFirstValidGeneration)
        s_nextGeneration = kFirstValidGeneration;
      return CacheGenerationOrInvalidationReason(s_nextGeneration++);
    }

    bool matches(const CacheGenerationOrInvalidationReason& other) const {
      return m_value >= kFirstValidGeneration &&
             other.m_value >= kFirstValidGeneration && m_value == other.m_value;
    }

    PaintInvalidationReason getPaintInvalidationReason() const {
      return m_value < kJustCreated
                 ? static_cast<PaintInvalidationReason>(m_value)
                 : PaintInvalidationNone;
    }

    bool isJustCreated() const { return m_value == kJustCreated; }
    void clearIsJustCreated() {
      m_value = static_cast<ValueType>(PaintInvalidationFull);
    }

   private:
    typedef uint32_t ValueType;
    explicit CacheGenerationOrInvalidationReason(ValueType value)
        : m_value(value) {}

    static const ValueType kJustCreated =
        static_cast<ValueType>(PaintInvalidationReasonMax) + 1;
    static const ValueType kFirstValidGeneration =
        static_cast<ValueType>(PaintInvalidationReasonMax) + 2;
    static ValueType s_nextGeneration;
    ValueType m_value;
  };

  bool displayItemsAreCached(CacheGenerationOrInvalidationReason other) const {
    return m_cacheGenerationOrInvalidationReason.matches(other);
  }
  void setDisplayItemsCached(
      CacheGenerationOrInvalidationReason cacheGeneration) const {
    m_cacheGenerationOrInvalidationReason = cacheGeneration;
  }

  mutable CacheGenerationOrInvalidationReason
      m_cacheGenerationOrInvalidationReason;

  DISALLOW_COPY_AND_ASSIGN(DisplayItemClient);
};

inline bool operator==(const DisplayItemClient& client1,
                       const DisplayItemClient& client2) {
  return &client1 == &client2;
}
inline bool operator!=(const DisplayItemClient& client1,
                       const DisplayItemClient& client2) {
  return &client1 != &client2;
}

}  // namespace blink

#endif  // DisplayItemClient_h

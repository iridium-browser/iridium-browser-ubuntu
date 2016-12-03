// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_BUILDER_H_
#define COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_BUILDER_H_

#include <memory>
#include <vector>

#include "components/sessions/core/sessions_export.h"

namespace content {
class BrowserContext;
class NavigationEntry;
}

namespace sessions {
class SerializedNavigationEntry;

// Provides methods to convert between SerializedNavigationEntry and content
// classes.
class SESSIONS_EXPORT ContentSerializedNavigationBuilder {
 public:
  // Construct a SerializedNavigationEntry for a particular index from the given
  // NavigationEntry.
  static SerializedNavigationEntry FromNavigationEntry(
      int index,
      const content::NavigationEntry& entry);

  // Convert the given SerializedNavigationEntry into a NavigationEntry with the
  // given page ID and context.  The NavigationEntry will have a transition type
  // of PAGE_TRANSITION_RELOAD and a new unique ID.
  static std::unique_ptr<content::NavigationEntry> ToNavigationEntry(
      const SerializedNavigationEntry* navigation,
      int page_id,
      content::BrowserContext* browser_context);

  // Converts a set of SerializedNavigationEntrys into a list of
  // NavigationEntrys with sequential page IDs and the given context.
  static std::vector<std::unique_ptr<content::NavigationEntry>>
  ToNavigationEntries(const std::vector<SerializedNavigationEntry>& navigations,
                      content::BrowserContext* browser_context);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_CONTENT_SERIALIZED_NAVIGATION_BUILDER_H_

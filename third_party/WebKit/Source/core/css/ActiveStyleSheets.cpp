// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/ActiveStyleSheets.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/RuleSet.h"
#include "core/css/resolver/ScopedStyleResolver.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"

namespace blink {

ActiveSheetsChange compareActiveStyleSheets(
    const ActiveStyleSheetVector& oldStyleSheets,
    const ActiveStyleSheetVector& newStyleSheets,
    HeapHashSet<Member<RuleSet>>& changedRuleSets) {
  unsigned newStyleSheetCount = newStyleSheets.size();
  unsigned oldStyleSheetCount = oldStyleSheets.size();

  unsigned minCount = std::min(newStyleSheetCount, oldStyleSheetCount);
  unsigned index = 0;

  // Walk the common prefix of stylesheets. If the stylesheet rules were
  // modified since last time, add them to the list of changed rulesets.
  for (; index < minCount &&
         newStyleSheets[index].first == oldStyleSheets[index].first;
       index++) {
    if (newStyleSheets[index].second == oldStyleSheets[index].second)
      continue;

    if (newStyleSheets[index].second)
      changedRuleSets.insert(newStyleSheets[index].second);
    if (oldStyleSheets[index].second)
      changedRuleSets.insert(oldStyleSheets[index].second);
  }

  if (index == oldStyleSheetCount) {
    // The old stylesheet vector is a prefix of the new vector in terms of
    // StyleSheets. If none of the RuleSets changed, we only need to add the new
    // sheets to the ScopedStyleResolver (ActiveSheetsAppended).
    bool ruleSetsChangedInCommonPrefix = !changedRuleSets.isEmpty();
    for (; index < newStyleSheetCount; index++) {
      if (newStyleSheets[index].second)
        changedRuleSets.insert(newStyleSheets[index].second);
    }
    if (ruleSetsChangedInCommonPrefix)
      return ActiveSheetsChanged;
    if (changedRuleSets.isEmpty())
      return NoActiveSheetsChanged;
    return ActiveSheetsAppended;
  }

  if (index == newStyleSheetCount) {
    // Sheets removed from the end.
    for (; index < oldStyleSheetCount; index++) {
      if (oldStyleSheets[index].second)
        changedRuleSets.insert(oldStyleSheets[index].second);
    }
    return changedRuleSets.isEmpty() ? NoActiveSheetsChanged
                                     : ActiveSheetsChanged;
  }

  DCHECK(index < oldStyleSheetCount && index < newStyleSheetCount);

  // Both the new and old active stylesheet vectors have stylesheets following
  // the common prefix. Figure out which were added or removed by sorting the
  // merged vector of old and new sheets.

  ActiveStyleSheetVector mergedSorted;
  mergedSorted.reserveCapacity(oldStyleSheetCount + newStyleSheetCount -
                               2 * index);
  mergedSorted.appendRange(oldStyleSheets.begin() + index,
                           oldStyleSheets.end());
  mergedSorted.appendRange(newStyleSheets.begin() + index,
                           newStyleSheets.end());

  std::sort(mergedSorted.begin(), mergedSorted.end());

  auto mergedIterator = mergedSorted.begin();
  while (mergedIterator != mergedSorted.end()) {
    const auto& sheet1 = *mergedIterator++;
    if (mergedIterator == mergedSorted.end() ||
        (*mergedIterator).first != sheet1.first) {
      // Sheet either removed or inserted.
      if (sheet1.second)
        changedRuleSets.insert(sheet1.second);
      continue;
    }

    // Sheet present in both old and new.
    const auto& sheet2 = *mergedIterator++;

    if (sheet1.second == sheet2.second)
      continue;

    // Active rules for the given stylesheet changed.
    // DOM, CSSOM, or media query changes.
    if (sheet1.second)
      changedRuleSets.insert(sheet1.second);
    if (sheet2.second)
      changedRuleSets.insert(sheet2.second);
  }
  return changedRuleSets.isEmpty() ? NoActiveSheetsChanged
                                   : ActiveSheetsChanged;
}

}  // namespace blink

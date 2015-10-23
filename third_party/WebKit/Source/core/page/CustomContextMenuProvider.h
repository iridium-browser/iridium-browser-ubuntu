// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomContextMenuProvider_h
#define CustomContextMenuProvider_h

#include "core/page/ContextMenuProvider.h"
#include "platform/ContextMenuItem.h"
#include "platform/heap/Handle.h"

namespace blink {

class ContextMenu;
class HTMLElement;
class HTMLMenuElement;
class HTMLMenuItemElement;

class CustomContextMenuProvider final : public ContextMenuProvider {
public:
    ~CustomContextMenuProvider() override;

    static PassRefPtrWillBeRawPtr<CustomContextMenuProvider> create(HTMLMenuElement& menu, HTMLElement& subject)
    {
        return adoptRefWillBeNoop(new CustomContextMenuProvider(menu, subject));
    }

    DECLARE_VIRTUAL_TRACE();

private:
    CustomContextMenuProvider(HTMLMenuElement&, HTMLElement&);

    void populateContextMenu(ContextMenu*) override;
    void contextMenuItemSelected(const ContextMenuItem*) override;
    void contextMenuCleared() override;
    void populateContextMenuItems(const HTMLMenuElement&, ContextMenu&);
    void appendSeparator(ContextMenu&);
    void appendMenuItem(HTMLMenuItemElement*, ContextMenu&);
    HTMLElement* menuItemAt(unsigned menuId);

    RefPtrWillBeMember<HTMLMenuElement> m_menu;
    RefPtrWillBeMember<HTMLElement> m_subjectElement;
    WillBeHeapVector<RefPtrWillBeMember<HTMLElement>> m_menuItems;
};

} // namespace blink

#endif

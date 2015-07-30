/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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


#ifndef AXMediaControls_h
#define AXMediaControls_h

#include "core/html/shadow/MediaControlElements.h"
#include "modules/accessibility/AXSlider.h"

namespace blink {

class AXObjectCacheImpl;

class AccessibilityMediaControl : public AXLayoutObject {

public:
    static PassRefPtr<AXObject> create(LayoutObject*, AXObjectCacheImpl*);
    virtual ~AccessibilityMediaControl() { }

    virtual AccessibilityRole roleValue() const override;

    virtual String deprecatedTitle(TextUnderElementMode) const override final;
    virtual String deprecatedAccessibilityDescription() const override;
    virtual String deprecatedHelpText() const override;

protected:
    AccessibilityMediaControl(LayoutObject*, AXObjectCacheImpl*);
    MediaControlElementType controlType() const;
    virtual bool computeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
};


class AccessibilityMediaTimeline final : public AXSlider {

public:
    static PassRefPtr<AXObject> create(LayoutObject*, AXObjectCacheImpl*);
    virtual ~AccessibilityMediaTimeline() { }

    virtual String deprecatedHelpText() const override;
    virtual String valueDescription() const override;
    const AtomicString& getAttribute(const QualifiedName& attribute) const;

private:
    AccessibilityMediaTimeline(LayoutObject*, AXObjectCacheImpl*);
};


class AXMediaControlsContainer final : public AccessibilityMediaControl {

public:
    static PassRefPtr<AXObject> create(LayoutObject*, AXObjectCacheImpl*);
    virtual ~AXMediaControlsContainer() { }

    virtual AccessibilityRole roleValue() const override { return ToolbarRole; }

    virtual String deprecatedHelpText() const override;
    virtual String deprecatedAccessibilityDescription() const override;

private:
    AXMediaControlsContainer(LayoutObject*, AXObjectCacheImpl*);
    bool controllingVideoElement() const;
    virtual bool computeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
};


class AccessibilityMediaTimeDisplay final : public AccessibilityMediaControl {

public:
    static PassRefPtr<AXObject> create(LayoutObject*, AXObjectCacheImpl*);
    virtual ~AccessibilityMediaTimeDisplay() { }

    virtual AccessibilityRole roleValue() const override { return StaticTextRole; }

    virtual String stringValue() const override;
    virtual String deprecatedAccessibilityDescription() const override;

private:
    AccessibilityMediaTimeDisplay(LayoutObject*, AXObjectCacheImpl*);
    virtual bool computeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;
};


} // namespace blink

#endif // AXMediaControls_h

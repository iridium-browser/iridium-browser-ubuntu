// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StringKeyframe_h
#define StringKeyframe_h

#include "core/animation/Keyframe.h"
#include "core/css/StylePropertySet.h"

namespace blink {

class StyleSheetContents;

class StringKeyframe : public Keyframe {
public:
    static PassRefPtrWillBeRawPtr<StringKeyframe> create()
    {
        return adoptRefWillBeNoop(new StringKeyframe);
    }
    void setPropertyValue(CSSPropertyID, const String& value, StyleSheetContents*);
    void setPropertyValue(CSSPropertyID, PassRefPtrWillBeRawPtr<CSSValue>);
    void clearPropertyValue(CSSPropertyID property) { m_propertySet->removeProperty(property); }
    CSSValue* propertyValue(CSSPropertyID property) const
    {
        int index = m_propertySet->findPropertyIndex(property);
        RELEASE_ASSERT(index >= 0);
        return m_propertySet->propertyAt(static_cast<unsigned>(index)).value();
    }
    virtual PropertySet properties() const override;
    RefPtrWillBeMember<MutableStylePropertySet> propertySetForInspector() const { return m_propertySet; }

    DECLARE_VIRTUAL_TRACE();

    class PropertySpecificKeyframe : public Keyframe::PropertySpecificKeyframe {
    public:
        PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue*, AnimationEffect::CompositeOperation);

        CSSValue* value() const { return m_value.get(); }
        void setEasing(PassRefPtrWillBeRawPtr<TimingFunction> easing) { m_easing = easing; }

        virtual void populateAnimatableValue(CSSPropertyID, Element&, const ComputedStyle* baseStyle) const;
        virtual const PassRefPtrWillBeRawPtr<AnimatableValue> getAnimatableValue() const override final { return m_animatableValueCache.get(); }
        void setAnimatableValue(PassRefPtrWillBeRawPtr<AnimatableValue> value) { m_animatableValueCache = value; }

        virtual PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> neutralKeyframe(double offset, PassRefPtr<TimingFunction> easing) const override final;
        virtual PassRefPtrWillBeRawPtr<Interpolation> maybeCreateInterpolation(CSSPropertyID, blink::Keyframe::PropertySpecificKeyframe& end, Element*, const ComputedStyle* baseStyle) const override final;

        DECLARE_VIRTUAL_TRACE();

    private:
        PropertySpecificKeyframe(double offset, PassRefPtr<TimingFunction> easing, CSSValue*);

        virtual PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> cloneWithOffset(double offset) const;
        virtual bool isStringPropertySpecificKeyframe() const override { return true; }

        static bool createInterpolationsFromCSSValues(CSSPropertyID, CSSValue* fromCSSValue, CSSValue* toCSSValue, Element*, OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<Interpolation>>>& interpolations);

        void populateAnimatableValueCaches(CSSPropertyID, Keyframe::PropertySpecificKeyframe&, Element*, CSSValue& fromCSSValue, CSSValue& toCSSValue) const;

        RefPtrWillBeMember<CSSValue> m_value;
        mutable RefPtrWillBeMember<AnimatableValue> m_animatableValueCache;
    };

private:
    StringKeyframe()
        : m_propertySet(MutableStylePropertySet::create())
    { }

    StringKeyframe(const StringKeyframe& copyFrom);

    virtual PassRefPtrWillBeRawPtr<Keyframe> clone() const override;
    virtual PassOwnPtrWillBeRawPtr<Keyframe::PropertySpecificKeyframe> createPropertySpecificKeyframe(CSSPropertyID) const override;

    virtual bool isStringKeyframe() const override { return true; }

    RefPtrWillBeMember<MutableStylePropertySet> m_propertySet;
};

using StringPropertySpecificKeyframe = StringKeyframe::PropertySpecificKeyframe;

DEFINE_TYPE_CASTS(StringKeyframe, Keyframe, value, value->isStringKeyframe(), value.isStringKeyframe());
DEFINE_TYPE_CASTS(StringPropertySpecificKeyframe, Keyframe::PropertySpecificKeyframe, value, value->isStringPropertySpecificKeyframe(), value.isStringPropertySpecificKeyframe());

}

#endif

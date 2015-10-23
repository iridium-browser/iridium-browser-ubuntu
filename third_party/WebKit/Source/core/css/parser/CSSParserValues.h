/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef CSSParserValues_h
#define CSSParserValues_h

#include "core/CSSValueKeywords.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSValueList.h"
#include "core/css/parser/CSSParserString.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "wtf/Allocator.h"

namespace blink {

class QualifiedName;

struct CSSParserFunction;
struct CSSParserCalcFunction;
class CSSParserValueList;

struct CSSParserValue {
    ALLOW_ONLY_INLINE_ALLOCATION();
    CSSValueID id;
    bool isInt;
    union {
        double fValue;
        int iValue;
        CSSParserString string;
        CSSParserFunction* function;
        CSSParserCalcFunction* calcFunction;
        CSSParserValueList* valueList;
        struct {
            UChar32 start;
            UChar32 end;
        } m_unicodeRange;
    };
    enum {
        Operator  = 0x100000,
        Function  = 0x100001,
        CalcFunction  = 0x100002,
        ValueList = 0x100003,
        HexColor = 0x100004,
        Identifier = 0x100005,
        // Represents a dimension by a list of two values, a UnitType::Number and an Identifier
        DimensionList = 0x100006,
        // Represents a unicode range by a pair of UChar32 values
        UnicodeRange = 0x100007,
    };
    int m_unit;
    CSSPrimitiveValue::UnitType unit() const { return static_cast<CSSPrimitiveValue::UnitType>(m_unit); }
    void setUnit(CSSPrimitiveValue::UnitType unit) { m_unit = static_cast<int>(unit); }

    inline void setFromNumber(double value, CSSPrimitiveValue::UnitType);
    inline void setFromOperator(UChar);
    inline void setFromValueList(PassOwnPtr<CSSParserValueList>);
};

class CORE_EXPORT CSSParserValueList {
    WTF_MAKE_FAST_ALLOCATED(CSSParserValueList);
public:
    CSSParserValueList()
        : m_current(0)
    {
    }
    CSSParserValueList(CSSParserTokenRange);
    ~CSSParserValueList();

    void addValue(const CSSParserValue&);

    unsigned size() const { return m_values.size(); }
    unsigned currentIndex() { return m_current; }
    CSSParserValue* current() { return m_current < m_values.size() ? &m_values[m_current] : 0; }
    CSSParserValue* next() { ++m_current; return current(); }
    CSSParserValue* previous()
    {
        if (!m_current)
            return 0;
        --m_current;
        return current();
    }
    void setCurrentIndex(unsigned index)
    {
        ASSERT(index < m_values.size());
        if (index < m_values.size())
            m_current = index;
    }

    CSSParserValue* valueAt(unsigned i) { return i < m_values.size() ? &m_values[i] : 0; }

    void clearAndLeakValues() { m_values.clear(); m_current = 0;}
    void destroyAndClear();

private:
    unsigned m_current;
    Vector<CSSParserValue, 4> m_values;
};

struct CSSParserFunction {
    WTF_MAKE_FAST_ALLOCATED(CSSParserFunction);
public:
    CSSValueID id;
    OwnPtr<CSSParserValueList> args;
};

struct CSSParserCalcFunction {
    WTF_MAKE_FAST_ALLOCATED(CSSParserCalcFunction);
public:
    CSSParserCalcFunction(CSSParserTokenRange args_) : args(args_) {}
    CSSParserTokenRange args;
};

class CSSParserSelector {
    WTF_MAKE_NONCOPYABLE(CSSParserSelector); WTF_MAKE_FAST_ALLOCATED(CSSParserSelector);
public:
    CSSParserSelector();
    explicit CSSParserSelector(const QualifiedName&, bool isImplicit = false);

    static PassOwnPtr<CSSParserSelector> create() { return adoptPtr(new CSSParserSelector); }
    static PassOwnPtr<CSSParserSelector> create(const QualifiedName& name, bool isImplicit = false) { return adoptPtr(new CSSParserSelector(name, isImplicit)); }

    ~CSSParserSelector();

    PassOwnPtr<CSSSelector> releaseSelector() { return m_selector.release(); }

    CSSSelector::Relation relation() const { return m_selector->relation(); }
    void setValue(const AtomicString& value) { m_selector->setValue(value); }
    void setAttribute(const QualifiedName& value, CSSSelector::AttributeMatchType matchType) { m_selector->setAttribute(value, matchType); }
    void setArgument(const AtomicString& value) { m_selector->setArgument(value); }
    void setNth(int a, int b) { m_selector->setNth(a, b); }
    void setMatch(CSSSelector::Match value) { m_selector->setMatch(value); }
    void setRelation(CSSSelector::Relation value) { m_selector->setRelation(value); }
    void setForPage() { m_selector->setForPage(); }
    void setRelationIsAffectedByPseudoContent() { m_selector->setRelationIsAffectedByPseudoContent(); }
    bool relationIsAffectedByPseudoContent() const { return m_selector->relationIsAffectedByPseudoContent(); }

    void updatePseudoType(const AtomicString& value, bool hasArguments = false) const { m_selector->updatePseudoType(value, hasArguments); }

    void adoptSelectorVector(Vector<OwnPtr<CSSParserSelector>>& selectorVector);
    void setSelectorList(PassOwnPtr<CSSSelectorList>);

    bool hasHostPseudoSelector() const;

    CSSSelector::PseudoType pseudoType() const { return m_selector->pseudoType(); }

    // TODO(esprehn): This set of cases doesn't make sense, why PseudoShadow but not a check for ::content or /deep/ ?
    bool crossesTreeScopes() const { return pseudoType() == CSSSelector::PseudoWebKitCustomElement || pseudoType() == CSSSelector::PseudoCue || pseudoType() == CSSSelector::PseudoShadow; }

    bool isSimple() const;
    bool hasShadowPseudo() const;

    CSSParserSelector* tagHistory() const { return m_tagHistory.get(); }
    void setTagHistory(PassOwnPtr<CSSParserSelector> selector) { m_tagHistory = selector; }
    void clearTagHistory() { m_tagHistory.clear(); }
    void insertTagHistory(CSSSelector::Relation before, PassOwnPtr<CSSParserSelector>, CSSSelector::Relation after);
    void appendTagHistory(CSSSelector::Relation, PassOwnPtr<CSSParserSelector>);
    void prependTagSelector(const QualifiedName&, bool tagIsImplicit = false);

private:
    OwnPtr<CSSSelector> m_selector;
    OwnPtr<CSSParserSelector> m_tagHistory;
};

inline bool CSSParserSelector::hasShadowPseudo() const
{
    return m_selector->relation() == CSSSelector::ShadowPseudo;
}

inline void CSSParserValue::setFromNumber(double value, CSSPrimitiveValue::UnitType unit)
{
    id = CSSValueInvalid;
    isInt = false;
    fValue = value;
    this->setUnit(std::isfinite(value) ? unit : CSSPrimitiveValue::UnitType::Unknown);
}

inline void CSSParserValue::setFromOperator(UChar c)
{
    id = CSSValueInvalid;
    m_unit = Operator;
    iValue = c;
    isInt = false;
}

inline void CSSParserValue::setFromValueList(PassOwnPtr<CSSParserValueList> valueList)
{
    id = CSSValueInvalid;
    this->valueList = valueList.leakPtr();
    m_unit = ValueList;
    isInt = false;
}

}

#endif

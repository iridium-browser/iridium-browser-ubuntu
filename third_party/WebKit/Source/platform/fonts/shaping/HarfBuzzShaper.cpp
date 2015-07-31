/*
 * Copyright (c) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 BlackBerry Limited. All rights reserved.
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

#include "config.h"
#include "platform/fonts/shaping/HarfBuzzShaper.h"

#include "hb.h"
#include "platform/LayoutUnit.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/fonts/Character.h"
#include "platform/fonts/Font.h"
#include "platform/fonts/GlyphBuffer.h"
#include "platform/fonts/UTF16TextIterator.h"
#include "platform/fonts/shaping/HarfBuzzFace.h"
#include "platform/text/TextBreakIterator.h"
#include "wtf/Compiler.h"
#include "wtf/MathExtras.h"
#include "wtf/unicode/Unicode.h"

#include <list>
#include <map>
#include <string>
#include <unicode/normlzr.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>

namespace blink {

template<typename T>
class HarfBuzzScopedPtr {
public:
    typedef void (*DestroyFunction)(T*);

    HarfBuzzScopedPtr(T* ptr, DestroyFunction destroy)
        : m_ptr(ptr)
        , m_destroy(destroy)
    {
        ASSERT(m_destroy);
    }
    ~HarfBuzzScopedPtr()
    {
        if (m_ptr)
            (*m_destroy)(m_ptr);
    }

    T* get() { return m_ptr; }
    void set(T* ptr) { m_ptr = ptr; }
private:
    T* m_ptr;
    DestroyFunction m_destroy;
};


static const unsigned cHarfBuzzCacheMaxSize = 256;

struct CachedShapingResultsLRUNode;
struct CachedShapingResults;
typedef std::map<std::wstring, CachedShapingResults*> CachedShapingResultsMap;
typedef std::list<CachedShapingResultsLRUNode*> CachedShapingResultsLRU;

struct CachedShapingResults {
    CachedShapingResults(hb_buffer_t* harfBuzzBuffer, const Font* runFont, hb_direction_t runDir, const String& newLocale);
    ~CachedShapingResults();

    hb_buffer_t* buffer;
    Font font;
    hb_direction_t dir;
    String locale;
    CachedShapingResultsLRU::iterator lru;
};

struct CachedShapingResultsLRUNode {
    CachedShapingResultsLRUNode(const CachedShapingResultsMap::iterator& cacheEntry);
    ~CachedShapingResultsLRUNode();

    CachedShapingResultsMap::iterator entry;
};

CachedShapingResults::CachedShapingResults(hb_buffer_t* harfBuzzBuffer, const Font* fontData, hb_direction_t dirData, const String& newLocale)
    : buffer(harfBuzzBuffer)
    , font(*fontData)
    , dir(dirData)
    , locale(newLocale)
{
}

CachedShapingResults::~CachedShapingResults()
{
    hb_buffer_destroy(buffer);
}

CachedShapingResultsLRUNode::CachedShapingResultsLRUNode(const CachedShapingResultsMap::iterator& cacheEntry)
    : entry(cacheEntry)
{
}

CachedShapingResultsLRUNode::~CachedShapingResultsLRUNode()
{
}

class HarfBuzzRunCache {
public:
    HarfBuzzRunCache();
    ~HarfBuzzRunCache();

    CachedShapingResults* find(const std::wstring& key) const;
    void remove(CachedShapingResults* node);
    void moveToBack(CachedShapingResults* node);
    bool insert(const std::wstring& key, CachedShapingResults* run);

private:
    CachedShapingResultsMap m_harfBuzzRunMap;
    CachedShapingResultsLRU m_harfBuzzRunLRU;
};


HarfBuzzRunCache::HarfBuzzRunCache()
{
}

HarfBuzzRunCache::~HarfBuzzRunCache()
{
    for (CachedShapingResultsMap::iterator it = m_harfBuzzRunMap.begin(); it != m_harfBuzzRunMap.end(); ++it)
        delete it->second;
    for (CachedShapingResultsLRU::iterator it = m_harfBuzzRunLRU.begin(); it != m_harfBuzzRunLRU.end(); ++it)
        delete *it;
}

bool HarfBuzzRunCache::insert(const std::wstring& key, CachedShapingResults* data)
{
    std::pair<CachedShapingResultsMap::iterator, bool> results =
        m_harfBuzzRunMap.insert(CachedShapingResultsMap::value_type(key, data));

    if (!results.second)
        return false;

    CachedShapingResultsLRUNode* node = new CachedShapingResultsLRUNode(results.first);

    m_harfBuzzRunLRU.push_back(node);
    data->lru = --m_harfBuzzRunLRU.end();

    if (m_harfBuzzRunMap.size() > cHarfBuzzCacheMaxSize) {
        CachedShapingResultsLRUNode* lru = m_harfBuzzRunLRU.front();
        CachedShapingResults* foo = lru->entry->second;
        m_harfBuzzRunMap.erase(lru->entry);
        m_harfBuzzRunLRU.pop_front();
        delete foo;
        delete lru;
    }

    return true;
}

inline CachedShapingResults* HarfBuzzRunCache::find(const std::wstring& key) const
{
    CachedShapingResultsMap::const_iterator it = m_harfBuzzRunMap.find(key);

    return it != m_harfBuzzRunMap.end() ? it->second : 0;
}

inline void HarfBuzzRunCache::remove(CachedShapingResults* node)
{
    CachedShapingResultsLRUNode* lruNode = *node->lru;

    m_harfBuzzRunLRU.erase(node->lru);
    m_harfBuzzRunMap.erase(lruNode->entry);
    delete lruNode;
    delete node;
}

inline void HarfBuzzRunCache::moveToBack(CachedShapingResults* node)
{
    CachedShapingResultsLRUNode* lruNode = *node->lru;
    m_harfBuzzRunLRU.erase(node->lru);
    m_harfBuzzRunLRU.push_back(lruNode);
    node->lru = --m_harfBuzzRunLRU.end();
}

HarfBuzzRunCache& harfBuzzRunCache()
{
    DEFINE_STATIC_LOCAL(HarfBuzzRunCache, globalHarfBuzzRunCache, ());
    return globalHarfBuzzRunCache;
}

static inline float harfBuzzPositionToFloat(hb_position_t value)
{
    return static_cast<float>(value) / (1 << 16);
}

static inline unsigned countGraphemesInCluster(const UChar* normalizedBuffer, unsigned normalizedBufferLength, uint16_t startIndex, uint16_t endIndex)
{
    if (startIndex > endIndex) {
        uint16_t tempIndex = startIndex;
        startIndex = endIndex;
        endIndex = tempIndex;
    }
    uint16_t length = endIndex - startIndex;
    ASSERT(static_cast<unsigned>(startIndex + length) <= normalizedBufferLength);
    TextBreakIterator* cursorPosIterator = cursorMovementIterator(&normalizedBuffer[startIndex], length);

    int cursorPos = cursorPosIterator->current();
    int numGraphemes = -1;
    while (0 <= cursorPos) {
        cursorPos = cursorPosIterator->next();
        numGraphemes++;
    }
    return numGraphemes < 0 ? 0 : numGraphemes;
}

inline HarfBuzzShaper::HarfBuzzRun::HarfBuzzRun(const SimpleFontData* fontData, unsigned startIndex, unsigned numCharacters, hb_direction_t direction, hb_script_t script)
    : m_fontData(fontData)
    , m_startIndex(startIndex)
    , m_numCharacters(numCharacters)
    , m_numGlyphs(0)
    , m_direction(direction)
    , m_script(script)
    , m_width(0)
{
}

inline HarfBuzzShaper::HarfBuzzRun::HarfBuzzRun(const HarfBuzzRun& rhs)
    : m_fontData(rhs.m_fontData)
    , m_startIndex(rhs.m_startIndex)
    , m_numCharacters(rhs.m_numCharacters)
    , m_numGlyphs(rhs.m_numGlyphs)
    , m_direction(rhs.m_direction)
    , m_script(rhs.m_script)
    , m_glyphs(rhs.m_glyphs)
    , m_advances(rhs.m_advances)
    , m_glyphToCharacterIndexes(rhs.m_glyphToCharacterIndexes)
    , m_offsets(rhs.m_offsets)
    , m_width(rhs.m_width)
{
}

HarfBuzzShaper::HarfBuzzRun::~HarfBuzzRun()
{
}

inline void HarfBuzzShaper::HarfBuzzRun::applyShapeResult(hb_buffer_t* harfBuzzBuffer)
{
    m_numGlyphs = hb_buffer_get_length(harfBuzzBuffer);
    m_glyphs.resize(m_numGlyphs);
    m_advances.resize(m_numGlyphs);
    m_glyphToCharacterIndexes.resize(m_numGlyphs);
    m_offsets.resize(m_numGlyphs);
}

inline void HarfBuzzShaper::HarfBuzzRun::setGlyphAndPositions(unsigned index, uint16_t glyphId, float advance, float offsetX, float offsetY)
{
    m_glyphs[index] = glyphId;
    m_advances[index] = advance;
    m_offsets[index] = FloatSize(offsetX, offsetY);
}

void HarfBuzzShaper::HarfBuzzRun::addAdvance(unsigned index, float advance)
{
    ASSERT(index < m_numGlyphs);
    m_advances[index] += advance;
}

int HarfBuzzShaper::HarfBuzzRun::characterIndexForXPosition(float targetX)
{
    ASSERT(targetX <= m_width);
    float currentX = 0;
    float currentAdvance = m_advances[0];
    unsigned glyphIndex = 0;

    // Sum up advances that belong to a character.
    while (glyphIndex < m_numGlyphs - 1 && m_glyphToCharacterIndexes[glyphIndex] == m_glyphToCharacterIndexes[glyphIndex + 1])
        currentAdvance += m_advances[++glyphIndex];
    currentAdvance = currentAdvance / 2.0;
    if (targetX <= currentAdvance)
        return rtl() ? m_numCharacters : 0;

    currentX = currentAdvance;
    ++glyphIndex;
    while (glyphIndex < m_numGlyphs) {
        unsigned prevCharacterIndex = m_glyphToCharacterIndexes[glyphIndex - 1];
        float prevAdvance = currentAdvance;
        currentAdvance = m_advances[glyphIndex];
        while (glyphIndex < m_numGlyphs - 1 && m_glyphToCharacterIndexes[glyphIndex] == m_glyphToCharacterIndexes[glyphIndex + 1])
            currentAdvance += m_advances[++glyphIndex];
        currentAdvance = currentAdvance / 2.0;
        float nextX = currentX + prevAdvance + currentAdvance;
        if (currentX <= targetX && targetX <= nextX)
            return rtl() ? prevCharacterIndex : m_glyphToCharacterIndexes[glyphIndex];
        currentX = nextX;
        ++glyphIndex;
    }

    return rtl() ? 0 : m_numCharacters;
}

float HarfBuzzShaper::HarfBuzzRun::xPositionForOffset(unsigned offset)
{
    ASSERT(offset < m_numCharacters);
    unsigned glyphIndex = 0;
    float position = 0;
    if (rtl()) {
        while (glyphIndex < m_numGlyphs && m_glyphToCharacterIndexes[glyphIndex] > offset) {
            position += m_advances[glyphIndex];
            ++glyphIndex;
        }
        // For RTL, we need to return the right side boundary of the character.
        // Add advance of glyphs which are part of the character.
        while (glyphIndex < m_numGlyphs - 1 && m_glyphToCharacterIndexes[glyphIndex] == m_glyphToCharacterIndexes[glyphIndex + 1]) {
            position += m_advances[glyphIndex];
            ++glyphIndex;
        }
        position += m_advances[glyphIndex];
    } else {
        while (glyphIndex < m_numGlyphs && m_glyphToCharacterIndexes[glyphIndex] < offset) {
            position += m_advances[glyphIndex];
            ++glyphIndex;
        }
    }
    return position;
}

static void normalizeCharacters(const TextRun& run, unsigned length, UChar* destination, unsigned* destinationLength)
{
    unsigned position = 0;
    bool error = false;
    const UChar* source;
    String stringFor8BitRun;
    if (run.is8Bit()) {
        stringFor8BitRun = String::make16BitFrom8BitSource(run.characters8(), run.length());
        source = stringFor8BitRun.characters16();
    } else {
        source = run.characters16();
    }

    *destinationLength = 0;
    while (position < length) {
        UChar32 character;
        U16_NEXT(source, position, length, character);
        // Don't normalize tabs as they are not treated as spaces for word-end.
        if (run.normalizeSpace() && Character::isNormalizedCanvasSpaceCharacter(character))
            character = spaceCharacter;
        else if (Character::treatAsSpace(character) && character != tabulationCharacter)
            character = spaceCharacter;
        else if (Character::treatAsZeroWidthSpaceInComplexScript(character))
            character = zeroWidthSpaceCharacter;

        U16_APPEND(destination, *destinationLength, length, character, error);
        ASSERT_UNUSED(error, !error);
    }
}

HarfBuzzShaper::HarfBuzzShaper(const Font* font, const TextRun& run, const GlyphData* emphasisData,
    HashSet<const SimpleFontData*>* fallbackFonts, FloatRect* bounds)
    : Shaper(font, run, emphasisData, fallbackFonts, bounds)
    , m_normalizedBufferLength(0)
    , m_wordSpacingAdjustment(font->fontDescription().wordSpacing())
    , m_letterSpacing(font->fontDescription().letterSpacing())
    , m_expansionOpportunityCount(0)
    , m_fromIndex(0)
    , m_toIndex(m_run.length())
    , m_totalWidth(0)
{
    m_normalizedBuffer = adoptArrayPtr(new UChar[m_run.length() + 1]);
    normalizeCharacters(m_run, m_run.length(), m_normalizedBuffer.get(), &m_normalizedBufferLength);
    setExpansion(m_run.expansion());
    setFontFeatures();
}

float HarfBuzzShaper::nextExpansionPerOpportunity()
{
    if (!m_expansionOpportunityCount) {
        ASSERT_NOT_REACHED(); // failures indicate that the logic in HarfBuzzShaper does not match to the one in expansionOpportunityCount()
        return 0;
    }
    if (!--m_expansionOpportunityCount) {
        float remaining = m_expansion;
        m_expansion = 0;
        return remaining;
    }
    m_expansion -= m_expansionPerOpportunity;
    return m_expansionPerOpportunity;
}

// setPadding sets a number of pixels to be distributed across the TextRun.
// WebKit uses this to justify text.
void HarfBuzzShaper::setExpansion(float padding)
{
    m_expansion = padding;
    if (!m_expansion)
        return;

    // If we have padding to distribute, then we try to give an equal
    // amount to each expansion opportunity.
    bool isAfterExpansion = m_isAfterExpansion;
    m_expansionOpportunityCount = Character::expansionOpportunityCount(m_normalizedBuffer.get(), m_normalizedBufferLength, m_run.direction(), isAfterExpansion, m_run.textJustify());
    if (isAfterExpansion && !m_run.allowsTrailingExpansion()) {
        ASSERT(m_expansionOpportunityCount > 0);
        --m_expansionOpportunityCount;
    }

    if (m_expansionOpportunityCount)
        m_expansionPerOpportunity = m_expansion / m_expansionOpportunityCount;
    else
        m_expansionPerOpportunity = 0;
}


void HarfBuzzShaper::setDrawRange(int from, int to)
{
    ASSERT_WITH_SECURITY_IMPLICATION(from >= 0);
    ASSERT_WITH_SECURITY_IMPLICATION(to <= m_run.length());
    m_fromIndex = from;
    m_toIndex = to;
}

void HarfBuzzShaper::setFontFeatures()
{
    const FontDescription& description = m_font->fontDescription();

    static hb_feature_t noKern = { HB_TAG('k', 'e', 'r', 'n'), 0, 0, static_cast<unsigned>(-1) };
    static hb_feature_t noVkrn = { HB_TAG('v', 'k', 'r', 'n'), 0, 0, static_cast<unsigned>(-1) };
    switch (description.kerning()) {
    case FontDescription::NormalKerning:
        // kern/vkrn are enabled by default
        break;
    case FontDescription::NoneKerning:
        m_features.append(description.isVerticalAnyUpright() ? noVkrn : noKern);
        break;
    case FontDescription::AutoKerning:
        break;
    }

    static hb_feature_t noClig = { HB_TAG('c', 'l', 'i', 'g'), 0, 0, static_cast<unsigned>(-1) };
    static hb_feature_t noLiga = { HB_TAG('l', 'i', 'g', 'a'), 0, 0, static_cast<unsigned>(-1) };
    switch (description.commonLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        m_features.append(noLiga);
        m_features.append(noClig);
        break;
    case FontDescription::EnabledLigaturesState:
        // liga and clig are on by default
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }
    static hb_feature_t dlig = { HB_TAG('d', 'l', 'i', 'g'), 1, 0, static_cast<unsigned>(-1) };
    switch (description.discretionaryLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        // dlig is off by default
        break;
    case FontDescription::EnabledLigaturesState:
        m_features.append(dlig);
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }
    static hb_feature_t hlig = { HB_TAG('h', 'l', 'i', 'g'), 1, 0, static_cast<unsigned>(-1) };
    switch (description.historicalLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        // hlig is off by default
        break;
    case FontDescription::EnabledLigaturesState:
        m_features.append(hlig);
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }
    static hb_feature_t noCalt = { HB_TAG('c', 'a', 'l', 't'), 0, 0, static_cast<unsigned>(-1) };
    switch (description.contextualLigaturesState()) {
    case FontDescription::DisabledLigaturesState:
        m_features.append(noCalt);
        break;
    case FontDescription::EnabledLigaturesState:
        // calt is on by default
        break;
    case FontDescription::NormalLigaturesState:
        break;
    }

    static hb_feature_t hwid = { HB_TAG('h', 'w', 'i', 'd'), 1, 0, static_cast<unsigned>(-1) };
    static hb_feature_t twid = { HB_TAG('t', 'w', 'i', 'd'), 1, 0, static_cast<unsigned>(-1) };
    static hb_feature_t qwid = { HB_TAG('q', 'w', 'i', 'd'), 1, 0, static_cast<unsigned>(-1) };
    switch (description.widthVariant()) {
    case HalfWidth:
        m_features.append(hwid);
        break;
    case ThirdWidth:
        m_features.append(twid);
        break;
    case QuarterWidth:
        m_features.append(qwid);
        break;
    case RegularWidth:
        break;
    }

    FontFeatureSettings* settings = description.featureSettings();
    if (!settings)
        return;

    unsigned numFeatures = settings->size();
    for (unsigned i = 0; i < numFeatures; ++i) {
        hb_feature_t feature;
        const AtomicString& tag = settings->at(i).tag();
        feature.tag = HB_TAG(tag[0], tag[1], tag[2], tag[3]);
        feature.value = settings->at(i).value();
        feature.start = 0;
        feature.end = static_cast<unsigned>(-1);
        m_features.append(feature);
    }
}

bool HarfBuzzShaper::shape(GlyphBuffer* glyphBuffer)
{
    if (!createHarfBuzzRuns())
        return false;

    if (!shapeHarfBuzzRuns())
        return false;

    if (glyphBuffer && !fillGlyphBuffer(glyphBuffer))
        return false;

    return true;
}

struct CandidateRun {
    UChar32 character;
    unsigned start;
    unsigned end;
    const SimpleFontData* fontData;
    UScriptCode script;
};

static inline bool collectCandidateRuns(const UChar* normalizedBuffer,
    size_t bufferLength, const Font* font, Vector<CandidateRun>* runs, bool isSpaceNormalize)
{
    UTF16TextIterator iterator(normalizedBuffer, bufferLength);
    UChar32 character;
    unsigned startIndexOfCurrentRun = 0;

    if (!iterator.consume(character))
        return false;

    const SimpleFontData* nextFontData = font->glyphDataForCharacter(character, false, isSpaceNormalize).fontData;
    UErrorCode errorCode = U_ZERO_ERROR;
    UScriptCode nextScript = uscript_getScript(character, &errorCode);
    if (U_FAILURE(errorCode))
        return false;

    do {
        const UChar* currentCharacterPosition = iterator.characters();
        const SimpleFontData* currentFontData = nextFontData;
        UScriptCode currentScript = nextScript;

        UChar32 lastCharacter = character;
        for (iterator.advance(); iterator.consume(character); iterator.advance()) {
            if (Character::treatAsZeroWidthSpace(character))
                continue;
            if ((U_GET_GC_MASK(character) & U_GC_M_MASK)
                && currentFontData->canRenderCombiningCharacterSequence(
                    currentCharacterPosition,
                    iterator.glyphEnd() - currentCharacterPosition))
                continue;

            nextFontData = font->glyphDataForCharacter(character, false, isSpaceNormalize).fontData;
            nextScript = uscript_getScript(character, &errorCode);
            if (U_FAILURE(errorCode))
                return false;
            if (lastCharacter == zeroWidthJoinerCharacter)
                currentFontData = nextFontData;
            if ((nextFontData != currentFontData) || ((currentScript != nextScript) && (nextScript != USCRIPT_INHERITED) && (!uscript_hasScript(character, currentScript))))
                break;
            currentCharacterPosition = iterator.characters();
            lastCharacter = character;
        }

        CandidateRun run = { character, startIndexOfCurrentRun, static_cast<unsigned>(iterator.offset()), currentFontData, currentScript };
        runs->append(run);

        startIndexOfCurrentRun = iterator.offset();
    } while (iterator.consume(character));

    return true;
}

static inline bool matchesAdjacentRun(UScriptCode* scriptExtensions, int length,
    CandidateRun& adjacentRun)
{
    for (int i = 0; i < length; i++) {
        if (scriptExtensions[i] == adjacentRun.script)
            return true;
    }
    return false;
}

static inline void resolveRunBasedOnScriptExtensions(Vector<CandidateRun>& runs,
    CandidateRun& run, size_t i, size_t length, UScriptCode* scriptExtensions,
    int extensionsLength, size_t& nextResolvedRun)
{
    // If uscript_getScriptExtensions returns 1 it only contains the script value,
    // we only care about ScriptExtensions which is indicated by a value >= 2.
    if (extensionsLength <= 1)
        return;

    if (i > 0 && matchesAdjacentRun(scriptExtensions, extensionsLength, runs[i - 1])) {
        run.script = runs[i - 1].script;
        return;
    }

    for (size_t j = i + 1; j < length; j++) {
        if (runs[j].script != USCRIPT_COMMON
            && runs[j].script != USCRIPT_INHERITED
            && matchesAdjacentRun(scriptExtensions, extensionsLength, runs[j])) {
            nextResolvedRun = j;
            break;
        }
    }
}

static inline void resolveRunBasedOnScriptValue(Vector<CandidateRun>& runs,
    CandidateRun& run, size_t i, size_t length, size_t& nextResolvedRun)
{
    if (run.script != USCRIPT_COMMON)
        return;

    if (i > 0 && runs[i - 1].script != USCRIPT_COMMON) {
        run.script = runs[i - 1].script;
        return;
    }

    for (size_t j = i + 1; j < length; j++) {
        if (runs[j].script != USCRIPT_COMMON
            && runs[j].script != USCRIPT_INHERITED) {
            nextResolvedRun = j;
            break;
        }
    }
}

static inline bool resolveCandidateRuns(Vector<CandidateRun>& runs)
{
    UScriptCode scriptExtensions[USCRIPT_CODE_LIMIT];
    UErrorCode errorCode = U_ZERO_ERROR;
    size_t length = runs.size();
    size_t nextResolvedRun = 0;
    for (size_t i = 0; i < length; i++) {
        CandidateRun& run = runs[i];
        nextResolvedRun = 0;

        if (run.script == USCRIPT_INHERITED)
            run.script = i > 0 ? runs[i - 1].script : USCRIPT_COMMON;

        int extensionsLength = uscript_getScriptExtensions(run.character,
            scriptExtensions, sizeof(scriptExtensions) / sizeof(scriptExtensions[0]),
            &errorCode);
        if (U_FAILURE(errorCode))
            return false;

        resolveRunBasedOnScriptExtensions(runs, run, i, length,
            scriptExtensions, extensionsLength, nextResolvedRun);
        resolveRunBasedOnScriptValue(runs, run, i, length,
            nextResolvedRun);
        for (size_t j = i; j < nextResolvedRun; j++)
            runs[j].script = runs[nextResolvedRun].script;

        i = std::max(i, nextResolvedRun);
    }
    return true;
}

// For ideographic (CJK) documents, 90-95% of calls from width() are one character length
// because most characters have break opportunities both before and after.
bool HarfBuzzShaper::createHarfBuzzRunsForSingleCharacter()
{
    ASSERT(m_normalizedBufferLength == 1);
    UChar32 character = m_normalizedBuffer[0];
    if (!U16_IS_SINGLE(character))
        return false;
    const SimpleFontData* fontData = m_font->glyphDataForCharacter(character, false, m_run.normalizeSpace()).fontData;
    UErrorCode errorCode = U_ZERO_ERROR;
    UScriptCode script = uscript_getScript(character, &errorCode);
    if (U_FAILURE(errorCode))
        return false;
    addHarfBuzzRun(0, 1, fontData, script);
    return true;
}

bool HarfBuzzShaper::createHarfBuzzRuns()
{
    if (m_normalizedBufferLength == 1)
        return createHarfBuzzRunsForSingleCharacter();

    Vector<CandidateRun> candidateRuns;
    if (!collectCandidateRuns(m_normalizedBuffer.get(),
        m_normalizedBufferLength, m_font, &candidateRuns, m_run.normalizeSpace()))
        return false;

    if (!resolveCandidateRuns(candidateRuns))
        return false;

    size_t length = candidateRuns.size();
    for (size_t i = 0; i < length; ) {
        CandidateRun& run = candidateRuns[i];
        CandidateRun lastMatchingRun = run;
        for (i++; i < length; i++) {
            if (candidateRuns[i].script != run.script
                || candidateRuns[i].fontData != run.fontData)
                break;
            lastMatchingRun = candidateRuns[i];
        }
        addHarfBuzzRun(run.start, lastMatchingRun.end, run.fontData, run.script);
    }
    return !m_harfBuzzRuns.isEmpty();
}

// A port of hb_icu_script_to_script because harfbuzz on CrOS is built
// without hb-icu. See http://crbug.com/356929
static inline hb_script_t ICUScriptToHBScript(UScriptCode script)
{
    if (UNLIKELY(script == USCRIPT_INVALID_CODE))
        return HB_SCRIPT_INVALID;

    return hb_script_from_string(uscript_getShortName(script), -1);
}

static inline hb_direction_t TextDirectionToHBDirection(TextDirection dir, FontOrientation orientation, const SimpleFontData* fontData)
{
    hb_direction_t harfBuzzDirection = isVerticalAnyUpright(orientation) && !fontData->isTextOrientationFallback() ? HB_DIRECTION_TTB : HB_DIRECTION_LTR;
    return dir == RTL ? HB_DIRECTION_REVERSE(harfBuzzDirection) : harfBuzzDirection;
}

void HarfBuzzShaper::addHarfBuzzRun(unsigned startCharacter,
    unsigned endCharacter, const SimpleFontData* fontData,
    UScriptCode script)
{
    ASSERT(endCharacter > startCharacter);
    ASSERT(script != USCRIPT_INVALID_CODE);
    if (m_fallbackFonts)
        trackNonPrimaryFallbackFont(fontData);
    return m_harfBuzzRuns.append(HarfBuzzRun::create(fontData,
        startCharacter, endCharacter - startCharacter,
        TextDirectionToHBDirection(m_run.direction(), m_font->fontDescription().orientation(), fontData),
        ICUScriptToHBScript(script)));
}

static inline bool isValidCachedResult(const Font* font, hb_direction_t dir,
    const String& localeString, const CachedShapingResults* cachedResults)
{
    ASSERT(cachedResults);
    return cachedResults->dir == dir
        && cachedResults->font == *font
        && !cachedResults->font.loadingCustomFonts()
        && !font->loadingCustomFonts()
        && cachedResults->locale == localeString;
}

static const uint16_t* toUint16(const UChar* src)
{
    // FIXME: This relies on undefined behavior however it works on the
    // current versions of all compilers we care about and avoids making
    // a copy of the string.
    static_assert(sizeof(UChar) == sizeof(uint16_t), "UChar should be the same size as uint16_t");
    return reinterpret_cast<const uint16_t*>(src);
}

static inline void addToHarfBuzzBufferInternal(hb_buffer_t* buffer,
    const FontDescription& fontDescription, const UChar* normalizedBuffer,
    unsigned startIndex, unsigned numCharacters)
{
    if (fontDescription.variant() == FontVariantSmallCaps
        && u_islower(normalizedBuffer[startIndex])) {
        String upperText = String(normalizedBuffer + startIndex, numCharacters)
            .upper();
        // TextRun is 16 bit, therefore upperText is 16 bit, even after we call
        // makeUpper().
        ASSERT(!upperText.is8Bit());
        hb_buffer_add_utf16(buffer, toUint16(upperText.characters16()),
            numCharacters, 0, numCharacters);
    } else {
        hb_buffer_add_utf16(buffer, toUint16(normalizedBuffer + startIndex),
            numCharacters, 0, numCharacters);
    }
}

bool HarfBuzzShaper::shapeHarfBuzzRuns()
{
    HarfBuzzScopedPtr<hb_buffer_t> harfBuzzBuffer(hb_buffer_create(), hb_buffer_destroy);

    HarfBuzzRunCache& runCache = harfBuzzRunCache();
    const FontDescription& fontDescription = m_font->fontDescription();
    const String& localeString = fontDescription.locale();
    CString locale = localeString.latin1();
    const hb_language_t language = hb_language_from_string(locale.data(), locale.length());
    HarfBuzzRun* previousRun = nullptr;

    for (unsigned i = 0; i < m_harfBuzzRuns.size(); ++i) {
        unsigned runIndex = m_run.rtl() ? m_harfBuzzRuns.size() - i - 1 : i;
        HarfBuzzRun* currentRun = m_harfBuzzRuns[runIndex].get();

        const SimpleFontData* currentFontData = currentRun->fontData();
        FontPlatformData* platformData = const_cast<FontPlatformData*>(&currentFontData->platformData());
        HarfBuzzFace* face = platformData->harfBuzzFace();
        if (!face)
            return false;

        hb_buffer_set_language(harfBuzzBuffer.get(), language);
        hb_buffer_set_script(harfBuzzBuffer.get(), currentRun->script());
        hb_buffer_set_direction(harfBuzzBuffer.get(), currentRun->direction());

        const UChar* src = m_normalizedBuffer.get() + currentRun->startIndex();
        std::wstring key(src, src + currentRun->numCharacters());

        CachedShapingResults* cachedResults = runCache.find(key);
        if (cachedResults) {
            if (isValidCachedResult(m_font, currentRun->direction(),
                localeString, cachedResults)) {
                currentRun->applyShapeResult(cachedResults->buffer);
                setGlyphPositionsForHarfBuzzRun(currentRun,
                    cachedResults->buffer, previousRun);
                hb_buffer_clear_contents(harfBuzzBuffer.get());
                runCache.moveToBack(cachedResults);
                previousRun = currentRun;
                continue;
            }
            runCache.remove(cachedResults);
        }

        // Add a space as pre-context to the buffer. This prevents showing dotted-circle
        // for combining marks at the beginning of runs.
        static const uint16_t preContext = spaceCharacter;
        hb_buffer_add_utf16(harfBuzzBuffer.get(), &preContext, 1, 1, 0);

        addToHarfBuzzBufferInternal(harfBuzzBuffer.get(),
            fontDescription, m_normalizedBuffer.get(), currentRun->startIndex(),
            currentRun->numCharacters());

        if (fontDescription.isVerticalAnyUpright())
            face->setScriptForVerticalGlyphSubstitution(harfBuzzBuffer.get());

        HarfBuzzScopedPtr<hb_font_t> harfBuzzFont(face->createFont(), hb_font_destroy);

        hb_shape(harfBuzzFont.get(), harfBuzzBuffer.get(), m_features.isEmpty() ? 0 : m_features.data(), m_features.size());
        currentRun->applyShapeResult(harfBuzzBuffer.get());
        setGlyphPositionsForHarfBuzzRun(currentRun, harfBuzzBuffer.get(), previousRun);

        runCache.insert(key, new CachedShapingResults(harfBuzzBuffer.get(), m_font, currentRun->direction(), localeString));

        harfBuzzBuffer.set(hb_buffer_create());

        previousRun = currentRun;
    }

    // We should have consumed all expansion opportunities.
    // Failures here means that our logic does not match to the one in expansionOpportunityCount().
    // FIXME: Ideally, we should ASSERT(!m_expansionOpportunityCount) here to ensure that,
    // or unify the two logics (and the one in SimplePath too,) but there are some cases where our impl
    // does not support justification very well yet such as U+3099, and it'll cause the ASSERT to fail.
    // It's to be fixed because they're very rarely used, and a broken justification is still somewhat readable.

    return true;
}

void HarfBuzzShaper::setGlyphPositionsForHarfBuzzRun(HarfBuzzRun* currentRun, hb_buffer_t* harfBuzzBuffer, HarfBuzzRun* previousRun)
{
    // Skip runs that only contain control characters.
    if (!currentRun->numGlyphs())
        return;

    const SimpleFontData* currentFontData = currentRun->fontData();
    hb_glyph_info_t* glyphInfos = hb_buffer_get_glyph_infos(harfBuzzBuffer, 0);
    hb_glyph_position_t* glyphPositions = hb_buffer_get_glyph_positions(harfBuzzBuffer, 0);

    unsigned numGlyphs = currentRun->numGlyphs();
    uint16_t* glyphToCharacterIndexes = currentRun->glyphToCharacterIndexes();
    float totalAdvance = 0;
    FloatPoint glyphOrigin;

    // HarfBuzz returns the shaping result in visual order. We need not to flip for RTL.
    for (size_t i = 0; i < numGlyphs; ++i) {
        bool runEnd = i + 1 == numGlyphs;
        uint16_t glyph = glyphInfos[i].codepoint;
        float offsetX = harfBuzzPositionToFloat(glyphPositions[i].x_offset);
        float offsetY = -harfBuzzPositionToFloat(glyphPositions[i].y_offset);
        // One out of x_advance and y_advance is zero, depending on
        // whether the buffer direction is horizontal or vertical.
        float advance = harfBuzzPositionToFloat(glyphPositions[i].x_advance - glyphPositions[i].y_advance);

        unsigned currentCharacterIndex = currentRun->startIndex() + glyphInfos[i].cluster;
        RELEASE_ASSERT(m_normalizedBufferLength > currentCharacterIndex);
        bool isClusterEnd = runEnd || glyphInfos[i].cluster != glyphInfos[i + 1].cluster;
        float spacing = 0;

        glyphToCharacterIndexes[i] = glyphInfos[i].cluster;

        if (isClusterEnd)
            spacing += adjustSpacing(currentRun, i, currentCharacterIndex, previousRun, offsetX, totalAdvance);

        if (currentFontData->isZeroWidthSpaceGlyph(glyph)) {
            currentRun->setGlyphAndPositions(i, glyph, 0, 0, 0);
            continue;
        }

        advance += spacing;
        if (m_run.rtl()) {
            // In RTL, spacing should be added to left side of glyphs.
            offsetX += spacing;
            if (!isClusterEnd)
                offsetX += m_letterSpacing;
        }

        currentRun->setGlyphAndPositions(i, glyph, advance, offsetX, offsetY);

        if (m_glyphBoundingBox) {
            FloatRect glyphBounds = currentFontData->boundsForGlyph(glyph);
            glyphBounds.move(glyphOrigin.x(), glyphOrigin.y());
            m_glyphBoundingBox->unite(glyphBounds);
            glyphOrigin += FloatSize(advance + offsetX, offsetY);
        }

        totalAdvance += advance;
    }
    currentRun->setWidth(totalAdvance > 0.0 ? totalAdvance : 0.0);
    m_totalWidth += currentRun->width();
}

float HarfBuzzShaper::adjustSpacing(HarfBuzzRun* currentRun, size_t glyphIndex, unsigned currentCharacterIndex, HarfBuzzRun* previousRun, float& offsetX, float& totalAdvance)
{
    float spacing = 0;
    UChar32 character = m_normalizedBuffer[currentCharacterIndex];
    if (m_letterSpacing && !Character::treatAsZeroWidthSpace(character))
        spacing += m_letterSpacing;

    bool treatAsSpace = Character::treatAsSpace(character);
    if (treatAsSpace && currentCharacterIndex && (character != '\t' || !m_run.allowTabs()))
        spacing += m_wordSpacingAdjustment;

    if (!m_expansionOpportunityCount)
        return spacing;

    if (treatAsSpace) {
        spacing += nextExpansionPerOpportunity();
        m_isAfterExpansion = true;
        return spacing;
    }

    if (m_run.textJustify() != TextJustify::TextJustifyAuto) {
        m_isAfterExpansion = false;
        return spacing;
    }

    // isCJKIdeographOrSymbol() has expansion opportunities both before and after each character.
    // http://www.w3.org/TR/jlreq/#line_adjustment
    if (U16_IS_LEAD(character) && currentCharacterIndex + 1 < m_normalizedBufferLength && U16_IS_TRAIL(m_normalizedBuffer[currentCharacterIndex + 1]))
        character = U16_GET_SUPPLEMENTARY(character, m_normalizedBuffer[currentCharacterIndex + 1]);
    if (!Character::isCJKIdeographOrSymbol(character)) {
        m_isAfterExpansion = false;
        return spacing;
    }

    if (!m_isAfterExpansion) {
        // Take the expansion opportunity before this ideograph.
        float expandBefore = nextExpansionPerOpportunity();
        if (expandBefore) {
            if (glyphIndex > 0) {
                currentRun->addAdvance(glyphIndex - 1, expandBefore);
                totalAdvance += expandBefore;
            } else if (previousRun) {
                previousRun->addAdvance(previousRun->numGlyphs() - 1, expandBefore);
                previousRun->setWidth(previousRun->width() + expandBefore);
                m_totalWidth += expandBefore;
            } else {
                offsetX += expandBefore;
                totalAdvance += expandBefore;
            }
        }
        if (!m_expansionOpportunityCount)
            return spacing;
    }

    // Don't need to check m_run.allowsTrailingExpansion() since it's covered by !m_expansionOpportunityCount above
    spacing += nextExpansionPerOpportunity();
    m_isAfterExpansion = true;
    return spacing;
}

float HarfBuzzShaper::fillGlyphBufferFromHarfBuzzRun(GlyphBuffer* glyphBuffer,
    HarfBuzzRun* currentRun, float initialAdvance)
{
    FloatSize* offsets = currentRun->offsets();
    uint16_t* glyphs = currentRun->glyphs();
    float* advances = currentRun->advances();
    unsigned numGlyphs = currentRun->numGlyphs();
    uint16_t* glyphToCharacterIndexes = currentRun->glyphToCharacterIndexes();
    float advanceSoFar = initialAdvance;
    if (m_run.rtl()) {
        for (unsigned i = 0; i < numGlyphs; ++i) {
            uint16_t currentCharacterIndex = currentRun->startIndex() + glyphToCharacterIndexes[i];
            if (currentCharacterIndex >= m_toIndex) {
                advanceSoFar += advances[i];
            } else if (currentCharacterIndex >= m_fromIndex) {
                FloatPoint runStartOffset = HB_DIRECTION_IS_HORIZONTAL(currentRun->direction()) ?
                    FloatPoint(advanceSoFar, 0) : FloatPoint(0, advanceSoFar);
                glyphBuffer->add(glyphs[i], currentRun->fontData(), runStartOffset + offsets[i]);
                advanceSoFar += advances[i];
            }
        }
    } else {
        for (unsigned i = 0; i < numGlyphs; ++i) {
            uint16_t currentCharacterIndex = currentRun->startIndex() + glyphToCharacterIndexes[i];
            if (currentCharacterIndex < m_fromIndex) {
                advanceSoFar += advances[i];
            } else if (currentCharacterIndex < m_toIndex) {
                FloatPoint runStartOffset = HB_DIRECTION_IS_HORIZONTAL(currentRun->direction()) ?
                    FloatPoint(advanceSoFar, 0) : FloatPoint(0, advanceSoFar);
                glyphBuffer->add(glyphs[i], currentRun->fontData(), runStartOffset + offsets[i]);
                advanceSoFar += advances[i];
            }
        }
    }

    return advanceSoFar - initialAdvance;
}

float HarfBuzzShaper::fillGlyphBufferForTextEmphasis(GlyphBuffer* glyphBuffer, HarfBuzzRun* currentRun, float initialAdvance)
{
    float* advances = currentRun->advances();
    unsigned numGlyphs = currentRun->numGlyphs();
    uint16_t* glyphToCharacterIndexes = currentRun->glyphToCharacterIndexes();
    unsigned graphemesInCluster = 1;
    float clusterAdvance = 0;
    uint16_t clusterStart;

    // A "cluster" in this context means a cluster as it is used by HarfBuzz:
    // The minimal group of characters and corresponding glyphs, that cannot be broken
    // down further from a text shaping point of view.
    // A cluster can contain multiple glyphs and grapheme clusters, with mutually
    // overlapping boundaries. Below we count grapheme clusters per HarfBuzz clusters,
    // then linearly split the sum of corresponding glyph advances by the number of
    // grapheme clusters in order to find positions for emphasis mark drawing.

    if (m_run.rtl())
        clusterStart = currentRun->startIndex() + currentRun->numCharacters();
    else
        clusterStart = currentRun->startIndex() + glyphToCharacterIndexes[0];

    float advanceSoFar = initialAdvance;
    for (unsigned i = 0; i < numGlyphs; ++i) {
        uint16_t currentCharacterIndex = currentRun->startIndex() + glyphToCharacterIndexes[i];
        bool isRunEnd = (i + 1 == numGlyphs);
        bool isClusterEnd =  isRunEnd || (currentRun->startIndex() + glyphToCharacterIndexes[i + 1] != currentCharacterIndex);

        if ((m_run.rtl() && currentCharacterIndex >= m_toIndex) || (!m_run.rtl() && currentCharacterIndex < m_fromIndex)) {
            advanceSoFar += advances[i];
            m_run.rtl() ? --clusterStart : ++clusterStart;
            continue;
        }

        clusterAdvance += advances[i];

        if (isClusterEnd) {
            uint16_t clusterEnd;
            if (m_run.rtl())
                clusterEnd = currentCharacterIndex;
            else
                clusterEnd = isRunEnd ? currentRun->startIndex() + currentRun->numCharacters() : currentRun->startIndex() + glyphToCharacterIndexes[i + 1];

            graphemesInCluster = countGraphemesInCluster(m_normalizedBuffer.get(), m_normalizedBufferLength, clusterStart, clusterEnd);
            if (!graphemesInCluster || !clusterAdvance)
                continue;

            float glyphAdvanceX = clusterAdvance / graphemesInCluster;
            for (unsigned j = 0; j < graphemesInCluster; ++j) {
                // Do not put emphasis marks on space, separator, and control characters.
                if (Character::canReceiveTextEmphasis(m_run[currentCharacterIndex]))
                    addEmphasisMark(glyphBuffer, advanceSoFar + glyphAdvanceX / 2);

                advanceSoFar += glyphAdvanceX;
            }
            clusterStart = clusterEnd;
            clusterAdvance = 0;
        }
    }

    return advanceSoFar - initialAdvance;
}

bool HarfBuzzShaper::fillGlyphBuffer(GlyphBuffer* glyphBuffer)
{
    ASSERT(glyphBuffer);

    unsigned numRuns = m_harfBuzzRuns.size();
    float advanceSoFar = 0;
    for (unsigned runIndex = 0; runIndex < numRuns; ++runIndex) {
        HarfBuzzRun* currentRun = m_harfBuzzRuns[m_run.ltr() ? runIndex : numRuns - runIndex - 1].get();
        // Skip runs that only contain control characters.
        if (!currentRun->numGlyphs())
            continue;
        advanceSoFar += forTextEmphasis()
            ? fillGlyphBufferForTextEmphasis(glyphBuffer, currentRun, advanceSoFar)
            : fillGlyphBufferFromHarfBuzzRun(glyphBuffer, currentRun, advanceSoFar);
    }
    return glyphBuffer->size();
}

int HarfBuzzShaper::offsetForPosition(float targetX)
{
    int charactersSoFar = 0;
    float currentX = 0;

    if (m_run.rtl()) {
        charactersSoFar = m_normalizedBufferLength;
        for (int i = m_harfBuzzRuns.size() - 1; i >= 0; --i) {
            charactersSoFar -= m_harfBuzzRuns[i]->numCharacters();
            float nextX = currentX + m_harfBuzzRuns[i]->width();
            float offsetForRun = targetX - currentX;
            if (offsetForRun >= 0 && offsetForRun <= m_harfBuzzRuns[i]->width()) {
                // The x value in question is within this script run.
                const unsigned index = m_harfBuzzRuns[i]->characterIndexForXPosition(offsetForRun);
                return charactersSoFar + index;
            }
            currentX = nextX;
        }
    } else {
        for (unsigned i = 0; i < m_harfBuzzRuns.size(); ++i) {
            float nextX = currentX + m_harfBuzzRuns[i]->width();
            float offsetForRun = targetX - currentX;
            if (offsetForRun >= 0 && offsetForRun <= m_harfBuzzRuns[i]->width()) {
                const unsigned index = m_harfBuzzRuns[i]->characterIndexForXPosition(offsetForRun);
                return charactersSoFar + index;
            }
            charactersSoFar += m_harfBuzzRuns[i]->numCharacters();
            currentX = nextX;
        }
    }

    return charactersSoFar;
}

FloatRect HarfBuzzShaper::selectionRect(const FloatPoint& point, int height, int from, int to)
{
    float currentX = 0;
    float fromX = 0;
    float toX = 0;
    bool foundFromX = false;
    bool foundToX = false;

    if (m_run.rtl())
        currentX = m_totalWidth;
    for (unsigned i = 0; i < m_harfBuzzRuns.size(); ++i) {
        if (m_run.rtl())
            currentX -= m_harfBuzzRuns[i]->width();
        int numCharacters = m_harfBuzzRuns[i]->numCharacters();
        if (!foundFromX && from >= 0 && from < numCharacters) {
            fromX = m_harfBuzzRuns[i]->xPositionForOffset(from) + currentX;
            foundFromX = true;
        } else {
            from -= numCharacters;
        }

        if (!foundToX && to >= 0 && to < numCharacters) {
            toX = m_harfBuzzRuns[i]->xPositionForOffset(to) + currentX;
            foundToX = true;
        } else {
            to -= numCharacters;
        }

        if (foundFromX && foundToX)
            break;
        if (!m_run.rtl())
            currentX += m_harfBuzzRuns[i]->width();
    }

    // The position in question might be just after the text.
    if (!foundFromX)
        fromX = 0;
    if (!foundToX)
        toX = m_run.rtl() ? 0 : m_totalWidth;
    // None of our HarfBuzzRuns is part of the selection,
    // possibly invalid from, to arguments.
    if (!foundToX && !foundFromX)
        fromX = toX = 0;

    if (fromX < toX)
        return FloatRect(point.x() + fromX, point.y(), toX - fromX, height);
    return FloatRect(point.x() + toX, point.y(), fromX - toX, height);
}

} // namespace blink

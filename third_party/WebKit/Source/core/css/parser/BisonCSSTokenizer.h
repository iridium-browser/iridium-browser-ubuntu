/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef BisonCSSTokenizer_h
#define BisonCSSTokenizer_h

#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BisonCSSParser;
struct CSSParserLocation;
struct CSSParserString;

class BisonCSSTokenizer {
    WTF_MAKE_NONCOPYABLE(BisonCSSTokenizer);
public:
    // FIXME: This should not be needed but there are still some ties between the 2 classes.
    friend class BisonCSSParser;

    BisonCSSTokenizer(BisonCSSParser& parser)
        : m_parser(parser)
        , m_parsedTextPrefixLength(0)
        , m_parsedTextSuffixLength(0)
        , m_parsingMode(NormalMode)
        , m_is8BitSource(false)
        , m_length(0)
        , m_token(0)
        , m_lineNumber(0)
        , m_tokenStartLineNumber(0)
        , m_internal(true)
    {
        m_tokenStart.ptr8 = 0;
    }

    void setupTokenizer(const char* prefix, unsigned prefixLength, const String&, const char* suffix, unsigned suffixLength);

    CSSParserLocation currentLocation();

    inline int lex(void* yylval) { return (this->*m_lexFunc)(yylval); }

    inline unsigned safeUserStringTokenOffset()
    {
        return std::min(tokenStartOffset(), static_cast<unsigned>(m_length - 1 - m_parsedTextSuffixLength)) - m_parsedTextPrefixLength;
    }

    bool is8BitSource() const { return m_is8BitSource; }

    // FIXME: These 2 functions should be private so that we don't need the definitions below.
    template <typename CharacterType>
    inline CharacterType* tokenStart();

    inline unsigned tokenStartOffset();

private:
    UChar* allocateStringBuffer16(size_t len);

    template <typename CharacterType>
    inline CharacterType*& currentCharacter();

    template <typename CharacterType>
    inline CharacterType* dataStart();

    template <typename CharacterType>
    inline void setTokenStart(CharacterType*);

    template <typename CharacterType>
    inline bool isIdentifierStart();

    template <typename CharacterType>
    inline CSSParserLocation tokenLocation();

    template <typename CharacterType>
    static unsigned parseEscape(CharacterType*&);
    template <typename DestCharacterType>
    static inline void UnicodeToChars(DestCharacterType*&, unsigned);

    template <typename SrcCharacterType, typename DestCharacterType>
    static inline bool parseIdentifierInternal(SrcCharacterType*&, DestCharacterType*&, bool&);
    template <typename SrcCharacterType>
    static size_t peekMaxIdentifierLen(SrcCharacterType*);
    template <typename CharacterType>
    inline void parseIdentifier(CharacterType*&, CSSParserString&, bool&);

    template <typename SrcCharacterType>
    static size_t peekMaxStringLen(SrcCharacterType*, UChar quote);
    template <typename SrcCharacterType, typename DestCharacterType>
    static inline bool parseStringInternal(SrcCharacterType*&, DestCharacterType*&, UChar);
    template <typename CharacterType>
    inline void parseString(CharacterType*&, CSSParserString& resultString, UChar);

    template <typename CharacterType>
    inline bool findURI(CharacterType*& start, CharacterType*& end, UChar& quote);
    template <typename SrcCharacterType>
    static size_t peekMaxURILen(SrcCharacterType*, UChar quote);
    template <typename SrcCharacterType, typename DestCharacterType>
    static inline bool parseURIInternal(SrcCharacterType*&, DestCharacterType*&, UChar quote);
    template <typename CharacterType>
    inline void parseURI(CSSParserString&);

    template <typename CharacterType>
    inline bool parseUnicodeRange();
    template <typename CharacterType>
    bool parseNthChild();
    template <typename CharacterType>
    bool parseNthChildExtra();
    template <typename CharacterType>
    inline bool detectFunctionTypeToken(int);
    template <typename CharacterType>
    inline void detectMediaQueryToken(int);
    template <typename CharacterType>
    inline void detectNumberToken(CharacterType*, int);
    template <typename CharacterType>
    inline void detectDashToken(int);
    template <typename CharacterType>
    inline void detectAtToken(int, bool);
    template <typename CharacterType>
    inline void detectSupportsToken(int);

    template <typename SourceCharacterType>
    int realLex(void* yylval);

    BisonCSSParser& m_parser;

    size_t m_parsedTextPrefixLength;
    size_t m_parsedTextSuffixLength;

    enum ParsingMode {
        NormalMode,
        MediaQueryMode,
        SupportsMode,
        NthChildMode
    };

    ParsingMode m_parsingMode;
    bool m_is8BitSource;
    OwnPtr<LChar[]> m_dataStart8;
    OwnPtr<UChar[]> m_dataStart16;
    LChar* m_currentCharacter8;
    UChar* m_currentCharacter16;

    // During parsing of an ASCII stylesheet we might locate escape
    // sequences that expand into UTF-16 code points. Strings,
    // identifiers and URIs containing such escape sequences are
    // stored in m_cssStrings16 so that we don't have to store the
    // whole stylesheet as UTF-16.
    Vector<OwnPtr<UChar[]>> m_cssStrings16;
    union {
        LChar* ptr8;
        UChar* ptr16;
    } m_tokenStart;
    unsigned m_length;
    int m_token;
    int m_lineNumber;
    int m_tokenStartLineNumber;

    // FIXME: This boolean is misnamed. Also it would be nice if we could consolidate it
    // with the CSSParserMode logic to determine if internal properties are allowed.
    bool m_internal;

    int (BisonCSSTokenizer::*m_lexFunc)(void*);
};

inline unsigned BisonCSSTokenizer::tokenStartOffset()
{
    if (is8BitSource())
        return m_tokenStart.ptr8 - m_dataStart8.get();
    return m_tokenStart.ptr16 - m_dataStart16.get();
}

template <>
inline LChar* BisonCSSTokenizer::tokenStart<LChar>()
{
    return m_tokenStart.ptr8;
}

template <>
inline UChar* BisonCSSTokenizer::tokenStart<UChar>()
{
    return m_tokenStart.ptr16;
}

} // namespace blink

#endif // BisonCSSTokenizer_h

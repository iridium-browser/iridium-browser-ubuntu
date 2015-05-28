/*
 * Copyright (c) 2013, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VTTCue_h
#define VTTCue_h

#include "core/html/track/TextTrackCue.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class DoubleOrAutoKeyword;
class ExecutionContext;
class VTTCue;
class VTTScanner;

struct VTTDisplayParameters {
    VTTDisplayParameters();

    FloatPoint position;
    float size;
    CSSValueID direction;
    CSSValueID writingMode;
};

class VTTCueBox final : public HTMLDivElement {
public:
    static PassRefPtrWillBeRawPtr<VTTCueBox> create(Document& document, VTTCue* cue)
    {
        return adoptRefWillBeNoop(new VTTCueBox(document, cue));
    }

    VTTCue* getCue() const { return m_cue; }
    void applyCSSProperties(const VTTDisplayParameters&);

    DECLARE_VIRTUAL_TRACE();

private:
    VTTCueBox(Document&, VTTCue*);

    virtual LayoutObject* createLayoutObject(const ComputedStyle&) override;

    RawPtrWillBeMember<VTTCue> m_cue;
};

class VTTCue final : public TextTrackCue {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<VTTCue> create(Document& document, double startTime, double endTime, const String& text)
    {
        return adoptRefWillBeNoop(new VTTCue(document, startTime, endTime, text));
    }

    virtual ~VTTCue();

    const String& vertical() const;
    void setVertical(const String&);

    bool snapToLines() const { return m_snapToLines; }
    void setSnapToLines(bool);

    void line(DoubleOrAutoKeyword&) const;
    void setLine(const DoubleOrAutoKeyword&);

    void position(DoubleOrAutoKeyword&) const;
    void setPosition(const DoubleOrAutoKeyword&, ExceptionState&);

    double size() const { return m_cueSize; }
    void setSize(double, ExceptionState&);

    const String& align() const;
    void setAlign(const String&);

    const String& text() const { return m_text; }
    void setText(const String&);

    void parseSettings(const String&);

    // Applies CSS override style from user settings.
    void applyUserOverrideCSSProperties();

    PassRefPtrWillBeRawPtr<DocumentFragment> getCueAsHTML();

    const String& regionId() const { return m_regionId; }
    void setRegionId(const String&);

    virtual void updateDisplay(HTMLDivElement& container) override;

    virtual void updatePastAndFutureNodes(double movieTime) override;

    virtual void removeDisplayTree(RemovalNotification) override;

    float calculateComputedLinePosition() const;

    enum WritingDirection {
        Horizontal = 0,
        VerticalGrowingLeft,
        VerticalGrowingRight,
        NumberOfWritingDirections
    };
    WritingDirection getWritingDirection() const { return m_writingDirection; }

    enum CueAlignment {
        Start = 0,
        Middle,
        End,
        Left,
        Right,
        NumberOfAlignments
    };
    CueAlignment cueAlignment() const { return m_cueAlignment; }

    virtual ExecutionContext* executionContext() const override;

#ifndef NDEBUG
    virtual String toString() const override;
#endif

    DECLARE_VIRTUAL_TRACE();

private:
    VTTCue(Document&, double startTime, double endTime, const String& text);

    Document& document() const;

    PassRefPtrWillBeRawPtr<VTTCueBox> getDisplayTree();

    virtual void cueDidChange() override;

    void createVTTNodeTree();
    void copyVTTNodeToDOMTree(ContainerNode* vttNode, ContainerNode* root);

    bool lineIsAuto() const;
    bool textPositionIsAuto() const;

    VTTDisplayParameters calculateDisplayParameters() const;
    float calculateComputedTextPosition() const;
    CueAlignment calculateComputedCueAlignment() const;

    enum CueSetting {
        None,
        Vertical,
        Line,
        Position,
        Size,
        Align,
        RegionId
    };
    CueSetting settingName(VTTScanner&) const;

    String m_text;
    float m_linePosition;
    float m_textPosition;
    float m_cueSize;
    WritingDirection m_writingDirection;
    CueAlignment m_cueAlignment;
    String m_regionId;

    RefPtrWillBeMember<DocumentFragment> m_vttNodeTree;
    RefPtrWillBeMember<HTMLDivElement> m_cueBackgroundBox;
    RefPtrWillBeMember<VTTCueBox> m_displayTree;

    bool m_snapToLines : 1;
    bool m_displayTreeShouldChange : 1;
};

// VTTCue is currently the only TextTrackCue subclass.
DEFINE_TYPE_CASTS(VTTCue, TextTrackCue, cue, true, true);

} // namespace blink

#endif // VTTCue_h

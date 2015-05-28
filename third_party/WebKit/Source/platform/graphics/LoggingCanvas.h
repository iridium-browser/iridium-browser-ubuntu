/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef LoggingCanvas_h
#define LoggingCanvas_h

#include "platform/JSONValues.h"
#include "platform/graphics/InterceptingCanvas.h"

namespace blink {

class LoggingCanvas : public InterceptingCanvasBase {
public:
    LoggingCanvas(int width, int height);
    PassRefPtr<JSONArray> log();

    virtual void onDrawPaint(const SkPaint&) override;
    virtual void onDrawPoints(PointMode, size_t count, const SkPoint pts[], const SkPaint&) override;
    virtual void onDrawRect(const SkRect&, const SkPaint&) override;
    virtual void onDrawOval(const SkRect&, const SkPaint&) override;
    virtual void onDrawRRect(const SkRRect&, const SkPaint&) override;
    virtual void onDrawPath(const SkPath&, const SkPaint&) override;
    virtual void onDrawBitmap(const SkBitmap&, SkScalar left, SkScalar top, const SkPaint*) override;
    virtual void onDrawBitmapRect(const SkBitmap&, const SkRect* src, const SkRect& dst, const SkPaint*, DrawBitmapRectFlags) override;
    virtual void onDrawBitmapNine(const SkBitmap&, const SkIRect& center, const SkRect& dst, const SkPaint*) override;
    virtual void onDrawSprite(const SkBitmap&, int left, int top, const SkPaint*) override;
    virtual void onDrawVertices(VertexMode vmode, int vertexCount, const SkPoint vertices[], const SkPoint texs[],
        const SkColor colors[], SkXfermode* xmode, const uint16_t indices[], int indexCount, const SkPaint&) override;
    virtual void beginCommentGroup(const char* description) override;
    virtual void addComment(const char* keyword, const char* value) override;
    virtual void endCommentGroup() override;

    virtual void onDrawDRRect(const SkRRect& outer, const SkRRect& inner, const SkPaint&) override;
    virtual void onDrawText(const void* text, size_t byteLength, SkScalar x, SkScalar y, const SkPaint&) override;
    virtual void onDrawPosText(const void* text, size_t byteLength, const SkPoint pos[], const SkPaint&) override;
    virtual void onDrawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[], SkScalar constY, const SkPaint&) override;
    virtual void onDrawTextOnPath(const void* text, size_t byteLength, const SkPath&, const SkMatrix*, const SkPaint&) override;
    virtual void onDrawTextBlob(const SkTextBlob*, SkScalar x, SkScalar y, const SkPaint&) override;
    virtual void onClipRect(const SkRect&, SkRegion::Op, ClipEdgeStyle) override;
    virtual void onClipRRect(const SkRRect&, SkRegion::Op, ClipEdgeStyle) override;
    virtual void onClipPath(const SkPath&, SkRegion::Op, ClipEdgeStyle) override;
    virtual void onClipRegion(const SkRegion&, SkRegion::Op) override;
    virtual void onDrawPicture(const SkPicture*, const SkMatrix*, const SkPaint*);
    virtual void didSetMatrix(const SkMatrix&) override;
    virtual void didConcat(const SkMatrix&) override;
    virtual void willSave() override;
    SaveLayerStrategy willSaveLayer(const SkRect* bounds, const SkPaint*, SaveFlags) override;
    virtual void willRestore() override;

private:
    friend class AutoLogger;

    RefPtr<JSONArray> m_log;

    struct VerbParams {
        String name;
        unsigned pointCount;
        unsigned pointOffset;

        VerbParams(const String& name, unsigned pointCount, unsigned pointOffset)
            : name(name)
            , pointCount(pointCount)
            , pointOffset(pointOffset) { }
    };

    PassRefPtr<JSONObject> addItem(const String& name);
    PassRefPtr<JSONObject> addItemWithParams(const String& name);
    PassRefPtr<JSONObject> objectForSkRect(const SkRect&);
    PassRefPtr<JSONObject> objectForSkIRect(const SkIRect&);
    String pointModeName(PointMode);
    PassRefPtr<JSONObject> objectForSkPoint(const SkPoint&);
    PassRefPtr<JSONArray> arrayForSkPoints(size_t count, const SkPoint points[]);
    PassRefPtr<JSONObject> objectForSkPicture(const SkPicture&);
    PassRefPtr<JSONObject> objectForRadius(const SkRRect& rrect, SkRRect::Corner);
    String rrectTypeName(SkRRect::Type);
    String radiusName(SkRRect::Corner);
    PassRefPtr<JSONObject> objectForSkRRect(const SkRRect&);
    String fillTypeName(SkPath::FillType);
    String convexityName(SkPath::Convexity);
    String verbName(SkPath::Verb);
    VerbParams segmentParams(SkPath::Verb);
    PassRefPtr<JSONObject> objectForSkPath(const SkPath&);
    String colorTypeName(SkColorType);
    PassRefPtr<JSONObject> objectForBitmapData(const SkBitmap&);
    PassRefPtr<JSONObject> objectForSkBitmap(const SkBitmap&);
    PassRefPtr<JSONObject> objectForSkShader(const SkShader&);
    String stringForSkColor(const SkColor&);
    void appendFlagToString(String* flagsString, bool isSet, const String& name);
    String stringForSkPaintFlags(const SkPaint&);
    String filterQualityName(SkFilterQuality);
    String textAlignName(SkPaint::Align);
    String strokeCapName(SkPaint::Cap);
    String strokeJoinName(SkPaint::Join);
    String styleName(SkPaint::Style);
    String textEncodingName(SkPaint::TextEncoding);
    String hintingName(SkPaint::Hinting);
    PassRefPtr<JSONObject> objectForSkPaint(const SkPaint&);
    PassRefPtr<JSONArray> arrayForSkMatrix(const SkMatrix&);
    PassRefPtr<JSONArray> arrayForSkScalars(size_t n, const SkScalar scalars[]);
    String regionOpName(SkRegion::Op);
    String saveFlagsToString(SkCanvas::SaveFlags);
    String textEncodingCanonicalName(SkPaint::TextEncoding);
    String stringForUTFText(const void* text, size_t length, SkPaint::TextEncoding);
    String stringForText(const void* text, size_t byteLength, const SkPaint&);
};

} // namespace blink

#endif // LoggingCanvas_h

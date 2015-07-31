/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "Test.h"

#include "SkPictureRecorder.h"
#include "SkRecord.h"
#include "SkRecorder.h"
#include "SkRecords.h"
#include "SkShader.h"
#include "SkSurface.h"

#define COUNT(T) + 1
static const int kRecordTypes = SK_RECORD_TYPES(COUNT);
#undef COUNT

// Tallies the types of commands it sees into a histogram.
class Tally {
public:
    Tally() { sk_bzero(&fHistogram, sizeof(fHistogram)); }

    template <typename T>
    void operator()(const T&) { ++fHistogram[T::kType]; }

    template <typename T>
    int count() const { return fHistogram[T::kType]; }

    void apply(const SkRecord& record) {
        for (unsigned i = 0; i < record.count(); i++) {
            record.visit<void>(i, *this);
        }
    }

private:
    int fHistogram[kRecordTypes];
};

DEF_TEST(Recorder, r) {
    SkRecord record;
    SkRecorder recorder(&record, 1920, 1080);

    recorder.drawRect(SkRect::MakeWH(10, 10), SkPaint());

    Tally tally;
    tally.apply(record);
    REPORTER_ASSERT(r, 1 == tally.count<SkRecords::DrawRect>());
}

// All of Skia will work fine without support for comment groups, but
// Chrome's inspector can break.  This serves as a simple regression test.
DEF_TEST(Recorder_CommentGroups, r) {
    SkRecord record;
    SkRecorder recorder(&record, 1920, 1080);

    recorder.beginCommentGroup("test");
        recorder.addComment("foo", "bar");
        recorder.addComment("baz", "quux");
    recorder.endCommentGroup();

    Tally tally;
    tally.apply(record);

    REPORTER_ASSERT(r, 1 == tally.count<SkRecords::BeginCommentGroup>());
    REPORTER_ASSERT(r, 2 == tally.count<SkRecords::AddComment>());
    REPORTER_ASSERT(r, 1 == tally.count<SkRecords::EndCommentGroup>());
}

// Regression test for leaking refs held by optional arguments.
DEF_TEST(Recorder_RefLeaking, r) {
    // We use SaveLayer to test:
    //   - its SkRect argument is optional and SkRect is POD.  Just testing that that works.
    //   - its SkPaint argument is optional and SkPaint is not POD.  The bug was here.

    SkRect bounds = SkRect::MakeWH(320, 240);
    SkPaint paint;
    paint.setShader(SkShader::CreateEmptyShader())->unref();

    REPORTER_ASSERT(r, paint.getShader()->unique());
    {
        SkRecord record;
        SkRecorder recorder(&record, 1920, 1080);
        recorder.saveLayer(&bounds, &paint);
        REPORTER_ASSERT(r, !paint.getShader()->unique());
    }
    REPORTER_ASSERT(r, paint.getShader()->unique());
}

DEF_TEST(Recorder_drawImage_takeReference, reporter) {

    SkAutoTUnref<SkImage> image;
    {
        SkAutoTUnref<SkSurface> surface(SkSurface::NewRasterN32Premul(100, 100));
        surface->getCanvas()->clear(SK_ColorGREEN);
        image.reset(surface->newImageSnapshot());
    }
    {
        SkRecord record;
        SkRecorder recorder(&record, 100, 100);

        // DrawImage is supposed to take a reference
        recorder.drawImage(image.get(), 0, 0);
        REPORTER_ASSERT(reporter, !image->unique());

        Tally tally;
        tally.apply(record);

        REPORTER_ASSERT(reporter, 1 == tally.count<SkRecords::DrawImage>());
    }
    REPORTER_ASSERT(reporter, image->unique());

    {
        SkRecord record;
        SkRecorder recorder(&record, 100, 100);

        // DrawImageRect is supposed to take a reference
        recorder.drawImageRect(image.get(), 0, SkRect::MakeWH(100, 100));
        REPORTER_ASSERT(reporter, !image->unique());

        Tally tally;
        tally.apply(record);

        REPORTER_ASSERT(reporter, 1 == tally.count<SkRecords::DrawImageRect>());
    }
    REPORTER_ASSERT(reporter, image->unique());
}

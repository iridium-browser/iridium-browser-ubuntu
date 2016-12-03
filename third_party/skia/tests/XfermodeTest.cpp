/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkColor.h"
#include "SkXfermode.h"
#include "Test.h"

#define ILLEGAL_MODE    ((SkXfermode::Mode)-1)

static void test_asMode(skiatest::Reporter* reporter) {
    for (int mode = 0; mode <= SkXfermode::kLastMode; mode++) {
        auto xfer = SkXfermode::Make((SkXfermode::Mode) mode);

        SkXfermode::Mode reportedMode = ILLEGAL_MODE;
        REPORTER_ASSERT(reporter, reportedMode != mode);

        // test IsMode
        REPORTER_ASSERT(reporter, SkXfermode::AsMode(xfer, &reportedMode));
        REPORTER_ASSERT(reporter, reportedMode == mode);

        // repeat that test, but with asMode instead
        if (xfer) {
            reportedMode = (SkXfermode::Mode) -1;
            REPORTER_ASSERT(reporter, xfer->asMode(&reportedMode));
            REPORTER_ASSERT(reporter, reportedMode == mode);
        } else {
            REPORTER_ASSERT(reporter, SkXfermode::kSrcOver_Mode == mode);
        }
    }
}

static void test_IsMode(skiatest::Reporter* reporter) {
    REPORTER_ASSERT(reporter, SkXfermode::IsMode(nullptr,
                                                 SkXfermode::kSrcOver_Mode));

    for (int i = 0; i <= SkXfermode::kLastMode; ++i) {
        SkXfermode::Mode mode = (SkXfermode::Mode)i;

        auto xfer = SkXfermode::Make(mode);
        REPORTER_ASSERT(reporter, SkXfermode::IsMode(xfer, mode));

        if (SkXfermode::kSrcOver_Mode != mode) {
            REPORTER_ASSERT(reporter, !SkXfermode::IsMode(nullptr, mode));
        }
    }
}

DEF_TEST(Xfermode, reporter) {
    test_asMode(reporter);
    test_IsMode(reporter);
}

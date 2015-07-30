// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/html/parser/HTMLPreloadScanner.h"

#include "core/MediaTypeNames.h"
#include "core/css/MediaValuesCached.h"
#include "core/fetch/ClientHintsPreferences.h"
#include "core/frame/Settings.h"
#include "core/html/parser/HTMLParserOptions.h"
#include "core/html/parser/HTMLResourcePreloader.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

namespace blink {

typedef struct {
    const char* baseURL;
    const char* inputHTML;
    const char* preloadedURL;
    const char* outputBaseURL;
    Resource::Type type;
    int resourceWidth;
    ClientHintsPreferences preferences;
} TestCase;

class MockHTMLResourcePreloader : public ResourcePreloader {
public:
    void preloadRequestVerification(Resource::Type type, const String& url, const String& baseURL, int width)
    {
        EXPECT_EQ(m_preloadRequest->resourceType(), type);
        EXPECT_STREQ(m_preloadRequest->resourceURL().ascii().data(),  url.ascii().data());
        EXPECT_STREQ(m_preloadRequest->baseURL().ascii().data(), baseURL.ascii().data());
        EXPECT_EQ(m_preloadRequest->resourceWidth(), width);
    }

protected:
    virtual void preload(PassOwnPtr<PreloadRequest> preloadRequest)
    {
        m_preloadRequest = preloadRequest;
    }

private:

    OwnPtr<PreloadRequest> m_preloadRequest;
};

class HTMLPreloadScannerTest : public testing::Test {
protected:
    HTMLPreloadScannerTest()
        : m_dummyPageHolder(DummyPageHolder::create())
    {
    }

    PassRefPtr<MediaValues> createMediaValues()
    {
        MediaValuesCached::MediaValuesCachedData data;
        data.viewportWidth = 500;
        data.viewportHeight = 600;
        data.deviceWidth = 500;
        data.deviceHeight = 500;
        data.devicePixelRatio = 2.0;
        data.colorBitsPerComponent = 24;
        data.monochromeBitsPerComponent = 0;
        data.primaryPointerType = PointerTypeFine;
        data.defaultFontSize = 16;
        data.threeDEnabled = true;
        data.mediaType = MediaTypeNames::screen;
        data.strictMode = true;
        data.displayMode = WebDisplayModeBrowser;
        return MediaValuesCached::create(data);
    }

    void runSetUp(bool viewportEnabled)
    {
        HTMLParserOptions options(&m_dummyPageHolder->document());
        KURL documentURL(ParsedURLString, "http://whatever.test/");
        m_dummyPageHolder->document().settings()->setViewportEnabled(viewportEnabled);
        m_dummyPageHolder->document().settings()->setViewportMetaEnabled(viewportEnabled);
        m_scanner = HTMLPreloadScanner::create(options, documentURL, CachedDocumentParameters::create(&m_dummyPageHolder->document(), createMediaValues()));
    }

    virtual void SetUp()
    {
        runSetUp(true);
    }

    void test(TestCase testCase)
    {
        MockHTMLResourcePreloader preloader;
        KURL baseURL(ParsedURLString, testCase.baseURL);
        m_scanner->appendToEnd(String(testCase.inputHTML));
        m_scanner->scan(&preloader, baseURL);

        preloader.preloadRequestVerification(testCase.type, testCase.preloadedURL, testCase.outputBaseURL, testCase.resourceWidth);
    }

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;
    OwnPtr<HTMLPreloadScanner> m_scanner;
};

TEST_F(HTMLPreloadScannerTest, testImages)
{
    TestCase testCases[] = {
        {"http://example.test", "<img src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w'>", "bla2.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w'>", "bla3.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' src='bla.gif'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' src='bla.gif' sizes='50vw'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' src='bla.gif'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
    };

    for (auto testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewport)
{
    TestCase testCases[] = {
        {"http://example.test", "<meta name=viewport content='width=160'><img srcset='bla.gif 320w, blabla.gif 640w'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img src='bla.gif' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' sizes='50vw'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img sizes='50vw' srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' src='bla.gif'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' src='bla.gif' sizes='50vw'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
        {"http://example.test", "<img srcset='bla2.gif 160w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' src='bla.gif'>", "bla2.gif", "http://example.test/", Resource::Image, 80},
    };

    for (auto testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testImagesWithViewportDisabled)
{
    runSetUp(false);
    TestCase testCases[] = {
        {"http://example.test", "<meta name=viewport content='width=160'><img src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 1x'>", "bla2.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 0.5x'>", "bla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w'>", "bla2.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w'>", "bla3.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img src='bla.gif' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img src='bla.gif' sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img sizes='50vw' srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' src='bla.gif'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' src='bla.gif' sizes='50vw'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
        {"http://example.test", "<img srcset='bla2.gif 100w, bla3.gif 250w, bla4.gif 500w' sizes='50vw' src='bla.gif'>", "bla4.gif", "http://example.test/", Resource::Image, 250},
    };

    for (auto testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testViewportNoContent)
{
    TestCase testCases[] = {
        {"http://example.test", "<meta name=viewport><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<meta name=viewport content=sdkbsdkjnejjha><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
    };

    for (auto testCase : testCases)
        test(testCase);
}

TEST_F(HTMLPreloadScannerTest, testMetaAcceptCH)
{
    ClientHintsPreferences dpr;
    ClientHintsPreferences rw;
    ClientHintsPreferences dprAndRw;
    dpr.setShouldSendDPR(true);
    dprAndRw.setShouldSendDPR(true);
    rw.setShouldSendRW(true);
    dprAndRw.setShouldSendRW(true);
    TestCase testCases[] = {
        {"http://example.test", "<meta http-equiv='accept-ch' content='bla'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<meta http-equiv='accept-ch' content='dprw'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<meta http-equiv='accept-ch'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0},
        {"http://example.test", "<meta http-equiv='accept-ch' content='dpr \t'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0, dpr},
        {"http://example.test", "<meta http-equiv='accept-ch' content='bla,dpr \t'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0, dpr},
        {"http://example.test", "<meta http-equiv='accept-ch' content='  rw  '><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0, rw},
        {"http://example.test", "<meta http-equiv='accept-ch' content='  rw  , wutever'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0, rw},
        {"http://example.test", "<meta http-equiv='accept-ch' content='  rw  , wutever, dpr \t'><img srcset='bla.gif 320w, blabla.gif 640w'>", "blabla.gif", "http://example.test/", Resource::Image, 0, dprAndRw},
    };

    for (auto testCase : testCases)
        test(testCase);
}

}

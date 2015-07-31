// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_util.h"

namespace extensions {

namespace {

const char kAppUrl[] = "http://www.chromium.org";
const char kAlternativeAppUrl[] = "http://www.notchromium.org";
const char kAppTitle[] = "Test title";
const char kAppShortName[] = "Test short name";
const char kAlternativeAppTitle[] = "Different test title";
const char kAppDescription[] = "Test description";
const char kAppIcon1[] = "fav1.png";
const char kAppIcon2[] = "fav2.png";
const char kAppIcon3[] = "fav3.png";
const char kAppIconURL1[] = "http://foo.com/1.png";
const char kAppIconURL2[] = "http://foo.com/2.png";
const char kAppIconURL3[] = "http://foo.com/3.png";
const char kAppIconURL4[] = "http://foo.com/4.png";

const int kIconSizeTiny = extension_misc::EXTENSION_ICON_BITTY;
const int kIconSizeSmall = extension_misc::EXTENSION_ICON_SMALL;
const int kIconSizeMedium = extension_misc::EXTENSION_ICON_MEDIUM;
const int kIconSizeLarge = extension_misc::EXTENSION_ICON_LARGE;
const int kIconSizeGigantor = extension_misc::EXTENSION_ICON_GIGANTOR;
const int kIconSizeUnsupported = 123;

const int kIconSizeSmallBetweenMediumAndLarge = 63;
const int kIconSizeLargeBetweenMediumAndLarge = 96;

class BookmarkAppHelperTest : public testing::Test {
 public:
  BookmarkAppHelperTest() {}
  ~BookmarkAppHelperTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperTest);
};

class BookmarkAppHelperExtensionServiceTest
    : public extensions::ExtensionServiceTestBase {
 public:
  BookmarkAppHelperExtensionServiceTest() {}
  ~BookmarkAppHelperExtensionServiceTest() override {}

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    service_->Init();
    EXPECT_EQ(0u, registry()->enabled_extensions().size());
  }

  void TearDown() override {
    ExtensionServiceTestBase::TearDown();
    for (content::RenderProcessHost::iterator i(
             content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd();
         i.Advance()) {
      content::RenderProcessHost* host = i.GetCurrentValue();
      if (Profile::FromBrowserContext(host->GetBrowserContext()) ==
          profile_.get())
        host->Cleanup();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkAppHelperExtensionServiceTest);
};

SkBitmap CreateSquareBitmapWithColor(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return bitmap;
}

BookmarkAppHelper::BitmapAndSource CreateSquareBitmapAndSourceWithColor(
    int size,
    SkColor color) {
  return BookmarkAppHelper::BitmapAndSource(
      GURL(), CreateSquareBitmapWithColor(size, color));
}

void ValidateBitmapSizeAndColor(SkBitmap bitmap, int size, SkColor color) {
  // Obtain pixel lock to access pixels.
  SkAutoLockPixels lock(bitmap);
  EXPECT_EQ(color, bitmap.getColor(0, 0));
  EXPECT_EQ(size, bitmap.width());
  EXPECT_EQ(size, bitmap.height());
}

WebApplicationInfo::IconInfo CreateIconInfoWithBitmap(int size, SkColor color) {
  WebApplicationInfo::IconInfo icon_info;
  icon_info.width = size;
  icon_info.height = size;
  icon_info.data = CreateSquareBitmapWithColor(size, color);
  return icon_info;
}

std::set<int> TestSizesToGenerate() {
  const int kIconSizesToGenerate[] = {
      extension_misc::EXTENSION_ICON_SMALL,
      extension_misc::EXTENSION_ICON_MEDIUM,
      extension_misc::EXTENSION_ICON_LARGE,
  };
  return std::set<int>(kIconSizesToGenerate,
                       kIconSizesToGenerate + arraysize(kIconSizesToGenerate));
}

void ValidateAllIconsWithURLsArePresent(const WebApplicationInfo& info_to_check,
                                        const WebApplicationInfo& other_info) {
  for (const auto& icon : info_to_check.icons) {
    if (!icon.url.is_empty()) {
      bool found = false;
      for (const auto& other_icon : info_to_check.icons) {
        if (other_icon.url == icon.url && other_icon.width == icon.width) {
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);
    }
  }
}

std::vector<BookmarkAppHelper::BitmapAndSource>::const_iterator
FindMatchingBitmapAndSourceVector(
    const std::vector<BookmarkAppHelper::BitmapAndSource>& bitmap_vector,
    int size) {
  for (std::vector<BookmarkAppHelper::BitmapAndSource>::const_iterator it =
           bitmap_vector.begin();
       it != bitmap_vector.end(); ++it) {
    if (it->bitmap.width() == size) {
      return it;
    }
  }
  return bitmap_vector.end();
}

std::vector<BookmarkAppHelper::BitmapAndSource>::const_iterator
FindEqualOrLargerBitmapAndSourceVector(
    const std::vector<BookmarkAppHelper::BitmapAndSource>& bitmap_vector,
    int size) {
  for (std::vector<BookmarkAppHelper::BitmapAndSource>::const_iterator it =
           bitmap_vector.begin();
       it != bitmap_vector.end(); ++it) {
    if (it->bitmap.width() >= size) {
      return it;
    }
  }
  return bitmap_vector.end();
}

void ValidateWebApplicationInfo(base::Closure callback,
                                const WebApplicationInfo& original,
                                const WebApplicationInfo& newly_made) {
  EXPECT_EQ(original.title, newly_made.title);
  EXPECT_EQ(original.description, newly_made.description);
  EXPECT_EQ(original.app_url, newly_made.app_url);
  // There should be 6 icons, as there are three sizes which need to be
  // generated, and each will generate a 1x and 2x icon.
  EXPECT_EQ(6u, newly_made.icons.size());
  callback.Run();
}

void ValidateOnlyGenerateIconsWhenNoLargerExists(
    std::vector<BookmarkAppHelper::BitmapAndSource> downloaded,
    std::map<int, BookmarkAppHelper::BitmapAndSource> size_map,
    std::set<int> sizes_to_generate, int expected_generated) {
  GURL empty_url("");
  int number_generated = 0;

  for (const auto& size : sizes_to_generate) {
    auto icon_downloaded = FindMatchingBitmapAndSourceVector(downloaded, size);
    auto icon_larger = FindEqualOrLargerBitmapAndSourceVector(downloaded, size);
    if (icon_downloaded == downloaded.end()) {
      auto icon_resized = size_map.find(size);
      if (icon_larger == downloaded.end()) {
        // There is no larger downloaded icon. Expect an icon to be generated.
        EXPECT_NE(icon_resized, size_map.end());
        EXPECT_EQ(icon_resized->second.bitmap.width(), size);
        EXPECT_EQ(icon_resized->second.bitmap.height(), size);
        EXPECT_EQ(icon_resized->second.bitmap.height(), size);
        EXPECT_EQ(icon_resized->second.source_url, empty_url);
        ++number_generated;
      } else {
        // There is a larger downloaded icon. Expect no icon to be generated.
        // However, an existing icon may be resized down to fit this size.
        // If this is the case, the |source_url| will be non-empty.
        if (icon_resized != size_map.end()) {
          EXPECT_EQ(icon_resized->second.bitmap.width(), size);
          EXPECT_EQ(icon_resized->second.bitmap.height(), size);
          EXPECT_EQ(icon_resized->second.bitmap.height(), size);
          EXPECT_EQ(icon_resized->second.source_url, icon_larger->source_url);
        }
      }
    } else {
      // There is an icon of exactly this size downloaded. Expect no icon to be
      // generated, and the existing downloaded icon to be used.
      auto icon_resized = size_map.find(size);
      EXPECT_NE(icon_resized, size_map.end());
      EXPECT_EQ(icon_resized->second.bitmap.width(), size);
      EXPECT_EQ(icon_resized->second.bitmap.height(), size);
      EXPECT_EQ(icon_downloaded->bitmap.width(), size);
      EXPECT_EQ(icon_downloaded->bitmap.height(), size);
      EXPECT_EQ(icon_resized->second.source_url, icon_downloaded->source_url);
    }
  }
  EXPECT_EQ(number_generated, expected_generated);
}

void TestIconGeneration(int icon_size, int expected_generated) {
  WebApplicationInfo web_app_info;
  std::vector<BookmarkAppHelper::BitmapAndSource> downloaded;

  // Add an icon with a URL and bitmap. 'Download' it.
  WebApplicationInfo::IconInfo icon_info =
      CreateIconInfoWithBitmap(icon_size, SK_ColorRED);
  icon_info.url = GURL(kAppIconURL1);
  web_app_info.icons.push_back(icon_info);
  downloaded.push_back(BookmarkAppHelper::BitmapAndSource(
        icon_info.url, icon_info.data));

  // Now run the resizing and generation.
  WebApplicationInfo new_web_app_info;
  std::map<int, BookmarkAppHelper::BitmapAndSource> size_map =
      BookmarkAppHelper::ResizeIconsAndGenerateMissing(downloaded,
                                                       TestSizesToGenerate(),
                                                       &new_web_app_info);

  // Test that we end up with the expected number of generated icons.
  ValidateOnlyGenerateIconsWhenNoLargerExists(downloaded, size_map,
                                              TestSizesToGenerate(),
                                              expected_generated);
}

}  // namespace

class TestBookmarkAppHelper : public BookmarkAppHelper {
 public:
  TestBookmarkAppHelper(ExtensionService* service,
                        WebApplicationInfo web_app_info,
                        content::WebContents* contents)
      : BookmarkAppHelper(service->profile(), web_app_info, contents) {}

  ~TestBookmarkAppHelper() override {}

  void CreationComplete(const extensions::Extension* extension,
                        const WebApplicationInfo& web_app_info) {
    extension_ = extension;
  }

  void CompleteGetManifest(const content::Manifest& manifest) {
    BookmarkAppHelper::OnDidGetManifest(manifest);
  }

  void CompleteIconDownload(
      bool success,
      const std::map<GURL, std::vector<SkBitmap> >& bitmaps) {
    BookmarkAppHelper::OnIconsDownloaded(success, bitmaps);
  }

  const Extension* extension() { return extension_; }

 private:
  const Extension* extension_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppHelper);
};

TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkApp) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);

  scoped_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile_.get(), NULL));
  TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  std::map<GURL, std::vector<SkBitmap> > icon_map;
  icon_map[GURL(kAppUrl)].push_back(
      CreateSquareBitmapWithColor(kIconSizeSmall, SK_ColorRED));
  helper.CompleteIconDownload(true, icon_map);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_FALSE(
      IconsInfo::GetIconResource(
          extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY).empty());
}

TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkAppWithManifest) {
  WebApplicationInfo web_app_info;

  scoped_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile_.get(), NULL));
  TestBookmarkAppHelper helper(service_, web_app_info, contents.get());
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  content::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);
  helper.CompleteGetManifest(manifest);

  std::map<GURL, std::vector<SkBitmap> > icon_map;
  helper.CompleteIconDownload(true, icon_map);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
}

TEST_F(BookmarkAppHelperExtensionServiceTest, CreateBookmarkAppNoContents) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);
  web_app_info.icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeTiny, SK_ColorRED));

  TestBookmarkAppHelper helper(service_, web_app_info, NULL);
  helper.Create(base::Bind(&TestBookmarkAppHelper::CreationComplete,
                           base::Unretained(&helper)));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper.extension());
  const Extension* extension =
      service_->GetInstalledExtension(helper.extension()->id());
  EXPECT_TRUE(extension);
  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_FALSE(
      IconsInfo::GetIconResource(extension, kIconSizeTiny,
                                 ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(
          extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(extension,
                                 kIconSizeSmall * 2,
                                 ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(
          extension, kIconSizeMedium, ExtensionIconSet::MATCH_EXACTLY).empty());
  EXPECT_FALSE(
      IconsInfo::GetIconResource(extension,
                                 kIconSizeMedium * 2,
                                 ExtensionIconSet::MATCH_EXACTLY).empty());
}

TEST_F(BookmarkAppHelperExtensionServiceTest, CreateAndUpdateBookmarkApp) {
  EXPECT_EQ(0u, registry()->enabled_extensions().size());
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);
  web_app_info.icons.push_back(
      CreateIconInfoWithBitmap(kIconSizeSmall, SK_ColorRED));

  extensions::CreateOrUpdateBookmarkApp(service_, &web_app_info);
  base::RunLoop().RunUntilIdle();

  {
    EXPECT_EQ(1u, registry()->enabled_extensions().size());
    const Extension* extension =
        registry()->enabled_extensions().begin()->get();
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(kAppTitle, extension->name());
    EXPECT_EQ(kAppDescription, extension->description());
    EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
  }

  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.icons[0] = CreateIconInfoWithBitmap(kIconSizeLarge, SK_ColorRED);

  extensions::CreateOrUpdateBookmarkApp(service_, &web_app_info);
  base::RunLoop().RunUntilIdle();

  {
    EXPECT_EQ(1u, registry()->enabled_extensions().size());
    const Extension* extension =
        registry()->enabled_extensions().begin()->get();
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(kAlternativeAppTitle, extension->name());
    EXPECT_EQ(kAppDescription, extension->description());
    EXPECT_EQ(GURL(kAppUrl), AppLaunchInfo::GetLaunchWebURL(extension));
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeLarge, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
  }
}

TEST_F(BookmarkAppHelperExtensionServiceTest, GetWebApplicationInfo) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kAppUrl);
  web_app_info.title = base::UTF8ToUTF16(kAppTitle);
  web_app_info.description = base::UTF8ToUTF16(kAppDescription);

  extensions::CreateOrUpdateBookmarkApp(service_, &web_app_info);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  base::RunLoop run_loop;
  extensions::GetWebApplicationInfoFromApp(
      profile_.get(), registry()->enabled_extensions().begin()->get(),
      base::Bind(&ValidateWebApplicationInfo, run_loop.QuitClosure(),
                 web_app_info));
  run_loop.Run();
}

TEST_F(BookmarkAppHelperExtensionServiceTest, LinkedAppIconsAreNotChanged) {
  WebApplicationInfo web_app_info;

  // Add two icons with a URL and bitmap, two icons with just a bitmap, an
  // icon with just URL and an icon in an unsupported size with just a URL.
  WebApplicationInfo::IconInfo icon_info =
      CreateIconInfoWithBitmap(kIconSizeSmall, SK_ColorRED);
  icon_info.url = GURL(kAppIconURL1);
  web_app_info.icons.push_back(icon_info);

  icon_info = CreateIconInfoWithBitmap(kIconSizeMedium, SK_ColorRED);
  icon_info.url = GURL(kAppIconURL2);
  web_app_info.icons.push_back(icon_info);

  icon_info.data = SkBitmap();
  icon_info.url = GURL(kAppIconURL3);
  icon_info.width = 0;
  icon_info.height = 0;
  web_app_info.icons.push_back(icon_info);

  icon_info.url = GURL(kAppIconURL4);
  web_app_info.icons.push_back(icon_info);

  icon_info = CreateIconInfoWithBitmap(kIconSizeLarge, SK_ColorRED);
  web_app_info.icons.push_back(icon_info);

  icon_info = CreateIconInfoWithBitmap(kIconSizeUnsupported, SK_ColorRED);
  web_app_info.icons.push_back(icon_info);

  // 'Download' one of the icons without a size or bitmap.
  std::vector<BookmarkAppHelper::BitmapAndSource> downloaded;
  downloaded.push_back(BookmarkAppHelper::BitmapAndSource(
      GURL(kAppIconURL3),
      CreateSquareBitmapWithColor(kIconSizeLarge, SK_ColorBLACK)));

  // Now run the resizing and generation into a new web app info.
  WebApplicationInfo new_web_app_info;
  std::map<int, BookmarkAppHelper::BitmapAndSource> size_map =
      BookmarkAppHelper::ResizeIconsAndGenerateMissing(downloaded,
                                                       TestSizesToGenerate(),
                                                       &new_web_app_info);

  // Now check that the linked app icons (i.e. those with URLs) are matching in
  // both lists.
  ValidateAllIconsWithURLsArePresent(web_app_info, new_web_app_info);
  ValidateAllIconsWithURLsArePresent(new_web_app_info, web_app_info);
}

TEST_F(BookmarkAppHelperTest, UpdateWebAppInfoFromManifest) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.app_url = GURL(kAlternativeAppUrl);
  WebApplicationInfo::IconInfo info;
  info.url = GURL(kAppIcon1);
  web_app_info.icons.push_back(info);

  content::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.short_name = base::NullableString16(base::UTF8ToUTF16(kAppShortName),
                                               false);

  BookmarkAppHelper::UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppShortName), web_app_info.title);
  EXPECT_EQ(GURL(kAppUrl), web_app_info.app_url);

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.icons.size());
  EXPECT_EQ(GURL(kAppIcon1), web_app_info.icons[0].url);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);

  content::Manifest::Icon icon;
  icon.src = GURL(kAppIcon2);
  manifest.icons.push_back(icon);
  icon.src = GURL(kAppIcon3);
  manifest.icons.push_back(icon);

  BookmarkAppHelper::UpdateWebAppInfoFromManifest(manifest, &web_app_info);
  EXPECT_EQ(base::UTF8ToUTF16(kAppTitle), web_app_info.title);

  EXPECT_EQ(2u, web_app_info.icons.size());
  EXPECT_EQ(GURL(kAppIcon2), web_app_info.icons[0].url);
  EXPECT_EQ(GURL(kAppIcon3), web_app_info.icons[1].url);
}

TEST_F(BookmarkAppHelperTest, ConstrainBitmapsToSizes) {
  std::set<int> desired_sizes;
  desired_sizes.insert(16);
  desired_sizes.insert(32);
  desired_sizes.insert(128);
  desired_sizes.insert(256);

  {
    std::vector<BookmarkAppHelper::BitmapAndSource> bitmaps;
    bitmaps.push_back(CreateSquareBitmapAndSourceWithColor(16, SK_ColorRED));
    bitmaps.push_back(CreateSquareBitmapAndSourceWithColor(32, SK_ColorGREEN));
    bitmaps.push_back(CreateSquareBitmapAndSourceWithColor(48, SK_ColorBLUE));
    bitmaps.push_back(
        CreateSquareBitmapAndSourceWithColor(144, SK_ColorYELLOW));

    std::map<int, BookmarkAppHelper::BitmapAndSource> results(
        BookmarkAppHelper::ConstrainBitmapsToSizes(bitmaps, desired_sizes));

    EXPECT_EQ(3u, results.size());
    ValidateBitmapSizeAndColor(results[16].bitmap, 16, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[32].bitmap, 32, SK_ColorGREEN);
    ValidateBitmapSizeAndColor(results[128].bitmap, 128, SK_ColorYELLOW);
  }
  {
    std::vector<BookmarkAppHelper::BitmapAndSource> bitmaps;
    bitmaps.push_back(CreateSquareBitmapAndSourceWithColor(512, SK_ColorRED));
    bitmaps.push_back(CreateSquareBitmapAndSourceWithColor(18, SK_ColorGREEN));
    bitmaps.push_back(CreateSquareBitmapAndSourceWithColor(33, SK_ColorBLUE));
    bitmaps.push_back(CreateSquareBitmapAndSourceWithColor(17, SK_ColorYELLOW));

    std::map<int, BookmarkAppHelper::BitmapAndSource> results(
        BookmarkAppHelper::ConstrainBitmapsToSizes(bitmaps, desired_sizes));

    EXPECT_EQ(3u, results.size());
    ValidateBitmapSizeAndColor(results[16].bitmap, 16, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[32].bitmap, 32, SK_ColorBLUE);
    ValidateBitmapSizeAndColor(results[256].bitmap, 256, SK_ColorRED);
  }
}

TEST_F(BookmarkAppHelperTest, IsValidBookmarkAppUrl) {
  EXPECT_TRUE(IsValidBookmarkAppUrl(GURL("https://www.chromium.org")));
  EXPECT_TRUE(IsValidBookmarkAppUrl(GURL("http://www.chromium.org/path")));
  EXPECT_FALSE(IsValidBookmarkAppUrl(GURL("ftp://www.chromium.org")));
  EXPECT_FALSE(IsValidBookmarkAppUrl(GURL("chrome://flags")));
}

TEST_F(BookmarkAppHelperTest, IconsGeneratedOnlyWhenNoneLarger) {
  WebApplicationInfo web_app_info;
  std::vector<BookmarkAppHelper::BitmapAndSource> downloaded;

  // Add three icons with a URL and bitmap. 'Download' each of them.
  WebApplicationInfo::IconInfo icon_info =
      CreateIconInfoWithBitmap(kIconSizeSmall, SK_ColorRED);
  icon_info.url = GURL(kAppIconURL1);
  web_app_info.icons.push_back(icon_info);
  downloaded.push_back(BookmarkAppHelper::BitmapAndSource(
        icon_info.url, icon_info.data));

  icon_info = CreateIconInfoWithBitmap(kIconSizeSmallBetweenMediumAndLarge,
                                       SK_ColorRED);
  icon_info.url = GURL(kAppIconURL2);
  web_app_info.icons.push_back(icon_info);
  downloaded.push_back(BookmarkAppHelper::BitmapAndSource(
        icon_info.url, icon_info.data));

  icon_info = CreateIconInfoWithBitmap(kIconSizeLargeBetweenMediumAndLarge,
                                       SK_ColorRED);
  icon_info.url = GURL(kAppIconURL3);
  web_app_info.icons.push_back(icon_info);
  downloaded.push_back(BookmarkAppHelper::BitmapAndSource(
        icon_info.url, icon_info.data));

  // Now run the resizing and generation.
  WebApplicationInfo new_web_app_info;
  std::map<int, BookmarkAppHelper::BitmapAndSource> size_map =
      BookmarkAppHelper::ResizeIconsAndGenerateMissing(downloaded,
                                                       TestSizesToGenerate(),
                                                       &new_web_app_info);

  // Test that icons are only generated when necessary. The largest icon
  // downloaded is smaller than EXTENSION_ICON_LARGE, so one icon should be
  // generated.
  ValidateOnlyGenerateIconsWhenNoLargerExists(downloaded, size_map,
                                              TestSizesToGenerate(), 1);
}

TEST_F(BookmarkAppHelperTest, AllIconsGeneratedWhenOnlyASmallOneIsProvided) {
  // When only a tiny icon is downloaded (smaller than the three desired
  // sizes), 3 icons should be generated.
  TestIconGeneration(kIconSizeTiny, 3);
}

TEST_F(BookmarkAppHelperTest, NoIconsGeneratedWhenAVeryLargeOneIsProvided) {
  // When an enormous icon is provided, each desired icon size should fall back
  // to it, and no icons should be generated.
  TestIconGeneration(kIconSizeGigantor, 0);
}

}  // namespace extensions

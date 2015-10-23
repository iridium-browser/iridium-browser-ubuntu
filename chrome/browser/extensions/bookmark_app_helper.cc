// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_helper.h"

#include <cctype>

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/favicon_downloader.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/url_pattern.h"
#include "grit/platform_locale_settings.h"
#include "net/base/load_flags.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/url_request/url_request.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"

#if defined(OS_MACOSX)
#include "base/command_line.h"
#include "chrome/browser/web_applications/web_app_mac.h"
#include "chrome/common/chrome_switches.h"
#endif

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#endif

namespace {

using extensions::BookmarkAppHelper;

// Overlays a shortcut icon over the bottom left corner of a given image.
class GeneratedIconImageSource : public gfx::CanvasImageSource {
 public:
  explicit GeneratedIconImageSource(char letter, SkColor color, int output_size)
      : gfx::CanvasImageSource(gfx::Size(output_size, output_size), false),
        letter_(letter),
        color_(color),
        output_size_(output_size) {}
  ~GeneratedIconImageSource() override {}

 private:
  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override {
    const unsigned char kLuminanceThreshold = 190;
    const int icon_size = output_size_ * 3 / 4;
    const int icon_inset = output_size_ / 8;
    const size_t border_radius = output_size_ / 16;
    const size_t font_size = output_size_ * 7 / 16;

    std::string font_name =
        l10n_util::GetStringUTF8(IDS_SANS_SERIF_FONT_FAMILY);
#if defined(OS_CHROMEOS)
    const std::string kChromeOSFontFamily = "Noto Sans";
    font_name = kChromeOSFontFamily;
#endif

    // Draw a rounded rect of the given |color|.
    SkPaint background_paint;
    background_paint.setFlags(SkPaint::kAntiAlias_Flag);
    background_paint.setColor(color_);

    gfx::Rect icon_rect(icon_inset, icon_inset, icon_size, icon_size);
    canvas->DrawRoundRect(icon_rect, border_radius, background_paint);

    // The text rect's size needs to be odd to center the text correctly.
    gfx::Rect text_rect(icon_inset, icon_inset, icon_size + 1, icon_size + 1);
    // Draw the letter onto the rounded rect. The letter's color depends on the
    // luminance of |color|.
    unsigned char luminance = color_utils::GetLuminanceForColor(color_);
    canvas->DrawStringRectWithFlags(
        base::string16(1, std::toupper(letter_)),
        gfx::FontList(gfx::Font(font_name, font_size)),
        luminance > kLuminanceThreshold ? SK_ColorBLACK : SK_ColorWHITE,
        text_rect,
        gfx::Canvas::TEXT_ALIGN_CENTER);
  }

  char letter_;

  SkColor color_;

  int output_size_;

  DISALLOW_COPY_AND_ASSIGN(GeneratedIconImageSource);
};

void OnIconsLoaded(
    WebApplicationInfo web_app_info,
    const base::Callback<void(const WebApplicationInfo&)> callback,
    const gfx::ImageFamily& image_family) {
  for (gfx::ImageFamily::const_iterator it = image_family.begin();
       it != image_family.end();
       ++it) {
    WebApplicationInfo::IconInfo icon_info;
    icon_info.data = *it->ToSkBitmap();
    icon_info.width = icon_info.data.width();
    icon_info.height = icon_info.data.height();
    web_app_info.icons.push_back(icon_info);
  }
  callback.Run(web_app_info);
}

std::set<int> SizesToGenerate() {
  // Generate container icons from smaller icons.
  const int kIconSizesToGenerate[] = {
      extension_misc::EXTENSION_ICON_SMALL,
      extension_misc::EXTENSION_ICON_MEDIUM,
      extension_misc::EXTENSION_ICON_LARGE,
  };
  return std::set<int>(kIconSizesToGenerate,
                       kIconSizesToGenerate + arraysize(kIconSizesToGenerate));
}

void GenerateIcons(
    std::set<int> generate_sizes,
    const GURL& app_url,
    SkColor generated_icon_color,
    std::map<int, BookmarkAppHelper::BitmapAndSource>* bitmap_map) {
  // The letter that will be painted on the generated icon.
  char icon_letter = ' ';
  std::string domain_and_registry(
      net::registry_controlled_domains::GetDomainAndRegistry(
          app_url,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  if (!domain_and_registry.empty()) {
    icon_letter = domain_and_registry[0];
  } else if (!app_url.host().empty()) {
    icon_letter = app_url.host()[0];
  }

  // If no color has been specified, use a dark gray so it will stand out on the
  // black shelf.
  if (generated_icon_color == SK_ColorTRANSPARENT)
    generated_icon_color = SK_ColorDKGRAY;

  for (std::set<int>::const_iterator it = generate_sizes.begin();
       it != generate_sizes.end(); ++it) {
    extensions::BookmarkAppHelper::GenerateIcon(
        bitmap_map, *it, generated_icon_color, icon_letter);
    // Also generate the 2x resource for this size.
    extensions::BookmarkAppHelper::GenerateIcon(
        bitmap_map, *it * 2, generated_icon_color, icon_letter);
  }
}

void ReplaceWebAppIcons(
    std::map<int, BookmarkAppHelper::BitmapAndSource> bitmap_map,
    WebApplicationInfo* web_app_info) {
  web_app_info->icons.clear();

  // Populate the icon data into the WebApplicationInfo we are using to
  // install the bookmark app.
  for (const auto& pair : bitmap_map) {
    WebApplicationInfo::IconInfo icon_info;
    icon_info.data = pair.second.bitmap;
    icon_info.url = pair.second.source_url;
    icon_info.width = icon_info.data.width();
    icon_info.height = icon_info.data.height();
    web_app_info->icons.push_back(icon_info);
  }
}

// Class to handle installing a bookmark app. Handles downloading and decoding
// the icons.
class BookmarkAppInstaller : public base::RefCounted<BookmarkAppInstaller>,
                             public chrome::BitmapFetcherDelegate {
 public:
  BookmarkAppInstaller(ExtensionService* service,
                       const WebApplicationInfo& web_app_info)
      : service_(service), web_app_info_(web_app_info) {}

  void Run() {
    for (const auto& icon : web_app_info_.icons) {
      if (icon.url.is_valid())
        urls_to_download_.push_back(icon.url);
    }

    if (urls_to_download_.size()) {
      DownloadNextImage();

      // Matched in OnFetchComplete.
      AddRef();
      return;
    }

    FinishInstallation();
  }

 private:
  friend class base::RefCounted<BookmarkAppInstaller>;
  ~BookmarkAppInstaller() override {}

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override {
    if (bitmap && !bitmap->empty() && bitmap->width() == bitmap->height()) {
      downloaded_bitmaps_.push_back(
          BookmarkAppHelper::BitmapAndSource(url, *bitmap));
    }

    if (urls_to_download_.size()) {
      DownloadNextImage();
      return;
    }

    FinishInstallation();
    Release();
  }

  void DownloadNextImage() {
    DCHECK(urls_to_download_.size());

    bitmap_fetcher_.reset(
        new chrome::BitmapFetcher(urls_to_download_.back(), this));
    urls_to_download_.pop_back();
    bitmap_fetcher_->Init(
        service_->profile()->GetRequestContext(), std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES);
    bitmap_fetcher_->Start();
  }

  void FinishInstallation() {
    std::map<int, BookmarkAppHelper::BitmapAndSource> size_map =
        BookmarkAppHelper::ResizeIconsAndGenerateMissing(downloaded_bitmaps_,
                                                         SizesToGenerate(),
                                                         &web_app_info_);
    BookmarkAppHelper::UpdateWebAppIconsWithoutChangingLinks(size_map,
                                                             &web_app_info_);
    scoped_refptr<extensions::CrxInstaller> installer(
        extensions::CrxInstaller::CreateSilent(service_));
    installer->set_error_on_unsupported_requirements(true);
    installer->InstallWebApp(web_app_info_);
  }

  ExtensionService* service_;
  WebApplicationInfo web_app_info_;

  scoped_ptr<chrome::BitmapFetcher> bitmap_fetcher_;
  std::vector<GURL> urls_to_download_;
  std::vector<BookmarkAppHelper::BitmapAndSource> downloaded_bitmaps_;
};

}  // namespace

namespace extensions {

// static
void BookmarkAppHelper::UpdateWebAppInfoFromManifest(
    const content::Manifest& manifest,
    WebApplicationInfo* web_app_info) {
  if (!manifest.short_name.is_null())
    web_app_info->title = manifest.short_name.string();

  // Give the full length name priority.
  if (!manifest.name.is_null())
    web_app_info->title = manifest.name.string();

  // Set the url based on the manifest value, if any.
  if (manifest.start_url.is_valid())
    web_app_info->app_url = manifest.start_url;

  // If any icons are specified in the manifest, they take precedence over any
  // we picked up from the web_app stuff.
  if (!manifest.icons.empty()) {
    web_app_info->icons.clear();
    for (const auto& icon : manifest.icons) {
      // TODO(benwells): Take the declared icon density and sizes into account.
      WebApplicationInfo::IconInfo info;
      info.url = icon.src;
      web_app_info->icons.push_back(info);
    }
  }
}

// static
std::map<int, BookmarkAppHelper::BitmapAndSource>
BookmarkAppHelper::ConstrainBitmapsToSizes(
    const std::vector<BookmarkAppHelper::BitmapAndSource>& bitmaps,
    const std::set<int>& sizes) {
  std::map<int, BitmapAndSource> output_bitmaps;
  std::map<int, BitmapAndSource> ordered_bitmaps;
  for (std::vector<BitmapAndSource>::const_iterator it = bitmaps.begin();
       it != bitmaps.end(); ++it) {
    DCHECK(it->bitmap.width() == it->bitmap.height());
    ordered_bitmaps[it->bitmap.width()] = *it;
  }

  std::set<int>::const_iterator sizes_it = sizes.begin();
  std::map<int, BitmapAndSource>::const_iterator bitmaps_it =
      ordered_bitmaps.begin();
  while (sizes_it != sizes.end() && bitmaps_it != ordered_bitmaps.end()) {
    int size = *sizes_it;
    // Find the closest not-smaller bitmap.
    bitmaps_it = ordered_bitmaps.lower_bound(size);
    ++sizes_it;
    // Ensure the bitmap is valid and smaller than the next allowed size.
    if (bitmaps_it != ordered_bitmaps.end() &&
        (sizes_it == sizes.end() ||
         bitmaps_it->second.bitmap.width() < *sizes_it)) {
      output_bitmaps[size] = bitmaps_it->second;
      // Resize the bitmap if it does not exactly match the desired size.
      if (output_bitmaps[size].bitmap.width() != size) {
        output_bitmaps[size].bitmap = skia::ImageOperations::Resize(
            output_bitmaps[size].bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
            size, size);
      }
    }
  }
  return output_bitmaps;
}

// static
void BookmarkAppHelper::GenerateIcon(
    std::map<int, BookmarkAppHelper::BitmapAndSource>* bitmaps,
    int output_size,
    SkColor color,
    char letter) {
  // Do nothing if there is already an icon of |output_size|.
  if (bitmaps->count(output_size))
    return;

  gfx::ImageSkia icon_image(
      new GeneratedIconImageSource(letter, color, output_size),
      gfx::Size(output_size, output_size));
  icon_image.bitmap()->deepCopyTo(&(*bitmaps)[output_size].bitmap);
}

// static
bool BookmarkAppHelper::BookmarkOrHostedAppInstalled(
    content::BrowserContext* browser_context,
    const GURL& url) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  const ExtensionSet& extensions = registry->enabled_extensions();

  // Iterate through the extensions and extract the LaunchWebUrl (bookmark apps)
  // or check the web extent (hosted apps).
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    const Extension* extension = iter->get();
    if (!extension->is_hosted_app())
      continue;

    if (extension->web_extent().MatchesURL(url) ||
        AppLaunchInfo::GetLaunchWebURL(extension) == url) {
      return true;
    }
  }
  return false;
}

// static
std::map<int, BookmarkAppHelper::BitmapAndSource>
BookmarkAppHelper::ResizeIconsAndGenerateMissing(
    std::vector<BookmarkAppHelper::BitmapAndSource> icons,
    std::set<int> sizes_to_generate,
    WebApplicationInfo* web_app_info) {
  // Add the downloaded icons. Extensions only allow certain icon sizes. First
  // populate icons that match the allowed sizes exactly and then downscale
  // remaining icons to the closest allowed size that doesn't yet have an icon.
  std::set<int> allowed_sizes(extension_misc::kExtensionIconSizes,
                              extension_misc::kExtensionIconSizes +
                                  extension_misc::kNumExtensionIconSizes);

  // If there are icons that don't match the accepted icon sizes, find the
  // closest bigger icon to the accepted sizes and resize the icon to it. An
  // icon will be resized and used for at most one size.
  std::map<int, BitmapAndSource> resized_bitmaps(
      ConstrainBitmapsToSizes(icons, allowed_sizes));

  // Determine the color that will be used for the icon's background. For this
  // the dominant color of the first icon found is used.
  if (resized_bitmaps.size()) {
    color_utils::GridSampler sampler;
    web_app_info->generated_icon_color =
        color_utils::CalculateKMeanColorOfBitmap(
            resized_bitmaps.begin()->second.bitmap);
  }

  // Work out what icons we need to generate here. Icons are only generated if:
  // a. there is no icon in the required size, AND
  // b. there is no icon LARGER than the required size.
  // Larger icons will be scaled down and used at display time.
  std::set<int> generate_sizes;
  for (int size : sizes_to_generate) {
    if (resized_bitmaps.lower_bound(size) == resized_bitmaps.end())
      generate_sizes.insert(size);
  }
  GenerateIcons(generate_sizes, web_app_info->app_url,
                web_app_info->generated_icon_color, &resized_bitmaps);

  return resized_bitmaps;
}

// static
void BookmarkAppHelper::UpdateWebAppIconsWithoutChangingLinks(
    std::map<int, BookmarkAppHelper::BitmapAndSource> bitmap_map,
    WebApplicationInfo* web_app_info) {
  // First add in the icon data that have urls with the url / size data from the
  // original web app info, and the data from the new icons (if any).
  for (auto& icon : web_app_info->icons) {
    if (!icon.url.is_empty() && icon.data.empty()) {
      const auto& it = bitmap_map.find(icon.width);
      if (it != bitmap_map.end() && it->second.source_url == icon.url)
        icon.data = it->second.bitmap;
    }
  }

  // Now add in any icons from the updated list that don't have URLs.
  for (const auto& pair : bitmap_map) {
    if (pair.second.source_url.is_empty()) {
      WebApplicationInfo::IconInfo icon_info;
      icon_info.data = pair.second.bitmap;
      icon_info.width = pair.first;
      icon_info.height = pair.first;
      web_app_info->icons.push_back(icon_info);
    }
  }
}

BookmarkAppHelper::BitmapAndSource::BitmapAndSource() {
}

BookmarkAppHelper::BitmapAndSource::BitmapAndSource(const GURL& source_url_p,
                                                    const SkBitmap& bitmap_p)
    : source_url(source_url_p),
      bitmap(bitmap_p) {
}

BookmarkAppHelper::BitmapAndSource::~BitmapAndSource() {
}

BookmarkAppHelper::BookmarkAppHelper(Profile* profile,
                                     WebApplicationInfo web_app_info,
                                     content::WebContents* contents)
    : profile_(profile),
      contents_(contents),
      web_app_info_(web_app_info),
      crx_installer_(extensions::CrxInstaller::CreateSilent(
          ExtensionSystem::Get(profile)->extension_service())) {
  web_app_info_.open_as_window =
      profile_->GetPrefs()->GetInteger(
          extensions::pref_names::kBookmarkAppCreationLaunchType) ==
      extensions::LAUNCH_TYPE_WINDOW;

  // The default app title is the page title, which can be quite long. Limit the
  // default name used to something sensible.
  const int kMaxDefaultTitle = 40;
  if (web_app_info_.title.length() > kMaxDefaultTitle) {
    web_app_info_.title = web_app_info_.title.substr(0, kMaxDefaultTitle - 3) +
                          base::UTF8ToUTF16("...");
  }

  registrar_.Add(this,
                 extensions::NOTIFICATION_CRX_INSTALLER_DONE,
                 content::Source<CrxInstaller>(crx_installer_.get()));

  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                 content::Source<CrxInstaller>(crx_installer_.get()));

  crx_installer_->set_error_on_unsupported_requirements(true);
}

BookmarkAppHelper::~BookmarkAppHelper() {}

void BookmarkAppHelper::Create(const CreateBookmarkAppCallback& callback) {
  callback_ = callback;

  // Do not fetch the manifest for extension URLs.
  if (contents_ &&
      !contents_->GetVisibleURL().SchemeIs(extensions::kExtensionScheme)) {
    contents_->GetManifest(base::Bind(&BookmarkAppHelper::OnDidGetManifest,
                                     base::Unretained(this)));
  } else {
    OnIconsDownloaded(true, std::map<GURL, std::vector<SkBitmap> >());
  }
}

void BookmarkAppHelper::CreateFromAppBanner(
    const CreateBookmarkAppCallback& callback,
    const content::Manifest& manifest) {
  DCHECK(!manifest.short_name.is_null() || !manifest.name.is_null());
  DCHECK(manifest.start_url.is_valid());

  callback_ = callback;
  OnDidGetManifest(manifest);
}

void BookmarkAppHelper::OnDidGetManifest(const content::Manifest& manifest) {
  if (contents_->IsBeingDestroyed())
    return;

  UpdateWebAppInfoFromManifest(manifest, &web_app_info_);

  // Add urls from the WebApplicationInfo.
  std::vector<GURL> web_app_info_icon_urls;
  for (std::vector<WebApplicationInfo::IconInfo>::const_iterator it =
           web_app_info_.icons.begin();
       it != web_app_info_.icons.end();
       ++it) {
    if (it->url.is_valid())
      web_app_info_icon_urls.push_back(it->url);
  }

  favicon_downloader_.reset(
      new FaviconDownloader(contents_,
                            web_app_info_icon_urls,
                            base::Bind(&BookmarkAppHelper::OnIconsDownloaded,
                                       base::Unretained(this))));
  favicon_downloader_->Start();
}

void BookmarkAppHelper::OnIconsDownloaded(
    bool success,
    const std::map<GURL, std::vector<SkBitmap> >& bitmaps) {
  // The tab has navigated away during the icon download. Cancel the bookmark
  // app creation.
  if (!success) {
    favicon_downloader_.reset();
    callback_.Run(nullptr, web_app_info_);
    return;
  }

  std::vector<BitmapAndSource> downloaded_icons;
  for (FaviconDownloader::FaviconMap::const_iterator map_it = bitmaps.begin();
       map_it != bitmaps.end();
       ++map_it) {
    for (std::vector<SkBitmap>::const_iterator bitmap_it =
             map_it->second.begin();
         bitmap_it != map_it->second.end();
         ++bitmap_it) {
      if (bitmap_it->empty() || bitmap_it->width() != bitmap_it->height())
        continue;

      downloaded_icons.push_back(BitmapAndSource(map_it->first, *bitmap_it));
    }
  }

  // Add all existing icons from WebApplicationInfo.
  for (std::vector<WebApplicationInfo::IconInfo>::const_iterator it =
           web_app_info_.icons.begin();
       it != web_app_info_.icons.end();
       ++it) {
    const SkBitmap& icon = it->data;
    if (!icon.drawsNothing() && icon.width() == icon.height()) {
      downloaded_icons.push_back(BitmapAndSource(it->url, icon));
    }
  }

  web_app_info_.generated_icon_color = SK_ColorTRANSPARENT;
  std::map<int, BitmapAndSource> size_to_icons =
      ResizeIconsAndGenerateMissing(downloaded_icons, SizesToGenerate(),
                                    &web_app_info_);
  ReplaceWebAppIcons(size_to_icons, &web_app_info_);
  favicon_downloader_.reset();

  if (!contents_) {
    // The web contents can be null in tests.
    OnBubbleCompleted(true, web_app_info_);
    return;
  }

  Browser* browser = chrome::FindBrowserWithWebContents(contents_);
  if (!browser) {
    // The browser can be null in tests.
    OnBubbleCompleted(true, web_app_info_);
    return;
  }
  browser->window()->ShowBookmarkAppBubble(
      web_app_info_, base::Bind(&BookmarkAppHelper::OnBubbleCompleted,
                                base::Unretained(this)));
}

void BookmarkAppHelper::OnBubbleCompleted(
    bool user_accepted,
    const WebApplicationInfo& web_app_info) {
  if (user_accepted) {
    web_app_info_ = web_app_info;
    crx_installer_->InstallWebApp(web_app_info_);
  } else {
    callback_.Run(nullptr, web_app_info_);
  }
}

void BookmarkAppHelper::FinishInstallation(const Extension* extension) {
  // Set the default 'open as' preference for use next time the dialog is
  // shown.
  extensions::LaunchType launch_type = web_app_info_.open_as_window
                                           ? extensions::LAUNCH_TYPE_WINDOW
                                           : extensions::LAUNCH_TYPE_REGULAR;
  profile_->GetPrefs()->SetInteger(
      extensions::pref_names::kBookmarkAppCreationLaunchType, launch_type);

  // Set the launcher type for the app.
  extensions::SetLaunchType(profile_, extension->id(), launch_type);

  if (!contents_) {
    // The web contents can be null in tests.
    callback_.Run(extension, web_app_info_);
    return;
  }

  Browser* browser = chrome::FindBrowserWithWebContents(contents_);
  if (!browser) {
    // The browser can be null in tests.
    callback_.Run(extension, web_app_info_);
    return;
  }

  // Pin the app to the relevant launcher depending on the OS.
  Profile* current_profile = profile_->GetOriginalProfile();

// On Mac, shortcuts are automatically created for hosted apps when they are
// installed, so there is no need to create them again.
#if !defined(OS_MACOSX)
  chrome::HostDesktopType desktop = browser->host_desktop_type();
  if (desktop != chrome::HOST_DESKTOP_TYPE_ASH) {
    web_app::ShortcutLocations creation_locations;
#if defined(OS_LINUX)
    creation_locations.on_desktop = true;
#else
    creation_locations.on_desktop = false;
#endif
    creation_locations.applications_menu_location =
        web_app::APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;
    web_app::CreateShortcuts(web_app::SHORTCUT_CREATION_BY_USER,
                             creation_locations, current_profile, extension);
    // Creating shortcuts in the start menu fails when the language is set
    // to certain languages (e.g. Hindi). To work around this, the taskbar /
    // quick launch icon is created separately to ensure it doesn't fail
    // due to the start menu shortcut creation failing.
    // See http://crbug.com/477297 and http://crbug.com/484577.
    creation_locations.on_desktop = false;
    creation_locations.applications_menu_location =
        web_app::APP_MENU_LOCATION_NONE;
    creation_locations.in_quick_launch_bar = true;
    web_app::CreateShortcuts(web_app::SHORTCUT_CREATION_BY_USER,
                             creation_locations, current_profile, extension);
#if defined(USE_ASH)
  } else {
    ChromeLauncherController::instance()->PinAppWithID(extension->id());
#endif
  }
#endif

#if defined(OS_MACOSX)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kDisableHostedAppShimCreation)) {
    web_app::RevealAppShimInFinderForApp(current_profile, extension);
  }
#endif

  callback_.Run(extension, web_app_info_);
}

void BookmarkAppHelper::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_CRX_INSTALLER_DONE: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      DCHECK(extension);
      DCHECK_EQ(AppLaunchInfo::GetLaunchWebURL(extension),
                web_app_info_.app_url);
      FinishInstallation(extension);
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_INSTALL_ERROR:
      callback_.Run(nullptr, web_app_info_);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void CreateOrUpdateBookmarkApp(ExtensionService* service,
                               WebApplicationInfo* web_app_info) {
  scoped_refptr<BookmarkAppInstaller> installer(
      new BookmarkAppInstaller(service, *web_app_info));
  installer->Run();
}

void GetWebApplicationInfoFromApp(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    const base::Callback<void(const WebApplicationInfo&)> callback) {
  if (!extension->from_bookmark()) {
    callback.Run(WebApplicationInfo());
    return;
  }

  WebApplicationInfo web_app_info;
  web_app_info.app_url = AppLaunchInfo::GetLaunchWebURL(extension);
  web_app_info.title = base::UTF8ToUTF16(extension->non_localized_name());
  web_app_info.description = base::UTF8ToUTF16(extension->description());

  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;
  for (size_t i = 0; i < extension_misc::kNumExtensionIconSizes; ++i) {
    int size = extension_misc::kExtensionIconSizes[i];
    extensions::ExtensionResource resource =
        extensions::IconsInfo::GetIconResource(
            extension, size, ExtensionIconSet::MATCH_EXACTLY);
    if (!resource.empty()) {
      info_list.push_back(extensions::ImageLoader::ImageRepresentation(
          resource,
          extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
          gfx::Size(size, size),
          ui::SCALE_FACTOR_100P));
    }
  }

  extensions::ImageLoader::Get(browser_context)->LoadImageFamilyAsync(
      extension, info_list, base::Bind(&OnIconsLoaded, web_app_info, callback));
}

bool IsValidBookmarkAppUrl(const GURL& url) {
  URLPattern origin_only_pattern(Extension::kValidBookmarkAppSchemes);
  origin_only_pattern.SetMatchAllURLs(true);
  return url.is_valid() && origin_only_pattern.MatchesURL(url);
}

}  // namespace extensions

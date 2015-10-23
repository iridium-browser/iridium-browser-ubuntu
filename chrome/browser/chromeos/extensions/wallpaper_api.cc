// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_api.h"

#include <string>
#include <vector>

#include "ash/desktop_background/desktop_background_controller.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "extensions/browser/event_router.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

using base::BinaryValue;
using content::BrowserThread;

typedef base::Callback<void(bool success, const std::string&)> FetchCallback;

namespace set_wallpaper = extensions::api::wallpaper::SetWallpaper;

namespace {

class WallpaperFetcher : public net::URLFetcherDelegate {
 public:
  WallpaperFetcher() {}

  ~WallpaperFetcher() override {}

  void FetchWallpaper(const GURL& url, FetchCallback callback) {
    CancelPreviousFetch();
    callback_ = callback;
    url_fetcher_ = net::URLFetcher::Create(url, net::URLFetcher::GET, this);
    url_fetcher_->SetRequestContext(
        g_browser_process->system_request_context());
    url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
    url_fetcher_->Start();
  }

 private:
  // URLFetcherDelegate overrides:
  void OnURLFetchComplete(const net::URLFetcher* source) override {
    DCHECK(url_fetcher_.get() == source);

    bool success = source->GetStatus().is_success() &&
                   source->GetResponseCode() == net::HTTP_OK;
    std::string response;
    if (success) {
      source->GetResponseAsString(&response);
    } else {
      response = base::StringPrintf(
          "Downloading wallpaper %s failed. The response code is %d.",
          source->GetOriginalURL().ExtractFileName().c_str(),
          source->GetResponseCode());
    }
    url_fetcher_.reset();
    callback_.Run(success, response);
    callback_.Reset();
  }

  void CancelPreviousFetch() {
    if (url_fetcher_.get()) {
      callback_.Run(false, wallpaper_api_util::kCancelWallpaperMessage);
      callback_.Reset();
      url_fetcher_.reset();
    }
  }

  scoped_ptr<net::URLFetcher> url_fetcher_;
  FetchCallback callback_;
};

base::LazyInstance<WallpaperFetcher> g_wallpaper_fetcher =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

WallpaperSetWallpaperFunction::WallpaperSetWallpaperFunction() {
}

WallpaperSetWallpaperFunction::~WallpaperSetWallpaperFunction() {
}

bool WallpaperSetWallpaperFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  params_ = set_wallpaper::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  // Gets email address and username hash while at UI thread.
  user_id_ = user_manager::UserManager::Get()->GetLoggedInUser()->email();
  user_id_hash_ =
      user_manager::UserManager::Get()->GetLoggedInUser()->username_hash();

  if (params_->details.data) {
    StartDecode(*params_->details.data);
  } else {
    GURL wallpaper_url(*params_->details.url);
    if (wallpaper_url.is_valid()) {
      g_wallpaper_fetcher.Get().FetchWallpaper(
          wallpaper_url,
          base::Bind(&WallpaperSetWallpaperFunction::OnWallpaperFetched, this));
    } else {
      SetError("URL is invalid.");
      SendResponse(false);
    }
  }
  return true;
}

void WallpaperSetWallpaperFunction::OnWallpaperDecoded(
    const gfx::ImageSkia& image) {
  chromeos::WallpaperManager* wallpaper_manager =
      chromeos::WallpaperManager::Get();
  base::FilePath thumbnail_path = wallpaper_manager->GetCustomWallpaperPath(
      wallpaper::kThumbnailWallpaperSubDir, user_id_hash_,
      params_->details.filename);

  sequence_token_ = BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
      wallpaper::kWallpaperSequenceTokenName);
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(sequence_token_,
              base::SequencedWorkerPool::BLOCK_SHUTDOWN);
  wallpaper::WallpaperLayout layout = wallpaper_api_util::GetLayoutEnum(
      extensions::api::wallpaper::ToString(params_->details.layout));
  bool update_wallpaper =
      user_id_ == user_manager::UserManager::Get()->GetActiveUser()->email();
  wallpaper_manager->SetCustomWallpaper(user_id_,
                                        user_id_hash_,
                                        params_->details.filename,
                                        layout,
                                        user_manager::User::CUSTOMIZED,
                                        image,
                                        update_wallpaper);
  unsafe_wallpaper_decoder_ = NULL;

  if (params_->details.thumbnail) {
    image.EnsureRepsForSupportedScales();
    scoped_ptr<gfx::ImageSkia> deep_copy(image.DeepCopy());
    // Generates thumbnail before call api function callback. We can then
    // request thumbnail in the javascript callback.
    task_runner->PostTask(
        FROM_HERE,
        base::Bind(&WallpaperSetWallpaperFunction::GenerateThumbnail,
                   this,
                   thumbnail_path,
                   base::Passed(deep_copy.Pass())));
  } else {
    // Save current extenion name. It will be displayed in the component
    // wallpaper picker app. If current extension is the component wallpaper
    // picker, set an empty string.
    Profile* profile = Profile::FromBrowserContext(browser_context());
    if (extension()->id() == extension_misc::kWallpaperManagerId) {
      profile->GetPrefs()->SetString(prefs::kCurrentWallpaperAppName,
                                     std::string());
    } else {
      profile->GetPrefs()->SetString(prefs::kCurrentWallpaperAppName,
                                     extension()->name());
    }
    SendResponse(true);
  }

  // Inform the native Wallpaper Picker Application that the current wallpaper
  // has been modified by a third party application.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  extensions::EventRouter* event_router = extensions::EventRouter::Get(profile);
  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::WALLPAPER_PRIVATE_ON_WALLPAPER_CHANGED_BY_3RD_PARTY,
      extensions::api::wallpaper_private::OnWallpaperChangedBy3rdParty::
          kEventName,
      event_args.Pass()));
  event_router->DispatchEventToExtension(extension_misc::kWallpaperManagerId,
                                         event.Pass());
}

void WallpaperSetWallpaperFunction::GenerateThumbnail(
    const base::FilePath& thumbnail_path, scoped_ptr<gfx::ImageSkia> image) {
  DCHECK(BrowserThread::GetBlockingPool()->IsRunningSequenceOnCurrentThread(
      sequence_token_));
  if (!base::PathExists(thumbnail_path.DirName()))
    base::CreateDirectory(thumbnail_path.DirName());

  scoped_refptr<base::RefCountedBytes> data;
  chromeos::WallpaperManager::Get()->ResizeImage(
      *image, wallpaper::WALLPAPER_LAYOUT_STRETCH,
      wallpaper::kWallpaperThumbnailWidth, wallpaper::kWallpaperThumbnailHeight,
      &data, NULL);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &WallpaperSetWallpaperFunction::ThumbnailGenerated,
          this, data));
}

void WallpaperSetWallpaperFunction::ThumbnailGenerated(
    base::RefCountedBytes* data) {
  BinaryValue* result = BinaryValue::CreateWithCopiedBuffer(
      reinterpret_cast<const char*>(data->front()), data->size());
  SetResult(result);
  SendResponse(true);
}

void WallpaperSetWallpaperFunction::OnWallpaperFetched(
    bool success,
    const std::string& response) {
  if (success) {
    params_->details.data.reset(
        new std::vector<char>(response.begin(), response.end()));
    StartDecode(*params_->details.data);
  } else {
    SetError(response);
    SendResponse(false);
  }
}

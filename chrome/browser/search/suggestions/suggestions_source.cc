// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_source.h"

#include <vector>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/suggestions/suggestions_service.h"
#include "components/suggestions/suggestions_utils.h"
#include "net/base/escape.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace suggestions {

namespace {

const char kHtmlHeader[] =
    "<!DOCTYPE html>\n<html>\n<head>\n<title>Suggestions</title>\n"
    "<meta charset=\"utf-8\">\n"
    "<style type=\"text/css\">\nli {white-space: nowrap;}\n</style>\n";
const char kHtmlBody[] = "</head>\n<body>\n";
const char kHtmlFooter[] = "</body>\n</html>\n";

// Fills |output| with the HTML needed to display the suggestions.
void RenderOutputHtml(const SuggestionsProfile& profile,
                      const std::map<GURL, std::string>& base64_encoded_pngs,
                      std::string* output) {
  std::vector<std::string> out;
  out.push_back(kHtmlHeader);
  out.push_back(kHtmlBody);
  out.push_back("<h1>Suggestions</h1>\n<ul>");

  int64 now = (base::Time::NowFromSystemTime() - base::Time::UnixEpoch())
      .ToInternalValue();
  size_t size = profile.suggestions_size();
  for (size_t i = 0; i < size; ++i) {
    const ChromeSuggestion& suggestion = profile.suggestions(i);
    base::TimeDelta remaining_time = base::TimeDelta::FromMicroseconds(
        suggestion.expiry_ts() - now);
    base::string16 remaining_time_formatted = ui::TimeFormat::Detailed(
        ui::TimeFormat::Format::FORMAT_DURATION,
        ui::TimeFormat::Length::LENGTH_LONG,
        -1, remaining_time);
    std::string line;
    line += "<li><a href=\"";
    line += net::EscapeForHTML(suggestion.url());
    line += "\" target=\"_blank\">";
    line += net::EscapeForHTML(suggestion.title());
    std::map<GURL, std::string>::const_iterator it =
        base64_encoded_pngs.find(GURL(suggestion.url()));
    if (it != base64_encoded_pngs.end()) {
      line += "<br><img src='";
      line += it->second;
      line += "'>";
    }
    line += "</a> Expires in ";
    line += base::UTF16ToUTF8(remaining_time_formatted);
    line += "</li>\n";
    out.push_back(line);
  }
  out.push_back("</ul>");
  out.push_back(kHtmlFooter);
  *output = base::JoinString(out, base::StringPiece());
}

// Fills |output| with the HTML needed to display that no suggestions are
// available.
void RenderOutputHtmlNoSuggestions(std::string* output) {
  std::vector<std::string> out;
  out.push_back(kHtmlHeader);
  out.push_back(kHtmlBody);
  out.push_back("<h1>Suggestions</h1>\n");
  out.push_back("<p>You have no suggestions.</p>\n");
  out.push_back(kHtmlFooter);
  *output = base::JoinString(out, base::StringPiece());
}

}  // namespace

SuggestionsSource::SuggestionsSource(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

SuggestionsSource::~SuggestionsSource() {}

SuggestionsSource::RequestContext::RequestContext(
    const SuggestionsProfile& suggestions_profile_in,
    const content::URLDataSource::GotDataCallback& callback_in)
    : suggestions_profile(suggestions_profile_in),   // Copy.
      callback(callback_in)  // Copy.
{}

SuggestionsSource::RequestContext::~RequestContext() {}

std::string SuggestionsSource::GetSource() const {
  return chrome::kChromeUISuggestionsHost;
}

void SuggestionsSource::StartDataRequest(
    const std::string& path, int render_process_id, int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  SuggestionsService* suggestions_service =
      SuggestionsServiceFactory::GetForProfile(profile_);

  if (!suggestions_service) {
    callback.Run(NULL);
    return;
  }

  // Since it's a debugging page, it's fine to specify that sync state is
  // initialized.
  suggestions_service->FetchSuggestionsData(
      INITIALIZED_ENABLED_HISTORY,
      base::Bind(&SuggestionsSource::OnSuggestionsAvailable,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

std::string SuggestionsSource::GetMimeType(const std::string& path) const {
  return "text/html";
}

base::MessageLoop* SuggestionsSource::MessageLoopForRequestPath(
    const std::string& path) const {
  // This can be accessed from the IO thread.
  return content::URLDataSource::MessageLoopForRequestPath(path);
}

void SuggestionsSource::OnSuggestionsAvailable(
    const content::URLDataSource::GotDataCallback& callback,
    const SuggestionsProfile& suggestions_profile) {
  size_t size = suggestions_profile.suggestions_size();
  if (!size) {
    std::string output;
    RenderOutputHtmlNoSuggestions(&output);
    callback.Run(base::RefCountedString::TakeString(&output));
  } else {
    RequestContext* context = new RequestContext(suggestions_profile, callback);
    base::Closure barrier = BarrierClosure(
        size, base::Bind(&SuggestionsSource::OnThumbnailsFetched,
                         weak_ptr_factory_.GetWeakPtr(), context));
    for (size_t i = 0; i < size; ++i) {
      const ChromeSuggestion& suggestion = suggestions_profile.suggestions(i);
      // Fetch the thumbnail for this URL (exercising the fetcher). After all
      // fetches are done, including NULL callbacks for unavailable thumbnails,
      // SuggestionsSource::OnThumbnailsFetched will be called.
      SuggestionsService* suggestions_service(
          SuggestionsServiceFactory::GetForProfile(profile_));
      suggestions_service->GetPageThumbnail(
          GURL(suggestion.url()),
          base::Bind(&SuggestionsSource::OnThumbnailAvailable,
                     weak_ptr_factory_.GetWeakPtr(), context, barrier));
    }
  }
}

void SuggestionsSource::OnThumbnailsFetched(RequestContext* context) {
  scoped_ptr<RequestContext> context_deleter(context);

  std::string output;
  RenderOutputHtml(context->suggestions_profile, context->base64_encoded_pngs,
                   &output);
  context->callback.Run(base::RefCountedString::TakeString(&output));
}

void SuggestionsSource::OnThumbnailAvailable(RequestContext* context,
                                             base::Closure barrier,
                                             const GURL& url,
                                             const SkBitmap* bitmap) {
  if (bitmap) {
    std::vector<unsigned char> output;
    gfx::PNGCodec::EncodeBGRASkBitmap(*bitmap, false, &output);

    std::string encoded_output;
    base::Base64Encode(std::string(output.begin(), output.end()),
                       &encoded_output);
    context->base64_encoded_pngs[url] = "data:image/png;base64,";
    context->base64_encoded_pngs[url] += encoded_output;
  }
  barrier.Run();
}

}  // namespace suggestions

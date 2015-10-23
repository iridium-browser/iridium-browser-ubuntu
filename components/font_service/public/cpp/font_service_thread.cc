// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/font_service/public/cpp/font_service_thread.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/synchronization/waitable_event.h"
#include "components/font_service/public/cpp/mapped_font_file.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/platform_handle/platform_handle_functions.h"

namespace font_service {
namespace internal {

namespace {
const char kFontThreadName[] = "Font_Proxy_Thread";
}  // namespace

FontServiceThread::FontServiceThread(FontServicePtr font_service)
    : base::Thread(kFontThreadName),
      font_service_info_(font_service.PassInterface().Pass()) {
  base::Thread::Options options;
  options.message_pump_factory =
      base::Bind(&mojo::common::MessagePumpMojo::Create);
  StartWithOptions(options);
}

bool FontServiceThread::MatchFamilyName(
    const char family_name[],
    SkTypeface::Style requested_style,
    SkFontConfigInterface::FontIdentity* out_font_identity,
    SkString* out_family_name,
    SkTypeface::Style* out_style) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  bool out_valid = false;
  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&FontServiceThread::MatchFamilyNameImpl, this, &done_event,
                 family_name, requested_style, &out_valid, out_font_identity,
                 out_family_name, out_style));
  done_event.Wait();

  return out_valid;
}

scoped_refptr<MappedFontFile> FontServiceThread::OpenStream(
    const SkFontConfigInterface::FontIdentity& identity) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  base::File stream_file;
  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&FontServiceThread::OpenStreamImpl, this,
                                     &done_event, &stream_file, identity.fID));
  done_event.Wait();

  if (!stream_file.IsValid()) {
    NOTREACHED();
    return nullptr;
  }

  // Converts the file to out internal type.
  scoped_refptr<MappedFontFile> mapped_font_file =
      new MappedFontFile(identity.fID);
  if (!mapped_font_file->Initialize(stream_file.Pass()))
    return nullptr;

  return mapped_font_file;
}

FontServiceThread::~FontServiceThread() {
  Stop();
}

void FontServiceThread::MatchFamilyNameImpl(
    base::WaitableEvent* done_event,
    const char family_name[],
    SkTypeface::Style requested_style,
    bool* out_valid,
    SkFontConfigInterface::FontIdentity* out_font_identity,
    SkString* out_family_name,
    SkTypeface::Style* out_style) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  font_service_->MatchFamilyName(
      mojo::String(family_name), static_cast<TypefaceStyle>(requested_style),
      base::Bind(&FontServiceThread::OnMatchFamilyNameComplete, this,
                 done_event, out_valid, out_font_identity, out_family_name,
                 out_style));
}

void FontServiceThread::OnMatchFamilyNameComplete(
    base::WaitableEvent* done_event,
    bool* out_valid,
    SkFontConfigInterface::FontIdentity* out_font_identity,
    SkString* out_family_name,
    SkTypeface::Style* out_style,
    FontIdentityPtr font_identity,
    mojo::String family_name,
    TypefaceStyle style) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  *out_valid = font_identity;
  if (font_identity) {
    out_font_identity->fID = font_identity->id;
    out_font_identity->fTTCIndex = font_identity->ttc_index;
    out_font_identity->fString = font_identity->str_representation.data();
    // TODO(erg): fStyle isn't set. This is rather odd, however it matches the
    // behaviour of the current Linux IPC version.

    *out_family_name = family_name.data();
    *out_style = static_cast<SkTypeface::Style>(style);
  }

  done_event->Signal();
}

void FontServiceThread::OpenStreamImpl(base::WaitableEvent* done_event,
                                       base::File* output_file,
                                       const uint32_t id_number) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  font_service_->OpenStream(
      id_number, base::Bind(&FontServiceThread::OnOpenStreamComplete, this,
                            done_event, output_file));
}

void FontServiceThread::OnOpenStreamComplete(base::WaitableEvent* done_event,
                                             base::File* output_file,
                                             mojo::ScopedHandle handle) {
  if (handle.is_valid()) {
    MojoPlatformHandle platform_handle;
    CHECK(MojoExtractPlatformHandle(handle.release().value(),
                                    &platform_handle) == MOJO_RESULT_OK);
    *output_file = base::File(platform_handle).Pass();
  }

  done_event->Signal();
}

void FontServiceThread::Init() {
  font_service_.Bind(font_service_info_.Pass());
}

void FontServiceThread::CleanUp() {
  font_service_.reset();
}

}  // namespace internal
}  // namespace font_service

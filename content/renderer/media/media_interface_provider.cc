// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_interface_provider.h"

#include <string>

#include "base/bind.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/interfaces/renderer.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/shell/public/cpp/interface_provider.h"

namespace content {

MediaInterfaceProvider::MediaInterfaceProvider(
    shell::InterfaceProvider* remote_interfaces)
    : remote_interfaces_(remote_interfaces) {
}

MediaInterfaceProvider::~MediaInterfaceProvider() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MediaInterfaceProvider::GetInterface(const std::string& interface_name,
                                          mojo::ScopedMessagePipeHandle pipe) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (interface_name == media::mojom::ContentDecryptionModule::Name_) {
    GetMediaServiceFactory()->CreateCdm(
        mojo::MakeRequest<media::mojom::ContentDecryptionModule>(
            std::move(pipe)));
  } else if (interface_name == media::mojom::Renderer::Name_) {
    GetMediaServiceFactory()->CreateRenderer(
        std::string(),
        mojo::MakeRequest<media::mojom::Renderer>(std::move(pipe)));
  } else if (interface_name == media::mojom::AudioDecoder::Name_) {
    GetMediaServiceFactory()->CreateAudioDecoder(
        mojo::MakeRequest<media::mojom::AudioDecoder>(std::move(pipe)));
  } else if (interface_name == media::mojom::VideoDecoder::Name_) {
    GetMediaServiceFactory()->CreateVideoDecoder(
        mojo::MakeRequest<media::mojom::VideoDecoder>(std::move(pipe)));
  } else {
    NOTREACHED();
  }
}

media::mojom::ServiceFactory* MediaInterfaceProvider::GetMediaServiceFactory() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!media_service_factory_) {
    remote_interfaces_->GetInterface(&media_service_factory_);
    media_service_factory_.set_connection_error_handler(base::Bind(
        &MediaInterfaceProvider::OnConnectionError, base::Unretained(this)));
  }

  return media_service_factory_.get();
}

void MediaInterfaceProvider::OnConnectionError() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  media_service_factory_.reset();
}

}  // namespace content

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the web manifest manager.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "content/public/common/manifest.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ManifestManagerMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::Manifest::Icon)
  IPC_STRUCT_TRAITS_MEMBER(src)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(sizes)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::Manifest::RelatedApplication)
  IPC_STRUCT_TRAITS_MEMBER(platform)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::Manifest)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(short_name)
  IPC_STRUCT_TRAITS_MEMBER(start_url)
  IPC_STRUCT_TRAITS_MEMBER(scope)
  IPC_STRUCT_TRAITS_MEMBER(display)
  IPC_STRUCT_TRAITS_MEMBER(orientation)
  IPC_STRUCT_TRAITS_MEMBER(icons)
  IPC_STRUCT_TRAITS_MEMBER(related_applications)
  IPC_STRUCT_TRAITS_MEMBER(prefer_related_applications)
  IPC_STRUCT_TRAITS_MEMBER(theme_color)
  IPC_STRUCT_TRAITS_MEMBER(background_color)
  IPC_STRUCT_TRAITS_MEMBER(gcm_sender_id)
IPC_STRUCT_TRAITS_END()

// The browser process requests for the manifest linked with the associated
// RenderFrame. The render process will respond via a RequestManifestResponse
// IPC message with a Manifest object attached to it and the associated
// |request_id| that was initially given.
IPC_MESSAGE_ROUTED1(ManifestManagerMsg_RequestManifest,
                    int /* request_id */)

// The render process' response to a RequestManifest. The |request_id| will
// match the one that was initially received. |manifest_url| will be empty if
// there is no manifest specified in the associated RenderFrame's document.
// |manifest| will be empty if a manifest was specified, but could not be
// parsed correctly.
IPC_MESSAGE_ROUTED3(ManifestManagerHostMsg_RequestManifestResponse,
                    int, /* request_id */
                    GURL, /* manifest URL */
                    content::Manifest /* manifest */)

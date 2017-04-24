/*
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/storage/StorageNamespace.h"

#include "modules/storage/StorageArea.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebStorageArea.h"
#include "public/platform/WebStorageNamespace.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

StorageNamespace::StorageNamespace(
    std::unique_ptr<WebStorageNamespace> webStorageNamespace)
    : m_webStorageNamespace(std::move(webStorageNamespace)) {}

StorageNamespace::~StorageNamespace() {}

StorageArea* StorageNamespace::localStorageArea(SecurityOrigin* origin) {
  ASSERT(isMainThread());
  static WebStorageNamespace* localStorageNamespace = nullptr;
  if (!localStorageNamespace)
    localStorageNamespace = Platform::current()->createLocalStorageNamespace();
  return StorageArea::create(
      WTF::wrapUnique(
          localStorageNamespace->createStorageArea(WebSecurityOrigin(origin))),
      LocalStorage);
}

StorageArea* StorageNamespace::storageArea(SecurityOrigin* origin) {
  return StorageArea::create(
      WTF::wrapUnique(
          m_webStorageNamespace->createStorageArea(WebSecurityOrigin(origin))),
      SessionStorage);
}

bool StorageNamespace::isSameNamespace(
    const WebStorageNamespace& sessionNamespace) const {
  return m_webStorageNamespace &&
         m_webStorageNamespace->isSameNamespace(sessionNamespace);
}

}  // namespace blink

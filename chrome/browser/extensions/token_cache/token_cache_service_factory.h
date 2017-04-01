// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TOKEN_CACHE_TOKEN_CACHE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_TOKEN_CACHE_TOKEN_CACHE_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace extensions {
class TokenCacheService;
}  // namespace extensions

class TokenCacheServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static extensions::TokenCacheService* GetForProfile(Profile* profile);
  static TokenCacheServiceFactory* GetInstance();

 private:
  TokenCacheServiceFactory();
  ~TokenCacheServiceFactory() override;

  friend struct base::DefaultSingletonTraits<TokenCacheServiceFactory>;

  // Inherited from BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

   DISALLOW_COPY_AND_ASSIGN(TokenCacheServiceFactory);
};

#endif  // CHROME_BROWSER_EXTENSIONS_TOKEN_CACHE_TOKEN_CACHE_SERVICE_FACTORY_H_

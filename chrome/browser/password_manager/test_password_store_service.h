// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_TEST_PASSWORD_STORE_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_TEST_PASSWORD_STORE_SERVICE_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/password_manager/password_store_factory.h"

namespace content {
class BrowserContext;
}

namespace password_manager {
class PasswordStore;
}

class TestPasswordStoreService : public PasswordStoreService {
 public:
  static scoped_ptr<KeyedService> Build(content::BrowserContext* profile);

 private:
  explicit TestPasswordStoreService(
      scoped_refptr<password_manager::PasswordStore> password_store);

  ~TestPasswordStoreService() override;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordStoreService);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_TEST_PASSWORD_STORE_SERVICE_H_

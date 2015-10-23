// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_SNACKBARS_AUTO_SIGNIN_SNACKBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_ANDROID_SNACKBARS_AUTO_SIGNIN_SNACKBAR_CONTROLLER_H_

#include <jni.h>

#include "base/strings/string16.h"

class TabAndroid;

// Shows an auto sign-in snackbar in order to inform the users that they were
// automatically signed in to the website. |username| is the username used by
// the user in order to login to the web site, it can be email, telephone number
// or any string.
void ShowAutoSigninSnackbar(TabAndroid *tab, const base::string16& username);

// Register native method.
bool RegisterAutoSigninSnackbarController(JNIEnv* env);

#endif // CHROME_BROWSER_UI_ANDROID_SNACKBARS_AUTO_SIGNIN_SNACKBAR_CONTROLLER_H_

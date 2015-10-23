// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLCHECKER_SESSION_BRIDGE_ANDROID_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLCHECKER_SESSION_BRIDGE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"

// A class used to interface between the Java class of the same name and the
// android message filter.  This class receives text to be spellchecked
// from the message filter, sends that text to the Java side via JNI to be
// spellchecked, and then sends those results to the renderer.
class SpellCheckerSessionBridge {
 public:
  explicit SpellCheckerSessionBridge(int render_process_id);
  ~SpellCheckerSessionBridge();
  static bool RegisterJNI(JNIEnv* env);

  // Receives text to be checked from the message filter and sends it to Java
  // to be spellchecked.
  void RequestTextCheck(int route_id,
                        int identifier,
                        const base::string16& text);

  // Receives information from Java side about the typos in a given string
  // of text, processes these and sends them to the renderer.
  void ProcessSpellCheckResults(JNIEnv* env,
                                jobject jobj,
                                jintArray offset_array,
                                jintArray length_array);

 private:
  struct SpellingRequest {
    SpellingRequest(int route_id, int identifier, const base::string16& text);
    ~SpellingRequest();

    int route_id;
    int identifier;
    base::string16 text;
  };

  int render_process_id_;

  scoped_ptr<SpellingRequest> active_request_;
  scoped_ptr<SpellingRequest> pending_request_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckerSessionBridge);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLCHECKER_SESSION_BRIDGE_ANDROID_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HOTWORD_PRIVATE_HOTWORD_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_HOTWORD_PRIVATE_HOTWORD_PRIVATE_API_H_

#include "base/values.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/hotword_private.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"

class Profile;

namespace extensions {

// Listens for changes in disable/enabled state and forwards as an extension
// event.
class HotwordPrivateEventService : public BrowserContextKeyedAPI {
 public:
  explicit HotwordPrivateEventService(content::BrowserContext* context);
  ~HotwordPrivateEventService() override;

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;
  static BrowserContextKeyedAPIFactory<HotwordPrivateEventService>*
      GetFactoryInstance();
  static const char* service_name();

  void OnEnabledChanged(const std::string& pref_name);

  void OnHotwordSessionRequested();

  void OnHotwordSessionStopped();

  void OnHotwordTriggered();

  void OnFinalizeSpeakerModel();

  void OnSpeakerModelSaved();

  void OnDeleteSpeakerModel();

  void OnSpeakerModelExists();

  void OnMicrophoneStateChanged(bool enabled);

 private:
  friend class BrowserContextKeyedAPIFactory<HotwordPrivateEventService>;

  void SignalEvent(events::HistogramValue histogram_value,
                   const std::string& event_name);
  void SignalEvent(events::HistogramValue histogram_value,
                   const std::string& event_name,
                   std::unique_ptr<base::ListValue> args);

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;
};


class HotwordPrivateSetEnabledFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.setEnabled",
                             HOTWORDPRIVATE_SETENABLED)

 protected:
  ~HotwordPrivateSetEnabledFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateSetAudioLoggingEnabledFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.setAudioLoggingEnabled",
                             HOTWORDPRIVATE_SETAUDIOLOGGINGENABLED)

 protected:
  ~HotwordPrivateSetAudioLoggingEnabledFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateSetHotwordAlwaysOnSearchEnabledFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.setHotwordAlwaysOnSearchEnabled",
                             HOTWORDPRIVATE_SETHOTWORDALWAYSONSEARCHENABLED)

 protected:
  ~HotwordPrivateSetHotwordAlwaysOnSearchEnabledFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateGetStatusFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.getStatus",
                             HOTWORDPRIVATE_GETSTATUS)

 protected:
  ~HotwordPrivateGetStatusFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateSetHotwordSessionStateFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.setHotwordSessionState",
                             HOTWORDPRIVATE_SETHOTWORDSESSIONSTATE);

 protected:
  ~HotwordPrivateSetHotwordSessionStateFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateNotifyHotwordRecognitionFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.notifyHotwordRecognition",
                             HOTWORDPRIVATE_NOTIFYHOTWORDRECOGNITION);

 protected:
  ~HotwordPrivateNotifyHotwordRecognitionFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateGetLaunchStateFunction :
    public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.getLaunchState",
                             HOTWORDPRIVATE_GETLAUNCHSTATE)

 protected:
  ~HotwordPrivateGetLaunchStateFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateStartTrainingFunction :
    public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.startTraining",
                             HOTWORDPRIVATE_STARTTRAINING)

 protected:
  ~HotwordPrivateStartTrainingFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateFinalizeSpeakerModelFunction :
    public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.finalizeSpeakerModel",
                             HOTWORDPRIVATE_FINALIZESPEAKERMODEL)

 protected:
  ~HotwordPrivateFinalizeSpeakerModelFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateNotifySpeakerModelSavedFunction :
    public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.notifySpeakerModelSaved",
                             HOTWORDPRIVATE_NOTIFYSPEAKERMODELSAVED)

 protected:
  ~HotwordPrivateNotifySpeakerModelSavedFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateStopTrainingFunction :
    public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.stopTraining",
                             HOTWORDPRIVATE_STOPTRAINING)

 protected:
  ~HotwordPrivateStopTrainingFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateGetLocalizedStringsFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.getLocalizedStrings",
                             HOTWORDPRIVATE_GETLOCALIZEDSTRINGS)

 protected:
  ~HotwordPrivateGetLocalizedStringsFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class HotwordPrivateSetAudioHistoryEnabledFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.setAudioHistoryEnabled",
                             HOTWORDPRIVATE_SETAUDIOHISTORYENABLED)

 protected:
  ~HotwordPrivateSetAudioHistoryEnabledFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;

  void SetResultAndSendResponse(bool success, bool new_enabled_value);
};

class HotwordPrivateGetAudioHistoryEnabledFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.getAudioHistoryEnabled",
                             HOTWORDPRIVATE_GETAUDIOHISTORYENABLED)

 protected:
  ~HotwordPrivateGetAudioHistoryEnabledFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;

  void SetResultAndSendResponse(bool success, bool new_enabled_value);
};

class HotwordPrivateSpeakerModelExistsResultFunction :
    public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.speakerModelExistsResult",
                             HOTWORDPRIVATE_SPEAKERMODELEXISTSRESULT)

 protected:
  ~HotwordPrivateSpeakerModelExistsResultFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HOTWORD_PRIVATE_HOTWORD_PRIVATE_API_H_

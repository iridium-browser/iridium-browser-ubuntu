// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_CONTROLLER_H_

#include <string>
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension.h"

class Browser;
class ExtensionService;
class Profile;

namespace extensions {

class ExtensionPrefs;
class ExtensionRegistry;

class ExtensionMessageBubbleController {
 public:
  // UMA histogram constants.
  enum BubbleAction {
    ACTION_LEARN_MORE = 0,
    ACTION_EXECUTE,
    ACTION_DISMISS,
    ACTION_BOUNDARY, // Must be the last value.
  };

  class Delegate {
   public:
    explicit Delegate(Profile* profile);
    virtual ~Delegate();

    virtual bool ShouldIncludeExtension(const Extension* extension) = 0;
    virtual void AcknowledgeExtension(
        const std::string& extension_id,
        BubbleAction action) = 0;
    virtual void PerformAction(const ExtensionIdList& list) = 0;

    // Text for various UI labels shown in the bubble.
    virtual base::string16 GetTitle() const = 0;
    // Fetches the message to show in the body. |anchored_to_browser_action|
    // will be true if the bubble is anchored against a specific extension
    // icon, allowing the bubble to show a different message than when it is
    // anchored against something else (e.g. show "This extension has..."
    // instead of "An extension has...").
    // |extension_count| is the number of extensions being referenced.
    virtual base::string16 GetMessageBody(
        bool anchored_to_browser_action,
        int extension_count) const = 0;
    virtual base::string16 GetOverflowText(
        const base::string16& overflow_count) const = 0;
    virtual base::string16 GetLearnMoreLabel() const;
    virtual GURL GetLearnMoreUrl() const = 0;
    virtual base::string16 GetActionButtonLabel() const = 0;
    virtual base::string16 GetDismissButtonLabel() const = 0;

    // Whether to show a list of extensions in the bubble.
    virtual bool ShouldShowExtensionList() const = 0;

    // Returns true if the set of affected extensions should be highlighted in
    // the toolbar.
    virtual bool ShouldHighlightExtensions() const = 0;

    // Returns true if only enabled extensions should be considered.
    virtual bool ShouldLimitToEnabledExtensions() const = 0;

    // Record, through UMA, how many extensions were found.
    virtual void LogExtensionCount(size_t count) = 0;
    virtual void LogAction(BubbleAction action) = 0;

    // Has the user acknowledged info about the extension the bubble reports.
    virtual bool HasBubbleInfoBeenAcknowledged(const std::string& extension_id);
    virtual void SetBubbleInfoBeenAcknowledged(const std::string& extension_id,
                                               bool value);

    // Returns the set of profiles for which this bubble has been shown.
    // If profiles are not tracked, returns null (default).
    virtual std::set<Profile*>* GetProfileSet();

   protected:
    Profile* profile() { return profile_; }
    ExtensionService* service() { return service_; }
    const ExtensionRegistry* registry() const { return registry_; }

    std::string get_acknowledged_flag_pref_name() const;
    void set_acknowledged_flag_pref_name(std::string pref_name);

   private:
    // A weak pointer to the profile we are associated with. Not owned by us.
    Profile* profile_;

    // The extension service associated with the profile.
    ExtensionService* service_;

    // The extension registry associated with the profile.
    ExtensionRegistry* registry_;

    // Name for corresponding pref that keeps if the info the bubble contains
    // was acknowledged by user.
    std::string acknowledged_pref_name_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  ExtensionMessageBubbleController(Delegate* delegate, Browser* browser);
  virtual ~ExtensionMessageBubbleController();

  Delegate* delegate() const { return delegate_.get(); }
  Profile* profile();

  // Returns true if the bubble should be displayed.
  bool ShouldShow();

  // Obtains a list of all extensions (by name) the controller knows about.
  std::vector<base::string16> GetExtensionList();

  // Returns the list of all extensions to display in the bubble, including
  // bullets and newlines. If the extension list should not be displayed,
  // returns an empty string.
  base::string16 GetExtensionListForDisplay();

  // Obtains a list of all extensions (by id) the controller knows about.
  const ExtensionIdList& GetExtensionIdList();

  // Whether to close the bubble when it loses focus.
  virtual bool CloseOnDeactivate();

  // Highlights the affected extensions if appropriate. Safe to call multiple
  // times.
  void HighlightExtensionsIfNecessary();

  // Sets up the callbacks and shows the bubble.
  virtual void Show(ExtensionMessageBubble* bubble);

  // Callbacks from bubble. Declared virtual for testing purposes.
  virtual void OnBubbleAction();
  virtual void OnBubbleDismiss();
  virtual void OnLinkClicked();

  static void set_should_ignore_learn_more_for_testing(
      bool should_ignore_learn_more);

 private:
  // Iterate over the known extensions and acknowledge each one.
  void AcknowledgeExtensions();

  // Get the data this class needs.
  ExtensionIdList* GetOrCreateExtensionList();

  // Performs cleanup after the bubble closes.
  void OnClose();

  // A weak pointer to the Browser we are associated with. Not owned by us.
  Browser* browser_;

  // The list of extensions found.
  ExtensionIdList extension_list_;

  // The action the user took in the bubble.
  BubbleAction user_action_;

  // Our delegate supplying information about what to show in the dialog.
  scoped_ptr<Delegate> delegate_;

  // Whether this class has initialized.
  bool initialized_;

  // Whether or not the bubble is highlighting extensions.
  bool did_highlight_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_BUBBLE_CONTROLLER_H_

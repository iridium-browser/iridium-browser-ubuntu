// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CHROME_CONTENT_RULES_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CHROME_CONTENT_RULES_REGISTRY_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/extensions/api/declarative_content/content_action.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_condition_tracker_delegate.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_css_condition_tracker.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_is_bookmarked_condition_tracker.h"
#include "chrome/browser/extensions/api/declarative_content/declarative_content_page_url_condition_tracker.h"
#include "components/url_matcher/url_matcher.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"

class ContentPermissions;

namespace content {
class BrowserContext;
class RenderProcessHost;
class WebContents;
struct FrameNavigateParams;
struct LoadCommittedDetails;
}

namespace extension_web_request_api_helpers {
struct EventResponseDelta;
}

namespace net {
class URLRequest;
}

namespace extensions {

class Extension;

// Representation of a condition in the Declarative Content API. A condition
// consists of a set of predicates on the page state, all of which must be
// satisified for the condition to be fulfilled.
struct ContentCondition {
 public:
  ContentCondition(
      scoped_ptr<DeclarativeContentPageUrlPredicate> page_url_predicate,
      scoped_ptr<DeclarativeContentCssPredicate> css_predicate,
      scoped_ptr<DeclarativeContentIsBookmarkedPredicate>
          is_bookmarked_predicate);
  ~ContentCondition();

  scoped_ptr<DeclarativeContentPageUrlPredicate> page_url_predicate;
  scoped_ptr<DeclarativeContentCssPredicate> css_predicate;
  scoped_ptr<DeclarativeContentIsBookmarkedPredicate> is_bookmarked_predicate;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentCondition);
};

// Defines the interface for a predicate factory. Temporary, until we can
// introduce an interface to be implemented by the trackers that returns a
// ContentPredicate.
template <class T>
using PredicateFactory =
    base::Callback<scoped_ptr<T>(const Extension* extension,
                                 const base::Value& value,
                                 std::string* error)>;

// Factory function that instantiates a ContentCondition according to the
// description |condition| passed by the extension API.  |condition| should be
// an instance of declarativeContent.PageStateMatcher.
scoped_ptr<ContentCondition> CreateContentCondition(
    const Extension* extension,
    const PredicateFactory<DeclarativeContentCssPredicate>&
        css_predicate_factory,
    const PredicateFactory<DeclarativeContentIsBookmarkedPredicate>&
        is_bookmarked_predicate_factory,
    const PredicateFactory<DeclarativeContentPageUrlPredicate>&
        page_url_predicate_factory,
    const base::Value& condition,
    std::string* error);

// The ChromeContentRulesRegistry is responsible for managing
// the internal representation of rules for the Declarative Content API.
//
// Here is the high level overview of this functionality:
//
// api::events::Rule consists of conditions and actions, these are
// represented as a ContentRule with ContentConditions and ContentRuleActions.
//
// The evaluation of URL related condition attributes (host_suffix, path_prefix)
// is delegated to a URLMatcher, because this is capable of evaluating many
// of such URL related condition attributes in parallel.
//
// A note on incognito support: separate instances of ChromeContentRulesRegistry
// are created for incognito and non-incognito contexts. The incognito instance,
// however, is only responsible for applying rules registered by the incognito
// side of split-mode extensions to incognito tabs. The non-incognito instance
// handles incognito tabs for spanning-mode extensions, plus all non-incognito
// tabs.
class ChromeContentRulesRegistry
    : public ContentRulesRegistry,
      public content::NotificationObserver,
      public DeclarativeContentConditionTrackerDelegate {
 public:
  // For testing, |ui_part| can be NULL. In that case it constructs the
  // registry with storage functionality suspended.
  ChromeContentRulesRegistry(content::BrowserContext* browser_context,
                             RulesCacheDelegate* cache_delegate);

  // ContentRulesRegistry:
  void MonitorWebContentsForRuleEvaluation(
      content::WebContents* contents) override;
  void DidNavigateMainFrame(
      content::WebContents* tab,
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  // RulesRegistry:
  std::string AddRulesImpl(
      const std::string& extension_id,
      const std::vector<linked_ptr<api::events::Rule>>& rules) override;
  std::string RemoveRulesImpl(
      const std::string& extension_id,
      const std::vector<std::string>& rule_identifiers) override;
  std::string RemoveAllRulesImpl(const std::string& extension_id) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // DeclarativeContentConditionTrackerDelegate:
  void RequestEvaluation(content::WebContents* contents) override;
  bool ShouldManageConditionsForBrowserContext(
      content::BrowserContext* context) override;

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

  // TODO(wittman): Remove once DeclarativeChromeContentRulesRegistry no longer
  // depends on concrete condition implementations. At that point
  // DeclarativeChromeContentRulesRegistryTest.ActiveRulesDoesntGrow will be
  // able to use a test condition object and not need to depend on force setting
  // matching CSS seleectors.
  void UpdateMatchingCssSelectorsForTesting(
      content::WebContents* contents,
      const std::vector<std::string>& matching_css_selectors);

  // Returns the number of active rules.
  size_t GetActiveRulesCountForTesting();

 protected:
  ~ChromeContentRulesRegistry() override;

 private:
  // The internal declarative rule representation. Corresponds to a declarative
  // API rule: https://developer.chrome.com/extensions/events.html#declarative.
  struct ContentRule {
   public:
    ContentRule(const Extension* extension,
                ScopedVector<const ContentCondition> conditions,
                ScopedVector<const ContentAction> actions,
                int priority);
    ~ContentRule();

    const Extension* extension;
    ScopedVector<const ContentCondition> conditions;
    ScopedVector<const ContentAction> actions;
    int priority;

   private:
    DISALLOW_COPY_AND_ASSIGN(ContentRule);
  };

  // Specifies what to do with evaluation requests.
  // TODO(wittman): Try to eliminate the need for IGNORE after refactoring to
  // treat all condition evaluation consistently. Currently RemoveRulesImpl only
  // updates the CSS selectors after the rules are removed, which is too late
  // for evaluation.
  enum EvaluationDisposition {
    EVALUATE_REQUESTS,  // Evaluate immediately.
    DEFER_REQUESTS,  // Defer for later evaluation.
    IGNORE_REQUESTS  // Ignore.
  };

  class EvaluationScope;

  // Creates a ContentRule for |extension| given a json definition.  The format
  // of each condition and action's json is up to the specific ContentCondition
  // and ContentAction.  |extension| may be NULL in tests.  If |error| is empty,
  // the translation was successful and the returned rule is internally
  // consistent.
  scoped_ptr<const ContentRule> CreateRule(
      const Extension* extension,
      const PredicateFactory<DeclarativeContentCssPredicate>&
          css_predicate_factory,
      const PredicateFactory<DeclarativeContentIsBookmarkedPredicate>&
          is_bookmarked_predicate_factory,
      const PredicateFactory<DeclarativeContentPageUrlPredicate>&
          page_url_predicate_factory,
      const api::events::Rule& api_rule,
      std::string* error);

  // True if this object is managing the rules for |context|.
  bool ManagingRulesForBrowserContext(content::BrowserContext* context);

  std::set<const ContentRule*> GetMatchingRules(
      content::WebContents* tab) const;

  // Updates the condition evaluator with the current watched CSS selectors.
  void UpdateCssSelectorsFromRules();

  // Evaluates the conditions for |tab| based on the tab state and matching CSS
  // selectors.
  void EvaluateConditionsForTab(content::WebContents* tab);

  // Returns true if a rule created by |extension| should be evaluated for an
  // incognito renderer.
  bool ShouldEvaluateExtensionRulesForIncognitoRenderer(
      const Extension* extension) const;

  using ExtensionIdRuleIdPair = std::pair<extensions::ExtensionId, std::string>;
  using RuleAndConditionForURLMatcherId =
      std::map<url_matcher::URLMatcherConditionSet::ID,
               std::pair<const ContentRule*, const ContentCondition*>>;
  using RulesMap = std::map<ExtensionIdRuleIdPair,
                            linked_ptr<const ContentRule>>;

  RulesMap content_rules_;

  // Maps a WebContents to the set of rules that match on that WebContents.
  // This lets us call Revert as appropriate. Note that this is expected to have
  // a key-value pair for every WebContents the registry is tracking, even if
  // the value is the empty set.
  std::map<content::WebContents*, std::set<const ContentRule*>> active_rules_;

  // Responsible for tracking declarative content page URL condition state.
  DeclarativeContentPageUrlConditionTracker page_url_condition_tracker_;

  // Responsible for tracking declarative content CSS condition state.
  DeclarativeContentCssConditionTracker css_condition_tracker_;

  // Responsible for tracking declarative content bookmarked condition state.
  DeclarativeContentIsBookmarkedConditionTracker
      is_bookmarked_condition_tracker_;

  // Specifies what to do with evaluation requests.
  EvaluationDisposition evaluation_disposition_;

  // Contains WebContents which require rule evaluation. Only used while
  // |evaluation_disposition_| is DEFER.
  std::set<content::WebContents*> evaluation_pending_;

  // Manages our notification registrations.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentRulesRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CHROME_CONTENT_RULES_REGISTRY_H_

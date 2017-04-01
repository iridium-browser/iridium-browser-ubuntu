/*
 * Copyright (C) 2008, 2009, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/accessibility/AXObject.h"

#include "SkMatrix44.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/VisibleUnits.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLDialogElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutBoxModelObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/UserGestureIndicator.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/HashSet.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/WTFString.h"

using blink::WebLocalizedString;

namespace blink {

using namespace HTMLNames;

namespace {
typedef HashMap<String, AccessibilityRole, CaseFoldingHash> ARIARoleMap;
typedef HashSet<String, CaseFoldingHash> ARIAWidgetSet;

struct RoleEntry {
  const char* ariaRole;
  AccessibilityRole webcoreRole;
};

const RoleEntry roles[] = {{"alert", AlertRole},
                           {"alertdialog", AlertDialogRole},
                           {"application", ApplicationRole},
                           {"article", ArticleRole},
                           {"banner", BannerRole},
                           {"button", ButtonRole},
                           {"cell", CellRole},
                           {"checkbox", CheckBoxRole},
                           {"columnheader", ColumnHeaderRole},
                           {"combobox", ComboBoxRole},
                           {"complementary", ComplementaryRole},
                           {"contentinfo", ContentInfoRole},
                           {"definition", DefinitionRole},
                           {"dialog", DialogRole},
                           {"directory", DirectoryRole},
                           {"document", DocumentRole},
                           {"feed", FeedRole},
                           {"figure", FigureRole},
                           {"form", FormRole},
                           {"grid", GridRole},
                           {"gridcell", CellRole},
                           {"group", GroupRole},
                           {"heading", HeadingRole},
                           {"img", ImageRole},
                           {"link", LinkRole},
                           {"list", ListRole},
                           {"listbox", ListBoxRole},
                           {"listitem", ListItemRole},
                           {"log", LogRole},
                           {"main", MainRole},
                           {"marquee", MarqueeRole},
                           {"math", MathRole},
                           {"menu", MenuRole},
                           {"menubar", MenuBarRole},
                           {"menuitem", MenuItemRole},
                           {"menuitemcheckbox", MenuItemCheckBoxRole},
                           {"menuitemradio", MenuItemRadioRole},
                           {"navigation", NavigationRole},
                           {"none", NoneRole},
                           {"note", NoteRole},
                           {"option", ListBoxOptionRole},
                           {"presentation", PresentationalRole},
                           {"progressbar", ProgressIndicatorRole},
                           {"radio", RadioButtonRole},
                           {"radiogroup", RadioGroupRole},
                           {"region", RegionRole},
                           {"row", RowRole},
                           {"rowheader", RowHeaderRole},
                           {"scrollbar", ScrollBarRole},
                           {"search", SearchRole},
                           {"searchbox", SearchBoxRole},
                           {"separator", SplitterRole},
                           {"slider", SliderRole},
                           {"spinbutton", SpinButtonRole},
                           {"status", StatusRole},
                           {"switch", SwitchRole},
                           {"tab", TabRole},
                           {"table", TableRole},
                           {"tablist", TabListRole},
                           {"tabpanel", TabPanelRole},
                           {"term", TermRole},
                           {"text", StaticTextRole},
                           {"textbox", TextFieldRole},
                           {"timer", TimerRole},
                           {"toolbar", ToolbarRole},
                           {"tooltip", UserInterfaceTooltipRole},
                           {"tree", TreeRole},
                           {"treegrid", TreeGridRole},
                           {"treeitem", TreeItemRole}};

struct InternalRoleEntry {
  AccessibilityRole webcoreRole;
  const char* internalRoleName;
};

const InternalRoleEntry internalRoles[] = {
    {UnknownRole, "Unknown"},
    {AbbrRole, "Abbr"},
    {AlertDialogRole, "AlertDialog"},
    {AlertRole, "Alert"},
    {AnnotationRole, "Annotation"},
    {ApplicationRole, "Application"},
    {ArticleRole, "Article"},
    {AudioRole, "Audio"},
    {BannerRole, "Banner"},
    {BlockquoteRole, "Blockquote"},
    // TODO(nektar): Delete busy_indicator role. It's used nowhere.
    {BusyIndicatorRole, "BusyIndicator"},
    {ButtonRole, "Button"},
    {CanvasRole, "Canvas"},
    {CaptionRole, "Caption"},
    {CellRole, "Cell"},
    {CheckBoxRole, "CheckBox"},
    {ColorWellRole, "ColorWell"},
    {ColumnHeaderRole, "ColumnHeader"},
    {ColumnRole, "Column"},
    {ComboBoxRole, "ComboBox"},
    {ComplementaryRole, "Complementary"},
    {ContentInfoRole, "ContentInfo"},
    {DateRole, "Date"},
    {DateTimeRole, "DateTime"},
    {DefinitionRole, "Definition"},
    {DescriptionListDetailRole, "DescriptionListDetail"},
    {DescriptionListRole, "DescriptionList"},
    {DescriptionListTermRole, "DescriptionListTerm"},
    {DetailsRole, "Details"},
    {DialogRole, "Dialog"},
    {DirectoryRole, "Directory"},
    {DisclosureTriangleRole, "DisclosureTriangle"},
    {DivRole, "Div"},
    {DocumentRole, "Document"},
    {EmbeddedObjectRole, "EmbeddedObject"},
    {FeedRole, "feed"},
    {FigcaptionRole, "Figcaption"},
    {FigureRole, "Figure"},
    {FooterRole, "Footer"},
    {FormRole, "Form"},
    {GridRole, "Grid"},
    {GroupRole, "Group"},
    {HeadingRole, "Heading"},
    {IframePresentationalRole, "IframePresentational"},
    {IframeRole, "Iframe"},
    {IgnoredRole, "Ignored"},
    {ImageMapLinkRole, "ImageMapLink"},
    {ImageMapRole, "ImageMap"},
    {ImageRole, "Image"},
    {InlineTextBoxRole, "InlineTextBox"},
    {InputTimeRole, "InputTime"},
    {LabelRole, "Label"},
    {LegendRole, "Legend"},
    {LinkRole, "Link"},
    {LineBreakRole, "LineBreak"},
    {ListBoxOptionRole, "ListBoxOption"},
    {ListBoxRole, "ListBox"},
    {ListItemRole, "ListItem"},
    {ListMarkerRole, "ListMarker"},
    {ListRole, "List"},
    {LogRole, "Log"},
    {MainRole, "Main"},
    {MarkRole, "Mark"},
    {MarqueeRole, "Marquee"},
    {MathRole, "Math"},
    {MenuBarRole, "MenuBar"},
    {MenuButtonRole, "MenuButton"},
    {MenuItemRole, "MenuItem"},
    {MenuItemCheckBoxRole, "MenuItemCheckBox"},
    {MenuItemRadioRole, "MenuItemRadio"},
    {MenuListOptionRole, "MenuListOption"},
    {MenuListPopupRole, "MenuListPopup"},
    {MenuRole, "Menu"},
    {MeterRole, "Meter"},
    {NavigationRole, "Navigation"},
    {NoneRole, "None"},
    {NoteRole, "Note"},
    {OutlineRole, "Outline"},
    {ParagraphRole, "Paragraph"},
    {PopUpButtonRole, "PopUpButton"},
    {PreRole, "Pre"},
    {PresentationalRole, "Presentational"},
    {ProgressIndicatorRole, "ProgressIndicator"},
    {RadioButtonRole, "RadioButton"},
    {RadioGroupRole, "RadioGroup"},
    {RegionRole, "Region"},
    {RootWebAreaRole, "RootWebArea"},
    {RowHeaderRole, "RowHeader"},
    {RowRole, "Row"},
    {RubyRole, "Ruby"},
    {RulerRole, "Ruler"},
    {SVGRootRole, "SVGRoot"},
    {ScrollAreaRole, "ScrollArea"},
    {ScrollBarRole, "ScrollBar"},
    {SeamlessWebAreaRole, "SeamlessWebArea"},
    {SearchRole, "Search"},
    {SearchBoxRole, "SearchBox"},
    {SliderRole, "Slider"},
    {SliderThumbRole, "SliderThumb"},
    {SpinButtonPartRole, "SpinButtonPart"},
    {SpinButtonRole, "SpinButton"},
    {SplitterRole, "Splitter"},
    {StaticTextRole, "StaticText"},
    {StatusRole, "Status"},
    {SwitchRole, "Switch"},
    {TabGroupRole, "TabGroup"},
    {TabListRole, "TabList"},
    {TabPanelRole, "TabPanel"},
    {TabRole, "Tab"},
    {TableHeaderContainerRole, "TableHeaderContainer"},
    {TableRole, "Table"},
    {TermRole, "Term"},
    {TextFieldRole, "TextField"},
    {TimeRole, "Time"},
    {TimerRole, "Timer"},
    {ToggleButtonRole, "ToggleButton"},
    {ToolbarRole, "Toolbar"},
    {TreeGridRole, "TreeGrid"},
    {TreeItemRole, "TreeItem"},
    {TreeRole, "Tree"},
    {UserInterfaceTooltipRole, "UserInterfaceTooltip"},
    {VideoRole, "Video"},
    {WebAreaRole, "WebArea"},
    {WindowRole, "Window"}};

static_assert(WTF_ARRAY_LENGTH(internalRoles) == NumRoles,
              "Not all internal roles have an entry in internalRoles array");

// Roles which we need to map in the other direction
const RoleEntry reverseRoles[] = {
    {"button", ToggleButtonRole},     {"combobox", PopUpButtonRole},
    {"contentinfo", FooterRole},      {"menuitem", MenuButtonRole},
    {"menuitem", MenuListOptionRole}, {"progressbar", MeterRole},
    {"textbox", TextFieldRole}};

static ARIARoleMap* createARIARoleMap() {
  ARIARoleMap* roleMap = new ARIARoleMap;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(roles); ++i)
    roleMap->set(String(roles[i].ariaRole), roles[i].webcoreRole);
  return roleMap;
}

static Vector<AtomicString>* createRoleNameVector() {
  Vector<AtomicString>* roleNameVector = new Vector<AtomicString>(NumRoles);
  for (int i = 0; i < NumRoles; i++)
    (*roleNameVector)[i] = nullAtom;

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(roles); ++i)
    (*roleNameVector)[roles[i].webcoreRole] = AtomicString(roles[i].ariaRole);

  for (size_t i = 0; i < WTF_ARRAY_LENGTH(reverseRoles); ++i)
    (*roleNameVector)[reverseRoles[i].webcoreRole] =
        AtomicString(reverseRoles[i].ariaRole);

  return roleNameVector;
}

static Vector<AtomicString>* createInternalRoleNameVector() {
  Vector<AtomicString>* internalRoleNameVector =
      new Vector<AtomicString>(NumRoles);
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(internalRoles); i++)
    (*internalRoleNameVector)[internalRoles[i].webcoreRole] =
        AtomicString(internalRoles[i].internalRoleName);

  return internalRoleNameVector;
}

const char* ariaWidgets[] = {
    // From http://www.w3.org/TR/wai-aria/roles#widget_roles
    "alert", "alertdialog", "button", "checkbox", "dialog", "gridcell", "link",
    "log", "marquee", "menuitem", "menuitemcheckbox", "menuitemradio", "option",
    "progressbar", "radio", "scrollbar", "slider", "spinbutton", "status",
    "tab", "tabpanel", "textbox", "timer", "tooltip", "treeitem",
    // Composite user interface widgets.
    // This list is also from the w3.org site referenced above.
    "combobox", "grid", "listbox", "menu", "menubar", "radiogroup", "tablist",
    "tree", "treegrid"};

static ARIAWidgetSet* createARIARoleWidgetSet() {
  ARIAWidgetSet* widgetSet = new HashSet<String, CaseFoldingHash>();
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(ariaWidgets); ++i)
    widgetSet->add(String(ariaWidgets[i]));
  return widgetSet;
}

const char* ariaInteractiveWidgetAttributes[] = {
    // These attributes implicitly indicate the given widget is interactive.
    // From http://www.w3.org/TR/wai-aria/states_and_properties#attrs_widgets
    "aria-activedescendant", "aria-checked",  "aria-controls",
    "aria-disabled",  // If it's disabled, it can be made interactive.
    "aria-expanded",         "aria-haspopup", "aria-multiselectable",
    "aria-pressed",          "aria-required", "aria-selected"};

HTMLDialogElement* getActiveDialogElement(Node* node) {
  return node->document().activeModalDialog();
}

}  // namespace

unsigned AXObject::s_numberOfLiveAXObjects = 0;

AXObject::AXObject(AXObjectCacheImpl& axObjectCache)
    : m_id(0),
      m_haveChildren(false),
      m_role(UnknownRole),
      m_lastKnownIsIgnoredValue(DefaultBehavior),
      m_explicitContainerID(0),
      m_parent(nullptr),
      m_lastModificationCount(-1),
      m_cachedIsIgnored(false),
      m_cachedIsInertOrAriaHidden(false),
      m_cachedIsDescendantOfLeafNode(false),
      m_cachedIsDescendantOfDisabledNode(false),
      m_cachedHasInheritedPresentationalRole(false),
      m_cachedIsPresentationalChild(false),
      m_cachedAncestorExposesActiveDescendant(false),
      m_cachedLiveRegionRoot(nullptr),
      m_axObjectCache(&axObjectCache) {
  ++s_numberOfLiveAXObjects;
}

AXObject::~AXObject() {
  ASSERT(isDetached());
  --s_numberOfLiveAXObjects;
}

void AXObject::detach() {
  // Clear any children and call detachFromParent on them so that
  // no children are left with dangling pointers to their parent.
  clearChildren();

  m_axObjectCache = nullptr;
}

bool AXObject::isDetached() const {
  return !m_axObjectCache;
}

bool AXObject::isARIATextControl() const {
  return ariaRoleAttribute() == TextFieldRole ||
         ariaRoleAttribute() == SearchBoxRole ||
         ariaRoleAttribute() == ComboBoxRole;
}

bool AXObject::isButton() const {
  AccessibilityRole role = roleValue();

  return role == ButtonRole || role == PopUpButtonRole ||
         role == ToggleButtonRole;
}

bool AXObject::isLandmarkRelated() const {
  switch (roleValue()) {
    case ApplicationRole:
    case ArticleRole:
    case BannerRole:
    case ComplementaryRole:
    case ContentInfoRole:
    case FooterRole:
    case FormRole:
    case MainRole:
    case NavigationRole:
    case RegionRole:
    case SearchRole:
      return true;
    default:
      return false;
  }
}

bool AXObject::isMenuRelated() const {
  switch (roleValue()) {
    case MenuRole:
    case MenuBarRole:
    case MenuButtonRole:
    case MenuItemRole:
    case MenuItemCheckBoxRole:
    case MenuItemRadioRole:
      return true;
    default:
      return false;
  }
}

bool AXObject::isPasswordFieldAndShouldHideValue() const {
  Settings* settings = getDocument()->settings();
  if (!settings || settings->getAccessibilityPasswordValuesEnabled())
    return false;

  return isPasswordField();
}

bool AXObject::isClickable() const {
  switch (roleValue()) {
    case ButtonRole:
    case CheckBoxRole:
    case ColorWellRole:
    case ComboBoxRole:
    case ImageMapLinkRole:
    case LinkRole:
    case ListBoxOptionRole:
    case MenuButtonRole:
    case PopUpButtonRole:
    case RadioButtonRole:
    case SpinButtonRole:
    case TabRole:
    case TextFieldRole:
    case ToggleButtonRole:
      return true;
    default:
      return false;
  }
}

bool AXObject::accessibilityIsIgnored() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedIsIgnored;
}

void AXObject::updateCachedAttributeValuesIfNeeded() const {
  if (isDetached())
    return;

  AXObjectCacheImpl& cache = axObjectCache();

  if (cache.modificationCount() == m_lastModificationCount)
    return;

  m_lastModificationCount = cache.modificationCount();
  m_cachedBackgroundColor = computeBackgroundColor();
  m_cachedIsInertOrAriaHidden = computeIsInertOrAriaHidden();
  m_cachedIsDescendantOfLeafNode = (leafNodeAncestor() != 0);
  m_cachedIsDescendantOfDisabledNode = (disabledAncestor() != 0);
  m_cachedHasInheritedPresentationalRole =
      (inheritsPresentationalRoleFrom() != 0);
  m_cachedIsPresentationalChild =
      (ancestorForWhichThisIsAPresentationalChild() != 0);
  m_cachedIsIgnored = computeAccessibilityIsIgnored();
  m_cachedLiveRegionRoot =
      isLiveRegion()
          ? const_cast<AXObject*>(this)
          : (parentObjectIfExists() ? parentObjectIfExists()->liveRegionRoot()
                                    : 0);
  m_cachedAncestorExposesActiveDescendant =
      computeAncestorExposesActiveDescendant();
}

bool AXObject::accessibilityIsIgnoredByDefault(
    IgnoredReasons* ignoredReasons) const {
  return defaultObjectInclusion(ignoredReasons) == IgnoreObject;
}

AXObjectInclusion AXObject::accessibilityPlatformIncludesObject() const {
  if (isMenuListPopup() || isMenuListOption())
    return IncludeObject;

  return DefaultBehavior;
}

AXObjectInclusion AXObject::defaultObjectInclusion(
    IgnoredReasons* ignoredReasons) const {
  if (isInertOrAriaHidden()) {
    if (ignoredReasons)
      computeIsInertOrAriaHidden(ignoredReasons);
    return IgnoreObject;
  }

  if (isPresentationalChild()) {
    if (ignoredReasons) {
      AXObject* ancestor = ancestorForWhichThisIsAPresentationalChild();
      ignoredReasons->push_back(
          IgnoredReason(AXAncestorDisallowsChild, ancestor));
    }
    return IgnoreObject;
  }

  return accessibilityPlatformIncludesObject();
}

bool AXObject::isInertOrAriaHidden() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedIsInertOrAriaHidden;
}

bool AXObject::computeIsInertOrAriaHidden(
    IgnoredReasons* ignoredReasons) const {
  if (getNode()) {
    if (getNode()->isInert()) {
      if (ignoredReasons) {
        HTMLDialogElement* dialog = getActiveDialogElement(getNode());
        if (dialog) {
          AXObject* dialogObject = axObjectCache().getOrCreate(dialog);
          if (dialogObject)
            ignoredReasons->push_back(
                IgnoredReason(AXActiveModalDialog, dialogObject));
          else
            ignoredReasons->push_back(IgnoredReason(AXInert));
        } else {
          // TODO(aboxhall): handle inert attribute if it eventuates
          ignoredReasons->push_back(IgnoredReason(AXInert));
        }
      }
      return true;
    }
  } else {
    AXObject* parent = parentObject();
    if (parent && parent->isInertOrAriaHidden()) {
      if (ignoredReasons)
        parent->computeIsInertOrAriaHidden(ignoredReasons);
      return true;
    }
  }

  const AXObject* hiddenRoot = ariaHiddenRoot();
  if (hiddenRoot) {
    if (ignoredReasons) {
      if (hiddenRoot == this)
        ignoredReasons->push_back(IgnoredReason(AXAriaHidden));
      else
        ignoredReasons->push_back(IgnoredReason(AXAriaHiddenRoot, hiddenRoot));
    }
    return true;
  }

  return false;
}

bool AXObject::isDescendantOfLeafNode() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedIsDescendantOfLeafNode;
}

AXObject* AXObject::leafNodeAncestor() const {
  if (AXObject* parent = parentObject()) {
    if (!parent->canHaveChildren())
      return parent;

    return parent->leafNodeAncestor();
  }

  return 0;
}

const AXObject* AXObject::ariaHiddenRoot() const {
  for (const AXObject* object = this; object; object = object->parentObject()) {
    if (equalIgnoringCase(object->getAttribute(aria_hiddenAttr), "true"))
      return object;
  }

  return 0;
}

bool AXObject::isDescendantOfDisabledNode() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedIsDescendantOfDisabledNode;
}

const AXObject* AXObject::disabledAncestor() const {
  const AtomicString& disabled = getAttribute(aria_disabledAttr);
  if (equalIgnoringCase(disabled, "true"))
    return this;
  if (equalIgnoringCase(disabled, "false"))
    return 0;

  if (AXObject* parent = parentObject())
    return parent->disabledAncestor();

  return 0;
}

bool AXObject::lastKnownIsIgnoredValue() {
  if (m_lastKnownIsIgnoredValue == DefaultBehavior)
    m_lastKnownIsIgnoredValue =
        accessibilityIsIgnored() ? IgnoreObject : IncludeObject;

  return m_lastKnownIsIgnoredValue == IgnoreObject;
}

void AXObject::setLastKnownIsIgnoredValue(bool isIgnored) {
  m_lastKnownIsIgnoredValue = isIgnored ? IgnoreObject : IncludeObject;
}

bool AXObject::hasInheritedPresentationalRole() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedHasInheritedPresentationalRole;
}

bool AXObject::isPresentationalChild() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedIsPresentationalChild;
}

bool AXObject::ancestorExposesActiveDescendant() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedAncestorExposesActiveDescendant;
}

bool AXObject::computeAncestorExposesActiveDescendant() const {
  const AXObject* parent = parentObjectUnignored();
  if (!parent)
    return false;

  if (parent->supportsActiveDescendant() &&
      !parent->getAttribute(aria_activedescendantAttr).isEmpty()) {
    return true;
  }

  return parent->ancestorExposesActiveDescendant();
}

// Simplify whitespace, but preserve a single leading and trailing whitespace
// character if it's present.
// static
String AXObject::collapseWhitespace(const String& str) {
  StringBuilder result;
  if (!str.isEmpty() && isHTMLSpace<UChar>(str[0]))
    result.append(' ');
  result.append(str.simplifyWhiteSpace(isHTMLSpace<UChar>));
  if (!str.isEmpty() && isHTMLSpace<UChar>(str[str.length() - 1]))
    result.append(' ');
  return result.toString();
}

String AXObject::computedName() const {
  AXNameFrom nameFrom;
  AXObject::AXObjectVector nameObjects;
  return name(nameFrom, &nameObjects);
}

String AXObject::name(AXNameFrom& nameFrom,
                      AXObject::AXObjectVector* nameObjects) const {
  HeapHashSet<Member<const AXObject>> visited;
  AXRelatedObjectVector relatedObjects;
  String text = textAlternative(false, false, visited, nameFrom,
                                &relatedObjects, nullptr);

  AccessibilityRole role = roleValue();
  if (!getNode() || (!isHTMLBRElement(getNode()) && role != StaticTextRole &&
                     role != InlineTextBoxRole))
    text = collapseWhitespace(text);

  if (nameObjects) {
    nameObjects->clear();
    for (size_t i = 0; i < relatedObjects.size(); i++)
      nameObjects->push_back(relatedObjects[i]->object);
  }

  return text;
}

String AXObject::name(NameSources* nameSources) const {
  AXObjectSet visited;
  AXNameFrom tmpNameFrom;
  AXRelatedObjectVector tmpRelatedObjects;
  String text = textAlternative(false, false, visited, tmpNameFrom,
                                &tmpRelatedObjects, nameSources);
  text = text.simplifyWhiteSpace(isHTMLSpace<UChar>);
  return text;
}

String AXObject::recursiveTextAlternative(const AXObject& axObj,
                                          bool inAriaLabelledByTraversal,
                                          AXObjectSet& visited) {
  if (visited.contains(&axObj) && !inAriaLabelledByTraversal)
    return String();

  AXNameFrom tmpNameFrom;
  return axObj.textAlternative(true, inAriaLabelledByTraversal, visited,
                               tmpNameFrom, nullptr, nullptr);
}

bool AXObject::isHiddenForTextAlternativeCalculation() const {
  if (equalIgnoringCase(getAttribute(aria_hiddenAttr), "false"))
    return false;

  if (getLayoutObject())
    return getLayoutObject()->style()->visibility() != EVisibility::kVisible;

  // This is an obscure corner case: if a node has no LayoutObject, that means
  // it's not rendered, but we still may be exploring it as part of a text
  // alternative calculation, for example if it was explicitly referenced by
  // aria-labelledby. So we need to explicitly call the style resolver to check
  // whether it's invisible or display:none, rather than relying on the style
  // cached in the LayoutObject.
  Document* doc = getDocument();
  if (doc && doc->frame() && getNode() && getNode()->isElementNode()) {
    RefPtr<ComputedStyle> style =
        doc->ensureStyleResolver().styleForElement(toElement(getNode()));
    return style->display() == EDisplay::None ||
           style->visibility() != EVisibility::kVisible;
  }

  return false;
}

String AXObject::ariaTextAlternative(bool recursive,
                                     bool inAriaLabelledByTraversal,
                                     AXObjectSet& visited,
                                     AXNameFrom& nameFrom,
                                     AXRelatedObjectVector* relatedObjects,
                                     NameSources* nameSources,
                                     bool* foundTextAlternative) const {
  String textAlternative;
  bool alreadyVisited = visited.contains(this);
  visited.add(this);

  // Step 2A from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  if (!inAriaLabelledByTraversal && isHiddenForTextAlternativeCalculation()) {
    *foundTextAlternative = true;
    return String();
  }

  // Step 2B from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  if (!inAriaLabelledByTraversal && !alreadyVisited) {
    const QualifiedName& attr =
        hasAttribute(aria_labeledbyAttr) && !hasAttribute(aria_labelledbyAttr)
            ? aria_labeledbyAttr
            : aria_labelledbyAttr;
    nameFrom = AXNameFromRelatedElement;
    if (nameSources) {
      nameSources->push_back(NameSource(*foundTextAlternative, attr));
      nameSources->back().type = nameFrom;
    }

    const AtomicString& ariaLabelledby = getAttribute(attr);
    if (!ariaLabelledby.isNull()) {
      if (nameSources)
        nameSources->back().attributeValue = ariaLabelledby;

      // Operate on a copy of |visited| so that if |nameSources| is not null,
      // the set of visited objects is preserved unmodified for future
      // calculations.
      AXObjectSet visitedCopy = visited;
      textAlternative = textFromAriaLabelledby(visitedCopy, relatedObjects);
      if (!textAlternative.isNull()) {
        if (nameSources) {
          NameSource& source = nameSources->back();
          source.type = nameFrom;
          source.relatedObjects = *relatedObjects;
          source.text = textAlternative;
          *foundTextAlternative = true;
        } else {
          *foundTextAlternative = true;
          return textAlternative;
        }
      } else if (nameSources) {
        nameSources->back().invalid = true;
      }
    }
  }

  // Step 2C from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  nameFrom = AXNameFromAttribute;
  if (nameSources) {
    nameSources->push_back(NameSource(*foundTextAlternative, aria_labelAttr));
    nameSources->back().type = nameFrom;
  }
  const AtomicString& ariaLabel = getAttribute(aria_labelAttr);
  if (!ariaLabel.isEmpty()) {
    textAlternative = ariaLabel;

    if (nameSources) {
      NameSource& source = nameSources->back();
      source.text = textAlternative;
      source.attributeValue = ariaLabel;
      *foundTextAlternative = true;
    } else {
      *foundTextAlternative = true;
      return textAlternative;
    }
  }

  return textAlternative;
}

String AXObject::textFromElements(bool inAriaLabelledbyTraversal,
                                  AXObjectSet& visited,
                                  HeapVector<Member<Element>>& elements,
                                  AXRelatedObjectVector* relatedObjects) const {
  StringBuilder accumulatedText;
  bool foundValidElement = false;
  AXRelatedObjectVector localRelatedObjects;

  for (const auto& element : elements) {
    AXObject* axElement = axObjectCache().getOrCreate(element);
    if (axElement) {
      foundValidElement = true;

      String result = recursiveTextAlternative(
          *axElement, inAriaLabelledbyTraversal, visited);
      localRelatedObjects.push_back(
          new NameSourceRelatedObject(axElement, result));
      if (!result.isEmpty()) {
        if (!accumulatedText.isEmpty())
          accumulatedText.append(' ');
        accumulatedText.append(result);
      }
    }
  }
  if (!foundValidElement)
    return String();
  if (relatedObjects)
    *relatedObjects = localRelatedObjects;
  return accumulatedText.toString();
}

void AXObject::tokenVectorFromAttribute(Vector<String>& tokens,
                                        const QualifiedName& attribute) const {
  Node* node = this->getNode();
  if (!node || !node->isElementNode())
    return;

  String attributeValue = getAttribute(attribute).getString();
  if (attributeValue.isEmpty())
    return;

  attributeValue.simplifyWhiteSpace();
  attributeValue.split(' ', tokens);
}

void AXObject::elementsFromAttribute(HeapVector<Member<Element>>& elements,
                                     const QualifiedName& attribute) const {
  Vector<String> ids;
  tokenVectorFromAttribute(ids, attribute);
  if (ids.isEmpty())
    return;

  TreeScope& scope = getNode()->treeScope();
  for (const auto& id : ids) {
    if (Element* idElement = scope.getElementById(AtomicString(id)))
      elements.push_back(idElement);
  }
}

void AXObject::ariaLabelledbyElementVector(
    HeapVector<Member<Element>>& elements) const {
  // Try both spellings, but prefer aria-labelledby, which is the official spec.
  elementsFromAttribute(elements, aria_labelledbyAttr);
  if (!elements.size())
    elementsFromAttribute(elements, aria_labeledbyAttr);
}

String AXObject::textFromAriaLabelledby(
    AXObjectSet& visited,
    AXRelatedObjectVector* relatedObjects) const {
  HeapVector<Member<Element>> elements;
  ariaLabelledbyElementVector(elements);
  return textFromElements(true, visited, elements, relatedObjects);
}

String AXObject::textFromAriaDescribedby(
    AXRelatedObjectVector* relatedObjects) const {
  AXObjectSet visited;
  HeapVector<Member<Element>> elements;
  elementsFromAttribute(elements, aria_describedbyAttr);
  return textFromElements(true, visited, elements, relatedObjects);
}

RGBA32 AXObject::backgroundColor() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedBackgroundColor;
}

AccessibilityOrientation AXObject::orientation() const {
  // In ARIA 1.1, the default value for aria-orientation changed from
  // horizontal to undefined.
  return AccessibilityOrientationUndefined;
}

AXSupportedAction AXObject::action() const {
  if (!actionElement())
    return AXSupportedAction::None;

  switch (roleValue()) {
    case ButtonRole:
    case ToggleButtonRole:
      return AXSupportedAction::Press;
    case TextFieldRole:
      return AXSupportedAction::Activate;
    case RadioButtonRole:
      return AXSupportedAction::Select;
    case CheckBoxRole:
    case SwitchRole:
      return isChecked() ? AXSupportedAction::Check
                         : AXSupportedAction::Uncheck;
    case LinkRole:
      return AXSupportedAction::Jump;
    case PopUpButtonRole:
      return AXSupportedAction::Open;
    default:
      return AXSupportedAction::Click;
  }
}

AccessibilityButtonState AXObject::checkboxOrRadioValue() const {
  const AtomicString& checkedAttribute = getAttribute(aria_checkedAttr);
  if (equalIgnoringCase(checkedAttribute, "true"))
    return ButtonStateOn;

  if (equalIgnoringCase(checkedAttribute, "mixed")) {
    // Only checkboxes should support the mixed state.
    AccessibilityRole role = ariaRoleAttribute();
    if (role == CheckBoxRole || role == MenuItemCheckBoxRole)
      return ButtonStateMixed;
  }

  return ButtonStateOff;
}

bool AXObject::isMultiline() const {
  Node* node = this->getNode();
  if (!node)
    return false;

  if (isHTMLTextAreaElement(*node))
    return true;

  if (hasEditableStyle(*node))
    return true;

  if (!isNativeTextControl() && !isNonNativeTextControl())
    return false;

  return equalIgnoringCase(getAttribute(aria_multilineAttr), "true");
}

bool AXObject::ariaPressedIsPresent() const {
  return !getAttribute(aria_pressedAttr).isEmpty();
}

bool AXObject::supportsActiveDescendant() const {
  // According to the ARIA Spec, all ARIA composite widgets, ARIA text boxes
  // and ARIA groups should be able to expose an active descendant.
  // Implicitly, <input> and <textarea> elements should also have this ability.
  switch (ariaRoleAttribute()) {
    case ComboBoxRole:
    case GridRole:
    case GroupRole:
    case ListBoxRole:
    case MenuRole:
    case MenuBarRole:
    case RadioGroupRole:
    case RowRole:
    case SearchBoxRole:
    case TabListRole:
    case TextFieldRole:
    case ToolbarRole:
    case TreeRole:
    case TreeGridRole:
      return true;
    default:
      return false;
  }
}

bool AXObject::supportsARIAAttributes() const {
  return isLiveRegion() || supportsARIADragging() || supportsARIADropping() ||
         supportsARIAFlowTo() || supportsARIAOwns() ||
         hasAttribute(aria_labelAttr);
}

bool AXObject::supportsRangeValue() const {
  return isProgressIndicator() || isMeter() || isSlider() || isScrollbar() ||
         isSpinButton();
}

bool AXObject::supportsSetSizeAndPosInSet() const {
  AXObject* parent = parentObject();
  if (!parent)
    return false;

  int role = roleValue();
  int parentRole = parent->roleValue();

  if ((role == ListBoxOptionRole && parentRole == ListBoxRole) ||
      (role == ListItemRole && parentRole == ListRole) ||
      (role == MenuItemRole && parentRole == MenuRole) ||
      (role == RadioButtonRole) ||
      (role == TabRole && parentRole == TabListRole) ||
      (role == TreeItemRole && parentRole == TreeRole) ||
      (role == TreeItemRole && parentRole == GroupRole)) {
    return true;
  }

  return false;
}

int AXObject::indexInParent() const {
  if (!parentObject())
    return 0;

  const auto& siblings = parentObject()->children();
  int childCount = siblings.size();

  for (int index = 0; index < childCount; ++index) {
    if (siblings[index].get() == this) {
      return index;
    }
  }
  return 0;
}

bool AXObject::isLiveRegion() const {
  const AtomicString& liveRegion = liveRegionStatus();
  return equalIgnoringCase(liveRegion, "polite") ||
         equalIgnoringCase(liveRegion, "assertive");
}

AXObject* AXObject::liveRegionRoot() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedLiveRegionRoot;
}

const AtomicString& AXObject::containerLiveRegionStatus() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedLiveRegionRoot ? m_cachedLiveRegionRoot->liveRegionStatus()
                                : nullAtom;
}

const AtomicString& AXObject::containerLiveRegionRelevant() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedLiveRegionRoot ? m_cachedLiveRegionRoot->liveRegionRelevant()
                                : nullAtom;
}

bool AXObject::containerLiveRegionAtomic() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedLiveRegionRoot && m_cachedLiveRegionRoot->liveRegionAtomic();
}

bool AXObject::containerLiveRegionBusy() const {
  updateCachedAttributeValuesIfNeeded();
  return m_cachedLiveRegionRoot && m_cachedLiveRegionRoot->liveRegionBusy();
}

AXObject* AXObject::elementAccessibilityHitTest(const IntPoint& point) const {
  // Check if there are any mock elements that need to be handled.
  for (const auto& child : m_children) {
    if (child->isMockObject() &&
        child->getBoundsInFrameCoordinates().contains(point))
      return child->elementAccessibilityHitTest(point);
  }

  return const_cast<AXObject*>(this);
}

const AXObject::AXObjectVector& AXObject::children() {
  updateChildrenIfNecessary();

  return m_children;
}

AXObject* AXObject::parentObject() const {
  if (isDetached())
    return 0;

  if (m_parent)
    return m_parent;

  if (axObjectCache().isAriaOwned(this))
    return axObjectCache().getAriaOwnedParent(this);

  return computeParent();
}

AXObject* AXObject::parentObjectIfExists() const {
  if (isDetached())
    return 0;

  if (m_parent)
    return m_parent;

  return computeParentIfExists();
}

AXObject* AXObject::parentObjectUnignored() const {
  AXObject* parent;
  for (parent = parentObject(); parent && parent->accessibilityIsIgnored();
       parent = parent->parentObject()) {
  }

  return parent;
}

void AXObject::updateChildrenIfNecessary() {
  if (!hasChildren())
    addChildren();
}

void AXObject::clearChildren() {
  // Detach all weak pointers from objects to their parents.
  for (const auto& child : m_children)
    child->detachFromParent();

  m_children.clear();
  m_haveChildren = false;
}

Document* AXObject::getDocument() const {
  FrameView* frameView = documentFrameView();
  if (!frameView)
    return 0;

  return frameView->frame().document();
}

FrameView* AXObject::documentFrameView() const {
  const AXObject* object = this;
  while (object && !object->isAXLayoutObject())
    object = object->parentObject();

  if (!object)
    return 0;

  return object->documentFrameView();
}

String AXObject::language() const {
  const AtomicString& lang = getAttribute(langAttr);
  if (!lang.isEmpty())
    return lang;

  AXObject* parent = parentObject();

  // As a last resort, fall back to the content language specified in the meta
  // tag.
  if (!parent) {
    Document* doc = getDocument();
    if (doc)
      return doc->contentLanguage();
    return nullAtom;
  }

  return parent->language();
}

bool AXObject::hasAttribute(const QualifiedName& attribute) const {
  Node* elementNode = getNode();
  if (!elementNode)
    return false;

  if (!elementNode->isElementNode())
    return false;

  Element* element = toElement(elementNode);
  return element->fastHasAttribute(attribute);
}

const AtomicString& AXObject::getAttribute(
    const QualifiedName& attribute) const {
  Node* elementNode = getNode();
  if (!elementNode)
    return nullAtom;

  if (!elementNode->isElementNode())
    return nullAtom;

  Element* element = toElement(elementNode);
  return element->fastGetAttribute(attribute);
}

//
// Scrollable containers.
//

bool AXObject::isScrollableContainer() const {
  return !!getScrollableAreaIfScrollable();
}

IntPoint AXObject::getScrollOffset() const {
  ScrollableArea* area = getScrollableAreaIfScrollable();
  if (!area)
    return IntPoint();

  return IntPoint(area->scrollOffsetInt().width(),
                  area->scrollOffsetInt().height());
}

IntPoint AXObject::minimumScrollOffset() const {
  ScrollableArea* area = getScrollableAreaIfScrollable();
  if (!area)
    return IntPoint();

  return IntPoint(area->minimumScrollOffsetInt().width(),
                  area->minimumScrollOffsetInt().height());
}

IntPoint AXObject::maximumScrollOffset() const {
  ScrollableArea* area = getScrollableAreaIfScrollable();
  if (!area)
    return IntPoint();

  return IntPoint(area->maximumScrollOffsetInt().width(),
                  area->maximumScrollOffsetInt().height());
}

void AXObject::setScrollOffset(const IntPoint& offset) const {
  ScrollableArea* area = getScrollableAreaIfScrollable();
  if (!area)
    return;

  // TODO(bokan): This should potentially be a UserScroll.
  area->setScrollOffset(ScrollOffset(offset.x(), offset.y()),
                        ProgrammaticScroll);
}

void AXObject::getRelativeBounds(AXObject** outContainer,
                                 FloatRect& outBoundsInContainer,
                                 SkMatrix44& outContainerTransform) const {
  *outContainer = nullptr;
  outBoundsInContainer = FloatRect();
  outContainerTransform.setIdentity();

  // First check if it has explicit bounds, for example if this element is tied
  // to a canvas path. When explicit coordinates are provided, the ID of the
  // explicit container element that the coordinates are relative to must be
  // provided too.
  if (!m_explicitElementRect.isEmpty()) {
    *outContainer = axObjectCache().objectFromAXID(m_explicitContainerID);
    if (*outContainer) {
      outBoundsInContainer = FloatRect(m_explicitElementRect);
      return;
    }
  }

  LayoutObject* layoutObject = layoutObjectForRelativeBounds();
  if (!layoutObject)
    return;

  if (isWebArea()) {
    if (layoutObject->frame()->view())
      outBoundsInContainer.setSize(
          FloatSize(layoutObject->frame()->view()->contentsSize()));
    return;
  }

  // First compute the container. The container must be an ancestor in the
  // accessibility tree, and its LayoutObject must be an ancestor in the layout
  // tree. Get the first such ancestor that's either scrollable or has a paint
  // layer.
  AXObject* container = parentObjectUnignored();
  LayoutObject* containerLayoutObject = nullptr;
  while (container) {
    containerLayoutObject = container->getLayoutObject();
    if (containerLayoutObject && containerLayoutObject->isBoxModelObject() &&
        layoutObject->isDescendantOf(containerLayoutObject)) {
      if (container->isScrollableContainer() ||
          containerLayoutObject->hasLayer())
        break;
    }

    container = container->parentObjectUnignored();
  }

  if (!container)
    return;
  *outContainer = container;
  outBoundsInContainer = layoutObject->localBoundingBoxRectForAccessibility();

  // If the container has a scroll offset, subtract that out because we want our
  // bounds to be relative to the *unscrolled* position of the container object.
  ScrollableArea* scrollableArea = container->getScrollableAreaIfScrollable();
  if (scrollableArea && !container->isWebArea()) {
    ScrollOffset scrollOffset = scrollableArea->getScrollOffset();
    outBoundsInContainer.move(scrollOffset);
  }

  // Compute the transform between the container's coordinate space and this
  // object.  If the transform is just a simple translation, apply that to the
  // bounding box, but if it's a non-trivial transformation like a rotation,
  // scaling, etc. then return the full matrix instead.
  TransformationMatrix transform = layoutObject->localToAncestorTransform(
      toLayoutBoxModelObject(containerLayoutObject));
  if (transform.isIdentityOr2DTranslation()) {
    outBoundsInContainer.move(transform.to2DTranslation());
  } else {
    outContainerTransform = TransformationMatrix::toSkMatrix44(transform);
  }
}

LayoutRect AXObject::getBoundsInFrameCoordinates() const {
  AXObject* container = nullptr;
  FloatRect bounds;
  SkMatrix44 transform;
  getRelativeBounds(&container, bounds, transform);
  FloatRect computedBounds(0, 0, bounds.width(), bounds.height());
  while (container && container != this) {
    computedBounds.move(bounds.x(), bounds.y());
    if (!container->isWebArea()) {
      computedBounds.move(-container->getScrollOffset().x(),
                          -container->getScrollOffset().y());
    }
    if (!transform.isIdentity()) {
      TransformationMatrix transformationMatrix(transform);
      transformationMatrix.mapRect(computedBounds);
    }
    container->getRelativeBounds(&container, bounds, transform);
  }
  return LayoutRect(computedBounds);
}

//
// Modify or take an action on an object.
//

bool AXObject::press() const {
  Element* actionElem = actionElement();
  if (!actionElem)
    return false;
  UserGestureIndicator gestureIndicator(DocumentUserGestureToken::create(
      &actionElem->document(), UserGestureToken::NewGesture));
  actionElem->accessKeyAction(true);
  return true;
}

void AXObject::scrollToMakeVisible() const {
  IntRect objectRect = pixelSnappedIntRect(getBoundsInFrameCoordinates());
  objectRect.setLocation(IntPoint());
  scrollToMakeVisibleWithSubFocus(objectRect);
}

// This is a 1-dimensional scroll offset helper function that's applied
// separately in the horizontal and vertical directions, because the
// logic is the same. The goal is to compute the best scroll offset
// in order to make an object visible within a viewport.
//
// If the object is already fully visible, returns the same scroll
// offset.
//
// In case the whole object cannot fit, you can specify a
// subfocus - a smaller region within the object that should
// be prioritized. If the whole object can fit, the subfocus is
// ignored.
//
// If possible, the object and subfocus are centered within the
// viewport.
//
// Example 1: the object is already visible, so nothing happens.
//   +----------Viewport---------+
//                 +---Object---+
//                 +--SubFocus--+
//
// Example 2: the object is not fully visible, so it's centered
// within the viewport.
//   Before:
//   +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
//   After:
//                 +----------Viewport---------+
//                         +---Object---+
//                         +--SubFocus--+
//
// Example 3: the object is larger than the viewport, so the
// viewport moves to show as much of the object as possible,
// while also trying to center the subfocus.
//   Before:
//   +----------Viewport---------+
//     +---------------Object--------------+
//                         +-SubFocus-+
//
//   After:
//             +----------Viewport---------+
//     +---------------Object--------------+
//                         +-SubFocus-+
//
// When constraints cannot be fully satisfied, the min
// (left/top) position takes precedence over the max (right/bottom).
//
// Note that the return value represents the ideal new scroll offset.
// This may be out of range - the calling function should clip this
// to the available range.
static int computeBestScrollOffset(int currentScrollOffset,
                                   int subfocusMin,
                                   int subfocusMax,
                                   int objectMin,
                                   int objectMax,
                                   int viewportMin,
                                   int viewportMax) {
  int viewportSize = viewportMax - viewportMin;

  // If the object size is larger than the viewport size, consider
  // only a portion that's as large as the viewport, centering on
  // the subfocus as much as possible.
  if (objectMax - objectMin > viewportSize) {
    // Since it's impossible to fit the whole object in the
    // viewport, exit now if the subfocus is already within the viewport.
    if (subfocusMin - currentScrollOffset >= viewportMin &&
        subfocusMax - currentScrollOffset <= viewportMax)
      return currentScrollOffset;

    // Subfocus must be within focus.
    subfocusMin = std::max(subfocusMin, objectMin);
    subfocusMax = std::min(subfocusMax, objectMax);

    // Subfocus must be no larger than the viewport size; favor top/left.
    if (subfocusMax - subfocusMin > viewportSize)
      subfocusMax = subfocusMin + viewportSize;

    // Compute the size of an object centered on the subfocus, the size of the
    // viewport.
    int centeredObjectMin = (subfocusMin + subfocusMax - viewportSize) / 2;
    int centeredObjectMax = centeredObjectMin + viewportSize;

    objectMin = std::max(objectMin, centeredObjectMin);
    objectMax = std::min(objectMax, centeredObjectMax);
  }

  // Exit now if the focus is already within the viewport.
  if (objectMin - currentScrollOffset >= viewportMin &&
      objectMax - currentScrollOffset <= viewportMax)
    return currentScrollOffset;

  // Center the object in the viewport.
  return (objectMin + objectMax - viewportMin - viewportMax) / 2;
}

void AXObject::scrollToMakeVisibleWithSubFocus(const IntRect& subfocus) const {
  // Search up the parent chain until we find the first one that's scrollable.
  const AXObject* scrollParent = parentObject() ? parentObject() : this;
  ScrollableArea* scrollableArea = 0;
  while (scrollParent) {
    scrollableArea = scrollParent->getScrollableAreaIfScrollable();
    if (scrollableArea)
      break;
    scrollParent = scrollParent->parentObject();
  }
  if (!scrollParent || !scrollableArea)
    return;

  IntRect objectRect = pixelSnappedIntRect(getBoundsInFrameCoordinates());
  IntSize scrollOffset = scrollableArea->scrollOffsetInt();
  IntRect scrollVisibleRect = scrollableArea->visibleContentRect();

  // Convert the object rect into local coordinates.
  if (!scrollParent->isWebArea()) {
    objectRect.moveBy(IntPoint(scrollOffset));
    objectRect.moveBy(
        -pixelSnappedIntRect(scrollParent->getBoundsInFrameCoordinates())
             .location());
  }

  int desiredX = computeBestScrollOffset(
      scrollOffset.width(), objectRect.x() + subfocus.x(),
      objectRect.x() + subfocus.maxX(), objectRect.x(), objectRect.maxX(), 0,
      scrollVisibleRect.width());
  int desiredY = computeBestScrollOffset(
      scrollOffset.height(), objectRect.y() + subfocus.y(),
      objectRect.y() + subfocus.maxY(), objectRect.y(), objectRect.maxY(), 0,
      scrollVisibleRect.height());

  scrollParent->setScrollOffset(IntPoint(desiredX, desiredY));

  // Convert the subfocus into the coordinates of the scroll parent.
  IntRect newSubfocus = subfocus;
  IntRect newElementRect = pixelSnappedIntRect(getBoundsInFrameCoordinates());
  IntRect scrollParentRect =
      pixelSnappedIntRect(scrollParent->getBoundsInFrameCoordinates());
  newSubfocus.move(newElementRect.x(), newElementRect.y());
  newSubfocus.move(-scrollParentRect.x(), -scrollParentRect.y());

  // Recursively make sure the scroll parent itself is visible.
  if (scrollParent->parentObject())
    scrollParent->scrollToMakeVisibleWithSubFocus(newSubfocus);
}

void AXObject::scrollToGlobalPoint(const IntPoint& globalPoint) const {
  // Search up the parent chain and create a vector of all scrollable parent
  // objects and ending with this object itself.
  HeapVector<Member<const AXObject>> objects;
  AXObject* parentObject;
  for (parentObject = this->parentObject(); parentObject;
       parentObject = parentObject->parentObject()) {
    if (parentObject->getScrollableAreaIfScrollable())
      objects.prepend(parentObject);
  }
  objects.push_back(this);

  // Start with the outermost scrollable (the main window) and try to scroll the
  // next innermost object to the given point.
  int offsetX = 0, offsetY = 0;
  IntPoint point = globalPoint;
  size_t levels = objects.size() - 1;
  for (size_t i = 0; i < levels; i++) {
    const AXObject* outer = objects[i];
    const AXObject* inner = objects[i + 1];
    ScrollableArea* scrollableArea = outer->getScrollableAreaIfScrollable();

    IntRect innerRect =
        inner->isWebArea()
            ? pixelSnappedIntRect(
                  inner->parentObject()->getBoundsInFrameCoordinates())
            : pixelSnappedIntRect(inner->getBoundsInFrameCoordinates());
    IntRect objectRect = innerRect;
    IntSize scrollOffset = scrollableArea->scrollOffsetInt();

    // Convert the object rect into local coordinates.
    objectRect.move(offsetX, offsetY);
    if (!outer->isWebArea())
      objectRect.move(scrollOffset.width(), scrollOffset.height());

    int desiredX = computeBestScrollOffset(0, objectRect.x(), objectRect.maxX(),
                                           objectRect.x(), objectRect.maxX(),
                                           point.x(), point.x());
    int desiredY = computeBestScrollOffset(0, objectRect.y(), objectRect.maxY(),
                                           objectRect.y(), objectRect.maxY(),
                                           point.y(), point.y());
    outer->setScrollOffset(IntPoint(desiredX, desiredY));

    if (outer->isWebArea() && !inner->isWebArea()) {
      // If outer object we just scrolled is a web area (frame) but the inner
      // object is not, keep track of the coordinate transformation to apply to
      // future nested calculations.
      scrollOffset = scrollableArea->scrollOffsetInt();
      offsetX -= (scrollOffset.width() + point.x());
      offsetY -= (scrollOffset.height() + point.y());
      point.move(scrollOffset.width() - innerRect.width(),
                 scrollOffset.height() - innerRect.y());
    } else if (inner->isWebArea()) {
      // Otherwise, if the inner object is a web area, reset the coordinate
      // transformation.
      offsetX = 0;
      offsetY = 0;
    }
  }
}

void AXObject::setSequentialFocusNavigationStartingPoint() {
  // Call it on the nearest ancestor that overrides this with a specific
  // implementation.
  if (parentObject())
    parentObject()->setSequentialFocusNavigationStartingPoint();
}

void AXObject::notifyIfIgnoredValueChanged() {
  bool isIgnored = accessibilityIsIgnored();
  if (lastKnownIsIgnoredValue() != isIgnored) {
    axObjectCache().childrenChanged(parentObject());
    setLastKnownIsIgnoredValue(isIgnored);
  }
}

void AXObject::selectionChanged() {
  if (AXObject* parent = parentObjectIfExists())
    parent->selectionChanged();
}

int AXObject::lineForPosition(const VisiblePosition& position) const {
  if (position.isNull() || !getNode())
    return -1;

  // If the position is not in the same editable region as this AX object,
  // return -1.
  Node* containerNode = position.deepEquivalent().computeContainerNode();
  if (!containerNode->isShadowIncludingInclusiveAncestorOf(getNode()) &&
      !getNode()->isShadowIncludingInclusiveAncestorOf(containerNode))
    return -1;

  int lineCount = -1;
  VisiblePosition currentPosition = position;
  VisiblePosition previousPosition;

  // Move up until we get to the top.
  // FIXME: This only takes us to the top of the rootEditableElement, not the
  // top of the top document.
  do {
    previousPosition = currentPosition;
    currentPosition =
        previousLinePosition(currentPosition, LayoutUnit(), HasEditableAXRole);
    ++lineCount;
  } while (currentPosition.isNotNull() &&
           !inSameLine(currentPosition, previousPosition));

  return lineCount;
}

bool AXObject::isARIAControl(AccessibilityRole ariaRole) {
  return isARIAInput(ariaRole) || ariaRole == ButtonRole ||
         ariaRole == ComboBoxRole || ariaRole == SliderRole;
}

bool AXObject::isARIAInput(AccessibilityRole ariaRole) {
  return ariaRole == RadioButtonRole || ariaRole == CheckBoxRole ||
         ariaRole == TextFieldRole || ariaRole == SwitchRole ||
         ariaRole == SearchBoxRole;
}

AccessibilityRole AXObject::ariaRoleToWebCoreRole(const String& value) {
  ASSERT(!value.isEmpty());

  static const ARIARoleMap* roleMap = createARIARoleMap();

  Vector<String> roleVector;
  value.split(' ', roleVector);
  AccessibilityRole role = UnknownRole;
  for (const auto& child : roleVector) {
    role = roleMap->get(child);
    if (role)
      return role;
  }

  return role;
}

bool AXObject::isInsideFocusableElementOrARIAWidget(const Node& node) {
  const Node* curNode = &node;
  do {
    if (curNode->isElementNode()) {
      const Element* element = toElement(curNode);
      if (element->isFocusable())
        return true;
      String role = element->getAttribute("role");
      if (!role.isEmpty() && AXObject::includesARIAWidgetRole(role))
        return true;
      if (hasInteractiveARIAAttribute(*element))
        return true;
    }
    curNode = curNode->parentNode();
  } while (curNode && !isHTMLBodyElement(node));
  return false;
}

bool AXObject::hasInteractiveARIAAttribute(const Element& element) {
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(ariaInteractiveWidgetAttributes);
       ++i) {
    const char* attribute = ariaInteractiveWidgetAttributes[i];
    if (element.hasAttribute(attribute)) {
      return true;
    }
  }
  return false;
}

bool AXObject::includesARIAWidgetRole(const String& role) {
  static const HashSet<String, CaseFoldingHash>* roleSet =
      createARIARoleWidgetSet();

  Vector<String> roleVector;
  role.split(' ', roleVector);
  for (const auto& child : roleVector) {
    if (roleSet->contains(child))
      return true;
  }
  return false;
}

bool AXObject::nameFromContents() const {
  switch (roleValue()) {
    case ButtonRole:
    case CheckBoxRole:
    case DirectoryRole:
    case DisclosureTriangleRole:
    case HeadingRole:
    case LineBreakRole:
    case LinkRole:
    case ListBoxOptionRole:
    case ListItemRole:
    case MenuItemRole:
    case MenuItemCheckBoxRole:
    case MenuItemRadioRole:
    case MenuListOptionRole:
    case PopUpButtonRole:
    case RadioButtonRole:
    case StaticTextRole:
    case StatusRole:
    case SwitchRole:
    case TabRole:
    case ToggleButtonRole:
    case TreeItemRole:
      return true;
    default:
      return false;
  }
}

AccessibilityRole AXObject::buttonRoleType() const {
  // If aria-pressed is present, then it should be exposed as a toggle button.
  // http://www.w3.org/TR/wai-aria/states_and_properties#aria-pressed
  if (ariaPressedIsPresent())
    return ToggleButtonRole;
  if (ariaHasPopup())
    return PopUpButtonRole;
  // We don't contemplate RadioButtonRole, as it depends on the input
  // type.

  return ButtonRole;
}

const AtomicString& AXObject::roleName(AccessibilityRole role) {
  static const Vector<AtomicString>* roleNameVector = createRoleNameVector();

  return roleNameVector->at(role);
}

const AtomicString& AXObject::internalRoleName(AccessibilityRole role) {
  static const Vector<AtomicString>* internalRoleNameVector =
      createInternalRoleNameVector();

  return internalRoleNameVector->at(role);
}

DEFINE_TRACE(AXObject) {
  visitor->trace(m_children);
  visitor->trace(m_parent);
  visitor->trace(m_cachedLiveRegionRoot);
  visitor->trace(m_axObjectCache);
}

}  // namespace blink

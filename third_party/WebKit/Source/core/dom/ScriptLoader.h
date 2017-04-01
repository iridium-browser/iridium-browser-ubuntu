/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ScriptLoader_h
#define ScriptLoader_h

#include "core/CoreExport.h"
#include "core/dom/PendingScript.h"
#include "core/dom/ScriptRunner.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceClient.h"
#include "core/loader/resource/ScriptResource.h"
#include "wtf/text/TextPosition.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Element;
class ScriptLoaderClient;
class ScriptSourceCode;
class LocalFrame;

class CORE_EXPORT ScriptLoader : public GarbageCollectedFinalized<ScriptLoader>,
                                 public PendingScriptClient {
  USING_GARBAGE_COLLECTED_MIXIN(ScriptLoader);

 public:
  static ScriptLoader* create(Element* element,
                              bool createdByParser,
                              bool isEvaluated,
                              bool createdDuringDocumentWrite = false) {
    return new ScriptLoader(element, createdByParser, isEvaluated,
                            createdDuringDocumentWrite);
  }

  ~ScriptLoader() override;
  DECLARE_VIRTUAL_TRACE();

  Element* element() const { return m_element; }

  enum LegacyTypeSupport {
    DisallowLegacyTypeInTypeAttribute,
    AllowLegacyTypeInTypeAttribute
  };
  static bool isValidScriptTypeAndLanguage(
      const String& typeAttributeValue,
      const String& languageAttributeValue,
      LegacyTypeSupport supportLegacyTypes);

  bool prepareScript(
      const TextPosition& scriptStartPosition = TextPosition::minimumPosition(),
      LegacyTypeSupport = DisallowLegacyTypeInTypeAttribute);

  String scriptCharset() const { return m_characterEncoding; }
  String scriptContent() const;
  // Returns false if and only if execution was blocked.
  bool executeScript(const ScriptSourceCode&);
  virtual void execute();

  // XML parser calls these
  void dispatchLoadEvent();
  void dispatchErrorEvent();
  bool isScriptTypeSupported(LegacyTypeSupport) const;

  bool haveFiredLoadEvent() const { return m_haveFiredLoad; }
  bool willBeParserExecuted() const { return m_willBeParserExecuted; }
  bool readyToBeParserExecuted() const { return m_readyToBeParserExecuted; }
  bool willExecuteWhenDocumentFinishedParsing() const {
    return m_willExecuteWhenDocumentFinishedParsing;
  }
  ScriptResource* resource() { return m_resource.get(); }

  void setHaveFiredLoadEvent(bool haveFiredLoad) {
    m_haveFiredLoad = haveFiredLoad;
  }
  bool isParserInserted() const { return m_parserInserted; }
  bool alreadyStarted() const { return m_alreadyStarted; }
  bool forceAsync() const { return m_forceAsync; }

  // Helper functions used by our parent classes.
  void didNotifySubtreeInsertionsToDocument();
  void childrenChanged();
  void handleSourceAttribute(const String& sourceUrl);
  void handleAsyncAttribute();

  virtual bool isReady() const {
    return m_pendingScript && m_pendingScript->isReady();
  }
  bool errorOccurred() const {
    return m_pendingScript && m_pendingScript->errorOccurred();
  }

  // Clears the connection to the PendingScript (and Element and Resource).
  void detach();

  bool wasCreatedDuringDocumentWrite() { return m_createdDuringDocumentWrite; }

  bool disallowedFetchForDocWrittenScript() {
    return m_documentWriteIntervention ==
           DocumentWriteIntervention::DoNotFetchDocWrittenScript;
  }
  void setFetchDocWrittenScriptDeferIdle();

 protected:
  ScriptLoader(Element*,
               bool createdByParser,
               bool isEvaluated,
               bool createdDuringDocumentWrite);

 private:
  bool ignoresLoadRequest() const;
  bool isScriptForEventSupported() const;
  void logScriptMIMEType(LocalFrame*, ScriptResource*, const String&);

  bool fetchScript(const String& sourceUrl, FetchRequest::DeferOption);
  bool doExecuteScript(const ScriptSourceCode&);

  ScriptLoaderClient* client() const;

  // PendingScriptClient
  void pendingScriptFinished(PendingScript*) override;

  Member<Element> m_element;
  Member<ScriptResource> m_resource;
  WTF::OrdinalNumber m_startLineNumber;
  String m_characterEncoding;
  String m_fallbackCharacterEncoding;

  bool m_parserInserted : 1;
  bool m_isExternalScript : 1;
  bool m_alreadyStarted : 1;
  bool m_haveFiredLoad : 1;
  // Same as "The parser will handle executing the script."
  bool m_willBeParserExecuted : 1;
  bool m_readyToBeParserExecuted : 1;
  bool m_willExecuteWhenDocumentFinishedParsing : 1;
  bool m_forceAsync : 1;
  const bool m_createdDuringDocumentWrite : 1;

  ScriptRunner::AsyncExecutionType m_asyncExecType;
  enum DocumentWriteIntervention {
    DocumentWriteInterventionNone = 0,
    // Based on what shouldDisallowFetchForMainFrameScript() returns.
    // This script will be blocked if not present in http cache.
    DoNotFetchDocWrittenScript,
    // If a parser blocking doc.written script was not fetched and was not
    // present in the http cache, send a GET for it with an interventions
    // header to allow the server to know of the intervention. This fetch
    // will be using DeferOption::IdleLoad to keep it out of the critical
    // path.
    FetchDocWrittenScriptDeferIdle,
  };

  DocumentWriteIntervention m_documentWriteIntervention;

  Member<PendingScript> m_pendingScript;
};

ScriptLoader* toScriptLoaderIfPossible(Element*);

}  // namespace blink

#endif  // ScriptLoader_h

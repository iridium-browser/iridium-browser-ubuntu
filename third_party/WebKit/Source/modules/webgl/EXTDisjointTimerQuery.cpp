// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/EXTDisjointTimerQuery.h"

#include "bindings/modules/v8/WebGLAny.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "modules/webgl/WebGLTimerQueryEXT.h"

namespace blink {

WebGLExtensionName EXTDisjointTimerQuery::name() const {
  return EXTDisjointTimerQueryName;
}

EXTDisjointTimerQuery* EXTDisjointTimerQuery::create(
    WebGLRenderingContextBase* context) {
  return new EXTDisjointTimerQuery(context);
}

bool EXTDisjointTimerQuery::supported(WebGLRenderingContextBase* context) {
  return context->extensionsUtil()->supportsExtension(
      "GL_EXT_disjoint_timer_query");
}

const char* EXTDisjointTimerQuery::extensionName() {
  return "EXT_disjoint_timer_query";
}

WebGLTimerQueryEXT* EXTDisjointTimerQuery::createQueryEXT() {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.isLost())
    return nullptr;

  return WebGLTimerQueryEXT::create(scoped.context());
}

void EXTDisjointTimerQuery::deleteQueryEXT(WebGLTimerQueryEXT* query) {
  WebGLExtensionScopedContext scoped(this);
  if (!query || scoped.isLost())
    return;
  query->deleteObject(scoped.context()->contextGL());

  if (query == m_currentElapsedQuery)
    m_currentElapsedQuery.clear();
}

GLboolean EXTDisjointTimerQuery::isQueryEXT(WebGLTimerQueryEXT* query) {
  WebGLExtensionScopedContext scoped(this);
  if (!query || scoped.isLost() || query->isDeleted() ||
      !query->validate(0, scoped.context())) {
    return false;
  }

  return scoped.context()->contextGL()->IsQueryEXT(query->object());
}

void EXTDisjointTimerQuery::beginQueryEXT(GLenum target,
                                          WebGLTimerQueryEXT* query) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.isLost())
    return;

  DCHECK(query);
  if (query->isDeleted() || !query->validate(0, scoped.context())) {
    scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "beginQueryEXT",
                                        "invalid query");
    return;
  }

  if (target != GL_TIME_ELAPSED_EXT) {
    scoped.context()->synthesizeGLError(GL_INVALID_ENUM, "beginQueryEXT",
                                        "invalid target");
    return;
  }

  if (m_currentElapsedQuery) {
    scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "beginQueryEXT",
                                        "a query is already active for target");
    return;
  }

  if (query->hasTarget() && query->target() != target) {
    scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "beginQueryEXT",
                                        "target does not match query");
    return;
  }

  scoped.context()->contextGL()->BeginQueryEXT(target, query->object());
  query->setTarget(target);
  m_currentElapsedQuery = query;
}

void EXTDisjointTimerQuery::endQueryEXT(GLenum target) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.isLost())
    return;

  if (target != GL_TIME_ELAPSED_EXT) {
    scoped.context()->synthesizeGLError(GL_INVALID_ENUM, "endQueryEXT",
                                        "invalid target");
    return;
  }

  if (!m_currentElapsedQuery) {
    scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "endQueryEXT",
                                        "no current query");
    return;
  }

  scoped.context()->contextGL()->EndQueryEXT(target);
  m_currentElapsedQuery->resetCachedResult();
  m_currentElapsedQuery.clear();
}

void EXTDisjointTimerQuery::queryCounterEXT(WebGLTimerQueryEXT* query,
                                            GLenum target) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.isLost())
    return;

  DCHECK(query);
  if (query->isDeleted() || !query->validate(0, scoped.context())) {
    scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "queryCounterEXT",
                                        "invalid query");
    return;
  }

  if (target != GL_TIMESTAMP_EXT) {
    scoped.context()->synthesizeGLError(GL_INVALID_ENUM, "queryCounterEXT",
                                        "invalid target");
    return;
  }

  if (query->hasTarget() && query->target() != target) {
    scoped.context()->synthesizeGLError(GL_INVALID_OPERATION, "queryCounterEXT",
                                        "target does not match query");
    return;
  }

  // Timestamps are disabled in WebGL due to lack of driver support on multiple
  // platforms, so we don't actually perform a GL call.
  query->setTarget(target);
  query->resetCachedResult();
}

ScriptValue EXTDisjointTimerQuery::getQueryEXT(ScriptState* scriptState,
                                               GLenum target,
                                               GLenum pname) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.isLost())
    return ScriptValue::createNull(scriptState);

  if (pname == GL_QUERY_COUNTER_BITS_EXT) {
    if (target == GL_TIMESTAMP_EXT || target == GL_TIME_ELAPSED_EXT) {
      GLint value = 0;
      scoped.context()->contextGL()->GetQueryivEXT(target, pname, &value);
      return WebGLAny(scriptState, value);
    }
    scoped.context()->synthesizeGLError(GL_INVALID_ENUM, "getQuery",
                                        "invalid target/pname combination");
    return ScriptValue::createNull(scriptState);
  }

  if (target == GL_TIME_ELAPSED_EXT && pname == GL_CURRENT_QUERY) {
    return m_currentElapsedQuery ? WebGLAny(scriptState, m_currentElapsedQuery)
                                 : ScriptValue::createNull(scriptState);
  }

  if (target == GL_TIMESTAMP_EXT && pname == GL_CURRENT_QUERY) {
    return ScriptValue::createNull(scriptState);
  }

  scoped.context()->synthesizeGLError(GL_INVALID_ENUM, "getQuery",
                                      "invalid target/pname combination");
  return ScriptValue::createNull(scriptState);
}

ScriptValue EXTDisjointTimerQuery::getQueryObjectEXT(ScriptState* scriptState,
                                                     WebGLTimerQueryEXT* query,
                                                     GLenum pname) {
  WebGLExtensionScopedContext scoped(this);
  if (scoped.isLost())
    return ScriptValue::createNull(scriptState);

  DCHECK(query);
  if (query->isDeleted() || !query->validate(0, scoped.context()) ||
      m_currentElapsedQuery == query) {
    scoped.context()->synthesizeGLError(GL_INVALID_OPERATION,
                                        "getQueryObjectEXT", "invalid query");
    return ScriptValue::createNull(scriptState);
  }

  switch (pname) {
    case GL_QUERY_RESULT_EXT: {
      query->updateCachedResult(scoped.context()->contextGL());
      return WebGLAny(scriptState, query->getQueryResult());
    }
    case GL_QUERY_RESULT_AVAILABLE_EXT: {
      query->updateCachedResult(scoped.context()->contextGL());
      return WebGLAny(scriptState, query->isQueryResultAvailable());
    }
    default:
      scoped.context()->synthesizeGLError(GL_INVALID_ENUM, "getQueryObjectEXT",
                                          "invalid pname");
      break;
  }

  return ScriptValue::createNull(scriptState);
}

DEFINE_TRACE(EXTDisjointTimerQuery) {
  visitor->trace(m_currentElapsedQuery);
  WebGLExtension::trace(visitor);
}

EXTDisjointTimerQuery::EXTDisjointTimerQuery(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->extensionsUtil()->ensureExtensionEnabled(
      "GL_EXT_disjoint_timer_query");
}

}  // namespace blink

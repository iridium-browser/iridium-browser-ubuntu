/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webdatabase/DatabaseManager.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseCallback.h"
#include "modules/webdatabase/DatabaseClient.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "modules/webdatabase/DatabaseTask.h"
#include "modules/webdatabase/DatabaseTracker.h"
#include "modules/webdatabase/StorageLog.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/PtrUtil.h"

namespace blink {

static DatabaseManager* s_databaseManager;

DatabaseManager& DatabaseManager::manager() {
  DCHECK(isMainThread());
  if (!s_databaseManager)
    s_databaseManager = new DatabaseManager();
  return *s_databaseManager;
}

void DatabaseManager::terminateDatabaseThread() {
  DCHECK(isMainThread());
  if (!s_databaseManager)
    return;
  for (const Member<DatabaseContext>& context :
       s_databaseManager->m_contextMap.values())
    context->stopDatabases();
}

DatabaseManager::DatabaseManager()
{
}

DatabaseManager::~DatabaseManager() {}

// This is just for ignoring DatabaseCallback::handleEvent()'s return value.
static void databaseCallbackHandleEvent(DatabaseCallback* callback,
                                        Database* database) {
  callback->handleEvent(database);
}

DatabaseContext* DatabaseManager::existingDatabaseContextFor(
    ExecutionContext* context) {
  ASSERT(m_databaseContextRegisteredCount >= 0);
  ASSERT(m_databaseContextInstanceCount >= 0);
  ASSERT(m_databaseContextRegisteredCount <= m_databaseContextInstanceCount);
  return m_contextMap.get(context);
}

DatabaseContext* DatabaseManager::databaseContextFor(
    ExecutionContext* context) {
  if (DatabaseContext* databaseContext = existingDatabaseContextFor(context))
    return databaseContext;
  return DatabaseContext::create(context);
}

void DatabaseManager::registerDatabaseContext(
    DatabaseContext* databaseContext) {
  ExecutionContext* context = databaseContext->getExecutionContext();
  m_contextMap.set(context, databaseContext);
#if DCHECK_IS_ON()
  m_databaseContextRegisteredCount++;
#endif
}

void DatabaseManager::unregisterDatabaseContext(
    DatabaseContext* databaseContext) {
  ExecutionContext* context = databaseContext->getExecutionContext();
  ASSERT(m_contextMap.get(context));
#if DCHECK_IS_ON()
  m_databaseContextRegisteredCount--;
#endif
  m_contextMap.remove(context);
}

#if DCHECK_IS_ON()
void DatabaseManager::didConstructDatabaseContext() {
  m_databaseContextInstanceCount++;
}

void DatabaseManager::didDestructDatabaseContext() {
  m_databaseContextInstanceCount--;
  ASSERT(m_databaseContextRegisteredCount <= m_databaseContextInstanceCount);
}
#endif

void DatabaseManager::throwExceptionForDatabaseError(
    DatabaseError error,
    const String& errorMessage,
    ExceptionState& exceptionState) {
  switch (error) {
    case DatabaseError::None:
      return;
    case DatabaseError::GenericSecurityError:
      exceptionState.throwSecurityError(errorMessage);
      return;
    case DatabaseError::InvalidDatabaseState:
      exceptionState.throwDOMException(InvalidStateError, errorMessage);
      return;
    default:
      ASSERT_NOT_REACHED();
  }
}

static void logOpenDatabaseError(ExecutionContext* context,
                                 const String& name) {
  STORAGE_DVLOG(1) << "Database " << name << " for origin "
                   << context->getSecurityOrigin()->toString()
                   << " not allowed to be established";
}

Database* DatabaseManager::openDatabaseInternal(ExecutionContext* context,
                                                const String& name,
                                                const String& expectedVersion,
                                                const String& displayName,
                                                unsigned estimatedSize,
                                                bool setVersionInNewDatabase,
                                                DatabaseError& error,
                                                String& errorMessage) {
  ASSERT(error == DatabaseError::None);

  DatabaseContext* backendContext = databaseContextFor(context)->backend();
  if (DatabaseTracker::tracker().canEstablishDatabase(
          backendContext, name, displayName, estimatedSize, error)) {
    Database* backend = new Database(backendContext, name, expectedVersion,
                                     displayName, estimatedSize);
    if (backend->openAndVerifyVersion(setVersionInNewDatabase, error,
                                      errorMessage))
      return backend;
  }

  ASSERT(error != DatabaseError::None);
  switch (error) {
    case DatabaseError::GenericSecurityError:
      logOpenDatabaseError(context, name);
      return nullptr;

    case DatabaseError::InvalidDatabaseState:
      logErrorMessage(context, errorMessage);
      return nullptr;

    default:
      ASSERT_NOT_REACHED();
  }
  return nullptr;
}

Database* DatabaseManager::openDatabase(ExecutionContext* context,
                                        const String& name,
                                        const String& expectedVersion,
                                        const String& displayName,
                                        unsigned estimatedSize,
                                        DatabaseCallback* creationCallback,
                                        DatabaseError& error,
                                        String& errorMessage) {
  ASSERT(error == DatabaseError::None);

  bool setVersionInNewDatabase = !creationCallback;
  Database* database = openDatabaseInternal(
      context, name, expectedVersion, displayName, estimatedSize,
      setVersionInNewDatabase, error, errorMessage);
  if (!database)
    return nullptr;

  databaseContextFor(context)->setHasOpenDatabases();
  DatabaseClient::from(context)->didOpenDatabase(
      database, context->getSecurityOrigin()->host(), name, expectedVersion);

  if (database->isNew() && creationCallback) {
    STORAGE_DVLOG(1) << "Scheduling DatabaseCreationCallbackTask for database "
                     << database;
    database->getExecutionContext()->postTask(
        TaskType::DatabaseAccess, BLINK_FROM_HERE,
        createSameThreadTask(&databaseCallbackHandleEvent,
                             wrapPersistent(creationCallback),
                             wrapPersistent(database)),
        "openDatabase");
  }

  ASSERT(database);
  return database;
}

String DatabaseManager::fullPathForDatabase(SecurityOrigin* origin,
                                            const String& name,
                                            bool createIfDoesNotExist) {
  return DatabaseTracker::tracker().fullPathForDatabase(origin, name,
                                                        createIfDoesNotExist);
}

void DatabaseManager::logErrorMessage(ExecutionContext* context,
                                      const String& message) {
  context->addConsoleMessage(
      ConsoleMessage::create(StorageMessageSource, ErrorMessageLevel, message));
}

}  // namespace blink

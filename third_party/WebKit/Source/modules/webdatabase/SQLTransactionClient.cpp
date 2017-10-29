/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webdatabase/SQLTransactionClient.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/TaskRunnerHelper.h"
#include "modules/webdatabase/Database.h"
#include "modules/webdatabase/DatabaseContext.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/WebTaskRunner.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/Functional.h"
#include "public/platform/Platform.h"
#include "public/platform/WebDatabaseObserver.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

namespace {

void DatabaseModified(const WebSecurityOrigin& origin,
                      const String& database_name) {
  if (Platform::Current()->DatabaseObserver())
    Platform::Current()->DatabaseObserver()->DatabaseModified(origin,
                                                              database_name);
}

void DatabaseModifiedCrossThread(const String& origin_string,
                                 const String& database_name) {
  DatabaseModified(WebSecurityOrigin::CreateFromString(origin_string),
                   database_name);
}

}  // namespace

void SQLTransactionClient::DidCommitWriteTransaction(Database* database) {
  String database_name = database->StringIdentifier();
  ExecutionContext* execution_context =
      database->GetDatabaseContext()->GetExecutionContext();
  SecurityOrigin* origin = database->GetSecurityOrigin();
  if (!execution_context->IsContextThread()) {
    database->GetDatabaseTaskRunner()->PostTask(
        BLINK_FROM_HERE, CrossThreadBind(&DatabaseModifiedCrossThread,
                                         origin->ToRawString(), database_name));
  } else {
    DatabaseModified(WebSecurityOrigin(origin), database_name);
  }
}

bool SQLTransactionClient::DidExceedQuota(Database* database) {
  // Chromium does not allow users to manually change the quota for an origin
  // (for now, at least).  Don't do anything.
  DCHECK(
      database->GetDatabaseContext()->GetExecutionContext()->IsContextThread());
  return false;
}

}  // namespace blink

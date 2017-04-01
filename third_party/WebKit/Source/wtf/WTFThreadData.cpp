/*
 * Copyright (C) 2008, 2010 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "wtf/WTFThreadData.h"

#include "wtf/StackUtil.h"
#include "wtf/text/AtomicStringTable.h"
#include "wtf/text/TextCodecICU.h"

namespace WTF {

ThreadSpecific<WTFThreadData>* WTFThreadData::staticData;

WTFThreadData::WTFThreadData()
    : m_atomicStringTable(new AtomicStringTable),
      m_cachedConverterICU(new ICUConverterWrapper),
      m_threadId(internal::currentThreadSyscall()) {}

WTFThreadData::~WTFThreadData() {}

#if OS(WIN) && COMPILER(MSVC)
size_t WTFThreadData::threadStackSize() {
  // Needed to bootstrap WTFThreadData on Windows, because this value is needed
  // before the main thread data is fully initialized.
  if (!WTFThreadData::staticData->isSet())
    return internal::threadStackSize();

  WTFThreadData& data = wtfThreadData();
  if (!data.m_threadStackSize)
    data.m_threadStackSize = internal::threadStackSize();
  return data.m_threadStackSize;
}
#endif

}  // namespace WTF

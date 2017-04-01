// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FWL_CFWL_TIMER_H_
#define XFA_FWL_CFWL_TIMER_H_

#include "core/fxcrt/fx_system.h"

class CFWL_TimerInfo;
class CFWL_Widget;

class CFWL_Timer {
 public:
  explicit CFWL_Timer(CFWL_Widget* parent) : m_pWidget(parent) {}
  virtual ~CFWL_Timer() {}

  virtual void Run(CFWL_TimerInfo* hTimer) = 0;
  CFWL_TimerInfo* StartTimer(uint32_t dwElapse, bool bImmediately);

 protected:
  CFWL_Widget* m_pWidget;  // Not owned.
};

#endif  // XFA_FWL_CFWL_TIMER_H_

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/ime_bridge.h"

#include <map>
#include "base/logging.h"
#include "base/memory/singleton.h"

namespace chromeos {

static IMEBridge* g_ime_bridge = NULL;

// An implementation of IMEBridge.
class IMEBridgeImpl : public IMEBridge {
 public:
  IMEBridgeImpl()
      : input_context_handler_(NULL),
        engine_handler_(NULL),
        candidate_window_handler_(NULL),
        current_input_context_(ui::TEXT_INPUT_TYPE_NONE,
                               ui::TEXT_INPUT_MODE_DEFAULT,
                               0) {}

  ~IMEBridgeImpl() override {}

  // IMEBridge override.
  IMEInputContextHandlerInterface* GetInputContextHandler() const override {
    return input_context_handler_;
  }

  // IMEBridge override.
  void SetInputContextHandler(
      IMEInputContextHandlerInterface* handler) override {
    input_context_handler_ = handler;
  }

  // IMEBridge override.
  void SetCurrentEngineHandler(IMEEngineHandlerInterface* handler) override {
    engine_handler_ = handler;
  }

  // IMEBridge override.
  IMEEngineHandlerInterface* GetCurrentEngineHandler() const override {
    return engine_handler_;
  }

  // IMEBridge override.
  IMECandidateWindowHandlerInterface* GetCandidateWindowHandler()
      const override {
    return candidate_window_handler_;
  }

  // IMEBridge override.
  void SetCandidateWindowHandler(
      IMECandidateWindowHandlerInterface* handler) override {
    candidate_window_handler_ = handler;
  }

  // IMEBridge override.
  void SetCurrentInputContext(
      const IMEEngineHandlerInterface::InputContext& input_context) override {
    current_input_context_ = input_context;
  }

  // IMEBridge override.
  const IMEEngineHandlerInterface::InputContext& GetCurrentInputContext()
      const override {
    return current_input_context_;
  }

 private:
  IMEInputContextHandlerInterface* input_context_handler_;
  IMEEngineHandlerInterface* engine_handler_;
  IMECandidateWindowHandlerInterface* candidate_window_handler_;
  IMEEngineHandlerInterface::InputContext current_input_context_;

  DISALLOW_COPY_AND_ASSIGN(IMEBridgeImpl);
};

///////////////////////////////////////////////////////////////////////////////
// IMEBridge
IMEBridge::IMEBridge() {
}

IMEBridge::~IMEBridge() {
}

// static.
void IMEBridge::Initialize() {
  if (!g_ime_bridge)
    g_ime_bridge = new IMEBridgeImpl();
}

// static.
void IMEBridge::Shutdown() {
  delete g_ime_bridge;
  g_ime_bridge = NULL;
}

// static.
IMEBridge* IMEBridge::Get() {
  return g_ime_bridge;
}

}  // namespace chromeos

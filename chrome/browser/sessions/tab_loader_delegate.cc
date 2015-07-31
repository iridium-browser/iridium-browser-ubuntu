// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/tab_loader_delegate.h"

#include "base/strings/string_number_conversions.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/network_change_notifier.h"

namespace {

// The timeout time after which the next tab gets loaded if the previous tab did
// not finish loading yet. The used value is half of the median value of all
// ChromeOS devices loading the 25 most common web pages. Half is chosen since
// the loading time is a mix of server response and data bandwidth.
static const int kInitialDelayTimerMS = 1500;

class TabLoaderDelegateImpl
    : public TabLoaderDelegate,
      public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  explicit TabLoaderDelegateImpl(TabLoaderCallback* callback);
  ~TabLoaderDelegateImpl() override;

  // TabLoaderDelegate:
  base::TimeDelta GetFirstTabLoadingTimeout() const override {
    return first_timeout_;
  }

  // TabLoaderDelegate:
  base::TimeDelta GetTimeoutBeforeLoadingNextTab() const override {
    return timeout_;
  }

  // net::NetworkChangeNotifier::ConnectionTypeObserver:
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  // The function to call when the connection type changes.
  TabLoaderCallback* callback_;

  // The timeouts to use in tab loading.
  base::TimeDelta first_timeout_;
  base::TimeDelta timeout_;

  DISALLOW_COPY_AND_ASSIGN(TabLoaderDelegateImpl);
};

TabLoaderDelegateImpl::TabLoaderDelegateImpl(TabLoaderCallback* callback)
    : callback_(callback) {
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
  if (net::NetworkChangeNotifier::IsOffline()) {
    // When we are off-line we do not allow loading of tabs, since each of
    // these tabs would start loading simultaneously when going online.
    // TODO(skuhne): Once we get a higher level resource control logic which
    // distributes network access, we can remove this.
    callback->SetTabLoadingEnabled(false);
  }

  // Initialize the timeouts to use from the session restore field trial.
  // Default to the usual value if none is specified.

  static const char kIntelligentSessionRestore[] = "IntelligentSessionRestore";
  std::string timeout = variations::GetVariationParamValue(
      kIntelligentSessionRestore, "FirstTabLoadTimeoutMs");
  int timeout_ms = 0;
  if (timeout.empty() || !base::StringToInt(timeout, &timeout_ms) ||
      timeout_ms <= 0) {
    timeout_ms = kInitialDelayTimerMS;
  }
  first_timeout_ = base::TimeDelta::FromMilliseconds(timeout_ms);

  timeout = variations::GetVariationParamValue(
      kIntelligentSessionRestore, "TabLoadTimeoutMs");
  timeout_ms = 0;
  if (timeout.empty() || !base::StringToInt(timeout, &timeout_ms) ||
      timeout_ms <= 0) {
    timeout_ms = kInitialDelayTimerMS;
  }
  timeout_ = base::TimeDelta::FromMilliseconds(timeout_ms);
}

TabLoaderDelegateImpl::~TabLoaderDelegateImpl() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void TabLoaderDelegateImpl::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  callback_->SetTabLoadingEnabled(
      type != net::NetworkChangeNotifier::CONNECTION_NONE);
}
}  // namespace

// static
scoped_ptr<TabLoaderDelegate> TabLoaderDelegate::Create(
    TabLoaderCallback* callback) {
  return scoped_ptr<TabLoaderDelegate>(
      new TabLoaderDelegateImpl(callback));
}

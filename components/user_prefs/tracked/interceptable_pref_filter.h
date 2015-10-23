// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_TRACKED_INTERCEPTABLE_PREF_FILTER_H_
#define COMPONENTS_USER_PREFS_TRACKED_INTERCEPTABLE_PREF_FILTER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_filter.h"
#include "base/values.h"

// A partial implementation of a PrefFilter whose FilterOnLoad call may be
// intercepted by a FilterOnLoadInterceptor. Implementations of
// InterceptablePrefFilter are expected to override FinalizeFilterOnLoad rather
// than re-overriding FilterOnLoad.
class InterceptablePrefFilter
    : public PrefFilter,
      public base::SupportsWeakPtr<InterceptablePrefFilter> {
 public:
  // A callback to be invoked by a FilterOnLoadInterceptor when its ready to
  // hand back the |prefs| it was handed for early filtering. |prefs_altered|
  // indicates whether the |prefs| were actually altered by the
  // FilterOnLoadInterceptor before being handed back.
  typedef base::Callback<void(scoped_ptr<base::DictionaryValue> prefs,
                              bool prefs_altered)> FinalizeFilterOnLoadCallback;

  // A callback to be invoked from FilterOnLoad. It takes ownership of prefs
  // and may modify them before handing them back to this
  // InterceptablePrefFilter via |finalize_filter_on_load|.
  typedef base::Callback<void(
      const FinalizeFilterOnLoadCallback& finalize_filter_on_load,
      scoped_ptr<base::DictionaryValue> prefs)> FilterOnLoadInterceptor;

  InterceptablePrefFilter();
  ~InterceptablePrefFilter() override;

  // PrefFilter partial implementation.
  void FilterOnLoad(
      const PostFilterOnLoadCallback& post_filter_on_load_callback,
      scoped_ptr<base::DictionaryValue> pref_store_contents) override;

  // Registers |filter_on_load_interceptor| to intercept the next FilterOnLoad
  // event. At most one FilterOnLoadInterceptor should be registered per
  // PrefFilter.
  void InterceptNextFilterOnLoad(
      const FilterOnLoadInterceptor& filter_on_load_interceptor);

 private:
  // Does any extra filtering required by the implementation of this
  // InterceptablePrefFilter and hands back the |pref_store_contents| to the
  // initial caller of FilterOnLoad.
  virtual void FinalizeFilterOnLoad(
      const PostFilterOnLoadCallback& post_filter_on_load_callback,
      scoped_ptr<base::DictionaryValue> pref_store_contents,
      bool prefs_altered) = 0;

  // Callback to be invoked only once (and subsequently reset) on the next
  // FilterOnLoad event. It will be allowed to modify the |prefs| handed to
  // FilterOnLoad before handing them back to this PrefHashFilter.
  FilterOnLoadInterceptor filter_on_load_interceptor_;
};

#endif  // COMPONENTS_USER_PREFS_TRACKED_INTERCEPTABLE_PREF_FILTER_H_

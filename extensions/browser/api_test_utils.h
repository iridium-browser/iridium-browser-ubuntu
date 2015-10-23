// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_API_TEST_UTILS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/common/manifest.h"

class UIThreadExtensionFunction;

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExtensionFunctionDispatcher;

// TODO(yoz): crbug.com/394840: Remove duplicate functionality in
// chrome/browser/extensions/extension_function_test_utils.h.
//
// TODO(ckehoe): Accept args as scoped_ptr<base::Value>,
// and migrate existing users to the new API.
namespace api_test_utils {

enum RunFunctionFlags { NONE = 0, INCLUDE_INCOGNITO = 1 << 0 };

// Parse JSON and return as the specified type, or NULL if the JSON is invalid
// or not the specified type.
scoped_ptr<base::DictionaryValue> ParseDictionary(const std::string& data);

// Get |key| from |val| as the specified type. If |key| does not exist, or is
// not of the specified type, adds a failure to the current test and returns
// false, 0, empty string, etc.
bool GetBoolean(const base::DictionaryValue* val, const std::string& key);
int GetInteger(const base::DictionaryValue* val, const std::string& key);
std::string GetString(const base::DictionaryValue* val, const std::string& key);

// Creates an extension instance with a specified extension value that can be
// attached to an ExtensionFunction before running.
scoped_refptr<extensions::Extension> CreateExtension(
    base::DictionaryValue* test_extension_value);

scoped_refptr<extensions::Extension> CreateExtension(
    extensions::Manifest::Location location,
    base::DictionaryValue* test_extension_value,
    const std::string& id_input);

// Creates an extension instance with a specified location that can be attached
// to an ExtensionFunction before running.
scoped_refptr<extensions::Extension> CreateEmptyExtensionWithLocation(
    extensions::Manifest::Location location);

// Run |function| with |args| and return the result. Adds an error to the
// current test if |function| returns an error. Takes ownership of
// |function|. The caller takes ownership of the result.
base::Value* RunFunctionWithDelegateAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    scoped_ptr<ExtensionFunctionDispatcher> dispatcher);
base::Value* RunFunctionWithDelegateAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    scoped_ptr<ExtensionFunctionDispatcher> dispatcher,
    RunFunctionFlags flags);

// RunFunctionWithDelegateAndReturnSingleResult, except with a NULL
// implementation of the Delegate.
base::Value* RunFunctionAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context);
base::Value* RunFunctionAndReturnSingleResult(
    UIThreadExtensionFunction* function,
    const std::string& args,
    content::BrowserContext* context,
    RunFunctionFlags flags);

// Run |function| with |args| and return the resulting error. Adds an error to
// the current test if |function| returns a result. Takes ownership of
// |function|.
std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                      const std::string& args,
                                      content::BrowserContext* context,
                                      RunFunctionFlags flags);
std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                      const std::string& args,
                                      content::BrowserContext* context);

// Create and run |function| with |args|. Works with both synchronous and async
// functions. Ownership of |function| remains with the caller.
//
// TODO(aa): It would be nice if |args| could be validated against the schema
// that |function| expects. That way, we know that we are testing something
// close to what the bindings would actually send.
//
// TODO(aa): I'm concerned that this style won't scale to all the bits and bobs
// we're going to need to frob for all the different extension functions. But
// we can refactor when we see what is needed.
bool RunFunction(UIThreadExtensionFunction* function,
                 const std::string& args,
                 content::BrowserContext* context);
bool RunFunction(UIThreadExtensionFunction* function,
                 const std::string& args,
                 content::BrowserContext* context,
                 scoped_ptr<ExtensionFunctionDispatcher> dispatcher,
                 RunFunctionFlags flags);
bool RunFunction(UIThreadExtensionFunction* function,
                 scoped_ptr<base::ListValue> args,
                 content::BrowserContext* context,
                 scoped_ptr<ExtensionFunctionDispatcher> dispatcher,
                 RunFunctionFlags flags);

}  // namespace api_test_utils
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_TEST_UTILS_H_

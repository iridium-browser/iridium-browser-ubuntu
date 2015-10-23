// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/err.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"

namespace functions {

namespace {

void ForwardAllValues(const FunctionCallNode* function,
                      Scope* source,
                      Scope* dest,
                      Err* err) {
  Scope::MergeOptions options;
  // This function needs to clobber existing for it to be useful. It will be
  // called in a template to forward all values, but there will be some
  // default stuff like configs set up in both scopes, so it would always
  // fail if it didn't clobber.
  options.clobber_existing = true;
  options.skip_private_vars = true;
  options.mark_dest_used = false;
  source->NonRecursiveMergeTo(dest, options, function,
                              "source scope", err);
  source->MarkAllUsed();
}

void ForwardValuesFromList(Scope* source,
                           Scope* dest,
                           const std::vector<Value>& list,
                           Err* err) {
  for (const Value& cur : list) {
    if (!cur.VerifyTypeIs(Value::STRING, err))
      return;
    const Value* value = source->GetValue(cur.string_value(), true);
    if (value) {
      // Use the storage key for the original value rather than the string in
      // "cur" because "cur" is a temporary that will be deleted, and Scopes
      // expect a persistent StringPiece (it won't copy). Not doing this will
      // lead the scope's key to point to invalid memory after this returns.
      base::StringPiece storage_key = source->GetStorageKey(cur.string_value());
      if (storage_key.empty()) {
        // Programmatic value, don't allow copying.
        *err = Err(cur, "This value can't be forwarded.",
            "The variable \"" + cur.string_value() + "\" is a built-in.");
        return;
      }

      // Keep the origin information from the original value. The normal
      // usage is for this to be used in a template, and if there's an error,
      // the user expects to see the line where they set the variable
      // blamed, rather than a template call to forward_variables_from().
      dest->SetValue(storage_key, *value, value->origin());
    }
  }
}

}  // namespace

const char kForwardVariablesFrom[] = "forward_variables_from";
const char kForwardVariablesFrom_HelpShort[] =
    "forward_variables_from: Copies variables from a different scope.";
const char kForwardVariablesFrom_Help[] =
    "forward_variables_from: Copies variables from a different scope.\n"
    "\n"
    "  forward_variables_from(from_scope, variable_list_or_star)\n"
    "\n"
    "  Copies the given variables from the given scope to the local scope\n"
    "  if they exist. This is normally used in the context of templates to\n"
    "  use the values of variables defined in the template invocation to\n"
    "  a template-defined target.\n"
    "\n"
    "  The variables in the given variable_list will be copied if they exist\n"
    "  in the given scope or any enclosing scope. If they do not exist,\n"
    "  nothing will happen and they be left undefined in the current scope.\n"
    "\n"
    "  As a special case, if the variable_list is a string with the value of\n"
    "  \"*\", all variables from the given scope will be copied. \"*\" only\n"
    "  copies variables set directly on the from_scope, not enclosing ones.\n"
    "  Otherwise it would duplicate all global variables.\n"
    "\n"
    "  When an explicit list of variables is supplied, if the variable exists\n"
    "  in the current (destination) scope already, an error will be thrown.\n"
    "  If \"*\" is specified, variables in the current scope will be\n"
    "  clobbered (the latter is important because most targets have an\n"
    "  implicit configs list, which means it wouldn't work at all if it\n"
    "  didn't clobber).\n"
    "\n"
    "  The sources assignment filter (see \"gn help "
          "set_sources_assignment_filter\")\n"
    "  is never applied by this function. It's assumed than any desired\n"
    "  filtering was already done when sources was set on the from_scope.\n"
    "\n"
    "Examples\n"
    "\n"
    "  # This is a common action template. It would invoke a script with\n"
    "  # some given parameters, and wants to use the various types of deps\n"
    "  # and the visibility from the invoker if it's defined. It also injects\n"
    "  # an additional dependency to all targets.\n"
    "  template(\"my_test\") {\n"
    "    action(target_name) {\n"
    "      forward_variables_from(invoker, [ \"data_deps\", \"deps\",\n"
    "                                        \"public_deps\", \"visibility\" "
                                                                         "])\n"
    "      # Add our test code to the dependencies.\n"
    "      # \"deps\" may or may not be defined at this point.\n"
    "      if (defined(deps)) {\n"
    "        deps += [ \"//tools/doom_melon\" ]\n"
    "      } else {\n"
    "        deps = [ \"//tools/doom_melon\" ]\n"
    "      }\n"
    "    }\n"
    "  }\n"
    "\n"
    "  # This is a template around either a target whose type depends on a\n"
    "  # global variable. It forwards all values from the invoker.\n"
    "  template(\"my_wrapper\") {\n"
    "    target(my_wrapper_target_type, target_name) {\n"
    "      forward_variables_from(invoker, \"*\")\n"
    "    }\n"
    " }\n";

// This function takes a ListNode rather than a resolved vector of values
// both avoid copying the potentially-large source scope, and so the variables
// in the source scope can be marked as used.
Value RunForwardVariablesFrom(Scope* scope,
                              const FunctionCallNode* function,
                              const ListNode* args_list,
                              Err* err) {
  const std::vector<const ParseNode*>& args_vector = args_list->contents();
  if (args_vector.size() != 2) {
    *err = Err(function, "Wrong number of arguments.",
               "Expecting exactly two.");
    return Value();
  }

  // Extract the scope identifier. This assumes the first parameter is an
  // identifier. It is difficult to write code where this is not the case, and
  // this saves an expensive scope copy. If necessary, this could be expanded
  // to execute the ParseNode and get the value out if it's not an identifer.
  const IdentifierNode* identifier = args_vector[0]->AsIdentifier();
  if (!identifier) {
    *err = Err(args_vector[0], "Expected an identifier for the scope.");
    return Value();
  }

  // Extract the source scope.
  Value* value = scope->GetMutableValue(identifier->value().value(), true);
  if (!value) {
    *err = Err(identifier, "Undefined identifier.");
    return Value();
  }
  if (!value->VerifyTypeIs(Value::SCOPE, err))
    return Value();
  Scope* source = value->scope_value();

  // Extract the list. If all_values is not set, the what_value will be a list.
  Value what_value = args_vector[1]->Execute(scope, err);
  if (err->has_error())
    return Value();
  if (what_value.type() == Value::STRING) {
    if (what_value.string_value() == "*") {
      ForwardAllValues(function, source, scope, err);
      return Value();
    }
  } else {
    if (what_value.type() == Value::LIST) {
      ForwardValuesFromList(source, scope, what_value.list_value(), err);
      return Value();
    }
  }

  // Not the right type of argument.
  *err = Err(what_value, "Not a valid list of variables to copy.",
             "Expecting either the string \"*\" or a list of strings.");
  return Value();
}

}  // namespace functions

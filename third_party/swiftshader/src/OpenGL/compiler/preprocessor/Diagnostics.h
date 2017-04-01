// Copyright 2016 The SwiftShader Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COMPILER_PREPROCESSOR_DIAGNOSTICS_H_
#define COMPILER_PREPROCESSOR_DIAGNOSTICS_H_

#include <string>

namespace pp
{

struct SourceLocation;

// Base class for reporting diagnostic messages.
// Derived classes are responsible for formatting and printing the messages.
class Diagnostics
{
public:
	enum Severity
	{
		PP_ERROR,
		PP_WARNING
	};
	enum ID
	{
		ERROR_BEGIN,
		INTERNAL_ERROR,
		OUT_OF_MEMORY,
		INVALID_CHARACTER,
		INVALID_NUMBER,
		INTEGER_OVERFLOW,
		FLOAT_OVERFLOW,
		TOKEN_TOO_LONG,
		INVALID_EXPRESSION,
		DIVISION_BY_ZERO,
		EOF_IN_COMMENT,
		UNEXPECTED_TOKEN,
		DIRECTIVE_INVALID_NAME,
		MACRO_NAME_RESERVED,
		MACRO_REDEFINED,
		MACRO_PREDEFINED_REDEFINED,
		MACRO_PREDEFINED_UNDEFINED,
		MACRO_UNTERMINATED_INVOCATION,
		MACRO_TOO_FEW_ARGS,
		MACRO_TOO_MANY_ARGS,
		MACRO_DUPLICATE_PARAMETER_NAMES,
		CONDITIONAL_ENDIF_WITHOUT_IF,
		CONDITIONAL_ELSE_WITHOUT_IF,
		CONDITIONAL_ELSE_AFTER_ELSE,
		CONDITIONAL_ELIF_WITHOUT_IF,
		CONDITIONAL_ELIF_AFTER_ELSE,
		CONDITIONAL_UNTERMINATED,
		CONDITIONAL_UNEXPECTED_TOKEN,
		INVALID_EXTENSION_NAME,
		INVALID_EXTENSION_BEHAVIOR,
		INVALID_EXTENSION_DIRECTIVE,
		INVALID_VERSION_NUMBER,
		INVALID_VERSION_DIRECTIVE,
		VERSION_NOT_FIRST_STATEMENT,
		INVALID_LINE_NUMBER,
		INVALID_FILE_NUMBER,
		INVALID_LINE_DIRECTIVE,
		UNDEFINED_IDENTIFIER,
		ERROR_END,

		WARNING_BEGIN,
		EOF_IN_DIRECTIVE,
		UNRECOGNIZED_PRAGMA,
		WARNING_END
	};

	virtual ~Diagnostics();

	void report(ID id, const SourceLocation& loc, const std::string& text);

protected:
	Severity severity(ID id);
	std::string message(ID id);

	virtual void print(ID id,
	                   const SourceLocation& loc,
	                   const std::string& text) = 0;
};

}  // namespace pp
#endif  // COMPILER_PREPROCESSOR_DIAGNOSTICS_H_

/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkSLCompiler.h"

#include "Test.h"

static void test_failure(skiatest::Reporter* r, const char* src, const char* error) {
    SkSL::Compiler compiler;
    std::stringstream out;
    bool result = compiler.toSPIRV(SkSL::Program::kFragment_Kind, src, out);
    if (compiler.errorText() != error) {
        SkDebugf("SKSL ERROR:\n    source: %s\n    expected: %s    received: %s", src, error,
                 compiler.errorText().c_str());
    }
    REPORTER_ASSERT(r, !result);
    REPORTER_ASSERT(r, compiler.errorText() == error);
}

static void test_success(skiatest::Reporter* r, const char* src) {
    SkSL::Compiler compiler;
    std::stringstream out;
    bool result = compiler.toSPIRV(SkSL::Program::kFragment_Kind, src, out);
    REPORTER_ASSERT(r, result);
}

DEF_TEST(SkSLUndefinedSymbol, r) {
    test_failure(r,
                 "void main() { x = vec2(1); }",
                 "error: 1: unknown identifier 'x'\n1 error\n");
}

DEF_TEST(SkSLUndefinedFunction, r) {
    test_failure(r,
                 "void main() { int x = foo(1); }", 
                 "error: 1: unknown identifier 'foo'\n1 error\n");
}

DEF_TEST(SkSLGenericArgumentMismatch, r) {
    test_failure(r,
                 "void main() { float x = sin(1, 2); }", 
                 "error: 1: no match for sin(int, int)\n1 error\n");
}

DEF_TEST(SkSLArgumentCountMismatch, r) {
    test_failure(r,
                 "float foo(float x) { return x * x; }"
                 "void main() { float x = foo(1, 2); }", 
                 "error: 1: call to 'foo' expected 1 argument, but found 2\n1 error\n");
}

DEF_TEST(SkSLArgumentMismatch, r) {
    test_failure(r, 
                 "float foo(float x) { return x * x; }"
                 "void main() { float x = foo(true); }", 
                 "error: 1: expected 'float', but found 'bool'\n1 error\n");
}

DEF_TEST(SkSLIfTypeMismatch, r) {
    test_failure(r,
                 "void main() { if (3) { } }",
                 "error: 1: expected 'bool', but found 'int'\n1 error\n");
}

DEF_TEST(SkSLDoTypeMismatch, r) {
    test_failure(r,
                 "void main() { do { } while (vec2(1)); }", 
                 "error: 1: expected 'bool', but found 'vec2'\n1 error\n");
}

DEF_TEST(SkSLWhileTypeMismatch, r) {
    test_failure(r,
                 "void main() { while (vec3(1)) { } }", 
                 "error: 1: expected 'bool', but found 'vec3'\n1 error\n");
}

DEF_TEST(SkSLForTypeMismatch, r) {
    test_failure(r,
                 "void main() { for (int x = 0; x; x++) { } }", 
                 "error: 1: expected 'bool', but found 'int'\n1 error\n");
}

DEF_TEST(SkSLConstructorTypeMismatch, r) {
    test_failure(r,
                 "void main() { vec2 x = vec2(1.0, false); }", 
                 "error: 1: expected 'float', but found 'bool'\n1 error\n");
    test_failure(r,
                 "void main() { bool x = bool(1.0); }",
                 "error: 1: cannot construct 'bool'\n1 error\n");
    test_failure(r,
                 "struct foo { int x; }; void main() { foo x = foo(5); }",
                 "error: 1: cannot construct 'foo'\n1 error\n");
    test_failure(r,
                 "struct foo { int x; } foo; void main() { float x = float(foo); }",
                 "error: 1: invalid argument to 'float' constructor (expected a number or bool, but found 'foo')\n1 error\n");
    test_failure(r,
                 "struct foo { int x; } foo; void main() { vec2 x = vec2(foo); }",
                 "error: 1: 'foo' is not a valid parameter to 'vec2' constructor\n1 error\n");
}

DEF_TEST(SkSLConstructorArgumentCount, r) {
    test_failure(r,
                 "void main() { vec3 x = vec3(1.0, 2.0); }",
                 "error: 1: invalid arguments to 'vec3' constructor (expected 3 scalars, but "
                 "found 2)\n1 error\n");
    test_success(r, "void main() { vec3 x = vec3(1.0, 2.0, 3.0, 4.0); }");
}

DEF_TEST(SkSLSwizzleScalar, r) {
    test_failure(r,
                 "void main() { float x = 1; float y = x.y; }",
                 "error: 1: cannot swizzle value of type 'float'\n1 error\n");
}

DEF_TEST(SkSLSwizzleMatrix, r) {
    test_failure(r,
                 "void main() { mat2 x = mat2(1); float y = x.y; }",
                 "error: 1: cannot swizzle value of type 'mat2'\n1 error\n");
}

DEF_TEST(SkSLSwizzleOutOfBounds, r) {
    test_failure(r,
                 "void main() { vec3 test = vec2(1).xyz; }",
                 "error: 1: invalid swizzle component 'z'\n1 error\n");
}

DEF_TEST(SkSLSwizzleTooManyComponents, r) {
    test_failure(r,
                 "void main() { vec4 test = vec2(1).xxxxx; }",
                 "error: 1: too many components in swizzle mask 'xxxxx'\n1 error\n");
}

DEF_TEST(SkSLSwizzleDuplicateOutput, r) {
    test_failure(r,
                 "void main() { vec4 test = vec4(1); test.xyyz = vec4(1); }",
                 "error: 1: cannot write to the same swizzle field more than once\n1 error\n");
}
DEF_TEST(SkSLAssignmentTypeMismatch, r) {
    test_failure(r,
                 "void main() { int x = 1.0; }",
                 "error: 1: expected 'int', but found 'float'\n1 error\n");
}

DEF_TEST(SkSLReturnFromVoid, r) {
    test_failure(r,
                 "void main() { return true; }",
                 "error: 1: may not return a value from a void function\n1 error\n");
}

DEF_TEST(SkSLReturnMissingValue, r) {
    test_failure(r,
                 "int foo() { return; } void main() { }",
                 "error: 1: expected function to return 'int'\n1 error\n");
}

DEF_TEST(SkSLReturnTypeMismatch, r) {
    test_failure(r,
                 "int foo() { return 1.0; } void main() { }", 
                 "error: 1: expected 'int', but found 'float'\n1 error\n");
}

DEF_TEST(SkSLDuplicateFunction, r) {
    test_failure(r,
                 "void main() { } void main() { }", 
                 "error: 1: duplicate definition of void main()\n1 error\n");
    test_success(r,
                 "void main(); void main() { }");
}

DEF_TEST(SkSLUsingInvalidValue, r) {
    test_failure(r,
                 "void main() { int x = int; }", 
                 "error: 1: expected '(' to begin constructor invocation\n1 error\n");
    test_failure(r,
                 "int test() { return 1; } void main() { int x = test; }", 
                 "error: 1: expected '(' to begin function call\n1 error\n");
}
DEF_TEST(SkSLDifferentReturnType, r) {
    test_failure(r,
                 "int main() { } void main() { }", 
                 "error: 1: functions 'void main()' and 'int main()' differ only in return type\n1 "
                 "error\n");
}

DEF_TEST(SkSLDifferentModifiers, r) {
    test_failure(r,
                 "void test(int x); void test(out int x) { }", 
                 "error: 1: modifiers on parameter 1 differ between declaration and definition\n1 "
                 "error\n");
}

DEF_TEST(SkSLDuplicateSymbol, r) {
    test_failure(r,
                 "int main; void main() { }", 
                 "error: 1: symbol 'main' was already defined\n1 error\n");

    test_failure(r,
                 "int x; int x; void main() { }",
                 "error: 1: symbol 'x' was already defined\n1 error\n");

    test_success(r, "int x; void main() { int x; }");
}

DEF_TEST(SkSLBinaryTypeMismatch, r) {
    test_failure(r,
                 "void main() { float x = 3 * true; }",
                 "error: 1: type mismatch: '*' cannot operate on 'int', 'bool'\n1 error\n");
    test_failure(r,
                 "void main() { bool x = 1 || 2.0; }",
                 "error: 1: type mismatch: '||' cannot operate on 'int', 'float'\n1 error\n");
}

DEF_TEST(SkSLCallNonFunction, r) {
    test_failure(r,
                 "void main() { float x = 3; x(); }",
                 "error: 1: 'x' is not a function\n1 error\n");
}

DEF_TEST(SkSLInvalidUnary, r) {
    test_failure(r,
                 "void main() { mat4 x = mat4(1); ++x; }",
                 "error: 1: '++' cannot operate on 'mat4'\n1 error\n");
    test_failure(r,
                 "void main() { vec3 x = vec3(1); --x; }",
                 "error: 1: '--' cannot operate on 'vec3'\n1 error\n");
    test_failure(r,
                 "void main() { mat4 x = mat4(1); x++; }",
                 "error: 1: '++' cannot operate on 'mat4'\n1 error\n");
    test_failure(r,
                 "void main() { vec3 x = vec3(1); x--; }",
                 "error: 1: '--' cannot operate on 'vec3'\n1 error\n");
    test_failure(r,
                 "void main() { int x = !12; }",
                 "error: 1: '!' cannot operate on 'int'\n1 error\n");
    test_failure(r,
                 "struct foo { } bar; void main() { foo x = +bar; }",
                 "error: 1: '+' cannot operate on 'foo'\n1 error\n");
    test_failure(r,
                 "struct foo { } bar; void main() { foo x = -bar; }",
                 "error: 1: '-' cannot operate on 'foo'\n1 error\n");
    test_success(r,
                 "void main() { vec2 x = vec2(1, 1); x = +x; x = -x; }");
}

DEF_TEST(SkSLInvalidAssignment, r) {
    test_failure(r,
                 "void main() { 1 = 2; }",
                 "error: 1: cannot assign to '1'\n1 error\n");
    test_failure(r,
                 "uniform int x; void main() { x = 0; }",
                 "error: 1: cannot modify immutable variable 'x'\n1 error\n");
    test_failure(r,
                 "const int x; void main() { x = 0; }",
                 "error: 1: cannot modify immutable variable 'x'\n1 error\n");
}

DEF_TEST(SkSLBadIndex, r) {
    test_failure(r,
                 "void main() { int x = 2[0]; }",
                 "error: 1: expected array, but found 'int'\n1 error\n");
    test_failure(r,
                 "void main() { vec2 x = vec2(0); int y = x[0]; }",
                 "error: 1: expected array, but found 'vec2'\n1 error\n");
}

DEF_TEST(SkSLTernaryMismatch, r) {
    test_failure(r,
                 "void main() { int x = 5 > 2 ? true : 1.0; }",
                 "error: 1: ternary operator result mismatch: 'bool', 'float'\n1 error\n");
}

DEF_TEST(SkSLInterfaceBlockStorageModifiers, r) {
    test_failure(r,
                 "uniform foo { out int x; };",
                 "error: 1: interface block fields may not have storage qualifiers\n1 error\n");
}

#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import unittest

from compile import Checker
from processor import FileCache, Processor


_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_DIR = os.path.join(_SCRIPT_DIR, os.pardir, os.pardir)
_RESOURCES_DIR = os.path.join(_SRC_DIR, "ui", "webui", "resources", "js")
_ASSERT_JS = os.path.join(_RESOURCES_DIR, "assert.js")
_CR_JS = os.path.join(_RESOURCES_DIR, "cr.js")
_CR_UI_JS = os.path.join(_RESOURCES_DIR, "cr", "ui.js")
_POLYMER_EXTERNS = os.path.join(_SRC_DIR, "third_party", "polymer", "v0_8",
                                "components-chromium", "polymer-externs",
                                "polymer.externs.js")


class CompilerCustomizationTest(unittest.TestCase):
  _ASSERT_DEFINITION = Processor(_ASSERT_JS).contents
  _CR_DEFINE_DEFINITION = Processor(_CR_JS).contents
  _CR_UI_DECORATE_DEFINITION = Processor(_CR_UI_JS).contents

  def setUp(self):
    self._checker = Checker()

  def _runChecker(self, source_code):
    file_path = "/script.js"
    FileCache._cache[file_path] = source_code
    return self._checker.check(file_path, externs=[_POLYMER_EXTERNS])

  def _runCheckerTestExpectError(self, source_code, expected_error):
    _, stderr = self._runChecker(source_code)

    self.assertTrue(expected_error in stderr,
        msg="Expected chunk: \n%s\n\nOutput:\n%s\n" % (
            expected_error, stderr))

  def _runCheckerTestExpectSuccess(self, source_code):
    found_errors, stderr = self._runChecker(source_code)

    self.assertFalse(found_errors,
        msg="Expected success, but got failure\n\nOutput:\n%s\n" % stderr)

  def testGetInstance(self):
    self._runCheckerTestExpectError("""
var cr = {
  /** @param {!Function} ctor */
  addSingletonGetter: function(ctor) {
    ctor.getInstance = function() {
      return ctor.instance_ || (ctor.instance_ = new ctor());
    };
  }
};

/** @constructor */
function Class() {
  /** @param {number} num */
  this.needsNumber = function(num) {};
}

cr.addSingletonGetter(Class);
Class.getInstance().needsNumber("wrong type");
""", "ERROR - actual parameter 1 of Class.needsNumber does not match formal "
        "parameter")

  def testCrDefineFunctionDefinition(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @param {number} num */
  function internalName(num) {}

  return {
    needsNumber: internalName
  };
});

a.b.c.needsNumber("wrong type");
""", "ERROR - actual parameter 1 of a.b.c.needsNumber does not match formal "
        "parameter")

  def testCrDefineFunctionAssignment(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @param {number} num */
  var internalName = function(num) {};

  return {
    needsNumber: internalName
  };
});

a.b.c.needsNumber("wrong type");
""", "ERROR - actual parameter 1 of a.b.c.needsNumber does not match formal "
        "parameter")

  def testCrDefineConstructorDefinitionPrototypeMethod(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @constructor */
  function ClassInternalName() {}

  ClassInternalName.prototype = {
    /** @param {number} num */
    method: function(num) {}
  };

  return {
    ClassExternalName: ClassInternalName
  };
});

new a.b.c.ClassExternalName().method("wrong type");
""", "ERROR - actual parameter 1 of a.b.c.ClassExternalName.prototype.method "
        "does not match formal parameter")

  def testCrDefineConstructorAssignmentPrototypeMethod(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @constructor */
  var ClassInternalName = function() {};

  ClassInternalName.prototype = {
    /** @param {number} num */
    method: function(num) {}
  };

  return {
    ClassExternalName: ClassInternalName
  };
});

new a.b.c.ClassExternalName().method("wrong type");
""", "ERROR - actual parameter 1 of a.b.c.ClassExternalName.prototype.method "
        "does not match formal parameter")

  def testCrDefineEnum(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
cr.define('a.b.c', function() {
  /** @enum {string} */
  var internalNameForEnum = {key: 'wrong_type'};

  return {
    exportedEnum: internalNameForEnum
  };
});

/** @param {number} num */
function needsNumber(num) {}

needsNumber(a.b.c.exportedEnum.key);
""", "ERROR - actual parameter 1 of needsNumber does not match formal "
        "parameter")

  def testObjectDefineProperty(self):
    self._runCheckerTestExpectSuccess("""
/** @constructor */
function Class() {}

Object.defineProperty(Class.prototype, 'myProperty', {});

alert(new Class().myProperty);
""")

  def testCrDefineProperty(self):
    self._runCheckerTestExpectSuccess(self._CR_DEFINE_DEFINITION + """
/** @constructor */
function Class() {}

cr.defineProperty(Class.prototype, 'myProperty', cr.PropertyKind.JS);

alert(new Class().myProperty);
""")

  def testCrDefinePropertyTypeChecking(self):
    self._runCheckerTestExpectError(self._CR_DEFINE_DEFINITION + """
/** @constructor */
function Class() {}

cr.defineProperty(Class.prototype, 'booleanProp', cr.PropertyKind.BOOL_ATTR);

/** @param {number} num */
function needsNumber(num) {}

needsNumber(new Class().booleanProp);
""", "ERROR - actual parameter 1 of needsNumber does not match formal "
        "parameter")

  def testCrDefineOnCrWorks(self):
    self._runCheckerTestExpectSuccess(self._CR_DEFINE_DEFINITION + """
cr.define('cr', function() {
  return {};
});
""")

  def testAssertWorks(self):
    self._runCheckerTestExpectSuccess(self._ASSERT_DEFINITION + """
/** @return {?string} */
function f() {
  return "string";
}

/** @type {!string} */
var a = assert(f());
""")

  def testAssertInstanceofWorks(self):
    self._runCheckerTestExpectSuccess(self._ASSERT_DEFINITION + """
/** @constructor */
function Class() {}

/** @return {Class} */
function f() {
  var a = document.createElement('div');
  return assertInstanceof(a, Class);
}
""")

  def testCrUiDecorateWorks(self):
    self._runCheckerTestExpectSuccess(self._CR_DEFINE_DEFINITION +
        self._CR_UI_DECORATE_DEFINITION + """
/** @constructor */
function Class() {}

/** @return {Class} */
function f() {
  var a = document.createElement('div');
  cr.ui.decorate(a, Class);
  return a;
}
""")


if __name__ == "__main__":
  unittest.main()

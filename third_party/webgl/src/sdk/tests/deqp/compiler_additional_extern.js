/*
** Copyright (c) 2015 The Khronos Group Inc.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and/or associated documentation files (the
** "Materials"), to deal in the Materials without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Materials, and to
** permit persons to whom the Materials are furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be included
** in all copies or substantial portions of the Materials.
**
** THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
*/

/* Author: Mobica LTD */

/**
 * @param {string} msg
 */
function description(msg){};

function finishTest(){};

/**
 * @param {?string} name
 */
function setCurrentTestName(name){};

/**
 * @param {string} msg
 */
function bufferedLogToConsole(msg){};

/**
 * @constructor
 * @param {string} message The error message.
 */
var TestFailedException = function (message) {};

/**
 * @param {boolean} assertion
 * @param {?string} msg
 * @param {boolean} verbose
 * @param {boolean} exthrow
 */
function assertMsgOptions(assertion, msg, verbose, exthrow) {};

/**
 * @param {string} msg
 */
function debug(msg){};

/**
 * @param {string} msg
 * @param {boolean} exthrow
 */
function testFailedOptions(msg, exthrow){};

/**
 * @param {string} msg
 * @param {boolean} exthrow
 */
function testPassedOptions(msg, exthrow){};

/**
 * @param {string} msg
 */
function testFailed(msg){};

/**
 * @param {string} msg
 */
function testPassed(msg){};

/** @type {WebGL2RenderingContext} */ var gl;
/** @type {HTMLElement} */ var canvas;

/** @type {{create3DContext: function(string):WebGL2RenderingContext,
            loadTextFileAsync: function(string, function(boolean, string)) }} */ var WebGLTestUtils;

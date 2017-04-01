// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var childProcess = require("child_process");
var fs = require('fs');
var path = require('path');

var args = process.argv.slice(2);

var CLANG_FORMAT_PATH = path.resolve(__dirname, 'clang_format', 'index.js');
var IGNORE_FILE_PATH = path.resolve(__dirname, '..', '.eslintignore');

var ignoreFile = fs.readFileSync(IGNORE_FILE_PATH, 'utf-8');
var ignores = ignoreFile.split('\n').filter(str => str.length);
var ignoreArg = '--ignore=' + ignores.join(',');

console.log("Running clang-format");
var clangArgs = ['-i', ignoreArg];
if (args.length)
    clangArgs = clangArgs.concat(args);
else
    clangArgs.push('--glob=front_end/**/*.js');


var options = {
    cwd: path.resolve(__dirname, "..")
};

childProcess.fork(CLANG_FORMAT_PATH, clangArgs, options);

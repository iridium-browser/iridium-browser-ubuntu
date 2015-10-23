#!/bin/bash

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Reproduces the content of 'components' and 'components-chromium' using the
# list of dependencies from 'bower.json'. Downloads needed packages and makes
# Chromium specific modifications. To launch the script you need 'bower',
# 'crisper', and 'vulcanize' installed on your system.

# IMPORTANT NOTE: The new vulcanize must be installed from
# https://github.com/Polymer/vulcanize/releases since it isn't on npm yet.

set -e

cd "$(dirname "$0")"

rm -rf components components-chromium
rm -rf ../../web-animations-js/sources

bower install

mv components/web-animations-js ../../web-animations-js/sources
cp ../../web-animations-js/sources/COPYING ../../web-animations-js/LICENSE

# Remove unused gzipped binary which causes git-cl problems.
rm ../../web-animations-js/sources/web-animations.min.js.gz

# Remove source mapping directives since we don't compile the maps.
sed -i 's/^\s*\/\/#\s*sourceMappingURL.*//' \
  ../../web-animations-js/sources/*.min.js

# These components are needed only for demos and docs.
rm -rf components/{hydrolysis,marked,marked-element,prism,prism-element,\
iron-component-page,iron-doc-viewer,webcomponentsjs}

# Test and demo directories aren't needed.
rm -rf components/*/{test,demo}
rm -rf components/polymer/explainer

# Remove promise-polyfill and components which depend on it.
rm -rf components/promise-polyfill
rm -rf components/iron-ajax
rm -rf components/iron-form

# Remove iron-image as it's only a developer dependency of iron-dropdown.
# https://github.com/PolymerElements/iron-dropdown/pull/17
rm -rf components/iron-image

# Make checkperms.py happy.
find components/*/hero.svg -type f -exec chmod -x {} \;
find components/iron-selector -type f -exec chmod -x {} \;

# Remove carriage returns to make CQ happy.
find components -type f \( -name \*.html -o -name \*.css -o -name \*.js\
  -o -name \*.md -o -name \*.sh -o -name \*.json -o -name \*.gitignore\
  -o -name \*.bat \) -print0 | xargs -0 sed -i -e $'s/\r$//g'

# Resolve a unicode encoding issue in dom-innerHTML.html.
NBSP=$(python -c 'print u"\u00A0".encode("utf-8")')
sed -i 's/['"$NBSP"']/\\u00A0/g' components/polymer/polymer-mini.html

# Remove import of external resource in font-roboto (fonts.googleapis.com).
patch -p1 < chromium.patch

./extract_inline_scripts.sh components components-chromium

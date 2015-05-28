/* Copyright 2014 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/

<include src="../../../../ui/webui/resources/js/util.js">
<include src="../../../../ui/webui/resources/js/load_time_data.js">

/**
 * The amount of delay to use in the opt-in action in order to give time for
 * the fade-out animation to execute, before navigating to the opt-in URL,
 * in milliseconds.
 * @const
 */
var OPT_IN_DELAY_MS = 65;

/**
 * Once the DOM is loaded, determine if the header image is to be kept and
 * register a handler to add the 'hide' class to the container element in order
 * to hide it.
 */
document.addEventListener('DOMContentLoaded', function(event) {
  if (config['hideHeader']) {
    removeHeaderImages();
  }
  $('optin-button').addEventListener('click', function() {
    $('container').classList.add('hide');
    setTimeout(function() {
      location.hash = 'optin';
    }, OPT_IN_DELAY_MS);
  });
  $('optout-button').addEventListener('click', function() {
    location.hash = 'optout';
  });
});

/**
 * Returns the height of the content. Method called from Chrome to properly size
 * the view embedding it.
 * @return {number} The height of the content, in pixels.
 */
function getContentHeight() {
  return $('container').clientHeight;
}

/**
 * Removes all header images from the promo.
 */
function removeHeaderImages() {
  var images = document.querySelectorAll('.header-image');
  for (var i = 0, length = images.length; i < length; i++) {
    var image = images[i];
    var parent = image.parentElement;
    if (parent) {
      parent.removeChild(image);
    }
  }
}

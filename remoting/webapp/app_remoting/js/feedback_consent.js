// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {string} The app id (from the webstore) for this application.
 */
var appId = '';

/**
 * @type {string} The host id corresponding to the user's VM. The @pending
 *     place-holder instructs the Orchestrator to abandon any pending host,
 *     and is used if no host id is provided by the main window.
 */
var hostId = '@pending';

/**
 * @type {string} The network stats at the time the feedback consent dialog
 *     was shown.
 */
var connectionStats = '';

/**
 * @type {string} JSON representation of recent console error messages.
 */
var consoleErrors = '';

/**
 * @type {string} The most recent id for the session (unless the session is
 *     longer than 24hrs, this will be the only session id).
 */
var sessionId = '';

/**
 * @type {string} "no" => user did not request a VM reset; "yes" => VM was
 *     successfully reset; "failed" => user requested a reset, but it failed.
 */
var abandonHost = 'no';

/**
 * @type {Window} The main application window.
 */
var applicationWindow = null;

/**
 * @type {string} An unique identifier that links the feedback post with the
 *     logs uploaded by the host.
 */
var crashServiceReportId = '';

/**
 * @type {string} The user-selected feedback category, represented by its
 *     l10n tag.
 */
var selectedCategory = '';

/**
 * @param {string} email
 * @param {string} realName
 */
function onUserInfo(email, realName) {
  /** @type {number} Identifies this product to Google Feedback. **/
  var productId = 93407;

  /** @type {string} The base URL for Google Feedback. */
  var url = 'https://www.google.com/tools/feedback/survey/xhtml';

  /** @type {string} The feedback 'bucket', used for clustering. */
  var bucket = 'feedback';

  /** @type {string} The user's locale, used to localize the feedback page. */
  var locale = chrome.i18n.getMessage('@@ui_locale');

  window.open(url +
              '?productId=' + productId +
              '&bucket=' + escape(bucket) +
              '&hl=' + escape(locale) +
              '&psd_email=' + escape(email) +
              '&psd_hostId=' + escape(hostId) +
              '&psd_abandonHost=' + escape(abandonHost) +
              '&psd_crashServiceReportId=' + escape(crashServiceReportId) +
              '&psd_connectionStats=' + escape(connectionStats) +
              '&psd_category=' + escape(selectedCategory) +
              '&psd_sessionId=' + escape(sessionId));
  window.close();

  // If the VM was successfully abandoned, close the application.
  if (abandonHost == 'yes') {
    applicationWindow.close();
  }
};

/**
 * @param {boolean} waiting
 */
function setWaiting(waiting) {
  var ok = document.getElementById('feedback-consent-ok');
  var cancel = document.getElementById('feedback-consent-cancel');
  var abandon = document.getElementById('abandon-host');
  var working = document.getElementById('working');
  ok.disabled = waiting;
  cancel.disabled = waiting;
  abandon.disabled = waiting;
  working.hidden = !waiting;
}

function showError() {
  setWaiting(false);
  var error = document.getElementById('abandon-failed');
  var abandon = document.getElementById('abandon-host');
  var logs = document.getElementById('include-logs');
  var formBody = document.getElementById('form-body');
  error.hidden = false;
  abandon.checked = false;
  logs.checked = false;
  abandonHost = 'failed';
  crashServiceReportId = '';
  formBody.hidden = true;
  base.resizeWindowToContent(true);
}

/**
 * @return {string} A random string ID.
 */
function generateId() {
  var idArray = new Uint8Array(20);
  window.crypto.getRandomValues(idArray);
  return window.btoa(String.fromCharCode.apply(null, idArray));
}

/**
 * @param {string=} token
 */
function onToken(token) {
  var getUserInfo = function() {
    console.assert(Boolean(token), 'token is undefined.');
    var oauth2Api = new remoting.OAuth2ApiImpl();
    oauth2Api.getUserInfo(
        onUserInfo, onUserInfo.bind(null, 'unknown', 'unknown'),
        /** @type {string} */(token));
  };
  if (!token) {
    onUserInfo('unknown', 'unknown');
  } else {
    if (abandonHost == 'yes') {
      var body = {
        'abandonHost': 'true',
        'crashServiceReportId': crashServiceReportId
      };
      var uri = remoting.settings.APP_REMOTING_API_BASE_URL +
          '/applications/' + appId +
          '/hosts/'  + hostId +
          '/reportIssue';
      var onDone = function(/** !remoting.Xhr.Response */ response) {
        if (response.status >= 200 && response.status < 300) {
          getUserInfo();
        } else {
          showError();
        }
      };
      new remoting.Xhr({
        method: 'POST',
        url: uri,
        jsonContent: body,
        oauthToken: token
      }).start().then(onDone);
    } else {
      getUserInfo();
    }
  }
}

function onOk() {
  setWaiting(true);
  var abandon = /** @type {HTMLInputElement} */
      (document.getElementById('abandon-host'));
  if (abandon.checked) {
    abandonHost = 'yes';
  }
  chrome.identity.getAuthToken({ 'interactive': false }, onToken);
}

function onCancel() {
  window.close();
}

function onToggleAbandon() {
  var abandon = document.getElementById('abandon-host');
  var includeLogs = document.getElementById('include-logs');
  var includeLogsLabel = document.getElementById('include-logs-label');
  var learnMoreLink = document.getElementById('learn-more');
  includeLogs.disabled = !abandon.checked;
  if (abandon.checked) {
    includeLogsLabel.classList.remove('disabled');
    learnMoreLink.classList.remove('disabled');
  } else {
    includeLogsLabel.classList.add('disabled');
    learnMoreLink.classList.add('disabled');
  }
}

function onToggleLogs() {
  var includeLogs = document.getElementById('include-logs');
  if (includeLogs.checked) {
    crashServiceReportId = generateId();
  } else {
    crashServiceReportId = '';
  }
}

/** @param {Event} event */
function onLearnMore(event) {
  event.preventDefault();  // Clicking the link should not tick the checkbox.
  var learnMoreLink = document.getElementById('learn-more');
  var learnMoreInfobox = document.getElementById('privacy-info');
  learnMoreLink.hidden = true;
  learnMoreInfobox.hidden = false;
  base.resizeWindowToContent(true);
}

function onCategorySelect() {
  var feedbackCategory = /** @type {HTMLSelectElement} */
      (document.getElementById('feedback-category'));
  console.assert(feedbackCategory.selectedOptions.length == 1,
                'Expected exactly one selection; got ' +
                feedbackCategory.selectedOptions.length + '.');
  var selectedOption = /** @type {HTMLElement} */
      (feedbackCategory.selectedOptions[0]);
  selectedCategory = selectedOption.getAttribute('i18n-content');
  var selected = selectedCategory != 'FEEDBACK_CATEGORY_SELECT';
  document.getElementById('feedback-consent-ok').disabled = !selected;
  document.getElementById('form-body').hidden = !selected;
  base.resizeWindowToContent(false);
}

function onLoad() {
  window.addEventListener('message', onWindowMessage, false);
  remoting.settings = new remoting.Settings();
  l10n.localize();
  var ok = document.getElementById('feedback-consent-ok');
  var cancel = document.getElementById('feedback-consent-cancel');
  var abandon = document.getElementById('abandon-host-label');
  var includeLogs = document.getElementById('include-logs-label');
  var learnMoreLink = document.getElementById('learn-more');
  var feedbackCategory = document.getElementById('feedback-category');
  ok.addEventListener('click', onOk, false);
  cancel.addEventListener('click', onCancel, false);
  abandon.addEventListener('click', onToggleAbandon, false);
  includeLogs.addEventListener('click', onToggleLogs, false);
  learnMoreLink.addEventListener('click', onLearnMore, false);
  feedbackCategory.addEventListener('change', onCategorySelect, false);
  base.resizeWindowToContent(true);
}

/** @param {Event} event */
function onWindowMessage(event) {
  applicationWindow = event.source;
  var method = /** @type {string} */ (event.data['method']);
  if (method == 'init') {
    if (event.data['hostId']) {
      hostId = /** @type {string} */ (event.data['hostId']);
    }
    appId = /** @type {string} */ (event.data['appId']);
    connectionStats = /** @type {string} */ (event.data['connectionStats']);
    consoleErrors =  /** @type {string} */ (event.data['consoleErrors']);
    sessionId = /** @type {string} */ (event.data['sessionId']);
  }
};

window.addEventListener('load', onLoad, false);

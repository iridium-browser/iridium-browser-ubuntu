// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @unrestricted
 */
Security.SecurityModel = class extends SDK.SDKModel {
  /**
   * @param {!SDK.Target} target
   */
  constructor(target) {
    super(Security.SecurityModel, target);
    this._dispatcher = new Security.SecurityDispatcher(this);
    this._securityAgent = target.securityAgent();
    target.registerSecurityDispatcher(this._dispatcher);
    this._securityAgent.enable();
  }

  /**
   * @param {!SDK.Target} target
   * @return {?Security.SecurityModel}
   */
  static fromTarget(target) {
    var model = target.model(Security.SecurityModel);
    if (!model)
      model = new Security.SecurityModel(target);
    return model;
  }

  /**
   * @param {!Protocol.Security.SecurityState} a
   * @param {!Protocol.Security.SecurityState} b
   * @return {number}
   */
  static SecurityStateComparator(a, b) {
    var securityStateMap;
    if (Security.SecurityModel._symbolicToNumericSecurityState) {
      securityStateMap = Security.SecurityModel._symbolicToNumericSecurityState;
    } else {
      securityStateMap = new Map();
      var ordering = [
        Protocol.Security.SecurityState.Info, Protocol.Security.SecurityState.Insecure,
        Protocol.Security.SecurityState.Neutral, Protocol.Security.SecurityState.Warning,
        Protocol.Security.SecurityState.Secure,
        // Unknown is max so that failed/cancelled requests don't overwrite the origin security state for successful requests,
        // and so that failed/cancelled requests appear at the bottom of the origins list.
        Protocol.Security.SecurityState.Unknown
      ];
      for (var i = 0; i < ordering.length; i++)
        securityStateMap.set(ordering[i], i + 1);
      Security.SecurityModel._symbolicToNumericSecurityState = securityStateMap;
    }
    var aScore = securityStateMap.get(a) || 0;
    var bScore = securityStateMap.get(b) || 0;

    return aScore - bScore;
  }

  showCertificateViewer() {
    this._securityAgent.showCertificateViewer();
  }
};

/** @enum {symbol} */
Security.SecurityModel.Events = {
  SecurityStateChanged: Symbol('SecurityStateChanged')
};


/**
 * @unrestricted
 */
Security.PageSecurityState = class {
  /**
   * @param {!Protocol.Security.SecurityState} securityState
   * @param {boolean} schemeIsCryptographic
   * @param {!Array<!Protocol.Security.SecurityStateExplanation>} explanations
   * @param {?Protocol.Security.InsecureContentStatus} insecureContentStatus
   * @param {?string} summary
   */
  constructor(securityState, schemeIsCryptographic, explanations, insecureContentStatus, summary) {
    this.securityState = securityState;
    this.schemeIsCryptographic = schemeIsCryptographic;
    this.explanations = explanations;
    this.insecureContentStatus = insecureContentStatus;
    this.summary = summary;
  }
};

/**
 * @implements {Protocol.SecurityDispatcher}
 * @unrestricted
 */
Security.SecurityDispatcher = class {
  constructor(model) {
    this._model = model;
  }

  /**
   * @override
   * @param {!Protocol.Security.SecurityState} securityState
   * @param {boolean} schemeIsCryptographic
   * @param {!Array<!Protocol.Security.SecurityStateExplanation>} explanations
   * @param {!Protocol.Security.InsecureContentStatus} insecureContentStatus
   * @param {?string=} summary
   */
  securityStateChanged(securityState, schemeIsCryptographic, explanations, insecureContentStatus, summary) {
    var pageSecurityState = new Security.PageSecurityState(
        securityState, schemeIsCryptographic, explanations, insecureContentStatus, summary || null);
    this._model.dispatchEventToListeners(Security.SecurityModel.Events.SecurityStateChanged, pageSecurityState);
  }
};

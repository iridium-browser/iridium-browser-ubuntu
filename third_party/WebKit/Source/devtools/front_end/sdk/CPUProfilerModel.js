/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * @implements {Protocol.ProfilerDispatcher}
 * @unrestricted
 */
SDK.CPUProfilerModel = class extends SDK.SDKModel {
  /**
   * @param {!SDK.Target} target
   */
  constructor(target) {
    super(SDK.CPUProfilerModel, target);
    this._isRecording = false;
    target.registerProfilerDispatcher(this);
    target.profilerAgent().enable();

    this._configureCpuProfilerSamplingInterval();
    Common.moduleSetting('highResolutionCpuProfiling')
        .addChangeListener(this._configureCpuProfilerSamplingInterval, this);
  }

  _configureCpuProfilerSamplingInterval() {
    var intervalUs = Common.moduleSetting('highResolutionCpuProfiling').get() ? 100 : 1000;
    this.target().profilerAgent().setSamplingInterval(intervalUs);
  }

  /**
   * @override
   * @param {string} id
   * @param {!Protocol.Debugger.Location} scriptLocation
   * @param {string=} title
   */
  consoleProfileStarted(id, scriptLocation, title) {
    this._dispatchProfileEvent(SDK.CPUProfilerModel.Events.ConsoleProfileStarted, id, scriptLocation, title);
  }

  /**
   * @override
   * @param {string} id
   * @param {!Protocol.Debugger.Location} scriptLocation
   * @param {!Protocol.Profiler.Profile} cpuProfile
   * @param {string=} title
   */
  consoleProfileFinished(id, scriptLocation, cpuProfile, title) {
    this._dispatchProfileEvent(
        SDK.CPUProfilerModel.Events.ConsoleProfileFinished, id, scriptLocation, title, cpuProfile);
  }

  /**
   * @param {symbol} eventName
   * @param {string} id
   * @param {!Protocol.Debugger.Location} scriptLocation
   * @param {string=} title
   * @param {!Protocol.Profiler.Profile=} cpuProfile
   */
  _dispatchProfileEvent(eventName, id, scriptLocation, title, cpuProfile) {
    // Make sure ProfilesPanel is initialized and CPUProfileType is created.
    self.runtime.loadModulePromise('profiler').then(_ => {
      var debuggerModel =
          /** @type {!SDK.DebuggerModel} */ (SDK.DebuggerModel.fromTarget(this.target()));
      var debuggerLocation = SDK.DebuggerModel.Location.fromPayload(debuggerModel, scriptLocation);
      var globalId = this.target().id() + '.' + id;
      var data = /** @type {!SDK.CPUProfilerModel.EventData} */ (
          {id: globalId, scriptLocation: debuggerLocation, cpuProfile: cpuProfile, title: title});
      this.dispatchEventToListeners(eventName, data);
    });
  }

  /**
   * @return {boolean}
   */
  isRecordingProfile() {
    return this._isRecording;
  }

  startRecording() {
    this._isRecording = true;
    this.target().profilerAgent().start();
    Host.userMetrics.actionTaken(Host.UserMetrics.Action.ProfilesCPUProfileTaken);
  }

  /**
   * @return {!Promise.<?Protocol.Profiler.Profile>}
   */
  stopRecording() {
    /**
     * @param {?Protocol.Error} error
     * @param {?Protocol.Profiler.Profile} profile
     * @return {?Protocol.Profiler.Profile}
     */
    function extractProfile(error, profile) {
      return !error && profile ? profile : null;
    }
    this._isRecording = false;
    return this.target().profilerAgent().stop(extractProfile);
  }

  /**
   * @override
   */
  dispose() {
    Common.moduleSetting('highResolutionCpuProfiling')
        .removeChangeListener(this._configureCpuProfilerSamplingInterval, this);
  }
};

/** @enum {symbol} */
SDK.CPUProfilerModel.Events = {
  ConsoleProfileStarted: Symbol('ConsoleProfileStarted'),
  ConsoleProfileFinished: Symbol('ConsoleProfileFinished')
};

/** @typedef {!{id: string, scriptLocation: !SDK.DebuggerModel.Location, title: (string|undefined), cpuProfile: (!Protocol.Profiler.Profile|undefined)}} */
SDK.CPUProfilerModel.EventData;

/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @unrestricted
 */
Workspace.IsolatedFileSystemManager = class extends Common.Object {
  constructor() {
    super();

    /** @type {!Map<string, !Workspace.IsolatedFileSystem>} */
    this._fileSystems = new Map();
    /** @type {!Map<number, function(!Array.<string>)>} */
    this._callbacks = new Map();
    /** @type {!Map<number, !Common.Progress>} */
    this._progresses = new Map();

    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.FileSystemRemoved, this._onFileSystemRemoved, this);
    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.FileSystemAdded, this._onFileSystemAdded, this);
    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.FileSystemFilesChanged, this._onFileSystemFilesChanged, this);
    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.IndexingTotalWorkCalculated, this._onIndexingTotalWorkCalculated, this);
    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.IndexingWorked, this._onIndexingWorked, this);
    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.IndexingDone, this._onIndexingDone, this);
    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.SearchCompleted, this._onSearchCompleted, this);

    this._initExcludePatterSetting();

    this._fileSystemsLoadedPromise = this._requestFileSystems();
  }

  /**
   * @return {!Promise<!Array<!Workspace.IsolatedFileSystem>>}
   */
  _requestFileSystems() {
    var fulfill;
    var promise = new Promise(f => fulfill = f);
    InspectorFrontendHost.events.addEventListener(
        InspectorFrontendHostAPI.Events.FileSystemsLoaded, onFileSystemsLoaded, this);
    InspectorFrontendHost.requestFileSystems();
    return promise;

    /**
     * @param {!Common.Event} event
     * @this {Workspace.IsolatedFileSystemManager}
     */
    function onFileSystemsLoaded(event) {
      var fileSystems = /** @type {!Array.<!Workspace.IsolatedFileSystemManager.FileSystem>} */ (event.data);
      var promises = [];
      for (var i = 0; i < fileSystems.length; ++i)
        promises.push(this._innerAddFileSystem(fileSystems[i], false));
      Promise.all(promises).then(onFileSystemsAdded);
    }

    /**
     * @param {!Array<?Workspace.IsolatedFileSystem>} fileSystems
     */
    function onFileSystemsAdded(fileSystems) {
      fulfill(fileSystems.filter(fs => !!fs));
    }
  }

  addFileSystem() {
    InspectorFrontendHost.addFileSystem('');
  }

  /**
   * @param {!Workspace.IsolatedFileSystem} fileSystem
   */
  removeFileSystem(fileSystem) {
    InspectorFrontendHost.removeFileSystem(fileSystem.embedderPath());
  }

  /**
   * @return {!Promise<!Array<!Workspace.IsolatedFileSystem>>}
   */
  waitForFileSystems() {
    return this._fileSystemsLoadedPromise;
  }

  /**
   * @param {!Workspace.IsolatedFileSystemManager.FileSystem} fileSystem
   * @param {boolean} dispatchEvent
   * @return {!Promise<?Workspace.IsolatedFileSystem>}
   */
  _innerAddFileSystem(fileSystem, dispatchEvent) {
    var embedderPath = fileSystem.fileSystemPath;
    var fileSystemURL = Common.ParsedURL.platformPathToURL(fileSystem.fileSystemPath);
    var promise = Workspace.IsolatedFileSystem.create(
        this, fileSystemURL, embedderPath, fileSystem.fileSystemName, fileSystem.rootURL);
    return promise.then(storeFileSystem.bind(this));

    /**
     * @param {?Workspace.IsolatedFileSystem} fileSystem
     * @this {Workspace.IsolatedFileSystemManager}
     */
    function storeFileSystem(fileSystem) {
      if (!fileSystem)
        return null;
      this._fileSystems.set(fileSystemURL, fileSystem);
      if (dispatchEvent)
        this.dispatchEventToListeners(Workspace.IsolatedFileSystemManager.Events.FileSystemAdded, fileSystem);
      return fileSystem;
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _onFileSystemAdded(event) {
    var errorMessage = /** @type {string} */ (event.data['errorMessage']);
    var fileSystem = /** @type {?Workspace.IsolatedFileSystemManager.FileSystem} */ (event.data['fileSystem']);
    if (errorMessage)
      Common.console.error(errorMessage);
    else if (fileSystem)
      this._innerAddFileSystem(fileSystem, true);
  }

  /**
   * @param {!Common.Event} event
   */
  _onFileSystemRemoved(event) {
    var embedderPath = /** @type {string} */ (event.data);
    var fileSystemPath = Common.ParsedURL.platformPathToURL(embedderPath);
    var isolatedFileSystem = this._fileSystems.get(fileSystemPath);
    if (!isolatedFileSystem)
      return;
    this._fileSystems.delete(fileSystemPath);
    isolatedFileSystem.fileSystemRemoved();
    this.dispatchEventToListeners(Workspace.IsolatedFileSystemManager.Events.FileSystemRemoved, isolatedFileSystem);
  }

  /**
   * @param {!Common.Event} event
   */
  _onFileSystemFilesChanged(event) {
    var embedderPaths = /** @type {!Array<string>} */ (event.data);
    var paths = embedderPaths.map(embedderPath => Common.ParsedURL.platformPathToURL(embedderPath));
    this.dispatchEventToListeners(Workspace.IsolatedFileSystemManager.Events.FileSystemFilesChanged, paths);
  }

  /**
   * @return {!Array<!Workspace.IsolatedFileSystem>}
   */
  fileSystems() {
    return this._fileSystems.valuesArray();
  }

  /**
   * @param {string} fileSystemPath
   * @return {?Workspace.IsolatedFileSystem}
   */
  fileSystem(fileSystemPath) {
    return this._fileSystems.get(fileSystemPath) || null;
  }

  _initExcludePatterSetting() {
    var defaultCommonExcludedFolders = [
      '/node_modules/', '/bower_components/', '/\\.devtools', '/\\.git/', '/\\.sass-cache/', '/\\.hg/', '/\\.idea/',
      '/\\.svn/', '/\\.cache/', '/\\.project/'
    ];
    var defaultWinExcludedFolders = ['/Thumbs.db$', '/ehthumbs.db$', '/Desktop.ini$', '/\\$RECYCLE.BIN/'];
    var defaultMacExcludedFolders = [
      '/\\.DS_Store$', '/\\.Trashes$', '/\\.Spotlight-V100$', '/\\.AppleDouble$', '/\\.LSOverride$', '/Icon$',
      '/\\._.*$'
    ];
    var defaultLinuxExcludedFolders = ['/.*~$'];
    var defaultExcludedFolders = defaultCommonExcludedFolders;
    if (Host.isWin())
      defaultExcludedFolders = defaultExcludedFolders.concat(defaultWinExcludedFolders);
    else if (Host.isMac())
      defaultExcludedFolders = defaultExcludedFolders.concat(defaultMacExcludedFolders);
    else
      defaultExcludedFolders = defaultExcludedFolders.concat(defaultLinuxExcludedFolders);
    var defaultExcludedFoldersPattern = defaultExcludedFolders.join('|');
    this._workspaceFolderExcludePatternSetting = Common.settings.createRegExpSetting(
        'workspaceFolderExcludePattern', defaultExcludedFoldersPattern, Host.isWin() ? 'i' : '');
  }

  /**
   * @return {!Common.Setting}
   */
  workspaceFolderExcludePatternSetting() {
    return this._workspaceFolderExcludePatternSetting;
  }

  /**
   * @param {function(!Array.<string>)} callback
   * @return {number}
   */
  registerCallback(callback) {
    var requestId = ++Workspace.IsolatedFileSystemManager._lastRequestId;
    this._callbacks.set(requestId, callback);
    return requestId;
  }

  /**
   * @param {!Common.Progress} progress
   * @return {number}
   */
  registerProgress(progress) {
    var requestId = ++Workspace.IsolatedFileSystemManager._lastRequestId;
    this._progresses.set(requestId, progress);
    return requestId;
  }

  /**
   * @param {!Common.Event} event
   */
  _onIndexingTotalWorkCalculated(event) {
    var requestId = /** @type {number} */ (event.data['requestId']);
    var totalWork = /** @type {number} */ (event.data['totalWork']);

    var progress = this._progresses.get(requestId);
    if (!progress)
      return;
    progress.setTotalWork(totalWork);
  }

  /**
   * @param {!Common.Event} event
   */
  _onIndexingWorked(event) {
    var requestId = /** @type {number} */ (event.data['requestId']);
    var worked = /** @type {number} */ (event.data['worked']);

    var progress = this._progresses.get(requestId);
    if (!progress)
      return;
    progress.worked(worked);
    if (progress.isCanceled()) {
      InspectorFrontendHost.stopIndexing(requestId);
      this._onIndexingDone(event);
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _onIndexingDone(event) {
    var requestId = /** @type {number} */ (event.data['requestId']);

    var progress = this._progresses.get(requestId);
    if (!progress)
      return;
    progress.done();
    this._progresses.delete(requestId);
  }

  /**
   * @param {!Common.Event} event
   */
  _onSearchCompleted(event) {
    var requestId = /** @type {number} */ (event.data['requestId']);
    var files = /** @type {!Array.<string>} */ (event.data['files']);

    var callback = this._callbacks.get(requestId);
    if (!callback)
      return;
    callback.call(null, files);
    this._callbacks.delete(requestId);
  }
};

/** @typedef {!{fileSystemName: string, rootURL: string, fileSystemPath: string}} */
Workspace.IsolatedFileSystemManager.FileSystem;

/** @enum {symbol} */
Workspace.IsolatedFileSystemManager.Events = {
  FileSystemAdded: Symbol('FileSystemAdded'),
  FileSystemRemoved: Symbol('FileSystemRemoved'),
  FileSystemFilesChanged: Symbol('FileSystemFilesChanged'),
  ExcludedFolderAdded: Symbol('ExcludedFolderAdded'),
  ExcludedFolderRemoved: Symbol('ExcludedFolderRemoved')
};

Workspace.IsolatedFileSystemManager._lastRequestId = 0;

/**
 * @type {!Workspace.IsolatedFileSystemManager}
 */
Workspace.isolatedFileSystemManager;

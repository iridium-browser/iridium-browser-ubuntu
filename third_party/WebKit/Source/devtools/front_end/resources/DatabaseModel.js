/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @unrestricted
 */
Resources.Database = class {
  /**
   * @param {!Resources.DatabaseModel} model
   * @param {string} id
   * @param {string} domain
   * @param {string} name
   * @param {string} version
   */
  constructor(model, id, domain, name, version) {
    this._model = model;
    this._id = id;
    this._domain = domain;
    this._name = name;
    this._version = version;
  }

  /** @return {string} */
  get id() {
    return this._id;
  }

  /** @return {string} */
  get name() {
    return this._name;
  }

  /** @param {string} x */
  set name(x) {
    this._name = x;
  }

  /** @return {string} */
  get version() {
    return this._version;
  }

  /** @param {string} x */
  set version(x) {
    this._version = x;
  }

  /** @return {string} */
  get domain() {
    return this._domain;
  }

  /** @param {string} x */
  set domain(x) {
    this._domain = x;
  }

  /**
   * @param {function(!Array.<string>)} callback
   */
  getTableNames(callback) {
    function sortingCallback(error, names) {
      if (!error)
        callback(names.sort());
    }
    this._model._agent.getDatabaseTableNames(this._id, sortingCallback);
  }

  /**
   * @param {string} query
   * @param {function(!Array.<string>=, !Array.<*>=)} onSuccess
   * @param {function(string)} onError
   */
  executeSql(query, onSuccess, onError) {
    /**
     * @param {?Protocol.Error} error
     * @param {!Array.<string>=} columnNames
     * @param {!Array.<*>=} values
     * @param {!Protocol.Database.Error=} errorObj
     */
    function callback(error, columnNames, values, errorObj) {
      if (error) {
        onError(error);
        return;
      }
      if (errorObj) {
        var message;
        if (errorObj.message)
          message = errorObj.message;
        else if (errorObj.code === 2)
          message = Common.UIString('Database no longer has expected version.');
        else
          message = Common.UIString('An unexpected error %s occurred.', errorObj.code);
        onError(message);
        return;
      }
      onSuccess(columnNames, values);
    }
    this._model._agent.executeSQL(this._id, query, callback);
  }
};

/**
 * @unrestricted
 */
Resources.DatabaseModel = class extends SDK.SDKModel {
  /**
   * @param {!SDK.Target} target
   */
  constructor(target) {
    super(Resources.DatabaseModel, target);

    this._databases = [];
    this._agent = target.databaseAgent();
    this.target().registerDatabaseDispatcher(new Resources.DatabaseDispatcher(this));
  }

  /**
   * @param {!SDK.Target} target
   * @return {!Resources.DatabaseModel}
   */
  static fromTarget(target) {
    if (!target[Resources.DatabaseModel._symbol])
      target[Resources.DatabaseModel._symbol] = new Resources.DatabaseModel(target);

    return target[Resources.DatabaseModel._symbol];
  }

  enable() {
    if (this._enabled)
      return;
    this._agent.enable();
    this._enabled = true;
  }

  disable() {
    if (!this._enabled)
      return;
    this._enabled = false;
    this._databases = [];
    this._agent.disable();
    this.emit(new Resources.DatabaseModel.DatabasesRemovedEvent());
  }

  /**
   * @return {!Array.<!Resources.Database>}
   */
  databases() {
    var result = [];
    for (var database of this._databases)
      result.push(database);
    return result;
  }

  /**
   * @param {!Resources.Database} database
   */
  _addDatabase(database) {
    this._databases.push(database);
    this.emit(new Resources.DatabaseModel.DatabaseAddedEvent(database));
  }
};

/** @implements {Common.Emittable} */
Resources.DatabaseModel.DatabaseAddedEvent = class {
  /**
   * @param {!Resources.Database} database
   */
  constructor(database) {
    this.database = database;
  }
};

/** @implements {Common.Emittable} */
Resources.DatabaseModel.DatabasesRemovedEvent = class {};

/**
 * @implements {Protocol.DatabaseDispatcher}
 * @unrestricted
 */
Resources.DatabaseDispatcher = class {
  /**
   * @param {!Resources.DatabaseModel} model
   */
  constructor(model) {
    this._model = model;
  }

  /**
   * @override
   * @param {!Protocol.Database.Database} payload
   */
  addDatabase(payload) {
    this._model._addDatabase(
        new Resources.Database(this._model, payload.id, payload.domain, payload.name, payload.version));
  }
};

Resources.DatabaseModel._symbol = Symbol('DatabaseModel');

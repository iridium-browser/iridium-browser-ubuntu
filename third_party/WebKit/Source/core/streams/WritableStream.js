// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of WritableStream for Blink.  See
// https://streams.spec.whatwg.org/#ws. The implementation closely follows the
// standard, except where required for performance or integration with Blink. In
// particular, classes, methods and abstract operations are implemented in the
// same order as in the standard, to simplify side-by-side reading.

(function(global, binding, v8) {
  'use strict';

  // Private symbols. These correspond to the internal slots in the standard.
  // "[[X]]" in the standard is spelt _X in this implementation.
  const _pendingWriteRequest = v8.createPrivateSymbol('[[pendingWriteRequest]]');
  const _pendingCloseRequest = v8.createPrivateSymbol('[[pendingCloseRequest]]');
  const _pendingAbortRequest = v8.createPrivateSymbol('[[pendingAbortRequest]]');
  const _state = v8.createPrivateSymbol('[[state]]');
  const _storedError = v8.createPrivateSymbol('[[storedError]]');
  const _writer = v8.createPrivateSymbol('[[writer]]');
  const _writableStreamController =
      v8.createPrivateSymbol('[[writableStreamController]]');
  const _writeRequests = v8.createPrivateSymbol('[[writeRequests]]');
  const _closedPromise = v8.createPrivateSymbol('[[closedPromise]]');
  const _ownerWritableStream =
      v8.createPrivateSymbol('[[ownerWritableStream]]');
  const _readyPromise = v8.createPrivateSymbol('[[readyPromise]]');
  const _controlledWritableStream =
      v8.createPrivateSymbol('[[controlledWritableStream]]');
  const _queue = v8.createPrivateSymbol('[[queue]]');
  const _queueSize = v8.createPrivateSymbol('[[queueSize]]');
  const _strategyHWM = v8.createPrivateSymbol('[[strategyHWM]]');
  const _strategySize = v8.createPrivateSymbol('[[strategySize]]');
  const _underlyingSink = v8.createPrivateSymbol('[[underlyingSink]]');

  // _defaultControllerFlags combines WritableStreamDefaultController's internal
  // slots [[started]], [[writing]], and [[inClose]] into a single bitmask for
  // efficiency.
  const _defaultControllerFlags =
      v8.createPrivateSymbol('[[defaultControllerFlags]]');
  const FLAG_STARTED = 0b1;
  const FLAG_WRITING = 0b10;
  const FLAG_INCLOSE = 0b100;

  // For efficiency, WritableStream [[state]] contains numeric values.
  const WRITABLE = 0;
  const CLOSING = 1;
  const CLOSED = 2;
  const ERRORED = 3;

  // Javascript functions. It is important to use these copies, as the ones on
  // the global object may have been overwritten. See "V8 Extras Design Doc",
  // section "Security Considerations".
  // https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0/edit#heading=h.9yixony1a18r
  const undefined = global.undefined;

  const defineProperty = global.Object.defineProperty;
  const hasOwnProperty = v8.uncurryThis(global.Object.hasOwnProperty);

  const Function_apply = v8.uncurryThis(global.Function.prototype.apply);

  const TypeError = global.TypeError;
  const RangeError = global.RangeError;

  const Boolean = global.Boolean;
  const Number = global.Number;
  const Number_isNaN = Number.isNaN;
  const Number_isFinite = Number.isFinite;

  const Promise = global.Promise;
  const thenPromise = v8.uncurryThis(Promise.prototype.then);
  const Promise_resolve = v8.simpleBind(Promise.resolve, Promise);
  const Promise_reject = v8.simpleBind(Promise.reject, Promise);

  // User-visible strings.
  const streamErrors = binding.streamErrors;
  const errAbortLockedStream = 'Cannot abort a writable stream that is locked to a writer';
  const errStreamAborted = 'The stream has been aborted';
  const errWriterLockReleasedPrefix = 'This writable stream writer has been released and cannot be ';
  const errCloseCloseRequestedStream =
      'Cannot close a writable stream that has already been requested to be closed';
  const errWriteCloseRequestedStream =
      'Cannot write to a writable stream that is due to be closed';
  const templateErrorCannotActionOnStateStream =
      (action, state) => `Cannot ${action} a ${state} writable stream`;
  const errReleasedWriterClosedPromise =
      'This writable stream writer has been released and cannot be used to monitor the stream\'s state';
  const templateErrorIsNotAFunction = f => `${f} is not a function`;

  // These verbs are used after errWriterLockReleasedPrefix
  const verbUsedToGetTheDesiredSize = 'used to get the desiredSize';
  const verbAborted = 'aborted';
  const verbClosed = 'closed';
  const verbWrittenTo = 'written to';

  // Utility functions (not from the standard).
  function createWriterLockReleasedError(verb) {
    return new TypeError(errWriterLockReleasedPrefix + verb);
  }

  const stateNames = {[CLOSED]: 'closed', [ERRORED]: 'errored'};
  function createCannotActionOnStateStreamError(action, state) {
    TEMP_ASSERT(stateNames[state] !== undefined,
                `name for state ${state} exists in stateNames`);
    return new TypeError(
        templateErrorCannotActionOnStateStream(action, stateNames[state]));
  }

  function setDefaultControllerFlag(controller, flag, value) {
    let flags = controller[_defaultControllerFlags];
    if (value) {
      flags = flags | flag;
    } else {
      flags = flags & ~flag;
    }
    controller[_defaultControllerFlags] = flags;
  }

  function getDefaultControllerStartedFlag(controller) {
    return Boolean(controller[_defaultControllerFlags] & FLAG_STARTED);
  }

  function setDefaultControllerStartedFlag(controller, value) {
    setDefaultControllerFlag(controller, FLAG_STARTED, value);
  }

  function getDefaultControllerWritingFlag(controller) {
    return Boolean(controller[_defaultControllerFlags] & FLAG_WRITING);
  }

  function setDefaultControllerWritingFlag(controller, value) {
    setDefaultControllerFlag(controller, FLAG_WRITING, value);
  }

  function getDefaultControllerInCloseFlag(controller) {
    return Boolean(controller[_defaultControllerFlags] & FLAG_INCLOSE);
  }

  function setDefaultControllerInCloseFlag(controller, value) {
    setDefaultControllerFlag(controller, FLAG_INCLOSE, value);
  }

  function rejectPromises(array, e) {
    // array is an InternalPackedArray so forEach won't work.
    for (let i = 0; i < array.length; ++i) {
      v8.rejectPromise(array[i], e);
    }
  }

  // https://tc39.github.io/ecma262/#sec-ispropertykey
  // TODO(ricea): Remove this when the asserts using it are removed.
  function IsPropertyKey(argument) {
    return typeof argument === 'string' || typeof argument === 'symbol';
  }

  // TODO(ricea): Remove all asserts once the implementation has stabilised.
  function TEMP_ASSERT(predicate, message) {
    if (predicate) {
      return;
    }
    v8.log(`Assertion failed: ${message}\n`);
    v8.logStackTrace();
    class WritableStreamInternalError extends Error {
      constructor(message) {
        super(message);
      }
    }
    throw new WritableStreamInternalError(message);
  }

  class WritableStream {
    constructor(underlyingSink = {}, { size, highWaterMark = 1 } = {}) {
      this[_state] = WRITABLE;
      this[_storedError] = undefined;
      this[_writer] = undefined;
      this[_writableStreamController] = undefined;
      this[_pendingWriteRequest] = undefined;
      this[_pendingCloseRequest] = undefined;
      this[_pendingAbortRequest] = undefined;
      this[_writeRequests] = new v8.InternalPackedArray();
      const type = underlyingSink.type;
      if (type !== undefined) {
        throw new RangeError(streamErrors.invalidType);
      }
      this[_writableStreamController] =
          new WritableStreamDefaultController(this, underlyingSink, size,
                                              highWaterMark);
    }

    get locked() {
      if (!IsWritableStream(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }
      return IsWritableStreamLocked(this);
    }

    abort(reason) {
      if (!IsWritableStream(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      if (IsWritableStreamLocked(this)) {
        return Promise_reject(new TypeError(errAbortLockedStream));
      }
      return WritableStreamAbort(this, reason);
    }

    getWriter() {
      if (!IsWritableStream(this)) {
         throw new TypeError(streamErrors.illegalInvocation);
      }
      return AcquireWritableStreamDefaultWriter(this);
    }
  }

  // General Writable Stream Abstract Operations

  function AcquireWritableStreamDefaultWriter(stream) {
    return new WritableStreamDefaultWriter(stream);
  }

  function IsWritableStream(x) {
    return hasOwnProperty(x, _writableStreamController);
  }

  function IsWritableStreamLocked(stream) {
    TEMP_ASSERT(IsWritableStream(stream),
                '! IsWritableStream(stream) is true.');
    return stream[_writer] !== undefined;
  }

  function WritableStreamAbort(stream, reason) {
    const state = stream[_state];
    if (state === CLOSED) {
      return Promise_resolve(undefined);
    }
    if (state === ERRORED) {
      return Promise_reject(stream[_storedError]);
    }
    TEMP_ASSERT(state === WRITABLE || state === CLOSING,
               'state is "writable" or "closing".');
    const error = new TypeError(errStreamAborted);
    WritableStreamError(stream, error);

    const controller = stream[_writableStreamController];
    TEMP_ASSERT(controller !== undefined,
                'controller is not undefined');

    const isWriting = getDefaultControllerWritingFlag(controller);
    if (isWriting || getDefaultControllerInCloseFlag(controller)) {
      const promise =  v8.createPromise();
      stream[_pendingAbortRequest] = promise;

      if (isWriting) {
        return thenPromise(promise, () => {
          return WritableStreamDefaultControllerAbort(controller, reason);
        });
      }
      return promise;
    }

    return WritableStreamDefaultControllerAbort(controller, reason);
  }

  // Writable Stream Abstract Operations Used by Controllers

  function WritableStreamAddWriteRequest(stream) {
    TEMP_ASSERT(IsWritableStreamLocked(stream),
                '! IsWritableStreamLocked(writer) is true.');
    TEMP_ASSERT(stream[_state] === WRITABLE,
                'stream.[[state]] is "writable".');
    const promise = v8.createPromise();
    stream[_writeRequests].push(promise);
    return promise;
  }

  function WritableStreamError(stream, e) {
    const oldState = stream[_state];
    TEMP_ASSERT(oldState === WRITABLE || oldState === CLOSING,
                'oldState is "writable" or "closing".');

    stream[_state] = ERRORED;
    stream[_storedError] = e;

    const controller = stream[_writableStreamController];
    if (controller === undefined ||
        (!getDefaultControllerWritingFlag(controller) &&
         !getDefaultControllerInCloseFlag(controller))) {
      WritableStreamRejectPromisesInReactionToError(stream);
    }

    const writer = stream[_writer];
    if (writer !== undefined) {
      if (oldState === WRITABLE &&
          WritableStreamDefaultControllerGetBackpressure(controller) === true) {
        v8.rejectPromise(writer[_readyPromise], e);
      } else {
        writer[_readyPromise] = Promise_reject(e);
      }
      v8.markPromiseAsHandled(writer[_readyPromise]);
    }
  }

  function WritableStreamFinishClose(stream) {
    const state = stream[_state];
    TEMP_ASSERT(state === CLOSING || state === ERRORED,
                'state is "closing" or "errored"');

    const writer = stream[_writer];
    if (state === CLOSING) {
      if (writer !== undefined) {
        v8.resolvePromise(writer[_closedPromise], undefined);
      }
      stream[_state] = CLOSED;
    } else if (writer !== undefined) {
      TEMP_ASSERT(state === ERRORED, 'state is "errored"');
      v8.rejectPromise(writer[_closedPromise], stream[_storedError]);
      v8.markPromiseAsHandled(writer[_closedPromise]);
    }

    if (stream[_pendingAbortRequest] !== undefined) {
      v8.resolvePromise(stream[_pendingAbortRequest], undefined);
      stream[_pendingAbortRequest] = undefined;
    }
  }

  function WritableStreamRejectPromisesInReactionToError(stream) {
    TEMP_ASSERT(stream[_state] === ERRORED, 'stream.[[state]] is "errored"');
    TEMP_ASSERT(stream[_pendingWriteRequest] === undefined,
                'stream.[[pendingWriteRequest]] is undefined');

    const storedError = stream[_storedError];
    rejectPromises(stream[_writeRequests], storedError);
    stream[_writeRequests] = new v8.InternalPackedArray();

    if (stream[_pendingCloseRequest] !== undefined) {
      TEMP_ASSERT(
          getDefaultControllerInCloseFlag(stream[_writableStreamController]) ===
          false, 'stream.[[writableStreamController]].[[inClose]] === false');
      v8.rejectPromise(stream[_pendingCloseRequest], storedError);
      stream[_pendingCloseRequest] = undefined;
    }

    const writer = stream[_writer];
    if (writer !== undefined) {
      v8.rejectPromise(writer[_closedPromise], storedError);
      v8.markPromiseAsHandled(writer[_closedPromise]);
    }
  }

  function WritableStreamUpdateBackpressure(stream, backpressure) {
    TEMP_ASSERT(stream[_state] === WRITABLE,
                'stream.[[state]] is "writable".');
    const writer = stream[_writer];
    if (writer === undefined) {
      return;
    }
    if (backpressure) {
      writer[_readyPromise] = v8.createPromise();
    } else {
      TEMP_ASSERT(backpressure === false,
                  'backpressure is false.');
      v8.resolvePromise(writer[_readyPromise], undefined);
    }
  }

  // Functions to expose internals for ReadableStream.pipeTo. These are not
  // part of the standard.
  function isWritableStreamErrored(stream) {
    TEMP_ASSERT(
        IsWritableStream(stream), '! IsWritableStream(stream) is true.');
    return stream[_state] === ERRORED;
  }

  function isWritableStreamClosingOrClosed(stream) {
    TEMP_ASSERT(
        IsWritableStream(stream), '! IsWritableStream(stream) is true.');
    return stream[_state] === CLOSING || stream[_state] === CLOSED;
  }

  function getWritableStreamStoredError(stream) {
    TEMP_ASSERT(
        IsWritableStream(stream), '! IsWritableStream(stream) is true.');
    return stream[_storedError];
  }

  class WritableStreamDefaultWriter {
    constructor(stream) {
      if (!IsWritableStream(stream)) {
        throw new TypeError(streamErrors.illegalConstructor);
      }
      if (IsWritableStreamLocked(stream)) {
        throw new TypeError(streamErrors.illegalConstructor);
      }
      this[_ownerWritableStream] = stream;
      stream[_writer] = this;
      const state = stream[_state];
      if (state === WRITABLE || state === CLOSING) {
        this[_closedPromise] = v8.createPromise();
      } else if (state === CLOSED) {
        this[_closedPromise] = Promise_resolve(undefined);
      } else {
        TEMP_ASSERT(state === ERRORED,
                    'state is "errored".');
        this[_closedPromise] = Promise_reject(stream[_storedError]);
        v8.markPromiseAsHandled(this[_closedPromise]);
      }
      if (state === WRITABLE &&
          WritableStreamDefaultControllerGetBackpressure(
              stream[_writableStreamController])) {
        this[_readyPromise] = v8.createPromise();
      } else {
        this[_readyPromise] = Promise_resolve(undefined);
      }
    }

    get closed() {
      if (!IsWritableStreamDefaultWriter(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      return this[_closedPromise];
    }

    get desiredSize() {
      if (!IsWritableStreamDefaultWriter(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }
      if (this[_ownerWritableStream] === undefined) {
        throw createWriterLockReleasedError(verbUsedToGetTheDesiredSize);
      }
      return WritableStreamDefaultWriterGetDesiredSize(this);
    }

    get ready() {
      if (!IsWritableStreamDefaultWriter(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      return this[_readyPromise];
    }

    abort(reason) {
     if (!IsWritableStreamDefaultWriter(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      if (this[_ownerWritableStream] === undefined) {
        return Promise_reject(createWriterLockReleasedError(verbAborted));
      }
      return WritableStreamDefaultWriterAbort(this, reason);
    }

    close() {
      if (!IsWritableStreamDefaultWriter(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      const stream = this[_ownerWritableStream];
      if (stream === undefined) {
        return Promise_reject(createWriterLockReleasedError(verbClosed));
      }
      if (stream[_state] === CLOSING) {
        return Promise_reject(new TypeError(errCloseCloseRequestedStream));
      }
      return WritableStreamDefaultWriterClose(this);
    }

    releaseLock() {
      if (!IsWritableStreamDefaultWriter(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }
      const stream = this[_ownerWritableStream];
      if (stream === undefined) {
        return;
      }
      TEMP_ASSERT(stream[_writer] !== undefined,
                  'stream.[[writer]] is not undefined.');
      WritableStreamDefaultWriterRelease(this);
    }

    write(chunk) {
      if (!IsWritableStreamDefaultWriter(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      const stream = this[_ownerWritableStream];
      if (stream === undefined) {
        return Promise_reject(createWriterLockReleasedError(verbWrittenTo));
      }
      if (stream[_state] === CLOSING) {
        return Promise_reject(new TypeError(errWriteCloseRequestedStream));
      }
      return WritableStreamDefaultWriterWrite(this, chunk);
    }
  }

  // Writable Stream Writer Abstract Operations

  function IsWritableStreamDefaultWriter(x) {
    return hasOwnProperty(x, _ownerWritableStream);
  }

  function WritableStreamDefaultWriterAbort(writer, reason) {
    const stream = writer[_ownerWritableStream];
    TEMP_ASSERT(stream !== undefined,
                'stream is not undefined.');
    return WritableStreamAbort(stream, reason);
  }

  function WritableStreamDefaultWriterClose(writer) {
    const stream = writer[_ownerWritableStream];
    TEMP_ASSERT(stream !== undefined,
                'stream is not undefined.');
    const state = stream[_state];
    if (state === CLOSED || state === ERRORED) {
      return Promise_reject(
          createCannotActionOnStateStreamError('close', state));
    }
    TEMP_ASSERT(state === WRITABLE,
                'state is "writable".');
    stream[_pendingCloseRequest] = v8.createPromise();
    if (WritableStreamDefaultControllerGetBackpressure(
        stream[_writableStreamController])) {
      v8.resolvePromise(writer[_readyPromise], undefined);
    }
    stream[_state] = CLOSING;
    WritableStreamDefaultControllerClose(stream[_writableStreamController]);
    return stream[_pendingCloseRequest];
  }

  function WritableStreamDefaultWriterCloseWithErrorPropagation(writer) {
    const stream = writer[_ownerWritableStream];
    TEMP_ASSERT(stream !== undefined, 'stream is not undefined.');
    const state = stream[_state];
    if (state === CLOSING || state === CLOSED) {
      return Promise_resolve(undefined);
    }
    if (state === ERRORED) {
      return Promise_reject(stream[_storedError]);
    }
    TEMP_ASSERT(state === WRITABLE, 'state is "writable".');
    return WritableStreamDefaultWriterClose(writer);
  }

  function WritableStreamDefaultWriterGetDesiredSize(writer) {
    const stream = writer[_ownerWritableStream];
    const state = stream[_state];
    if (state === ERRORED) {
      return null;
    }
    if (state === CLOSED) {
      return 0;
    }
    return WritableStreamDefaultControllerGetDesiredSize(
        stream[_writableStreamController]);
  }

  function WritableStreamDefaultWriterRelease(writer) {
    const stream = writer[_ownerWritableStream];
    TEMP_ASSERT(stream !== undefined,
                'stream is not undefined.');
    TEMP_ASSERT(stream[_writer] === writer,
                'stream.[[writer]] is writer.');
    const releasedError = new TypeError(errReleasedWriterClosedPromise);
    const state = stream[_state];
    if (state === WRITABLE || state === CLOSING ||
        stream[_pendingAbortRequest] !== undefined) {
      v8.rejectPromise(writer[_closedPromise], releasedError);
    } else {
      writer[_closedPromise] = Promise_reject(releasedError);
    }
    v8.markPromiseAsHandled(writer[_closedPromise]);

    if (state === WRITABLE &&
        WritableStreamDefaultControllerGetBackpressure(
            stream[_writableStreamController])) {
      v8.rejectPromise(writer[_readyPromise], releasedError);
    } else {
      writer[_readyPromise] = Promise_reject(releasedError);
    }
    v8.markPromiseAsHandled(writer[_readyPromise]);

    stream[_writer] = undefined;
    writer[_ownerWritableStream] = undefined;
  }

  function WritableStreamDefaultWriterWrite(writer, chunk) {
    const stream = writer[_ownerWritableStream];
    TEMP_ASSERT(stream !== undefined,
                'stream is not undefined.');
    const state = stream[_state];
    if (state === CLOSED || state === ERRORED) {
      return Promise_reject(
          createCannotActionOnStateStreamError('write to', state));
    }
    TEMP_ASSERT(state === WRITABLE,
                'state is "writable".');
    const promise = WritableStreamAddWriteRequest(stream);
    WritableStreamDefaultControllerWrite(stream[_writableStreamController],
                                         chunk);
    return promise;
  }

  // Functions to expose internals for ReadableStream.pipeTo. These do not
  // appear in the standard.
  function getWritableStreamDefaultWriterClosedPromise(writer) {
    TEMP_ASSERT(
        IsWritableStreamDefaultWriter(writer),
        'writer is a WritableStreamDefaultWriter.');
    return writer[_closedPromise];
  }

  function getWritableStreamDefaultWriterReadyPromise(writer) {
    TEMP_ASSERT(
        IsWritableStreamDefaultWriter(writer),
        'writer is a WritableStreamDefaultWriter.');
    return writer[_readyPromise];
  }

  class WritableStreamDefaultController {
    constructor(stream, underlyingSink, size, highWaterMark) {
      if (!IsWritableStream(stream)) {
        throw new TypeError(streamErrors.illegalConstructor);
      }
      if (stream[_writableStreamController] !== undefined) {
        throw new TypeError(streamErrors.illegalConstructor);
      }
      this[_controlledWritableStream] = stream;
      this[_underlyingSink] = underlyingSink;
      this[_queue] = new v8.InternalPackedArray();
      this[_queueSize] = 0;
      this[_defaultControllerFlags] = 0;
      const normalizedStrategy =
          ValidateAndNormalizeQueuingStrategy(size, highWaterMark);
      this[_strategySize] = normalizedStrategy.size;
      this[_strategyHWM] = normalizedStrategy.highWaterMark;
      const backpressure = WritableStreamDefaultControllerGetBackpressure(this);
      if (backpressure) {
        WritableStreamUpdateBackpressure(stream, backpressure);
      }
      const controller = this;
      const startResult = InvokeOrNoop(underlyingSink, 'start', [this]);
      const onFulfilled = () => {
        setDefaultControllerStartedFlag(controller, true);
        WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
      };
      const onRejected = r => {
        WritableStreamDefaultControllerErrorIfNeeded(controller, r);
      };
      thenPromise(Promise_resolve(startResult), onFulfilled, onRejected);
    }

    error(e) {
      if (!IsWritableStreamDefaultController(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }
      const state = this[_controlledWritableStream][_state];
      if (state === CLOSED || state === ERRORED) {
        throw createCannotActionOnStateStreamError('error', state);
      }
      WritableStreamDefaultControllerError(this, e);
    }
  }

  // Writable Stream Default Controller Abstract Operations

  function IsWritableStreamDefaultController(x) {
    return hasOwnProperty(x, _underlyingSink);
  }

  function WritableStreamDefaultControllerAbort(controller, reason) {
    controller[_queue] = v8.InternalPackedArray();
    controller[_queueSize] = 0;
    const sinkAbortPromise =
        PromiseInvokeOrNoop(controller[_underlyingSink], 'abort', [reason]);
    return thenPromise(sinkAbortPromise, () => undefined);
  }

  function WritableStreamDefaultControllerClose(controller) {
    EnqueueValueWithSizeForController(controller, 'close', 0);
    WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
  }

  function WritableStreamDefaultControllerGetDesiredSize(controller) {
    const queueSize = GetTotalQueueSizeForController(controller);
    return controller[_strategyHWM] - queueSize;
  }

  function WritableStreamDefaultControllerWrite(controller, chunk) {
    const stream = controller[_controlledWritableStream];
    TEMP_ASSERT(stream[_state] === WRITABLE,
                'stream.[[state]] is "writable".');
    let chunkSize = 1;
    const strategySize = controller[_strategySize];
    if (strategySize !== undefined) {
      try {
        chunkSize = strategySize(chunk);
      } catch (e) {
        WritableStreamDefaultControllerErrorIfNeeded(controller, e);
        return;
      }
    }
    const writeRecord = {chunk};
    const lastBackpressure =
        WritableStreamDefaultControllerGetBackpressure(controller);
    try {
      EnqueueValueWithSizeForController(controller, writeRecord, chunkSize);
    } catch (e) {
      WritableStreamDefaultControllerErrorIfNeeded(controller, e);
      return;
    }
    if (stream[_state] === WRITABLE) {
      const backpressure =
          WritableStreamDefaultControllerGetBackpressure(controller);
      if (lastBackpressure !== backpressure) {
        WritableStreamUpdateBackpressure(stream, backpressure);
      }
    }
    WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
  }

  function WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller) {
    const state = controller[_controlledWritableStream][_state];
    if (state === CLOSED || state === ERRORED) {
      return;
    }
    if (!getDefaultControllerStartedFlag(controller)) {
      return;
    }
    if (getDefaultControllerWritingFlag(controller)) {
      return;
    }
    if (controller[_queue].length === 0) {
      return;
    }
    const writeRecord = PeekQueueValue(controller[_queue]);
    if (writeRecord === 'close') {
      WritableStreamDefaultControllerProcessClose(controller);
    } else {
      WritableStreamDefaultControllerProcessWrite(controller,
                                                  writeRecord.chunk);
    }
  }

  function WritableStreamDefaultControllerErrorIfNeeded(controller, e) {
    const state = controller[_controlledWritableStream][_state];
    if (state === WRITABLE || state === CLOSING) {
      WritableStreamDefaultControllerError(controller, e);
    }
  }

  function WritableStreamDefaultControllerProcessClose(controller) {
    const stream = controller[_controlledWritableStream];
    TEMP_ASSERT(stream[_state] === CLOSING,
                'stream.[[state]] is "closing".');
    DequeueValueForController(controller);
    TEMP_ASSERT(controller[_queue].length === 0,
                'controller.[[queue]] is empty.');
    setDefaultControllerInCloseFlag(controller, true);
    const sinkClosePromise = PromiseInvokeOrNoop(controller[_underlyingSink],
                                                 'close', [controller]);
    thenPromise(sinkClosePromise,
                () => {
                  TEMP_ASSERT(getDefaultControllerInCloseFlag(controller)
                      === true,
                      'controller.[[inClose]] is true');
                  setDefaultControllerInCloseFlag(controller, false);

                  if (stream[_state] !== CLOSING &&
                      stream[_state] !== ERRORED) {
                    return;
                  }

                  TEMP_ASSERT(stream[_pendingCloseRequest] !== undefined);
                  v8.resolvePromise(stream[_pendingCloseRequest], undefined);
                  stream[_pendingCloseRequest] = undefined;

                  WritableStreamFinishClose(stream);
                },
                r => {
                  TEMP_ASSERT(getDefaultControllerInCloseFlag(controller)
                      === true,
                      'controller.[[inClose]] is true');
                  setDefaultControllerInCloseFlag(controller, false);

                  TEMP_ASSERT(stream[_pendingCloseRequest] !== undefined);
                  v8.rejectPromise(stream[_pendingCloseRequest], r);
                  stream[_pendingCloseRequest] = undefined;

                  if (stream[_pendingAbortRequest] !== undefined) {
                    v8.rejectPromise(stream[_pendingAbortRequest], r);
                    stream[_pendingAbortRequest] = undefined;
                  }

                  WritableStreamDefaultControllerErrorIfNeeded(controller, r);
                }
               );
  }

  function WritableStreamDefaultControllerProcessWrite(controller, chunk) {
    setDefaultControllerWritingFlag(controller, true);

    const stream = controller[_controlledWritableStream];

    TEMP_ASSERT(stream[_pendingWriteRequest] === undefined,
                'stream.[[pendingWriteRequest]] is undefined');
    TEMP_ASSERT(stream[_writeRequests].length > 0,
                'stream.[[writeRequests]] is not empty');
    stream[_pendingWriteRequest] = stream[_writeRequests].shift();

    const sinkWritePromise = PromiseInvokeOrNoop(controller[_underlyingSink],
                                               'write', [chunk, controller]);
    thenPromise(
        sinkWritePromise,
        () => {
          TEMP_ASSERT(getDefaultControllerWritingFlag(controller) === true,
                      'controller.[[writing]] is true');
          setDefaultControllerWritingFlag(controller, false);

          TEMP_ASSERT(stream[_pendingWriteRequest] !== undefined,
                      'stream.[[pendingWriteRequest]] is not undefined');
          v8.resolvePromise(stream[_pendingWriteRequest], undefined);
          stream[_pendingWriteRequest] = undefined;

          const state = stream[_state];
          if (state === ERRORED) {
            WritableStreamRejectPromisesInReactionToError(stream);

            if (stream[_pendingAbortRequest] !== undefined) {
              v8.resolvePromise(stream[_pendingAbortRequest], undefined);
              stream[_pendingAbortRequest] = undefined;
            }
            return;
          }

          const lastBackpressure =
              WritableStreamDefaultControllerGetBackpressure(controller);
          DequeueValueForController(controller);

          if (state !== CLOSING) {
            const backpressure =
                WritableStreamDefaultControllerGetBackpressure(controller);
            if (lastBackpressure !== backpressure) {
              WritableStreamUpdateBackpressure(
                  controller[_controlledWritableStream], backpressure);
            }
          }

          WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
        },
        r => {
          TEMP_ASSERT(getDefaultControllerWritingFlag(controller) === true,
                      'controller.[[writing]] is true');
          setDefaultControllerWritingFlag(controller, false);

          TEMP_ASSERT(stream[_pendingWriteRequest] !== undefined,
                      'stream.[[pendingWriteRequest]] is not undefined');
          v8.rejectPromise(stream[_pendingWriteRequest], r);
          stream[_pendingWriteRequest] = undefined;

          if (stream[_state] === ERRORED) {
            stream[_storedError] = r;
            WritableStreamRejectPromisesInReactionToError(stream);
          }

          if (stream[_pendingAbortRequest] !== undefined) {
            v8.rejectPromise(stream[_pendingAbortRequest], r);
            stream[_pendingAbortRequest] = undefined;
          }

          WritableStreamDefaultControllerErrorIfNeeded(controller, r);
        }
    );
  }

  function WritableStreamDefaultControllerGetBackpressure(controller) {
    const desiredSize =
        WritableStreamDefaultControllerGetDesiredSize(controller);
    return desiredSize <= 0;
  }

  function WritableStreamDefaultControllerError(controller, e) {
    const stream = controller[_controlledWritableStream];
    const state = stream[_state];
    TEMP_ASSERT(state === WRITABLE || state === CLOSING,
                'stream.[[state]] is "writable" or "closing".');
    WritableStreamError(stream, e);
    controller[_queue] = new v8.InternalPackedArray();
    controller[_queueSize] = 0;
  }

  // Queue-with-Sizes Operations
  //
  // These differ from the versions in the standard: they take a controller
  // argument in order to cache the total queue size. This is necessary to avoid
  // O(N^2) behaviour.
  //
  // TODO(ricea): Share these operations with ReadableStream.js.
  function DequeueValueForController(controller) {
    TEMP_ASSERT(controller[_queue].length !== 0,
                'queue is not empty.');
    const result = controller[_queue].shift();
    controller[_queueSize] -= result.size;
    return result.value;
  }

  function EnqueueValueWithSizeForController(controller, value, size) {
    size = Number(size);
    if (!IsFiniteNonNegativeNumber(size)) {
      throw new RangeError(streamErrors.invalidSize);
    }

    controller[_queueSize] += size;
    controller[_queue].push({value, size});
  }

  function GetTotalQueueSizeForController(controller) {
    return controller[_queueSize];
  }

  function PeekQueueValue(queue) {
    TEMP_ASSERT(queue.length !== 0,
                'queue is not empty.');
    return queue[0].value;
  }

  // Miscellaneous Operations

  // This differs from "CallOrNoop" in the ReadableStream implementation in
  // that it takes the arguments as an array, so that multiple arguments can be
  // passed.
  //
  // TODO(ricea): Consolidate with ReadableStream implementation.
  function InvokeOrNoop(O, P, args) {
    TEMP_ASSERT(IsPropertyKey(P),
                'P is a valid property key.');
    if (args === undefined) {
      args = [];
    }
    const method = O[P];
    if (method === undefined) {
      return undefined;
    }
    if (typeof method !== 'function') {
      throw new TypeError(templateErrorIsNotAFunction(P));
    }
    return Function_apply(method, O, args);
  }

  function IsFiniteNonNegativeNumber(v) {
    return Number_isFinite(v) && v >= 0;
  }

  function PromiseInvokeOrNoop(O, P, args) {
    try {
      return Promise_resolve(InvokeOrNoop(O, P, args));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  // TODO(ricea): Share this operation with ReadableStream.js.
  function ValidateAndNormalizeQueuingStrategy(size, highWaterMark) {
    if (size !== undefined && typeof size !== 'function') {
      throw new TypeError(streamErrors.sizeNotAFunction);
    }

    highWaterMark = Number(highWaterMark);
    if (Number_isNaN(highWaterMark)) {
      throw new RangeError(streamErrors.errInvalidHWM);
    }
    if (highWaterMark < 0) {
      throw new RangeError(streamErrors.invalidHWM);
    }

    return {size, highWaterMark};
  }

  //
  // Additions to the global object
  //

  defineProperty(global, 'WritableStream', {
    value: WritableStream,
    enumerable: false,
    configurable: true,
    writable: true
  });

  // TODO(ricea): Exports to Blink

  // Exports for ReadableStream
  binding.AcquireWritableStreamDefaultWriter =
      AcquireWritableStreamDefaultWriter;
  binding.IsWritableStream = IsWritableStream;
  binding.isWritableStreamClosingOrClosed = isWritableStreamClosingOrClosed;
  binding.isWritableStreamErrored = isWritableStreamErrored;
  binding.IsWritableStreamLocked = IsWritableStreamLocked;
  binding.WritableStreamAbort = WritableStreamAbort;
  binding.WritableStreamDefaultWriterCloseWithErrorPropagation =
      WritableStreamDefaultWriterCloseWithErrorPropagation;
  binding.getWritableStreamDefaultWriterClosedPromise =
      getWritableStreamDefaultWriterClosedPromise;
  binding.WritableStreamDefaultWriterGetDesiredSize =
      WritableStreamDefaultWriterGetDesiredSize;
  binding.getWritableStreamDefaultWriterReadyPromise =
      getWritableStreamDefaultWriterReadyPromise;
  binding.WritableStreamDefaultWriterRelease =
      WritableStreamDefaultWriterRelease;
  binding.WritableStreamDefaultWriterWrite = WritableStreamDefaultWriterWrite;
  binding.getWritableStreamStoredError = getWritableStreamStoredError;
});

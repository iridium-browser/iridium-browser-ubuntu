// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @unrestricted
 */
Network.NetworkOverview = class extends PerfUI.TimelineOverviewBase {
  constructor() {
    super();
    this._selectedFilmStripTime = -1;
    this.element.classList.add('network-overview');

    /** @type {number} */
    this._numBands = 1;
    /** @type {boolean} */
    this._updateScheduled = false;

    SDK.targetManager.addModelListener(
        SDK.ResourceTreeModel, SDK.ResourceTreeModel.Events.Load, this._loadEventFired, this);
    SDK.targetManager.addModelListener(
        SDK.ResourceTreeModel, SDK.ResourceTreeModel.Events.DOMContentLoaded, this._domContentLoadedEventFired, this);

    this.reset();
  }

  /**
   * @param {?SDK.FilmStripModel} filmStripModel
   */
  setFilmStripModel(filmStripModel) {
    this._filmStripModel = filmStripModel;
    this.scheduleUpdate();
  }

  /**
   * @param {number} time
   */
  selectFilmStripFrame(time) {
    this._selectedFilmStripTime = time;
    this.scheduleUpdate();
  }

  clearFilmStripFrame() {
    this._selectedFilmStripTime = -1;
    this.scheduleUpdate();
  }

  /**
   * @param {!Common.Event} event
   */
  _loadEventFired(event) {
    var time = /** @type {number} */ (event.data.loadTime);
    if (time)
      this._loadEvents.push(time * 1000);
    this.scheduleUpdate();
  }

  /**
   * @param {!Common.Event} event
   */
  _domContentLoadedEventFired(event) {
    var data = /** @type {number} */ (event.data);
    if (data)
      this._domContentLoadedEvents.push(data * 1000);
    this.scheduleUpdate();
  }

  /**
   * @param {string} connectionId
   * @return {number}
   */
  _bandId(connectionId) {
    if (!connectionId || connectionId === '0')
      return -1;
    if (this._bandMap.has(connectionId))
      return /** @type {number} */ (this._bandMap.get(connectionId));
    var result = this._nextBand++;
    this._bandMap.set(connectionId, result);
    return result;
  }

  /**
   * @param {!SDK.NetworkRequest} request
   */
  updateRequest(request) {
    if (!this._requestsSet.has(request)) {
      this._requestsSet.add(request);
      this._requestsList.push(request);
    }
    this.scheduleUpdate();
  }

  /**
   * @override
   */
  wasShown() {
    this.onResize();
  }

  /**
   * @override
   */
  onResize() {
    var width = this.element.offsetWidth;
    var height = this.element.offsetHeight;
    this.calculator().setDisplayWidth(width);
    this.resetCanvas();
    var numBands = (((height - 1) / Network.NetworkOverview._bandHeight) - 1) | 0;
    this._numBands = (numBands > 0) ? numBands : 1;
    this.scheduleUpdate();
  }

  /**
   * @override
   */
  reset() {
    /** @type {?SDK.FilmStripModel} */
    this._filmStripModel = null;

    /** @type {number} */
    this._span = 1;
    /** @type {?Network.NetworkTimeBoundary} */
    this._lastBoundary = null;
    /** @type {number} */
    this._nextBand = 0;
    /** @type {!Map.<string, number>} */
    this._bandMap = new Map();
    /** @type {!Array.<!SDK.NetworkRequest>} */
    this._requestsList = [];
    /** @type {!Set.<!SDK.NetworkRequest>} */
    this._requestsSet = new Set();
    /** @type {!Array.<number>} */
    this._loadEvents = [];
    /** @type {!Array.<number>} */
    this._domContentLoadedEvents = [];

    // Clear screen.
    this.resetCanvas();
  }

  /**
   * @protected
   */
  scheduleUpdate() {
    if (this._updateScheduled || !this.isShowing())
      return;
    this._updateScheduled = true;
    this.element.window().requestAnimationFrame(this.update.bind(this));
  }

  /**
   * @override
   */
  update() {
    this._updateScheduled = false;

    var calculator = this.calculator();

    var newBoundary = new Network.NetworkTimeBoundary(calculator.minimumBoundary(), calculator.maximumBoundary());
    if (!this._lastBoundary || !newBoundary.equals(this._lastBoundary)) {
      var span = calculator.boundarySpan();
      while (this._span < span)
        this._span *= 1.25;

      calculator.setBounds(calculator.minimumBoundary(), calculator.minimumBoundary() + this._span);
      this._lastBoundary = new Network.NetworkTimeBoundary(calculator.minimumBoundary(), calculator.maximumBoundary());
    }

    var context = this.context();
    var linesByType = {};
    var paddingTop = 2;

    /**
     * @param {string} type
     * @param {string} strokeStyle
     */
    function drawLines(type, strokeStyle) {
      var lines = linesByType[type];
      if (!lines)
        return;
      var n = lines.length;
      context.beginPath();
      context.strokeStyle = strokeStyle;
      for (var i = 0; i < n;) {
        var y = lines[i++] * Network.NetworkOverview._bandHeight + paddingTop;
        var startTime = lines[i++];
        var endTime = lines[i++];
        if (endTime === Number.MAX_VALUE)
          endTime = calculator.maximumBoundary();
        context.moveTo(calculator.computePosition(startTime), y);
        context.lineTo(calculator.computePosition(endTime) + 1, y);
      }
      context.stroke();
    }

    /**
     * @param {string} type
     * @param {number} y
     * @param {number} start
     * @param {number} end
     */
    function addLine(type, y, start, end) {
      var lines = linesByType[type];
      if (!lines) {
        lines = [];
        linesByType[type] = lines;
      }
      lines.push(y, start, end);
    }

    var requests = this._requestsList;
    var n = requests.length;
    for (var i = 0; i < n; ++i) {
      var request = requests[i];
      var band = this._bandId(request.connectionId);
      var y = (band === -1) ? 0 : (band % this._numBands + 1);
      var timeRanges =
          Network.RequestTimingView.calculateRequestTimeRanges(request, this.calculator().minimumBoundary());
      for (var j = 0; j < timeRanges.length; ++j) {
        var type = timeRanges[j].name;
        if (band !== -1 || type === Network.RequestTimeRangeNames.Total)
          addLine(type, y, timeRanges[j].start * 1000, timeRanges[j].end * 1000);
      }
    }

    context.clearRect(0, 0, this.width(), this.height());
    context.save();
    context.scale(window.devicePixelRatio, window.devicePixelRatio);
    context.lineWidth = 2;
    drawLines(Network.RequestTimeRangeNames.Total, '#CCCCCC');
    drawLines(Network.RequestTimeRangeNames.Blocking, '#AAAAAA');
    drawLines(Network.RequestTimeRangeNames.Connecting, '#FF9800');
    drawLines(Network.RequestTimeRangeNames.ServiceWorker, '#FF9800');
    drawLines(Network.RequestTimeRangeNames.ServiceWorkerPreparation, '#FF9800');
    drawLines(Network.RequestTimeRangeNames.Push, '#8CDBff');
    drawLines(Network.RequestTimeRangeNames.Proxy, '#A1887F');
    drawLines(Network.RequestTimeRangeNames.DNS, '#009688');
    drawLines(Network.RequestTimeRangeNames.SSL, '#9C27B0');
    drawLines(Network.RequestTimeRangeNames.Sending, '#B0BEC5');
    drawLines(Network.RequestTimeRangeNames.Waiting, '#00C853');
    drawLines(Network.RequestTimeRangeNames.Receiving, '#03A9F4');

    var height = this.element.offsetHeight;
    context.lineWidth = 1;
    context.beginPath();
    context.strokeStyle = '#8080FF';  // Keep in sync with .network-blue-divider CSS rule.
    for (var i = this._domContentLoadedEvents.length - 1; i >= 0; --i) {
      var x = Math.round(calculator.computePosition(this._domContentLoadedEvents[i])) + 0.5;
      context.moveTo(x, 0);
      context.lineTo(x, height);
    }
    context.stroke();

    context.beginPath();
    context.strokeStyle = '#FF8080';  // Keep in sync with .network-red-divider CSS rule.
    for (var i = this._loadEvents.length - 1; i >= 0; --i) {
      var x = Math.round(calculator.computePosition(this._loadEvents[i])) + 0.5;
      context.moveTo(x, 0);
      context.lineTo(x, height);
    }
    context.stroke();

    if (this._selectedFilmStripTime !== -1) {
      context.lineWidth = 2;
      context.beginPath();
      context.strokeStyle = '#FCCC49';  // Keep in sync with .network-frame-divider CSS rule.
      var x = Math.round(calculator.computePosition(this._selectedFilmStripTime));
      context.moveTo(x, 0);
      context.lineTo(x, height);
      context.stroke();
    }
    context.restore();
  }
};

/** @type {number} */
Network.NetworkOverview._bandHeight = 3;

/** @typedef {{start: number, end: number}} */
Network.NetworkOverview.Window;

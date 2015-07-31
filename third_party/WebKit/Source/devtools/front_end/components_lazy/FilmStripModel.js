/*
 * Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @constructor
 * @param {!WebInspector.TracingModel} tracingModel
 */
WebInspector.FilmStripModel = function(tracingModel)
{
    this._tracingModel = tracingModel;

    /** @type {!Array<!WebInspector.FilmStripModel.Frame>} */
    this._frames = [];

    var browserProcess = tracingModel.processByName("Browser");
    if (!browserProcess)
        return;
    var mainThread = browserProcess.threadByName("CrBrowserMain");
    if (!mainThread)
        return;

    var events = mainThread.events();
    for (var i = 0; i < events.length; ++i) {
        if (events[i].category === "disabled-by-default-devtools.screenshot" && events[i].name === "CaptureFrame") {
            var data = events[i].args.data;
            if (!data)
                continue;
            this._frames.push(new WebInspector.FilmStripModel.Frame(this, data, events[i].startTime, this._frames.length));
        }
    }
}

WebInspector.FilmStripModel.prototype = {
    /**
     * @return {!Array<!WebInspector.FilmStripModel.Frame>}
     */
    frames: function()
    {
        return this._frames;
    },

    /**
     * @return {number}
     */
    zeroTime: function()
    {
        return this._tracingModel.minimumRecordTime();
    },

    /**
     * @param {number} timestamp
     * @return {?WebInspector.FilmStripModel.Frame}
     */
    firstFrameAfterCommit: function(timestamp)
    {
        var bestIndex = 0;
        var bestDelta = Number.MAX_VALUE;
        for (var i = 0; i < this._frames.length; ++i) {
            var delta = this._frames[i].timestamp - timestamp;
            if (delta < 0)
                continue;
            if (delta < bestDelta) {
                bestIndex = i;
                bestDelta = delta;
            }
        }
        return bestDelta < 10 ? this._frames[bestIndex] : null;
    }
}

/**
 * @constructor
 * @param {!WebInspector.FilmStripModel} model
 * @param {string} imageData
 * @param {number} timestamp
 * @param {number} index
 */
WebInspector.FilmStripModel.Frame = function(model, imageData, timestamp, index)
{
    this._model = model;
    this.imageData = imageData;
    this.timestamp = timestamp;
    this.index = index;
}

WebInspector.FilmStripModel.Frame.prototype = {
    /**
     * @return {!WebInspector.FilmStripModel}
     */
    model: function()
    {
        return this._model;
    }
}

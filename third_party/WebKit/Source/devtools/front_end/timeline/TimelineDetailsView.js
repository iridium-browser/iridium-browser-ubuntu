// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @unrestricted
 */
Timeline.TimelineDetailsView = class extends UI.TabbedPane {
  /**
   * @param {!TimelineModel.TimelineModel} timelineModel
   * @param {!TimelineModel.TimelineFrameModel} frameModel
   * @param {!SDK.FilmStripModel} filmStripModel
   * @param {!Array<!TimelineModel.TimelineModel.Filter>} filters
   * @param {!Timeline.TimelineModeViewDelegate} delegate
   */
  constructor(timelineModel, frameModel, filmStripModel, filters, delegate) {
    super();
    this.element.classList.add('timeline-details');

    this._model = timelineModel;
    this._frameModel = frameModel;
    this._filmStripModel = filmStripModel;
    this._detailsLinkifier = new Components.Linkifier();

    const tabIds = Timeline.TimelineDetailsView.Tab;
    this._defaultDetailsWidget = new UI.VBox();
    this._defaultDetailsWidget.element.classList.add('timeline-details-view');
    this._defaultDetailsContentElement =
        this._defaultDetailsWidget.element.createChild('div', 'timeline-details-view-body vbox');
    this._defaultDetailsContentElement.tabIndex = 0;
    this.appendTab(tabIds.Details, Common.UIString('Summary'), this._defaultDetailsWidget);
    this.setPreferredTab(tabIds.Details);

    /** @type Map<string, Timeline.TimelineTreeView> */
    this._rangeDetailViews = new Map();

    const bottomUpView = new Timeline.BottomUpTimelineTreeView(timelineModel, filters);
    this.appendTab(tabIds.BottomUp, Common.UIString('Bottom-Up'), bottomUpView);
    this._rangeDetailViews.set(tabIds.BottomUp, bottomUpView);

    const callTreeView = new Timeline.CallTreeTimelineTreeView(timelineModel, filters);
    this.appendTab(tabIds.CallTree, Common.UIString('Call Tree'), callTreeView);
    this._rangeDetailViews.set(tabIds.CallTree, callTreeView);

    const eventsView = new Timeline.EventsTimelineTreeView(timelineModel, filters, delegate);
    this.appendTab(tabIds.Events, Common.UIString('Event Log'), eventsView);
    this._rangeDetailViews.set(tabIds.Events, eventsView);

    this.addEventListener(UI.TabbedPane.Events.TabSelected, this._tabSelected, this);
  }

  /**
   * @param {!Node} node
   */
  _setContent(node) {
    const allTabs = this.otherTabs(Timeline.TimelineDetailsView.Tab.Details);
    for (var i = 0; i < allTabs.length; ++i) {
      if (!this._rangeDetailViews.has(allTabs[i]))
        this.closeTab(allTabs[i]);
    }
    this._defaultDetailsContentElement.removeChildren();
    this._defaultDetailsContentElement.appendChild(node);
  }

  _updateContents() {
    var view = this.selectedTabId ? this._rangeDetailViews.get(this.selectedTabId) : null;
    if (view)
      view.updateContents(this._selection);
  }

  /**
   * @override
   * @param {string} id
   * @param {string} tabTitle
   * @param {!UI.Widget} view
   * @param {string=} tabTooltip
   * @param {boolean=} userGesture
   * @param {boolean=} isCloseable
   */
  appendTab(id, tabTitle, view, tabTooltip, userGesture, isCloseable) {
    super.appendTab(id, tabTitle, view, tabTooltip, userGesture, isCloseable);
    if (this._preferredTabId !== this.selectedTabId)
      this.selectTab(id);
  }

  /**
   * @param {string} tabId
   */
  setPreferredTab(tabId) {
    this._preferredTabId = tabId;
  }

  /**
   * @param {!Timeline.TimelineSelection} selection
   */
  setSelection(selection) {
    this._detailsLinkifier.reset();
    this._selection = selection;
    switch (this._selection.type()) {
      case Timeline.TimelineSelection.Type.TraceEvent:
        var event = /** @type {!SDK.TracingModel.Event} */ (this._selection.object());
        Timeline.TimelineUIUtils.buildTraceEventDetails(
            event, this._model, this._detailsLinkifier, true,
            this._appendDetailsTabsForTraceEventAndShowDetails.bind(this, event));
        break;
      case Timeline.TimelineSelection.Type.Frame:
        var frame = /** @type {!TimelineModel.TimelineFrame} */ (this._selection.object());
        var screenshotTime = frame.idle ?
            frame.startTime :
            frame.endTime;  // For idle frames, look at the state at the beginning of the frame.
        var filmStripFrame = filmStripFrame = this._filmStripModel.frameByTimestamp(screenshotTime);
        if (filmStripFrame && filmStripFrame.timestamp - frame.endTime > 10)
          filmStripFrame = null;
        this._setContent(
            Timeline.TimelineUIUtils.generateDetailsContentForFrame(this._frameModel, frame, filmStripFrame));
        if (frame.layerTree) {
          var layersView = this._layersView();
          layersView.showLayerTree(frame.layerTree);
          if (!this.hasTab(Timeline.TimelineDetailsView.Tab.LayerViewer))
            this.appendTab(Timeline.TimelineDetailsView.Tab.LayerViewer, Common.UIString('Layers'), layersView);
        }
        break;
      case Timeline.TimelineSelection.Type.NetworkRequest:
        var request = /** @type {!TimelineModel.TimelineModel.NetworkRequest} */ (this._selection.object());
        Timeline.TimelineUIUtils.buildNetworkRequestDetails(request, this._model, this._detailsLinkifier)
            .then(this._setContent.bind(this));
        break;
      case Timeline.TimelineSelection.Type.Range:
        this._updateSelectedRangeStats(this._selection.startTime(), this._selection.endTime());
        break;
    }

    this._updateContents();
  }

  /**
   * @param {!Common.Event} event
   */
  _tabSelected(event) {
    if (!event.data.isUserGesture)
      return;
    this.setPreferredTab(event.data.tabId);
    this._updateContents();
  }

  /**
   * @return {!UI.Widget}
   */
  _layersView() {
    if (this._lazyLayersView)
      return this._lazyLayersView;
    this._lazyLayersView = new Timeline.TimelineLayersView(this._model, this._showSnapshotInPaintProfiler.bind(this));
    return this._lazyLayersView;
  }

  /**
   * @return {!Timeline.TimelinePaintProfilerView}
   */
  _paintProfilerView() {
    if (this._lazyPaintProfilerView)
      return this._lazyPaintProfilerView;
    this._lazyPaintProfilerView = new Timeline.TimelinePaintProfilerView(this._frameModel);
    return this._lazyPaintProfilerView;
  }

  /**
   * @param {!SDK.PaintProfilerSnapshot} snapshot
   */
  _showSnapshotInPaintProfiler(snapshot) {
    var paintProfilerView = this._paintProfilerView();
    paintProfilerView.setSnapshot(snapshot);
    if (!this.hasTab(Timeline.TimelineDetailsView.Tab.PaintProfiler)) {
      this.appendTab(
          Timeline.TimelineDetailsView.Tab.PaintProfiler, Common.UIString('Paint Profiler'), paintProfilerView,
          undefined, undefined, true);
    }
    this.selectTab(Timeline.TimelineDetailsView.Tab.PaintProfiler, true);
  }

  /**
   * @param {!SDK.TracingModel.Event} event
   * @param {!Node} content
   */
  _appendDetailsTabsForTraceEventAndShowDetails(event, content) {
    this._setContent(content);
    if (event.name === TimelineModel.TimelineModel.RecordType.Paint ||
        event.name === TimelineModel.TimelineModel.RecordType.RasterTask)
      this._showEventInPaintProfiler(event);
  }

  /**
   * @param {!SDK.TracingModel.Event} event
   */
  _showEventInPaintProfiler(event) {
    const target = SDK.targetManager.mainTarget();
    if (!target)
      return;
    const paintProfilerView = this._paintProfilerView();
    const hasProfileData = paintProfilerView.setEvent(target, event);
    if (!hasProfileData)
      return;
    if (this.hasTab(Timeline.TimelineDetailsView.Tab.PaintProfiler))
      return;
    this.appendTab(
        Timeline.TimelineDetailsView.Tab.PaintProfiler, Common.UIString('Paint Profiler'), paintProfilerView,
        undefined, undefined, false);
  }

  /**
   * @param {number} startTime
   * @param {number} endTime
   */
  _updateSelectedRangeStats(startTime, endTime) {
    this._setContent(Timeline.TimelineUIUtils.buildRangeStats(this._model, startTime, endTime));
  }
};

/**
 * @enum {string}
 */
Timeline.TimelineDetailsView.Tab = {
  Details: 'Details',
  Events: 'Events',
  CallTree: 'CallTree',
  BottomUp: 'BottomUp',
  PaintProfiler: 'PaintProfiler',
  LayerViewer: 'LayerViewer'
};

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The base class for simple filters that only modify the image content
 * but do not modify the image dimensions.
 * @param {string} name
 * @param {string} title
 * @constructor
 * @struct
 * @extends {ImageEditor.Mode}
 */
ImageEditor.Mode.Adjust = function(name, title) {
  ImageEditor.Mode.call(this, name, title);

  /**
   * @type {boolean}
   * @const
   */
  this.implicitCommit = true;

  /**
   * @type {?string}
   * @private
   */
  this.doneMessage_ = null;

  /**
   * @type {number}
   * @private
   */
  this.viewportGeneration_ = 0;

  /**
   * @type {?function(!ImageData,!ImageData,number,number)}
   * @private
   */
  this.filter_ = null;

  /**
   * @type {HTMLCanvasElement}
   * @private
   */
  this.canvas_ = null;

  /**
   * @private {ImageData}
   */
  this.previewImageData_ = null;

  /**
   * @private {ImageData}
   */
  this.originalImageData_ = null;
};

ImageEditor.Mode.Adjust.prototype = {__proto__: ImageEditor.Mode.prototype};

/**
 * Gets command to do filter.
 *
 * @return {Command.Filter} Filter command.
 */
ImageEditor.Mode.Adjust.prototype.getCommand = function() {
  if (!this.filter_) return null;

  return new Command.Filter(this.name, this.filter_, this.doneMessage_);
};

/** @override */
ImageEditor.Mode.Adjust.prototype.cleanUpUI = function() {
  ImageEditor.Mode.prototype.cleanUpUI.apply(this, arguments);
  this.hidePreview();
};

/**
 * Hides preview.
 */
ImageEditor.Mode.Adjust.prototype.hidePreview = function() {
  if (this.canvas_) {
    this.canvas_.parentNode.removeChild(this.canvas_);
    this.canvas_ = null;
  }
};

/** @override */
ImageEditor.Mode.Adjust.prototype.cleanUpCaches = function() {
  this.filter_ = null;
  this.previewImageData_ = null;
};

/** @override */
ImageEditor.Mode.Adjust.prototype.reset = function() {
  ImageEditor.Mode.prototype.reset.call(this);
  this.hidePreview();
  this.cleanUpCaches();
};

/** @override */
ImageEditor.Mode.Adjust.prototype.update = function(options) {
  ImageEditor.Mode.prototype.update.apply(this, arguments);
  this.updatePreviewImage_(options);
};

/**
 * Copy the source image data for the preview.
 * Use the cached copy if the viewport has not changed.
 * @param {Object} options Options that describe the filter. It it is null, it
 *     does not update current filter.
 * @private
 */
ImageEditor.Mode.Adjust.prototype.updatePreviewImage_ = function(options) {
  assert(this.getViewport());

  var isPreviewImageInvalidated = false;

  // Update filter.
  if (options) {
    // We assume filter names are used in the UI directly.
    // This will have to change with i18n.
    this.filter_ = this.createFilter(options);
    isPreviewImageInvalidated = true;
  }

  // Update canvas size and/or transformation.
  if (!this.previewImageData_ ||
      this.viewportGeneration_ !== this.getViewport().getCacheGeneration()) {
    this.viewportGeneration_ = this.getViewport().getCacheGeneration();

    if (!this.canvas_)
      this.canvas_ = this.getImageView().createOverlayCanvas();

    this.getImageView().setupDeviceBuffer(this.canvas_);
    var canvas = this.getImageView().getImageCanvasWith(
        this.canvas_.width, this.canvas_.height);
    var context = canvas.getContext('2d');
    this.originalImageData_ = context.getImageData(0, 0,
        this.canvas_.width, this.canvas_.height);
    this.previewImageData_ = context.getImageData(0, 0,
        this.canvas_.width, this.canvas_.height);

    isPreviewImageInvalidated = true;
  } else {
    this.getImageView().setTransform_(
        assert(this.canvas_), assert(this.getViewport()));
  }

  // Update preview image with applying filter.
  if (isPreviewImageInvalidated) {
    assert(this.originalImageData_);
    assert(this.previewImageData_);

    ImageUtil.trace.resetTimer('preview');
    this.filter_(this.previewImageData_, this.originalImageData_, 0, 0);
    ImageUtil.trace.reportTimer('preview');

    this.canvas_.getContext('2d').putImageData(this.previewImageData_, 0, 0);
  }
};

/** @override */
ImageEditor.Mode.Adjust.prototype.draw = function() {
  this.updatePreviewImage_(null);
};

/*
 * Own methods
 */

/**
 * Creates a filter.
 * @param {!Object} options A map of filter-specific options.
 * @return {function(!ImageData,!ImageData,number,number)} Created function.
 */
ImageEditor.Mode.Adjust.prototype.createFilter = function(options) {
  return filter.create(this.name, options);
};

/**
 * A base class for color filters that are scale independent.
 * @constructor
 * @param {string} name The mode name.
 * @param {string} title The mode title.
 * @extends {ImageEditor.Mode.Adjust}
 * @struct
 */
ImageEditor.Mode.ColorFilter = function(name, title) {
  ImageEditor.Mode.Adjust.call(this, name, title);
};

ImageEditor.Mode.ColorFilter.prototype =
    {__proto__: ImageEditor.Mode.Adjust.prototype};

/**
 * Gets a histogram from a thumbnail.
 * @return {{r: !Array<number>, g: !Array<number>, b: !Array<number>}}
 *    histogram.
 */
ImageEditor.Mode.ColorFilter.prototype.getHistogram = function() {
  return filter.getHistogram(this.getImageView().getThumbnail());
};

/**
 * Exposure/contrast filter.
 * @constructor
 * @extends {ImageEditor.Mode.ColorFilter}
 * @struct
 */
ImageEditor.Mode.Exposure = function() {
  ImageEditor.Mode.ColorFilter.call(this, 'exposure', 'GALLERY_EXPOSURE');
};

ImageEditor.Mode.Exposure.prototype =
    {__proto__: ImageEditor.Mode.ColorFilter.prototype};

/** @override */
ImageEditor.Mode.Exposure.prototype.createTools = function(toolbar) {
  toolbar.addRange('brightness', 'GALLERY_BRIGHTNESS', -1, 0, 1, 100);
  toolbar.addRange('contrast', 'GALLERY_CONTRAST', -1, 0, 1, 100);
};

/**
 * Autofix.
 * @constructor
 * @struct
 * @extends {ImageEditor.Mode.ColorFilter}
 */
ImageEditor.Mode.Autofix = function() {
  ImageEditor.Mode.ColorFilter.call(this, 'autofix', 'GALLERY_AUTOFIX');
  this.doneMessage_ = 'GALLERY_FIXED';
};

ImageEditor.Mode.Autofix.prototype =
    {__proto__: ImageEditor.Mode.ColorFilter.prototype};

/** @override */
ImageEditor.Mode.Autofix.prototype.isApplicable = function() {
  return this.getImageView().hasValidImage() &&
      filter.autofix.isApplicable(this.getHistogram());
};

/**
 * Applies autofix.
 */
ImageEditor.Mode.Autofix.prototype.apply = function() {
  this.update({histogram: this.getHistogram()});
};

/**
 * Instant Autofix.
 * @constructor
 * @extends {ImageEditor.Mode.Autofix}
 * @struct
 */
ImageEditor.Mode.InstantAutofix = function() {
  ImageEditor.Mode.Autofix.call(this);
  this.instant = true;
};

ImageEditor.Mode.InstantAutofix.prototype =
    {__proto__: ImageEditor.Mode.Autofix.prototype};

/** @override */
ImageEditor.Mode.InstantAutofix.prototype.setUp = function() {
  ImageEditor.Mode.Autofix.prototype.setUp.apply(this, arguments);
  this.apply();
};

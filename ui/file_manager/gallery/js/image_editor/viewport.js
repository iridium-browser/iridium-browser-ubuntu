// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Viewport class controls the way the image is displayed (scale, offset etc).
 * @constructor
 * @struct
 */
function Viewport() {
  /**
   * Size of the full resolution image.
   * @type {!ImageRect}
   * @private
   */
  this.imageBounds_ = new ImageRect(0, 0, 0, 0);

  /**
   * Size of the application window.
   * @type {!ImageRect}
   * @private
   */
  this.screenBounds_ = new ImageRect(0, 0, 0, 0);

  /**
   * Bounds of the image element on screen without zoom and offset.
   * @type {ImageRect}
   * @private
   */
  this.imageElementBoundsOnScreen_ = null;

  /**
   * Bounds of the image with zoom and offset.
   * @type {ImageRect}
   * @private
   */
  this.imageBoundsOnScreen_ = null;

  /**
   * Image bounds that is clipped with the screen bounds.
   * @type {ImageRect}
   * @private
   */
  this.imageBoundsOnScreenClipped_ = null;

  /**
   * Scale from the full resolution image to the screen displayed image. This is
   * not zoom operated by users.
   * @type {number}
   * @private
   */
  this.scale_ = 1;

  /**
   * Zoom ratio specified by user operations.
   * @type {number}
   * @private
   */
  this.zoom_ = 1;

  /**
   * Offset specified by user operations.
   * @type {number}
   * @private
   */
  this.offsetX_ = 0;

  /**
   * Offset specified by user operations.
   * @type {number}
   * @private
   */
  this.offsetY_ = 0;

  /**
   * Integer Rotation value.
   * The rotation angle is this.rotation_ * 90.
   * @type {number}
   * @private
   */
  this.rotation_ = 0;

  /**
   * Generation of the screen size image cache.
   * This is incremented every time when the size of image cache is changed.
   * @type {number}
   * @private
   */
  this.generation_ = 0;

  this.update_();
}

/**
 * Zoom ratios.
 *
 * @type {Array.<number>}
 * @const
 */
Viewport.ZOOM_RATIOS = [1, 1.5, 2, 3];

/**
 * Sets image size.
 * @param {number} width Image width.
 * @param {number} height Image height.
 */
Viewport.prototype.setImageSize = function(width, height) {
  this.imageBounds_ = ImageRect.createFromWidthAndHeight(width, height);
  this.update_();
};

/**
 * Sets screen size.
 * @param {number} width Screen width.
 * @param {number} height Screen height.
 */
Viewport.prototype.setScreenSize = function(width, height) {
  this.screenBounds_ = ImageRect.createFromWidthAndHeight(width, height);
  this.update_();
};

/**
 * Sets zoom value directly.
 * @param {number} zoom New zoom value.
 */
Viewport.prototype.setZoom = function(zoom) {
  var zoomMin = Viewport.ZOOM_RATIOS[0];
  var zoomMax = Viewport.ZOOM_RATIOS[Viewport.ZOOM_RATIOS.length - 1];
  var adjustedZoom = Math.max(zoomMin, Math.min(zoom, zoomMax));
  this.zoom_ = adjustedZoom;
  this.update_();
};

/**
 * Returns the value of zoom.
 * @return {number} Zoom value.
 */
Viewport.prototype.getZoom = function() {
  return this.zoom_;
};

/**
 * Sets the nearest larger value of ZOOM_RATIOS.
 */
Viewport.prototype.zoomIn = function() {
  var zoom = Viewport.ZOOM_RATIOS[0];
  for (var i = 0; i < Viewport.ZOOM_RATIOS.length; i++) {
    zoom = Viewport.ZOOM_RATIOS[i];
    if (zoom > this.zoom_)
      break;
  }
  this.setZoom(zoom);
};

/**
 * Sets the nearest smaller value of ZOOM_RATIOS.
 */
Viewport.prototype.zoomOut = function() {
  var zoom = Viewport.ZOOM_RATIOS[Viewport.ZOOM_RATIOS.length - 1];
  for (var i = Viewport.ZOOM_RATIOS.length - 1; i >= 0; i--) {
    zoom = Viewport.ZOOM_RATIOS[i];
    if (zoom < this.zoom_)
      break;
  }
  this.setZoom(zoom);
};

/**
 * Obtains whether the picture is zoomed or not.
 * @return {boolean}
 */
Viewport.prototype.isZoomed = function() {
  return this.zoom_ !== 1;
};

/**
 * Sets the rotation value.
 * @param {number} rotation New rotation value.
 */
Viewport.prototype.setRotation = function(rotation) {
  this.rotation_ = rotation;
  this.update_();
};


/**
 * Obtains the rotation value.
 * @return {number} Current rotation value.
 */
Viewport.prototype.getRotation = function() {
  return this.rotation_;
};

/**
 * Obtains the scale for the specified image size.
 *
 * @param {number} width Width of the full resolution image.
 * @param {number} height Height of the full resolution image.
 * @return {number} The ratio of the full resotion image size and the calculated
 * displayed image size.
 * @private
 */
Viewport.prototype.getFittingScaleForImageSize_ = function(width, height) {
  var scaleX = this.screenBounds_.width / width;
  var scaleY = this.screenBounds_.height / height;
  return Math.min(scaleX, scaleY, 1);
};

/**
 * Returns offset X.
 * @return {number} X-offset of the viewport.
 */
Viewport.prototype.getOffsetX = function() { return this.offsetX_; };

/**
 * Returns offset Y.
 * @return {number} Y-offset of the viewport.
 */
Viewport.prototype.getOffsetY = function() { return this.offsetY_; };

/**
 * Set the image offset in the viewport.
 * @param {number} x X-offset.
 * @param {number} y Y-offset.
 */
Viewport.prototype.setOffset = function(x, y) {
  if (this.offsetX_ == x && this.offsetY_ == y)
    return;
  this.offsetX_ = x;
  this.offsetY_ = y;
  this.update_();
};

/**
 * Returns image bounds.
 * @return {!ImageRect} The image bounds in image coordinates.
 */
Viewport.prototype.getImageBounds = function() { return this.imageBounds_; };

/**
 * Returns screen bounds.
 * @return {!ImageRect} The screen bounds in screen coordinates.
 */
Viewport.prototype.getScreenBounds = function() { return this.screenBounds_; };

/**
 * Returns device bounds.
 * @return {!ImageRect} The size of screen cache canvas.
 */
Viewport.prototype.getDeviceBounds = function() {
  var size = this.getImageElementBoundsOnScreen();
  return ImageRect.createFromWidthAndHeight(
      size.width * window.devicePixelRatio,
      size.height * window.devicePixelRatio);
};

/**
 * A counter that is incremented with each viewport state change.
 * Clients that cache anything that depends on the viewport state should keep
 * track of this counter.
 * @return {number} counter.
 */
Viewport.prototype.getCacheGeneration = function() { return this.generation_; };

/**
 * Returns image bounds in screen coordinates.
 * @return {!ImageRect} The image bounds in screen coordinates.
 */
Viewport.prototype.getImageBoundsOnScreen = function() {
  assert(this.imageBoundsOnScreen_);
  return this.imageBoundsOnScreen_;
};

/**
 * The image bounds in screen coordinates.
 * This returns the bounds of element before applying zoom and offset.
 * @return {!ImageRect}
 */
Viewport.prototype.getImageElementBoundsOnScreen = function() {
  assert(this.imageElementBoundsOnScreen_);
  return this.imageElementBoundsOnScreen_;
};

/**
 * The image bounds on screen, which is clipped with the screen size.
 * @return {!ImageRect}
 */
Viewport.prototype.getImageBoundsOnScreenClipped = function() {
  assert(this.imageBoundsOnScreenClipped_);
  return this.imageBoundsOnScreenClipped_;
};

/**
 * Returns size in image coordinates.
 * @param {number} size Size in screen coordinates.
 * @return {number} Size in image coordinates.
 */
Viewport.prototype.screenToImageSize = function(size) {
  return size / this.scale_;
};

/**
 * Returns X in image coordinates.
 * @param {number} x X in screen coordinates.
 * @return {number} X in image coordinates.
 */
Viewport.prototype.screenToImageX = function(x) {
  return Math.round((x - this.imageBoundsOnScreen_.left) / this.scale_);
};

/**
 * Returns Y in image coordinates.
 * @param {number} y Y in screen coordinates.
 * @return {number} Y in image coordinates.
 */
Viewport.prototype.screenToImageY = function(y) {
  return Math.round((y - this.imageBoundsOnScreen_.top) / this.scale_);
};

/**
 * Returns a rectangle in image coordinates.
 * @param {!ImageRect} rect Rectangle in screen coordinates.
 * @return {!ImageRect} Rectangle in image coordinates.
 */
Viewport.prototype.screenToImageRect = function(rect) {
  return new ImageRect(
      this.screenToImageX(rect.left),
      this.screenToImageY(rect.top),
      this.screenToImageSize(rect.width),
      this.screenToImageSize(rect.height));
};

/**
 * Returns size in screen coordinates.
 * @param {number} size Size in image coordinates.
 * @return {number} Size in screen coordinates.
 */
Viewport.prototype.imageToScreenSize = function(size) {
  return size * this.scale_;
};

/**
 * Returns X in screen coordinates.
 * @param {number} x X in image coordinates.
 * @return {number} X in screen coordinates.
 */
Viewport.prototype.imageToScreenX = function(x) {
  return Math.round(this.imageBoundsOnScreen_.left + x * this.scale_);
};

/**
 * Returns Y in screen coordinates.
 * @param {number} y Y in image coordinates.
 * @return {number} Y in screen coordinates.
 */
Viewport.prototype.imageToScreenY = function(y) {
  return Math.round(this.imageBoundsOnScreen_.top + y * this.scale_);
};

/**
 * Returns a rectangle in screen coordinates.
 * @param {!ImageRect} rect Rectangle in image coordinates.
 * @return {!ImageRect} Rectangle in screen coordinates.
 */
Viewport.prototype.imageToScreenRect = function(rect) {
  return new ImageRect(
      this.imageToScreenX(rect.left),
      this.imageToScreenY(rect.top),
      Math.round(this.imageToScreenSize(rect.width)),
      Math.round(this.imageToScreenSize(rect.height)));
};

/**
 * Returns a rectangle with given geometry.
 * @param {number} width Width of the rectangle.
 * @param {number} height Height of the rectangle.
 * @param {number} offsetX X-offset of center position of the rectangle.
 * @param {number} offsetY Y-offset of center position of the rectangle.
 * @return {!ImageRect} Rectangle with given geometry.
 * @private
 */
Viewport.prototype.getCenteredRect_ = function(
    width, height, offsetX, offsetY) {
  return new ImageRect(
      ~~((this.screenBounds_.width - width) / 2) + offsetX,
      ~~((this.screenBounds_.height - height) / 2) + offsetY,
      width,
      height);
};

/**
 * Resets zoom and offset.
 */
Viewport.prototype.resetView = function() {
  this.zoom_ = 1;
  this.offsetX_ = 0;
  this.offsetY_ = 0;
  this.rotation_ = 0;
  this.update_();
};

/**
 * Recalculate the viewport parameters.
 * @private
 */
Viewport.prototype.update_ = function() {
  // Update scale.
  this.scale_ = this.getFittingScaleForImageSize_(
      this.imageBounds_.width, this.imageBounds_.height);

  // Limit offset values.
  var zoomedWidht;
  var zoomedHeight;
  if (this.rotation_ % 2 == 0) {
    zoomedWidht = ~~(this.imageBounds_.width * this.scale_ * this.zoom_);
    zoomedHeight = ~~(this.imageBounds_.height * this.scale_ * this.zoom_);
  } else {
    var scale = this.getFittingScaleForImageSize_(
        this.imageBounds_.height, this.imageBounds_.width);
    zoomedWidht = ~~(this.imageBounds_.height * scale * this.zoom_);
    zoomedHeight = ~~(this.imageBounds_.width * scale * this.zoom_);
  }
  var dx = Math.max(zoomedWidht - this.screenBounds_.width, 0) / 2;
  var dy = Math.max(zoomedHeight - this.screenBounds_.height, 0) / 2;
  this.offsetX_ = ImageUtil.clamp(-dx, this.offsetX_, dx);
  this.offsetY_ = ImageUtil.clamp(-dy, this.offsetY_, dy);

  // Image bounds on screen.
  this.imageBoundsOnScreen_ = this.getCenteredRect_(
      zoomedWidht, zoomedHeight, this.offsetX_, this.offsetY_);

  // Image bounds of element (that is not applied zoom and offset) on screen.
  var oldBounds = this.imageElementBoundsOnScreen_;
  this.imageElementBoundsOnScreen_ = this.getCenteredRect_(
      ~~(this.imageBounds_.width * this.scale_),
      ~~(this.imageBounds_.height * this.scale_),
      0,
      0);
  if (!oldBounds ||
      this.imageElementBoundsOnScreen_.width != oldBounds.width ||
      this.imageElementBoundsOnScreen_.height != oldBounds.height) {
    this.generation_++;
  }

  // Image bounds on screen clipped with the screen bounds.
  var left = Math.max(this.imageBoundsOnScreen_.left, 0);
  var top = Math.max(this.imageBoundsOnScreen_.top, 0);
  var right = Math.min(
      this.imageBoundsOnScreen_.right, this.screenBounds_.width);
  var bottom = Math.min(
      this.imageBoundsOnScreen_.bottom, this.screenBounds_.height);
  this.imageBoundsOnScreenClipped_ = new ImageRect(
      left, top, right - left, bottom - top);
};

/**
 * Clones the viewport.
 * @return {!Viewport} New instance.
 */
Viewport.prototype.clone = function() {
  var viewport = new Viewport();
  viewport.imageBounds_ = ImageRect.createFromBounds(this.imageBounds_);
  viewport.screenBounds_ = ImageRect.createFromBounds(this.screenBounds_);
  viewport.scale_ = this.scale_;
  viewport.zoom_ = this.zoom_;
  viewport.offsetX_ = this.offsetX_;
  viewport.offsetY_ = this.offsetY_;
  viewport.rotation_ = this.rotation_;
  viewport.generation_ = this.generation_;
  viewport.update_();
  return viewport;
};

/**
 * Obtains CSS transformation for the screen image.
 * @return {string} Transformation description.
 */
Viewport.prototype.getTransformation = function() {
  var rotationScaleAdjustment;
  if (this.rotation_ % 2) {
    rotationScaleAdjustment = this.getFittingScaleForImageSize_(
        this.imageBounds_.height, this.imageBounds_.width) / this.scale_;
  } else {
    rotationScaleAdjustment = 1;
  }
  return [
    'translate(' + this.offsetX_ + 'px, ' + this.offsetY_ + 'px) ',
    'rotate(' + (this.rotation_ * 90) + 'deg)',
    'scale(' + (this.zoom_ * rotationScaleAdjustment) + ')'
  ].join(' ');
};

/**
 * Obtains shift CSS transformation for the screen image.
 * @param {number} dx Amount of shift.
 * @return {string} Transformation description.
 */
Viewport.prototype.getShiftTransformation = function(dx) {
  return 'translateX(' + dx + 'px) ' + this.getTransformation();
};

/**
 * Obtains CSS transformation that makes the rotated image fit the original
 * image. The new rotated image that the transformation is applied to looks the
 * same with original image.
 *
 * @param {boolean} orientation Orientation of the rotation from the original
 *     image to the rotated image. True is for clockwise and false is for
 *     counterclockwise.
 * @return {string} Transformation description.
 */
Viewport.prototype.getInverseTransformForRotatedImage = function(orientation) {
  var previousImageWidth = this.imageBounds_.height;
  var previousImageHeight = this.imageBounds_.width;
  var oldScale = this.getFittingScaleForImageSize_(
      previousImageWidth, previousImageHeight);
  var scaleRatio = oldScale / this.scale_;
  var degree = orientation ? '-90deg' : '90deg';
  return [
    'scale(' + scaleRatio + ')',
    'rotate(' + degree + ')',
    this.getTransformation()
  ].join(' ');
};

/**
 * Obtains CSS transformation that makes the cropped image fit the original
 * image. The new cropped image that the transformation is applied to fits to
 * the cropped rectangle in the original image.
 *
 * @param {number} imageWidth Width of the original image.
 * @param {number} imageHeight Height of the original image.
 * @param {!ImageRect} imageCropRect Crop rectangle in the image's coordinate
 *     system.
 * @return {string} Transformation description.
 */
Viewport.prototype.getInverseTransformForCroppedImage =
    function(imageWidth, imageHeight, imageCropRect) {
  var wholeScale = this.getFittingScaleForImageSize_(
      imageWidth, imageHeight);
  var croppedScale = this.getFittingScaleForImageSize_(
      imageCropRect.width, imageCropRect.height);
  var dx =
      (imageCropRect.left + imageCropRect.width / 2 - imageWidth / 2) *
      wholeScale;
  var dy =
      (imageCropRect.top + imageCropRect.height / 2 - imageHeight / 2) *
      wholeScale;
  return [
    'translate(' + dx + 'px,' + dy + 'px)',
    'scale(' + wholeScale / croppedScale + ')',
    this.getTransformation()
  ].join(' ');
};

/**
 * Obtains CSS transformation that makes the image fit to the screen rectangle.
 *
 * @param {!ImageRect} screenRect Screen rectangle.
 * @return {string} Transformation description.
 */
Viewport.prototype.getScreenRectTransformForImage = function(screenRect) {
  var imageBounds = this.getImageElementBoundsOnScreen();
  var scaleX = screenRect.width / imageBounds.width;
  var scaleY = screenRect.height / imageBounds.height;
  var screenWidth = this.screenBounds_.width;
  var screenHeight = this.screenBounds_.height;
  var dx = screenRect.left + screenRect.width / 2 - screenWidth / 2;
  var dy = screenRect.top + screenRect.height / 2 - screenHeight / 2;
  return [
    'translate(' + dx + 'px,' + dy + 'px)',
    'scale(' + scaleX + ',' + scaleY + ')',
    this.getTransformation()
  ].join(' ');
};

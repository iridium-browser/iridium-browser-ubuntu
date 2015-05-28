// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testImageView() {
  var mockFileSystem = new MockFileSystem('volumeId');
  var mockEntry = new MockEntry(mockFileSystem, '/test.jpg');

  // Item has full size cache.
  var itemWithFullCache = new Gallery.Item(mockEntry, null, {}, null, false);
  itemWithFullCache.contentImage = document.createElement('canvas');
  assertEquals(
      ImageView.LoadTarget.CACHED_MAIN_IMAGE,
      ImageView.getLoadTarget(itemWithFullCache, new ImageView.Effect.None()));

  // Item has screen size cache.
  var itemWithScreenCache = new Gallery.Item(mockEntry, null, {}, null, false);
  itemWithScreenCache.screenImage = document.createElement('canvas');
  assertEquals(
      ImageView.LoadTarget.CACHED_THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithScreenCache, new ImageView.Effect.None()));

  // Item with content thumbnail.
  var itemWithContentThumbnail = new Gallery.Item(
      mockEntry, null, {thumbnail: {url: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithContentThumbnail, new ImageView.Effect.None()));

  // Item with external thumbnail.
  var itemWithExternalThumbnail = new Gallery.Item(
      mockEntry, null, {external: {thumbnailUrl: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithExternalThumbnail, new ImageView.Effect.None()));

  // Item with external thumbnail but present localy.
  var itemWithExternalThumbnailPresent = new Gallery.Item(
      mockEntry, null, {external: {thumbnailUrl: 'url', present: true}}, null,
      false);
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailPresent, new ImageView.Effect.None()));

  // Item with external thumbnail shown by slide effect.
  var itemWithExternalThumbnailSlide = new Gallery.Item(
      mockEntry, null, {external: {thumbnailUrl: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.THUMBNAIL,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailSlide, new ImageView.Effect.Slide(1)));

  // Item with external thumbnail shown by zoom effect.
  var itemWithExternalThumbnailZoom = new Gallery.Item(
      mockEntry, null, {external: {thumbnailUrl: 'url'}}, null, false);
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithExternalThumbnailZoom,
          new ImageView.Effect.Zoom(0, 0, null)));

  // Item without cache/thumbnail.
  var itemWithoutCacheOrThumbnail = new Gallery.Item(
      mockEntry, null, {}, null, false);
  assertEquals(
      ImageView.LoadTarget.MAIN_IMAGE,
      ImageView.getLoadTarget(
          itemWithoutCacheOrThumbnail, new ImageView.Effect.None));
}

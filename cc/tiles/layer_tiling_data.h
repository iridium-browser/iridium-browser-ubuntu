// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_LAYER_TILING_DATA_H_
#define CC_TILES_LAYER_TILING_DATA_H_

#include <utility>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/base/tiling_data.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class CC_EXPORT LayerTilingData {
 public:
  enum BorderTexelOption {
    HAS_BORDER_TEXELS,
    NO_BORDER_TEXELS
  };

  ~LayerTilingData();

  static scoped_ptr<LayerTilingData> Create(const gfx::Size& tile_size,
                                            BorderTexelOption option);

  bool has_empty_bounds() const { return tiling_data_.has_empty_bounds(); }
  int num_tiles_x() const { return tiling_data_.num_tiles_x(); }
  int num_tiles_y() const { return tiling_data_.num_tiles_y(); }
  gfx::Rect tile_bounds(int i, int j) const {
    return tiling_data_.TileBounds(i, j);
  }
  gfx::Vector2d texture_offset(int x_index, int y_index) const {
    return tiling_data_.TextureOffset(x_index, y_index);
  }

  // Change the tile size. This may invalidate all the existing tiles.
  void SetTileSize(const gfx::Size& size);
  gfx::Size tile_size() const;
  // Change the border texel setting. This may invalidate all existing tiles.
  void SetBorderTexelOption(BorderTexelOption option);
  bool has_border_texels() const { return !!tiling_data_.border_texels(); }

  bool is_empty() const { return has_empty_bounds() || !tiles().size(); }

  const LayerTilingData& operator=(const LayerTilingData&);

  class Tile {
   public:
    Tile() : i_(-1), j_(-1) {}
    virtual ~Tile() {}

    int i() const { return i_; }
    int j() const { return j_; }
    void move_to(int i, int j) {
      i_ = i;
      j_ = j;
    }

   private:
    int i_;
    int j_;
    DISALLOW_COPY_AND_ASSIGN(Tile);
  };
  typedef std::pair<int, int> TileMapKey;
  typedef base::ScopedPtrHashMap<TileMapKey, scoped_ptr<Tile>> TileMap;

  void AddTile(scoped_ptr<Tile> tile, int i, int j);
  scoped_ptr<Tile> TakeTile(int i, int j);
  Tile* TileAt(int i, int j) const;
  const TileMap& tiles() const { return tiles_; }

  void SetTilingSize(const gfx::Size& tiling_size);
  gfx::Size tiling_size() const { return tiling_data_.tiling_size(); }

  void ContentRectToTileIndices(const gfx::Rect& rect,
                                int* left,
                                int* top,
                                int* right,
                                int* bottom) const;
  gfx::Rect TileRect(const Tile* tile) const;

  void reset() { tiles_.clear(); }

 protected:
  LayerTilingData(const gfx::Size& tile_size, BorderTexelOption option);

  TileMap tiles_;
  TilingData tiling_data_;

  DISALLOW_COPY(LayerTilingData);
};

}  // namespace cc

#endif  // CC_TILES_LAYER_TILING_DATA_H_

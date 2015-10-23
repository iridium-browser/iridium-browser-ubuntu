// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../../../include/fxge/fx_ge.h"
#include "../../dib/dib_int.h"
#include "../../ge/text_int.h"
#include "../../../../include/fxcodec/fx_codec.h"
#include "../../../../../third_party/agg23/agg_pixfmt_gray.h"
#include "../../../../../third_party/agg23/agg_path_storage.h"
#include "../../../../../third_party/agg23/agg_scanline_u.h"
#include "../../../../../third_party/agg23/agg_rasterizer_scanline_aa.h"
#include "../../../../../third_party/agg23/agg_renderer_scanline.h"
#include "../../../../../third_party/agg23/agg_curves.h"
#include "../../../../../third_party/agg23/agg_conv_stroke.h"
#include "../../../../../third_party/agg23/agg_conv_dash.h"
#include "../include/fx_agg_driver.h"
void _HardClip(FX_FLOAT& x, FX_FLOAT& y) {
  if (x > 50000) {
    x = 50000;
  }
  if (x < -50000) {
    x = -50000;
  }
  if (y > 50000) {
    y = 50000;
  }
  if (y < -50000) {
    y = -50000;
  }
}
void CAgg_PathData::BuildPath(const CFX_PathData* pPathData,
                              const CFX_AffineMatrix* pObject2Device) {
  int nPoints = pPathData->GetPointCount();
  FX_PATHPOINT* pPoints = pPathData->GetPoints();
  for (int i = 0; i < nPoints; i++) {
    FX_FLOAT x = pPoints[i].m_PointX, y = pPoints[i].m_PointY;
    if (pObject2Device) {
      pObject2Device->Transform(x, y);
    }
    _HardClip(x, y);
    int point_type = pPoints[i].m_Flag & FXPT_TYPE;
    if (point_type == FXPT_MOVETO) {
      m_PathData.move_to(x, y);
    } else if (point_type == FXPT_LINETO) {
      if (pPoints[i - 1].m_Flag == FXPT_MOVETO &&
          (i == nPoints - 1 || pPoints[i + 1].m_Flag == FXPT_MOVETO) &&
          pPoints[i].m_PointX == pPoints[i - 1].m_PointX &&
          pPoints[i].m_PointY == pPoints[i - 1].m_PointY) {
        x += 1;
      }
      m_PathData.line_to(x, y);
    } else if (point_type == FXPT_BEZIERTO) {
      FX_FLOAT x0 = pPoints[i - 1].m_PointX, y0 = pPoints[i - 1].m_PointY;
      FX_FLOAT x2 = pPoints[i + 1].m_PointX, y2 = pPoints[i + 1].m_PointY;
      FX_FLOAT x3 = pPoints[i + 2].m_PointX, y3 = pPoints[i + 2].m_PointY;
      if (pObject2Device) {
        pObject2Device->Transform(x0, y0);
        pObject2Device->Transform(x2, y2);
        pObject2Device->Transform(x3, y3);
      }
      agg::curve4 curve(x0, y0, x, y, x2, y2, x3, y3);
      i += 2;
      m_PathData.add_path_curve(curve);
    }
    if (pPoints[i].m_Flag & FXPT_CLOSEFIGURE) {
      m_PathData.end_poly();
    }
  }
}
namespace agg {
template <class BaseRenderer>
class renderer_scanline_aa_offset {
 public:
  typedef BaseRenderer base_ren_type;
  typedef typename base_ren_type::color_type color_type;
  renderer_scanline_aa_offset(base_ren_type& ren, unsigned left, unsigned top)
      : m_ren(&ren), m_left(left), m_top(top) {}
  void color(const color_type& c) { m_color = c; }
  const color_type& color() const { return m_color; }
  void prepare(unsigned) {}
  template <class Scanline>
  void render(const Scanline& sl) {
    int y = sl.y();
    unsigned num_spans = sl.num_spans();
    typename Scanline::const_iterator span = sl.begin();
    for (;;) {
      int x = span->x;
      if (span->len > 0) {
        m_ren->blend_solid_hspan(x - m_left, y - m_top, (unsigned)span->len,
                                 m_color, span->covers);
      } else {
        m_ren->blend_hline(x - m_left, y - m_top, (unsigned)(x - span->len - 1),
                           m_color, *(span->covers));
      }
      if (--num_spans == 0) {
        break;
      }
      ++span;
    }
  }

 private:
  base_ren_type* m_ren;
  color_type m_color;
  unsigned m_left, m_top;
};
}
static void RasterizeStroke(agg::rasterizer_scanline_aa& rasterizer,
                            agg::path_storage& path_data,
                            const CFX_AffineMatrix* pObject2Device,
                            const CFX_GraphStateData* pGraphState,
                            FX_FLOAT scale = 1.0f,
                            FX_BOOL bStrokeAdjust = FALSE,
                            FX_BOOL bTextMode = FALSE) {
  agg::line_cap_e cap;
  switch (pGraphState->m_LineCap) {
    case CFX_GraphStateData::LineCapRound:
      cap = agg::round_cap;
      break;
    case CFX_GraphStateData::LineCapSquare:
      cap = agg::square_cap;
      break;
    default:
      cap = agg::butt_cap;
      break;
  }
  agg::line_join_e join;
  switch (pGraphState->m_LineJoin) {
    case CFX_GraphStateData::LineJoinRound:
      join = agg::round_join;
      break;
    case CFX_GraphStateData::LineJoinBevel:
      join = agg::bevel_join;
      break;
    default:
      join = agg::miter_join_revert;
      break;
  }
  FX_FLOAT width = pGraphState->m_LineWidth * scale;
  FX_FLOAT unit = 1.f;
  if (pObject2Device) {
    unit = FXSYS_Div(
        1.0f, (pObject2Device->GetXUnit() + pObject2Device->GetYUnit()) / 2);
  }
  if (width < unit) {
    width = unit;
  }
  if (pGraphState->m_DashArray == NULL) {
    agg::conv_stroke<agg::path_storage> stroke(path_data);
    stroke.line_join(join);
    stroke.line_cap(cap);
    stroke.miter_limit(pGraphState->m_MiterLimit);
    stroke.width(width);
    rasterizer.add_path_transformed(stroke, pObject2Device);
  } else {
    typedef agg::conv_dash<agg::path_storage> dash_converter;
    dash_converter dash(path_data);
    for (int i = 0; i < (pGraphState->m_DashCount + 1) / 2; i++) {
      FX_FLOAT on = pGraphState->m_DashArray[i * 2];
      if (on <= 0.000001f) {
        on = 1.0f / 10;
      }
      FX_FLOAT off = i * 2 + 1 == pGraphState->m_DashCount
                         ? on
                         : pGraphState->m_DashArray[i * 2 + 1];
      if (off < 0) {
        off = 0;
      }
      dash.add_dash(on * scale, off * scale);
    }
    dash.dash_start(pGraphState->m_DashPhase * scale);
    typedef agg::conv_stroke<dash_converter> dash_stroke;
    dash_stroke stroke(dash);
    stroke.line_join(join);
    stroke.line_cap(cap);
    stroke.miter_limit(pGraphState->m_MiterLimit);
    stroke.width(width);
    rasterizer.add_path_transformed(stroke, pObject2Device);
  }
}
IFX_RenderDeviceDriver* IFX_RenderDeviceDriver::CreateFxgeDriver(
    CFX_DIBitmap* pBitmap,
    FX_BOOL bRgbByteOrder,
    CFX_DIBitmap* pOriDevice,
    FX_BOOL bGroupKnockout) {
  return new CFX_AggDeviceDriver(pBitmap, 0, bRgbByteOrder, pOriDevice,
                                 bGroupKnockout);
}
CFX_AggDeviceDriver::CFX_AggDeviceDriver(CFX_DIBitmap* pBitmap,
                                         int dither_bits,
                                         FX_BOOL bRgbByteOrder,
                                         CFX_DIBitmap* pOriDevice,
                                         FX_BOOL bGroupKnockout) {
  m_pBitmap = pBitmap;
  m_DitherBits = dither_bits;
  m_pClipRgn = NULL;
  m_pPlatformBitmap = NULL;
  m_pPlatformGraphics = NULL;
  m_pDwRenderTartget = NULL;
  m_bRgbByteOrder = bRgbByteOrder;
  m_pOriDevice = pOriDevice;
  m_bGroupKnockout = bGroupKnockout;
  m_FillFlags = 0;
  InitPlatform();
}
CFX_AggDeviceDriver::~CFX_AggDeviceDriver() {
  delete m_pClipRgn;
  for (int i = 0; i < m_StateStack.GetSize(); i++)
    delete (CFX_ClipRgn*)m_StateStack[i];
  DestroyPlatform();
}
#if _FXM_PLATFORM_ != _FXM_PLATFORM_APPLE_
void CFX_AggDeviceDriver::InitPlatform() {}
void CFX_AggDeviceDriver::DestroyPlatform() {}
FX_BOOL CFX_AggDeviceDriver::DrawDeviceText(
    int nChars,
    const FXTEXT_CHARPOS* pCharPos,
    CFX_Font* pFont,
    CFX_FontCache* pCache,
    const CFX_AffineMatrix* pObject2Device,
    FX_FLOAT font_size,
    FX_DWORD color,
    int alpha_flag,
    void* pIccTransform) {
  return FALSE;
}
#endif
int CFX_AggDeviceDriver::GetDeviceCaps(int caps_id) {
  switch (caps_id) {
    case FXDC_DEVICE_CLASS:
      return FXDC_DISPLAY;
    case FXDC_PIXEL_WIDTH:
      return m_pBitmap->GetWidth();
    case FXDC_PIXEL_HEIGHT:
      return m_pBitmap->GetHeight();
    case FXDC_BITS_PIXEL:
      return m_pBitmap->GetBPP();
    case FXDC_HORZ_SIZE:
    case FXDC_VERT_SIZE:
      return 0;
    case FXDC_RENDER_CAPS: {
      int flags = FXRC_GET_BITS | FXRC_ALPHA_PATH | FXRC_ALPHA_IMAGE |
                  FXRC_BLEND_MODE | FXRC_SOFT_CLIP;
      if (m_pBitmap->HasAlpha()) {
        flags |= FXRC_ALPHA_OUTPUT;
      } else if (m_pBitmap->IsAlphaMask()) {
        if (m_pBitmap->GetBPP() == 1) {
          flags |= FXRC_BITMASK_OUTPUT;
        } else {
          flags |= FXRC_BYTEMASK_OUTPUT;
        }
      }
      if (m_pBitmap->IsCmykImage()) {
        flags |= FXRC_CMYK_OUTPUT;
      }
      return flags;
    }
    case FXDC_DITHER_BITS:
      return m_DitherBits;
  }
  return 0;
}
void CFX_AggDeviceDriver::SaveState() {
  void* pClip = NULL;
  if (m_pClipRgn) {
    pClip = new CFX_ClipRgn(*m_pClipRgn);
  }
  m_StateStack.Add(pClip);
}
void CFX_AggDeviceDriver::RestoreState(FX_BOOL bKeepSaved) {
  if (m_StateStack.GetSize() == 0) {
    delete m_pClipRgn;
    m_pClipRgn = NULL;
    return;
  }
  CFX_ClipRgn* pSavedClip =
      (CFX_ClipRgn*)m_StateStack[m_StateStack.GetSize() - 1];
  delete m_pClipRgn;
  m_pClipRgn = NULL;
  if (bKeepSaved) {
    if (pSavedClip) {
      m_pClipRgn = new CFX_ClipRgn(*pSavedClip);
    }
  } else {
    m_StateStack.RemoveAt(m_StateStack.GetSize() - 1);
    m_pClipRgn = pSavedClip;
  }
}
void CFX_AggDeviceDriver::SetClipMask(agg::rasterizer_scanline_aa& rasterizer) {
  FX_RECT path_rect(rasterizer.min_x(), rasterizer.min_y(),
                    rasterizer.max_x() + 1, rasterizer.max_y() + 1);
  path_rect.Intersect(m_pClipRgn->GetBox());
  CFX_DIBitmapRef mask;
  CFX_DIBitmap* pThisLayer = mask.New();
  if (!pThisLayer) {
    return;
  }
  pThisLayer->Create(path_rect.Width(), path_rect.Height(), FXDIB_8bppMask);
  pThisLayer->Clear(0);
  agg::rendering_buffer raw_buf(pThisLayer->GetBuffer(), pThisLayer->GetWidth(),
                                pThisLayer->GetHeight(),
                                pThisLayer->GetPitch());
  agg::pixfmt_gray8 pixel_buf(raw_buf);
  agg::renderer_base<agg::pixfmt_gray8> base_buf(pixel_buf);
  agg::renderer_scanline_aa_offset<agg::renderer_base<agg::pixfmt_gray8> >
      final_render(base_buf, path_rect.left, path_rect.top);
  final_render.color(agg::gray8(255));
  agg::scanline_u8 scanline;
  agg::render_scanlines(rasterizer, scanline, final_render,
                        (m_FillFlags & FXFILL_NOPATHSMOOTH) != 0);
  m_pClipRgn->IntersectMaskF(path_rect.left, path_rect.top, mask);
}
FX_BOOL CFX_AggDeviceDriver::SetClip_PathFill(
    const CFX_PathData* pPathData,
    const CFX_AffineMatrix* pObject2Device,
    int fill_mode) {
  m_FillFlags = fill_mode;
  if (m_pClipRgn == NULL) {
    m_pClipRgn = new CFX_ClipRgn(GetDeviceCaps(FXDC_PIXEL_WIDTH),
                                 GetDeviceCaps(FXDC_PIXEL_HEIGHT));
  }
  if (pPathData->GetPointCount() == 5 || pPathData->GetPointCount() == 4) {
    CFX_FloatRect rectf;
    if (pPathData->IsRect(pObject2Device, &rectf)) {
      rectf.Intersect(
          CFX_FloatRect(0, 0, (FX_FLOAT)GetDeviceCaps(FXDC_PIXEL_WIDTH),
                        (FX_FLOAT)GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
      FX_RECT rect = rectf.GetOutterRect();
      m_pClipRgn->IntersectRect(rect);
      return TRUE;
    }
  }
  CAgg_PathData path_data;
  path_data.BuildPath(pPathData, pObject2Device);
  path_data.m_PathData.end_poly();
  agg::rasterizer_scanline_aa rasterizer;
  rasterizer.clip_box(0.0f, 0.0f, (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_WIDTH)),
                      (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
  rasterizer.add_path(path_data.m_PathData);
  rasterizer.filling_rule((fill_mode & 3) == FXFILL_WINDING
                              ? agg::fill_non_zero
                              : agg::fill_even_odd);
  SetClipMask(rasterizer);
  return TRUE;
}
FX_BOOL CFX_AggDeviceDriver::SetClip_PathStroke(
    const CFX_PathData* pPathData,
    const CFX_AffineMatrix* pObject2Device,
    const CFX_GraphStateData* pGraphState) {
  if (m_pClipRgn == NULL) {
    m_pClipRgn = new CFX_ClipRgn(GetDeviceCaps(FXDC_PIXEL_WIDTH),
                                 GetDeviceCaps(FXDC_PIXEL_HEIGHT));
  }
  CAgg_PathData path_data;
  path_data.BuildPath(pPathData, NULL);
  agg::rasterizer_scanline_aa rasterizer;
  rasterizer.clip_box(0.0f, 0.0f, (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_WIDTH)),
                      (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
  RasterizeStroke(rasterizer, path_data.m_PathData, pObject2Device,
                  pGraphState);
  rasterizer.filling_rule(agg::fill_non_zero);
  SetClipMask(rasterizer);
  return TRUE;
}
class CFX_Renderer {
 private:
  int m_Alpha, m_Red, m_Green, m_Blue, m_Gray;
  FX_DWORD m_Color;
  FX_BOOL m_bFullCover;
  FX_BOOL m_bRgbByteOrder;
  CFX_DIBitmap* m_pOriDevice;
  FX_RECT m_ClipBox;
  const CFX_DIBitmap* m_pClipMask;
  CFX_DIBitmap* m_pDevice;
  const CFX_ClipRgn* m_pClipRgn;
  void (CFX_Renderer::*composite_span)(uint8_t*,
                                       int,
                                       int,
                                       int,
                                       uint8_t*,
                                       int,
                                       int,
                                       uint8_t*,
                                       uint8_t*);

 public:
  void prepare(unsigned) {}
  void CompositeSpan(uint8_t* dest_scan,
                     uint8_t* ori_scan,
                     int Bpp,
                     FX_BOOL bDestAlpha,
                     int span_left,
                     int span_len,
                     uint8_t* cover_scan,
                     int clip_left,
                     int clip_right,
                     uint8_t* clip_scan) {
    ASSERT(!m_pDevice->IsCmykImage());
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    if (Bpp) {
      dest_scan += col_start * Bpp;
      ori_scan += col_start * Bpp;
    } else {
      dest_scan += col_start / 8;
      ori_scan += col_start / 8;
    }
    if (m_bRgbByteOrder) {
      if (Bpp == 4 && bDestAlpha) {
        for (int col = col_start; col < col_end; col++) {
          int src_alpha;
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
          uint8_t dest_alpha =
              ori_scan[3] + src_alpha - ori_scan[3] * src_alpha / 255;
          dest_scan[3] = dest_alpha;
          int alpha_ratio = src_alpha * 255 / dest_alpha;
          if (m_bFullCover) {
            *dest_scan++ = FXDIB_ALPHA_MERGE(*ori_scan++, m_Red, alpha_ratio);
            *dest_scan++ = FXDIB_ALPHA_MERGE(*ori_scan++, m_Green, alpha_ratio);
            *dest_scan++ = FXDIB_ALPHA_MERGE(*ori_scan++, m_Blue, alpha_ratio);
            dest_scan++;
            ori_scan++;
          } else {
            int r = FXDIB_ALPHA_MERGE(*ori_scan++, m_Red, alpha_ratio);
            int g = FXDIB_ALPHA_MERGE(*ori_scan++, m_Green, alpha_ratio);
            int b = FXDIB_ALPHA_MERGE(*ori_scan++, m_Blue, alpha_ratio);
            ori_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, r, cover_scan[col]);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, g, cover_scan[col]);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, b, cover_scan[col]);
            dest_scan += 2;
          }
        }
        return;
      }
      if (Bpp == 3 || Bpp == 4) {
        for (int col = col_start; col < col_end; col++) {
          int src_alpha;
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
          int r = FXDIB_ALPHA_MERGE(*ori_scan++, m_Red, src_alpha);
          int g = FXDIB_ALPHA_MERGE(*ori_scan++, m_Green, src_alpha);
          int b = FXDIB_ALPHA_MERGE(*ori_scan, m_Blue, src_alpha);
          ori_scan += Bpp - 2;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, r, cover_scan[col]);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, g, cover_scan[col]);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, b, cover_scan[col]);
          dest_scan += Bpp - 2;
        }
      }
      return;
    }
    if (Bpp == 4 && bDestAlpha) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * clip_scan[col] / 255;
        } else {
          src_alpha = m_Alpha;
        }
        int src_alpha_covered = src_alpha * cover_scan[col] / 255;
        if (src_alpha_covered == 0) {
          dest_scan += 4;
          continue;
        }
        if (cover_scan[col] == 255) {
          dest_scan[3] = src_alpha_covered;
          *dest_scan++ = m_Blue;
          *dest_scan++ = m_Green;
          *dest_scan = m_Red;
          dest_scan += 2;
          continue;
        } else {
          if (dest_scan[3] == 0) {
            dest_scan[3] = src_alpha_covered;
            *dest_scan++ = m_Blue;
            *dest_scan++ = m_Green;
            *dest_scan = m_Red;
            dest_scan += 2;
            continue;
          }
          uint8_t cover = cover_scan[col];
          dest_scan[3] = FXDIB_ALPHA_MERGE(dest_scan[3], src_alpha, cover);
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, cover);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, cover);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, cover);
          dest_scan += 2;
        }
      }
      return;
    }
    if (Bpp == 3 || Bpp == 4) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * clip_scan[col] / 255;
        } else {
          src_alpha = m_Alpha;
        }
        if (m_bFullCover) {
          *dest_scan++ = FXDIB_ALPHA_MERGE(*ori_scan++, m_Blue, src_alpha);
          *dest_scan++ = FXDIB_ALPHA_MERGE(*ori_scan++, m_Green, src_alpha);
          *dest_scan = FXDIB_ALPHA_MERGE(*ori_scan, m_Red, src_alpha);
          dest_scan += Bpp - 2;
          ori_scan += Bpp - 2;
          continue;
        }
        int b = FXDIB_ALPHA_MERGE(*ori_scan++, m_Blue, src_alpha);
        int g = FXDIB_ALPHA_MERGE(*ori_scan++, m_Green, src_alpha);
        int r = FXDIB_ALPHA_MERGE(*ori_scan, m_Red, src_alpha);
        ori_scan += Bpp - 2;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, b, cover_scan[col]);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, g, cover_scan[col]);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, r, cover_scan[col]);
        dest_scan += Bpp - 2;
        continue;
      }
      return;
    }
    if (Bpp == 1) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * clip_scan[col] / 255;
        } else {
          src_alpha = m_Alpha;
        }
        if (m_bFullCover) {
          *dest_scan = FXDIB_ALPHA_MERGE(*ori_scan++, m_Gray, src_alpha);
        } else {
          int gray = FXDIB_ALPHA_MERGE(*ori_scan++, m_Gray, src_alpha);
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, gray, cover_scan[col]);
          dest_scan++;
        }
      }
    } else {
      int index = 0;
      if (m_pDevice->GetPalette() == NULL) {
        index = ((uint8_t)m_Color == 0xff) ? 1 : 0;
      } else {
        for (int i = 0; i < 2; i++)
          if (FXARGB_TODIB(m_pDevice->GetPalette()[i]) == m_Color) {
            index = i;
          }
      }
      uint8_t* dest_scan1 = dest_scan;
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
        } else {
          src_alpha = m_Alpha * cover_scan[col] / 255;
        }
        if (src_alpha) {
          if (!index) {
            *dest_scan1 &= ~(1 << (7 - (col + span_left) % 8));
          } else {
            *dest_scan1 |= 1 << (7 - (col + span_left) % 8);
          }
        }
        dest_scan1 = dest_scan + (span_left % 8 + col - col_start + 1) / 8;
      }
    }
  }
  void CompositeSpan1bpp(uint8_t* dest_scan,
                         int Bpp,
                         int span_left,
                         int span_len,
                         uint8_t* cover_scan,
                         int clip_left,
                         int clip_right,
                         uint8_t* clip_scan,
                         uint8_t* dest_extra_alpha_scan) {
    ASSERT(!m_bRgbByteOrder);
    ASSERT(!m_pDevice->IsCmykImage());
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    dest_scan += col_start / 8;
    int index = 0;
    if (m_pDevice->GetPalette() == NULL) {
      index = ((uint8_t)m_Color == 0xff) ? 1 : 0;
    } else {
      for (int i = 0; i < 2; i++)
        if (FXARGB_TODIB(m_pDevice->GetPalette()[i]) == m_Color) {
          index = i;
        }
    }
    uint8_t* dest_scan1 = dest_scan;
    for (int col = col_start; col < col_end; col++) {
      int src_alpha;
      if (clip_scan) {
        src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
      } else {
        src_alpha = m_Alpha * cover_scan[col] / 255;
      }
      if (src_alpha) {
        if (!index) {
          *dest_scan1 &= ~(1 << (7 - (col + span_left) % 8));
        } else {
          *dest_scan1 |= 1 << (7 - (col + span_left) % 8);
        }
      }
      dest_scan1 = dest_scan + (span_left % 8 + col - col_start + 1) / 8;
    }
  }
  void CompositeSpanGray(uint8_t* dest_scan,
                         int Bpp,
                         int span_left,
                         int span_len,
                         uint8_t* cover_scan,
                         int clip_left,
                         int clip_right,
                         uint8_t* clip_scan,
                         uint8_t* dest_extra_alpha_scan) {
    ASSERT(!m_bRgbByteOrder);
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    dest_scan += col_start;
    if (dest_extra_alpha_scan) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (m_bFullCover) {
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
        } else {
          if (clip_scan) {
            src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
          } else {
            src_alpha = m_Alpha * cover_scan[col] / 255;
          }
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *dest_scan = m_Gray;
            *dest_extra_alpha_scan = m_Alpha;
          } else {
            uint8_t dest_alpha = (*dest_extra_alpha_scan) + src_alpha -
                                 (*dest_extra_alpha_scan) * src_alpha / 255;
            *dest_extra_alpha_scan++ = dest_alpha;
            int alpha_ratio = src_alpha * 255 / dest_alpha;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Gray, alpha_ratio);
            dest_scan++;
            continue;
          }
        }
        dest_extra_alpha_scan++;
        dest_scan++;
      }
    } else {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
        } else {
          src_alpha = m_Alpha * cover_scan[col] / 255;
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *dest_scan = m_Gray;
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Gray, src_alpha);
          }
        }
        dest_scan++;
      }
    }
  }
  void CompositeSpanARGB(uint8_t* dest_scan,
                         int Bpp,
                         int span_left,
                         int span_len,
                         uint8_t* cover_scan,
                         int clip_left,
                         int clip_right,
                         uint8_t* clip_scan,
                         uint8_t* dest_extra_alpha_scan) {
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    dest_scan += col_start * Bpp;
    if (m_bRgbByteOrder) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (m_bFullCover) {
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
        } else {
          if (clip_scan) {
            src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
          } else {
            src_alpha = m_Alpha * cover_scan[col] / 255;
          }
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *(FX_DWORD*)dest_scan = m_Color;
          } else {
            uint8_t dest_alpha =
                dest_scan[3] + src_alpha - dest_scan[3] * src_alpha / 255;
            dest_scan[3] = dest_alpha;
            int alpha_ratio = src_alpha * 255 / dest_alpha;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, alpha_ratio);
            dest_scan += 2;
            continue;
          }
        }
        dest_scan += 4;
      }
      return;
    }
    for (int col = col_start; col < col_end; col++) {
      int src_alpha;
      if (m_bFullCover) {
        if (clip_scan) {
          src_alpha = m_Alpha * clip_scan[col] / 255;
        } else {
          src_alpha = m_Alpha;
        }
      } else {
        if (clip_scan) {
          src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
        } else {
          src_alpha = m_Alpha * cover_scan[col] / 255;
        }
      }
      if (src_alpha) {
        if (src_alpha == 255) {
          *(FX_DWORD*)dest_scan = m_Color;
        } else {
          if (dest_scan[3] == 0) {
            dest_scan[3] = src_alpha;
            *dest_scan++ = m_Blue;
            *dest_scan++ = m_Green;
            *dest_scan = m_Red;
            dest_scan += 2;
            continue;
          }
          uint8_t dest_alpha =
              dest_scan[3] + src_alpha - dest_scan[3] * src_alpha / 255;
          dest_scan[3] = dest_alpha;
          int alpha_ratio = src_alpha * 255 / dest_alpha;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, alpha_ratio);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, alpha_ratio);
          dest_scan++;
          *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, alpha_ratio);
          dest_scan += 2;
          continue;
        }
      }
      dest_scan += Bpp;
    }
  }
  void CompositeSpanRGB(uint8_t* dest_scan,
                        int Bpp,
                        int span_left,
                        int span_len,
                        uint8_t* cover_scan,
                        int clip_left,
                        int clip_right,
                        uint8_t* clip_scan,
                        uint8_t* dest_extra_alpha_scan) {
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    dest_scan += col_start * Bpp;
    if (m_bRgbByteOrder) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
        } else {
          src_alpha = m_Alpha * cover_scan[col] / 255;
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            if (Bpp == 4) {
              *(FX_DWORD*)dest_scan = m_Color;
            } else if (Bpp == 3) {
              *dest_scan++ = m_Red;
              *dest_scan++ = m_Green;
              *dest_scan++ = m_Blue;
              continue;
            }
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, src_alpha);
            dest_scan += Bpp - 2;
            continue;
          }
        }
        dest_scan += Bpp;
      }
      return;
    }
    if (Bpp == 3 && dest_extra_alpha_scan) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (m_bFullCover) {
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
        } else {
          if (clip_scan) {
            src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
          } else {
            src_alpha = m_Alpha * cover_scan[col] / 255;
          }
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *dest_scan++ = (uint8_t)m_Blue;
            *dest_scan++ = (uint8_t)m_Green;
            *dest_scan++ = (uint8_t)m_Red;
            *dest_extra_alpha_scan++ = (uint8_t)m_Alpha;
            continue;
          } else {
            uint8_t dest_alpha = (*dest_extra_alpha_scan) + src_alpha -
                                 (*dest_extra_alpha_scan) * src_alpha / 255;
            *dest_extra_alpha_scan++ = dest_alpha;
            int alpha_ratio = src_alpha * 255 / dest_alpha;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, alpha_ratio);
            dest_scan++;
            continue;
          }
        }
        dest_extra_alpha_scan++;
        dest_scan += Bpp;
      }
    } else {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (m_bFullCover) {
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
        } else {
          if (clip_scan) {
            src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
          } else {
            src_alpha = m_Alpha * cover_scan[col] / 255;
          }
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            if (Bpp == 4) {
              *(FX_DWORD*)dest_scan = m_Color;
            } else if (Bpp == 3) {
              *dest_scan++ = m_Blue;
              *dest_scan++ = m_Green;
              *dest_scan++ = m_Red;
              continue;
            }
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, src_alpha);
            dest_scan += Bpp - 2;
            continue;
          }
        }
        dest_scan += Bpp;
      }
    }
  }
  void CompositeSpanCMYK(uint8_t* dest_scan,
                         int Bpp,
                         int span_left,
                         int span_len,
                         uint8_t* cover_scan,
                         int clip_left,
                         int clip_right,
                         uint8_t* clip_scan,
                         uint8_t* dest_extra_alpha_scan) {
    ASSERT(!m_bRgbByteOrder);
    int col_start = span_left < clip_left ? clip_left - span_left : 0;
    int col_end = (span_left + span_len) < clip_right
                      ? span_len
                      : (clip_right - span_left);
    dest_scan += col_start * 4;
    if (dest_extra_alpha_scan) {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (m_bFullCover) {
          if (clip_scan) {
            src_alpha = m_Alpha * clip_scan[col] / 255;
          } else {
            src_alpha = m_Alpha;
          }
        } else {
          if (clip_scan) {
            src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
          } else {
            src_alpha = m_Alpha * cover_scan[col] / 255;
          }
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *(FX_CMYK*)dest_scan = m_Color;
            *dest_extra_alpha_scan = (uint8_t)m_Alpha;
          } else {
            uint8_t dest_alpha = (*dest_extra_alpha_scan) + src_alpha -
                                 (*dest_extra_alpha_scan) * src_alpha / 255;
            *dest_extra_alpha_scan++ = dest_alpha;
            int alpha_ratio = src_alpha * 255 / dest_alpha;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, alpha_ratio);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Gray, alpha_ratio);
            dest_scan++;
            continue;
          }
        }
        dest_extra_alpha_scan++;
        dest_scan += 4;
      }
    } else {
      for (int col = col_start; col < col_end; col++) {
        int src_alpha;
        if (clip_scan) {
          src_alpha = m_Alpha * cover_scan[col] * clip_scan[col] / 255 / 255;
        } else {
          src_alpha = m_Alpha * cover_scan[col] / 255;
        }
        if (src_alpha) {
          if (src_alpha == 255) {
            *(FX_CMYK*)dest_scan = m_Color;
          } else {
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Red, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Green, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Blue, src_alpha);
            dest_scan++;
            *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, m_Gray, src_alpha);
            dest_scan++;
            continue;
          }
        }
        dest_scan += 4;
      }
    }
  }
  template <class Scanline>
  void render(const Scanline& sl) {
    if (m_pOriDevice == NULL && composite_span == NULL) {
      return;
    }
    int y = sl.y();
    if (y < m_ClipBox.top || y >= m_ClipBox.bottom) {
      return;
    }
    uint8_t* dest_scan = m_pDevice->GetBuffer() + m_pDevice->GetPitch() * y;
    uint8_t* dest_scan_extra_alpha = NULL;
    CFX_DIBitmap* pAlphaMask = m_pDevice->m_pAlphaMask;
    if (pAlphaMask) {
      dest_scan_extra_alpha =
          pAlphaMask->GetBuffer() + pAlphaMask->GetPitch() * y;
    }
    uint8_t* ori_scan = NULL;
    if (m_pOriDevice) {
      ori_scan = m_pOriDevice->GetBuffer() + m_pOriDevice->GetPitch() * y;
    }
    int Bpp = m_pDevice->GetBPP() / 8;
    FX_BOOL bDestAlpha = m_pDevice->HasAlpha() || m_pDevice->IsAlphaMask();
    unsigned num_spans = sl.num_spans();
    typename Scanline::const_iterator span = sl.begin();
    while (1) {
      int x = span->x;
      ASSERT(span->len > 0);
      uint8_t* dest_pos = NULL;
      uint8_t* dest_extra_alpha_pos = NULL;
      uint8_t* ori_pos = NULL;
      if (Bpp) {
        ori_pos = ori_scan ? ori_scan + x * Bpp : NULL;
        dest_pos = dest_scan + x * Bpp;
        dest_extra_alpha_pos =
            dest_scan_extra_alpha ? dest_scan_extra_alpha + x : NULL;
      } else {
        dest_pos = dest_scan + x / 8;
        ori_pos = ori_scan ? ori_scan + x / 8 : NULL;
      }
      uint8_t* clip_pos = NULL;
      if (m_pClipMask) {
        clip_pos = m_pClipMask->GetBuffer() +
                   (y - m_ClipBox.top) * m_pClipMask->GetPitch() + x -
                   m_ClipBox.left;
      }
      if (ori_pos) {
        CompositeSpan(dest_pos, ori_pos, Bpp, bDestAlpha, x, span->len,
                      span->covers, m_ClipBox.left, m_ClipBox.right, clip_pos);
      } else {
        (this->*composite_span)(dest_pos, Bpp, x, span->len, span->covers,
                                m_ClipBox.left, m_ClipBox.right, clip_pos,
                                dest_extra_alpha_pos);
      }
      if (--num_spans == 0) {
        break;
      }
      ++span;
    }
  }

  FX_BOOL Init(CFX_DIBitmap* pDevice,
               CFX_DIBitmap* pOriDevice,
               const CFX_ClipRgn* pClipRgn,
               FX_DWORD color,
               FX_BOOL bFullCover,
               FX_BOOL bRgbByteOrder,
               int alpha_flag = 0,
               void* pIccTransform = NULL) {
    m_pDevice = pDevice;
    m_pClipRgn = pClipRgn;
    composite_span = NULL;
    m_bRgbByteOrder = bRgbByteOrder;
    m_pOriDevice = pOriDevice;
    if (m_pClipRgn) {
      m_ClipBox = m_pClipRgn->GetBox();
    } else {
      m_ClipBox.left = m_ClipBox.top = 0;
      m_ClipBox.right = m_pDevice->GetWidth();
      m_ClipBox.bottom = m_pDevice->GetHeight();
    }
    m_pClipMask = NULL;
    if (m_pClipRgn && m_pClipRgn->GetType() == CFX_ClipRgn::MaskF) {
      m_pClipMask = m_pClipRgn->GetMask();
    }
    m_bFullCover = bFullCover;
    FX_BOOL bObjectCMYK = FXGETFLAG_COLORTYPE(alpha_flag);
    FX_BOOL bDeviceCMYK = pDevice->IsCmykImage();
    m_Alpha = bObjectCMYK ? FXGETFLAG_ALPHA_FILL(alpha_flag) : FXARGB_A(color);
    ICodec_IccModule* pIccModule = NULL;
    if (!CFX_GEModule::Get()->GetCodecModule() ||
        !CFX_GEModule::Get()->GetCodecModule()->GetIccModule()) {
      pIccTransform = NULL;
    } else {
      pIccModule = CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
    }
    if (m_pDevice->GetBPP() == 8) {
      ASSERT(!m_bRgbByteOrder);
      composite_span = &CFX_Renderer::CompositeSpanGray;
      if (m_pDevice->IsAlphaMask()) {
        m_Gray = 255;
      } else {
        if (pIccTransform) {
          uint8_t gray;
          color = bObjectCMYK ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
          pIccModule->TranslateScanline(pIccTransform, &gray,
                                        (const uint8_t*)&color, 1);
          m_Gray = gray;
        } else {
          if (bObjectCMYK) {
            uint8_t r, g, b;
            AdobeCMYK_to_sRGB1(FXSYS_GetCValue(color), FXSYS_GetMValue(color),
                               FXSYS_GetYValue(color), FXSYS_GetKValue(color),
                               r, g, b);
            m_Gray = FXRGB2GRAY(r, g, b);
          } else {
            m_Gray =
                FXRGB2GRAY(FXARGB_R(color), FXARGB_G(color), FXARGB_B(color));
          }
        }
      }
      return TRUE;
    }
    if (bDeviceCMYK) {
      ASSERT(!m_bRgbByteOrder);
      composite_span = &CFX_Renderer::CompositeSpanCMYK;
      if (bObjectCMYK) {
        m_Color = FXCMYK_TODIB(color);
        if (pIccTransform) {
          pIccModule->TranslateScanline(pIccTransform, (uint8_t*)&m_Color,
                                        (const uint8_t*)&m_Color, 1);
        }
      } else {
        if (!pIccTransform) {
          return FALSE;
        }
        color = FXARGB_TODIB(color);
        pIccModule->TranslateScanline(pIccTransform, (uint8_t*)&m_Color,
                                      (const uint8_t*)&color, 1);
      }
      m_Red = ((uint8_t*)&m_Color)[0];
      m_Green = ((uint8_t*)&m_Color)[1];
      m_Blue = ((uint8_t*)&m_Color)[2];
      m_Gray = ((uint8_t*)&m_Color)[3];
    } else {
      composite_span = (pDevice->GetFormat() == FXDIB_Argb)
                           ? &CFX_Renderer::CompositeSpanARGB
                           : &CFX_Renderer::CompositeSpanRGB;
      if (pIccTransform) {
        color = bObjectCMYK ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
        pIccModule->TranslateScanline(pIccTransform, (uint8_t*)&m_Color,
                                      (const uint8_t*)&color, 1);
        ((uint8_t*)&m_Color)[3] = m_Alpha;
        m_Red = ((uint8_t*)&m_Color)[2];
        m_Green = ((uint8_t*)&m_Color)[1];
        m_Blue = ((uint8_t*)&m_Color)[0];
        if (m_bRgbByteOrder) {
          m_Color = FXARGB_TODIB(m_Color);
          m_Color = FXARGB_TOBGRORDERDIB(m_Color);
        }
      } else {
        if (bObjectCMYK) {
          uint8_t r, g, b;
          AdobeCMYK_to_sRGB1(FXSYS_GetCValue(color), FXSYS_GetMValue(color),
                             FXSYS_GetYValue(color), FXSYS_GetKValue(color), r,
                             g, b);
          m_Color = FXARGB_MAKE(m_Alpha, r, g, b);
          if (m_bRgbByteOrder) {
            m_Color = FXARGB_TOBGRORDERDIB(m_Color);
          } else {
            m_Color = FXARGB_TODIB(m_Color);
          }
          m_Red = r;
          m_Green = g;
          m_Blue = b;
        } else {
          if (m_bRgbByteOrder) {
            m_Color = FXARGB_TOBGRORDERDIB(color);
          } else {
            m_Color = FXARGB_TODIB(color);
          }
          ArgbDecode(color, m_Alpha, m_Red, m_Green, m_Blue);
        }
      }
    }
    if (m_pDevice->GetBPP() == 1) {
      composite_span = &CFX_Renderer::CompositeSpan1bpp;
    }
    return TRUE;
  }
};
FX_BOOL CFX_AggDeviceDriver::RenderRasterizer(
    agg::rasterizer_scanline_aa& rasterizer,
    FX_DWORD color,
    FX_BOOL bFullCover,
    FX_BOOL bGroupKnockout,
    int alpha_flag,
    void* pIccTransform) {
  CFX_DIBitmap* pt = bGroupKnockout ? m_pOriDevice : NULL;
  CFX_Renderer render;
  if (!render.Init(m_pBitmap, pt, m_pClipRgn, color, bFullCover,
                   m_bRgbByteOrder, alpha_flag, pIccTransform)) {
    return FALSE;
  }
  agg::scanline_u8 scanline;
  agg::render_scanlines(rasterizer, scanline, render,
                        (m_FillFlags & FXFILL_NOPATHSMOOTH) != 0);
  return TRUE;
}
FX_BOOL CFX_AggDeviceDriver::DrawPath(const CFX_PathData* pPathData,
                                      const CFX_AffineMatrix* pObject2Device,
                                      const CFX_GraphStateData* pGraphState,
                                      FX_DWORD fill_color,
                                      FX_DWORD stroke_color,
                                      int fill_mode,
                                      int alpha_flag,
                                      void* pIccTransform,
                                      int blend_type) {
  if (blend_type != FXDIB_BLEND_NORMAL) {
    return FALSE;
  }
  if (GetBuffer() == NULL) {
    return TRUE;
  }
  m_FillFlags = fill_mode;
  if ((fill_mode & 3) && fill_color) {
    CAgg_PathData path_data;
    path_data.BuildPath(pPathData, pObject2Device);
    agg::rasterizer_scanline_aa rasterizer;
    rasterizer.clip_box(0.0f, 0.0f, (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_WIDTH)),
                        (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
    rasterizer.add_path(path_data.m_PathData);
    rasterizer.filling_rule((fill_mode & 3) == FXFILL_WINDING
                                ? agg::fill_non_zero
                                : agg::fill_even_odd);
    if (!RenderRasterizer(rasterizer, fill_color, fill_mode & FXFILL_FULLCOVER,
                          FALSE, alpha_flag, pIccTransform)) {
      return FALSE;
    }
  }
  int stroke_alpha = FXGETFLAG_COLORTYPE(alpha_flag)
                         ? FXGETFLAG_ALPHA_STROKE(alpha_flag)
                         : FXARGB_A(stroke_color);
  if (pGraphState && stroke_alpha) {
    if (fill_mode & FX_ZEROAREA_FILL) {
      CAgg_PathData path_data;
      path_data.BuildPath(pPathData, pObject2Device);
      agg::rasterizer_scanline_aa rasterizer;
      rasterizer.clip_box(0.0f, 0.0f,
                          (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_WIDTH)),
                          (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
      RasterizeStroke(rasterizer, path_data.m_PathData, NULL, pGraphState, 1,
                      FALSE, fill_mode & FX_STROKE_TEXT_MODE);
      int fill_flag = FXGETFLAG_COLORTYPE(alpha_flag) << 8 |
                      FXGETFLAG_ALPHA_STROKE(alpha_flag);
      if (!RenderRasterizer(rasterizer, stroke_color,
                            fill_mode & FXFILL_FULLCOVER, m_bGroupKnockout,
                            fill_flag, pIccTransform)) {
        return FALSE;
      }
      return TRUE;
    }
    CFX_AffineMatrix matrix1, matrix2;
    if (pObject2Device) {
      matrix1.a =
          FX_MAX(FXSYS_fabs(pObject2Device->a), FXSYS_fabs(pObject2Device->b));
      matrix1.d = matrix1.a;
      matrix2.Set(pObject2Device->a / matrix1.a, pObject2Device->b / matrix1.a,
                  pObject2Device->c / matrix1.d, pObject2Device->d / matrix1.d,
                  0, 0);
      CFX_AffineMatrix mtRervese;
      mtRervese.SetReverse(matrix2);
      matrix1 = *pObject2Device;
      matrix1.Concat(mtRervese);
    }
    CAgg_PathData path_data;
    path_data.BuildPath(pPathData, &matrix1);
    agg::rasterizer_scanline_aa rasterizer;
    rasterizer.clip_box(0.0f, 0.0f, (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_WIDTH)),
                        (FX_FLOAT)(GetDeviceCaps(FXDC_PIXEL_HEIGHT)));
    RasterizeStroke(rasterizer, path_data.m_PathData, &matrix2, pGraphState,
                    matrix1.a, FALSE, fill_mode & FX_STROKE_TEXT_MODE);
    int fill_flag = FXGETFLAG_COLORTYPE(alpha_flag) << 8 |
                    FXGETFLAG_ALPHA_STROKE(alpha_flag);
    if (!RenderRasterizer(rasterizer, stroke_color,
                          fill_mode & FXFILL_FULLCOVER, m_bGroupKnockout,
                          fill_flag, pIccTransform)) {
      return FALSE;
    }
  }
  return TRUE;
}
void RgbByteOrderSetPixel(CFX_DIBitmap* pBitmap, int x, int y, FX_DWORD argb) {
  if (x < 0 || x >= pBitmap->GetWidth() || y < 0 || y >= pBitmap->GetHeight()) {
    return;
  }
  uint8_t* pos = (uint8_t*)pBitmap->GetBuffer() + y * pBitmap->GetPitch() +
                 x * pBitmap->GetBPP() / 8;
  if (pBitmap->GetFormat() == FXDIB_Argb) {
    FXARGB_SETRGBORDERDIB(pos, ArgbGamma(argb));
  } else {
    int alpha = FXARGB_A(argb);
    pos[0] = (FXARGB_R(argb) * alpha + pos[0] * (255 - alpha)) / 255;
    pos[1] = (FXARGB_G(argb) * alpha + pos[1] * (255 - alpha)) / 255;
    pos[2] = (FXARGB_B(argb) * alpha + pos[2] * (255 - alpha)) / 255;
  }
}
void RgbByteOrderCompositeRect(CFX_DIBitmap* pBitmap,
                               int left,
                               int top,
                               int width,
                               int height,
                               FX_ARGB argb) {
  int src_alpha = FXARGB_A(argb);
  if (src_alpha == 0) {
    return;
  }
  FX_RECT rect(left, top, left + width, top + height);
  rect.Intersect(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight());
  width = rect.Width();
  int src_r = FXARGB_R(argb), src_g = FXARGB_G(argb), src_b = FXARGB_B(argb);
  int Bpp = pBitmap->GetBPP() / 8;
  FX_BOOL bAlpha = pBitmap->HasAlpha();
  int dib_argb = FXARGB_TOBGRORDERDIB(argb);
  uint8_t* pBuffer = pBitmap->GetBuffer();
  if (src_alpha == 255) {
    for (int row = rect.top; row < rect.bottom; row++) {
      uint8_t* dest_scan =
          pBuffer + row * pBitmap->GetPitch() + rect.left * Bpp;
      if (Bpp == 4) {
        FX_DWORD* scan = (FX_DWORD*)dest_scan;
        for (int col = 0; col < width; col++) {
          *scan++ = dib_argb;
        }
      } else {
        for (int col = 0; col < width; col++) {
          *dest_scan++ = src_r;
          *dest_scan++ = src_g;
          *dest_scan++ = src_b;
        }
      }
    }
    return;
  }
  src_r = FX_GAMMA(src_r);
  src_g = FX_GAMMA(src_g);
  src_b = FX_GAMMA(src_b);
  for (int row = rect.top; row < rect.bottom; row++) {
    uint8_t* dest_scan = pBuffer + row * pBitmap->GetPitch() + rect.left * Bpp;
    if (bAlpha) {
      for (int col = 0; col < width; col++) {
        uint8_t back_alpha = dest_scan[3];
        if (back_alpha == 0) {
          FXARGB_SETRGBORDERDIB(dest_scan,
                                FXARGB_MAKE(src_alpha, src_r, src_g, src_b));
          dest_scan += 4;
          continue;
        }
        uint8_t dest_alpha =
            back_alpha + src_alpha - back_alpha * src_alpha / 255;
        dest_scan[3] = dest_alpha;
        int alpha_ratio = src_alpha * 255 / dest_alpha;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_r, alpha_ratio);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_g, alpha_ratio);
        dest_scan++;
        *dest_scan = FXDIB_ALPHA_MERGE(*dest_scan, src_b, alpha_ratio);
        dest_scan += 2;
      }
    } else {
      for (int col = 0; col < width; col++) {
        *dest_scan = FX_GAMMA_INVERSE(
            FXDIB_ALPHA_MERGE(FX_GAMMA(*dest_scan), src_r, src_alpha));
        dest_scan++;
        *dest_scan = FX_GAMMA_INVERSE(
            FXDIB_ALPHA_MERGE(FX_GAMMA(*dest_scan), src_g, src_alpha));
        dest_scan++;
        *dest_scan = FX_GAMMA_INVERSE(
            FXDIB_ALPHA_MERGE(FX_GAMMA(*dest_scan), src_b, src_alpha));
        dest_scan++;
        if (Bpp == 4) {
          dest_scan++;
        }
      }
    }
  }
}
void RgbByteOrderTransferBitmap(CFX_DIBitmap* pBitmap,
                                int dest_left,
                                int dest_top,
                                int width,
                                int height,
                                const CFX_DIBSource* pSrcBitmap,
                                int src_left,
                                int src_top) {
  if (pBitmap == NULL) {
    return;
  }
  pBitmap->GetOverlapRect(dest_left, dest_top, width, height,
                          pSrcBitmap->GetWidth(), pSrcBitmap->GetHeight(),
                          src_left, src_top, NULL);
  if (width == 0 || height == 0) {
    return;
  }
  int Bpp = pBitmap->GetBPP() / 8;
  FXDIB_Format dest_format = pBitmap->GetFormat();
  FXDIB_Format src_format = pSrcBitmap->GetFormat();
  int pitch = pBitmap->GetPitch();
  uint8_t* buffer = pBitmap->GetBuffer();
  if (dest_format == src_format) {
    for (int row = 0; row < height; row++) {
      uint8_t* dest_scan = buffer + (dest_top + row) * pitch + dest_left * Bpp;
      uint8_t* src_scan =
          (uint8_t*)pSrcBitmap->GetScanline(src_top + row) + src_left * Bpp;
      if (Bpp == 4) {
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, FXARGB_MAKE(src_scan[3], src_scan[0],
                                               src_scan[1], src_scan[2]));
          dest_scan += 4;
          src_scan += 4;
        }
      } else {
        for (int col = 0; col < width; col++) {
          *dest_scan++ = src_scan[2];
          *dest_scan++ = src_scan[1];
          *dest_scan++ = src_scan[0];
          src_scan += 3;
        }
      }
    }
    return;
  }
  uint8_t* dest_buf = buffer + dest_top * pitch + dest_left * Bpp;
  if (dest_format == FXDIB_Rgb) {
    if (src_format == FXDIB_Rgb32) {
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = dest_buf + row * pitch;
        uint8_t* src_scan =
            (uint8_t*)pSrcBitmap->GetScanline(src_top + row) + src_left * 4;
        for (int col = 0; col < width; col++) {
          *dest_scan++ = src_scan[2];
          *dest_scan++ = src_scan[1];
          *dest_scan++ = src_scan[0];
          src_scan += 4;
        }
      }
    } else {
      ASSERT(FALSE);
    }
  } else if (dest_format == FXDIB_Argb || dest_format == FXDIB_Rgb32) {
    if (src_format == FXDIB_Rgb) {
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = (uint8_t*)(dest_buf + row * pitch);
        uint8_t* src_scan =
            (uint8_t*)pSrcBitmap->GetScanline(src_top + row) + src_left * 3;
        if (src_format == FXDIB_Argb) {
          for (int col = 0; col < width; col++) {
            FXARGB_SETDIB(dest_scan, FXARGB_MAKE(0xff, FX_GAMMA(src_scan[0]),
                                                 FX_GAMMA(src_scan[1]),
                                                 FX_GAMMA(src_scan[2])));
            dest_scan += 4;
            src_scan += 3;
          }
        } else {
          for (int col = 0; col < width; col++) {
            FXARGB_SETDIB(dest_scan, FXARGB_MAKE(0xff, src_scan[0], src_scan[1],
                                                 src_scan[2]));
            dest_scan += 4;
            src_scan += 3;
          }
        }
      }
    } else if (src_format == FXDIB_Rgb32) {
      ASSERT(dest_format == FXDIB_Argb);
      for (int row = 0; row < height; row++) {
        uint8_t* dest_scan = dest_buf + row * pitch;
        uint8_t* src_scan =
            (uint8_t*)(pSrcBitmap->GetScanline(src_top + row) + src_left * 4);
        for (int col = 0; col < width; col++) {
          FXARGB_SETDIB(dest_scan, FXARGB_MAKE(0xff, src_scan[0], src_scan[1],
                                               src_scan[2]));
          src_scan += 4;
          dest_scan += 4;
        }
      }
    }
  } else {
    ASSERT(FALSE);
  }
}
FX_ARGB _DefaultCMYK2ARGB(FX_CMYK cmyk, uint8_t alpha) {
  uint8_t r, g, b;
  AdobeCMYK_to_sRGB1(FXSYS_GetCValue(cmyk), FXSYS_GetMValue(cmyk),
                     FXSYS_GetYValue(cmyk), FXSYS_GetKValue(cmyk), r, g, b);
  return ArgbEncode(alpha, r, g, b);
}
FX_BOOL _DibSetPixel(CFX_DIBitmap* pDevice,
                     int x,
                     int y,
                     FX_DWORD color,
                     int alpha_flag,
                     void* pIccTransform) {
  FX_BOOL bObjCMYK = FXGETFLAG_COLORTYPE(alpha_flag);
  int alpha = bObjCMYK ? FXGETFLAG_ALPHA_FILL(alpha_flag) : FXARGB_A(color);
  if (pIccTransform) {
    ICodec_IccModule* pIccModule =
        CFX_GEModule::Get()->GetCodecModule()->GetIccModule();
    color = bObjCMYK ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
    pIccModule->TranslateScanline(pIccTransform, (uint8_t*)&color,
                                  (uint8_t*)&color, 1);
    color = bObjCMYK ? FXCMYK_TODIB(color) : FXARGB_TODIB(color);
    if (!pDevice->IsCmykImage()) {
      color = (color & 0xffffff) | (alpha << 24);
    }
  } else {
    if (pDevice->IsCmykImage()) {
      if (!bObjCMYK) {
        return FALSE;
      }
    } else {
      if (bObjCMYK) {
        color = _DefaultCMYK2ARGB(color, alpha);
      }
    }
  }
  pDevice->SetPixel(x, y, color);
  if (pDevice->m_pAlphaMask) {
    pDevice->m_pAlphaMask->SetPixel(x, y, alpha << 24);
  }
  return TRUE;
}
FX_BOOL CFX_AggDeviceDriver::SetPixel(int x,
                                      int y,
                                      FX_DWORD color,
                                      int alpha_flag,
                                      void* pIccTransform) {
  if (m_pBitmap->GetBuffer() == NULL) {
    return TRUE;
  }
  if (!CFX_GEModule::Get()->GetCodecModule() ||
      !CFX_GEModule::Get()->GetCodecModule()->GetIccModule()) {
    pIccTransform = NULL;
  }
  if (m_pClipRgn == NULL) {
    if (m_bRgbByteOrder) {
      RgbByteOrderSetPixel(m_pBitmap, x, y, color);
    } else {
      return _DibSetPixel(m_pBitmap, x, y, color, alpha_flag, pIccTransform);
    }
  } else if (m_pClipRgn->GetBox().Contains(x, y)) {
    if (m_pClipRgn->GetType() == CFX_ClipRgn::RectI) {
      if (m_bRgbByteOrder) {
        RgbByteOrderSetPixel(m_pBitmap, x, y, color);
      } else {
        return _DibSetPixel(m_pBitmap, x, y, color, alpha_flag, pIccTransform);
      }
    } else if (m_pClipRgn->GetType() == CFX_ClipRgn::MaskF) {
      const CFX_DIBitmap* pMask = m_pClipRgn->GetMask();
      FX_BOOL bCMYK = FXGETFLAG_COLORTYPE(alpha_flag);
      int new_alpha =
          bCMYK ? FXGETFLAG_ALPHA_FILL(alpha_flag) : FXARGB_A(color);
      new_alpha = new_alpha * pMask->GetScanline(y)[x] / 255;
      if (m_bRgbByteOrder) {
        RgbByteOrderSetPixel(m_pBitmap, x, y,
                             (color & 0xffffff) | (new_alpha << 24));
        return TRUE;
      }
      if (bCMYK) {
        FXSETFLAG_ALPHA_FILL(alpha_flag, new_alpha);
      } else {
        color = (color & 0xffffff) | (new_alpha << 24);
      }
      return _DibSetPixel(m_pBitmap, x, y, color, alpha_flag, pIccTransform);
    }
  }
  return TRUE;
}
FX_BOOL CFX_AggDeviceDriver::FillRect(const FX_RECT* pRect,
                                      FX_DWORD fill_color,
                                      int alpha_flag,
                                      void* pIccTransform,
                                      int blend_type) {
  if (blend_type != FXDIB_BLEND_NORMAL) {
    return FALSE;
  }
  if (m_pBitmap->GetBuffer() == NULL) {
    return TRUE;
  }
  FX_RECT clip_rect;
  GetClipBox(&clip_rect);
  FX_RECT draw_rect = clip_rect;
  if (pRect) {
    draw_rect.Intersect(*pRect);
  }
  if (draw_rect.IsEmpty()) {
    return TRUE;
  }
  if (m_pClipRgn == NULL || m_pClipRgn->GetType() == CFX_ClipRgn::RectI) {
    if (m_bRgbByteOrder) {
      RgbByteOrderCompositeRect(m_pBitmap, draw_rect.left, draw_rect.top,
                                draw_rect.Width(), draw_rect.Height(),
                                fill_color);
    } else {
      m_pBitmap->CompositeRect(draw_rect.left, draw_rect.top, draw_rect.Width(),
                               draw_rect.Height(), fill_color, alpha_flag,
                               pIccTransform);
    }
    return TRUE;
  }
  m_pBitmap->CompositeMask(
      draw_rect.left, draw_rect.top, draw_rect.Width(), draw_rect.Height(),
      (const CFX_DIBitmap*)m_pClipRgn->GetMask(), fill_color,
      draw_rect.left - clip_rect.left, draw_rect.top - clip_rect.top,
      FXDIB_BLEND_NORMAL, NULL, m_bRgbByteOrder, alpha_flag, pIccTransform);
  return TRUE;
}
FX_BOOL CFX_AggDeviceDriver::GetClipBox(FX_RECT* pRect) {
  if (m_pClipRgn == NULL) {
    pRect->left = pRect->top = 0;
    pRect->right = GetDeviceCaps(FXDC_PIXEL_WIDTH);
    pRect->bottom = GetDeviceCaps(FXDC_PIXEL_HEIGHT);
    return TRUE;
  }
  *pRect = m_pClipRgn->GetBox();
  return TRUE;
}
FX_BOOL CFX_AggDeviceDriver::GetDIBits(CFX_DIBitmap* pBitmap,
                                       int left,
                                       int top,
                                       void* pIccTransform,
                                       FX_BOOL bDEdge) {
  if (m_pBitmap->GetBuffer() == NULL) {
    return TRUE;
  }
  if (bDEdge) {
    if (m_bRgbByteOrder) {
      RgbByteOrderTransferBitmap(pBitmap, 0, 0, pBitmap->GetWidth(),
                                 pBitmap->GetHeight(), m_pBitmap, left, top);
    } else {
      return pBitmap->TransferBitmap(0, 0, pBitmap->GetWidth(),
                                     pBitmap->GetHeight(), m_pBitmap, left, top,
                                     pIccTransform);
    }
    return TRUE;
  }
  FX_RECT rect(left, top, left + pBitmap->GetWidth(),
               top + pBitmap->GetHeight());
  CFX_DIBitmap* pBack = NULL;
  if (m_pOriDevice) {
    pBack = m_pOriDevice->Clone(&rect);
    if (!pBack) {
      return TRUE;
    }
    pBack->CompositeBitmap(0, 0, pBack->GetWidth(), pBack->GetHeight(),
                           m_pBitmap, 0, 0);
  } else {
    pBack = m_pBitmap->Clone(&rect);
  }
  if (!pBack) {
    return TRUE;
  }
  FX_BOOL bRet = TRUE;
  left = left >= 0 ? 0 : left;
  top = top >= 0 ? 0 : top;
  if (m_bRgbByteOrder) {
    RgbByteOrderTransferBitmap(pBitmap, 0, 0, rect.Width(), rect.Height(),
                               pBack, left, top);
  } else {
    bRet = pBitmap->TransferBitmap(0, 0, rect.Width(), rect.Height(), pBack,
                                   left, top, pIccTransform);
  }
  delete pBack;
  return bRet;
}
FX_BOOL CFX_AggDeviceDriver::SetDIBits(const CFX_DIBSource* pBitmap,
                                       FX_DWORD argb,
                                       const FX_RECT* pSrcRect,
                                       int left,
                                       int top,
                                       int blend_type,
                                       int alpha_flag,
                                       void* pIccTransform) {
  if (m_pBitmap->GetBuffer() == NULL) {
    return TRUE;
  }
  if (pBitmap->IsAlphaMask())
    return m_pBitmap->CompositeMask(
        left, top, pSrcRect->Width(), pSrcRect->Height(), pBitmap, argb,
        pSrcRect->left, pSrcRect->top, blend_type, m_pClipRgn, m_bRgbByteOrder,
        alpha_flag, pIccTransform);
  return m_pBitmap->CompositeBitmap(
      left, top, pSrcRect->Width(), pSrcRect->Height(), pBitmap, pSrcRect->left,
      pSrcRect->top, blend_type, m_pClipRgn, m_bRgbByteOrder, pIccTransform);
}
FX_BOOL CFX_AggDeviceDriver::StretchDIBits(const CFX_DIBSource* pSource,
                                           FX_DWORD argb,
                                           int dest_left,
                                           int dest_top,
                                           int dest_width,
                                           int dest_height,
                                           const FX_RECT* pClipRect,
                                           FX_DWORD flags,
                                           int alpha_flag,
                                           void* pIccTransform,
                                           int blend_type) {
  if (m_pBitmap->GetBuffer() == NULL) {
    return TRUE;
  }
  if (dest_width == pSource->GetWidth() &&
      dest_height == pSource->GetHeight()) {
    FX_RECT rect(0, 0, dest_width, dest_height);
    return SetDIBits(pSource, argb, &rect, dest_left, dest_top, blend_type,
                     alpha_flag, pIccTransform);
  }
  FX_RECT dest_rect(dest_left, dest_top, dest_left + dest_width,
                    dest_top + dest_height);
  dest_rect.Normalize();
  FX_RECT dest_clip = dest_rect;
  dest_clip.Intersect(*pClipRect);
  CFX_BitmapComposer composer;
  composer.Compose(m_pBitmap, m_pClipRgn, 255, argb, dest_clip, FALSE, FALSE,
                   FALSE, m_bRgbByteOrder, alpha_flag, pIccTransform,
                   blend_type);
  dest_clip.Offset(-dest_rect.left, -dest_rect.top);
  CFX_ImageStretcher stretcher;
  if (stretcher.Start(&composer, pSource, dest_width, dest_height, dest_clip,
                      flags)) {
    stretcher.Continue(NULL);
  }
  return TRUE;
}
FX_BOOL CFX_AggDeviceDriver::StartDIBits(const CFX_DIBSource* pSource,
                                         int bitmap_alpha,
                                         FX_DWORD argb,
                                         const CFX_AffineMatrix* pMatrix,
                                         FX_DWORD render_flags,
                                         void*& handle,
                                         int alpha_flag,
                                         void* pIccTransform,
                                         int blend_type) {
  if (m_pBitmap->GetBuffer() == NULL) {
    return TRUE;
  }
  CFX_ImageRenderer* pRenderer = new CFX_ImageRenderer;
  pRenderer->Start(m_pBitmap, m_pClipRgn, pSource, bitmap_alpha, argb, pMatrix,
                   render_flags, m_bRgbByteOrder, alpha_flag, pIccTransform);
  handle = pRenderer;
  return TRUE;
}
FX_BOOL CFX_AggDeviceDriver::ContinueDIBits(void* pHandle, IFX_Pause* pPause) {
  if (m_pBitmap->GetBuffer() == NULL) {
    return TRUE;
  }
  return ((CFX_ImageRenderer*)pHandle)->Continue(pPause);
}
void CFX_AggDeviceDriver::CancelDIBits(void* pHandle) {
  if (m_pBitmap->GetBuffer() == NULL) {
    return;
  }
  delete (CFX_ImageRenderer*)pHandle;
}
CFX_FxgeDevice::CFX_FxgeDevice() {
  m_bOwnedBitmap = FALSE;
}
FX_BOOL CFX_FxgeDevice::Attach(CFX_DIBitmap* pBitmap,
                               int dither_bits,
                               FX_BOOL bRgbByteOrder,
                               CFX_DIBitmap* pOriDevice,
                               FX_BOOL bGroupKnockout) {
  if (pBitmap == NULL) {
    return FALSE;
  }
  SetBitmap(pBitmap);
  IFX_RenderDeviceDriver* pDriver = new CFX_AggDeviceDriver(
      pBitmap, dither_bits, bRgbByteOrder, pOriDevice, bGroupKnockout);
  SetDeviceDriver(pDriver);
  return TRUE;
}
FX_BOOL CFX_FxgeDevice::Create(int width,
                               int height,
                               FXDIB_Format format,
                               int dither_bits,
                               CFX_DIBitmap* pOriDevice) {
  m_bOwnedBitmap = TRUE;
  CFX_DIBitmap* pBitmap = new CFX_DIBitmap;
  if (!pBitmap->Create(width, height, format)) {
    delete pBitmap;
    return FALSE;
  }
  SetBitmap(pBitmap);
  IFX_RenderDeviceDriver* pDriver =
      new CFX_AggDeviceDriver(pBitmap, dither_bits, FALSE, pOriDevice, FALSE);
  SetDeviceDriver(pDriver);
  return TRUE;
}
CFX_FxgeDevice::~CFX_FxgeDevice() {
  if (m_bOwnedBitmap) {
    delete GetBitmap();
  }
}

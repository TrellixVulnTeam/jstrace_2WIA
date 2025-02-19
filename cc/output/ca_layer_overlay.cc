// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/ca_layer_overlay.h"

#include "base/metrics/histogram.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace cc {

namespace {

bool g_allow_rpdq_quad_conversion = false;

// This enum is used for histogram states and should only have new values added
// to the end before COUNT.
enum CALayerResult {
  CA_LAYER_SUCCESS = 0,
  CA_LAYER_FAILED_UNKNOWN,
  CA_LAYER_FAILED_IO_SURFACE_NOT_CANDIDATE,
  CA_LAYER_FAILED_STREAM_VIDEO_NOT_CANDIDATE,
  CA_LAYER_FAILED_STREAM_VIDEO_TRANSFORM,
  CA_LAYER_FAILED_TEXTURE_NOT_CANDIDATE,
  CA_LAYER_FAILED_TEXTURE_Y_FLIPPED,
  CA_LAYER_FAILED_TILE_NOT_CANDIDATE,
  CA_LAYER_FAILED_QUAD_BLEND_MODE,
  CA_LAYER_FAILED_QUAD_TRANSFORM,
  CA_LAYER_FAILED_QUAD_CLIPPING,
  CA_LAYER_FAILED_DEBUG_BORDER,
  CA_LAYER_FAILED_PICTURE_CONTENT,
  CA_LAYER_FAILED_RENDER_PASS,
  CA_LAYER_FAILED_SURFACE_CONTENT,
  CA_LAYER_FAILED_YUV_VIDEO_CONTENT,
  CA_LAYER_FAILED_DIFFERENT_CLIP_SETTINGS,
  CA_LAYER_FAILED_DIFFERENT_VERTEX_OPACITIES,
  CA_LAYER_FAILED_RENDER_PASS_FILTER_SCALE,
  CA_LAYER_FAILED_RENDER_PASS_BACKGROUND_FILTERS,
  CA_LAYER_FAILED_RENDER_PASS_MASK,
  CA_LAYER_FAILED_RENDER_PASS_FILTER_OPERATION,
  CA_LAYER_FAILED_COUNT,
};

bool FilterOperationSupported(const FilterOperation& operation) {
  switch (operation.type()) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
    case FilterOperation::INVERT:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
    case FilterOperation::BLUR:
    case FilterOperation::DROP_SHADOW:
      return true;
    default:
      return false;
  }
}

CALayerResult FromRenderPassQuad(ResourceProvider* resource_provider,
                                 const RenderPassDrawQuad* quad,
                                 CALayerOverlay* ca_layer_overlay) {
  if (quad->background_filters.size() != 0)
    return CA_LAYER_FAILED_RENDER_PASS_BACKGROUND_FILTERS;
  if (quad->mask_resource_id() != 0)
    return CA_LAYER_FAILED_RENDER_PASS_MASK;

  for (const FilterOperation& operation : quad->filters.operations()) {
    bool success = FilterOperationSupported(operation);
    if (!success)
      return CA_LAYER_FAILED_RENDER_PASS_FILTER_OPERATION;
  }

  if (quad->filters_scale != gfx::Vector2dF(1, 1)) {
    for (const FilterOperation& operation : quad->filters.operations()) {
      if (operation.type() == FilterOperation::BLUR ||
          operation.type() == FilterOperation::DROP_SHADOW) {
        return CA_LAYER_FAILED_RENDER_PASS_FILTER_SCALE;
      }
    }
  }

  ca_layer_overlay->contents_rect = gfx::RectF(0, 0, 1, 1);

  // TODO(erikchen): Enable this when RenderPassDrawQuad promotion to CALayer
  // is fully functional. https://crbug.com/581526.
  if (g_allow_rpdq_quad_conversion)
    return CA_LAYER_SUCCESS;
  return CA_LAYER_FAILED_RENDER_PASS;
}

CALayerResult FromStreamVideoQuad(ResourceProvider* resource_provider,
                                  const StreamVideoDrawQuad* quad,
                                  CALayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return CA_LAYER_FAILED_STREAM_VIDEO_NOT_CANDIDATE;
  ca_layer_overlay->contents_resource_id = resource_id;
  // TODO(ccameron): Support merging at least some basic transforms into the
  // layer transform.
  if (!quad->matrix.IsIdentity())
    return CA_LAYER_FAILED_STREAM_VIDEO_TRANSFORM;
  ca_layer_overlay->contents_rect = gfx::RectF(0, 0, 1, 1);
  return CA_LAYER_SUCCESS;
}

CALayerResult FromSolidColorDrawQuad(const SolidColorDrawQuad* quad,
                                     CALayerOverlay* ca_layer_overlay,
                                     bool* skip) {
  // Do not generate quads that are completely transparent.
  if (SkColorGetA(quad->color) == 0) {
    *skip = true;
    return CA_LAYER_SUCCESS;
  }
  ca_layer_overlay->background_color = quad->color;
  return CA_LAYER_SUCCESS;
}

CALayerResult FromTextureQuad(ResourceProvider* resource_provider,
                              const TextureDrawQuad* quad,
                              CALayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return CA_LAYER_FAILED_TEXTURE_NOT_CANDIDATE;
  if (quad->y_flipped) {
    // The anchor point is at the bottom-left corner of the CALayer. The
    // transformation that flips the contents of the layer without changing its
    // frame is the composition of a vertical flip about the anchor point, and a
    // translation by the height of the layer.
    ca_layer_overlay->shared_state->transform.preTranslate(
        0, ca_layer_overlay->bounds_rect.height(), 0);
    ca_layer_overlay->shared_state->transform.preScale(1, -1, 1);
  }
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect =
      BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
  ca_layer_overlay->background_color = quad->background_color;
  for (int i = 1; i < 4; ++i) {
    if (quad->vertex_opacity[i] != quad->vertex_opacity[0])
      return CA_LAYER_FAILED_DIFFERENT_VERTEX_OPACITIES;
  }
  ca_layer_overlay->shared_state->opacity *= quad->vertex_opacity[0];
  ca_layer_overlay->filter = quad->nearest_neighbor ? GL_NEAREST : GL_LINEAR;
  return CA_LAYER_SUCCESS;
}

CALayerResult FromTileQuad(ResourceProvider* resource_provider,
                           const TileDrawQuad* quad,
                           CALayerOverlay* ca_layer_overlay) {
  unsigned resource_id = quad->resource_id();
  if (!resource_provider->IsOverlayCandidate(resource_id))
    return CA_LAYER_FAILED_TILE_NOT_CANDIDATE;
  ca_layer_overlay->contents_resource_id = resource_id;
  ca_layer_overlay->contents_rect = quad->tex_coord_rect;
  ca_layer_overlay->contents_rect.Scale(1.f / quad->texture_size.width(),
                                        1.f / quad->texture_size.height());
  return CA_LAYER_SUCCESS;
}

class CALayerOverlayProcessor {
 public:
  CALayerResult FromDrawQuad(ResourceProvider* resource_provider,
                             const gfx::RectF& display_rect,
                             const DrawQuad* quad,
                             CALayerOverlay* ca_layer_overlay,
                             bool* skip) {
    if (quad->shared_quad_state->blend_mode != SkXfermode::kSrcOver_Mode)
      return CA_LAYER_FAILED_QUAD_BLEND_MODE;

    // Early-out for invisible quads.
    if (quad->shared_quad_state->opacity == 0.f) {
      *skip = true;
      return CA_LAYER_SUCCESS;
    }

    // Enable edge anti-aliasing only on layer boundaries.
    ca_layer_overlay->edge_aa_mask = 0;
    if (quad->IsLeftEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_LEFT_CHROMIUM;
    if (quad->IsRightEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_RIGHT_CHROMIUM;
    if (quad->IsBottomEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_BOTTOM_CHROMIUM;
    if (quad->IsTopEdge())
      ca_layer_overlay->edge_aa_mask |= GL_CA_LAYER_EDGE_TOP_CHROMIUM;

    if (most_recent_shared_quad_state_ != quad->shared_quad_state) {
      most_recent_shared_quad_state_ = quad->shared_quad_state;
      most_recent_overlay_shared_state_ = new CALayerOverlaySharedState;
      // Set rect clipping and sorting context ID.
      most_recent_overlay_shared_state_->sorting_context_id =
          quad->shared_quad_state->sorting_context_id;
      most_recent_overlay_shared_state_->is_clipped =
          quad->shared_quad_state->is_clipped;
      most_recent_overlay_shared_state_->clip_rect =
          gfx::RectF(quad->shared_quad_state->clip_rect);

      most_recent_overlay_shared_state_->opacity =
          quad->shared_quad_state->opacity;
      most_recent_overlay_shared_state_->transform =
          quad->shared_quad_state->quad_to_target_transform.matrix();
    }
    ca_layer_overlay->shared_state = most_recent_overlay_shared_state_;

    ca_layer_overlay->bounds_rect = gfx::RectF(quad->rect);

    switch (quad->material) {
      case DrawQuad::TEXTURE_CONTENT:
        return FromTextureQuad(resource_provider,
                               TextureDrawQuad::MaterialCast(quad),
                               ca_layer_overlay);
      case DrawQuad::TILED_CONTENT:
        return FromTileQuad(resource_provider, TileDrawQuad::MaterialCast(quad),
                            ca_layer_overlay);
      case DrawQuad::SOLID_COLOR:
        return FromSolidColorDrawQuad(SolidColorDrawQuad::MaterialCast(quad),
                                      ca_layer_overlay, skip);
      case DrawQuad::STREAM_VIDEO_CONTENT:
        return FromStreamVideoQuad(resource_provider,
                                   StreamVideoDrawQuad::MaterialCast(quad),
                                   ca_layer_overlay);
      case DrawQuad::DEBUG_BORDER:
        return CA_LAYER_FAILED_DEBUG_BORDER;
      case DrawQuad::PICTURE_CONTENT:
        return CA_LAYER_FAILED_PICTURE_CONTENT;
      case DrawQuad::RENDER_PASS:
        return FromRenderPassQuad(resource_provider,
                                  RenderPassDrawQuad::MaterialCast(quad),
                                  ca_layer_overlay);
      case DrawQuad::SURFACE_CONTENT:
        return CA_LAYER_FAILED_SURFACE_CONTENT;
      case DrawQuad::YUV_VIDEO_CONTENT:
        return CA_LAYER_FAILED_YUV_VIDEO_CONTENT;
      default:
        break;
    }

    return CA_LAYER_FAILED_UNKNOWN;
  }

 private:
  const SharedQuadState* most_recent_shared_quad_state_ = nullptr;
  scoped_refptr<CALayerOverlaySharedState> most_recent_overlay_shared_state_;
};

}  // namespace

CALayerOverlay::CALayerOverlay() : filter(GL_LINEAR) {}

CALayerOverlay::CALayerOverlay(const CALayerOverlay& other) = default;

CALayerOverlay::~CALayerOverlay() {}

bool ProcessForCALayerOverlays(ResourceProvider* resource_provider,
                               const gfx::RectF& display_rect,
                               const QuadList& quad_list,
                               CALayerOverlayList* ca_layer_overlays) {
  CALayerResult result = CA_LAYER_SUCCESS;
  ca_layer_overlays->reserve(quad_list.size());

  CALayerOverlayProcessor processor;
  for (auto it = quad_list.BackToFrontBegin(); it != quad_list.BackToFrontEnd();
       ++it) {
    const DrawQuad* quad = *it;
    CALayerOverlay ca_layer;
    bool skip = false;
    result = processor.FromDrawQuad(resource_provider, display_rect, quad,
                                    &ca_layer, &skip);
    if (result != CA_LAYER_SUCCESS)
      break;

    if (skip)
      continue;

    // It is not possible to correctly represent two different clipping settings
    // within one sorting context.
    if (!ca_layer_overlays->empty()) {
      const CALayerOverlay& previous_ca_layer = ca_layer_overlays->back();
      if (ca_layer.shared_state->sorting_context_id &&
          previous_ca_layer.shared_state->sorting_context_id ==
              ca_layer.shared_state->sorting_context_id) {
        if (previous_ca_layer.shared_state->is_clipped !=
                ca_layer.shared_state->is_clipped ||
            previous_ca_layer.shared_state->clip_rect !=
                ca_layer.shared_state->clip_rect) {
          result = CA_LAYER_FAILED_DIFFERENT_CLIP_SETTINGS;
          break;
        }
      }
    }

    ca_layer_overlays->push_back(ca_layer);
  }

  UMA_HISTOGRAM_ENUMERATION("Compositing.Renderer.CALayerResult", result,
                            CA_LAYER_FAILED_COUNT);

  if (result != CA_LAYER_SUCCESS) {
    ca_layer_overlays->clear();
    return false;
  }
  return true;
}

void EnableRenderPassDrawQuadForTesting() {
  g_allow_rpdq_quad_conversion = true;
}

}  // namespace cc

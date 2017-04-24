/*
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef DRV_MEDIATEK

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <mediatek_drm.h>

#include "drv_priv.h"
#include "helpers.h"
#include "util.h"

static struct supported_combination combos[6] = {
	{DRM_FORMAT_ABGR8888, DRM_FORMAT_MOD_NONE,
		BO_USE_LINEAR | BO_USE_RENDERING | BO_USE_SW_READ_OFTEN | BO_USE_SW_WRITE_OFTEN |
		BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
	{DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_NONE,
		BO_USE_CURSOR | BO_USE_LINEAR | BO_USE_RENDERING | BO_USE_SW_READ_OFTEN |
		BO_USE_SW_WRITE_OFTEN | BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
	{DRM_FORMAT_RGB565, DRM_FORMAT_MOD_NONE,
		BO_USE_RENDERING | BO_USE_SW_READ_OFTEN | BO_USE_SW_WRITE_OFTEN |
		BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
	{DRM_FORMAT_XBGR8888, DRM_FORMAT_MOD_NONE,
		BO_USE_RENDERING | BO_USE_SW_READ_OFTEN | BO_USE_SW_WRITE_OFTEN |
		BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
	{DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_NONE,
		BO_USE_CURSOR | BO_USE_LINEAR | BO_USE_RENDERING | BO_USE_SW_READ_OFTEN |
		BO_USE_SW_WRITE_OFTEN | BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
	{DRM_FORMAT_YVU420, DRM_FORMAT_MOD_NONE,
		BO_USE_LINEAR | BO_USE_RENDERING | BO_USE_SW_READ_OFTEN | BO_USE_SW_WRITE_OFTEN |
		BO_USE_SW_READ_RARELY | BO_USE_SW_WRITE_RARELY},
};

static int mediatek_init(struct driver *drv)
{
	drv_insert_combinations(drv, combos, ARRAY_SIZE(combos));
	return drv_add_kms_flags(drv);
}

static int mediatek_bo_create(struct bo *bo, uint32_t width, uint32_t height,
			      uint32_t format, uint32_t flags)
{
	int ret;
	size_t plane;
	struct drm_mtk_gem_create gem_create;

	drv_bo_from_format(bo, width, height, format);

	memset(&gem_create, 0, sizeof(gem_create));
	gem_create.size = bo->total_size;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MTK_GEM_CREATE, &gem_create);
	if (ret) {
		fprintf(stderr, "drv: DRM_IOCTL_MTK_GEM_CREATE failed "
				"(size=%llu)\n", gem_create.size);
		return ret;
	}

	for (plane = 0; plane < bo->num_planes; plane++)
		bo->handles[plane].u32 = gem_create.handle;

	return 0;
}

static void *mediatek_bo_map(struct bo *bo, struct map_info *data, size_t plane)
{
	int ret;
	struct drm_mtk_gem_map_off gem_map;

	memset(&gem_map, 0, sizeof(gem_map));
	gem_map.handle = bo->handles[0].u32;

	ret = drmIoctl(bo->drv->fd, DRM_IOCTL_MTK_GEM_MAP_OFFSET, &gem_map);
	if (ret) {
		fprintf(stderr,"drv: DRM_IOCTL_MTK_GEM_MAP_OFFSET failed\n");
		return MAP_FAILED;
	}

	data->length = bo->total_size;

	return mmap(0, bo->total_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		    bo->drv->fd, gem_map.offset);
}

static uint32_t mediatek_resolve_format(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_FLEX_IMPLEMENTATION_DEFINED:
		/*HACK: See b/28671744 */
		return DRM_FORMAT_XBGR8888;
	case DRM_FORMAT_FLEX_YCbCr_420_888:
		return DRM_FORMAT_YVU420;
	default:
		return format;
	}
}

struct backend backend_mediatek =
{
	.name = "mediatek",
	.init = mediatek_init,
	.bo_create = mediatek_bo_create,
	.bo_destroy = drv_gem_bo_destroy,
	.bo_import = drv_prime_bo_import,
	.bo_map = mediatek_bo_map,
	.resolve_format = mediatek_resolve_format,
};

#endif

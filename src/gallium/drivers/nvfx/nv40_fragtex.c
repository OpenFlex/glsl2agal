#include "util/u_format.h"
#include "nvfx_context.h"
#include "nvfx_tex.h"

void
nv40_sampler_state_init(struct pipe_context *pipe,
			  struct nvfx_sampler_state *ps,
			  const struct pipe_sampler_state *cso)
{
	if (cso->max_anisotropy >= 2) {
		/* no idea, binary driver sets it, works without it.. meh.. */
		ps->wrap |= (1 << 5);

		if (cso->max_anisotropy >= 16) {
			ps->en |= NV40TCL_TEX_ENABLE_ANISO_16X;
		} else
		if (cso->max_anisotropy >= 12) {
			ps->en |= NV40TCL_TEX_ENABLE_ANISO_12X;
		} else
		if (cso->max_anisotropy >= 10) {
			ps->en |= NV40TCL_TEX_ENABLE_ANISO_10X;
		} else
		if (cso->max_anisotropy >= 8) {
			ps->en |= NV40TCL_TEX_ENABLE_ANISO_8X;
		} else
		if (cso->max_anisotropy >= 6) {
			ps->en |= NV40TCL_TEX_ENABLE_ANISO_6X;
		} else
		if (cso->max_anisotropy >= 4) {
			ps->en |= NV40TCL_TEX_ENABLE_ANISO_4X;
		} else {
			ps->en |= NV40TCL_TEX_ENABLE_ANISO_2X;
		}
	}

	{
		float limit;

		limit = CLAMP(cso->lod_bias, -16.0, 15.0);
		ps->filt |= (int)(cso->lod_bias * 256.0) & 0x1fff;

		limit = CLAMP(cso->max_lod, 0.0, 15.0);
		ps->en |= (int)(limit * 256.0) << 7;

		limit = CLAMP(cso->min_lod, 0.0, 15.0);
		ps->en |= (int)(limit * 256.0) << 19;
	}
}

#define _(m,tf,ts0x,ts0y,ts0z,ts0w,ts1x,ts1y,ts1z,ts1w,sx,sy,sz,sw)            \
{                                                                              \
  TRUE,                                                                        \
  PIPE_FORMAT_##m,                                                             \
  NV40TCL_TEX_FORMAT_FORMAT_##tf,                                              \
  (NV34TCL_TX_SWIZZLE_S0_X_##ts0x | NV34TCL_TX_SWIZZLE_S0_Y_##ts0y |         \
   NV34TCL_TX_SWIZZLE_S0_Z_##ts0z | NV34TCL_TX_SWIZZLE_S0_W_##ts0w |         \
   NV34TCL_TX_SWIZZLE_S1_X_##ts1x | NV34TCL_TX_SWIZZLE_S1_Y_##ts1y |         \
   NV34TCL_TX_SWIZZLE_S1_Z_##ts1z | NV34TCL_TX_SWIZZLE_S1_W_##ts1w),         \
  ((NV34TCL_TX_FILTER_SIGNED_RED*sx) | (NV34TCL_TX_FILTER_SIGNED_GREEN*sy) |       \
   (NV34TCL_TX_FILTER_SIGNED_BLUE*sz) | (NV34TCL_TX_FILTER_SIGNED_ALPHA*sw))       \
}

struct nv40_texture_format {
	boolean defined;
	uint	pipe;
	int     format;
	int     swizzle;
	int     sign;
};

static struct nv40_texture_format
nv40_texture_formats[] = {
	_(B8G8R8X8_UNORM, A8R8G8B8,   S1,   S1,   S1,  ONE, X, Y, Z, W, 0, 0, 0, 0),
	_(B8G8R8A8_UNORM, A8R8G8B8,   S1,   S1,   S1,   S1, X, Y, Z, W, 0, 0, 0, 0),
	_(B5G5R5A1_UNORM, A1R5G5B5,   S1,   S1,   S1,   S1, X, Y, Z, W, 0, 0, 0, 0),
	_(B4G4R4A4_UNORM, A4R4G4B4,   S1,   S1,   S1,   S1, X, Y, Z, W, 0, 0, 0, 0),
	_(B5G6R5_UNORM  , R5G6B5  ,   S1,   S1,   S1,  ONE, X, Y, Z, W, 0, 0, 0, 0),
	_(L8_UNORM      , L8      ,   S1,   S1,   S1,  ONE, X, X, X, X, 0, 0, 0, 0),
	_(A8_UNORM      , L8      , ZERO, ZERO, ZERO,   S1, X, X, X, X, 0, 0, 0, 0),
	_(R16_SNORM     , A16     , ZERO, ZERO,   S1,  ONE, X, X, X, Y, 1, 1, 1, 1),
	_(I8_UNORM      , L8      ,   S1,   S1,   S1,   S1, X, X, X, X, 0, 0, 0, 0),
	_(L8A8_UNORM    , A8L8    ,   S1,   S1,   S1,   S1, X, X, X, Y, 0, 0, 0, 0),
	_(Z16_UNORM     , Z16     ,   S1,   S1,   S1,  ONE, X, X, X, X, 0, 0, 0, 0),
	_(S8Z24_UNORM   , Z24     ,   S1,   S1,   S1,  ONE, X, X, X, X, 0, 0, 0, 0),
	_(DXT1_RGB      , DXT1    ,   S1,   S1,   S1,  ONE, X, Y, Z, W, 0, 0, 0, 0),
	_(DXT1_RGBA     , DXT1    ,   S1,   S1,   S1,   S1, X, Y, Z, W, 0, 0, 0, 0),
	_(DXT3_RGBA     , DXT3    ,   S1,   S1,   S1,   S1, X, Y, Z, W, 0, 0, 0, 0),
	_(DXT5_RGBA     , DXT5    ,   S1,   S1,   S1,   S1, X, Y, Z, W, 0, 0, 0, 0),
	{},
};

static struct nv40_texture_format *
nv40_fragtex_format(uint pipe_format)
{
	struct nv40_texture_format *tf = nv40_texture_formats;

	while (tf->defined) {
		if (tf->pipe == pipe_format)
			return tf;
		tf++;
	}

	NOUVEAU_ERR("unknown texture format %s\n", util_format_name(pipe_format));
	return NULL;
}


struct nouveau_stateobj *
nv40_fragtex_build(struct nvfx_context *nvfx, int unit)
{
	struct nvfx_sampler_state *ps = nvfx->tex_sampler[unit];
	struct nvfx_miptree *nv40mt = nvfx->tex_miptree[unit];
	struct nouveau_bo *bo = nouveau_bo(nv40mt->buffer);
	struct pipe_texture *pt = &nv40mt->base;
	struct nv40_texture_format *tf;
	struct nouveau_stateobj *so;
	uint32_t txf, txs, txp;
	unsigned tex_flags = NOUVEAU_BO_VRAM | NOUVEAU_BO_GART | NOUVEAU_BO_RD;

	tf = nv40_fragtex_format(pt->format);
	if (!tf)
		assert(0);

	txf  = ps->fmt;
	txf |= tf->format | 0x8000;
	txf |= ((pt->last_level + 1) << NV40TCL_TEX_FORMAT_MIPMAP_COUNT_SHIFT);

	if (1) /* XXX */
		txf |= NV34TCL_TX_FORMAT_NO_BORDER;

	switch (pt->target) {
	case PIPE_TEXTURE_CUBE:
		txf |= NV34TCL_TX_FORMAT_CUBIC;
		/* fall-through */
	case PIPE_TEXTURE_2D:
		txf |= NV34TCL_TX_FORMAT_DIMS_2D;
		break;
	case PIPE_TEXTURE_3D:
		txf |= NV34TCL_TX_FORMAT_DIMS_3D;
		break;
	case PIPE_TEXTURE_1D:
		txf |= NV34TCL_TX_FORMAT_DIMS_1D;
		break;
	default:
		NOUVEAU_ERR("Unknown target %d\n", pt->target);
		return NULL;
	}

	if (!(pt->tex_usage & NOUVEAU_TEXTURE_USAGE_LINEAR)) {
		txp = 0;
	} else {
		txp  = nv40mt->level[0].pitch;
		txf |= NV40TCL_TEX_FORMAT_LINEAR;
	}

	txs = tf->swizzle;

	so = so_new(2, 9, 2);
	so_method(so, nvfx->screen->eng3d, NV34TCL_TX_OFFSET(unit), 8);
	so_reloc (so, bo, 0, tex_flags | NOUVEAU_BO_LOW, 0, 0);
	so_reloc (so, bo, txf, tex_flags | NOUVEAU_BO_OR,
		      NV34TCL_TX_FORMAT_DMA0, NV34TCL_TX_FORMAT_DMA1);
	so_data  (so, ps->wrap);
	so_data  (so, NV40TCL_TEX_ENABLE_ENABLE | ps->en);
	so_data  (so, txs);
	so_data  (so, ps->filt | tf->sign | 0x2000 /*voodoo*/);
	so_data  (so, (pt->width0 << NV34TCL_TX_NPOT_SIZE_W_SHIFT) |
		       pt->height0);
	so_data  (so, ps->bcol);
	so_method(so, nvfx->screen->eng3d, NV40TCL_TEX_SIZE1(unit), 1);
	so_data  (so, (pt->depth0 << NV40TCL_TEX_SIZE1_DEPTH_SHIFT) | txp);

	return so;
}
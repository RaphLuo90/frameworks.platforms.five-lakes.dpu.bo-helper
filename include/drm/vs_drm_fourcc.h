/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

#ifndef __VS_DRM_FOURCC_H__
#define __VS_DRM_FOURCC_H__

#include <drm/drm_fourcc.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define DRM_FORMAT_MOD_VENDOR_VS 0x0b

/*
 * VeriSilicon Compressed Format
 *
 * DEC(Decompressor-Compressor) format consists of category (3 bits),
 * tile mode (6 bits) and align mode (2 bits).
 *
 * PVRIC(PowerVR Image Compression) format consists of category (3 bits),
 * tile mode (6 bits) and lossless/lossy mode (2 bits).
 *
 * DECNano(Decompressor-Compressor Nano) format consists of category (3 bits),
 * tile mode (6 bits) and decompression mode (2 bits).
 *
 * ETC2(Ericsson Texture Compression 2) format consists of category (3 bits) and
 * tile mode (6 bits).
 *
 */

#define DRM_FORMAT_MOD_VS_TYPE_NORMAL 0x00
#define DRM_FORMAT_MOD_VS_TYPE_COMPRESSED 0x01
#define DRM_FORMAT_MOD_VS_TYPE_PVRIC 0x02
#define DRM_FORMAT_MOD_VS_TYPE_DECNANO 0x03
#define DRM_FORMAT_MOD_VS_TYPE_ETC2 0x04
#define DRM_FORMAT_MOD_VS_TYPE_DEC400A 0x05

#define DRM_FORMAT_MOD_VS_TYPE_MASK ((__u64)0x07 << 53)

#define fourcc_mod_vs_code(type, val) \
	fourcc_mod_code(VS, (((__u64)type) << 53) | ((val)&0x001fffffffffffffULL))

#define DRM_FORMAT_MOD_VS_DEC_TILE_MODE_MASK 0x3F
#define DRM_FORMAT_MOD_VS_DEC_TILE_8X8_XMAJOR 0x00
#define DRM_FORMAT_MOD_VS_DEC_TILE_8X8_YMAJOR 0x01
#define DRM_FORMAT_MOD_VS_DEC_TILE_16X4 0x02
#define DRM_FORMAT_MOD_VS_DEC_TILE_8X4 0x03
#define DRM_FORMAT_MOD_VS_DEC_TILE_4X8 0x04
#define DRM_FORMAT_MOD_VS_DEC_RASTER_16X4 0x06
#define DRM_FORMAT_MOD_VS_DEC_TILE_64X4 0x07
#define DRM_FORMAT_MOD_VS_DEC_TILE_32X4 0x08
#define DRM_FORMAT_MOD_VS_DEC_RASTER_256X1 0x09
#define DRM_FORMAT_MOD_VS_DEC_RASTER_128X1 0x0A
#define DRM_FORMAT_MOD_VS_DEC_RASTER_64X4 0x0B
#define DRM_FORMAT_MOD_VS_DEC_RASTER_256X2 0x0C
#define DRM_FORMAT_MOD_VS_DEC_RASTER_128X2 0x0D
#define DRM_FORMAT_MOD_VS_DEC_RASTER_128X4 0x0E
#define DRM_FORMAT_MOD_VS_DEC_RASTER_64X1 0x0F
#define DRM_FORMAT_MOD_VS_DEC_TILE_16X8 0x10
#define DRM_FORMAT_MOD_VS_DEC_TILE_8X16 0x11
#define DRM_FORMAT_MOD_VS_DEC_RASTER_512X1 0x12
#define DRM_FORMAT_MOD_VS_DEC_RASTER_32X4 0x13
#define DRM_FORMAT_MOD_VS_DEC_RASTER_64X2 0x14
#define DRM_FORMAT_MOD_VS_DEC_RASTER_32X2 0x15
#define DRM_FORMAT_MOD_VS_DEC_RASTER_32X1 0x16
#define DRM_FORMAT_MOD_VS_DEC_RASTER_16X1 0x17
#define DRM_FORMAT_MOD_VS_DEC_TILE_128X4 0x18
#define DRM_FORMAT_MOD_VS_DEC_TILE_256X4 0x19
#define DRM_FORMAT_MOD_VS_DEC_TILE_512X4 0x1A
#define DRM_FORMAT_MOD_VS_DEC_TILE_16X16 0x1B
#define DRM_FORMAT_MOD_VS_DEC_TILE_32X16 0x1C
#define DRM_FORMAT_MOD_VS_DEC_TILE_64X16 0x1D
#define DRM_FORMAT_MOD_VS_DEC_TILE_128X8 0x1E
#define DRM_FORMAT_MOD_VS_DEC_TILE_8X4_S 0x1F
#define DRM_FORMAT_MOD_VS_DEC_TILE_16X4_S 0x20
#define DRM_FORMAT_MOD_VS_DEC_TILE_32X4_S 0x21
#define DRM_FORMAT_MOD_VS_DEC_TILE_16X4_LSB 0x22
#define DRM_FORMAT_MOD_VS_DEC_TILE_32X4_LSB 0x23
#define DRM_FORMAT_MOD_VS_DEC_TILE_32X8 0x24
#define DRM_FORMAT_MOD_VS_DEC_TILE_8X8 0x30
#define DRM_FORMAT_MOD_VS_DEC_TILE_32X2 0x31
#define DRM_FORMAT_MOD_VS_DEC_LINEAR 0x32
#define DRM_FORMAT_MOD_VS_DEC_TILE_4X4 0x33
#define DRM_FORMAT_MOD_VS_DEC_TILE_8X8_UNIT2X2 0x34
#define DRM_FORMAT_MOD_VS_DEC_TILE_8X4_UNIT2X2 0x35
#define DRM_FORMAT_MOD_VS_DEC_TILE_8X8_SUPERTILE_X 0x36
#define DRM_FORMAT_MOD_VS_DEC_TILE_32X8_YUVSP8X8 0x37
#define DRM_FORMAT_MOD_VS_DEC_TILE_32X4_YUVSP8X8 0x38
#define DRM_FORMAT_MOD_VS_DEC_TILE_16X8_YUVSP8X8 0x39
#define DRM_FORMAT_MOD_VS_DEC_TILE_16X4_YUVSP8X8 0x3A
#define DRM_FORMAT_MOD_VS_DEC_TILE_32X8_A 0x3B

#define DRM_FORMAT_MOD_VS_DEC_ALIGN_SHIFT 6
#define DRM_FORMAT_MOD_VS_DEC_ALIGN_MASK ((__u64)0x03 << 6)
#define DRM_FORMAT_MOD_VS_DEC_ALIGN_32 0x00
#define DRM_FORMAT_MOD_VS_DEC_ALIGN_64 0x01

#define DRM_FORMAT_MOD_VS_DEC_LOSSY (0x01 << 8)
#define DRM_FORMAT_MOD_VS_DEC_LOSSLESS (0x01 << 9)

#define DRM_FORMAT_MOD_VS_DECNANO_NON_SAMPLE (0x01 << 10)
#define DRM_FORMAT_MOD_VS_DECNANO_H_SAMPLE (0x02 << 10)
#define DRM_FORMAT_MOD_VS_DECNANO_HV_SAMPLE (0x03 << 10)

#define DRM_FORMAT_MOD_VS_DEC_YUV_TRANSFORM_SHIFT 12
#define DRM_FORMAT_MOD_VS_DEC_YUV_TRANSFORM_MASK (0x01 << 12)
#define DRM_FORMAT_MOD_VS_DEC_YUV_TRANSFORM_DISABLE 0x00
#define DRM_FORMAT_MOD_VS_DEC_YUV_TRANSFORM_ENABLE 0x01

/* custom format modifier, which can be combined with all of the color
 * buffer tiling modifiers defined above (compression or non-compression) */
#define DRM_FORMAT_MOD_VS_CUSTOM_FORMAT_SHIFT 13
#define DRM_FORMAT_MOD_VS_CUSTOM_FORMAT_MASK (0x01 << 13)
#define DRM_FORMAT_MOD_VS_CUSTOM_FORMAT_DISABLE (0x00 << 13)
#define DRM_FORMAT_MOD_VS_CUSTOM_FORMAT_ENABLE (0x01 << 13)
#define fourcc_mod_is_custom_format(modifier) ((modifier)&DRM_FORMAT_MOD_VS_CUSTOM_FORMAT_MASK)

#define DRM_FORMAT_MOD_VS_DEC_FC_SHIFT 14
#define DRM_FORMAT_MOD_VS_DEC_FC_MASK (0x01 << 14)
#define DRM_FORMAT_MOD_VS_DEC_FC_DISABLE (0x00 << 14)
#define DRM_FORMAT_MOD_VS_DEC_FC_ENABLE (0x01 << 14)
#define dec_mod_is_fc(modifier) ((modifier)&DRM_FORMAT_MOD_VS_DEC_FC_MASK)

#define DRM_FORMAT_MOD_VS_DEC_FC_SIZE__SHIFT 15
#define DRM_FORMAT_MOD_VS_DEC_FC_SIZE_MASK (0x01 << 15)
#define DRM_FORMAT_MOD_VS_DEC_FC_SIZE_128 (0x00 << 15)
#define DRM_FORMAT_MOD_VS_DEC_FC_SIZE_256 (0x01 << 15)
#define dec_mod_get_fc_size(modifier) (((modifier)&DRM_FORMAT_MOD_VS_DEC_FC_SIZE_MASK) >> 15)

#define fourcc_mod_vs_dec_code(tile, align)                   \
	fourcc_mod_vs_code(DRM_FORMAT_MOD_VS_TYPE_COMPRESSED, \
			   ((tile) | (align << DRM_FORMAT_MOD_VS_DEC_ALIGN_SHIFT)))

#define fourcc_mod_vs_dec400a_code(tile, yuv_transform)    \
	fourcc_mod_vs_code(DRM_FORMAT_MOD_VS_TYPE_DEC400A, \
			   ((tile) |                       \
			    (yuv_transform << DRM_FORMAT_MOD_VS_DEC_YUV_TRANSFORM_SHIFT)))

#define fourcc_mod_vs_pvric_code(tile, lossy) \
	fourcc_mod_vs_code(DRM_FORMAT_MOD_VS_TYPE_PVRIC, ((tile) | (lossy)))

#define fourcc_mod_vs_decnano_code(tile, mode) \
	fourcc_mod_vs_code(DRM_FORMAT_MOD_VS_TYPE_DECNANO, ((tile) | (mode)))

#define fourcc_mod_vs_etc2_code(tile) fourcc_mod_vs_code(DRM_FORMAT_MOD_VS_TYPE_ETC2, tile)

#define DRM_FORMAT_MOD_VS_NORM_MODE_MASK 0x3F
#define DRM_FORMAT_MOD_VS_LINEAR 0x00
#define DRM_FORMAT_MOD_VS_TILED4x4 0x01
#define DRM_FORMAT_MOD_VS_SUPER_TILED_XMAJOR 0x02
#define DRM_FORMAT_MOD_VS_SUPER_TILED_YMAJOR 0x03
#define DRM_FORMAT_MOD_VS_TILE_8X8 0x04
#define DRM_FORMAT_MOD_VS_TILE_8X4 0x07
#define DRM_FORMAT_MOD_VS_SUPER_TILED_XMAJOR_8X4 0x0B
#define DRM_FORMAT_MOD_VS_SUPER_TILED_YMAJOR_4X8 0x0C
#define DRM_FORMAT_MOD_VS_TILE_Y 0x0D
#define DRM_FORMAT_MOD_VS_TILE_128X1 0x0F
#define DRM_FORMAT_MOD_VS_TILE_256X1 0x10
#define DRM_FORMAT_MOD_VS_TILE_32X1 0x11
#define DRM_FORMAT_MOD_VS_TILE_64X1 0x12
#define DRM_FORMAT_MOD_VS_TILE_MODE4X4 0x15
#define DRM_FORMAT_MOD_VS_TILE_32X2 0x20
#define DRM_FORMAT_MOD_VS_TILE_16X4 0x21
#define DRM_FORMAT_MOD_VS_TILE_32X4 0x22
#define DRM_FORMAT_MOD_VS_TILE_32X8 0x23
#define DRM_FORMAT_MOD_VS_TILE_16X8 0x24
#define DRM_FORMAT_MOD_VS_TILE_16X16 0x25
#define DRM_FORMAT_MOD_VS_TILE_8X8_UNIT2X2 0x26
#define DRM_FORMAT_MOD_VS_TILE_8X4_UNIT2X2 0x27
#define DRM_FORMAT_MOD_VS_TILE_32X8_A 0x28
#define DRM_FORMAT_MOD_VS_TILE_8X8_SUPERTILE_X 0x29
#define DRM_FORMAT_MOD_VS_TILE_32X8_YUVSP8X8 0x2A
#define DRM_FORMAT_MOD_VS_TILE_16X8_YUVSP8X8 0x2B

#define fourcc_mod_vs_norm_code(tile) fourcc_mod_vs_code(DRM_FORMAT_MOD_VS_TYPE_NORMAL, (tile))

#ifndef DRM_FORMAT_MOD_VIVANTE_SUPER_TILED_FC
#define DRM_FORMAT_MOD_VIVANTE_SUPER_TILED_FC fourcc_mod_code(VIVANTE, 5)
#endif

#ifndef fourcc_mod_is_vendor
#define fourcc_mod_get_vendor(modifier) (((modifier) >> 56) & 0xff)
#define fourcc_mod_is_vendor(modifier, vendor) \
		(fourcc_mod_get_vendor(modifier) == DRM_FORMAT_MOD_VENDOR_##vendor)
#endif

#if defined(__cplusplus)
}
#endif
#endif /* __VS_DRM_FOURCC_H__ */

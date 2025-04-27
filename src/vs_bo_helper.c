/***************************************************************************
*    Copyright 2012 - 2023 Vivante Corporation, Santa Clara, California.
*    All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/

#include <drm/vs_drm_fourcc.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vs_bo_helper.h"

#define MIN_DS_OUT_SIZE 64
#define MAX_DS_OUT_SIZE 512
#define LTM_DS_FIXED_BIT 21
#define LTM_LUMA_AVE_FRAC_BIT 24
#define MAX_LTM_CD_WGT 16
#define MAX_LTM_CD_COEF_SUM 64
#define LTM_CD_FILT_NORM_FRAC_BIT 16
#define LTM_CD_SLOPE_FRAC_BIT 14

/* align needs to be power of 2 */
#define UP_ALIGN(x, align) ((x + align - 1) & ~(align - 1))

/* align with non-power of 2 */
#define ALIGN_NP2(n, align) (((n) + (align)-1) - (((n) + (align)-1) % (align)))

#ifndef fourcc_mod_get_vendor
#define fourcc_mod_get_vendor(val) (((val) >> 56) & 0xff)
#endif

#define fourcc_mod_vs_get_type(val) (((val)&DRM_FORMAT_MOD_VS_TYPE_MASK) >> 53)
#define fourcc_mod_vs_get_tile_mode(val) (uint8_t)((val)&DRM_FORMAT_MOD_VS_DEC_TILE_MODE_MASK)
#define fourcc_mod_vs_is_compressed(val) \
	!!(fourcc_mod_vs_get_type(val) == DRM_FORMAT_MOD_VS_TYPE_COMPRESSED ? 1 : 0)
#define fourcc_mod_vs_is_dec400a(val) \
	!!(fourcc_mod_vs_get_type(val) == DRM_FORMAT_MOD_VS_TYPE_DEC400A ? 1 : 0)
#define fourcc_mod_vs_is_pvric(val) \
	!!(fourcc_mod_vs_get_type(val) == DRM_FORMAT_MOD_VS_TYPE_PVRIC ? 1 : 0)
#define fourcc_mod_vs_is_decnano(val) \
	!!(fourcc_mod_vs_get_type(val) == DRM_FORMAT_MOD_VS_TYPE_DECNANO ? 1 : 0)
#define fourcc_mod_vs_is_etc2(val) \
	!!(fourcc_mod_vs_get_type(val) == DRM_FORMAT_MOD_VS_TYPE_ETC2 ? 1 : 0)
#define fourcc_mod_vs_is_normal(val) \
	!!(fourcc_mod_vs_get_type(val) == DRM_FORMAT_MOD_VS_TYPE_NORMAL ? 1 : 0)

#define NUM_SUPERBLOCK_LAYOUTS 7
#define HEADER_SIZE 16
const int superblock_width[NUM_SUPERBLOCK_LAYOUTS] = { 16, 16, 16, 32, 32, 32, 32 };
const int superblock_height[NUM_SUPERBLOCK_LAYOUTS] = { 16, 16, 16, 8, 8, 8, 8 };

typedef struct _context {
	uint32_t current_display;
	uint32_t display_width[VS_DISPLAY_COUNT];
	uint32_t display_height[VS_DISPLAY_COUNT];
	uint32_t ds_width[VS_DISPLAY_COUNT];
	uint32_t ds_height[VS_DISPLAY_COUNT];
} Context;

static Context context = { 0 };

static vs_status _drm_vs_display_size_convert(vs_display_size_type type, uint32_t *width,
					      uint32_t *height)
{
	vs_status status = VS_STATUS_OK;

	uint32_t width_internal = 0;
	uint32_t height_internal = 0;
	if (type > VS_DISPLAY_7680_4320_30) {
		printf("undefined display size type.\n");
		status = VS_STATUS_INVALID_ARGUMENTS;
		return status;
	}

	width_internal = 1920;
	height_internal = 1080;

	switch (type) {
	case VS_DISPLAY_640_480_60:
		width_internal = 640;
		height_internal = 480;
		break;

	case VS_DISPLAY_720_1612_60:
		width_internal = 720;
		height_internal = 1612;
		break;

	case VS_DISPLAY_1080_2400_60:
		width_internal = 1080;
		height_internal = 2400;
		break;

	case VS_DISPLAY_1280_720_60:
		width_internal = 1280;
		height_internal = 720;
		break;

	case VS_DISPLAY_1440_3216_60:
		width_internal = 1440;
		height_internal = 3216;
		break;

	case VS_DISPLAY_1440_3360_120:
		width_internal = 1440;
		height_internal = 3360;
		break;

	case VS_DISPLAY_1440_3520_120:
	case VS_DISPLAY_1440_3520_144:
		width_internal = 1440;
		height_internal = 3520;
		break;
	case VS_DISPLAY_1920_1080_120:
	case VS_DISPLAY_1920_1080_60:
		width_internal = 1920;
		height_internal = 1080;
		break;
	case VS_DISPLAY_2700_2600_120:
	case VS_DISPLAY_2700_2600_144:
		width_internal = 2700;
		height_internal = 2600;
		break;
	case VS_DISPLAY_2500_2820_120:
	case VS_DISPLAY_2500_2820_144:
		width_internal = 2500;
		height_internal = 2820;
		break;
	case VS_DISPLAY_2340_3404_120:
		width_internal = 2340;
		height_internal = 3404;
		break;
	case VS_DISPLAY_3200_1920_120:
		width_internal = 3200;
		height_internal = 1920;
		break;
	case VS_DISPLAY_3840_2160_120:
	case VS_DISPLAY_3840_2160_60:
		width_internal = 3840;
		height_internal = 2160;
		break;
	case VS_DISPLAY_7680_4320_30:
		width_internal = 7680;
		height_internal = 4320;
		break;
	default:
		break;
	}

	if (width)
		*width = width_internal;

	if (height)
		*height = height_internal;

	return status;
}

static double vs_util_dc_to_gamma(double x)
{
	double c1, c2, c3, m, y, ret;

	if (!x)
		return 0;

	m = (double)((2523.f / 4096.f) * 128.f);
	c1 = (double)(3424.f / 4096.f);
	c2 = (double)((2413.f / 4096.f) * 32.f);
	c3 = (double)((2392.f / 4096.f) * 32.f);
	y = (double)(x / 10000.f);

	c1 = c1 + c2 * y;
	c3 = 1 + c3 * y;
	c1 = c1 / c3;
	x = (double)(pow(c1, m));

	ret = (double)(x * 4095.f / 4096.f);

	return ret;
}

vs_status drm_vs_select_display(vs_display_id display_id)
{
	vs_status status = VS_STATUS_OK;

	if (display_id >= VS_DISPLAY_COUNT) {
		printf("invalid argument, display id exceeds the number of display.\n");
		status = VS_STATUS_INVALID_ARGUMENTS;
		return status;
	}

	context.current_display = display_id;

	return status;
}

vs_status drm_vs_display_set_timing(vs_display_size_type type)
{
	vs_status status = VS_STATUS_OK;

	uint32_t width = 0, height = 0;

	if (type > VS_DISPLAY_7680_4320_30) {
		printf("undefined display size type.\n");
		status = VS_STATUS_INVALID_ARGUMENTS;
		return status;
	}

	_drm_vs_display_size_convert(type, &width, &height);

	context.display_width[context.current_display] = width;
	context.display_height[context.current_display] = height;

	return status;
}

vs_status drm_vs_get_ltm_ds_params(struct drm_vs_rect *cropped, struct drm_vs_rect *output,
				   struct drm_vs_ltm_ds *ds_params)
{
	vs_status status = VS_STATUS_OK;

	uint32_t input_w = context.display_width[context.current_display];
	uint32_t input_h = context.display_height[context.current_display];

	if (output->w < MIN_DS_OUT_SIZE || output->w > MAX_DS_OUT_SIZE ||
	    output->h < MIN_DS_OUT_SIZE || output->h > MAX_DS_OUT_SIZE) {
		printf("output size of LTM ds out of range.\n");
		status = VS_STATUS_INVALID_ARGUMENTS;
		return status;
	}

	if (!cropped->w || !cropped->h || cropped->x + cropped->w > input_w ||
	    cropped->y + cropped->h > input_h) {
		printf("invalid 1cropped area of LTM ds.\n");
		status = VS_STATUS_INVALID_ARGUMENTS;
		return status;
	}

	ds_params->h_norm = (uint32_t)((1 << LTM_DS_FIXED_BIT) / (float)cropped->w + 0.5f);
	ds_params->v_norm = (uint32_t)((1 << LTM_DS_FIXED_BIT) / (float)cropped->h + 0.5f);
	ds_params->crop_l = cropped->x;
	ds_params->crop_r = input_w - cropped->x - cropped->w;
	ds_params->crop_t = cropped->y;
	ds_params->crop_b = input_h - cropped->y - cropped->h;
	memcpy(&ds_params->output, output, sizeof(struct drm_vs_rect));

	context.ds_width[context.current_display] = output->w;
	context.ds_height[context.current_display] = output->h;

	return status;
}

vs_status drm_vs_get_ltm_luma_ave_params(uint16_t margin_x, uint16_t margin_y,
					 struct drm_vs_ltm_luma_ave *luma_params)
{
	vs_status status = VS_STATUS_OK;

	uint32_t src_w = context.ds_width[context.current_display];
	uint32_t src_h = context.ds_height[context.current_display];
	uint32_t pixel_count = 0, pixel_norm = 0;

	if (!src_w || !src_h) {
		printf("need specify the downscaled luma image size firstly.\n");
		status = VS_STATUS_INVALID_ARGUMENTS;
		return status;
	}

	if (margin_x >= src_w / 2 || margin_y >= src_h / 2) {
		printf("the margin size out of range.\n");
		status = VS_STATUS_INVALID_ARGUMENTS;
		return status;
	}

	pixel_count = (src_w - 2 * margin_x) * (src_h - 2 * margin_y);
	pixel_norm = (uint32_t)((1 << LTM_LUMA_AVE_FRAC_BIT) / (float)pixel_count + 0.5f);

	luma_params->margin_x = margin_x;
	luma_params->margin_y = margin_y;
	luma_params->pixel_norm = pixel_norm;

	return status;
}

vs_status drm_vs_get_ltm_cd_params(struct drm_vs_ltm_cd_set *cd_set)
{
	vs_status status = VS_STATUS_OK;

	uint32_t filter_sum = 0, filter_norm = 0;
	uint32_t slope[2];

	if (cd_set->min_wgt > MAX_LTM_CD_WGT) {
		printf("the content detection minimum weight out of range.\n");
		status = VS_STATUS_INVALID_ARGUMENTS;
		return status;
	}

	filter_sum =
		4 * cd_set->coef[0] + 2 * cd_set->coef[1] + 2 * cd_set->coef[2] + cd_set->coef[3];
	if (filter_sum > MAX_LTM_CD_COEF_SUM) {
		printf("the coef sum of LTM content detection should be smaller than 64.\n");
		status = VS_STATUS_INVALID_ARGUMENTS;
		return status;
	}

	filter_norm = (uint32_t)((1 << LTM_CD_FILT_NORM_FRAC_BIT) / (float)filter_sum + 0.5f);

	slope[0] = (uint32_t)((1 << LTM_CD_SLOPE_FRAC_BIT) /
				      (float)(cd_set->thresh[1] - cd_set->thresh[0]) +
			      0.5f);
	slope[1] = (uint32_t)((1 << LTM_CD_SLOPE_FRAC_BIT) /
				      (float)(cd_set->thresh[3] - cd_set->thresh[2]) +
			      0.5f);

	cd_set->filt_norm = filter_norm;
	cd_set->slope[0] = slope[0];
	cd_set->slope[1] = slope[1];

	return status;
}

uint32_t drm_vs_get_stretch_factor(uint32_t src_size, uint32_t dst_size, bool scale_factor_set)
{
	uint32_t stretch_factor = 0;

	if (scale_factor_set) {
		if ((src_size > 1) && (dst_size > 1))
			stretch_factor = ((src_size - 1) << 16) / (dst_size - 1);
	} else {
		if ((src_size > 1) && (dst_size > 1))
			stretch_factor = (src_size << 16) / (dst_size);
	}

	return stretch_factor;
}

uint32_t drm_vs_get_stretch_initOffset(uint32_t stretch_factor, bool scale_factor_set)
{
	uint32_t stretch_initOffset = 0;

	if ((stretch_factor >> 16) == 1 && (stretch_factor & 0xFFFF) == 0) {
		//Equal
		stretch_initOffset = 0;
	} else {
		if ((stretch_factor >> 16) > 0) //Scale down, use sinc filter
			stretch_initOffset = 0; //Stretch, if no stretch then use 0x8000
		else {
			//Scale up, use bicubic filter
			if (scale_factor_set)
				stretch_initOffset = 0x8000;
			else
				stretch_initOffset = stretch_factor / 2 + (8 << 7);
		}
	}

	return stretch_initOffset;
}

float _drm_vs_sinc_filter(float x, int32_t radius)
{
	float pit, pitd, f1, f2, result;
	float f_radius = VS_MATH_INT2FLOAT(radius);

	if (x == 0.0f)
		result = 1.0f;
	else if ((x < -f_radius) || (x > f_radius))
		result = 0.0f;
	else {
		pit = VS_MATH_MULTIPLY(VS_PI, x);
		pitd = VS_MATH_DIVIDE(pit, f_radius);

		f1 = VS_MATH_DIVIDE(VS_MATH_SINE(pit), pit);
		f2 = VS_MATH_DIVIDE(VS_MATH_SINE(pitd), pitd);

		result = VS_MATH_MULTIPLY(f1, f2);
	}

	return result;
}

double _drm_vs_cmitchell_filter(double t)
{
	const double B = 0.;
	const double C = 0.75;
	double tt = t * t;

	if (t < 0)
		t = -t;

	if (t < 1.0) {
		t = (((12.0 - 9.0 * B - 6.0 * C) * (t * tt)) + ((-18.0 + 12.0 * B + 6.0 * C) * tt) +
		     (6.0 - 2 * B));
		return (t / 6.0);
	} else if (t < 2.0) {
		t = (((-1.0 * B - 6.0 * C) * (t * tt)) + ((6.0 * B + 30.0 * C) * tt) +
		     ((-12.0 * B - 48.0 * C) * t) + (8.0 * B + 24 * C));
		return t / 6.0;
	}

	return 0.0;
}

/* Calculate weight array for sync filter. compatible with dc8200 and dc9x00.
 */
vs_status drm_vs_calculate_sync_table(uint8_t kernel_size, uint32_t src_size, uint32_t dst_size,
				      int16_t *coef, uint32_t filter)
{
	uint32_t scale_factor = 0;
	float f_scale = 0;
	int32_t kernel_half = 0;
	float f_subpixel_step = 0;
	float f_subpixel_offset = 0;
	uint32_t sub_pixel_pos = 0;
	int32_t kernel_pos = 0;
	int32_t padding;
	int32_t kernel_pos_control;
	float f_weight = 0.0f;

	if (filter == VS_H9_V5) {
		kernel_pos_control = kernel_size;
	} else {
		kernel_pos_control = VS_MAXKERNELSIZE;
	}

	/* Compute the scale factor. */
	if (dst_size != 0)
		scale_factor = (src_size << 16) / dst_size;

	do {
		/* Compute the scale factor. */
		if (src_size != 0)
			f_scale = ((float)dst_size) / ((float)src_size);

		/* Adjust the factor for magnification. */
		if (f_scale > 1.0f)
			f_scale = 1.0f;

		/* Calculate the kernel half. */
		kernel_half = (int32_t)(kernel_size >> 1);
		/* Calculate the subpixel step. */
		f_subpixel_step = (float)VS_MATH_DIVIDE(1.0f, VS_MATH_INT2FLOAT(VS_SUBPIXELCOUNT));

		/* Init the subpixel offset. */
		if ((scale_factor >> 16) == 1 && (scale_factor & 0xFFFF) == 0)
			f_subpixel_offset = 0.0f;
		else
			f_subpixel_offset = 0.5f;

		padding = (VS_MAXKERNELSIZE - kernel_size) / 2;

		/* Loop through each subpixel. */
		for (sub_pixel_pos = 0; sub_pixel_pos < VS_SUBPIXELLOADCOUNT; sub_pixel_pos++) {
			/* Define a temporary set of weights. */
			float f_subpixel_set[VS_MAXKERNELSIZE];

			/* Init the sum of all weights for the current subpixel. */
			float f_weight_sum = 0.0f;
			uint16_t weight_sum = 0;
			int16_t adjust_count = 0, adjust_from = 0;
			int16_t adjustment = 0;

			/* Compute weights. */
			for (kernel_pos = 0; kernel_pos < kernel_pos_control; kernel_pos++) {
				/* Determine the current index. */
				int32_t index = 0;
				if (filter == VS_H9_V5) {
					index = kernel_pos;
				} else {
					index = kernel_pos - padding;
				}

				/* Pad with zeros. */
				if ((index < 0) || (index >= kernel_size))
					f_subpixel_set[kernel_pos] = 0.0f;
				else {
					if (kernel_size == 1)
						f_subpixel_set[kernel_pos] = 1.0f;
					else {
						/* Compute the x position for filter function. */
						float f_x = ((float)(index - kernel_half) +
							     f_subpixel_offset) *
							    f_scale;
						/* Compute the weight. */
						if ((scale_factor >> 16) > 0)
							f_subpixel_set[kernel_pos] =
								_drm_vs_sinc_filter(f_x,
										    kernel_half);
						else
							f_subpixel_set[kernel_pos] =
								(float)_drm_vs_cmitchell_filter(
									(double)f_x);
					}
					/* Update the sum of weights. */
					f_weight_sum = f_weight_sum + f_subpixel_set[kernel_pos];
				}
			}
			/* Adjust weights so that the sum will be 1.0. */
			for (kernel_pos = 0; kernel_pos < kernel_pos_control; kernel_pos++) {
				/* Normalize the current weight. */
				if (f_weight_sum)
					f_weight = f_subpixel_set[kernel_pos] / f_weight_sum;
				/* Convert the weight to fixed point and store in the table. */
				if (f_weight == 0.0f)
					coef[kernel_pos] = 0x0000;
				else if (f_weight >= 1.0f)
					coef[kernel_pos] = 0x4000;
				else if (f_weight <= -1.0f)
					coef[kernel_pos] = 0xC000;
				else
					coef[kernel_pos] = (int16_t)(f_weight * 16384.0f);

				weight_sum += coef[kernel_pos];
			}

			/* Adjust the fixed point coefficients. */
			adjust_count = 0x4000 - weight_sum;
			if (adjust_count < 0) {
				adjust_count = -adjust_count;
				adjustment = -1;
			} else
				adjustment = 1;

			adjust_from = (kernel_size - adjust_count) / 2;

			for (kernel_pos = 0; kernel_pos < adjust_count; kernel_pos++)
				coef[adjust_from + kernel_pos] += adjustment;

			coef += kernel_pos_control;
			/* Advance to the next subpixel. */
			f_subpixel_offset = f_subpixel_offset - f_subpixel_step;
		}
	} while (0);

	return VS_STATUS_OK;
}

enum drm_vs_filter_type drm_vs_get_info_filter_type(uint8_t filter_type_mask)
{
	enum drm_vs_filter_type filter_type = VS_H9_V5;

	switch (filter_type_mask) {
	case 0x01:
		filter_type = VS_H9_V5;
		break;
	case 0x02:
		filter_type = VS_H5_V3;
		break;
	case 0x04:
		filter_type = VS_H3_V3;
		break;
	case 0x06:
		filter_type = VS_H5_V3;
		break;
	case 0x08:
		filter_type = VS_H8_V4;
		break;
	default:
		filter_type = VS_H9_V5;
		break;
	}
	return filter_type;
}

const char *drm_vs_get_info_filter_name(enum drm_vs_filter_type filter_type)
{
	const char *filter_type_name[] = { "VS_H9_V5", "VS_H5_V3", "VS_H3_V3", "VS_H8_V4" };

	return filter_type_name[filter_type];
}

void drm_vs_get_filter_tap(enum drm_vs_filter_type filter, uint8_t *tap_h, uint8_t *tap_v)
{
	switch (filter) {
	case 0:
		*tap_h = 9;
		*tap_v = 5;
		break;
	case 1:
		*tap_h = 5;
		*tap_v = 3;
		break;
	case 2:
		*tap_h = 3;
		*tap_v = 3;
		break;
	case 3:
		*tap_h = 8;
		*tap_v = 4;
		break;
	default:
		*tap_h = 9;
		*tap_v = 5;
		break;
	}
}
static int _vs_get_format_info(uint32_t width, uint32_t height, uint32_t format, uint64_t mod,
			       uint32_t *num_planes, drm_vs_bo_param bo_param[4])
{
	if (fourcc_mod_vs_get_type(mod) == DRM_FORMAT_MOD_VS_TYPE_DEC400A) {
		switch (format) {
		case DRM_FORMAT_YUV420_8BIT:
			*num_planes = 1;
			bo_param[0].width = width;
			bo_param[0].height = height;
			bo_param[0].bpp = 12;
			break;
		case DRM_FORMAT_YUV420_10BIT:
			*num_planes = 1;
			bo_param[0].width = width;
			bo_param[0].height = height;
			bo_param[0].bpp = 24;
			break;
		default:
			*num_planes = 1;
			bo_param[0].width = width;
			bo_param[0].height = height;
			bo_param[0].bpp = 32;
		}
	} else {
		if (fourcc_mod_is_custom_format(mod)) {
			switch (format) {
			case DRM_FORMAT_NV12:
				*num_planes = 2;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 10;
				bo_param[1].width = width / 2;
				bo_param[1].height = height / 2;
				bo_param[1].bpp = 20;
				break;
			case DRM_FORMAT_YUV444:
				*num_planes = 3;
				bo_param[0].width = bo_param[1].width = bo_param[2].width = width;
				bo_param[0].height = bo_param[1].height = bo_param[2].height =
					height;
				bo_param[0].bpp = bo_param[1].bpp = bo_param[2].bpp = 10;
				break;
			case DRM_FORMAT_RGB888:
			case DRM_FORMAT_BGR888:
				*num_planes = 3;
				bo_param[0].width = bo_param[1].width = bo_param[2].width = width;
				bo_param[0].height = bo_param[1].height = bo_param[2].height =
					height;
				bo_param[0].bpp = bo_param[1].bpp = bo_param[2].bpp = 8;
				break;
			case DRM_FORMAT_RGB565_A8:
			case DRM_FORMAT_BGR565_A8:
				*num_planes = 1;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 24;
				break;
			case DRM_FORMAT_P010:
				*num_planes = 2;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 16;
				bo_param[1].width = width / 2;
				bo_param[1].height = height / 2;
				bo_param[1].bpp = 32;
				break;
			case DRM_FORMAT_P210:
				*num_planes = 2;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 16;
				bo_param[1].width = width / 2;
				bo_param[1].height = height;
				bo_param[1].bpp = 32;
				break;
			case DRM_FORMAT_YVU420:
			case DRM_FORMAT_YUV420:
				*num_planes = 3;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 16;
				bo_param[1].width = bo_param[2].width = width / 2;
				bo_param[1].height = bo_param[2].height = height / 2;
				bo_param[1].bpp = bo_param[2].bpp = 16;
				break;
			case DRM_FORMAT_YUV420_10BIT:
			case DRM_FORMAT_P016:
				*num_planes = 2;
				bo_param[0].width = width / 3;
				bo_param[0].height = height;
				bo_param[0].bpp = 32;
				bo_param[1].width = width / 6;
				bo_param[1].height = height / 2;
				bo_param[1].bpp = 64;
				break;
			/* LUMA_10 */
			case DRM_FORMAT_Y0L0:
				*num_planes = 1;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 16;
				break;
			default:
				fprintf(stderr, "unsupported format %u for mod: %lx \n", format,
					mod);
				return -1;
			}
		} else {
			switch (format) {
			case DRM_FORMAT_XRGB4444:
			case DRM_FORMAT_XBGR4444:
			case DRM_FORMAT_RGBX4444:
			case DRM_FORMAT_BGRX4444:
			case DRM_FORMAT_ARGB4444:
			case DRM_FORMAT_ABGR4444:
			case DRM_FORMAT_RGBA4444:
			case DRM_FORMAT_BGRA4444:
			case DRM_FORMAT_XRGB1555:
			case DRM_FORMAT_XBGR1555:
			case DRM_FORMAT_RGBX5551:
			case DRM_FORMAT_BGRX5551:
			case DRM_FORMAT_ARGB1555:
			case DRM_FORMAT_ABGR1555:
			case DRM_FORMAT_RGBA5551:
			case DRM_FORMAT_BGRA5551:
			case DRM_FORMAT_RGB565:
			case DRM_FORMAT_BGR565:
			case DRM_FORMAT_YUYV:
			case DRM_FORMAT_YVYU:
			case DRM_FORMAT_UYVY:
			case DRM_FORMAT_VYUY:
				*num_planes = 1;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 16;
				break;
			case DRM_FORMAT_RGB888:
			case DRM_FORMAT_BGR888:
				*num_planes = 1;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 24;
				break;
			case DRM_FORMAT_ARGB16161616F:
			case DRM_FORMAT_ABGR16161616F:
			case DRM_FORMAT_XRGB16161616F:
			case DRM_FORMAT_XBGR16161616F:
				*num_planes = 1;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 64;
				break;
			case DRM_FORMAT_NV12:
			case DRM_FORMAT_NV21:
				*num_planes = 2;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 8;
				bo_param[1].width = width / 2;
				bo_param[1].height = height / 2;
				bo_param[1].bpp = 16;
				break;
			case DRM_FORMAT_NV16:
			case DRM_FORMAT_NV61:
				*num_planes = 2;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 8;
				bo_param[1].width = width / 2;
				bo_param[1].height = height;
				bo_param[1].bpp = 16;
				break;
			case DRM_FORMAT_P010:
				*num_planes = 2;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 16;
				bo_param[1].width = width / 2;
				bo_param[1].height = height / 2;
				bo_param[1].bpp = 32;
				break;
			case DRM_FORMAT_P210:
				*num_planes = 2;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 16;
				bo_param[1].width = width / 2;
				bo_param[1].height = height;
				bo_param[1].bpp = 32;
				break;
			case DRM_FORMAT_YVU420:
			case DRM_FORMAT_YUV420:
				*num_planes = 3;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 8;
				bo_param[1].width = bo_param[2].width = width / 2;
				bo_param[1].height = bo_param[2].height = height / 2;
				bo_param[1].bpp = bo_param[2].bpp = 8;
				break;
			case DRM_FORMAT_YUV444:
			case DRM_FORMAT_YVU444:
				*num_planes = 3;
				bo_param[0].width = bo_param[1].width = bo_param[2].width = width;
				bo_param[0].height = bo_param[1].height = bo_param[2].height =
					height;
				bo_param[0].bpp = bo_param[1].bpp = bo_param[2].bpp = 8;
				break;
			case DRM_FORMAT_C8:
				*num_planes = 1;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 8;
				break;
			default:
				*num_planes = 1;
				bo_param[0].width = width;
				bo_param[0].height = height;
				bo_param[0].bpp = 32;
			}
		}
	}

	return 0;
}

int vs_mod_config(uint32_t format, uint64_t mod, uint32_t num_planes, uint64_t modifiers[4])
{
	uint32_t i;

	if (fourcc_mod_vs_is_compressed(mod)) {
		switch (format) {
		case DRM_FORMAT_NV12:
		case DRM_FORMAT_NV21:
			switch (fourcc_mod_vs_get_tile_mode(mod)) {
			case DRM_FORMAT_MOD_VS_DEC_RASTER_256X1:
				modifiers[0] = mod;
				modifiers[1] =
					fourcc_mod_vs_dec_code(DRM_FORMAT_MOD_VS_DEC_RASTER_128X1,
							       DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			case DRM_FORMAT_MOD_VS_DEC_RASTER_128X1:
				modifiers[0] = mod;
				modifiers[1] =
					fourcc_mod_vs_dec_code(DRM_FORMAT_MOD_VS_DEC_RASTER_64X1,
							       DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			case DRM_FORMAT_MOD_VS_DEC_TILE_32X8:
				modifiers[0] = mod;
				modifiers[1] =
					fourcc_mod_vs_dec_code(DRM_FORMAT_MOD_VS_DEC_TILE_32X4,
							       DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			case DRM_FORMAT_MOD_VS_DEC_TILE_16X8:
				modifiers[0] = mod;
				modifiers[1] =
					fourcc_mod_vs_dec_code(DRM_FORMAT_MOD_VS_DEC_TILE_16X4,
							       DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			case DRM_FORMAT_MOD_VS_DEC_TILE_32X8_YUVSP8X8:
				modifiers[0] = mod;
				modifiers[1] = fourcc_mod_vs_dec_code(
					DRM_FORMAT_MOD_VS_DEC_TILE_32X4_YUVSP8X8,
					DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			default:
				fprintf(stderr, "unsupported mod%lx for NV12\n", mod);
				return -1;
			}
			break;
		case DRM_FORMAT_P010:
		case DRM_FORMAT_P210:
			switch (fourcc_mod_vs_get_tile_mode(mod)) {
			case DRM_FORMAT_MOD_VS_DEC_RASTER_128X1:
				modifiers[0] = mod;
				modifiers[1] =
					fourcc_mod_vs_dec_code(DRM_FORMAT_MOD_VS_DEC_RASTER_64X1,
							       DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			case DRM_FORMAT_MOD_VS_DEC_RASTER_64X1:
				modifiers[0] = mod;
				modifiers[1] =
					fourcc_mod_vs_dec_code(DRM_FORMAT_MOD_VS_DEC_RASTER_32X1,
							       DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			case DRM_FORMAT_MOD_VS_DEC_TILE_16X8:
				modifiers[0] = mod;
				modifiers[1] =
					fourcc_mod_vs_dec_code(DRM_FORMAT_MOD_VS_DEC_TILE_16X4,
							       DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			case DRM_FORMAT_MOD_VS_DEC_TILE_8X8_XMAJOR:
				modifiers[0] = mod;
				modifiers[1] =
					fourcc_mod_vs_dec_code(DRM_FORMAT_MOD_VS_DEC_TILE_8X4,
							       DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			case DRM_FORMAT_MOD_VS_DEC_TILE_16X8_YUVSP8X8:
				modifiers[0] = mod;
				modifiers[1] = fourcc_mod_vs_dec_code(
					DRM_FORMAT_MOD_VS_DEC_TILE_16X4_YUVSP8X8,
					DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			default:
				fprintf(stderr, "unsupported mod%lx for P010\n", mod);
				return -1;
			}
			break;
		case DRM_FORMAT_NV16:
		case DRM_FORMAT_NV61:
			switch (fourcc_mod_vs_get_tile_mode(mod)) {
			case DRM_FORMAT_MOD_VS_DEC_RASTER_256X1:
				modifiers[0] = mod;
				modifiers[1] =
					fourcc_mod_vs_dec_code(DRM_FORMAT_MOD_VS_DEC_RASTER_128X1,
							       DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			case DRM_FORMAT_MOD_VS_DEC_RASTER_128X1:
				modifiers[0] = mod;
				modifiers[1] =
					fourcc_mod_vs_dec_code(DRM_FORMAT_MOD_VS_DEC_RASTER_64X1,
							       DRM_FORMAT_MOD_VS_DEC_ALIGN_32);
				break;
			default:
				fprintf(stderr, "unsupported mod%lx for NV16\n", mod);
				return -1;
			}
			break;
		default:
			for (i = 0; i < num_planes; i++)
				modifiers[i] = mod;
		}
	} else {
		for (i = 0; i < num_planes; i++)
			modifiers[i] = mod;
	}

	return 0;
}

uint16_t vs_get_dec_tile_size(uint8_t tile_mode, uint8_t bpp)
{
	uint16_t multi = 0;

	switch (tile_mode) {
	case DRM_FORMAT_MOD_VS_DEC_RASTER_16X1:
		multi = 16;
		break;
	case DRM_FORMAT_MOD_VS_DEC_TILE_8X4:
	case DRM_FORMAT_MOD_VS_DEC_TILE_4X8:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_32X1:
	case DRM_FORMAT_MOD_VS_DEC_TILE_8X4_S:
	case DRM_FORMAT_MOD_VS_DEC_TILE_8X4_UNIT2X2:
		multi = 32;
		break;
	case DRM_FORMAT_MOD_VS_DEC_TILE_8X8_XMAJOR:
	case DRM_FORMAT_MOD_VS_DEC_TILE_8X8_YMAJOR:
	case DRM_FORMAT_MOD_VS_DEC_TILE_8X8_UNIT2X2:
	case DRM_FORMAT_MOD_VS_DEC_TILE_8X8_SUPERTILE_X:
	case DRM_FORMAT_MOD_VS_DEC_TILE_16X4:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_16X4:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_64X1:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_32X2:
	case DRM_FORMAT_MOD_VS_DEC_TILE_16X4_S:
	case DRM_FORMAT_MOD_VS_DEC_TILE_16X4_LSB:
	case DRM_FORMAT_MOD_VS_DEC_TILE_16X4_YUVSP8X8:
		multi = 64;
		break;
	case DRM_FORMAT_MOD_VS_DEC_TILE_32X4:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_128X1:
	case DRM_FORMAT_MOD_VS_DEC_TILE_16X8:
	case DRM_FORMAT_MOD_VS_DEC_TILE_8X16:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_32X4:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_64X2:
	case DRM_FORMAT_MOD_VS_DEC_TILE_32X4_S:
	case DRM_FORMAT_MOD_VS_DEC_TILE_32X4_LSB:
	case DRM_FORMAT_MOD_VS_DEC_TILE_32X4_YUVSP8X8:
	case DRM_FORMAT_MOD_VS_DEC_TILE_16X8_YUVSP8X8:
		multi = 128;
		break;
	case DRM_FORMAT_MOD_VS_DEC_TILE_64X4:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_256X1:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_64X4:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_128X2:
	case DRM_FORMAT_MOD_VS_DEC_TILE_16X16:
	case DRM_FORMAT_MOD_VS_DEC_TILE_32X8:
	case DRM_FORMAT_MOD_VS_DEC_TILE_32X8_YUVSP8X8:
		multi = 256;
		break;
	case DRM_FORMAT_MOD_VS_DEC_RASTER_256X2:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_128X4:
	case DRM_FORMAT_MOD_VS_DEC_RASTER_512X1:
	case DRM_FORMAT_MOD_VS_DEC_TILE_128X4:
	case DRM_FORMAT_MOD_VS_DEC_TILE_32X16:
		multi = 512;
		break;
	case DRM_FORMAT_MOD_VS_DEC_TILE_256X4:
	case DRM_FORMAT_MOD_VS_DEC_TILE_64X16:
	case DRM_FORMAT_MOD_VS_DEC_TILE_128X8:
		multi = 1024;
		break;
	case DRM_FORMAT_MOD_VS_DEC_TILE_512X4:
		multi = 2048;
		break;
	default:
		break;
	}

	return multi * bpp / 8;
}

static uint16_t _vs_get_pvric_tile_height(uint8_t tile_mode)
{
	uint16_t height = 1;

	switch (tile_mode) {
	case DRM_FORMAT_MOD_VS_DEC_TILE_8X8:
		height = 8;
		break;
	case DRM_FORMAT_MOD_VS_DEC_TILE_16X4:
		height = 4;
		break;
	case DRM_FORMAT_MOD_VS_DEC_TILE_32X2:
		height = 2;
		break;
	default:
		break;
	}

	return height;
}

static uint32_t _vs_get_ts_buf_size(uint32_t buf_size, uint16_t tile_size, uint8_t tile_mode)
{
	uint32_t ts_size = 0;

	if (tile_mode == DRM_FORMAT_MOD_VS_DEC_TILE_8X8_UNIT2X2 ||
	    tile_mode == DRM_FORMAT_MOD_VS_DEC_TILE_8X4_UNIT2X2) {
		/*
		 * 8-bit tile status
		 * for compression unit size = 256 bytes, ts_size = buf_size / 256
		 */
		ts_size = UP_ALIGN(buf_size, 256) / 256;
	} else {
		/*
		 * 4-bit tile status
		 * for compression unit size = 256 bytes, ts_size = buf_size / 256 / 2
		 * for compression unit size = 128 bytes, ts_size = buf_size / 128 / 2
		 */
		ts_size = UP_ALIGN(buf_size, 512) / 512;

		if (tile_size == 128)
			ts_size = ts_size << 1;
	}

	return ts_size;
}

static void _vs_cal_bo_size(drm_vs_bo_param bo_param[4], uint64_t modifiers[4], uint32_t format,
			    uint32_t num_planes)
{
	uint32_t i;

	for (i = 0; i < num_planes; i++) {
		drm_vs_calibrate_bo_size(&bo_param[i], modifiers[i], format);
	}
}

int _drm_vs_eotf_pq(double *value)
{
	if (value == NULL) {
		printf(" NULL pointer in drm_vs_eotf_pq function. \n");
		return -1;
	}

	double result = 0, temp = 0;

	double m1 = 2610.0 / 4096.0 / 4.0;
	double m2 = 2523.0 / 4096.0 * 128.0;
	double c1 = 3424.0 / 4096.0;
	double c2 = 2413.0 / 4096.0 * 32.0;
	double c3 = 2392.0 / 4096.0 * 32.0;

	temp = pow(*value, (1.0 / m2));
	result = pow(VS_MAX(0.0, (temp - c1)) / (c2 - c3 * temp), (1.0 / m1));
	*value = result;

	return 0;
}

/* pure gamma function */
int _drm_vs_eotf_degamma(double *value, float exp)
{
	if (value == NULL) {
		printf(" NULL pointer in _drm_vs_eotf_degamma function. \n");
		return -1;
	}

	if ((*value) < 0 || (*value) > 1) {
		printf(" The entered data is out of range in EOTF_degamma. \n");
		return -1;
	}

	double result = 0;

	result = pow((*value), exp);

	*value = result;

	return 0;
}

int _drm_vs_oetf_regamma(double *value, float exp)
{
	if (value == NULL) {
		printf(" NULL pointer in _drm_vs_oetf_regamma function. \n");
		return -1;
	}

	if ((*value) < 0 || (*value) > 1) {
		printf(" The entered data is out of range in EOTF_regamma. \n");
		return -1;
	}

	double result = 0;

	result = pow((*value), (1.0 / exp));

	*value = result;

	return 0;
}

int _drm_vs_oetf_pq(double *value)
{
	if (value == NULL) {
		printf(" NULL pointer in _drm_vs_oetf_pq function. \n");
		return -1;
	}

	double result = 0, temp = 0;

	double m1 = (2610.0) / (4096.0 * 4.0);
	double m2 = (2523.0 * 128.0) / 4096.0;
	double c1 = (3424.0) / 4096.0;
	double c2 = (2413.0 * 32.0) / 4096.0;
	double c3 = (2392.0 * 32.0) / 4096.0;

	temp = pow(*value, m1);
	result = (double)(pow(((c2 * (temp) + c1) / (1.0 + c3 * (temp))), m2));

	*value = result;

	return 0;
}

/* sRGB-IEC61966 , exp default 2.4 */
int _drm_vs_eotf_srgb(double *value)
{
	if (value == NULL) {
		printf(" NULL pointer in _drm_vs_eotf_srgb function. \n");
		return -1;
	}

	double result = 0;

	if (0 <= *value && *value < 0.04045)
		result = *value / 12.92;
	else if (0.04045 <= *value && *value <= 1)
		result = pow((*value + 0.055) / 1.055, 2.4);

	*value = result;

	return 0;
}

int _drm_vs_oetf_srgb(double *value)
{
	if (value == NULL) {
		printf(" NULL pointer in _drm_vs_oetf_srgb function. \n");
		return -1;
	}

	double result = 0;

	if (0 <= *value && *value < 0.0031308)
		result = *value * 12.92;
	else if (0.0031308 <= *value && *value <= 1)
		result = 1.055 * pow(*value, (1.0 / 2.4)) - 0.055;

	*value = result;

	return 0;
}

int drm_vs_init_data_trans_entry(drm_vs_data_trans_mode mode, float exp, int in_bit, int out_bit,
				 uint32_t seg_cnt, uint32_t *seg_point, uint32_t *seg_step,
				 uint32_t *data)
{
	uint32_t gap_accu[VS_MAX_LUT_SEG_CNT] = { 0 };
	uint32_t gap_n[VS_MAX_LUT_SEG_CNT] = { 0 };
	double x_point[VS_MAX_LUT_ENTRY_CNT] = { 0 };
	uint32_t max_value = 1 << in_bit;
	uint32_t entry_cnt = 0;
	uint32_t i = 0, j = 0;
	int ret = 0;

	/* calculate each segment's pieces */
	for (i = 0; i < seg_cnt; i++) {
		if (i == 0)
			gap_n[i] = seg_point[0] / seg_step[0];
		else if (i == seg_cnt - 1)
			gap_n[i] = (max_value - seg_point[i - 1]) / seg_step[i];
		else
			gap_n[i] = (seg_point[i] - seg_point[i - 1]) / seg_step[i];
	}

	if (seg_cnt == 1) {
		entry_cnt = 0;
		for (i = 0; i <= max_value; i += seg_step[0]) {
			x_point[entry_cnt] = (double)i / max_value;
			entry_cnt++;
		}

		/* initation each entry data */
		for (i = 0; i < entry_cnt; i++) {
			switch (mode) {
			case DRM_VS_EOTF_PQ:
				ret = _drm_vs_eotf_pq(&x_point[i]);
				break;
			case DRM_VS_EOTF_DEGAMMA:
				ret = _drm_vs_eotf_degamma(&x_point[i], exp);
				break;
			case DRM_VS_EOTF_SRGB:
				ret = _drm_vs_eotf_srgb(&x_point[i]);
				break;
			case DRM_VS_OETF_PQ:
				ret = _drm_vs_oetf_pq(&x_point[i]);
				break;
			case DRM_VS_OETF_REGAMMA:
				ret = _drm_vs_oetf_regamma(&x_point[i], exp);
				break;
			case DRM_VS_OETF_SRGB:
				ret = _drm_vs_oetf_srgb(&x_point[i]);
				break;
			default:
				break;
			}

			data[i] = (uint32_t)(x_point[i] * (((uint32_t)1 << out_bit) - 1) + 0.5f);
		}
	} else {
		/* calculate the entry count */
		for (i = 0; i < seg_cnt; i++) {
			if (0 != i)
				gap_accu[i] = gap_accu[i - 1] + gap_n[i];
			else
				gap_accu[i] = gap_n[i] + 1; /* plus 0 point */
		}

		/* initation the entry count */
		entry_cnt = gap_accu[seg_cnt - 1];

		for (i = 0; i <= gap_n[0]; i++)
			x_point[i] = (double)(i * seg_step[0]) / max_value;

		for (j = 1; j < seg_cnt; j++) {
			for (i = 0; i < gap_n[j]; i++) {
				x_point[gap_accu[j - 1] + i] =
					(double)(seg_point[j - 1] + (i + 1) * seg_step[j]) /
					max_value;
			}
		}

		for (i = 0; i < entry_cnt; i++) {
			switch (mode) {
			case DRM_VS_EOTF_PQ:
				ret = _drm_vs_eotf_pq(&x_point[i]);
				break;
			case DRM_VS_EOTF_DEGAMMA:
				ret = _drm_vs_eotf_degamma(&x_point[i], exp);
				break;
			case DRM_VS_EOTF_SRGB:
				ret = _drm_vs_eotf_srgb(&x_point[i]);
				break;
			case DRM_VS_OETF_PQ:
				ret = _drm_vs_oetf_pq(&x_point[i]);
				break;
			case DRM_VS_OETF_REGAMMA:
				ret = _drm_vs_oetf_regamma(&x_point[i], exp);
				break;
			case DRM_VS_OETF_SRGB:
				ret = _drm_vs_oetf_srgb(&x_point[i]);
				break;
			default:
				break;
			}

			data[i] = (uint32_t)(x_point[i] * (((uint32_t)1 << out_bit) - 1) + 0.5f);
		}
	}

	if (ret) {
		printf(" fail to init data transform entries. \n");
		return -1;
	}

	return entry_cnt;
}

int drm_vs_init_gamma_lut(int new_gamma, const char *curve_type, double gamma_value,
			  int gamma_bit_out, int gamma_entry_cnt, struct drm_color_lut *lut)
{
	int i;
	uint32_t temp, table_value = 0;
	uint32_t table0[17] = { 0 };
	uint32_t table1[257] = { 0 };
	double tmpf, tmpf0, tmpf1;

	if (new_gamma) {
		if (!strcmp(curve_type, "PQ")) {
			for (i = 0; i <= 16; i++) {
				tmpf0 = (double)(i / 1024.f * 10000.f);
				tmpf1 = vs_util_dc_to_gamma(tmpf0);
				table0[i] = (uint32_t)(tmpf1 * 4095.f + 0.95f);
			}

			for (i = 4; i < 256; i++) {
				tmpf0 = (double)(i / 256.f * 10000.f);
				tmpf1 = vs_util_dc_to_gamma(tmpf0);
				table1[i] = (uint32_t)(tmpf1 * 4095.f + 0.5f);
			}
			table1[256] = 4095;

			/* final table construct */
			for (i = 0; i < gamma_entry_cnt; i++) {
				/* When input data is less than 32, directly look up the table
                 * Direct look up needs 32 entries
                 */
				if (i < 32) {
					tmpf0 = (double)(i / 16384.f * 10000.f);
					tmpf1 = vs_util_dc_to_gamma(tmpf0);
					table_value = (uint32_t)(tmpf1 * 4095.f + 0.5f);
				}

				/* When input data >=32 and < 256, look up the table and then do interpolation
                 * Each interval has 16 data. [32,48),[48,64), ...[240,256).
                 * It needs 15 entries
                 */
				else if (i < 47)
					table_value = table0[i - 30]; /* use table0[2] - [16] */

				/* When input data >=256, look up the  table and then do interpolation
                 * Each interval has 64 data. [256,320),[320,384) ...[16320,12384)
                 * It needs 252+1 entries.  Extra 1 is for 10384 look up.
                 */
				else
					table_value = table1[i - 43]; /* use table1[4] - [256] */

				lut[i].red = table_value & 0xFFF;
				lut[i].green = table_value & 0xFFF;
				lut[i].blue = table_value & 0xFFF;
			}
		} else {
			for (i = 0; i <= 16; i++) {
				tmpf0 = (double)(i / 1024.f);
				tmpf1 = (double)pow(tmpf0, 1 / gamma_value);
				table0[i] = (uint32_t)(tmpf1 * 4095.f + 0.95f);
			}

			for (i = 4; i < 256; i++) {
				tmpf0 = (double)(i / 256.f);
				tmpf1 = (double)pow(tmpf0, 1 / gamma_value);
				table1[i] = (uint32_t)(tmpf1 * 4095.f + 0.5f);
			}
			table1[256] = 4095;

			/* final table construct */
			for (i = 0; i < gamma_entry_cnt; i++) {
				/* When input data is less than 32, directly look up the table
                 * Direct look up needs 32 entries
                 */
				if (i < 32) {
					tmpf0 = (double)(i / 16384.f);
					tmpf1 = (double)pow(tmpf0, 1 / gamma_value);
					table_value = (uint32_t)(tmpf1 * (4095.f - 1.f));
				}

				/* When input data >=32 and < 256, look up the table and then do interpolation
                 * Each interval has 16 data. [32,48),[48,64), ...[240,256).
                 * It needs 15 entries
                 */
				else if (i < 47)
					table_value = table0[i - 30]; /* use table0[2] - [16] */

				/* When input data >=256, look up the  table and then do interpolation
                 * Each interval has 64 data. [256,320),[320,384) ...[16320,12384)
                 * It needs 252+1 entries.  Extra 1 is for 10384 look up.
                 */
				else
					table_value = table1[i - 43]; /* use table1[4] - [256] */

				lut[i].red = table_value & 0xFFF;
				lut[i].green = table_value & 0xFFF;
				lut[i].blue = table_value & 0xFFF;
			}
		}
	} else {
		for (i = 0; i < gamma_entry_cnt; i++) {
			tmpf = (i + 0.5f) / (double)(1 << gamma_bit_out);
			tmpf = (double)(pow((double)tmpf, (double)1 / gamma_value));
			temp = (uint32_t)(tmpf * (1 << gamma_bit_out) - 0.5f);

			lut[i].red = temp & ((1 << gamma_bit_out) - 1);
			lut[i].green = temp & ((1 << gamma_bit_out) - 1);
			lut[i].blue = temp & ((1 << gamma_bit_out) - 1);
		}
	}

	return 0;
}

int drm_vs_init_gamma_curve(uint32_t in_bit, uint32_t out_bit, uint32_t entry_cnt, float exp,
			    uint32_t *data)
{
	double x_point[VS_MAX_LUT_ENTRY_CNT] = { 0 };
	uint32_t step = 0;
	uint32_t max_value = 1 << in_bit;
	uint32_t i = 0;

	step = max_value / entry_cnt;
	/* step value should power of 2 */
	while (!VS_IS_POWER_OF_TWO(step))
		step--;

	for (i = 0; i < entry_cnt; i++)
		x_point[i] = (double)(i * step) / max_value;

	/* initation each entry data */
	for (i = 0; i < entry_cnt; i++) {
		_drm_vs_oetf_regamma(&x_point[i], exp);
		data[i] = (uint32_t)(x_point[i] * (((uint32_t)1 << out_bit) - 1) + 0.5f);
	}

	data[entry_cnt] = ((uint32_t)1 << out_bit) - 1;

	return 0;
}

int drm_vs_init_degamma_curve(uint32_t in_bit, uint32_t out_bit, uint32_t entry_cnt, float exp,
			      uint32_t *data)
{
	double x_point[VS_MAX_LUT_ENTRY_CNT] = { 0 };
	uint32_t step = 0;
	uint32_t max_value = 1 << in_bit;
	uint32_t i = 0;

	step = max_value / entry_cnt;
	/* step value should power of 2 */
	while (!VS_IS_POWER_OF_TWO(step))
		step--;

	for (i = 0; i < entry_cnt; i++)
		x_point[i] = (double)(i * step) / max_value;

	/* initation each entry data */
	for (i = 0; i < entry_cnt; i++) {
		_drm_vs_eotf_degamma(&x_point[i], exp);
		data[i] = (uint32_t)(x_point[i] * (((uint32_t)1 << out_bit) - 1) + 0.5f);
	}

	data[entry_cnt] = ((uint32_t)1 << out_bit) - 1;

	return 0;
}

void drm_vs_get_tile_height(uint32_t format, uint64_t mod, int *height)
{
	uint8_t tile_mode;

	if (fourcc_mod_vs_is_normal(mod)) {
		tile_mode = (uint8_t)((mod)&DRM_FORMAT_MOD_VS_NORM_MODE_MASK);
		switch (tile_mode) {
		case DRM_FORMAT_MOD_VS_LINEAR:
			*height = 1;
			*(height + 1) = 1;
			*(height + 2) = 1;
			break;
		case DRM_FORMAT_MOD_VS_TILE_16X4:
			*height = 4; /* TODO: need to add other plane if YUV format is supported */
			break;
		case DRM_FORMAT_MOD_VS_TILE_16X8_YUVSP8X8:
			if (format == DRM_FORMAT_P010) {
				*height = 8;
				*(height + 1) = 4;
			}
			break;
		case DRM_FORMAT_MOD_VS_TILE_32X8_YUVSP8X8:
			if (format == DRM_FORMAT_NV12 || format == DRM_FORMAT_NV21) {
				*height = 8;
				*(height + 1) = 4;
			}
			break;
		default:
			break;
		}
	} else if (fourcc_mod_vs_is_compressed(mod)) {
		tile_mode = (uint8_t)((mod)&DRM_FORMAT_MOD_VS_DEC_TILE_MODE_MASK);
		switch (tile_mode) {
		case DRM_FORMAT_MOD_VS_DEC_RASTER_16X1:
		case DRM_FORMAT_MOD_VS_DEC_RASTER_32X1:
		case DRM_FORMAT_MOD_VS_DEC_RASTER_64X1:
		case DRM_FORMAT_MOD_VS_DEC_RASTER_128X1:
		case DRM_FORMAT_MOD_VS_DEC_RASTER_256X1:
		case DRM_FORMAT_MOD_VS_DEC_RASTER_512X1:
			*height = 1;
			*(height + 1) = 1;
			*(height + 2) = 1;
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_16X8_YUVSP8X8:
			if (format == DRM_FORMAT_P010) {
				*height = 8;
				*(height + 1) = 4;
			}
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_32X8_YUVSP8X8:
			if (format == DRM_FORMAT_NV12 || format == DRM_FORMAT_NV21) {
				*height = 8;
				*(height + 1) = 4;
			}
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_8X8_SUPERTILE_X:
			*height = 64;
			break;
		default:
			break;
		}
	} else if (fourcc_mod_vs_is_pvric(mod)) {
		tile_mode = (uint8_t)((mod)&DRM_FORMAT_MOD_VS_DEC_TILE_MODE_MASK);
	} else if (fourcc_mod_vs_is_decnano(mod)) {
		tile_mode = (uint8_t)((mod)&DRM_FORMAT_MOD_VS_DEC_TILE_MODE_MASK);
	} else if (fourcc_mod_vs_is_etc2(mod)) {
		tile_mode = (uint8_t)((mod)&DRM_FORMAT_MOD_VS_DEC_TILE_MODE_MASK);
	} else if (fourcc_mod_vs_is_dec400a(mod)) {
		tile_mode = (uint8_t)((mod)&DRM_FORMAT_MOD_VS_DEC_TILE_MODE_MASK);
	}
}

int drm_vs_get_align_size(uint32_t *width, uint32_t *height, uint32_t format, uint64_t mod)
{
	uint8_t tile_mode;
	uint32_t ori_width = *width;
	uint32_t ori_height = *height;

	tile_mode = fourcc_mod_vs_get_tile_mode(mod);
	if (fourcc_mod_vs_is_compressed(mod)) {
		/* alignment requirements for dec400 sub-IP */
		switch (tile_mode) {
		case DRM_FORMAT_MOD_VS_DEC_RASTER_32X1:
			*width = UP_ALIGN(ori_width, 32);
			break;
		case DRM_FORMAT_MOD_VS_DEC_RASTER_64X1:
			*width = UP_ALIGN(ori_width, 64);
			break;
		case DRM_FORMAT_MOD_VS_DEC_RASTER_128X1:
			*width = UP_ALIGN(ori_width, 128);
			break;
		case DRM_FORMAT_MOD_VS_DEC_RASTER_256X1:
			if (format == DRM_FORMAT_YVU420 || format == DRM_FORMAT_YUV420)
				*width = UP_ALIGN(ori_width, 512);
			else
				*width = UP_ALIGN(ori_width, 256);
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_8X4:
		case DRM_FORMAT_MOD_VS_DEC_TILE_4X8:
			*width = UP_ALIGN(ori_width, 64);
			*height = UP_ALIGN(ori_height, 64);
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_8X4_UNIT2X2:
			*width = UP_ALIGN(ori_width, 8);
			*height = UP_ALIGN(ori_height, 4);
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_8X8_UNIT2X2:
			*width = UP_ALIGN(ori_width, 8);
			*height = UP_ALIGN(ori_height, 8);
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_8X8_XMAJOR:
			if (format == DRM_FORMAT_YUYV || format == DRM_FORMAT_UYVY ||
			    format == DRM_FORMAT_P010) {
				*width = UP_ALIGN(ori_width, 16);
				*height = UP_ALIGN(ori_height, 8);
			} else {
				*width = UP_ALIGN(ori_width, 64);
				*height = UP_ALIGN(ori_height, 64);
			}
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_16X8:
		case DRM_FORMAT_MOD_VS_DEC_TILE_32X8:
			*width = UP_ALIGN(ori_width, 16);
			*height = UP_ALIGN(ori_height, 8);
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_8X8_SUPERTILE_X:
			*width = UP_ALIGN(ori_width, 64);
			*height = UP_ALIGN(ori_height, 64);
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_32X8_YUVSP8X8:
			*width = UP_ALIGN(ori_width, 32);
			*height = UP_ALIGN(ori_height, 8);
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_16X8_YUVSP8X8:
			*width = UP_ALIGN(ori_width, 16);
			*height = UP_ALIGN(ori_height, 8);
			break;
		default:
			break;
		}
	} else if (fourcc_mod_vs_is_pvric(mod)) {
		/* alignment requirements for PVRIC sub-IP */
		switch (tile_mode) {
		case DRM_FORMAT_MOD_VS_DEC_TILE_8X8:
			if (format == DRM_FORMAT_P010) {
				*width = UP_ALIGN(ori_width, 16);
				*height = UP_ALIGN(ori_height, 8);
			} else if (format == DRM_FORMAT_C8) {
				*width = UP_ALIGN(ori_width, 32);
				*height = UP_ALIGN(ori_height, 8);
			} else {
				*width = UP_ALIGN(ori_width, 32);
				*height = UP_ALIGN(ori_height, 8);
			}
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_16X4:
			if (format == DRM_FORMAT_RGB565 || format == DRM_FORMAT_BGR565) {
				*width = UP_ALIGN(ori_width, 32);
				*height = UP_ALIGN(ori_height, 4);
			} else {
				*width = UP_ALIGN(ori_width, 16);
				*height = UP_ALIGN(ori_height, 4);
			}
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_32X2:
			*width = UP_ALIGN(ori_width, 32);
			*height = UP_ALIGN(ori_height, 2);
			break;
		default:
			break;
		}
	} else if (fourcc_mod_vs_is_decnano(mod)) {
		/* alignment requirements for DECNano sub-IP */
		switch (tile_mode) {
		case DRM_FORMAT_MOD_VS_DEC_LINEAR:
			*width = UP_ALIGN(ori_width, 16);
			break;
		case DRM_FORMAT_MOD_VS_DEC_TILE_4X4:
			*width = UP_ALIGN(ori_width, 16);
			*height = UP_ALIGN(ori_height, 4);
			break;
		default:
			break;
		}
	} else if (fourcc_mod_vs_is_etc2(mod)) {
		/* alignment requirements for ETC2 sub-IP */
		switch (tile_mode) {
		case DRM_FORMAT_MOD_VS_DEC_TILE_4X4:
			*width = UP_ALIGN(ori_width, 16);
			*height = UP_ALIGN(ori_height, 4);
			break;
		default:
			break;
		}
	} else {
		switch (tile_mode) {
		case DRM_FORMAT_MOD_VS_LINEAR:
			switch (format) {
			case DRM_FORMAT_YUYV:
			case DRM_FORMAT_YVYU:
			case DRM_FORMAT_UYVY:
			case DRM_FORMAT_VYUY:
			case DRM_FORMAT_NV12:
			case DRM_FORMAT_NV21:
			case DRM_FORMAT_NV16:
			case DRM_FORMAT_NV61:
			case DRM_FORMAT_YVU420:
			case DRM_FORMAT_YUV420:
			case DRM_FORMAT_P010:
			case DRM_FORMAT_P210:
				*width = UP_ALIGN(ori_width, 2);
				*height = UP_ALIGN(ori_height, 2);
				break;
			case DRM_FORMAT_YUV420_10BIT:
			case DRM_FORMAT_P016:
				*width = ALIGN_NP2(ori_width, 2);
				*height = UP_ALIGN(ori_height, 2);
				break;
			case DRM_FORMAT_C8:
				*width = UP_ALIGN(ori_width, 1);
				*height = UP_ALIGN(ori_height, 1);
				break;
			default:
				break;
			}
			break;
		case DRM_FORMAT_MOD_VS_TILE_8X8:
		case DRM_FORMAT_MOD_VS_TILE_8X8_UNIT2X2:
			*width = UP_ALIGN(ori_width, 8);
			*height = UP_ALIGN(ori_height, 8);
			break;
		case DRM_FORMAT_MOD_VS_TILE_8X4:
		case DRM_FORMAT_MOD_VS_TILE_8X4_UNIT2X2:
			*width = UP_ALIGN(ori_width, 8);
			*height = UP_ALIGN(ori_height, 4);
			break;
		case DRM_FORMAT_MOD_VS_SUPER_TILED_XMAJOR:
		case DRM_FORMAT_MOD_VS_SUPER_TILED_XMAJOR_8X4:
		case DRM_FORMAT_MOD_VS_SUPER_TILED_YMAJOR_4X8:
			*width = UP_ALIGN(ori_width, 64);
			*height = UP_ALIGN(ori_height, 64);
			break;
		case DRM_FORMAT_MOD_VS_TILE_MODE4X4:
			if (format == DRM_FORMAT_NV12 || format == DRM_FORMAT_NV21) {
				*width = UP_ALIGN(ori_width, 64);
				*height = UP_ALIGN(ori_height, 4);
			} else if (format == DRM_FORMAT_YUV444) {
				*width = UP_ALIGN(ori_width, 32);
				*height = UP_ALIGN(ori_height, 4);
			}
			break;
		case DRM_FORMAT_MOD_VS_TILE_32X8:
		case DRM_FORMAT_MOD_VS_TILE_32X8_A:
			*width = UP_ALIGN(ori_width, 32);
			*height = UP_ALIGN(ori_height, 8);
			break;
		case DRM_FORMAT_MOD_VS_TILE_16X16:
			*width = UP_ALIGN(ori_width, 16);
			*height = UP_ALIGN(ori_height, 16);
			break;
		case DRM_FORMAT_MOD_VS_TILE_16X4:
			*width = UP_ALIGN(ori_width, 16);
			*height = UP_ALIGN(ori_height, 4);
			break;
		case DRM_FORMAT_MOD_VS_TILE_8X8_SUPERTILE_X:
			*width = UP_ALIGN(ori_width, 64);
			*height = UP_ALIGN(ori_height, 64);
			break;
		case DRM_FORMAT_MOD_VS_TILE_32X8_YUVSP8X8:
			*width = UP_ALIGN(ori_width, 32);
			*height = UP_ALIGN(ori_height, 8);
			break;
		case DRM_FORMAT_MOD_VS_TILE_16X8_YUVSP8X8:
			*width = UP_ALIGN(ori_width, 16);
			*height = UP_ALIGN(ori_height, 8);
			break;
		default:
			break;
		}
	}

	return 0;
}

static uint32_t mb_round(int mb, int q)
{
	return (mb / q + ((mb % q) ? 1 : 0)) * q;
}

static uint32_t _vs_get_dec400a_ts_buf_size(drm_vs_bo_param *bo_param, uint32_t format)
{
	int superblock_layout = 0;
	uint32_t mb_sizew = 0, mb_sizeh = 0, mbw = 0, mbh = 0;
	uint32_t ts_buffer_size;

	if (format == DRM_FORMAT_ARGB8888 || format == DRM_FORMAT_ARGB2101010) {
		superblock_layout = 3;
	} else if (format == DRM_FORMAT_YUV420_8BIT || format == DRM_FORMAT_YUV420_10BIT) {
		superblock_layout = 5;
	}

	mb_sizew = superblock_width[superblock_layout];
	mb_sizeh = superblock_height[superblock_layout];
	mbw = (bo_param->width + mb_sizew - 1) / mb_sizew;
	mbh = (bo_param->height + mb_sizeh - 1) / mb_sizeh;

	ts_buffer_size = mb_round(mbw, 1) * mb_round(mbh, 1) * HEADER_SIZE;

	return ts_buffer_size;
}

void drm_vs_calibrate_bo_size(drm_vs_bo_param *bo_param, uint64_t modifier, uint32_t format)
{
	uint8_t tile_mode;
	uint16_t tile_size, tile_height;
	uint32_t ts_buf_size, stride, bo_height, tile_cnt, fc_size = 0;
	uint64_t aligned_area;
	bool lossy;

	if (fourcc_mod_vs_is_compressed(modifier)) {
		/* buffer size calculation for dec400 sub-IP */
		tile_mode = fourcc_mod_vs_get_tile_mode(modifier);
		tile_size = vs_get_dec_tile_size(tile_mode, bo_param->bpp);
		stride = bo_param->width * bo_param->bpp / 8;
		aligned_area = ALIGN_NP2(stride, tile_size) * bo_param->height;
		/* Align ts buf size to stride, so we can get integer height */
		ts_buf_size = ALIGN_NP2((_vs_get_ts_buf_size(aligned_area, tile_size, tile_mode)),
					stride);

		if (dec_mod_is_fc(modifier))
			fc_size = 128 + 128 * dec_mod_get_fc_size(modifier);
		bo_param->ts_buf_size = ts_buf_size + fc_size;

		/* Get bo_height with tile status buffer */
		bo_param->height += ts_buf_size / stride;
	} else if (fourcc_mod_vs_is_dec400a(modifier)) {
		stride = bo_param->width * bo_param->bpp / 8;
		ts_buf_size = _vs_get_dec400a_ts_buf_size(bo_param, format);
		bo_param->ts_buf_size = ts_buf_size;

		/* Get bo_height with tile status buffer */
		bo_param->height += ALIGN_NP2(ts_buf_size, stride) / stride;
	} else if (fourcc_mod_vs_is_pvric(modifier)) {
		/* buffer size calculation for PVRIC sub-IP */
		tile_mode = fourcc_mod_vs_get_tile_mode(modifier);
		tile_height = _vs_get_pvric_tile_height(tile_mode);
		stride = bo_param->width * bo_param->bpp / 8;
		aligned_area = UP_ALIGN(stride, 256 / tile_height) * bo_param->height;
		tile_cnt = aligned_area / 256;
		/* Align (header size + data base addr alignment) to stride,
		 * so we can get integer height
		 */
		ts_buf_size = UP_ALIGN(tile_cnt + 256, stride);

		if (modifier & DRM_FORMAT_MOD_VS_DEC_LOSSY)
			lossy = true;
		else
			lossy = false;

		if (!lossy)
			aligned_area += ts_buf_size;
		else if (format == DRM_FORMAT_P010)
			aligned_area = ts_buf_size + UP_ALIGN(tile_cnt * 96, stride);
		else
			aligned_area = ts_buf_size + UP_ALIGN(tile_cnt * 128, stride);

		/* Get bo_height with header buffer and addr alignment */
		bo_height = aligned_area / stride;
		bo_param->height = bo_height;
		bo_param->header_size = tile_cnt;
	}
}

int drm_vs_bo_config(uint32_t width, uint32_t height, uint32_t format, uint64_t mod,
		     drm_vs_bo_param bo_param[4])
{
	uint32_t num_planes;
	uint64_t modifiers[4] = { 0 };
	int ret;

	memset(bo_param, 0, sizeof(drm_vs_bo_param) * 4);

	ret = _vs_get_format_info(width, height, format, mod, &num_planes, bo_param);
	if (ret)
		return ret;
	ret = vs_mod_config(format, mod, num_planes, modifiers);
	if (ret)
		return ret;

	_vs_cal_bo_size(bo_param, modifiers, format, num_planes);

	return 0;
}

uint32_t drm_vs_get_ltm_norm(uint16_t *coef, uint32_t size)
{
#define VS_LTM_FREQ_NORM_FRAC_BIT 18
#define VS_LTM_FREQ_COEF_NUM 9

	uint32_t filter_sum = 0, filter_norm = 0;

	if (size < VS_LTM_FREQ_COEF_NUM) {
		printf("invalid coef size.\n");
		return 0;
	}

	filter_sum = 4 * (coef[0] + coef[1] + coef[3] + coef[4]) +
		     2 * (coef[2] + coef[5] + coef[6] + coef[7]) + coef[8];

	filter_norm = (uint32_t)((1 << VS_LTM_FREQ_NORM_FRAC_BIT) / (float)filter_sum + 0.5f);

	return filter_norm;

#undef VS_LTM_FREQ_NORM_FRAC_BIT
#undef VS_LTM_FREQ_COEF_NUM
}

/* Gamut mapping coefs */
/* RGB709 to RGB2020 */
const float RGB2RGB_2020[VS_MAX_GAMUT_COEF_NUM] = { 0.6274, 0.3293, 0.0433, 0.0691, 0.9195, 0.0114,
						    0.0164, 0.0880, 0.8956, 0.0,    0.0,    0.0 };
/* RGB2020 to RGB709 */
const float RGB2RGB_709[VS_MAX_GAMUT_COEF_NUM] = { 1.6605, -0.5876, -0.0729, -0.1246,
						   1.1329, -0.0083, -0.0182, -0.1006,
						   1.1187, 0.0,	    0.0,     0.0 };
/* RGB2020 to DCIP3 */
const float RGB2DCIP3[VS_MAX_GAMUT_COEF_NUM] = { 1.34351787, -0.28214023, -0.06137764, -0.06530391,
						 1.07579231, -0.01048839, 0.00282263,  -0.0196025,
						 1.01677987, 0.0,	  0.0,	       0.0 };
/* DCIP3 to RGB2020 */
const float DCIP32RGB[VS_MAX_GAMUT_COEF_NUM] = { 0.75386691, 0.19857772, 0.04755536,  0.04575024,
						 0.94177337, 0.01247638, -0.00121075, 0.01760519,
						 0.98360556, 0.0,	 0.0,	      0.0 };
/* DCIP3 to SRGB */
const float DCIP32SRGB[VS_MAX_GAMUT_COEF_NUM] = { 1.22490054e+00,
						  -2.24900543e-01,
						  2.22044605e-16,
						  -4.20632597e-02,
						  1.04206326e+00,
						  -3.46944695e-17,
						  -1.96447584e-02,
						  -7.86535769e-02,
						  1.09829834e+00,
						  0.0,
						  0.0,
						  0.0 };
/* SRGB to DCIP3 */
const float SRGB2DCIP3[VS_MAX_GAMUT_COEF_NUM] = { 8.22488581e-01,
						  1.77511419e-01,
						  0.00000000e+00,
						  3.32000485e-02,
						  9.66799951e-01,
						  2.77555756e-17,
						  1.70890654e-02,
						  7.24115122e-02,
						  9.10499422e-01,
						  0.0,
						  0.0,
						  0.0 };

const float *vs_dc_get_ccm_coef(enum drm_vs_ccm_mode mode)
{
	switch (mode) {
	case VS_CCM_709_TO_2020:
		return RGB2RGB_2020;
	case VS_CCM_2020_TO_709:
		return RGB2RGB_709;
	case VS_CCM_2020_TO_DCIP3:
		return RGB2DCIP3;
	case VS_CCM_DCIP3_TO_2020:
		return DCIP32RGB;
	case VS_CCM_DCIP3_TO_SRGB:
		return DCIP32SRGB;
	case VS_CCM_SRGB_TO_DCIP3:
		return SRGB2DCIP3;
	case VS_CCM_USER_DEF:
		return NULL;
	default:
		break;
	}
	return NULL;
}

void vs_dc_cal_ccm_coef(int32_t *coef, int32_t *offset, enum drm_vs_ccm_mode mode, uint32_t ccm_bit)
{
	uint32_t i = 0;
	const float *temp = NULL;

	if (VS_CCM_USER_DEF != mode) {
		temp = vs_dc_get_ccm_coef(mode);
		for (i = 0; i < 9; i++)
			coef[i] = (int32_t)(temp[i] * (1 << ccm_bit) + 0.5f);
		for (i = 9; i < VS_MAX_GAMUT_COEF_NUM; i++)
			offset[i - 9] = (int32_t)(temp[i] * (1 << ccm_bit) + 0.5f);
	}
}

struct drm_vs_color vs_dpu_color_to_struct(uint32_t color, bool is_yuv)
{
	struct drm_vs_color convert_color = { 0, 0, 0, 0 };

	if (!is_yuv) {
		/* The default format is ARGB888 */
		convert_color.a = (uint8_t)((color & 0xFF000000) >> 24);
		convert_color.r = (uint8_t)((color & 0x00FF0000) >> 16);
		convert_color.g = (uint8_t)((color & 0x0000FF00) >> 8);
		convert_color.b = (uint8_t)((color & 0x000000FF) >> 0);
	} else {
		/* Currently available for YUV semi-planar formats */
		convert_color.a = (uint16_t)((color & 0xFFFF0000) >> 16);
		convert_color.r = (uint16_t)(color & 0x0000FFFF);
	}

	return convert_color;
}

void ssr3_get_ratio_and_shift(int ssr50_51DepWid, int ssr5Size, int *ratio, int *shift)
{
	double wid_ratio_full = 0.0;
	double ratio_tmp = 0.0;
	int ratio_shifted = 0;

	wid_ratio_full = ((16 * 4096 * 256 * (double)ssr50_51DepWid) / (double)ssr5Size);
	ratio_tmp = wid_ratio_full;
	ratio_shifted = wid_ratio_full;
	*shift = 0;
	ratio_shifted = ratio_shifted & 0x1FFFFFF;
	while (ratio_shifted < 0x1000000) {
		ratio_tmp = ratio_tmp * 2;
		ratio_shifted = ratio_shifted << 1;
		*shift = *shift + 1;
	}

	*ratio = ratio_tmp + 1;
}

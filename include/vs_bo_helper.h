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

#ifndef __VS_BO_HELPER_H__
#define __VS_BO_HELPER_H__

#include <drm/vs_drm.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define VS_PI 3.14159265358979323846f
#define VS_MAX(a, b) ((a) > (b) ? (a) : (b))
#define VS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define VS_MATH_SINE(X) (float)(sinf((X)))
#define VS_MATH_MULTIPLY(X, Y) (float)((X) * (Y))
#define VS_MATH_INT2FLOAT(X) (float)(X)
#define VS_MATH_DIVIDE(X, Y) (float)((X) / (Y))
#define VS_IS_POWER_OF_TWO(X) ((X) != 0 && (((X) & ((X)-1)) == 0))

typedef struct drm_vs_bo_param {
	uint32_t width;
	uint32_t height;
	uint8_t bpp;

	/* header resource size of PVRIC */
	uint32_t header_size;
	/* tile status buffer size of DEC400 */
	uint32_t ts_buf_size;
} drm_vs_bo_param;

typedef enum _vs_display_size_type {
	VS_DISPLAY_640_480_60,
	VS_DISPLAY_720_1612_60,
	VS_DISPLAY_1080_2400_60,
	VS_DISPLAY_1280_720_60,
	VS_DISPLAY_1920_1080_60,
	VS_DISPLAY_1440_3520_120,
	VS_DISPLAY_1440_3216_60,
	VS_DISPLAY_1440_3360_120,
	VS_DISPLAY_1440_3520_144,
	VS_DISPLAY_1920_1080_120,
	VS_DISPLAY_2700_2600_120,
	VS_DISPLAY_2700_2600_144,
	VS_DISPLAY_2500_2820_120,
	VS_DISPLAY_2500_2820_144,
	VS_DISPLAY_2340_3404_120,
	VS_DISPLAY_3200_1920_120,
	VS_DISPLAY_3840_2160_60,
	VS_DISPLAY_3840_2160_120,
	VS_DISPLAY_7680_4320_30,
} vs_display_size_type;

typedef enum _vs_display_id {
	VS_DISPLAY_0,
	VS_DISPLAY_1,
	VS_DISPLAY_2,
	VS_DISPLAY_3,
	VS_DISPLAY_4,
	VS_DISPLAY_COUNT,
} vs_display_id;

typedef enum _vs_status {
	VS_STATUS_FAILED = -2,
	VS_STATUS_INVALID_ARGUMENTS = -1,
	VS_STATUS_OK = 0,
} vs_status;

typedef enum drm_vs_data_trans_mode {
	DRM_VS_EOTF_PQ,
	DRM_VS_EOTF_DEGAMMA,
	DRM_VS_EOTF_SRGB,
	DRM_VS_OETF_PQ,
	DRM_VS_OETF_REGAMMA,
	DRM_VS_OETF_SRGB,
} drm_vs_data_trans_mode;

int drm_vs_init_data_trans_entry(drm_vs_data_trans_mode mode, float exp, int in_bit, int out_bit,
				 uint32_t seg_count, uint32_t *seg_point, uint32_t *seg_step,
				 uint32_t *data);

int drm_vs_init_gamma_lut(int new_gamma, const char *curve_type, double gamma_value,
			  int gamma_bit_out, int gamma_entry_cnt, struct drm_color_lut *lut);

int drm_vs_init_degamma_curve(uint32_t in_bit, uint32_t out_bit, uint32_t entry_cnt, float exp,
			      uint32_t *data);

int drm_vs_init_gamma_curve(uint32_t in_bit, uint32_t out_bit, uint32_t entry_cnt, float exp,
			    uint32_t *data);

int vs_mod_config(uint32_t format, uint64_t mod, uint32_t num_planes, uint64_t modifiers[4]);

uint16_t vs_get_dec_tile_size(uint8_t tile_mode, uint8_t bpp);

/*
 * Get tile height of each plane accoring to
 * input format and tile mode/dec tile mode.
 *
 * @format: 4CC format identifier (DRM_FORMAT_*).
 *
 * @mod: the modifier value.
 *
 * @height: pointer to tile height of each plane.
 *          height[0] for RGB/Y/YUV plane,
 *          height[1] for U/UV plane,
 *          height[2] for V plane.
 */
void drm_vs_get_tile_height(uint32_t format, uint64_t mod, int *height);

/*
 * Get aligned width and height accoring to
 * input format and tile mode/dec tile mode.
 *
 * @width: pointer to aligned width.
 *
 * @height: pointer to aligned height.
 *
 * @format: 4CC format identifier (DRM_FORMAT_*).
 *
 * @mod: the modifier value.
 */
int drm_vs_get_align_size(uint32_t *width, uint32_t *height, uint32_t format, uint64_t mod);

/*
 * Prepare parameter values required by DRM_IOCTL_MODE_CREATE_DUMB
 * for each plane
 *
 * @width: aligned width obtained by drm_vs_get_align_size.
 *
 * @height: aligned height obtained by drm_vs_get_align_size.
 *
 * @format: 4CC format identifier (DRM_FORMAT_*).
 *
 * @mod: the modifier value.
 *
 * @bo_param: point to drm_vs_bo_param object for each plane.
 */
int drm_vs_bo_config(uint32_t width, uint32_t height, uint32_t format, uint64_t mod,
		     drm_vs_bo_param bo_param[4]);
/*
+ * Prepare ltm freq_decomp norm parameter values
+ * for ltm freq_decomp norm
+ *
+ * @coef: point to drm_vs_ltm_freq_decomp coef.
+ * @size: the numbers of coef data.
+*/
uint32_t drm_vs_get_ltm_norm(uint16_t *coef, uint32_t size);

vs_status drm_vs_select_display(vs_display_id display_id);
vs_status drm_vs_display_set_timing(vs_display_size_type type);
vs_status drm_vs_get_ltm_cd_params(struct drm_vs_ltm_cd_set *cd_set);
vs_status drm_vs_get_ltm_luma_ave_params(uint16_t margin_x, uint16_t margin_y,
					 struct drm_vs_ltm_luma_ave *luma_params);
vs_status drm_vs_get_ltm_ds_params(struct drm_vs_rect *cropped, struct drm_vs_rect *output,
				   struct drm_vs_ltm_ds *ds_params);

uint32_t drm_vs_get_stretch_factor(uint32_t src_size, uint32_t dst_size, bool scale_factor_set);
uint32_t drm_vs_get_stretch_initOffset(uint32_t stretch_factor, bool scale_factor_set);

vs_status drm_vs_calculate_sync_table(uint8_t kernel_size, uint32_t src_size, uint32_t dst_size,
				      int16_t *coef, uint32_t filter);
void drm_vs_get_filter_tap(enum drm_vs_filter_type filter, uint8_t *tap_h, uint8_t *tap_v);
enum drm_vs_filter_type drm_vs_get_info_filter_type(uint8_t filter_type_mask);
const char *drm_vs_get_info_filter_name(enum drm_vs_filter_type filter_type);

/*
 * Calibrate size required by DRM_IOCTL_MODE_CREATE_DUMB
 *
 * @bo_param: point to drm_vs_bo_param object.
 *
 * @modifier: modifier for this buffer.
 *
 * @format: 4CC format identifier (DRM_FORMAT_*).
 *
 */
void drm_vs_calibrate_bo_size(drm_vs_bo_param *bo_param, uint64_t modifier, uint32_t format);

const float *vs_dc_get_ccm_coef(enum drm_vs_ccm_mode mode);
void vs_dc_cal_ccm_coef(int32_t *coef, int32_t *offset, enum drm_vs_ccm_mode mode,
			uint32_t ccm_bit);

/* convert the input data of U32 to color data */
struct drm_vs_color vs_dpu_color_to_struct(uint32_t color, bool is_yuv);

void ssr3_get_ratio_and_shift(int ssr50_51DepWid, int ssr5Size, int *ratio, int *shift);

#endif /* __VS_BO_HELPER_H__ */

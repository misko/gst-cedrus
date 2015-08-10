/*
 * Copyright (c) 2014-2015 Jens Kuske <jenskuske@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef __H264ENC_H__
#define __H264ENC_H__

struct h264enc_params {
	unsigned int width;
	unsigned int height;

	unsigned int src_width;
	unsigned int src_height;
	enum color_format { H264_FMT_NV12 = 0, H264_FMT_NV16 = 1 } src_format;

	unsigned int profile_idc, level_idc;

	enum { H264_EC_CAVLC = 0, H264_EC_CABAC = 1 } entropy_coding_mode;

	unsigned int qp;

	unsigned int keyframe_interval;
};
struct h264enc_internal {
	unsigned int mb_width, mb_height, mb_stride;
	unsigned int crop_right, crop_bottom;

	uint8_t *luma_buffer, *chroma_buffer;
	unsigned int input_buffer_size;
	enum color_format input_color_format;

	uint8_t *bytestream_buffer;
	unsigned int bytestream_buffer_size;
	unsigned int bytestream_length;

	struct h264enc_ref_pic {
		void *luma_buffer, *chroma_buffer;
		void *extra_buffer; /* unknown purpose, looks like smaller luma */
	} ref_picture[2];

	void *extra_buffer_line, *extra_buffer_frame; /* unknown purpose */

	void *regs;

	unsigned int write_sps_pps;

	unsigned int profile_idc, level_idc, constraints;

	unsigned int entropy_coding_mode_flag;
	unsigned int pic_init_qp;

	unsigned int keyframe_interval;

	uint32_t sps_id;
	uint32_t pps_id;
	int32_t d_qp;

	unsigned int current_frame_num;
	enum slice_type { SLICE_P = 0, SLICE_I = 2 } current_slice_type;

};

typedef struct h264enc_internal h264enc;

h264enc *h264enc_new(const struct h264enc_params *p);
void h264enc_free(h264enc *c);
void *h264enc_get_input_buffer(const h264enc *c);
void *h264enc_get_bytestream_buffer(const h264enc *c);
unsigned int h264enc_get_bytestream_length(const h264enc *c);
int h264enc_encode_picture(h264enc *c);

#endif

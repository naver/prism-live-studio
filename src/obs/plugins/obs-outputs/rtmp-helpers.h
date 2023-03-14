/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include "librtmp/rtmp.h"

static inline AVal *flv_str(AVal *out, const char *str)
{
	out->av_val = (char *)str;
	out->av_len = (int)strlen(str);
	return out;
}

static inline void enc_num_val(char **enc, char *end, const char *name,
			       double val)
{
	AVal s;
	*enc = AMF_EncodeNamedNumber(*enc, end, flv_str(&s, name), val);
}

static inline void enc_bool_val(char **enc, char *end, const char *name,
				bool val)
{
	AVal s;
	*enc = AMF_EncodeNamedBoolean(*enc, end, flv_str(&s, name), val);
}

static inline void enc_str_val(char **enc, char *end, const char *name,
			       const char *val)
{
	AVal s1, s2;
	*enc = AMF_EncodeNamedString(*enc, end, flv_str(&s1, name),
				     flv_str(&s2, val));
}

static inline void enc_str(char **enc, char *end, const char *str)
{
	AVal s;
	*enc = AMF_EncodeString(*enc, end, flv_str(&s, str));
}

//PRISM/LiuHaibin/20200915/#4748/add id3v2
static inline AVal *flv_str_ex(AVal *out, const char *str, int len)
{
	out->av_val = (char *)str;
	out->av_len = len;
	return out;
}

//PRISM/LiuHaibin/20200915/#4748/add id3v2
static inline void enc_str_ex(char **enc, char *end, const char *str, int len)
{
	AVal s;
	*enc = AMF_EncodeString(*enc, end, flv_str_ex(&s, str, len));
}

static inline void enc_array_val(char **enc, char *end, const char *name,
				 const char **val)
{
	AVal s1, s2;
	AMFObjectProperty property[BTRS_IMMERSIVE_TRAKCS];
	AMFObjectProperty element = {.p_name = NULL,
				     .p_type = AMF_NULL,
				     .p_vu = {.p_number = 1,
					      .p_aval = AMF_NULL},
				     .p_UTCoffset = 0};
	flv_str(&s1, name);
	*enc = AMF_EncodeInt16(*enc, end, s1.av_len);
	memcpy(*enc, s1.av_val, s1.av_len);
	*enc += s1.av_len;
	for (int i = 0; i < BTRS_IMMERSIVE_TRAKCS; i++) {
		flv_str(&s2, *val);
		if (!strcmp(*val, "")) {
			element.p_type = AMF_NULL;
			element.p_vu.p_number = 1;
			element.p_vu.p_aval = s2;

		} else {
			element.p_type = AMF_STRING;
			element.p_vu.p_number = 1;
			element.p_vu.p_aval = s2;
		}
		property[i] = element;
		*val++;
	}
	AMFObject object = {.o_num = BTRS_IMMERSIVE_TRAKCS,
			    .o_props = &property};
	*enc = AMF_EncodeArray(&object, *enc, end);
}

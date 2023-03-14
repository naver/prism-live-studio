#pragma once

#include "util/c99defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HEVC_KEYFRAME_SEC 1 // in seconds
#define HEVC_DEFAULT_BITRATT 6000 // in Kbps
#define HEVC_DEFAULT_CQP 32
#define HEVC_DEFAULT_CRF 28

enum {
	PLS_NAL_UNIT_CODED_SLICE_TRAIL_N = 0,
	PLS_NAL_UNIT_CODED_SLICE_TRAIL_R,
	PLS_NAL_UNIT_CODED_SLICE_TSA_N,
	PLS_NAL_UNIT_CODED_SLICE_TLA_R,
	PLS_NAL_UNIT_CODED_SLICE_STSA_N,
	PLS_NAL_UNIT_CODED_SLICE_STSA_R,
	PLS_NAL_UNIT_CODED_SLICE_RADL_N,
	PLS_NAL_UNIT_CODED_SLICE_RADL_R,
	PLS_NAL_UNIT_CODED_SLICE_RASL_N,
	PLS_NAL_UNIT_CODED_SLICE_RASL_R,
	PLS_NAL_UNIT_CODED_SLICE_BLA_W_LP = 16,
	PLS_NAL_UNIT_CODED_SLICE_BLA_W_RADL,
	PLS_NAL_UNIT_CODED_SLICE_BLA_N_LP,
	PLS_NAL_UNIT_CODED_SLICE_IDR_W_RADL,
	PLS_NAL_UNIT_CODED_SLICE_IDR_N_LP,
	PLS_NAL_UNIT_CODED_SLICE_CRA,
	PLS_NAL_UNIT_VPS = 32,
	PLS_NAL_UNIT_SPS,
	PLS_NAL_UNIT_PPS,
	PLS_NAL_UNIT_ACCESS_UNIT_DELIMITER,
	PLS_NAL_UNIT_EOS,
	PLS_NAL_UNIT_EOB,
	PLS_NAL_UNIT_FILLER_DATA,
	PLS_NAL_UNIT_PREFIX_SEI,
	PLS_NAL_UNIT_SUFFIX_SEI,
	PLS_NAL_UNIT_UNSPECIFIED = 62,
	PLS_NAL_UNIT_INVALID = 64,
};

EXPORT bool obs_hevc_keynal(int naltype);

EXPORT bool obs_hevc_keyframe(const uint8_t *data, size_t size);

EXPORT size_t obs_parse_hevc_header(uint8_t **header, const uint8_t *data,
				    size_t size);

EXPORT void obs_extract_hevc_headers(const uint8_t *packet, size_t size,
				     uint8_t **new_packet_data,
				     size_t *new_packet_size,
				     uint8_t **header_data, size_t *header_size,
				     uint8_t **sei_data, size_t *sei_size);

#ifdef __cplusplus
}
#endif

#include "obs.h"
#include "obs-hevc.h"
#include "util/array-serializer.h"
#include "obs-avc.h"

struct HEVCDecoderConfigurationRecord {
	uint8_t configurationVersion;
	uint8_t general_profile_space;
	uint8_t general_tier_flag;
	uint8_t general_profile_idc;
	uint32_t general_profile_compatibility_flags;
	uint64_t general_constraint_indicator_flags;
	uint8_t general_level_idc;

	uint16_t min_spatial_segmentation_idc;
	uint8_t parallelismType;
	uint8_t chromaFormat;

	uint8_t bitDepthLumaMinus8;
	uint8_t bitDepthChromaMinus8;

	uint16_t avgFrameRate;
	uint8_t constantFrameRate;

	uint8_t numTemporalLayers;
	uint8_t temporalIdNested;
	uint8_t lengthSizeMinusOne;

	uint8_t numOfArrays;
};

bool obs_hevc_keynal(int naltype)
{
	switch (naltype) {
	case PLS_NAL_UNIT_CODED_SLICE_BLA_W_LP:
	case PLS_NAL_UNIT_CODED_SLICE_BLA_W_RADL:
	case PLS_NAL_UNIT_CODED_SLICE_BLA_N_LP:
	case PLS_NAL_UNIT_CODED_SLICE_IDR_W_RADL:
	case PLS_NAL_UNIT_CODED_SLICE_IDR_N_LP:
	case PLS_NAL_UNIT_CODED_SLICE_CRA:
		return true;

	default:
		return false;
	}
}

bool obs_hevc_keyframe(const uint8_t *data, size_t size)
{
	const uint8_t *nal_start, *nal_end;
	const uint8_t *end = data + size;
	int type;

	nal_start = obs_avc_find_startcode(data, end);
	while (true) {
		while (nal_start < end && !*(nal_start++))
			;

		if (nal_start == end)
			break;

		short nalu_header = nal_start[0];
		type = (nalu_header & 0x7E) >> 1;

		if (obs_hevc_keynal(type)) {
			return true;
		}

		nal_end = obs_avc_find_startcode(nal_start, end);
		nal_start = nal_end;
	}

	return false;
}

static inline bool has_start_code(const uint8_t *data)
{
	if (data[0] != 0 || data[1] != 0)
		return false;

	return data[2] == 1 || (data[2] == 0 && data[3] == 1);
}

static void get_vps_sps_pps(const uint8_t *data, size_t size,
			    const uint8_t **vps, size_t *vps_size,
			    const uint8_t **sps, size_t *sps_size,
			    const uint8_t **pps, size_t *pps_size)
{
	const uint8_t *nal_start, *nal_end;
	const uint8_t *end = data + size;

	nal_start = obs_avc_find_startcode(data, end);
	while (true) {
		while (nal_start < end && !*(nal_start++))
			;

		if (nal_start == end)
			break;

		nal_end = obs_avc_find_startcode(nal_start, end);

		short nalu_header = nal_start[0];
		int type = (nalu_header & 0x7E) >> 1;

		if (type == PLS_NAL_UNIT_VPS) {
			*vps = nal_start;
			*vps_size = nal_end - nal_start;
		} else if (type == PLS_NAL_UNIT_SPS) {
			*sps = nal_start;
			*sps_size = nal_end - nal_start;
		} else if (type == PLS_NAL_UNIT_PPS) {
			*pps = nal_start;
			*pps_size = nal_end - nal_start;
		}

		nal_start = nal_end;
	}
}

size_t obs_parse_hevc_header(uint8_t **header, const uint8_t *data, size_t size)
{
	struct array_output_data output;
	struct serializer s;

	uint8_t *vps = NULL, *sps = NULL, *pps = NULL;
	size_t vps_size = 0, sps_size = 0, pps_size = 0;

	struct HEVCDecoderConfigurationRecord mHvcc;
	memset(&mHvcc, 0, sizeof(mHvcc));
	mHvcc.configurationVersion = 1;
	mHvcc.numTemporalLayers = 1;
	mHvcc.lengthSizeMinusOne = 3;
	mHvcc.numOfArrays = 3;

	if (size <= 6) {
		assert(false);
		return 0;
	}

	if (!has_start_code(data)) {
		*header = bmemdup(data, size);
		return size;
	}

	get_vps_sps_pps(data, size, &vps, &vps_size, &sps, &sps_size, &pps,
			&pps_size);
	if (!vps || !sps || !pps || sps_size < 4) {
		assert(false);
		return 0;
	}

	array_output_serializer_init(&s, &output);

	// unsigned int(8) configurationVersion = 1;
	s_w8(&s, mHvcc.configurationVersion);

	// unsigned int(2) general_profile_space;
	// unsigned int(1) general_tier_flag;
	// unsigned int(5) general_profile_idc;
	s_w8(&s, mHvcc.general_profile_space << 6 |
			 mHvcc.general_tier_flag << 5 |
			 mHvcc.general_profile_idc);

	// unsigned int(32) general_profile_compatibility_flags;
	s_wb32(&s, mHvcc.general_profile_compatibility_flags);

	// unsigned int(48) general_constraint_indicator_flags;
	s_wb32(&s, mHvcc.general_constraint_indicator_flags >> 16);
	s_wb16(&s, mHvcc.general_constraint_indicator_flags);

	// unsigned int(8) general_level_idc;
	s_w8(&s, mHvcc.general_level_idc);

	// bit(4) reserved = ‘1111’b;
	// unsigned int(12) min_spatial_segmentation_idc;
	s_wb16(&s, mHvcc.min_spatial_segmentation_idc | 0xf000);

	// bit(6) reserved = ‘111111’b;
	// unsigned int(2) parallelismType;
	s_w8(&s, mHvcc.parallelismType | 0xfc);

	// bit(6) reserved = ‘111111’b;
	// unsigned int(2) chromaFormat;
	s_w8(&s, mHvcc.chromaFormat | 0xfc);

	// bit(5) reserved = ‘11111’b;
	// unsigned int(3) bitDepthLumaMinus8;
	s_w8(&s, mHvcc.bitDepthLumaMinus8 | 0xf8);

	// bit(5) reserved = ‘11111’b;
	// unsigned int(3) bitDepthChromaMinus8;
	s_w8(&s, mHvcc.bitDepthChromaMinus8 | 0xf8);

	// bit(16) avgFrameRate;
	s_wb16(&s, mHvcc.avgFrameRate);

	// bit(2) constantFrameRate;
	// bit(3) numTemporalLayers;
	// bit(1) temporalIdNested;
	// unsigned int(2) lengthSizeMinusOne;
	s_w8(&s, mHvcc.constantFrameRate << 6 | mHvcc.numTemporalLayers << 3 |
			 mHvcc.temporalIdNested << 2 |
			 mHvcc.lengthSizeMinusOne);

	// unsigned int(8) numOfNalUnitType (VPS/SPS/PPS or VPS/SPS/PPS/SEI_PREFIX);
	s_w8(&s, mHvcc.numOfArrays);

	// VPS
	// bit(1) array_completeness = 0;
	// unsigned int(1) reserved = 0;
	// unsigned int(6) NAL_unit_type = 0x20;
	s_w8(&s, (0 << 7) | (0x20 & 0x3f));
	s_wb16(&s, 1);        // number of vps
	s_wb16(&s, vps_size); // vps size
	s_write(&s, vps, vps_size);

	// SPS
	// bit(1) array_completeness = 0;
	// unsigned int(1) reserved = 0;
	// unsigned int(6) NAL_unit_type = 0x21;
	s_w8(&s, (0 << 7) | (0x21 & 0x3f));
	s_wb16(&s, 1);        // number of sps
	s_wb16(&s, sps_size); // sps size
	s_write(&s, sps, sps_size);

	// PPS
	// bit(1) array_completeness = 0;
	// unsigned int(1) reserved = 0;
	// unsigned int(6) NAL_unit_type = 0x22;
	s_w8(&s, (0 << 7) | (0x22 & 0x3f));
	s_wb16(&s, 1);        // number of pps
	s_wb16(&s, pps_size); // pps size
	s_write(&s, pps, pps_size);

	*header = output.bytes.array;
	return output.bytes.num;
}

void obs_extract_hevc_headers(const uint8_t *packet, size_t size,
			      uint8_t **new_packet_data,
			      size_t *new_packet_size, uint8_t **header_data,
			      size_t *header_size, uint8_t **sei_data,
			      size_t *sei_size)
{
	DARRAY(uint8_t) new_packet;
	DARRAY(uint8_t) header;
	DARRAY(uint8_t) sei;
	const uint8_t *nal_start, *nal_end, *nal_codestart;
	const uint8_t *end = packet + size;
	int type;

	da_init(new_packet);
	da_init(header);
	da_init(sei);

	nal_start = obs_avc_find_startcode(packet, end);
	nal_end = NULL;
	while (nal_end != end) {
		nal_codestart = nal_start;

		while (nal_start < end && !*(nal_start++))
			;

		if (nal_start == end)
			break;

		short nalu_header = nal_start[0];
		type = (nalu_header & 0x7E) >> 1;

		nal_end = obs_avc_find_startcode(nal_start, end);
		if (!nal_end)
			nal_end = end;

		switch (type) {
		case PLS_NAL_UNIT_VPS:
		case PLS_NAL_UNIT_SPS:
		case PLS_NAL_UNIT_PPS:
			da_push_back_array(header, nal_codestart,
					   nal_end - nal_codestart);
			break;

		case PLS_NAL_UNIT_PREFIX_SEI:
		case PLS_NAL_UNIT_SUFFIX_SEI:
			da_push_back_array(sei, nal_codestart,
					   nal_end - nal_codestart);
			break;

		default:
			da_push_back_array(new_packet, nal_codestart,
					   nal_end - nal_codestart);
			break;
		}

		nal_start = nal_end;
	}

	*new_packet_data = new_packet.array;
	*new_packet_size = new_packet.num;
	*header_data = header.array;
	*header_size = header.num;
	*sei_data = sei.array;
	*sei_size = sei.num;
}

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define CK_CAPTURE "ck_capture"
#define CK_COLOR "ck_color"
#define CK_COLOR_TYPE "ck_color_type"
#define CK_SIMILARITY "ck_similarity"
#define CK_SMOOTH "ck_smooth"
#define CK_SPILL "ck_spill"
#define CK_OPACITY "ck_opacity"
#define CK_CONTRAST "ck_contrast"
#define CK_BRIGHTNESS "ck_brightness"
#define CK_GAMMA "ck_gamma"
#define GROUND_BLUR "blur"
#define GROUND_BLUR_ENABLE "blur_enable"
#define GROUND_ORIGINAL "original_background"
#define GROUND_SELECT_TYPE "background_select_type"
#define GROUND_BG_ITEM_TYPE "bg_type"
#define GROUND_PATH "path"
#define FORE_GROUND_PATH "foreground_path"
#define GROUND_STOPMOTION_PATH "stop_motion_file_path"
#define GROUND_SRC_RECT_X "src_rect_x"
#define GROUND_SRC_RECT_Y "src_rect_y"
#define GROUND_SRC_RECT_CX "src_rect_cx"
#define GROUND_SRC_RECT_CY "src_rect_cy"
#define GROUND_DST_RECT_X "dst_rect_x"
#define GROUND_DST_RECT_Y "dst_rect_y"
#define GROUND_DST_RECT_CX "dst_rect_cx"
#define GROUND_DST_RECT_CY "dst_rect_cy"
#define GROUND_MOTION "motion"
#define GROUND_H_FLIP "horizontal_flip"
#define GROUND_V_FLIP "vertical_flip"
#define GROUND_IS_TEMP_ORIGINAL "is_temp_origin"
#define GROUND_UI_MOTION_ID "ui_motion_id"
#define GROUND_THUMBNAIL_FILE_PATH "thumbnail_file_path"
#define FORE_GROUND_STATIC_PATH "foreground_static_path"
#define GROUND_IS_PRISM_RESOURCE "ui_is_prism_resource"

//for action log
#define VB_ACTIONLOG_MODULE "virtual_background"
#define VB_ACTIONLOG_REMOVE "just_remove"
#define VB_ACTIONLOG_MOTION "motion"
#define VB_ACTIONLOG_STATIC "static"
#define VB_ACTIONLOG_CUSTOM "custom"

enum ITEM_TYPE {
	ITEM_TYPE_UNKNOWN,
	ITEM_TYPE_IMAGE,
	ITEM_TYPE_VIDEO,
	ITEM_TYPE_WEB,
};

enum GROUND_TYPE {
	GROUND_TYPE_BG = 0, //background
	GROUND_TYPE_FG,     //foreground
};

enum GROUND_SEG_TYPE {
	GROUND_SEG_TYPE_UNKNOWN = 0, //error type
	GROUND_SEG_TYPE_ORIGINAL,    //original background
	GROUND_SEG_TYPE_DEL_BG,      //delete background, transparent alpha
	GROUND_SEG_TYPE_ADD_BG,      //add image/video/web background
};

//range: [0, 1]
struct item_rect {
	float x;  //offset x [0, 1]
	float y;  //offset y [0, 1]
	float cx; //width [0, 1]
	float cy; //height [0, 1]
	item_rect()
	{
		x = 0.0;
		y = 0.0;
		cx = 1.0;
		cy = 1.0;
	}
};

struct item_private_data {
	//item type, image, video, web
	ITEM_TYPE item_type;

	//which part of the source should be rendered to dst
	item_rect rt_source;

	//the position where the source to be rendered to
	item_rect rt_target;

	//file path for video or image, url for web
	std::string path;

	//file path for video motion disabled
	std::string stop_moiton_path;

	//if item need to be flipped vertically
	bool v_flip;

	//if item need to be flipped horizontally
	bool h_flip;

	//motion
	bool motion;

	//if background item needed
	bool enable;

	//ui index
	std::string ui_motion_id;

	//thumbnail file path
	std::string thumbnail_file_path;

	//if the background file is prism resource
	bool is_prism_resource;

	item_private_data()
	{
		item_type = ITEM_TYPE_UNKNOWN;
		v_flip = false;
		h_flip = false;
		rt_source.x = 0.0;
		rt_source.y = 0.0;
		rt_source.cx = 1.0;
		rt_source.cy = 1.0;
		rt_target.x = 0.0;
		rt_target.y = 0.0;
		rt_target.cx = 1.0;
		rt_target.cy = 1.0;
		path = "";
		v_flip = false;
		h_flip = false;
		enable = false;
		ui_motion_id = "";
	}
};

struct ground_private_data {
	bool blur_enable;
	int blur;
	GROUND_SEG_TYPE ground_seg_type;
	int temp_origin;
	bool enable;
	ground_private_data()
	{
		ground_seg_type = GROUND_SEG_TYPE_ORIGINAL;
		blur = 0;
		blur_enable = false;
		temp_origin = 0;
		enable = true;
	}
};

typedef void (*ground_exception)(void *, int, int);

#ifdef __cplusplus
}
#endif

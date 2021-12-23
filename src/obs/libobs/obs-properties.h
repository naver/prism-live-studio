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

#include "util/c99defs.h"
#include "obs-data.h"
#include "media-io/frame-rate.h"

/**
 * @file
 * @brief libobs header for the properties system used in libobs
 *
 * @page properties Properties
 * @brief Platform and Toolkit independent settings implementation
 *
 * @section prop_overview_sec Overview
 *
 * libobs uses a property system which lets for example sources specify
 * settings that can be displayed to the user by the UI.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Only update when the user presses OK or Apply */
#define OBS_PROPERTIES_DEFER_UPDATE (1 << 0)

enum obs_property_type {
	OBS_PROPERTY_INVALID,
	OBS_PROPERTY_BOOL,
	OBS_PROPERTY_BOOL_GROUP,
	OBS_PROPERTY_INT,
	OBS_PROPERTY_FLOAT,
	OBS_PROPERTY_TEXT,
	OBS_PROPERTY_PATH,
	OBS_PROPERTY_LIST,
	OBS_PROPERTY_COLOR,
	OBS_PROPERTY_BUTTON,

	OBS_PROPERTY_FONT,
	OBS_PROPERTY_EDITABLE_LIST,
	OBS_PROPERTY_FRAME_RATE,
	OBS_PROPERTY_GROUP,

	//PRISM/Wangshaohui/20200914/Noissue/for add new property
	OBS_PROPERTY_PRISM_BASE = 5000,

	//PRISM/Liuying/20200617/No issue/for the same row of buttons
	OBS_PROPERTY_BUTTON_GROUP,

	//PRISM/Liuying/20200617/new bgm ux
	OBS_PROPERTY_BGM_MUSIC_LIST,
	OBS_PROPERTY_TIPS,

	//PRISM/Zhangdewen/20200901/feature/for chat source
	OBS_PROPERTY_CHAT_TEMPLATE_LIST,
	OBS_PROPERTY_CHAT_FONT_SIZE,

	//PRISM/Chengbing/20200902/feature/for text motion source
	OBS_PROPERTY_TM_TEXT_CONTENT,
	OBS_PROPERTY_TM_TAB,
	OBS_PROPERTY_TM_TEMPLATE_TAB,
	OBS_PROPERTY_TM_TEMPLATE_LIST,
	OBS_PROPERTY_TM_TEXT,
	OBS_PROPERTY_TM_COLOR,
	OBS_PROPERTY_TM_MOTION,

	//PRISM/Wangshaohui/20200914/Noissue/region source
	OBS_PROPERTY_REGION_SELECT,

	//PRISM/Zengqin/20201021/Noissue/spectralizer source
	OBS_PROPERTY_IMAGE_GROUP,

	//PRISM/Zengqin/20201023/Noissue/custom group
	OBS_PROPERTY_CUSTOM_GROUP,

	//PRISM/Zengqin/20201027/Noissue/horizontal line
	OBS_PROPERTY_H_LINE,

	//PRISM/Zengqin/20201030/Noissue/checkbox text is on left
	OBS_PROPERTY_BOOL_LEFT,

	//PRISM/Zhangdewen/20201022/Noissue/virtual background
	OBS_PROPERTY_CAMERA_VIRTUAL_BACKGROUND_STATE,
	OBS_PROPERTY_VIRTUAL_BACKGROUND_RESOURCE,
	OBS_PROPERTY_SWITCH,

	//PRSIM/WuLongyue/20200915/No issue/For PRISM Mobile
	OBS_PROPERTY_MOBILE_GUIDER,
	OBS_PROPERTY_MOBILE_HELP,
	OBS_PROPERTY_MOBILE_NAME,
	OBS_PROPERTY_MOBILE_STATUS,
	OBS_PROPERTY_PRIVATE_DATA_TEXT,

	//PRISM/Xiewei/20210629/No issue/For Camera Flip
	OBS_PROPERTY_CHECKBOX_GROUP,

	//PRSIM/Renjinbo/20210518/timer feature/add right spinbox box group, like the OBS_PROPERTY_BOOL_GROUP ui.
	OBS_PROPERTY_INT_GROUP,
	OBS_PROPERTY_FONT_SIMPLE,
	OBS_PROPERTY_COLOR_CHECKBOX,
	OBS_PROPERTY_TIMER_LIST_LISTEN,
	OBS_PROPERTY_LABEL_TIP,
};

enum obs_combo_format {
	OBS_COMBO_FORMAT_INVALID,
	OBS_COMBO_FORMAT_INT,
	OBS_COMBO_FORMAT_FLOAT,
	OBS_COMBO_FORMAT_STRING,
};

enum obs_combo_type {
	OBS_COMBO_TYPE_INVALID,
	OBS_COMBO_TYPE_EDITABLE,
	OBS_COMBO_TYPE_LIST,
};

enum obs_editable_list_type {
	OBS_EDITABLE_LIST_TYPE_STRINGS,
	OBS_EDITABLE_LIST_TYPE_FILES,
	OBS_EDITABLE_LIST_TYPE_FILES_AND_URLS,
};

enum obs_path_type {
	OBS_PATH_FILE,
	OBS_PATH_FILE_SAVE,
	OBS_PATH_DIRECTORY,
};

enum obs_text_type {
	OBS_TEXT_DEFAULT,
	OBS_TEXT_PASSWORD,
	OBS_TEXT_MULTILINE,
	OBS_TEXT_DEFAULT_LIMIT,
};

enum obs_number_type {
	OBS_NUMBER_SCROLLER,
	OBS_NUMBER_SLIDER,
};

enum obs_group_type {
	OBS_COMBO_INVALID,
	OBS_GROUP_NORMAL,
	OBS_GROUP_CHECKABLE,
};

enum obs_control_type {
	OBS_CONTROL_UNKNOWN,
	OBS_CONTROL_INT,
};

enum obs_image_style_type {
	OBS_IMAGE_STYLE_UNKNOWN = -1,
	OBS_IMAGE_STYLE_TEMPLATE,
	OBS_IMAGE_STYLE_SOLID_COLOR,
	OBS_IMAGE_STYLE_GRADIENT_COLOR,
	OBS_IMAGE_STYLE_BORDER_BUTTON, //svg width=height = 34
	OBS_IMAGE_STYLE_APNG_BUTTON,   //APNG
};

#define OBS_FONT_BOLD (1 << 0)
#define OBS_FONT_ITALIC (1 << 1)
#define OBS_FONT_UNDERLINE (1 << 2)
#define OBS_FONT_STRIKEOUT (1 << 3)

struct obs_properties;
struct obs_property;
typedef struct obs_properties obs_properties_t;
typedef struct obs_property obs_property_t;

/* ------------------------------------------------------------------------- */

EXPORT obs_properties_t *obs_properties_create(void);
EXPORT obs_properties_t *
obs_properties_create_param(void *param, void (*destroy)(void *param));
EXPORT void obs_properties_destroy(obs_properties_t *props);

EXPORT void obs_properties_set_flags(obs_properties_t *props, uint32_t flags);
EXPORT uint32_t obs_properties_get_flags(obs_properties_t *props);

EXPORT void obs_properties_set_param(obs_properties_t *props, void *param,
				     void (*destroy)(void *param));
EXPORT void *obs_properties_get_param(obs_properties_t *props);

EXPORT obs_property_t *obs_properties_first(obs_properties_t *props);

EXPORT obs_property_t *obs_properties_get(obs_properties_t *props,
					  const char *property);

EXPORT obs_properties_t *obs_properties_get_parent(obs_properties_t *props);

/** Remove a property from a properties list.
 *
 * Removes a property from a properties list. Only valid in either
 * get_properties or modified_callback(2). modified_callback(2) must return
 * true so that all UI properties are rebuilt and returning false is undefined
 * behavior.
 *
 * @param props Properties to remove from.
 * @param property Name of the property to remove.
 */
EXPORT void obs_properties_remove_by_name(obs_properties_t *props,
					  const char *property);

/**
 * Applies settings to the properties by calling all the necessary
 * modification callbacks
 */
EXPORT void obs_properties_apply_settings(obs_properties_t *props,
					  obs_data_t *settings);

/* ------------------------------------------------------------------------- */

/**
 * Callback for when a button property is clicked.  If the properties
 * need to be refreshed due to changes to the property layout, return true,
 * otherwise return false.
 */
typedef bool (*obs_property_clicked_t)(obs_properties_t *props,
				       obs_property_t *property, void *data);

EXPORT obs_property_t *obs_properties_add_bool(obs_properties_t *props,
					       const char *name,
					       const char *description);

EXPORT obs_property_t *obs_properties_add_bool_group(obs_properties_t *props,
						     const char *name,
						     const char *description);

EXPORT size_t obs_properties_add_bool_group_item(
	obs_property_t *p, const char *description, const char *tooltip,
	bool enabled, obs_property_clicked_t callback);

EXPORT obs_property_t *obs_properties_add_int(obs_properties_t *props,
					      const char *name,
					      const char *description, int min,
					      int max, int step);

EXPORT obs_property_t *obs_properties_add_float(obs_properties_t *props,
						const char *name,
						const char *description,
						double min, double max,
						double step);

EXPORT obs_property_t *obs_properties_add_int_slider(obs_properties_t *props,
						     const char *name,
						     const char *description,
						     int min, int max,
						     int step);

EXPORT obs_property_t *obs_properties_add_float_slider(obs_properties_t *props,
						       const char *name,
						       const char *description,
						       double min, double max,
						       double step);

EXPORT obs_property_t *obs_properties_add_text(obs_properties_t *props,
					       const char *name,
					       const char *description,
					       enum obs_text_type type);

//PRISM/WuLongyue/20201204/Get text from private data
EXPORT obs_property_t *
obs_properties_add_private_data_text(obs_properties_t *props, const char *name,
				     const char *text);

/**
 * Adds a 'path' property.  Can be a directory or a file.
 *
 * If target is a file path, the filters should be this format, separated by
 * double semi-colens, and extensions separated by space:
 *   "Example types 1 and 2 (*.ex1 *.ex2);;Example type 3 (*.ex3)"
 *
 * @param  props        Properties object
 * @param  name         Settings name
 * @param  description  Description (display name) of the property
 * @param  type         Type of path (directory or file)
 * @param  filter       If type is a file path, then describes the file filter
 *                      that the user can browse.  Items are separated via
 *                      double semi-colens.  If multiple file types in a
 *                      filter, separate with space.
 */
EXPORT obs_property_t *
obs_properties_add_path(obs_properties_t *props, const char *name,
			const char *description, enum obs_path_type type,
			const char *filter, const char *default_path);

EXPORT obs_property_t *obs_properties_add_list(obs_properties_t *props,
					       const char *name,
					       const char *description,
					       enum obs_combo_type type,
					       enum obs_combo_format format);

//PRISM/Liuying/20200706/new bgm ux
EXPORT obs_property_t *
obs_properties_add_bgm_music_list(obs_properties_t *props, const char *name,
				  const char *description);

//PRISM/Liuying/20200706/new bgm ux
EXPORT size_t obs_property_music_group_add_item(
	obs_property_t *p, const char *name, const char *producer,
	const char *url, int duration, int duration_type,
	obs_property_clicked_t callback);

//PRISM/Liuying/20200706/new bgm ux
EXPORT void obs_property_music_group_clear(obs_property_t *p);

//PRISM/Liuying/20200706/new bgm ux
EXPORT size_t obs_property_music_group_item_count(obs_property_t *p);

//PRISM/Liuying/20200706/new bgm ux
EXPORT int obs_property_music_group_item_duration(obs_property_t *p,
						  size_t idx);

//PRISM/Liuying/20200706/new bgm ux
EXPORT int obs_property_music_group_item_duration_type(obs_property_t *p,
						       size_t idx);

//PRISM/Liuying/20200706/new bgm ux
EXPORT const char *obs_property_music_group_item_name(obs_property_t *p,
						      size_t idx);

//PRISM/Liuying/20200706/new bgm ux
EXPORT const char *obs_property_music_group_item_producer(obs_property_t *p,
							  size_t idx);

//PRISM/Liuying/20200706/new bgm ux
EXPORT const char *obs_property_music_group_item_url(obs_property_t *p,
						     size_t idx);

//PRISM/Liuying/20200706/new bgm ux
EXPORT obs_property_t *obs_properties_add_tips(obs_properties_t *props,
					       const char *name,
					       const char *description);

//PRISM/Wangshaohui/20200914/Noissue/region source
EXPORT obs_property_t *obs_properties_add_region_select(obs_properties_t *props,
							const char *name,
							const char *desc);

EXPORT obs_property_t *obs_properties_add_color(obs_properties_t *props,
						const char *name,
						const char *description);

EXPORT obs_property_t *
obs_properties_add_button(obs_properties_t *props, const char *name,
			  const char *text, obs_property_clicked_t callback);

EXPORT obs_property_t *
obs_properties_add_button2(obs_properties_t *props, const char *name,
			   const char *text, obs_property_clicked_t callback,
			   void *priv);

//PRISM/Liuying/20200617/No issue/for the same row of buttons
EXPORT obs_property_t *obs_properties_add_button_group(obs_properties_t *props,
						       const char *name,
						       const char *desc);

//PRISM/Liuying/20200617/No issue/for the same row of buttons
EXPORT obs_property_t *obs_properties_add_button2_group(obs_properties_t *props,
							const char *name,
							const char *desc,
							void *priv);

//PRISM/Liuying/20200617/No issue/for the same row of buttons
EXPORT size_t obs_property_button_group_add_item(
	obs_property_t *p, const char *name, const char *text, bool enabled,
	obs_property_clicked_t callback);

//PRISM/Liuying/20200617/No issue/for the same row of buttons
EXPORT void obs_property_button_group_clear(obs_property_t *p);

//PRISM/Liuying/20200617/No issue/for the same row of buttons
EXPORT size_t obs_property_button_group_item_count(obs_property_t *p);

//PRISM/Liuying/20200617/No issue/for the same row of buttons
EXPORT const char *obs_property_button_group_item_name(obs_property_t *p,
						       size_t idx);

//PRISM/Liuying/20200617/No issue/for the same row of buttons
EXPORT const char *obs_property_button_group_item_text(obs_property_t *p,
						       size_t idx);

//PRISM/RenJinbo/20210611/No issue/add enable properties
EXPORT bool obs_property_button_group_item_enable(obs_property_t *p,
						  size_t idx);

//PRISM/Liuying/20200707/#3266/add new interface
EXPORT int obs_property_button_group_get_idx_by_name(obs_property_t *p,
						     const char *name);

//PRISM/Liuying/20200617/No issue/for the same row of buttons
EXPORT void obs_property_button_group_set_item_text(obs_property_t *p,
						    size_t idx,
						    const char *text);
//PRISM/RenJinbo/20210611/No issue/add enable properties
EXPORT void obs_property_button_group_set_item_enable(obs_property_t *p,
						      size_t idx, bool enable);

//PRISM/RenJinbo/20210708/#8551/add button highlight style
EXPORT void obs_property_button_group_set_item_highlight(obs_property_t *p,
							 size_t idx,
							 bool highlight);
//PRISM/RenJinbo/20210708/#8551/add button highlight style
EXPORT bool obs_property_button_group_get_item_highlight(obs_property_t *p,
							 size_t idx);
//PRISM/Zhangdewen/20200901/feature/for chat source
EXPORT obs_property_t *
obs_properties_add_chat_template_list(obs_properties_t *props, const char *name,
				      const char *description);

//PRISM/Zhangdewen/20200901/feature/for chat source
EXPORT obs_property_t *
obs_properties_add_chat_font_size(obs_properties_t *props, const char *name,
				  const char *description, int min, int max,
				  int step);

////PRISM/Chengbing/20200907/feature/for text motion source
EXPORT obs_property_t *obs_properties_add_tm_content(obs_properties_t *props,
						     const char *name,
						     const char *description);
////PRISM/Chengbing/20200907/feature/for text motion source
EXPORT obs_property_t *obs_properties_add_tm_tab(obs_properties_t *props,
						 const char *name);

////PRISM/Chengbing/20200907/feature/for text motion source
EXPORT obs_property_t *
obs_properties_add_tm_template_tab(obs_properties_t *props, const char *name);
////PRISM/Chengbing/20200907/feature/for text motion source
EXPORT obs_property_t *
obs_properties_add_tm_template_list(obs_properties_t *props, const char *name);

//PRISM/Chengbing/20200902/feature/for text motion source
EXPORT obs_property_t *obs_properties_add_tm_text(obs_properties_t *props,
						  const char *name,
						  const char *description,
						  int min, int max, int step);

////PRISM/Chengbing/20200907/feature/for text motion source
EXPORT obs_property_t *obs_properties_add_tm_color(obs_properties_t *props,
						   const char *name,
						   const char *description,
						   int min, int max, int step);
////PRISM/Chengbing/20200907/feature/for text motion source
EXPORT obs_property_t *obs_properties_add_tm_motion(obs_properties_t *props,
						    const char *name,
						    const char *description,
						    int min, int max, int step);

//PRISM/Zengqin/20201103/no issue/for image group
EXPORT obs_property_t *
obs_properties_add_image_group(obs_properties_t *props, const char *name,
			       const char *desc, int row, int column,
			       enum obs_image_style_type type);
EXPORT size_t obs_property_image_group_add_item(
	obs_property_t *props, const char *name, const char *url, long long val,
	obs_property_clicked_t callback);
//PRISM/RenJinBo/20200622/feature/for timer source
EXPORT bool obs_property_image_group_clicked(obs_property_t *p, void *obj,
					     size_t idx);
EXPORT size_t obs_property_image_group_item_count(obs_property_t *props);
EXPORT void obs_property_image_group_params(obs_property_t *prop, int *row,
					    int *colum,
					    enum obs_image_style_type *type);
EXPORT const char *obs_property_image_group_item_url(obs_property_t *prop,
						     int idx);
EXPORT const char *obs_property_image_group_item_name(obs_property_t *prop,
						      int idx);
/* image group end */

//PRISM/Zengqin/20201030/no issue/for custom group
EXPORT obs_property_t *obs_properties_add_custom_group(obs_properties_t *props,
						       const char *name,
						       const char *desc,
						       int row, int column);
EXPORT size_t obs_properties_custom_group_add_int(obs_property_t *prop,
						  const char *name,
						  const char *desc, int min,
						  int max, int step,
						  char *suffix);
EXPORT void obs_properties_custom_group_int_params(obs_property_t *prop,
						   int *min, int *max,
						   int *step, size_t idx);
EXPORT void obs_properties_custom_group_set_int_params(obs_property_t *prop,
						       int min, int max,
						       int step, size_t idx);
EXPORT const char *obs_property_custom_group_int_suffix(obs_property_t *prop,
							size_t idx);
EXPORT void obs_property_custom_group_row_column(obs_property_t *prop, int *row,
						 int *colum);
EXPORT size_t obs_property_custom_group_item_count(obs_property_t *props);
EXPORT enum obs_control_type
obs_property_custom_group_item_type(obs_property_t *prop, size_t idx);
EXPORT const char *obs_property_custom_group_item_name(obs_property_t *prop,
						       size_t idx);
EXPORT const char *obs_property_custom_group_item_desc(obs_property_t *prop,
						       size_t idx);
/* custom group end */

//PRISM/Zengqin/20201027/no issue/for horizontal line
EXPORT obs_property_t *obs_properties_add_h_line(obs_properties_t *props,
						 const char *name,
						 const char *desc);

//PRISM/Zengqin/20201030/no issue/This checkbox text on the left
EXPORT obs_property_t *obs_properties_add_bool_left(obs_properties_t *props,
						    const char *name,
						    const char *desc);

//PRISM/Zhangdewen/20201022/feature/for virtual background
EXPORT obs_property_t *obs_properties_add_camera_virtual_background_state(
	obs_properties_t *props, const char *name, const char *description);
//PRISM/Zhangdewen/20201023/feature/for virtual background
EXPORT obs_property_t *obs_properties_add_virtual_background_resource(
	obs_properties_t *props, const char *name, const char *description);
//PRISM/Zhangdewen/20201027/feature/for virtual background
EXPORT obs_property_t *obs_properties_add_switch(obs_properties_t *props,
						 const char *name,
						 const char *description);

/**
 * Adds a font selection property.
 *
 * A font is an obs_data sub-object which contains the following items:
 *   face:   face name string
 *   style:  style name string
 *   size:   size integer
 *   flags:  font flags integer (OBS_FONT_* defined above)
 */
EXPORT obs_property_t *obs_properties_add_font(obs_properties_t *props,
					       const char *name,
					       const char *description);

EXPORT obs_property_t *
obs_properties_add_editable_list(obs_properties_t *props, const char *name,
				 const char *description,
				 enum obs_editable_list_type type,
				 const char *filter, const char *default_path);

EXPORT obs_property_t *obs_properties_add_frame_rate(obs_properties_t *props,
						     const char *name,
						     const char *description);

EXPORT obs_property_t *obs_properties_add_group(obs_properties_t *props,
						const char *name,
						const char *description,
						enum obs_group_type type,
						obs_properties_t *group);

//PRSIM/WuLongyue/20200915/No issue/For PRISM Mobile
EXPORT obs_property_t *
obs_properties_add_mobile_guider(obs_properties_t *props, const char *name,
				 const char *description);
EXPORT obs_property_t *obs_properties_add_mobile_help(obs_properties_t *props,
						      const char *name,
						      const char *description);
EXPORT obs_property_t *
obs_properties_add_mobile_name(obs_properties_t *props, const char *name,
			       const char *description, const char *text,
			       obs_property_clicked_t callback);
EXPORT obs_property_t *
obs_properties_add_mobile_status(obs_properties_t *props, const char *name,
				 const char *description);

//PRISM/Xiewei/20210629/No issue/For Camare Flip ---start---
typedef bool (*obs_property_checkbox_clicked_t)(obs_properties_t *props,
						obs_property_t *property,
						bool checked, void *data);

EXPORT obs_property_t *
obs_properties_add_checkbox_group(obs_properties_t *props, const char *name,
				  const char *description);
EXPORT size_t obs_properties_add_checkbox_group_item(
	obs_property_t *p, const char *name, const char *description,
	const char *tooltip, bool enabled,
	obs_property_checkbox_clicked_t callback);

EXPORT bool obs_property_checkbox_group_clicked(obs_property_t *p, void *obj,
						size_t idx, bool checked);
EXPORT size_t obs_property_checkbox_group_item_count(obs_property_t *p);

EXPORT const char *obs_property_checkbox_group_item_id(obs_property_t *p,
						       size_t idx);
EXPORT const char *obs_property_checkbox_group_item_text(obs_property_t *p,
							 size_t idx);
EXPORT bool obs_property_checkbox_group_item_enabled(obs_property_t *p,
						     size_t idx);
EXPORT const char *obs_property_checkbox_group_item_tooltip(obs_property_t *p,
							    size_t idx);
//PRISM/Xiewei/20210629/No issue/For Camare Flip ---end---
/* ------------------------------------------------------------------------- */

/**
 * Optional callback for when a property is modified.  If the properties
 * need to be refreshed due to changes to the property layout, return true,
 * otherwise return false.
 */
typedef bool (*obs_property_modified_t)(obs_properties_t *props,
					obs_property_t *property,
					obs_data_t *settings);
typedef bool (*obs_property_modified2_t)(void *priv, obs_properties_t *props,
					 obs_property_t *property,
					 obs_data_t *settings);

EXPORT void
obs_property_set_modified_callback(obs_property_t *p,
				   obs_property_modified_t modified);
EXPORT void obs_property_set_modified_callback2(
	obs_property_t *p, obs_property_modified2_t modified, void *priv);

EXPORT bool obs_property_modified(obs_property_t *p, obs_data_t *settings);
EXPORT bool obs_property_button_clicked(obs_property_t *p, void *obj);

//PRISM/Liuying/20200617/No issue/for the same row of buttons
EXPORT bool obs_property_button_group_clicked(obs_property_t *p, void *obj,
					      size_t idx);
EXPORT bool obs_property_bool_group_clicked(obs_property_t *p, void *obj,
					    size_t idx);

//PRISM/Liuying/20200707/#3266/add new interface
EXPORT void obs_property_button_group_clicked_by_name(obs_property_t *p,
						      void *obj,
						      const char *name);

EXPORT bool obs_property_text_button_clicked(obs_property_t *p, void *obj);

EXPORT void obs_property_set_visible(obs_property_t *p, bool visible);
EXPORT void obs_property_set_enabled(obs_property_t *p, bool enabled);

EXPORT void obs_property_set_description(obs_property_t *p,
					 const char *description);
EXPORT void obs_property_set_long_description(obs_property_t *p,
					      const char *long_description);
//PRISM/Zhangdewen/20200909/new ndi ux
EXPORT void obs_property_set_placeholder(obs_property_t *p,
					 const char *placeholder);
//PRISM/WuLongyue/20201204/for property UI.
EXPORT void obs_property_set_tooltip(obs_property_t *p, const char *tooltip);
//PRISM/RenJinbo/20210624/None/ignore call modified callback, when refresh properties.
EXPORT void obs_property_set_ignore_callback_when_refresh(obs_property_t *p,
							  bool ignore);

EXPORT const char *obs_property_name(obs_property_t *p);
EXPORT const char *obs_property_description(obs_property_t *p);
EXPORT const char *obs_property_long_description(obs_property_t *p);
//PRISM/Zhangdewen/20200909/new ndi ux
EXPORT const char *obs_property_placeholder(obs_property_t *p);
EXPORT const char *obs_property_tooltip(obs_property_t *p);
EXPORT enum obs_property_type obs_property_get_type(obs_property_t *p);
EXPORT bool obs_property_enabled(obs_property_t *p);
EXPORT bool obs_property_visible(obs_property_t *p);

EXPORT bool obs_property_next(obs_property_t **p);

EXPORT int obs_property_int_min(obs_property_t *p);
EXPORT int obs_property_int_max(obs_property_t *p);
EXPORT int obs_property_int_step(obs_property_t *p);
EXPORT enum obs_number_type obs_property_int_type(obs_property_t *p);
EXPORT const char *obs_property_int_suffix(obs_property_t *p);
EXPORT double obs_property_float_min(obs_property_t *p);
EXPORT double obs_property_float_max(obs_property_t *p);
EXPORT double obs_property_float_step(obs_property_t *p);
EXPORT enum obs_number_type obs_property_float_type(obs_property_t *p);
EXPORT const char *obs_property_float_suffix(obs_property_t *p);
EXPORT enum obs_text_type obs_property_text_type(obs_property_t *p);
EXPORT enum obs_path_type obs_property_path_type(obs_property_t *p);
EXPORT const char *obs_property_path_filter(obs_property_t *p);
EXPORT const char *obs_property_path_default_path(obs_property_t *p);
EXPORT enum obs_combo_type obs_property_list_type(obs_property_t *p);
EXPORT enum obs_combo_format obs_property_list_format(obs_property_t *p);
//PRISM/Zhangdewen/20200916/new ndi ux
EXPORT void obs_property_set_list_readonly(obs_property_t *p, bool readonly);
//PRISM/Zhangdewen/20200916/new ndi ux
EXPORT bool obs_property_list_readonly(obs_property_t *p);

EXPORT const char *obs_property_text_button_text(obs_property_t *p);

EXPORT void obs_property_int_set_limits(obs_property_t *p, int min, int max,
					int step);
EXPORT void obs_property_float_set_limits(obs_property_t *p, double min,
					  double max, double step);
EXPORT void obs_property_int_set_suffix(obs_property_t *p, const char *suffix);
EXPORT void obs_property_float_set_suffix(obs_property_t *p,
					  const char *suffix);

EXPORT void obs_property_list_clear(obs_property_t *p);

EXPORT size_t obs_property_list_add_string(obs_property_t *p, const char *name,
					   const char *val);
EXPORT size_t obs_property_list_add_int(obs_property_t *p, const char *name,
					long long val);
EXPORT size_t obs_property_list_add_float(obs_property_t *p, const char *name,
					  double val);

EXPORT void obs_property_list_insert_string(obs_property_t *p, size_t idx,
					    const char *name, const char *val);
EXPORT void obs_property_list_insert_int(obs_property_t *p, size_t idx,
					 const char *name, long long val);
EXPORT void obs_property_list_insert_float(obs_property_t *p, size_t idx,
					   const char *name, double val);

EXPORT void obs_property_list_item_disable(obs_property_t *p, size_t idx,
					   bool disabled);
EXPORT bool obs_property_list_item_disabled(obs_property_t *p, size_t idx);

EXPORT void obs_property_list_item_remove(obs_property_t *p, size_t idx);

//PRISM/WangShaohui/20210922/noIssue/add tooltips for combox list
EXPORT void obs_property_list_item_set_tips(obs_property_t *p, size_t idx,
					    const char *tips);
//PRISM/WangShaohui/20210922/noIssue/add tooltips for combox list
EXPORT const char *obs_property_list_item_get_tips(obs_property_t *p,
						   size_t idx);

EXPORT size_t obs_property_list_item_count(obs_property_t *p);
EXPORT const char *obs_property_list_item_name(obs_property_t *p, size_t idx);
EXPORT const char *obs_property_list_item_string(obs_property_t *p, size_t idx);
EXPORT long long obs_property_list_item_int(obs_property_t *p, size_t idx);
EXPORT double obs_property_list_item_float(obs_property_t *p, size_t idx);

EXPORT enum obs_editable_list_type
obs_property_editable_list_type(obs_property_t *p);
EXPORT const char *obs_property_editable_list_filter(obs_property_t *p);
EXPORT const char *obs_property_editable_list_default_path(obs_property_t *p);

EXPORT void obs_property_frame_rate_clear(obs_property_t *p);
EXPORT void obs_property_frame_rate_options_clear(obs_property_t *p);
EXPORT void obs_property_frame_rate_fps_ranges_clear(obs_property_t *p);

EXPORT size_t obs_property_frame_rate_option_add(obs_property_t *p,
						 const char *name,
						 const char *description);
EXPORT size_t obs_property_frame_rate_fps_range_add(
	obs_property_t *p, struct media_frames_per_second min,
	struct media_frames_per_second max);

EXPORT void obs_property_frame_rate_option_insert(obs_property_t *p, size_t idx,
						  const char *name,
						  const char *description);
EXPORT void
obs_property_frame_rate_fps_range_insert(obs_property_t *p, size_t idx,
					 struct media_frames_per_second min,
					 struct media_frames_per_second max);

EXPORT size_t obs_property_frame_rate_options_count(obs_property_t *p);
EXPORT const char *obs_property_frame_rate_option_name(obs_property_t *p,
						       size_t idx);
EXPORT const char *obs_property_frame_rate_option_description(obs_property_t *p,
							      size_t idx);

EXPORT size_t obs_property_frame_rate_fps_ranges_count(obs_property_t *p);
EXPORT struct media_frames_per_second
obs_property_frame_rate_fps_range_min(obs_property_t *p, size_t idx);
EXPORT struct media_frames_per_second
obs_property_frame_rate_fps_range_max(obs_property_t *p, size_t idx);

EXPORT enum obs_group_type obs_property_group_type(obs_property_t *p);
EXPORT obs_properties_t *obs_property_group_content(obs_property_t *p);

//PRISM/Liuying/20200624/No issue/for the same row of buttons
EXPORT size_t obs_property_bool_group_item_count(obs_property_t *p);

//PRISM/Liuying/20200624/No issue/for the same row of buttons
EXPORT const char *obs_property_bool_group_item_text(obs_property_t *p,
						     size_t idx);

EXPORT bool obs_property_bool_group_item_enabled(obs_property_t *p, size_t idx);
EXPORT const char *obs_property_bool_group_item_tooltip(obs_property_t *p,
							size_t idx);

#ifndef SWIG
DEPRECATED
EXPORT enum obs_text_type obs_proprety_text_type(obs_property_t *p);
#endif

//PRISM/Zhangdewen/20200901/feature/for chat source
EXPORT int obs_property_chat_font_size_min(obs_property_t *p);
EXPORT int obs_property_chat_font_size_max(obs_property_t *p);
EXPORT int obs_property_chat_font_size_step(obs_property_t *p);

//PRISM/Chengbing/20200902/feature/for text motion source
EXPORT int obs_property_tm_text_min(obs_property_t *p, size_t propertyType);
EXPORT int obs_property_tm_text_max(obs_property_t *p, size_t propertyType);
EXPORT int obs_property_tm_text_step(obs_property_t *p, size_t propertyType);

//PRISM/WangShaohui/20201029/#5497/limite text length
EXPORT void obs_property_set_length_limit(obs_property_t *p, int max_length);
EXPORT int obs_property_get_length_limit(obs_property_t *p);

//PRISM/Zengqin/20201125/#none/for property UI
EXPORT void obs_property_add_flags(obs_property_t *p, uint32_t flag);
EXPORT uint32_t obs_property_get_flags(obs_property_t *p);

//PRSIM/Renjinbo/20210518/timer feature/add right spinbox box group, like the OBS_PROPERTY_BOOL_GROUP ui.
EXPORT obs_property_t *obs_properties_add_int_group(obs_properties_t *props,
						    const char *name,
						    const char *description);
EXPORT size_t obs_properties_add_int_group_item(obs_property_t *p,
						const char *name,
						const char *desc, int min,
						int max, int step);

EXPORT size_t obs_property_int_group_item_count(obs_property_t *p);

EXPORT void obs_property_int_group_item_params(obs_property_t *prop,
					       char **subName, char **des,
					       int *min, int *max, int *step,
					       size_t idx);

EXPORT obs_property_t *obs_properties_add_font_simple(obs_properties_t *props,
						      const char *name,
						      const char *description);

EXPORT obs_property_t *
obs_properties_add_color_checkbox(obs_properties_t *props, const char *name,
				  const char *description);

EXPORT obs_property_t *
obs_properties_add_timer_list_Listen(obs_properties_t *props, const char *name,
				     const char *description);

EXPORT obs_property_t *obs_properties_add_label_tip(obs_properties_t *props,
						    const char *name,
						    const char *description);

#ifdef __cplusplus
}
#endif

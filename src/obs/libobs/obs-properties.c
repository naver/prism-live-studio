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

#include "util/bmem.h"
#include "util/darray.h"
#include "obs-internal.h"
#include "obs-properties.h"

static inline void *get_property_data(struct obs_property *prop);

/* ------------------------------------------------------------------------- */

struct float_data {
	double min, max, step;
	enum obs_number_type type;
	char *suffix;
};

struct int_data {
	int min, max, step;
	enum obs_number_type type;
	char *suffix;
};

struct list_item {
	char *name;
	bool disabled;

	//PRISM/WangShaohui/20210922/noIssue/add tooltips for combox list
	char *tips;

	union {
		char *str;
		long long ll;
		double d;
	};
};

struct path_data {
	char *filter;
	char *default_path;
	enum obs_path_type type;
};

struct text_data {
	enum obs_text_type type;
};

struct list_data {
	DARRAY(struct list_item) items;
	enum obs_combo_type type;
	enum obs_combo_format format;
	bool readonly;
};

struct editable_list_data {
	enum obs_editable_list_type type;
	char *filter;
	char *default_path;
};

struct button_data {
	obs_property_clicked_t callback;
};

struct text_button_data {
	char *text;
	obs_property_clicked_t callback;
};

//PRISM/Liuying/20200617/No issue/for the same row of buttons
struct button_group_item {
	char *name;
	char *text;
	//PRISM/RenJinbo/20210628/#none/Timer source feature
	bool enabled;
	//PRISM/RenJinbo/20210708/#8551/add button highlight style
	bool highlight;
	obs_property_clicked_t callback;
};

struct button_group_data {
	DARRAY(struct button_group_item) items;
};

struct frame_rate_option {
	char *name;
	char *description;
};

struct frame_rate_range {
	struct media_frames_per_second min_time;
	struct media_frames_per_second max_time;
};

struct frame_rate_data {
	DARRAY(struct frame_rate_option) extra_options;
	DARRAY(struct frame_rate_range) ranges;
};

struct group_data {
	enum obs_group_type type;
	obs_properties_t *content;
};

struct bool_data {
	char *text;
	char *tooltip;
	bool enabled;
	obs_property_clicked_t callback;
};

struct bool_group_data {
	DARRAY(struct bool_data) items;
};

//PRISM/Xiewei/20210629/No issue/For Camera flip.
struct checkbox_data {
	char *name;
	char *text;
	char *tooltip;
	bool enabled;
	obs_property_checkbox_clicked_t callback;
};

struct checkbox_group_data {
	DARRAY(struct checkbox_data) items;
};

struct music_data {
	char *name;
	char *producer;
	char *url;
	int duration;
	int duration_type;
	obs_property_clicked_t callback;
};

struct music_group_data {
	DARRAY(struct music_data) items;
};

//PRISM/RenJinbo/20210628/#none/Timer source feature
struct int_data_item {
	char *name;
	char *description;
	int min, max, step;
};
//PRISM/RenJinbo/20210628/#none/Timer source feature
struct int_group_data {
	char *name;
	char *description;
	DARRAY(struct int_data_item) items;
};

//PRISM/Zhangdewen/20200901/feature/for chat source
struct chat_font_size_data {
	int min, max, step;
};

//PRISM/Chengbing/20200901/feature/for text motion source
struct tm_text_data {
	int min, max, step;
};

//PRISM/Zengqin/20201021/feature/for spectralizer source
struct image_group_item {
	char *name;
	char *url;
	long long val;
	//PRISM/RenJinbo/20210628/#none/Timer source feature
	obs_property_clicked_t callback;
};

//PRISM/Zengqin/20201021/feature/for spectralizer source
struct image_group_data {
	int row;
	int column;
	enum obs_image_style_type type;
	DARRAY(struct image_group_item) items;
};

//PRISM/Zengqin/20201021/feature/for custom froup item in line
struct custom_group_item {

	char *name;
	char *desc;
	enum obs_control_type type;
	union {
		struct list_data ld;
		struct bool_data bd;
		struct button_data buttonddata;
		struct float_data fd;
		struct int_data id;
	};
};

//PRISM/Zengqin/20201021/feature/for custom group
struct custom_group_data {
	int row;
	int column;
	DARRAY(struct custom_group_item) items;
};

//PRISM/Liuying/20200624/No issue/for the same row of buttons
static inline void bool_group_data_free(struct bool_group_data *data)
{
	for (size_t i = 0; i < data->items.num; i++) {
		struct bool_data *item = data->items.array + i;
		bfree(item->text);
		bfree(item->tooltip);
	}

	da_free(data->items);
}

//PRISM/Liuying/20200706/new bgm ux
static inline void music_group_free(struct music_group_data *data)
{
	for (size_t i = 0; i < data->items.num; i++) {
		struct music_data *item = data->items.array + i;
		bfree(item->name);
		bfree(item->producer);
		bfree(item->url);
	}

	da_free(data->items);
}

//PRISM/Xiewei/20210629/No issue/For Camera flip.
static inline void checkbox_group_free(struct checkbox_group_data *data)
{
	for (size_t i = 0; i < data->items.num; i++) {
		struct checkbox_data *item = data->items.array + i;
		bfree(item->name);
		bfree(item->text);
		bfree(item->tooltip);
	}

	da_free(data->items);
}

static inline void path_data_free(struct path_data *data)
{
	bfree(data->default_path);
	if (data->type == OBS_PATH_FILE)
		bfree(data->filter);
}

static inline void editable_list_data_free(struct editable_list_data *data)
{
	bfree(data->default_path);
	bfree(data->filter);
}

static inline void list_item_free(struct list_data *data,
				  struct list_item *item)
{
	bfree(item->name);
	if (data->format == OBS_COMBO_FORMAT_STRING)
		bfree(item->str);

	//PRISM/WangShaohui/20210922/noIssue/add tooltips for combox list
	if (item->tips)
		bfree(item->tips);
}

static inline void list_data_free(struct list_data *data)
{
	for (size_t i = 0; i < data->items.num; i++)
		list_item_free(data, data->items.array + i);

	da_free(data->items);
}

static inline void frame_rate_data_options_free(struct frame_rate_data *data)
{
	for (size_t i = 0; i < data->extra_options.num; i++) {
		struct frame_rate_option *opt = &data->extra_options.array[i];
		bfree(opt->name);
		bfree(opt->description);
	}

	da_resize(data->extra_options, 0);
}

static inline void frame_rate_data_ranges_free(struct frame_rate_data *data)
{
	da_resize(data->ranges, 0);
}

static inline void frame_rate_data_free(struct frame_rate_data *data)
{
	frame_rate_data_options_free(data);
	frame_rate_data_ranges_free(data);

	da_free(data->extra_options);
	da_free(data->ranges);
}

static inline void group_data_free(struct group_data *data)
{
	obs_properties_destroy(data->content);
}

static inline void int_data_free(struct int_data *data)
{
	if (data->suffix)
		bfree(data->suffix);
}

static inline void float_data_free(struct float_data *data)
{
	if (data->suffix)
		bfree(data->suffix);
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
static inline void button_group_data_free(struct button_group_data *data)
{
	for (size_t i = 0; i < data->items.num; i++) {
		struct button_group_item *item = data->items.array + i;
		bfree(item->name);
		bfree(item->text);
	}

	da_free(data->items);
}

//PRISM/Zengqin/20201021/No issue/for image group
static inline void image_group_data_free(struct image_group_data *data)
{
	for (size_t i = 0; i < data->items.num; i++) {
		struct image_group_item *item = data->items.array + i;
		bfree(item->name);
		bfree(item->url);
	}

	da_free(data->items);
}

static inline void text_button_data_free(struct text_button_data *data)
{
	if (data->text)
		bfree(data->text);
}

//PRISM/RenJinbo/20210628/#none/Timer source feature
static inline void int_group_data_free(struct int_group_data *data)
{
	bfree(data->name);
	bfree(data->description);
	for (size_t i = 0; i < data->items.num; i++) {
		struct int_data_item *item = data->items.array + i;
		bfree(item->name);
		bfree(item->description);
	}
	da_free(data->items);
}

struct obs_properties;

struct obs_property {
	char *name;
	char *desc;
	char *long_desc;
	char *placeholder; //PRISM/Zhangdewen/20200909/new ndi ux
	void *priv;
	enum obs_property_type type;
	bool visible;
	bool enabled;

	//PRISM/Zengqin/20201125/for property UI.
	uint32_t flags;
	//PRISM/WuLongyue/20201204/for property UI.
	char *tooltip;

	//PRISM/WangShaohui/20201029/#5497/Limite text length for editor. It is valid if more than 0.
	int length_limit;

	struct obs_properties *parent;

	obs_property_modified_t modified;
	obs_property_modified2_t modified2;

	struct obs_property *next;

	//PRISM/RenJinbo/20210624/None/ignore call modified callback, when refresh properties.
	bool ignoreCallbackWhenRefresh;
};

struct obs_properties {
	void *param;
	void (*destroy)(void *param);
	uint32_t flags;

	struct obs_property *first_property;
	struct obs_property **last;
	struct obs_property *parent;
};

obs_properties_t *obs_properties_create(void)
{
	struct obs_properties *props;
	props = bzalloc(sizeof(struct obs_properties));
	props->last = &props->first_property;
	return props;
}

void obs_properties_set_param(obs_properties_t *props, void *param,
			      void (*destroy)(void *param))
{
	if (!props)
		return;

	if (props->param && props->destroy)
		props->destroy(props->param);

	props->param = param;
	props->destroy = destroy;
}

void obs_properties_set_flags(obs_properties_t *props, uint32_t flags)
{
	if (props)
		props->flags = flags;
}

uint32_t obs_properties_get_flags(obs_properties_t *props)
{
	return props ? props->flags : 0;
}

void *obs_properties_get_param(obs_properties_t *props)
{
	return props ? props->param : NULL;
}

obs_properties_t *obs_properties_create_param(void *param,
					      void (*destroy)(void *param))
{
	struct obs_properties *props = obs_properties_create();
	obs_properties_set_param(props, param, destroy);
	return props;
}

static void obs_property_destroy(struct obs_property *property)
{
	if (property->type == OBS_PROPERTY_LIST ||
	    property->type == OBS_PROPERTY_TIMER_LIST_LISTEN)
		list_data_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_PATH)
		path_data_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_EDITABLE_LIST)
		editable_list_data_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_FRAME_RATE)
		frame_rate_data_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_GROUP)
		group_data_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_INT)
		int_data_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_FLOAT)
		float_data_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_BGM_MUSIC_LIST)
		music_group_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_BOOL_GROUP)
		bool_group_data_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_MOBILE_NAME)
		text_button_data_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_INT_GROUP)
		int_group_data_free(get_property_data(property));
	else if (property->type == OBS_PROPERTY_CHECKBOX_GROUP)
		checkbox_group_free(get_property_data(property));

	bfree(property->name);
	bfree(property->desc);
	bfree(property->long_desc);
	bfree(property->placeholder); //PRISM/Zhangdewen/20200911/new ndi ux
	bfree(property->tooltip);
	bfree(property);
}

void obs_properties_destroy(obs_properties_t *props)
{
	if (props) {
		struct obs_property *p = props->first_property;

		if (props->destroy && props->param)
			props->destroy(props->param);

		while (p) {
			struct obs_property *next = p->next;
			obs_property_destroy(p);
			p = next;
		}

		bfree(props);
	}
}

obs_property_t *obs_properties_first(obs_properties_t *props)
{
	return (props != NULL) ? props->first_property : NULL;
}

obs_property_t *obs_properties_get(obs_properties_t *props, const char *name)
{
	struct obs_property *property;

	if (!props)
		return NULL;

	property = props->first_property;
	while (property) {
		if (strcmp(property->name, name) == 0)
			return property;

		if (property->type == OBS_PROPERTY_GROUP) {
			obs_properties_t *group =
				obs_property_group_content(property);
			obs_property_t *found = obs_properties_get(group, name);
			if (found != NULL) {
				return found;
			}
		}

		property = property->next;
	}

	return NULL;
}

obs_properties_t *obs_properties_get_parent(obs_properties_t *props)
{
	return props->parent ? props->parent->parent : NULL;
}

void obs_properties_remove_by_name(obs_properties_t *props, const char *name)
{
	if (!props)
		return;

	/* obs_properties_t is a forward-linked-list, so we need to keep both
	 * previous and current pointers around. That way we can fix up the
	 * pointers for the previous element if we find a match.
	 */
	struct obs_property *cur = props->first_property;
	struct obs_property *prev = props->first_property;

	while (cur) {
		if (strcmp(cur->name, name) == 0) {
			prev->next = cur->next;
			cur->next = 0;
			obs_property_destroy(cur);
			break;
		}

		if (cur->type == OBS_PROPERTY_GROUP) {
			obs_properties_remove_by_name(
				obs_property_group_content(cur), name);
		}

		prev = cur;
		cur = cur->next;
	}
}

void obs_properties_apply_settings_internal(obs_properties_t *props,
					    obs_data_t *settings,
					    obs_properties_t *realprops)
{
	struct obs_property *p;

	p = props->first_property;
	while (p) {
		if (p->type == OBS_PROPERTY_GROUP) {
			obs_properties_apply_settings_internal(
				obs_property_group_content(p), settings,
				realprops);
		}
		if (p->modified && !p->ignoreCallbackWhenRefresh)
			p->modified(realprops, p, settings);
		else if (p->modified2 && !p->ignoreCallbackWhenRefresh)
			p->modified2(p->priv, realprops, p, settings);
		p = p->next;
	}
}

void obs_properties_apply_settings(obs_properties_t *props,
				   obs_data_t *settings)
{
	if (!props)
		return;

	obs_properties_apply_settings_internal(props, settings, props);
}

/* ------------------------------------------------------------------------- */

static inline void propertes_add(struct obs_properties *props,
				 struct obs_property *p)
{
	*props->last = p;
	props->last = &p->next;
}

static inline size_t get_property_size(enum obs_property_type type)
{
	switch (type) {
	case OBS_PROPERTY_INVALID:
		return 0;
	case OBS_PROPERTY_BOOL:
		return 0;
	//PRISM/Liuying/20200713/new bgm ux
	case OBS_PROPERTY_TIPS:
		return 0;
	case OBS_PROPERTY_BOOL_GROUP:
		return sizeof(struct bool_group_data);
	case OBS_PROPERTY_INT:
		return sizeof(struct int_data);
	case OBS_PROPERTY_FLOAT:
		return sizeof(struct float_data);
	case OBS_PROPERTY_TEXT:
		return sizeof(struct text_data);
	case OBS_PROPERTY_PATH:
		return sizeof(struct path_data);
	case OBS_PROPERTY_LIST:
	case OBS_PROPERTY_TIMER_LIST_LISTEN:
		return sizeof(struct list_data);
	//PRISM/Liuying/20200706/new bgm ux
	case OBS_PROPERTY_BGM_MUSIC_LIST:
		return sizeof(struct music_data);
	case OBS_PROPERTY_COLOR:
		return 0;
	case OBS_PROPERTY_BUTTON:
		return sizeof(struct button_data);
	//PRISM/Liuying/20200617/No issue/for the same row of buttons
	case OBS_PROPERTY_BUTTON_GROUP:
		return sizeof(struct button_group_data);
	case OBS_PROPERTY_FONT:
		return 0;
	case OBS_PROPERTY_EDITABLE_LIST:
		return sizeof(struct editable_list_data);
	case OBS_PROPERTY_FRAME_RATE:
		return sizeof(struct frame_rate_data);
	case OBS_PROPERTY_GROUP:
		return sizeof(struct group_data);
	case OBS_PROPERTY_TM_TEXT: //PRISM/Chengbing/20200901/feature/for text motion source
	case OBS_PROPERTY_TM_COLOR:
	case OBS_PROPERTY_TM_MOTION:
		return sizeof(struct tm_text_data);
	case OBS_PROPERTY_CHAT_FONT_SIZE: //PRISM/Zhangdewen/20200901/feature/for chat source
		return sizeof(struct chat_font_size_data);
	case OBS_PROPERTY_IMAGE_GROUP:
		return sizeof(struct image_group_data);
	case OBS_PROPERTY_CUSTOM_GROUP:
		return sizeof(struct custom_group_data);
	case OBS_PROPERTY_MOBILE_NAME:
		return sizeof(struct text_button_data);
	case OBS_PROPERTY_PRIVATE_DATA_TEXT:
		return 0;
	case OBS_PROPERTY_CHECKBOX_GROUP: //PRISM/Xiewei/20210629/No issue/For Camera flip.
		return sizeof(struct checkbox_group_data);
	case OBS_PROPERTY_INT_GROUP:
		return sizeof(struct int_group_data);
	case OBS_PROPERTY_FONT_SIMPLE:
		return 0;
	case OBS_PROPERTY_COLOR_CHECKBOX:
		return 0;
	}

	return 0;
}

static inline struct obs_property *new_prop(struct obs_properties *props,
					    const char *name, const char *desc,
					    enum obs_property_type type)
{
	size_t data_size = get_property_size(type);
	struct obs_property *p;

	p = bzalloc(sizeof(struct obs_property) + data_size);
	p->parent = props;
	p->enabled = true;
	p->visible = true;
	p->type = type;
	p->name = bstrdup(name);
	p->desc = bstrdup(desc);
	//PRISM/Zengqin/20201125/#none/for property UI
	p->flags = 0;

	//PRISM/WangShaohui/20201029/#5497/limite text length
	p->length_limit = 0;

	//PRISM/RenJinbo/20210628/#none/Timer source feature
	p->ignoreCallbackWhenRefresh = false;
	propertes_add(props, p);

	return p;
}

static inline obs_properties_t *get_topmost_parent(obs_properties_t *props)
{
	obs_properties_t *parent = props;
	obs_properties_t *last_parent = parent;
	while (parent) {
		last_parent = parent;
		parent = obs_properties_get_parent(parent);
	}
	return last_parent;
}

static inline bool contains_prop(struct obs_properties *props, const char *name)
{
	struct obs_property *p = props->first_property;

	while (p) {
		if (strcmp(p->name, name) == 0) {
			plog(LOG_WARNING, "Property '%s' exists", name);
			return true;
		}

		if (p->type == OBS_PROPERTY_GROUP) {
			if (contains_prop(obs_property_group_content(p),
					  name)) {
				return true;
			}
		}

		p = p->next;
	}

	return false;
}

static inline bool has_prop(struct obs_properties *props, const char *name)
{
	return contains_prop(get_topmost_parent(props), name);
}

static inline void *get_property_data(struct obs_property *prop)
{
	return (uint8_t *)prop + sizeof(struct obs_property);
}

static inline void *get_type_data(struct obs_property *prop,
				  enum obs_property_type type)
{
	if (!prop || prop->type != type)
		return NULL;

	return get_property_data(prop);
}

obs_property_t *obs_properties_add_bool(obs_properties_t *props,
					const char *name, const char *desc)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, desc, OBS_PROPERTY_BOOL);
}

static inline struct bool_group_data *
get_bool_group_data(struct obs_property *p)
{
	if (!p || p->type != OBS_PROPERTY_BOOL_GROUP) {
		return NULL;
	}
	return get_property_data(p);
}

obs_property_t *obs_properties_add_bool_group(obs_properties_t *props,
					      const char *name,
					      const char *desc)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, desc, OBS_PROPERTY_BOOL_GROUP);
}

size_t obs_properties_add_bool_group_item(obs_property_t *p,
					  const char *description,
					  const char *tooltip, bool enabled,
					  obs_property_clicked_t callback)
{
	struct bool_group_data *data = get_bool_group_data(p);
	if (data) {
		struct bool_data *item = bzalloc(sizeof(struct bool_data));
		item->text = bstrdup(description);
		item->tooltip = bstrdup(tooltip);
		item->enabled = enabled;
		item->callback = callback;
		return da_push_back(data->items, item);
	}
	return 0;
}

static obs_property_t *add_int(obs_properties_t *props, const char *name,
			       const char *desc, int min, int max, int step,
			       enum obs_number_type type)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p = new_prop(props, name, desc, OBS_PROPERTY_INT);
	struct int_data *data = get_property_data(p);
	data->min = min;
	data->max = max;
	data->step = step;
	data->type = type;
	return p;
}

static obs_property_t *add_flt(obs_properties_t *props, const char *name,
			       const char *desc, double min, double max,
			       double step, enum obs_number_type type)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_FLOAT);
	struct float_data *data = get_property_data(p);
	data->min = min;
	data->max = max;
	data->step = step;
	data->type = type;
	return p;
}

obs_property_t *obs_properties_add_int(obs_properties_t *props,
				       const char *name, const char *desc,
				       int min, int max, int step)
{
	return add_int(props, name, desc, min, max, step, OBS_NUMBER_SCROLLER);
}

obs_property_t *obs_properties_add_float(obs_properties_t *props,
					 const char *name, const char *desc,
					 double min, double max, double step)
{
	return add_flt(props, name, desc, min, max, step, OBS_NUMBER_SCROLLER);
}

obs_property_t *obs_properties_add_int_slider(obs_properties_t *props,
					      const char *name,
					      const char *desc, int min,
					      int max, int step)
{
	return add_int(props, name, desc, min, max, step, OBS_NUMBER_SLIDER);
}

obs_property_t *obs_properties_add_float_slider(obs_properties_t *props,
						const char *name,
						const char *desc, double min,
						double max, double step)
{
	return add_flt(props, name, desc, min, max, step, OBS_NUMBER_SLIDER);
}

obs_property_t *obs_properties_add_text(obs_properties_t *props,
					const char *name, const char *desc,
					enum obs_text_type type)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p = new_prop(props, name, desc, OBS_PROPERTY_TEXT);
	struct text_data *data = get_property_data(p);
	data->type = type;
	return p;
}

//PRSIM/WuLongyue/20200915/No issue/For PRISM Mobile
obs_property_t *obs_properties_add_private_data_text(obs_properties_t *props,
						     const char *name,
						     const char *text)
{
	if (!props || has_prop(props, name))
		return NULL;

	return new_prop(props, name, text, OBS_PROPERTY_PRIVATE_DATA_TEXT);
}

obs_property_t *obs_properties_add_path(obs_properties_t *props,
					const char *name, const char *desc,
					enum obs_path_type type,
					const char *filter,
					const char *default_path)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p = new_prop(props, name, desc, OBS_PROPERTY_PATH);
	struct path_data *data = get_property_data(p);
	data->type = type;
	data->default_path = bstrdup(default_path);

	if (data->type == OBS_PATH_FILE)
		data->filter = bstrdup(filter);

	return p;
}

obs_property_t *obs_properties_add_list(obs_properties_t *props,
					const char *name, const char *desc,
					enum obs_combo_type type,
					enum obs_combo_format format)
{
	if (!props || has_prop(props, name))
		return NULL;

	if (type == OBS_COMBO_TYPE_EDITABLE &&
	    format != OBS_COMBO_FORMAT_STRING) {
		plog(LOG_WARNING,
		     "List '%s', error: Editable combo boxes "
		     "must be of the 'string' type",
		     name);
		return NULL;
	}

	struct obs_property *p = new_prop(props, name, desc, OBS_PROPERTY_LIST);
	struct list_data *data = get_property_data(p);
	data->format = format;
	data->type = type;
	data->readonly = false;

	return p;
}

obs_property_t *obs_properties_add_mobile_guider(obs_properties_t *props,
						 const char *name,
						 const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, description, OBS_PROPERTY_MOBILE_GUIDER);
}

obs_property_t *obs_properties_add_mobile_help(obs_properties_t *props,
					       const char *name,
					       const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, description, OBS_PROPERTY_MOBILE_HELP);
}

obs_property_t *obs_properties_add_mobile_name(obs_properties_t *props,
					       const char *name,
					       const char *description,
					       const char *text,
					       obs_property_clicked_t callback)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, description, OBS_PROPERTY_MOBILE_NAME);
	struct text_button_data *data = get_property_data(p);
	data->text = bstrdup(text);
	data->callback = callback;

	return p;
}

obs_property_t *obs_properties_add_mobile_status(obs_properties_t *props,
						 const char *name,
						 const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, description, OBS_PROPERTY_MOBILE_STATUS);
}

//PRISM/Xiewei/20210629/No issue/For Camera flip.
obs_property_t *obs_properties_add_checkbox_group(obs_properties_t *props,
						  const char *name,
						  const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, description, OBS_PROPERTY_CHECKBOX_GROUP);
}

//PRISM/Xiewei/20210629/No issue/For Camera flip.
static inline struct checkbox_group_data *
get_checkbox_group_data(struct obs_property *p)
{
	if (!p || p->type != OBS_PROPERTY_CHECKBOX_GROUP) {
		return NULL;
	}
	return get_property_data(p);
}

//PRISM/Xiewei/20210629/No issue/For Camera flip.
size_t
obs_properties_add_checkbox_group_item(obs_property_t *p, const char *id,
				       const char *description,
				       const char *tooltip, bool enabled,
				       obs_property_checkbox_clicked_t callback)
{
	struct checkbox_group_data *data = get_checkbox_group_data(p);
	if (data) {
		struct checkbox_data *item =
			bzalloc(sizeof(struct checkbox_data));
		item->text = bstrdup(description);
		item->tooltip = bstrdup(tooltip);
		item->enabled = enabled;
		item->callback = callback;
		item->name = bstrdup(id);
		return da_push_back(data->items, item);
	}
	return 0;
}

//PRISM/Xiewei/20210629/No issue/For Camera flip.
bool obs_property_checkbox_group_clicked(obs_property_t *p, void *obj,
					 size_t idx, bool checked)
{
	struct obs_context_data *context = obj;
	if (p) {
		struct checkbox_group_data *data =
			get_type_data(p, OBS_PROPERTY_CHECKBOX_GROUP);

		if (!data) {
			return false;
		}

		if (idx >= obs_property_checkbox_group_item_count(p)) {
			return false;
		}

		struct checkbox_data item = data->items.array[idx];
		if (item.callback) {
			obs_properties_t *top = get_topmost_parent(p->parent);
			if (p->priv)
				return item.callback(top, p, checked, p->priv);
			return item.callback(top, p, checked,
					     (context ? context->data : NULL));
		}
	}

	return false;
}

//PRISM/Xiewei/20210629/No issue/For Camera flip.
size_t obs_property_checkbox_group_item_count(obs_property_t *p)
{
	struct checkbox_group_data *data = get_checkbox_group_data(p);
	return data ? data->items.num : 0;
}

//PRISM/Xiewei/20210629/No issue/For Camera flip.
const char *obs_property_checkbox_group_item_id(obs_property_t *p, size_t idx)
{
	struct checkbox_group_data *data = get_checkbox_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].name
					       : NULL;
}

//PRISM/Xiewei/20210629/No issue/For Camera flip.
const char *obs_property_checkbox_group_item_text(obs_property_t *p, size_t idx)
{
	struct checkbox_group_data *data = get_checkbox_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].text
					       : NULL;
}

//PRISM/Xiewei/20210629/No issue/For Camera flip.
bool obs_property_checkbox_group_item_enabled(obs_property_t *p, size_t idx)
{
	struct checkbox_group_data *data = get_checkbox_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].enabled
					       : false;
}

//PRISM/Xiewei/20210629/No issue/For Camera flip.
const char *obs_property_checkbox_group_item_tooltip(obs_property_t *p,
						     size_t idx)
{
	struct checkbox_group_data *data = get_checkbox_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].tooltip
					       : NULL;
}

//PRISM/Liuying/20200706/new bgm ux
static inline bool is_music_group(struct obs_property *p)
{
	return p->type == OBS_PROPERTY_BGM_MUSIC_LIST;
}

//PRISM/Liuying/20200706/new bgm ux
static inline struct music_group_data *
get_music_group_data(struct obs_property *p)
{
	if (!p || !is_music_group(p))
		return NULL;

	return get_property_data(p);
}

//PRISM/Liuying/20200706/new bgm ux
static size_t add_music_item(struct music_group_data *data, const char *name,
			     const char *producer, const char *url,
			     int duration, int duration_type,
			     obs_property_clicked_t callback)
{
	struct music_data *item = bzalloc(sizeof(struct music_data));
	item->name = bstrdup(name);
	item->producer = bstrdup(producer);
	item->url = bstrdup(url);
	item->duration_type = duration_type;
	item->duration = duration;
	item->callback = callback;

	return da_push_back(data->items, item);
}

//PRISM/Liuying/20200706/new bgm ux
size_t obs_property_music_group_add_item(obs_property_t *p, const char *name,
					 const char *producer, const char *url,
					 int duration, int duration_type,
					 obs_property_clicked_t callback)
{
	struct music_group_data *data = get_music_group_data(p);
	if (data)
		return add_music_item(data, name, producer, url, duration,
				      duration_type, callback);
	return 0;
}

//PRISM/Liuying/20200706/new bgm ux
void obs_property_music_group_clear(obs_property_t *p)
{
	struct music_group_data *data = get_music_group_data(p);
	if (!data) {
		return;
	}
	for (size_t i = 0; i < data->items.num; i++) {
		struct music_data *item = data->items.array + i;
		bfree(item->name);
		bfree(item->producer);
		bfree(item->url);
	}
	da_free(data->items);
}

//PRISM/Liuying/20200706/new bgm ux
size_t obs_property_music_group_item_count(obs_property_t *p)
{
	struct music_group_data *data = get_music_group_data(p);
	return data ? data->items.num : 0;
}

//PRISM/Liuying/20200706/new bgm ux
const char *obs_property_music_group_item_name(obs_property_t *p, size_t idx)
{
	struct music_group_data *data = get_music_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].name
					       : NULL;
}

//PRISM/Liuying/20200706/new bgm ux
const char *obs_property_music_group_item_producer(obs_property_t *p,
						   size_t idx)
{
	struct music_group_data *data = get_music_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].producer
					       : NULL;
}

//PRISM/Liuying/20200706/new bgm ux
const char *obs_property_music_group_item_url(obs_property_t *p, size_t idx)
{
	struct music_group_data *data = get_music_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].url
					       : NULL;
}

//PRISM/Liuying/20200706/new bgm ux
int obs_property_music_group_item_duration(obs_property_t *p, size_t idx)
{
	struct music_group_data *data = get_music_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].duration
					       : 0;
}

//PRISM/Liuying/20200706/new bgm ux
int obs_property_music_group_item_duration_type(obs_property_t *p, size_t idx)
{
	struct music_group_data *data = get_music_group_data(p);
	return (data && idx < data->items.num)
		       ? data->items.array[idx].duration_type
		       : -1;
}

//PRISM/Liuying/20200706/new bgm ux
obs_property_t *obs_properties_add_bgm_music_list(obs_properties_t *props,
						  const char *name,
						  const char *desc)
{
	if (!props || has_prop(props, name))
		return NULL;

	return new_prop(props, name, desc, OBS_PROPERTY_BGM_MUSIC_LIST);
}

//PRISM/Liuying/20200706/new bgm ux
obs_property_t *obs_properties_add_tips(obs_properties_t *props,
					const char *name,
					const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, description, OBS_PROPERTY_TIPS);
}

//PRISM/Wangshaohui/20200914/Noissue/region source
obs_property_t *obs_properties_add_region_select(obs_properties_t *props,
						 const char *name,
						 const char *desc)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, desc, OBS_PROPERTY_REGION_SELECT);
}

obs_property_t *obs_properties_add_color(obs_properties_t *props,
					 const char *name, const char *desc)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, desc, OBS_PROPERTY_COLOR);
}

obs_property_t *obs_properties_add_button(obs_properties_t *props,
					  const char *name, const char *text,
					  obs_property_clicked_t callback)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, text, OBS_PROPERTY_BUTTON);
	struct button_data *data = get_property_data(p);
	data->callback = callback;
	return p;
}

obs_property_t *obs_properties_add_button2(obs_properties_t *props,
					   const char *name, const char *text,
					   obs_property_clicked_t callback,
					   void *priv)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, text, OBS_PROPERTY_BUTTON);
	struct button_data *data = get_property_data(p);
	data->callback = callback;
	p->priv = priv;
	return p;
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
static inline bool is_button_group(struct obs_property *p)
{
	return p->type == OBS_PROPERTY_BUTTON_GROUP;
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
static inline struct button_group_data *
get_button_group_data(struct obs_property *p)
{
	if (!p || !is_button_group(p))
		return NULL;

	return get_property_data(p);
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
obs_property_t *obs_properties_add_button_group(obs_properties_t *props,
						const char *name,
						const char *desc)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_BUTTON_GROUP);
	struct button_group_data *data = get_property_data(p);
	return p;
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
obs_property_t *obs_properties_add_button2_group(obs_properties_t *props,
						 const char *name,
						 const char *desc, void *priv)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_BUTTON_GROUP);
	struct button_group_data *data = get_property_data(p);
	p->priv = priv;
	return p;
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
static size_t add_group_item(struct button_group_data *data, const char *name,
			     const char *text, bool enabled,
			     obs_property_clicked_t callback)
{
	struct button_group_item *item =
		bzalloc(sizeof(struct button_group_item));
	item->name = bstrdup(name);
	item->text = bstrdup(text);
	item->enabled = enabled;
	item->callback = callback;
	//PRISM/RenJinbo/20210708/#8551/add button highlight style
	item->highlight = false;

	return da_push_back(data->items, item);
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
size_t obs_property_button_group_add_item(obs_property_t *p, const char *name,
					  const char *text, bool enabled,
					  obs_property_clicked_t callback)
{
	struct button_group_data *data = get_button_group_data(p);
	if (data)
		return add_group_item(data, name, text, enabled, callback);
	return 0;
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
void obs_property_button_group_clear(obs_property_t *p)
{
	struct button_group_data *data = get_button_group_data(p);
	if (!data) {
		return;
	}
	for (size_t i = 0; i < data->items.num; i++) {
		struct button_group_item *item = data->items.array + i;
		bfree(item->name);
		bfree(item->text);
	}
	da_free(data->items);
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
size_t obs_property_button_group_item_count(obs_property_t *p)
{
	struct button_group_data *data = get_button_group_data(p);
	return data ? data->items.num : 0;
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
const char *obs_property_button_group_item_name(obs_property_t *p, size_t idx)
{
	struct button_group_data *data = get_button_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].name
					       : NULL;
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
const char *obs_property_button_group_item_text(obs_property_t *p, size_t idx)
{
	struct button_group_data *data = get_button_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].text
					       : NULL;
}

//PRISM/RenJinbo/20210611/No issue/add enable properties
bool obs_property_button_group_item_enable(obs_property_t *p, size_t idx)
{
	struct button_group_data *data = get_button_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].enabled
					       : true;
}

//PRISM/Liuying/20200707/#3266/add new interface
int obs_property_button_group_get_idx_by_name(obs_property_t *p,
					      const char *name)
{
	struct button_group_data *data = get_button_group_data(p);
	if (!data) {
		return -1;
	}
	for (size_t i = 0; i < data->items.num; i++) {
		struct button_group_item *item = data->items.array + i;
		if (0 == strcmp(item->name, name)) {
			return i;
		}
	}
	return -1;
}

//PRISM/Liuying/20200617/No issue/for the same row of buttons
void obs_property_button_group_set_item_text(obs_property_t *p, size_t idx,
					     const char *text)
{
	struct button_group_data *data = get_button_group_data(p);
	if (!data) {
		return;
	}

	bfree(data->items.array[idx].text);

	data->items.array[idx].text = bstrdup(text);
}

//PRISM/RenJinbo/20210611/No issue/add enable properties
void obs_property_button_group_set_item_enable(obs_property_t *p, size_t idx,
					       bool enable)
{
	struct button_group_data *data = get_button_group_data(p);
	if (!data) {
		return;
	}
	data->items.array[idx].enabled = enable;
}

//PRISM/RenJinbo/20210708/#8551/add button highlight style
void obs_property_button_group_set_item_highlight(obs_property_t *p, size_t idx,
						  bool highlight)
{

	struct button_group_data *data = get_button_group_data(p);
	if (!data) {
		return;
	}
	data->items.array[idx].highlight = highlight;
}

//PRISM/RenJinbo/20210708/#8551/add button highlight style
bool obs_property_button_group_get_item_highlight(obs_property_t *p, size_t idx)
{
	struct button_group_data *data = get_button_group_data(p);
	if (!data) {
		return false;
	}
	return data->items.array[idx].highlight;
}

//PRISM/Zhangdewen/20200901/feature/for chat source
obs_property_t *obs_properties_add_chat_template_list(obs_properties_t *props,
						      const char *name,
						      const char *desc)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_CHAT_TEMPLATE_LIST);
	return p;
}

//PRISM/Zhangdewen/20200901/feature/for chat source
obs_property_t *obs_properties_add_chat_font_size(obs_properties_t *props,
						  const char *name,
						  const char *desc, int min,
						  int max, int step)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_CHAT_FONT_SIZE);

	struct chat_font_size_data *data = get_property_data(p);
	data->min = min;
	data->max = max;
	data->step = step;
	return p;
}

//PRISM/Chengbing/20200907/feature/for text motion source
obs_property_t *obs_properties_add_tm_content(obs_properties_t *props,
					      const char *name,
					      const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p = new_prop(props, name, description,
					  OBS_PROPERTY_TM_TEXT_CONTENT);
	return p;
}

//PRISM/Chengbing/20200907/feature/for text motion source
obs_property_t *obs_properties_add_tm_tab(obs_properties_t *props,
					  const char *name)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p = new_prop(props, name, "", OBS_PROPERTY_TM_TAB);
	return p;
}
//PRISM/Chengbing/20200907/feature/for text motion source
obs_property_t *obs_properties_add_tm_template_tab(obs_properties_t *props,
						   const char *name)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, "", OBS_PROPERTY_TM_TEMPLATE_TAB);
	return p;
}
//PRISM/Chengbing/20200907/feature/for text motion source
obs_property_t *obs_properties_add_tm_template_list(obs_properties_t *props,
						    const char *name)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, "", OBS_PROPERTY_TM_TEMPLATE_LIST);
	return p;
}

//PRISM/Chengbing/20200902/feature/for text motion source
obs_property_t *obs_properties_add_tm_text(obs_properties_t *props,
					   const char *name, const char *desc,
					   int min, int max, int step)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_TM_TEXT);

	struct tm_text_data *data = get_property_data(p);
	data->min = min;
	data->max = max;
	data->step = step;
	return p;
}

//PRISM/Chengbing/20200907/feature/for text motion source
obs_property_t *obs_properties_add_tm_color(obs_properties_t *props,
					    const char *name, const char *desc,
					    int min, int max, int step)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_TM_COLOR);
	struct tm_text_data *data = get_property_data(p);
	data->min = min;
	data->max = max;
	data->step = step;
	return p;
}

//PRISM/Chengbing/20200907/feature/for text motion source
obs_property_t *obs_properties_add_tm_motion(obs_properties_t *props,
					     const char *name,
					     const char *description, int min,
					     int max, int step)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, description, OBS_PROPERTY_TM_MOTION);
	struct tm_text_data *data = get_property_data(p);
	data->min = min;
	data->max = max;
	data->step = step;
	return p;
}

//PRISM/Zengqin/20201021/feature/for spectralizer source
obs_property_t *obs_properties_add_image_group(obs_properties_t *props,
					       const char *name,
					       const char *desc, int row,
					       int column,
					       enum obs_image_style_type type)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_IMAGE_GROUP);
	struct image_group_data *data = get_property_data(p);
	data->row = row;
	data->column = column;
	data->type = type;
	return p;
}

//PRISM/RenJinBo/20200622/feature/for timer source
bool obs_property_image_group_clicked(obs_property_t *p, void *obj, size_t idx)
{
	struct obs_context_data *context = obj;
	if (p) {
		struct image_group_data *data =
			get_type_data(p, OBS_PROPERTY_IMAGE_GROUP);

		if (!data) {
			return false;
		}

		if (idx >= obs_property_image_group_item_count(p)) {
			return false;
		}

		struct image_group_item item = data->items.array[idx];
		if (item.callback) {
			obs_properties_t *top = get_topmost_parent(p->parent);
			if (p->priv)
				return item.callback(top, p, p->priv);
			return item.callback(top, p,
					     (context ? context->data : NULL));
		}
	}

	return false;
}

//PRISM/Zengqin/20201021/no issue/add item for images group
size_t obs_property_image_group_add_item(obs_property_t *props,
					 const char *name, const char *url,
					 long long val,
					 obs_property_clicked_t callback)
{
	if (!props || !props->type == OBS_PROPERTY_IMAGE_GROUP)
		return 0;

	struct image_group_data *data = get_property_data(props);
	if (data) {
		struct image_group_item *item =
			bzalloc(sizeof(struct image_group_item));
		item->name = bstrdup(name);
		item->url = bstrdup(url);
		item->val = val;
		item->callback = callback;
		return da_push_back(data->items, item);
	}
	return 0;
}

//PRISM/Zengqin/20201021/no issue/image group
size_t obs_property_image_group_item_count(obs_property_t *props)
{
	if (!props || !props->type == OBS_PROPERTY_IMAGE_GROUP)
		return 0;

	struct image_group_data *data = get_property_data(props);
	return data ? data->items.num : 0;
}

//PRISM/Zengqin/20201102/no issue/for images group
void obs_property_image_group_params(obs_property_t *prop, int *row, int *colum,
				     enum obs_image_style_type *type)
{
	if (!prop || !prop->type == OBS_PROPERTY_IMAGE_GROUP)
		return;
	struct image_group_data *data = get_property_data(prop);
	*row = data->row;
	*colum = data->column;
	*type = data->type;
}

//PRISM/Zengqin/20201116/no issue/for images group
const char *obs_property_image_group_item_url(obs_property_t *prop, int idx)
{
	if (!prop || !prop->type == OBS_PROPERTY_IMAGE_GROUP)
		return NULL;

	struct image_group_data *data = get_property_data(prop);
	return (data && idx < data->items.num) ? data->items.array[idx].url
					       : NULL;
}

//PRISM/Zengqin/20201125/no issue/for image group
const char *obs_property_image_group_item_name(obs_property_t *prop, int idx)
{
	if (!prop || !prop->type == OBS_PROPERTY_IMAGE_GROUP)
		return NULL;

	struct image_group_data *data = get_property_data(prop);
	return (data && idx < data->items.num) ? data->items.array[idx].name
					       : NULL;
}

//PRISM/Zengqin/20201021/no issue/for custom group
obs_property_t *obs_properties_add_custom_group(obs_properties_t *props,
						const char *name,
						const char *desc, int row,
						int column)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_CUSTOM_GROUP);
	struct custom_group_data *data = get_property_data(p);
	data->row = row;
	data->column = column;
	return p;
}

//PRISM/Zengqin/20201030/no issue/for custom group
size_t obs_properties_custom_group_add_int(obs_property_t *prop,
					   const char *name, const char *desc,
					   int min, int max, int step,
					   char *suffix)
{
	if (!prop || !prop->type == OBS_PROPERTY_CUSTOM_GROUP)
		return 0;

	struct custom_group_data *data = get_property_data(prop);
	if (data) {
		struct custom_group_item *item =
			bzalloc(sizeof(struct custom_group_item));

		struct int_data id;
		id.min = min;
		id.max = max;
		id.step = step;
		id.suffix = bstrdup(suffix);
		id.type = OBS_NUMBER_SCROLLER;

		item->id = id;
		item->name = bstrdup(name);
		item->desc = bstrdup(desc);
		item->type = OBS_CONTROL_INT;
		return da_push_back(data->items, item);
	}
	return 0;
}

//PRISM/Zengqin/20201103/no issue/for custom group
void obs_properties_custom_group_int_params(obs_property_t *prop, int *min,
					    int *max, int *step, size_t idx)
{
	if (!prop || prop->type != OBS_PROPERTY_CUSTOM_GROUP)
		return;

	struct custom_group_data *data = get_property_data(prop);
	if (data && idx < data->items.num &&
	    data->items.array[idx].type == OBS_CONTROL_INT) {
		struct int_data id = data->items.array[idx].id;
		*min = id.min;
		*max = id.max;
		*step = id.step;
	}
	return;
}

//PRISM/Zengqin/20201216/no issue/for custom group
void obs_properties_custom_group_set_int_params(obs_property_t *prop, int min,
						int max, int step, size_t idx)
{
	if (!prop || prop->type != OBS_PROPERTY_CUSTOM_GROUP)
		return;

	struct custom_group_data *data = get_property_data(prop);
	if (data && idx < data->items.num &&
	    data->items.array[idx].type == OBS_CONTROL_INT) {
		data->items.array[idx].id.min = min;
		data->items.array[idx].id.max = max;
		data->items.array[idx].id.step = step;
	}
	return;
}

//PRISM/Zengqin/20201103/no issue/for custom group
const char *obs_property_custom_group_int_suffix(obs_property_t *prop,
						 size_t idx)
{
	if (!prop || prop->type != OBS_PROPERTY_CUSTOM_GROUP)
		return NULL;

	struct custom_group_data *data = get_property_data(prop);
	if (data && idx < data->items.num &&
	    data->items.array[idx].type == OBS_CONTROL_INT) {
		struct int_data id = data->items.array[idx].id;
		return id.suffix;
	}
	return NULL;
}

//PRISM/Zengqin/20201027/no issue/for custom group
void obs_property_custom_group_row_column(obs_property_t *prop, int *row,
					  int *colum)
{
	if (!prop || !prop->type == OBS_PROPERTY_CUSTOM_GROUP)
		return;
	struct custom_group_data *data = get_property_data(prop);
	*row = data->row;
	*colum = data->column;
}

//PRISM/Zengqin/20201023/no issue/for custom group
size_t obs_property_custom_group_item_count(obs_property_t *props)
{
	if (!props || !props->type == OBS_PROPERTY_CUSTOM_GROUP)
		return 0;

	struct custom_group_data *data = get_property_data(props);
	return data ? data->items.num : 0;
}

//PRISM/Zengqin/20201023/no issue/for custom group
enum obs_control_type obs_property_custom_group_item_type(obs_property_t *prop,
							  size_t idx)
{
	if (!prop || prop->type != OBS_PROPERTY_CUSTOM_GROUP)
		return OBS_CONTROL_UNKNOWN;

	struct custom_group_data *data = get_property_data(prop);
	return (data && idx < data->items.num) ? data->items.array[idx].type
					       : OBS_CONTROL_UNKNOWN;
}

//PRISM/Zengqin/20201026/no issue/for custom group
const char *obs_property_custom_group_item_name(obs_property_t *prop,
						size_t idx)
{
	if (!prop || prop->type != OBS_PROPERTY_CUSTOM_GROUP)
		return NULL;

	struct custom_group_data *data = get_property_data(prop);
	return (data && idx < data->items.num) ? data->items.array[idx].name
					       : NULL;
}

//PRISM/Zengqin/20201103/no issue/for custom group
const char *obs_property_custom_group_item_desc(obs_property_t *prop,
						size_t idx)
{
	if (!prop || prop->type != OBS_PROPERTY_CUSTOM_GROUP)
		return NULL;

	struct custom_group_data *data = get_property_data(prop);
	return (data && idx < data->items.num) ? data->items.array[idx].desc
					       : NULL;
}

//PRISM/Zengqin/20201027/no issue/for horizontal line
obs_property_t *obs_properties_add_h_line(obs_properties_t *props,
					  const char *name, const char *desc)
{
	if (!props)
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_H_LINE);
	return p;
}

//PRISM/Zengqin/20201030/no issue/This checkbox text on the left
obs_property_t *obs_properties_add_bool_left(obs_properties_t *props,
					     const char *name, const char *desc)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, desc, OBS_PROPERTY_BOOL_LEFT);
}

//PRISM/Zhangdewen/20201022/feature/for virtual background
obs_property_t *obs_properties_add_camera_virtual_background_state(
	obs_properties_t *props, const char *name, const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, description,
			 OBS_PROPERTY_CAMERA_VIRTUAL_BACKGROUND_STATE);
	return p;
}

//PRISM/Zhangdewen/20201023/feature/for virtual background
obs_property_t *obs_properties_add_virtual_background_resource(
	obs_properties_t *props, const char *name, const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, description,
			 OBS_PROPERTY_VIRTUAL_BACKGROUND_RESOURCE);
	return p;
}

//PRISM/Zhangdewen/20201027/feature/for virtual background
obs_property_t *obs_properties_add_switch(obs_properties_t *props,
					  const char *name,
					  const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, description, OBS_PROPERTY_SWITCH);
	return p;
}

obs_property_t *obs_properties_add_font(obs_properties_t *props,
					const char *name, const char *desc)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, desc, OBS_PROPERTY_FONT);
}

obs_property_t *
obs_properties_add_editable_list(obs_properties_t *props, const char *name,
				 const char *desc,
				 enum obs_editable_list_type type,
				 const char *filter, const char *default_path)
{
	if (!props || has_prop(props, name))
		return NULL;
	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_EDITABLE_LIST);

	struct editable_list_data *data = get_property_data(p);
	data->type = type;
	data->filter = bstrdup(filter);
	data->default_path = bstrdup(default_path);
	return p;
}

obs_property_t *obs_properties_add_frame_rate(obs_properties_t *props,
					      const char *name,
					      const char *desc)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p =
		new_prop(props, name, desc, OBS_PROPERTY_FRAME_RATE);

	struct frame_rate_data *data = get_property_data(p);
	da_init(data->extra_options);
	da_init(data->ranges);
	return p;
}

static bool check_property_group_recursion(obs_properties_t *parent,
					   obs_properties_t *group)
{
	/* Scan the group for the parent. */
	obs_property_t *current_property = group->first_property;
	while (current_property) {
		if (current_property->type == OBS_PROPERTY_GROUP) {
			obs_properties_t *cprops =
				obs_property_group_content(current_property);
			if (cprops == parent) {
				/* Contains find_props */
				return true;
			} else if (cprops == group) {
				/* Contains self, shouldn't be possible but
				 * lets verify anyway. */
				return true;
			}
			check_property_group_recursion(cprops, group);
		}

		current_property = current_property->next;
	}

	return false;
}

static bool check_property_group_duplicates(obs_properties_t *parent,
					    obs_properties_t *group)
{
	obs_property_t *current_property = group->first_property;
	while (current_property) {
		if (has_prop(parent, current_property->name)) {
			return true;
		}

		current_property = current_property->next;
	}

	return false;
}

obs_property_t *obs_properties_add_group(obs_properties_t *props,
					 const char *name, const char *desc,
					 enum obs_group_type type,
					 obs_properties_t *group)
{
	if (!props || has_prop(props, name))
		return NULL;
	if (!group)
		return NULL;

	/* Prevent recursion. */
	if (props == group)
		return NULL;
	if (check_property_group_recursion(props, group))
		return NULL;

	/* Prevent duplicate properties */
	if (check_property_group_duplicates(props, group))
		return NULL;

	obs_property_t *p = new_prop(props, name, desc, OBS_PROPERTY_GROUP);
	group->parent = p;

	struct group_data *data = get_property_data(p);
	data->type = type;
	data->content = group;
	return p;
}

/* ------------------------------------------------------------------------- */

static inline bool is_combo(struct obs_property *p)
{
	return p->type == OBS_PROPERTY_LIST ||
	       p->type == OBS_PROPERTY_TIMER_LIST_LISTEN;
}

static inline struct list_data *get_list_data(struct obs_property *p)
{
	if (!p || !is_combo(p))
		return NULL;

	return get_property_data(p);
}

static inline struct list_data *get_list_fmt_data(struct obs_property *p,
						  enum obs_combo_format format)
{
	struct list_data *data = get_list_data(p);
	return (data && data->format == format) ? data : NULL;
}

/* ------------------------------------------------------------------------- */

bool obs_property_next(obs_property_t **p)
{
	if (!p || !*p)
		return false;

	*p = (*p)->next;
	return *p != NULL;
}

void obs_property_set_modified_callback(obs_property_t *p,
					obs_property_modified_t modified)
{
	if (p)
		p->modified = modified;
}

void obs_property_set_modified_callback2(obs_property_t *p,
					 obs_property_modified2_t modified2,
					 void *priv)
{
	if (p) {
		p->modified2 = modified2;
		p->priv = priv;
	}
}

bool obs_property_modified(obs_property_t *p, obs_data_t *settings)
{
	if (p) {
		if (p->modified) {
			obs_properties_t *top = get_topmost_parent(p->parent);
			return p->modified(top, p, settings);
		} else if (p->modified2) {
			obs_properties_t *top = get_topmost_parent(p->parent);
			return p->modified2(p->priv, top, p, settings);
		}
	}
	return false;
}

bool obs_property_button_clicked(obs_property_t *p, void *obj)
{
	struct obs_context_data *context = obj;
	if (p) {
		struct button_data *data =
			get_type_data(p, OBS_PROPERTY_BUTTON);
		if (data && data->callback) {
			obs_properties_t *top = get_topmost_parent(p->parent);
			if (p->priv)
				return data->callback(top, p, p->priv);
			return data->callback(top, p,
					      (context ? context->data : NULL));
		}
	}

	return false;
}

bool obs_property_button_group_clicked(obs_property_t *p, void *obj, size_t idx)
{
	struct obs_context_data *context = obj;
	if (p) {
		struct button_group_data *data =
			get_type_data(p, OBS_PROPERTY_BUTTON_GROUP);
		if (!data) {
			return false;
		}

		if (idx >= obs_property_button_group_item_count(p)) {
			return false;
		}

		struct button_group_item item = data->items.array[idx];
		if (item.callback) {
			obs_properties_t *top = get_topmost_parent(p->parent);
			if (p->priv)
				return item.callback(top, p, p->priv);
			return item.callback(top, p,
					     (context ? context->data : NULL));
		}
	}

	return false;
}
bool obs_property_bool_group_clicked(obs_property_t *p, void *obj, size_t idx)
{
	struct obs_context_data *context = obj;
	if (p) {
		struct bool_group_data *data =
			get_type_data(p, OBS_PROPERTY_BOOL_GROUP);

		if (!data) {
			return false;
		}

		if (idx >= obs_property_bool_group_item_count(p)) {
			return false;
		}

		struct bool_data item = data->items.array[idx];
		if (item.callback) {
			obs_properties_t *top = get_topmost_parent(p->parent);
			if (p->priv)
				return item.callback(top, p, p->priv);
			return item.callback(top, p,
					     (context ? context->data : NULL));
		}
	}

	return false;
}

//PRISM/Liuying/20200707/#3266/add new interface
void obs_property_button_group_clicked_by_name(obs_property_t *p, void *obj,
					       const char *name)
{
	int index = obs_property_button_group_get_idx_by_name(p, name);
	if (-1 == index) {
		return;
	}

	obs_property_button_group_clicked(p, obj, index);
}

bool obs_property_text_button_clicked(obs_property_t *p, void *obj)
{
	struct obs_context_data *context = obj;
	if (p) {
		struct text_button_data *data =
			get_type_data(p, OBS_PROPERTY_MOBILE_NAME);
		if (data && data->callback) {
			obs_properties_t *top = get_topmost_parent(p->parent);
			if (p->priv)
				return data->callback(top, p, p->priv);
			return data->callback(top, p,
					      (context ? context->data : NULL));
		}
	}

	return false;
}

void obs_property_set_visible(obs_property_t *p, bool visible)
{
	if (p)
		p->visible = visible;
}

void obs_property_set_enabled(obs_property_t *p, bool enabled)
{
	if (p)
		p->enabled = enabled;
}

void obs_property_set_description(obs_property_t *p, const char *description)
{
	if (p) {
		bfree(p->desc);
		p->desc = description && *description ? bstrdup(description)
						      : NULL;
	}
}

void obs_property_set_long_description(obs_property_t *p, const char *long_desc)
{
	if (p) {
		bfree(p->long_desc);
		p->long_desc = long_desc && *long_desc ? bstrdup(long_desc)
						       : NULL;
	}
}

//PRISM/Zhangdewen/20200909/new ndi ux
void obs_property_set_placeholder(obs_property_t *p, const char *placeholder)
{
	if (p) {
		bfree(p->placeholder);
		p->placeholder = placeholder && *placeholder
					 ? bstrdup(placeholder)
					 : NULL;
	}
}

void obs_property_set_tooltip(obs_property_t *p, const char *tooltip)
{
	if (p) {
		bfree(p->tooltip);
		p->tooltip = tooltip && *tooltip ? bstrdup(tooltip) : NULL;
	}
}

//PRISM/RenJinbo/20210624/None/ignore call modified callback, when refresh properties.
void obs_property_set_ignore_callback_when_refresh(obs_property_t *p,
						   bool ignore)
{
	if (p)
		p->ignoreCallbackWhenRefresh = ignore;
}

const char *obs_property_name(obs_property_t *p)
{
	return p ? p->name : NULL;
}

const char *obs_property_description(obs_property_t *p)
{
	return p ? p->desc : NULL;
}

const char *obs_property_long_description(obs_property_t *p)
{
	return p ? p->long_desc : NULL;
}

//PRISM/Zhangdewen/20200909/new ndi ux
const char *obs_property_placeholder(obs_property_t *p)
{
	return p ? p->placeholder : NULL;
}

const char *obs_property_tooltip(obs_property_t *p)
{
	return p ? p->tooltip : NULL;
}

enum obs_property_type obs_property_get_type(obs_property_t *p)
{
	return p ? p->type : OBS_PROPERTY_INVALID;
}

bool obs_property_enabled(obs_property_t *p)
{
	return p ? p->enabled : false;
}

bool obs_property_visible(obs_property_t *p)
{
	return p ? p->visible : false;
}
//PRISM/RenJinbo/20210628/#none/Timer source feature
bool obs_property_ignore_callback_when_refresh(obs_property_t *p, bool ignore)
{
	return p ? p->ignoreCallbackWhenRefresh : false;
}

int obs_property_int_min(obs_property_t *p)
{
	struct int_data *data = get_type_data(p, OBS_PROPERTY_INT);
	return data ? data->min : 0;
}

int obs_property_int_max(obs_property_t *p)
{
	struct int_data *data = get_type_data(p, OBS_PROPERTY_INT);
	return data ? data->max : 0;
}

int obs_property_int_step(obs_property_t *p)
{
	struct int_data *data = get_type_data(p, OBS_PROPERTY_INT);
	return data ? data->step : 0;
}

enum obs_number_type obs_property_int_type(obs_property_t *p)
{
	struct int_data *data = get_type_data(p, OBS_PROPERTY_INT);
	return data ? data->type : OBS_NUMBER_SCROLLER;
}

const char *obs_property_int_suffix(obs_property_t *p)
{
	struct int_data *data = get_type_data(p, OBS_PROPERTY_INT);
	return data ? data->suffix : NULL;
}

double obs_property_float_min(obs_property_t *p)
{
	struct float_data *data = get_type_data(p, OBS_PROPERTY_FLOAT);
	return data ? data->min : 0;
}

double obs_property_float_max(obs_property_t *p)
{
	struct float_data *data = get_type_data(p, OBS_PROPERTY_FLOAT);
	return data ? data->max : 0;
}

double obs_property_float_step(obs_property_t *p)
{
	struct float_data *data = get_type_data(p, OBS_PROPERTY_FLOAT);
	return data ? data->step : 0;
}

const char *obs_property_float_suffix(obs_property_t *p)
{
	struct float_data *data = get_type_data(p, OBS_PROPERTY_FLOAT);
	return data ? data->suffix : NULL;
}

enum obs_number_type obs_property_float_type(obs_property_t *p)
{
	struct float_data *data = get_type_data(p, OBS_PROPERTY_FLOAT);
	return data ? data->type : OBS_NUMBER_SCROLLER;
}

enum obs_text_type obs_property_text_type(obs_property_t *p)
{
	struct text_data *data = get_type_data(p, OBS_PROPERTY_TEXT);
	return data ? data->type : OBS_TEXT_DEFAULT;
}

enum obs_path_type obs_property_path_type(obs_property_t *p)
{
	struct path_data *data = get_type_data(p, OBS_PROPERTY_PATH);
	return data ? data->type : OBS_PATH_DIRECTORY;
}

const char *obs_property_path_filter(obs_property_t *p)
{
	struct path_data *data = get_type_data(p, OBS_PROPERTY_PATH);
	return data ? data->filter : NULL;
}

const char *obs_property_path_default_path(obs_property_t *p)
{
	struct path_data *data = get_type_data(p, OBS_PROPERTY_PATH);
	return data ? data->default_path : NULL;
}

enum obs_combo_type obs_property_list_type(obs_property_t *p)
{
	struct list_data *data = get_list_data(p);
	return data ? data->type : OBS_COMBO_TYPE_INVALID;
}

enum obs_combo_format obs_property_list_format(obs_property_t *p)
{
	struct list_data *data = get_list_data(p);
	return data ? data->format : OBS_COMBO_FORMAT_INVALID;
}

//PRISM/Zhangdewen/20200916/new ndi ux
void obs_property_set_list_readonly(obs_property_t *p, bool readonly)
{
	struct list_data *data = get_list_data(p);
	if (data) {
		data->readonly = readonly;
	}
}

//PRISM/Zhangdewen/20200916/new ndi ux
bool obs_property_list_readonly(obs_property_t *p)
{
	struct list_data *data = get_list_data(p);
	return data ? data->readonly : false;
}

const char *obs_property_text_button_text(obs_property_t *p)
{
	struct text_button_data *data =
		get_type_data(p, OBS_PROPERTY_MOBILE_NAME);
	return data->text;
}

void obs_property_int_set_limits(obs_property_t *p, int min, int max, int step)
{
	struct int_data *data = get_type_data(p, OBS_PROPERTY_INT);
	if (!data)
		return;

	data->min = min;
	data->max = max;
	data->step = step;
}

void obs_property_float_set_limits(obs_property_t *p, double min, double max,
				   double step)
{
	struct float_data *data = get_type_data(p, OBS_PROPERTY_FLOAT);
	if (!data)
		return;

	data->min = min;
	data->max = max;
	data->step = step;
}

void obs_property_int_set_suffix(obs_property_t *p, const char *suffix)
{
	struct int_data *data = get_type_data(p, OBS_PROPERTY_INT);
	if (!data)
		return;

	bfree(data->suffix);
	data->suffix = bstrdup(suffix);
}

void obs_property_float_set_suffix(obs_property_t *p, const char *suffix)
{
	struct float_data *data = get_type_data(p, OBS_PROPERTY_FLOAT);
	if (!data)
		return;

	bfree(data->suffix);
	data->suffix = bstrdup(suffix);
}

void obs_property_list_clear(obs_property_t *p)
{
	struct list_data *data = get_list_data(p);
	if (data)
		list_data_free(data);
}

static size_t add_item(struct list_data *data, const char *name,
		       const void *val)
{
	struct list_item item = {NULL};
	item.name = bstrdup(name);

	if (data->format == OBS_COMBO_FORMAT_INT)
		item.ll = *(const long long *)val;
	else if (data->format == OBS_COMBO_FORMAT_FLOAT)
		item.d = *(const double *)val;
	else
		item.str = bstrdup(val);

	return da_push_back(data->items, &item);
}

static void insert_item(struct list_data *data, size_t idx, const char *name,
			const void *val)
{
	struct list_item item = {NULL};
	item.name = bstrdup(name);

	if (data->format == OBS_COMBO_FORMAT_INT)
		item.ll = *(const long long *)val;
	else if (data->format == OBS_COMBO_FORMAT_FLOAT)
		item.d = *(const double *)val;
	else
		item.str = bstrdup(val);

	da_insert(data->items, idx, &item);
}

size_t obs_property_list_add_string(obs_property_t *p, const char *name,
				    const char *val)
{
	struct list_data *data = get_list_data(p);
	if (data && data->format == OBS_COMBO_FORMAT_STRING)
		return add_item(data, name, val);
	return 0;
}

size_t obs_property_list_add_int(obs_property_t *p, const char *name,
				 long long val)
{
	struct list_data *data = get_list_data(p);
	if (data && data->format == OBS_COMBO_FORMAT_INT)
		return add_item(data, name, &val);
	return 0;
}

size_t obs_property_list_add_float(obs_property_t *p, const char *name,
				   double val)
{
	struct list_data *data = get_list_data(p);
	if (data && data->format == OBS_COMBO_FORMAT_FLOAT)
		return add_item(data, name, &val);
	return 0;
}

void obs_property_list_insert_string(obs_property_t *p, size_t idx,
				     const char *name, const char *val)
{
	struct list_data *data = get_list_data(p);
	if (data && data->format == OBS_COMBO_FORMAT_STRING)
		insert_item(data, idx, name, val);
}

void obs_property_list_insert_int(obs_property_t *p, size_t idx,
				  const char *name, long long val)
{
	struct list_data *data = get_list_data(p);
	if (data && data->format == OBS_COMBO_FORMAT_INT)
		insert_item(data, idx, name, &val);
}

void obs_property_list_insert_float(obs_property_t *p, size_t idx,
				    const char *name, double val)
{
	struct list_data *data = get_list_data(p);
	if (data && data->format == OBS_COMBO_FORMAT_FLOAT)
		insert_item(data, idx, name, &val);
}

void obs_property_list_item_remove(obs_property_t *p, size_t idx)
{
	struct list_data *data = get_list_data(p);
	if (data && idx < data->items.num) {
		list_item_free(data, data->items.array + idx);
		da_erase(data->items, idx);
	}
}

size_t obs_property_list_item_count(obs_property_t *p)
{
	struct list_data *data = get_list_data(p);
	return data ? data->items.num : 0;
}

bool obs_property_list_item_disabled(obs_property_t *p, size_t idx)
{
	struct list_data *data = get_list_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].disabled
					       : false;
}

void obs_property_list_item_disable(obs_property_t *p, size_t idx,
				    bool disabled)
{
	struct list_data *data = get_list_data(p);
	if (!data || idx >= data->items.num)
		return;
	data->items.array[idx].disabled = disabled;
}

//PRISM/WangShaohui/20210922/noIssue/add tooltips for combox list
void obs_property_list_item_set_tips(obs_property_t *p, size_t idx,
				     const char *tips)
{
	struct list_data *data = get_list_data(p);
	if (data && idx < data->items.num) {
		if (data->items.array[idx].tips) {
			bfree(data->items.array[idx].tips);
			data->items.array[idx].tips = NULL;
		}

		if (tips)
			data->items.array[idx].tips = bstrdup(tips);
	}
}

//PRISM/WangShaohui/20210922/noIssue/add tooltips for combox list
const char *obs_property_list_item_get_tips(obs_property_t *p, size_t idx)
{
	struct list_data *data = get_list_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].tips
					       : NULL;
}

const char *obs_property_list_item_name(obs_property_t *p, size_t idx)
{
	struct list_data *data = get_list_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].name
					       : NULL;
}

const char *obs_property_list_item_string(obs_property_t *p, size_t idx)
{
	struct list_data *data = get_list_fmt_data(p, OBS_COMBO_FORMAT_STRING);
	return (data && idx < data->items.num) ? data->items.array[idx].str
					       : NULL;
}

long long obs_property_list_item_int(obs_property_t *p, size_t idx)
{
	struct list_data *data = get_list_fmt_data(p, OBS_COMBO_FORMAT_INT);
	return (data && idx < data->items.num) ? data->items.array[idx].ll : 0;
}

double obs_property_list_item_float(obs_property_t *p, size_t idx)
{
	struct list_data *data = get_list_fmt_data(p, OBS_COMBO_FORMAT_FLOAT);
	return (data && idx < data->items.num) ? data->items.array[idx].d : 0.0;
}

enum obs_editable_list_type obs_property_editable_list_type(obs_property_t *p)
{
	struct editable_list_data *data =
		get_type_data(p, OBS_PROPERTY_EDITABLE_LIST);
	return data ? data->type : OBS_EDITABLE_LIST_TYPE_STRINGS;
}

const char *obs_property_editable_list_filter(obs_property_t *p)
{
	struct editable_list_data *data =
		get_type_data(p, OBS_PROPERTY_EDITABLE_LIST);
	return data ? data->filter : NULL;
}

const char *obs_property_editable_list_default_path(obs_property_t *p)
{
	struct editable_list_data *data =
		get_type_data(p, OBS_PROPERTY_EDITABLE_LIST);
	return data ? data->default_path : NULL;
}

/* ------------------------------------------------------------------------- */
/* OBS_PROPERTY_FRAME_RATE */

void obs_property_frame_rate_clear(obs_property_t *p)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	if (!data)
		return;

	frame_rate_data_options_free(data);
	frame_rate_data_ranges_free(data);
}

void obs_property_frame_rate_options_clear(obs_property_t *p)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	if (!data)
		return;

	frame_rate_data_options_free(data);
}

void obs_property_frame_rate_fps_ranges_clear(obs_property_t *p)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	if (!data)
		return;

	frame_rate_data_ranges_free(data);
}

size_t obs_property_frame_rate_option_add(obs_property_t *p, const char *name,
					  const char *description)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	if (!data)
		return DARRAY_INVALID;

	struct frame_rate_option *opt = da_push_back_new(data->extra_options);

	opt->name = bstrdup(name);
	opt->description = bstrdup(description);

	return data->extra_options.num - 1;
}

size_t obs_property_frame_rate_fps_range_add(obs_property_t *p,
					     struct media_frames_per_second min,
					     struct media_frames_per_second max)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	if (!data)
		return DARRAY_INVALID;

	struct frame_rate_range *rng = da_push_back_new(data->ranges);

	rng->min_time = min;
	rng->max_time = max;

	return data->ranges.num - 1;
}

void obs_property_frame_rate_option_insert(obs_property_t *p, size_t idx,
					   const char *name,
					   const char *description)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	if (!data)
		return;

	struct frame_rate_option *opt = da_insert_new(data->extra_options, idx);

	opt->name = bstrdup(name);
	opt->description = bstrdup(description);
}

void obs_property_frame_rate_fps_range_insert(
	obs_property_t *p, size_t idx, struct media_frames_per_second min,
	struct media_frames_per_second max)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	if (!data)
		return;

	struct frame_rate_range *rng = da_insert_new(data->ranges, idx);

	rng->min_time = min;
	rng->max_time = max;
}

size_t obs_property_frame_rate_options_count(obs_property_t *p)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	return data ? data->extra_options.num : 0;
}

const char *obs_property_frame_rate_option_name(obs_property_t *p, size_t idx)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	return data && data->extra_options.num > idx
		       ? data->extra_options.array[idx].name
		       : NULL;
}

const char *obs_property_frame_rate_option_description(obs_property_t *p,
						       size_t idx)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	return data && data->extra_options.num > idx
		       ? data->extra_options.array[idx].description
		       : NULL;
}

size_t obs_property_frame_rate_fps_ranges_count(obs_property_t *p)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	return data ? data->ranges.num : 0;
}

struct media_frames_per_second
obs_property_frame_rate_fps_range_min(obs_property_t *p, size_t idx)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	return data && data->ranges.num > idx
		       ? data->ranges.array[idx].min_time
		       : (struct media_frames_per_second){0};
}
struct media_frames_per_second
obs_property_frame_rate_fps_range_max(obs_property_t *p, size_t idx)
{
	struct frame_rate_data *data =
		get_type_data(p, OBS_PROPERTY_FRAME_RATE);
	return data && data->ranges.num > idx
		       ? data->ranges.array[idx].max_time
		       : (struct media_frames_per_second){0};
}

enum obs_text_type obs_proprety_text_type(obs_property_t *p)
{
	return obs_property_text_type(p);
}

enum obs_group_type obs_property_group_type(obs_property_t *p)
{
	struct group_data *data = get_type_data(p, OBS_PROPERTY_GROUP);
	return data ? data->type : OBS_COMBO_INVALID;
}

obs_properties_t *obs_property_group_content(obs_property_t *p)
{
	struct group_data *data = get_type_data(p, OBS_PROPERTY_GROUP);
	return data ? data->content : NULL;
}

size_t obs_property_bool_group_item_count(obs_property_t *p)
{
	struct bool_group_data *data = get_bool_group_data(p);
	return data ? data->items.num : 0;
}

const char *obs_property_bool_group_item_text(obs_property_t *p, size_t idx)
{
	struct bool_group_data *data = get_bool_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].text
					       : NULL;
}

//PRISM/Zhangdewen/20200901/feature/for chat source
int obs_property_chat_font_size_min(obs_property_t *p)
{
	struct chat_font_size_data *data =
		get_type_data(p, OBS_PROPERTY_CHAT_FONT_SIZE);
	return data ? data->min : 0;
}

//PRISM/Zhangdewen/20200901/feature/for chat source
int obs_property_chat_font_size_max(obs_property_t *p)
{
	struct chat_font_size_data *data =
		get_type_data(p, OBS_PROPERTY_CHAT_FONT_SIZE);
	return data ? data->max : 0;
}

//PRISM/Zhangdewen/20200901/feature/for chat source
int obs_property_chat_font_size_step(obs_property_t *p)
{
	struct chat_font_size_data *data =
		get_type_data(p, OBS_PROPERTY_CHAT_FONT_SIZE);
	return data ? data->step : 0;
}

//PRISM/Chengbing/20200902/feature/for text motion source
int obs_property_tm_text_min(obs_property_t *p, size_t propertyType)
{
	struct tm_text_data *data = get_type_data(p, propertyType);
	return data ? data->min : 0;
}

//PRISM/Chengbing/20200902/feature/for text motion source
int obs_property_tm_text_max(obs_property_t *p, size_t propertyType)
{
	struct tm_text_data *data = get_type_data(p, propertyType);
	return data ? data->max : 0;
}

//PRISM/Chengbing/20200902/feature/for text motion source
int obs_property_tm_text_step(obs_property_t *p, size_t propertyType)
{
	struct tm_text_data *data = get_type_data(p, propertyType);
	return data ? data->step : 0;
}

//PRISM/WangShaohui/20201029/#5497/limite text length
void obs_property_set_length_limit(obs_property_t *p, int max_length)
{
	p->length_limit = max_length;
}

//PRISM/WangShaohui/20201029/#5497/limite text length
int obs_property_get_length_limit(obs_property_t *p)
{
	return p->length_limit;
}

//PRISM/Zengqin/20201125/#none/for property UI
void obs_property_add_flags(obs_property_t *p, uint32_t flag)
{
	if (p) {
		p->flags |= flag;
	}
}

//PRISM/Zengqin/20201125/#none/for property UI
uint32_t obs_property_get_flags(obs_property_t *p)
{
	return p ? p->flags : 0;
}

bool obs_property_bool_group_item_enabled(obs_property_t *p, size_t idx)
{
	struct bool_group_data *data = get_bool_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].enabled
					       : false;
}

const char *obs_property_bool_group_item_tooltip(obs_property_t *p, size_t idx)
{
	struct bool_group_data *data = get_bool_group_data(p);
	return (data && idx < data->items.num) ? data->items.array[idx].tooltip
					       : NULL;
}

//PRISM/RenJinbo/20210628/#none/Timer source featuress
obs_property_t *obs_properties_add_int_group(obs_properties_t *props,
					     const char *name,
					     const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, description, OBS_PROPERTY_INT_GROUP);
}

//PRISM/RenJinbo/20210628/#none/Timer source feature
static inline struct int_group_data *get_int_group_data(struct obs_property *p)
{
	if (!p || p->type != OBS_PROPERTY_INT_GROUP) {
		return NULL;
	}
	return get_property_data(p);
}

//PRISM/RenJinbo/20210628/#none/Timer source feature
size_t obs_properties_add_int_group_item(obs_property_t *p, const char *name,
					 const char *desc, int min, int max,
					 int step)
{
	struct int_group_data *data = get_int_group_data(p);

	if (data) {
		struct int_data_item *item =
			bzalloc(sizeof(struct int_data_item));
		item->min = min;
		item->max = max;
		item->step = step;
		item->name = bstrdup(name);
		item->description = bstrdup(desc);

		return da_push_back(data->items, item);
	}

	return 0;
}

//PRISM/RenJinbo/20210628/#none/Timer source feature
size_t obs_property_int_group_item_count(obs_property_t *p)
{
	struct int_group_data *data = get_int_group_data(p);
	return data ? data->items.num : 0;
}

//PRISM/RenJinbo/20210628/#none/Timer source feature
void obs_property_int_group_item_params(obs_property_t *prop, char **subName,
					char **des, int *min, int *max,
					int *step, size_t idx)
{
	if (!prop || prop->type != OBS_PROPERTY_INT_GROUP)
		return;

	struct int_group_data *data = get_int_group_data(prop);
	if (!data || idx > data->items.num) {
		return;
	}

	//struct int_group_data *data = get_int_group_data(prop);
	struct int_data_item i_data = data->items.array[idx];
	*min = i_data.min;
	*max = i_data.max;
	*step = i_data.step;
	*des = i_data.description;
	*subName = i_data.name;
}

//PRISM/RenJinbo/20210628/#none/Timer source feature
obs_property_t *obs_properties_add_font_simple(obs_properties_t *props,
					       const char *name,
					       const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, description, OBS_PROPERTY_FONT_SIMPLE);
}

//PRISM/RenJinbo/20210628/#none/Timer source feature
obs_property_t *obs_properties_add_color_checkbox(obs_properties_t *props,
						  const char *name,
						  const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, description, OBS_PROPERTY_COLOR_CHECKBOX);
}

//PRISM/RenJinbo/20210628/#none/Timer source feature
obs_property_t *obs_properties_add_timer_list_Listen(obs_properties_t *props,
						     const char *name,
						     const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;

	struct obs_property *p = new_prop(props, name, description,
					  OBS_PROPERTY_TIMER_LIST_LISTEN);
	struct list_data *data = get_property_data(p);
	data->format = OBS_COMBO_FORMAT_STRING;
	data->type = OBS_COMBO_TYPE_LIST;
	data->readonly = false;

	return p;
}

//PRISM/RenJinbo/20210628/#none/Timer source feature
obs_property_t *obs_properties_add_label_tip(obs_properties_t *props,
					     const char *name,
					     const char *description)
{
	if (!props || has_prop(props, name))
		return NULL;
	return new_prop(props, name, description, OBS_PROPERTY_LABEL_TIP);
}

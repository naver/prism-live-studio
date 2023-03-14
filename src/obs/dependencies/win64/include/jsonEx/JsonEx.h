#pragma once

//-------------------------------------------------------------------------------------
enum JSON_ITEM_TYPE {
	IT_NONE = -1,
	IT_FALSE = 0,
	IT_TRUE = 1,
	IT_NULL = 2,
	IT_NUMBER = 3,
	IT_STRING = 4,
	IT_ARRAY = 5,
	IT_OBJECT = 6
};

union uVALUE {
	char *m_str;
	struct tIntValue {
		int m_nValue;
		double m_dValue;
	} m_num;
};

//-------------------------------------------------------------------------------------
class __declspec(dllexport) CJsonEx {
public:
	CJsonEx();
	~CJsonEx();

public:
	bool is_valid(JSON_ITEM_TYPE destType = IT_NONE);
	CJsonEx &get_root();
	CJsonEx &get_parent();

	bool parse_stream(const char *str);
	JSON_ITEM_TYPE get_item_type();
	const char *get_item_name();
	int get_array_size();
	CJsonEx &get_item_from_name(const char *name);
	CJsonEx &get_array_item_from_pos(int pos);
	CJsonEx &get_next_item();
	CJsonEx &get_prev_item();
	CJsonEx &get_sub_item();
	uVALUE get_item_value(JSON_ITEM_TYPE *it = 0);
	CJsonEx &get_item_value(char *&out); // free by json_mem_free
	CJsonEx &get_item_value(int &out, int defaultV = 0);
	CJsonEx &get_item_value(double &out, double defaultV = 0);

	// never need to call it when to create array object
	CJsonEx &new_root();
	// create object. never need to trans name when to create object of array.
	CJsonEx &new_object(const char *name = "");
	// create array.  note:	after calling new_array(), the first object has been created
	CJsonEx &new_array(const char *name);
	// call it immediately after calling new_***()
	CJsonEx &attach_object();

	void delete_item_from_array(int pos);
	void delete_item_from_object(const char *name);
	CJsonEx &set_int32(const char *name, int v);
	CJsonEx &set_int64(const char *name, long long v);
	CJsonEx &set_double(const char *name, double v);
	CJsonEx &set_bool(const char *name, bool v);
	CJsonEx &set_string(const char *name, const char *v);
	CJsonEx &set_int_group(const char *name, int *group, int count);
	CJsonEx &set_double_group(const char *name, double *group, int count);
	CJsonEx &set_string_group(const char *name, const char **group,
				  int count);

	// need to call json_mem_free() to free memory
	char *encode();
	void json_mem_free(void *mem);

private:
	struct impl;
	impl *self;
};

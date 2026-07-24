#pragma once
#if defined(_WIN32) && defined(PLS_UI_ACTION_STATS)
#include <Windows.h>

#define SELECT_REGION_SHOW_SIGNAL L"select_region_ui_show"

class CSignalEvent {
public:
	CSignalEvent(const wchar_t *name, bool force_reset);
	~CSignalEvent();

	void set_sign(bool sign_flag);
	bool get_sign();

private:
	HANDLE get_event(const wchar_t *pName, bool bNeedManualReset, bool *pNewCreate);

	HANDLE event = 0;
};

#endif // _WIN32

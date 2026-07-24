#include "win-signal.h"
#include <assert.h>
#if defined(_WIN32) && defined(PLS_UI_ACTION_STATS)

CSignalEvent::CSignalEvent(const wchar_t *name, bool force_reset)
{
	assert(name);
	if (name) {
		bool new_create = false;
		event = get_event(name, true, &new_create);
		if (!event) {
			assert(false);
			return;
		}

		if (new_create || force_reset) {
			ResetEvent(event);
		}
	}
}

CSignalEvent::~CSignalEvent()
{
	if (!event) {
		return;
	}

	ResetEvent(event);
	CloseHandle(event);
}

void CSignalEvent::set_sign(bool sign_flag)
{
	if (!event)
		return;

	if (sign_flag) {
		SetEvent(event);
	} else {
		ResetEvent(event);
	}
}

bool CSignalEvent::get_sign()
{
	if (!event)
		return false;

	bool is_signed = (WAIT_OBJECT_0 == WaitForSingleObject(event, 0));
	return is_signed;
}

HANDLE CSignalEvent::get_event(const wchar_t *name, bool bNeedManualReset, bool *pNewCreate)
{
	HANDLE handle = CreateEventW(NULL, bNeedManualReset, false, name);
	if (handle) {
		if (pNewCreate)
			*pNewCreate = (GetLastError() != ERROR_ALREADY_EXISTS);
	} else {
		handle = OpenEventW(EVENT_ALL_ACCESS, false, name);
		if (pNewCreate)
			*pNewCreate = false;
	}

	assert(handle);
	return handle;
}

#endif // _WIN32
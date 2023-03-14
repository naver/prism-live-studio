#include "win-power-monitor.h"
#ifdef _WIN32
#include "util/bmem.h"
#include "util/threading.h"
#include <Windows.h>

#define DEVICE_NOTIFY_CALLBACK 2
typedef ULONG DEVICE_NOTIFY_CALLBACK_ROUTINE(_In_opt_ PVOID Context,
					     _In_ ULONG Type,
					     _In_ PVOID Setting);
typedef DEVICE_NOTIFY_CALLBACK_ROUTINE *PDEVICE_NOTIFY_CALLBACK_ROUTINE;
typedef struct _DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS {
	PDEVICE_NOTIFY_CALLBACK_ROUTINE Callback;
	PVOID Context;
} DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS, *PDEVICE_NOTIFY_SUBSCRIBE_PARAMETERS;
typedef PVOID HPOWERNOTIFY;
typedef HPOWERNOTIFY *PHPOWERNOTIFY;
typedef DWORD(_stdcall *REGISTER_POWER_NOTIFICATION)(_In_ DWORD, _In_ HANDLE,
						     _Out_ PHPOWERNOTIFY);
typedef DWORD(_stdcall *UNREGISTER_POWER_NOTIFICATION)(_Inout_ PHPOWERNOTIFY);

struct power_monitor {
	HPOWERNOTIFY registration_handle;
	HMODULE powrprof;
	REGISTER_POWER_NOTIFICATION register_power_notification;
	UNREGISTER_POWER_NOTIFICATION unregister_power_notification;
	volatile int power_state;
	pthread_mutex_t mutex;
};

static ULONG power_changed(PVOID context, ULONG type, PVOID setting)
{
	struct power_monitor *pm = (struct power_monitor *)context;
	if (pm) {
		pthread_mutex_lock(&pm->mutex);
		switch (type) {
		case PBT_APMRESUMEAUTOMATIC:
		case PBT_APMRESUMESUSPEND:
			plog(LOG_INFO, "WPM: PC wake up.");
			pm->power_state = WPS_POWER_RESUME_SUSPEND;
			break;
		case PBT_APMSUSPEND:
			plog(LOG_INFO, "WPM: PC sleep.");
			pm->power_state = WPS_POWER_SUSPEND;
			break;
		default:
			break;
		}
		pthread_mutex_unlock(&pm->mutex);
	}

	return 0;
}

power_monitor_t *power_monitor_start()
{
	struct power_monitor *pm =
		(struct power_monitor *)bmalloc(sizeof(struct power_monitor));
	if (!pm) {
		plog(LOG_WARNING, "WPM: Could not allocate power_monitor.");
		return NULL;
	}

	memset(pm, 0, sizeof(struct power_monitor));

	pthread_mutex_init_value(&pm->mutex);
	if (pthread_mutex_init(&pm->mutex, NULL) != 0) {
		plog(LOG_WARNING, "WPM: Failed to init mutex");
		power_monitor_stop(&pm);
		return NULL;
	}

	pm->powrprof = LoadLibraryW(L"powrprof.dll");
	if (pm->powrprof != NULL) {
		pm->register_power_notification =
			(REGISTER_POWER_NOTIFICATION)GetProcAddress(
				pm->powrprof,
				"PowerRegisterSuspendResumeNotification");
		pm->unregister_power_notification =
			(UNREGISTER_POWER_NOTIFICATION)GetProcAddress(
				pm->powrprof,
				"PowerUnregisterSuspendResumeNotification");

		if (!pm->register_power_notification ||
		    !pm->unregister_power_notification) {
			plog(LOG_WARNING,
			     "WPM: Could not get function for register(%p)/unregister(%p) power notification.",
			     pm->register_power_notification,
			     pm->unregister_power_notification);
			power_monitor_stop(&pm);
			return NULL;
		}

		DEVICE_NOTIFY_SUBSCRIBE_PARAMETERS
		cb = {power_changed, pm};

		pm->register_power_notification(DEVICE_NOTIFY_CALLBACK, &cb,
						&pm->registration_handle);

		if (!pm->registration_handle) {
			plog(LOG_WARNING,
			     "WPM: Fail to register power notification.");
			power_monitor_stop(&pm);
			return NULL;
		}
	}

	plog(LOG_INFO, "WPM: Windows PC power monitor is created.");
	return pm;
}

void power_monitor_stop(power_monitor_t **ppm)
{
	struct power_monitor *pm = *ppm;
	if (pm) {
		if (pm->powrprof) {
			if (pm->registration_handle &&
			    pm->unregister_power_notification)
				pm->unregister_power_notification(
					&pm->registration_handle);
			FreeLibrary(pm->powrprof);
		}

		pthread_mutex_destroy(&pm->mutex);
		bfree(pm);
		*ppm = NULL;

		plog(LOG_INFO, "WPM: Windows PC power monitor is destroyed.");
	}
}

bool power_monitor_state(power_monitor_t *pm, enum win_power_state state,
			 bool reset)
{
	bool result = false;
	if (pm) {
		pthread_mutex_lock(&pm->mutex);
		result = pm->power_state & state;
		if (reset)
			pm->power_state = WPS_POWER_NORMAL;
		pthread_mutex_unlock(&pm->mutex);
	}

	return result;
}
#endif

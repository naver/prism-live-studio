#include <obs-module.h>
#include <obs-frontend-api.h>
#include <frontend-api.h>

#include "SocketServer.h"

SocketServer *g_socketServer = nullptr;
bool g_unload = false;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("prism-remote-control-plugin", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Prism Remote Control plugin";
}

extern bool register_prism_remote_control();
extern void unregister_prism_remote_control();
static void OBSEvent(enum obs_frontend_event event, void *);
static void PrismEvent(pls_frontend_event event, const QVariantList &params, void *context);

bool obs_module_load(void)
{
	RC_LOG_INFO("module load");

	return register_prism_remote_control();
}

void obs_module_unload(void)
{
	RC_LOG_INFO("module unload");

	obs_frontend_remove_event_callback(OBSEvent, nullptr);
	pls_frontend_remove_event_callback(PrismEvent, nullptr);

	unregister_prism_remote_control();
}

const char *obs_module_name(void)
{
	return obs_module_description();
}

bool register_prism_remote_control()
{
	RC_LOG_INFO("register frontend callback");

	obs_frontend_add_event_callback(OBSEvent, nullptr);
	pls_frontend_add_event_callback(PrismEvent, nullptr);

	auto signal_handler = obs_get_signal_handler();
	signal_handler_add(signal_handler, "void remote_control_socket_initialized(int port)");
	return true;
}

void unregister_prism_remote_control()
{
	RC_LOG_INFO("unregister frontend callback1");

	if (g_unload)
		return;

	RC_LOG_INFO("unregister frontend callback2");

	g_unload = true;

	if (g_socketServer) {
		RC_LOG_INFO("unregister stop server socket");
		g_socketServer->Stop();
		pls_delete(g_socketServer, nullptr);
	}
}

static void handle_finish_loading()
{
	QTimer::singleShot(2000, []() {
		PLS_INFO("PrismRemoteControlPlugin", "single shot timer triggered for remote control.");

		pls_async_call_mt([]() {
			RC_LOG_INFO("start handle frontend event finish loading");

			if (g_unload)
				return;

			if (g_socketServer) {
				assert(false);
				return;
			}

			RC_LOG_INFO("start handle frontend event finish loading 2");

			g_socketServer = pls_new<SocketServer>();
			g_socketServer->Start();
		});
	});
}

static void OBSEvent(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		RC_LOG_INFO("receive frontend event finish loading");
		handle_finish_loading();
		break;
	default:
		break;
	}
}

static void PrismEvent(pls_frontend_event event, const QVariantList &params, void *context)
{
	switch (event) {
	case pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_SHUTTING_DOWN:
		RC_LOG_INFO("receive frontend event exit");
		unregister_prism_remote_control();
		break;

	default:
		break;
	}
}

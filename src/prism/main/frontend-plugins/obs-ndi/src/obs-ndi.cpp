/*
obs-ndi
Copyright (C) 2016-2018 Stéphane Lepin <steph  name of author

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <https://www.gnu.org/licenses/>
*/

#ifdef _WIN32
#include <Windows.h>
#endif

#include <sys/stat.h>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QLibrary>
#include <QMainWindow>
#include <QAction>
#include <QMessageBox>
#include <QString>
#include <QStringList>

#include "obs-ndi.h"
#include "main-output.h"
#include "preview-output.h"
#include "Config.h"
#include "forms/output-settings.h"
#include "alert-view.hpp"
#include "log/log.h"

#define NDI_SDK_DIR "NDI_SDK_DIR"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Stephane Lepin (Palakis)")
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ndi", "en-US")

const NDIlib_v4 *ndiLib = nullptr;
bool ndiLibInitialized = false;

extern struct obs_source_info create_ndi_source_info();
struct obs_source_info ndi_source_info;

extern struct obs_output_info create_ndi_output_info();
struct obs_output_info ndi_output_info;

extern struct obs_source_info create_ndi_filter_info();
struct obs_source_info ndi_filter_info;

extern struct obs_source_info create_ndi_audiofilter_info();
struct obs_source_info ndi_audiofilter_info;

extern struct obs_source_info create_alpha_filter_info();
struct obs_source_info alpha_filter_info;

const NDIlib_v4 *load_ndilib();

typedef const NDIlib_v4 *(*NDIlib_v4_load_)(void);
QLibrary *loaded_lib = nullptr;

NDIlib_find_instance_t ndi_finder;

OutputSettings *output_settings;

enum { NdiSuccess = 0, NoNdiRuntimeFound = 1, NDIInitializeFail = 2 };

extern "C" Q_DECL_EXPORT int loadNDIRuntime()
{
	if (ndiLib) {
		return ndiLibInitialized ? NdiSuccess : NDIInitializeFail;
	}

	ndiLib = load_ndilib();
	if (!ndiLib) {
		return NoNdiRuntimeFound;
	} else if (!ndiLib->initialize()) {
		return NDIInitializeFail;
	}

	ndiLibInitialized = true;

	NDIlib_find_create_t find_desc = {0};
	find_desc.show_local_sources = true;
	find_desc.p_groups = NULL;
	ndi_finder = ndiLib->find_create_v2(&find_desc);
	return NdiSuccess;
}

bool obs_module_load(void)
{
	PLS_INFO(FRONTEND_PLUGINS_NDI_SOURCE, "hello ! (version %s)", OBS_NDI_VERSION);

	ndiLib = load_ndilib();
	if (!ndiLib) {
		PLS_INFO(FRONTEND_PLUGINS_NDI_SOURCE, "NDI library not found.");
	} else if (ndiLib->initialize()) {
		ndiLibInitialized = true;

		PLS_INFO(FRONTEND_PLUGINS_NDI_SOURCE, "NDI library initialized successfully (%s)", ndiLib->version());

		NDIlib_find_create_t find_desc = {0};
		find_desc.show_local_sources = true;
		find_desc.p_groups = NULL;
		ndi_finder = ndiLib->find_create_v2(&find_desc);
	} else {
		PLS_WARN(FRONTEND_PLUGINS_NDI_SOURCE, "CPU unsupported by NDI library. Module won't load.");
	}

	ndi_source_info = create_ndi_source_info();
	obs_register_source(&ndi_source_info);

	ndi_output_info = create_ndi_output_info();
	obs_register_output(&ndi_output_info);

	//ndi_filter_info = create_ndi_filter_info();
	//obs_register_source(&ndi_filter_info);

	//ndi_audiofilter_info = create_ndi_audiofilter_info();
	//obs_register_source(&ndi_audiofilter_info);

	alpha_filter_info = create_alpha_filter_info();
	obs_register_source(&alpha_filter_info);

	//PRISM/Zhangdewen/20210305/#/remove ndi output
	/*
	if (main_window) {
		Config *conf = Config::Current();
		conf->Load();

		main_output_init(conf->OutputName.toUtf8().constData());
		preview_output_init(conf->PreviewOutputName.toUtf8().constData());

		// Ui setup
		//QAction* menu_action = (QAction*)obs_frontend_add_tools_menu_qaction(
		//	obs_module_text("NDIPlugin.Menu.OutputSettings"));

		//obs_frontend_push_ui_translation(obs_module_get_string);
		//output_settings = new OutputSettings(main_window);
		//obs_frontend_pop_ui_translation();

		//auto menu_cb = [] {
		//	output_settings->ToggleShowHide();
		//};
		//menu_action->connect(menu_action, &QAction::triggered, menu_cb);

		obs_frontend_add_event_callback(
			[](enum obs_frontend_event event, void *private_data) {
				Config *conf = (Config *)private_data;

				if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
					if (conf->OutputEnabled) {
						main_output_start(conf->OutputName.toUtf8().constData());
					}
					if (conf->PreviewOutputEnabled) {
						preview_output_start(conf->PreviewOutputName.toUtf8().constData());
					}
				} else if (event == OBS_FRONTEND_EVENT_EXIT) {
					preview_output_stop();
					main_output_stop();

					preview_output_deinit();
					main_output_deinit();
				}
			},
			(void *)conf);
	}*/

	return true;
}

void obs_module_unload()
{
	PLS_INFO(FRONTEND_PLUGINS_NDI_SOURCE, "goodbye !");

	if (ndiLib) {
		ndiLib->find_destroy(ndi_finder);
		ndiLib->destroy();
	}

	if (loaded_lib) {
		delete loaded_lib;
	}
}

const char *obs_module_name()
{
	return "obs-ndi";
}

const char *obs_module_description()
{
	return "NDI input/output integration for OBS Studio";
}

const NDIlib_v4 *load_ndilib()
{
	QStringList locations;

	if (QByteArray sdk = qgetenv(NDI_SDK_DIR); !sdk.isEmpty()) { // support load ndi runtime from NDI SDK
		locations << QString::fromUtf8(qgetenv(NDI_SDK_DIR)) + "/Bin/x64";
	}

	if (QByteArray rt = qgetenv(NDILIB_REDIST_FOLDER); !rt.isEmpty()) {
		locations << QString::fromUtf8(rt);
	}

	locations << "C:/Program Files/NewTek/NDI 4 Runtime/v4";
	locations << "C:/Program Files/NewTek/NDI 4 SDK/Bin/x64";

	for (QString path : locations) {
		QFileInfo libPath(QDir(path).absoluteFilePath(NDILIB_LIBRARY_NAME));

		if (libPath.exists() && libPath.isFile()) {
			QString libFilePath = libPath.absoluteFilePath();

			loaded_lib = new QLibrary(libFilePath, nullptr);
			if (loaded_lib->load()) {
				PLS_INFO(FRONTEND_PLUGINS_NDI_SOURCE, "NDI runtime loaded successfully");

				NDIlib_v4_load_ lib_load = (NDIlib_v4_load_)loaded_lib->resolve("NDIlib_v4_load");

				if (lib_load != nullptr) {
					return lib_load();
				} else {
					PLS_INFO(FRONTEND_PLUGINS_NDI_SOURCE, "ERROR: NDIlib_v4_load not found in loaded library");
				}
			} else {
				delete loaded_lib;
				loaded_lib = nullptr;
			}
		}
	}

	PLS_INFO(FRONTEND_PLUGINS_NDI_SOURCE, "Can't find the NDI library");
	return nullptr;
}

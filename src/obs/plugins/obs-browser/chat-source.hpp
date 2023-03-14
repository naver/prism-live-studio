/******************************************************************************
//PRISM/Zhangdewen/20200901/#for chat source
 ******************************************************************************/

#pragma once

#include <obs-module.h>
#include <obs.hpp>

#include "cef-headers.hpp"
#include "browser-config.h"
#include "browser-app.hpp"

#include <unordered_map>
#include <functional>
#include <vector>
#include <string>
#include <mutex>
#include "obs-browser-source.hpp"
#include <qstring.h>
#include <qjsonobject.h>

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
extern bool hwaccel;
#endif

struct ChatSource : public BrowserSource {
	int style = 1;
	int fontSize = 20;

	ChatSource(obs_data_t *settings, obs_source_t *source);

	void Update(obs_data_t *settings);
	void propertiesEditStart(obs_data_t *settings);
	void propertiesEditEnd(obs_data_t *settings);
	virtual void onBrowserLoadEnd() override;
	void dispatchJSEvent(const QByteArray &json);
	//PRISM/Zhangdewen/20211015/#/Chat Source Event
	void updateExternParams(const calldata_t *extern_params);
	QByteArray toJson(const char *cjson) const;
};

/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2018 by Hugh Bailey ("Jim") <jim@obsproject.com>

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

#include <obs-module.h>
#include <obs.hpp>

#include <unordered_map>
#include <functional>
#include <vector>
#include <string>
#include <mutex>
#include <qstring.h>
#include <qjsonobject.h>

#if EXPERIMENTAL_SHARED_TEXTURE_SUPPORT_ENABLED
extern bool hwaccel;
#endif

struct text_template_source : public QObject {
public:
	void update(obs_data_t *settings = nullptr, bool isForce = false);
	QString toJsonStr(bool isForce = false);
	void propertiesEditStart(obs_data_t *settings);
	void propertiesEditEnd(obs_data_t *settings);

	void sendNotifyAsync(int type, int sub_code);
	void updateExternParamsAsync(const calldata_t *extern_params);
	void sendNotify(int type, int sub_code);
	void updateExternParams(const QByteArray &cjson, int sub_code);
	void updateBoxSize(const QByteArray &cjson);
	void dispatchJSEvent(const QByteArray &json);
	void updateSize(obs_data_t *settings);
	void updateUrl();
	obs_source_t *m_source = nullptr;
	obs_source_t *m_browser = nullptr;
	gs_texture_t *m_source_texture = nullptr;
	QString type;
	long long isMotionOnce;
	double animateDuration;
	long long hAlign = -1;
	long long letter = -1;
	QString textColor;
	QString textFamily;
	QString textStyle;
	QString textFontSize;
	QString bkColor;
	QString textLineColor;
	QString textLineSize;
	QString content;
	QString subTitle;
	bool isVertical = false;
	long long baseWidth = -1;
	long long baseHeight = -1;
	QString cacheData;
	long long width = 720;
	long long height = 210;
	long long currentTemplateListIndex = -1;
	bool m_isDestory = false;
};

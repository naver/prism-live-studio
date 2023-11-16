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

#include "text-template-source.hpp"

#include <functional>
#include "obs-frontend-api.h"
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonarray.h>
#include <QTimer>
#include <QFile>
#include <qcolor.h>
#include <QDir>
#include <QUrl>
#include <qdebug.h>
#include <libutils-api.h>
#include "log.h"
#include <pls/pls-obs-api.h>
#include <pls/pls-properties.h>
#include <pls/pls-source.h>
#include <libui.h>
#include <QRandomGenerator>
#include <utils-api.h>

#ifdef USE_QT_LOOP
#include <QEventLoop>
#include <QThread>
#endif

using namespace std;

#define S_FONT "font"
#define S_TEXT "text"
#define S_COLOR "color"
#define S_BKCOLOR "bk_color"
#define S_FACE "face"
#define S_TM_TEXT "TextMotion.Text"
#define S_TM_TEXT_CONTENT "textmotion.text.content"
#define S_TM_TAB "textmotion.tab"
#define S_TM_TEMPLATE_TAB "textmotion.template.tab"
#define S_TM_TEMPLATE_LIST "textmotion.template.list"
#define S_TM_COLOR "textmotion.color"
#define S_TM_MOTION "textmotion.motion"
#define S_TM_COLOR_LIST_1 "textmotion.color_1"
#define S_TM_COLOR_LIST_2 "textmotion.color_2"
#define S_TM_DEFAULT_TEXT "text.template.default.text"
const auto VC_DEFAULT_WIDTH = 720;
const auto VC_DEFAULT_HEIGHT = 210;
static const char *const s_default_font = "";
#if defined(Q_OS_WIN)
const auto UPDATE_SIZE_INTERVAL = 210;
#elif defined(Q_OS_MACOS)
const auto UPDATE_SIZE_INTERVAL = 300;
#endif
struct TMTemplateAttr;
static QMap<long long, TMTemplateAttr> g_TMTemplateJsons;
static bool isFirstShow = true;

static const char *get_language(char *lang)
{
	const char *locale = obs_get_locale();
	if (!locale || !locale[0]) {
		return strcpy(lang, "en");
	}

	const char *pos = strchr(locale, '-');
	if (pos) {
		return strncpy(lang, locale, pos - locale);
	} else {
		return strcpy(lang, "en");
	}
}
static void process_url_language(QString &url)
{
	char lang[20] = {0};
	url += "?lang=";
	url += get_language(lang);
	url += QString("&timestamp=%1").arg(QRandomGenerator::global()->bounded(0, 10000));
}
static inline uint32_t rgb_to_bgr(uint32_t rgb)
{
	return ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb & 0xFF0000) >> 16);
}
static int getItemId(const QString &id)
{
	if (id.contains('_')) {
		return id.split('_').last().toInt();
	}
	return 0;
}
static QString getTextAlign(const long long &index)
{
	switch (index) {
	case 0:
		return "left";
	case 1:
		return "center";
	case 2:
		return "right";
	default:
		return "";
	}
}
static int getTextAlignIndex(const QString &alignStr)
{
	if (0 == alignStr.compare("left", Qt::CaseInsensitive)) {
		return 0;
	} else if (0 == alignStr.compare("center", Qt::CaseInsensitive)) {
		return 1;
	} else if (0 == alignStr.compare("right", Qt::CaseInsensitive)) {
		return 2;
	} else {
		return -1;
	}
}
static QString getLetter(const long long &index)
{
	switch (index) {
	case 0:
		return "uppercase";
	case 1:
		return "lowercase";
	default:
		return "none";
	}
}
static int getLetterIndex(const QString &letterStr)
{
	if (0 == letterStr.compare("up", Qt::CaseInsensitive)) {
		return 0;
	} else if (0 == letterStr.compare("down", Qt::CaseInsensitive)) {
		return 1;
	} else {
		return -1;
	}
}

static inline long long color_to_int(const QString &colorStr)
{
	QString colorStr_(colorStr.trimmed());
	if (colorStr_.isEmpty() || colorStr_ == "#000000") {
		return 0;
	}
	QColor color(colorStr_);
	auto shift = [&](unsigned val, int shift) { return ((val & 0xff) << shift); };

	return shift(color.red(), 0) | shift(color.green(), 8) | shift(color.blue(), 16) | shift(color.alpha(), 24);
}
static inline void colorIntList(QString &colorList, const QJsonValue &colorsData)
{
	if (!colorsData.isArray()) {
		return;
	}

	QStringList tmpList;
	auto colorStrList = colorsData.toArray();
	for (auto colorStr : colorStrList) {
		auto tmpStr = colorStr.toString().trimmed();
		if (!tmpStr.isEmpty()) {
			tmpList.append(QString::number(color_to_int(tmpStr)));
		}
	}
	colorList = tmpList.join(",");
}

using TMTemplateAttr = struct TMTemplateAttr {
	QString name;
	int textCount;
	QString firstText;
	QString secText;
	bool isFontSettings;
	bool isFont;
	bool isSize;
	QString fontFamily;
	QString fontWeight;
	int fontSizeValue;
	bool isBoxSize;
	bool isHorizontalAlignment;
	bool isLetter;
	int hAlignValue;
	int letterValue;

	bool isColorSettings;
	bool isTextColor;
	long long textColorValue;
	QString textcolorList;
	bool isTextTransparent;
	int textTransparentValue;
	bool isBackgroundOn;
	bool isBackgroundColor;
	long long backgroundColorValue;
	QString backgroundColorList;
	bool isBackgroundTransparent;
	int backgroundTransparentValue;
	bool isStrokeOn;
	bool isStrokeColor;
	long long strokeColorValue;
	int strokeLineValue;

	bool isMotionSettings;
	bool isRotate;
	bool isMotion;
	int motionValue;
};
static inline int defaultFirstId()
{
	static long long id = 0;
	if (id != 0) {
		return static_cast<int>(id);
	}
	if (!g_TMTemplateJsons.isEmpty()) {
		auto interAttr = find_if(g_TMTemplateJsons.constBegin(), g_TMTemplateJsons.constEnd(), [](const TMTemplateAttr &attr) { return attr.name == "AflowingTitle"; });
		if (interAttr != g_TMTemplateJsons.constEnd()) {
			id = interAttr.key();
		}
	}
	return static_cast<int>(id);
}

static void attachTMInfo()
{
	if (!g_TMTemplateJsons.isEmpty())
		return;
	auto jsonPath = pls_get_app_data_dir("PRISMLiveStudio") + "/textmotion/textmotion.json";
	QFile file(jsonPath);
	if (!file.open(QFile::ReadOnly | QIODevice::Text)) {
		return;
	}
	QByteArray array = file.readAll();
	file.close();
	QString lang = QString(obs_get_locale()).split('-')[0];
	bool isFirst = false;
	QJsonArray category = QJsonDocument::fromJson(array).object().value("group").toArray();
	for (auto cate : category) {
		QJsonArray itemObj = cate.toObject().value("items").toArray();
		for (auto item : itemObj) {
			QJsonObject itemObj = item.toObject();
			QJsonObject itemAttrObj = itemObj.value("properties").toObject();
			if (!isFirst) {
				auto adjustAttributes = itemAttrObj.value("mediaProperties").toObject().value("thumbnail").toObject();
				if (adjustAttributes.find(lang) == adjustAttributes.end()) {
					lang = "en";
				}
				isFirst = true;
			}

			QJsonObject contentObj = itemAttrObj.value("diaplayDefaultText").toObject();
			QJsonObject characterObj = itemAttrObj.value("characterProperties").toObject();

			TMTemplateAttr templateAttr;
			templateAttr.name = itemObj.value("title").toString();
			templateAttr.firstText = contentObj.value("defaultText1").toObject().value(lang).toString();
			templateAttr.secText = contentObj.value("defaultText2").toObject().value(lang).toString();
			templateAttr.textCount = templateAttr.secText.isEmpty() ? 1 : 2;
			templateAttr.isFontSettings = characterObj.value("isDisplay").toBool();
			templateAttr.isFont = characterObj.value("font").toObject().value("isDisplay").toBool();
#if defined(Q_OS_WIN)
			templateAttr.fontFamily = characterObj.value("font").toObject().value("family").toObject().value(lang).toString().simplified();
			templateAttr.fontWeight = characterObj.value("font").toObject().value("style").toObject().value(lang).toString().simplified();
#elif defined(Q_OS_MACOS)
			templateAttr.fontFamily = s_default_font;
			templateAttr.fontWeight = "Bold";
#endif

			templateAttr.isHorizontalAlignment = characterObj.value("horizontalAlignment").toObject().value("isDisplay").toBool();
			templateAttr.hAlignValue = getTextAlignIndex(characterObj.value("horizontalAlignment").toObject().value("alignmentStyle").toString());
			templateAttr.isSize = characterObj.value("fontSize").toObject().value("isDisplay").toBool();

			templateAttr.fontSizeValue = characterObj.value("fontSize").toObject().value("defaultSizePt").toInt();
			templateAttr.isBoxSize = characterObj.value("boxSize").toObject().value("isDisplay").toBool();
			templateAttr.letterValue = getLetterIndex(characterObj.value("upCase").toObject().value("style").toString());
			templateAttr.isLetter = characterObj.value("upCase").toObject().value("isDisplay").toBool();
			auto textColorObj = itemAttrObj.value("colorProperties").toObject().value("textColor").toObject();
			auto outLineColorObj = itemAttrObj.value("colorProperties").toObject().value("outLineColor").toObject();
			auto bgColorObj = itemAttrObj.value("colorProperties").toObject().value("BackgroudColor").toObject();

			templateAttr.isColorSettings = itemAttrObj.value("colorProperties").toObject().value("isDisplay").toBool();
			templateAttr.isTextColor = textColorObj.value("isDisplay").toBool();

			templateAttr.textColorValue = color_to_int(textColorObj.value("defaultColorRGBCode").toString());
			templateAttr.isTextTransparent = textColorObj.value("hasColorAplha").toBool();
			;
			templateAttr.textTransparentValue = textColorObj.value("colorAlpha").toInt(100);
			colorIntList(templateAttr.textcolorList, textColorObj.value("defaultColorRGBCodeList"));

			templateAttr.isBackgroundOn = bgColorObj.value("isCheck").toBool();
			templateAttr.isBackgroundColor = bgColorObj.value("isDisplay").toBool();
			templateAttr.backgroundColorValue = color_to_int(bgColorObj.value("defaultColorRGBCode").toString());
			templateAttr.isBackgroundTransparent = bgColorObj.value("hasColorAplha").toBool();

			templateAttr.backgroundTransparentValue = bgColorObj.value("colorAlpha").toInt(100);

			colorIntList(templateAttr.backgroundColorList, bgColorObj.value("defaultColorRGBCodeList"));

			templateAttr.isStrokeOn = outLineColorObj.value("isCheck").toBool();
			templateAttr.isStrokeColor = outLineColorObj.value("isDisplay").toBool();
			templateAttr.strokeColorValue = color_to_int(outLineColorObj.value("defaultColorRGBCode").toString());
			templateAttr.strokeLineValue = outLineColorObj.value("outLineSizePt").toInt(1);

			auto motionObj = itemAttrObj.value("motionProperties").toObject();

			templateAttr.isMotionSettings = itemAttrObj.value("motionProperties").toObject().value("isDisplay").toBool();
			templateAttr.isRotate = motionObj.value("isRepeat").toBool();
			templateAttr.isMotion = motionObj.value("isDisplay").toBool();

			templateAttr.motionValue = motionObj.value("defaultmotionSpeed").toInt(50);

			g_TMTemplateJsons.insert(getItemId(itemObj.value("itemId").toString()), templateAttr);
		}
	}
}
void set_tm_text_content_data(obs_data_t *settings, const TMTemplateAttr &tm_attr, bool is_default)
{
	obs_data_t *tm_text_content_obj = obs_data_create();
	if (is_default) {
		obs_data_set_default_bool(tm_text_content_obj, "text-content-change-1", false);
		obs_data_set_default_bool(tm_text_content_obj, "text-content-change-2", false);
		obs_data_set_default_int(tm_text_content_obj, "text-count", tm_attr.textCount);
		obs_data_set_default_string(tm_text_content_obj, "text-content-1", tm_attr.firstText.toUtf8());
		obs_data_set_default_string(tm_text_content_obj, "text-content-2", tm_attr.secText.toUtf8());
		obs_data_set_default_obj(settings, S_TM_TEXT_CONTENT, tm_text_content_obj);
	} else {
		obs_data_t *val = obs_data_get_obj(settings, S_TM_TEXT_CONTENT);
		QString firText = obs_data_get_string(val, "text-content-1");
		QString secText = obs_data_get_string(val, "text-content-2");
		bool isFirstTextChange = obs_data_get_bool(val, "text-content-change-1");
		bool isSecTextChange = obs_data_get_bool(val, "text-content-change-2");

		obs_data_set_int(tm_text_content_obj, "text-count", tm_attr.textCount);
		if (isFirstTextChange) {
			obs_data_set_string(tm_text_content_obj, "text-content-1", firText.toUtf8());
		} else {
			obs_data_set_string(tm_text_content_obj, "text-content-1", tm_attr.firstText.toUtf8());
		}

		if (isSecTextChange) {
			obs_data_set_string(tm_text_content_obj, "text-content-2", secText.toUtf8());
		} else {
			obs_data_set_string(tm_text_content_obj, "text-content-2", tm_attr.secText.toUtf8());
		}
		obs_data_set_bool(tm_text_content_obj, "text-content-change-1", isFirstTextChange);
		obs_data_set_bool(tm_text_content_obj, "text-content-change-2", isSecTextChange);
		obs_data_set_obj(settings, S_TM_TEXT_CONTENT, tm_text_content_obj);
		obs_data_release(val);
	}
	obs_data_release(tm_text_content_obj);
}
void set_tm_text_data(obs_data_t *settings, const TMTemplateAttr &tm_attr, bool is_default)
{
	obs_data_t *tm_text_obj = obs_data_create();
	if (is_default) {
		obs_data_set_default_bool(tm_text_obj, "is-font", tm_attr.isFont);
		obs_data_set_default_string(tm_text_obj, "font-family", tm_attr.fontFamily.toUtf8());
		obs_data_set_default_string(tm_text_obj, "font-weight", tm_attr.fontWeight.toUtf8());
		obs_data_set_default_bool(tm_text_obj, "is-h-aligin", tm_attr.isHorizontalAlignment);
		obs_data_set_default_int(tm_text_obj, "h-aligin", tm_attr.hAlignValue);
		obs_data_set_default_bool(tm_text_obj, "is-font-size", tm_attr.isSize);
		obs_data_set_default_int(tm_text_obj, "font-size", tm_attr.fontSizeValue);
		obs_data_set_default_bool(tm_text_obj, "is-box-size", tm_attr.isBoxSize);

		obs_data_set_default_int(tm_text_obj, "letter", tm_attr.letterValue);
		obs_data_set_default_bool(tm_text_obj, "is-letter", tm_attr.isLetter);
		obs_data_set_default_bool(tm_text_obj, "is-font-settings", tm_attr.isFontSettings);
		obs_video_info ovi;
		obs_get_video_info(&ovi);
		obs_data_set_default_int(settings, "width", ovi.base_width);
		obs_data_set_default_int(settings, "height", VC_DEFAULT_HEIGHT);
		obs_data_set_default_obj(settings, S_TM_TEXT, tm_text_obj);

	} else {

		obs_data_set_bool(tm_text_obj, "is-font", tm_attr.isFont);
		obs_data_set_string(tm_text_obj, "font-family", tm_attr.fontFamily.toUtf8());
		obs_data_set_string(tm_text_obj, "font-weight", tm_attr.fontWeight.toUtf8());
		obs_data_set_bool(tm_text_obj, "is-h-aligin", tm_attr.isHorizontalAlignment);
		obs_data_set_int(tm_text_obj, "h-aligin", tm_attr.hAlignValue);
		obs_data_set_bool(tm_text_obj, "is-font-size", tm_attr.isSize);
		obs_data_set_int(tm_text_obj, "font-size", tm_attr.fontSizeValue);
		obs_data_set_bool(tm_text_obj, "is-box-size", tm_attr.isBoxSize);
		obs_data_set_int(tm_text_obj, "letter", tm_attr.letterValue);
		obs_data_set_bool(tm_text_obj, "is-letter", tm_attr.isLetter);
		obs_data_set_bool(tm_text_obj, "is-font-settings", tm_attr.isFontSettings);

		obs_data_set_obj(settings, S_TM_TEXT, tm_text_obj);
	}
	obs_data_release(tm_text_obj);
}
void set_tm_text_color_data(obs_data_t *settings, const TMTemplateAttr &tm_attr, bool is_default)
{
	obs_data_t *tm_color = obs_data_create();

	if (is_default) {
		obs_data_set_default_bool(tm_color, "text-color-change", false);

		obs_data_set_default_int(tm_color, "color-tab", 0);

		obs_data_set_default_bool(tm_color, "is-color", tm_attr.isTextColor);
		obs_data_set_default_int(tm_color, "text-color", tm_attr.textColorValue);
		obs_data_set_default_bool(tm_color, "is-text-color-alpha", tm_attr.isTextTransparent);
		obs_data_set_default_int(tm_color, "text-color-alpha", tm_attr.textTransparentValue);
		obs_data_set_default_string(tm_color, "text-color-list", tm_attr.textcolorList.toUtf8().constData());
		obs_data_set_default_bool(tm_color, "is-bk-init-color-on", tm_attr.isBackgroundColor);
		obs_data_set_default_bool(tm_color, "is-bk-color-on", tm_attr.isBackgroundOn);
		obs_data_set_default_bool(tm_color, "is-bk-color", tm_attr.isBackgroundColor);
		obs_data_set_default_int(tm_color, "bk-color", tm_attr.backgroundColorValue);
		obs_data_set_default_bool(tm_color, "is-bk-color-alpha", tm_attr.isBackgroundTransparent);
		obs_data_set_default_int(tm_color, "bk-color-alpha", tm_attr.backgroundTransparentValue);
		obs_data_set_default_string(tm_color, "bk-color-list", tm_attr.backgroundColorList.toUtf8().constData());
		obs_data_set_default_bool(tm_color, "is-outline-color-on", tm_attr.isStrokeOn);
		obs_data_set_bool(tm_color, "is-outline-init-color-on", tm_attr.isStrokeOn);
		obs_data_set_default_bool(tm_color, "is-outline-color", tm_attr.isStrokeColor);
		obs_data_set_default_int(tm_color, "outline-color", tm_attr.strokeColorValue);
		obs_data_set_default_int(tm_color, "outline-color-line", tm_attr.strokeLineValue);

		obs_data_set_default_bool(tm_color, "is-color-settings", tm_attr.isColorSettings);

		obs_data_set_default_obj(settings, S_TM_COLOR, tm_color);

	} else {
		obs_data_t *val = obs_data_get_obj(settings, S_TM_COLOR);
		long long textColor = obs_data_get_int(val, "text-color");
		auto textColorAlaph = obs_data_get_int(val, "text-color-alpha");
		bool textColorChange = obs_data_get_bool(val, "text-color-change");

		if (textColorChange) {
			obs_data_set_int(tm_color, "text-color", textColor);
			obs_data_set_int(tm_color, "text-color-alpha", textColorAlaph);

		} else {
			obs_data_set_int(tm_color, "text-color", tm_attr.textColorValue);
			obs_data_set_int(tm_color, "text-color-alpha", tm_attr.textTransparentValue);
		}

		obs_data_set_bool(tm_color, "text-color-change", textColorChange);
		obs_data_set_bool(tm_color, "is-color", tm_attr.isTextColor);
		obs_data_set_bool(tm_color, "is-text-color-alpha", tm_attr.isTextTransparent);
		obs_data_set_string(tm_color, "text-color-list", tm_attr.textcolorList.toUtf8().constData());
		obs_data_set_bool(tm_color, "is-bk-color-on", tm_attr.isBackgroundOn);
		obs_data_set_bool(tm_color, "is-bk-init-color-on", tm_attr.isBackgroundOn);
		obs_data_set_bool(tm_color, "is-bk-color", tm_attr.isBackgroundColor);
		obs_data_set_int(tm_color, "bk-color", tm_attr.backgroundColorValue);
		obs_data_set_bool(tm_color, "is-bk-color-alpha", tm_attr.isBackgroundTransparent);
		obs_data_set_int(tm_color, "bk-color-alpha", tm_attr.backgroundTransparentValue);
		obs_data_set_string(tm_color, "bk-color-list", tm_attr.backgroundColorList.toUtf8().constData());
		obs_data_set_bool(tm_color, "is-outline-color-on", tm_attr.isStrokeOn);
		obs_data_set_bool(tm_color, "is-outline-init-color-on", tm_attr.isStrokeOn);
		obs_data_set_bool(tm_color, "is-outline-color", tm_attr.isStrokeColor);
		obs_data_set_int(tm_color, "outline-color", tm_attr.strokeColorValue);
		obs_data_set_int(tm_color, "outline-color-line", tm_attr.strokeLineValue);

		obs_data_set_bool(tm_color, "is-color-settings", tm_attr.isColorSettings);
		obs_data_set_obj(settings, S_TM_COLOR, tm_color);
		obs_data_release(val);
	}
	obs_data_release(tm_color);
}
void set_tm_text_template_data(obs_data_t *settings, const TMTemplateAttr &tm_attr, bool is_default)
{
	obs_data_t *tm_motion = obs_data_create();
	if (is_default) {
		obs_data_set_default_int(tm_motion, "text-motion", tm_attr.isRotate);
		obs_data_set_default_bool(tm_motion, "is-text-motion-speed", tm_attr.isMotion);

		obs_data_set_default_int(tm_motion, "text-motion-speed", tm_attr.motionValue);
		obs_data_set_default_bool(tm_motion, "is-motion-settings", tm_attr.isMotionSettings);
		obs_data_set_default_obj(settings, S_TM_MOTION, tm_motion);
	} else {
		obs_data_set_int(tm_motion, "text-motion", tm_attr.isRotate);
		obs_data_set_bool(tm_motion, "is-text-motion-speed", tm_attr.isMotion);

		obs_data_set_int(tm_motion, "text-motion-speed", tm_attr.motionValue);

		obs_data_set_bool(tm_motion, "is-motion-settings", tm_attr.isMotionSettings);
		obs_data_set_obj(settings, S_TM_MOTION, tm_motion);
	}
	obs_data_release(tm_motion);
}

static bool tm_tab_changed(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	auto index = obs_data_get_int(settings, S_TM_TAB);
	obs_property_t *tm_template_tab = obs_properties_get(props, S_TM_TEMPLATE_TAB);
	obs_property_t *tm_template_list = obs_properties_get(props, S_TM_TEMPLATE_LIST);
	obs_property_t *tm_text_content = obs_properties_get(props, S_TM_TEXT_CONTENT);
	obs_property_t *tm_text = obs_properties_get(props, S_TM_TEXT);
	obs_property_t *tm_color = obs_properties_get(props, S_TM_COLOR);
	obs_property_t *tm_motion = obs_properties_get(props, S_TM_MOTION);
	obs_property_t *tm_default_text = obs_properties_get(props, S_TM_DEFAULT_TEXT);

	obs_property_set_visible(tm_template_tab, false);
	obs_property_set_visible(tm_template_list, false);
	obs_property_set_visible(tm_text, false);
	obs_property_set_visible(tm_color, false);
	obs_property_set_visible(tm_motion, false);
	obs_property_set_visible(tm_default_text, false);

	switch (index) {
	case 0:
		obs_property_set_visible(tm_template_tab, true);
		obs_property_set_visible(tm_template_list, true);
		break;
	case 1:
		obs_property_set_visible(tm_text, true);
		obs_property_set_visible(tm_color, true);
		obs_property_set_visible(tm_motion, true);
		obs_property_set_visible(tm_default_text, 0 == strcmp(obs_data_get_string(settings, "templateName"), "notice1"));

		break;
	default:
		break;
	}
	return true;
}

static bool tm_template_list_changed(void *data, obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	attachTMInfo();
	static int currentIndex = -1;
	auto index = obs_data_get_int(settings, S_TM_TEMPLATE_LIST);
	auto context = (text_template_source *)(data);

	if (!context)
		return false;
	auto browser_settings = obs_source_get_settings(context->m_browser);

	if ((context->currentTemplateListIndex == index) || isFirstShow) {
		context->currentTemplateListIndex = index;
		isFirstShow = false;
	} else {
		if (g_TMTemplateJsons.find(index) != g_TMTemplateJsons.end()) {
			context->currentTemplateListIndex = index;
			TMTemplateAttr templateAttr = g_TMTemplateJsons.value(index);
			obs_data_set_string(settings, "templateName", templateAttr.name.toUtf8().constData());
			set_tm_text_content_data(settings, templateAttr, false);
			set_tm_text_data(settings, templateAttr, false);
			set_tm_text_color_data(settings, templateAttr, false);
			set_tm_text_template_data(settings, templateAttr, false);

			QString url = QUrl::fromLocalFile(pls_get_app_data_dir("PRISMLiveStudio") + "/textmotion/web/index.html").toString();
			process_url_language(url);
			obs_data_set_string(browser_settings, "url", url.toUtf8().constData());

			context->updateSize(browser_settings);
			obs_source_update(context->m_browser, browser_settings);
		}
	}
	obs_data_release(browser_settings);

	return true;
}

obs_properties_t *text_template_source_get_properties(void *data)
{
	isFirstShow = true;
	obs_properties_t *properties = obs_properties_create();
	pls_properties_tm_add_content(properties, S_TM_TEXT_CONTENT, obs_module_text(S_TM_TEXT_CONTENT));
	obs_property_t *tm_tab_prop = pls_properties_tm_add_tab(properties, S_TM_TAB);
	obs_property_set_modified_callback(tm_tab_prop, tm_tab_changed);

	pls_properties_tm_add_template_tab(properties, S_TM_TEMPLATE_TAB);

	obs_property_t *tm_template_list_prop = pls_properties_tm_add_template_list(properties, S_TM_TEMPLATE_LIST);
	obs_property_set_modified_callback2(tm_template_list_prop, tm_template_list_changed, data);
	pls_properties_tm_add_text(properties, S_TM_TEXT, obs_module_text(S_TM_TEXT), 6, 72, 1);
	pls_properties_tm_add_color(properties, S_TM_COLOR, obs_module_text(S_TM_COLOR), 0, 100, 1);

	pls_properties_tm_add_motion(properties, S_TM_MOTION, obs_module_text(S_TM_MOTION), 1, 100, 1);

	pls_properties_tm_add_template_default_text(properties, S_TM_DEFAULT_TEXT, obs_module_text(S_TM_DEFAULT_TEXT));
	return properties;
}

void text_template_source_get_defaults(obs_data_t *settings)
{
	attachTMInfo();

	if (g_TMTemplateJsons.count() <= 0)
		return;

	obs_data_set_default_int(settings, S_TM_TAB, 0);
	obs_data_set_default_int(settings, S_TM_TEMPLATE_TAB, 0);
	obs_data_set_default_int(settings, S_TM_TEMPLATE_LIST, defaultFirstId());

	obs_data_set_default_string(settings, "templateName", g_TMTemplateJsons.value(defaultFirstId()).name.toUtf8().constData());
	set_tm_text_content_data(settings, g_TMTemplateJsons.value(defaultFirstId()), true);

	set_tm_text_data(settings, g_TMTemplateJsons.value(defaultFirstId()), true);

	set_tm_text_color_data(settings, g_TMTemplateJsons.value(defaultFirstId()), true);

	set_tm_text_template_data(settings, g_TMTemplateJsons.value(defaultFirstId()), true);

	obs_data_set_default_bool(settings, "reroute_audio", false);
}
static void source_notified(void *data, calldata_t *calldata)
{
	pls_used(calldata);
	auto context = static_cast<struct text_template_source *>(data);
	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (!source || (source != context->m_browser) || !data)
		return;

	auto type = (int)calldata_int(calldata, "message");
	auto settings = obs_source_get_settings(context->m_source);
	switch (type) {
	case OBS_SOURCE_BROWSER_LOADED:
		pls_async_call_mt(context, [context, settings]() { context->update(settings, true); });
		break;
	default:
		break;
	}
	obs_data_release(settings);
}
static void init_browser_source(struct text_template_source *context)
{
	if (!context || context->m_browser) {
		return;
	}

	// init browser source

	QString url = QUrl::fromLocalFile(pls_get_app_data_dir("PRISMLiveStudio") + "/textmotion/web/index.html").toString();

	process_url_language(url);

	obs_video_info ovi;
	obs_get_video_info(&ovi);

	obs_data_t *browser_settings = obs_data_create();
	obs_data_set_string(browser_settings, "url", url.toUtf8().constData());
	obs_data_set_bool(browser_settings, "is_local_file", false);
	obs_data_set_bool(browser_settings, "reroute_audio", true);
	obs_data_set_bool(browser_settings, "ignore_reload", true);
	obs_data_set_int(browser_settings, "width", context->width);
	obs_data_set_int(browser_settings, "height", context->height);
	context->m_browser = obs_source_create_private("browser_source", "prism_text_template_browser_source", browser_settings);
	obs_data_release(browser_settings);

	signal_handler_connect_ref(obs_get_signal_handler(), "source_notify", source_notified, context);

	obs_source_inc_active(context->m_browser);
	obs_source_inc_showing(context->m_browser);
}
static void source_created(void *data, calldata_t *calldata)
{
	pls_used(calldata);
	auto context = static_cast<struct text_template_source *>(data);
	auto source = (uint64_t)calldata_int(calldata, "source_address");
	if (!source || (source != (uint64_t)context->m_source) || !data)
		return;

	init_browser_source(context);
}
static void *text_template_source_create(obs_data_t *settings, obs_source_t *source)
{
	//obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);

	auto context = pls_new_nothrow<text_template_source>();
	if (!context) {
		PLS_PLUGIN_ERROR("viewer count source create failed, because out of memory.");
		//obs_source_set_capture_valid(source, false, OBS_SOURCE_ERROR_UNKNOWN);
		return nullptr;
	}

	signal_handler_connect_ref(obs_get_signal_handler(), "source_create_finished", source_created, context);
	signal_handler_add(obs_source_get_signal_handler(source), "void source_notify(ptr source, int message, int sub_code)");
	obs_video_info ovi;
	obs_get_video_info(&ovi);
	context->baseHeight = ovi.base_height;
	context->baseWidth = ovi.base_width;
	context->m_source = source;
	context->width = obs_data_get_int(settings, "width");
	context->height = obs_data_get_int(settings, "height");
	context->update(settings);
	return context;
}
static void text_template_source_destroy(void *data)
{
	PLS_PLUGIN_INFO("start destory text template source");
	auto context = (text_template_source *)(data);
	context->m_isDestory = true;
	signal_handler_disconnect(obs_get_signal_handler(), "source_create_finished", source_created, context);

	if (context->m_browser) {
		signal_handler_disconnect(obs_get_signal_handler(), "source_notify", source_notified, context);
		obs_source_dec_active(context->m_browser);
		obs_source_release(context->m_browser);
	}

	if (context->m_source_texture) {
		obs_enter_graphics();
		gs_texture_destroy(context->m_source_texture);
		context->m_source_texture = nullptr;
		obs_leave_graphics();
	}
	pls_delete_later(context);
}
static void text_template_source_activate(void *data)
{
	if (auto context = (text_template_source *)(data); context->m_browser) {
		obs_source_inc_active(context->m_browser);
	}
}
static void text_template_source_deactivate(void *data)
{
	if (auto context = (text_template_source *)(data); context->m_browser) {
		obs_source_dec_active(context->m_browser);
	}
}

static void text_template_source_clear_texture(gs_texture_t *tex)
{
	if (!tex) {
		return;
	}
	obs_enter_graphics();
	gs_texture_t *pre_rt = gs_get_render_target();
	gs_projection_push();
	gs_set_render_target(tex, nullptr);
	struct vec4 clear_color = {0};
	vec4_zero(&clear_color);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);
	gs_set_render_target(pre_rt, nullptr);
	gs_projection_pop();
	obs_leave_graphics();
}
static void text_template_source_video_render(void *data, gs_effect_t *effect)
{
	auto context = (text_template_source *)(data);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), context->m_source_texture);
	gs_draw_sprite(context->m_source_texture, 0, 0, 0);
	pls_used(effect);
}
static void text_template_source_render(void *data, obs_source_t *source)
{
	auto vc_source = (text_template_source *)(data);
	uint32_t source_width = obs_source_get_width(source);
	uint32_t source_height = obs_source_get_height(source);
	if (source_width <= 0 || source_height <= 0) {
		text_template_source_clear_texture(vc_source->m_source_texture);
		return;
	}
	obs_enter_graphics();
	if (vc_source->m_source_texture) {
		uint32_t tex_width = gs_texture_get_width(vc_source->m_source_texture);
		uint32_t tex_height = gs_texture_get_height(vc_source->m_source_texture);
		if (tex_width != source_width || tex_height != source_height) {
			gs_texture_destroy(vc_source->m_source_texture);
			vc_source->m_source_texture = nullptr;
		}
	}

	if (!vc_source->m_source_texture) {
		vc_source->m_source_texture = gs_texture_create(source_width, source_height, GS_RGBA, 1, nullptr, GS_RENDER_TARGET);
	}

	gs_texture_t *pre_rt = gs_get_render_target();
	gs_viewport_push();
	gs_projection_push();
	struct vec4 clear_color;
	vec4_set(&clear_color, 0.0f, 0.0f, 0.0f, 0.0f);

	gs_set_render_target(vc_source->m_source_texture, nullptr);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 1.0f, 0);

	gs_enable_depth_test(false);
	gs_set_cull_mode(GS_NEITHER);
	gs_set_viewport(0, 0, source_width, source_height);

	gs_ortho(0.0f, float(source_width), 0.0f, float(source_height), -100.0f, 100.0f);
	obs_source_video_render(source);

	gs_set_render_target(pre_rt, nullptr);
	gs_viewport_pop();
	gs_projection_pop();

	obs_leave_graphics();
}

static void text_template_source_video_tick(void *data, float /*seconds*/)
{
	if (auto context = (text_template_source *)(data); context->m_browser) {
		text_template_source_render(data, context->m_browser);
	}
}
static void text_template_cef_dispatch_js(void *data, const char *event_name, const char *json_data)
{
	if (auto context = (text_template_source *)(data); context->m_browser) {
		pls_source_dispatch_cef_js(context->m_browser, event_name, json_data);
	}
}
static uint32_t text_template_width(void *data)
{
	auto context = (text_template_source *)(data);
	//auto settings = obs_source_get_settings(context->m_source);
	//auto updateWidth = obs_data_get_int(settings, "width");
	//obs_data_release(settings);
	return static_cast<uint32_t>(context->width);
}
static uint32_t text_template_height(void *data)
{
	auto context = (text_template_source *)(data);
	//auto settings = obs_source_get_settings(context->m_source);
	//auto updateHeight = obs_data_get_int(settings, "height");
	//obs_data_release(settings);
	return static_cast<uint32_t>(context->height);
}
static void text_template_set_private_data(void *data, obs_data_t *private_data)
{
	if (!data || !private_data) {
		return;
	}
	auto source = static_cast<text_template_source *>(data);
	string method = obs_data_get_string(private_data, "method");
	if (method == "default_properties") {
		source->updateUrl();
	}
	return;
}
void register_prism_text_template_source()
{
	struct obs_source_info info = {0};
	info.id = "prism_text_motion_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_properties = text_template_source_get_properties;
	info.get_defaults = text_template_source_get_defaults;
	info.activate = text_template_source_activate;
	info.deactivate = text_template_source_deactivate;

	info.get_name = [](void *) { return obs_module_text(obs_module_text("text.template")); };
	info.create = text_template_source_create;
	info.destroy = text_template_source_destroy;
	info.update = [](void *data, obs_data_t *settings) { static_cast<text_template_source *>(data)->update(settings); };
	info.get_width = text_template_width;
	info.get_height = text_template_height;
	info.video_tick = text_template_source_video_tick;
	info.video_render = text_template_source_video_render;

	pls_source_info psi = {0};
	psi.properties_edit_end = [](void *data, obs_data_t *settings, bool) { static_cast<text_template_source *>(data)->propertiesEditEnd(settings); };

	psi.properties_edit_start = [](void *data, obs_data_t *settings) { static_cast<text_template_source *>(data)->propertiesEditStart(settings); };
	psi.update_extern_params = [](void *data, const calldata_t *extern_params) { static_cast<text_template_source *>(data)->updateExternParamsAsync(extern_params); };
	psi.cef_dispatch_js = text_template_cef_dispatch_js;

	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_TEXT_TEMPLATE);
	psi.set_private_data = text_template_set_private_data;
	register_pls_source_info(&info, &psi);
	obs_register_source(&info);
}

void release_prism_text_template_source() {}

void text_template_source::update(obs_data_t *settings, bool isForce)
{
	pls_check_app_exiting();
	if (!m_browser || !pls_object_is_valid(this) || m_isDestory) {
		return;
	}
	attachTMInfo();
	isVertical = obs_data_get_bool(settings, "isVertical");

	auto index = obs_data_get_int(settings, S_TM_TEMPLATE_LIST);

	width = obs_data_get_int(settings, "width");
	height = obs_data_get_int(settings, "height");
	auto browser_settings = obs_source_get_settings(m_browser);
	obs_data_set_int(browser_settings, "width", width);
	obs_data_set_int(browser_settings, "height", height);
	obs_source_update(m_browser, browser_settings);

	obs_data_release(browser_settings);
	if (g_TMTemplateJsons.find(index) != g_TMTemplateJsons.end()) {
		TMTemplateAttr templateAttr = g_TMTemplateJsons.value(index);
		type = templateAttr.name;
	}

	obs_data_t *tmTextVal = obs_data_get_obj(settings, S_TM_TEXT);
	obs_data_t *tmContentVal = obs_data_get_obj(settings, S_TM_TEXT_CONTENT);
	obs_data_t *tmColorVal = obs_data_get_obj(settings, S_TM_COLOR);
	obs_data_t *tmMotionVal = obs_data_get_obj(settings, S_TM_MOTION);

	bool isBackground = obs_data_get_bool(tmColorVal, "is-bk-color-on");
	bool isOutLine = obs_data_get_bool(tmColorVal, "is-outline-color-on");
	auto textColorInt = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(tmColorVal, "text-color")));
	auto textColorAlaph = obs_data_get_int(tmColorVal, "text-color-alpha") * 0.01 * 255;

	textColor = QString("%1%2").arg(QColor(textColorInt).name(QColor::HexRgb)).arg(QString::asprintf("%02x", static_cast<uint32_t>(textColorAlaph)));

	textFamily = QString("%1").arg(obs_data_get_string(tmTextVal, "font-family"));
	textStyle = obs_data_get_string(tmTextVal, "font-weight");
	textFontSize = QString("%1pt").arg(obs_data_get_int(tmTextVal, "font-size"));
	if (isBackground) {
		auto bkColorInt = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(tmColorVal, "bk-color")));
		auto bkColorAlaph = obs_data_get_int(tmColorVal, "bk-color-alpha") * 0.01 * 255;
		bkColor = QString("%1%2").arg(QColor(bkColorInt).name(QColor::HexRgb)).arg(QString::asprintf("%02x", static_cast<uint32_t>(bkColorAlaph)));

	} else {
		bkColor.clear();
	}
	if (isOutLine) {
		auto outlineColorInt = rgb_to_bgr(static_cast<uint32_t>(obs_data_get_int(tmColorVal, "outline-color")));
		textLineColor = QString("%1").arg(QColor(outlineColorInt).name(QColor::HexRgb));
		textLineSize = QString("%1").arg(obs_data_get_int(tmColorVal, "outline-color-line"));
	} else {
		textLineColor.clear();
		textLineSize.clear();
	}

	content = obs_data_get_string(tmContentVal, "text-content-1");
	subTitle = obs_data_get_string(tmContentVal, "text-content-2");
	hAlign = obs_data_get_int(tmTextVal, "h-aligin");
	letter = obs_data_get_int(tmTextVal, "letter");
	isMotionOnce = qAbs(obs_data_get_int(tmMotionVal, "text-motion") - 1);
	animateDuration = obs_data_get_int(tmMotionVal, "text-motion-speed");
	if (animateDuration <= 50) {
		animateDuration = animateDuration * 0.02;
	} else {
		animateDuration = animateDuration * 0.04;
	}
	obs_data_release(tmTextVal);
	obs_data_release(tmColorVal);
	obs_data_release(tmContentVal);
	obs_data_release(tmMotionVal);
	QByteArray updateTextTemplateInfo(toJsonStr(isForce).toUtf8());
	if (!updateTextTemplateInfo.isEmpty()) {
		dispatchJSEvent(updateTextTemplateInfo.constData());
	}
}

QString text_template_source::toJsonStr(bool isForce)
{
	QJsonObject root;
	QJsonObject data;
	data.insert("isVertical", isVertical);
	data.insert("type", type);
	data.insert("isOnce", isMotionOnce);
	data.insert("animateRate", animateDuration);
	data.insert("align", getTextAlign(hAlign));
	data.insert("textTransform", getLetter(letter));
	data.insert("fontColor", textColor);
	data.insert("background", bkColor);
	data.insert("fontFamily", textFamily);
	data.insert("fontStyle", textStyle);
	data.insert("fontSize", textFontSize);
	data.insert("textLineColor", textLineColor);
	data.insert("textLineSize", textLineSize);
	data.insert("content", content.replace('\n', "\\n"));
	data.insert("subContent", subTitle.replace('\n', "\\n"));
	QString json = QJsonDocument(data).toJson(QJsonDocument::Compact);
	if (!isForce && cacheData == json) {
		return QString();
	}
	cacheData = json;

	return json;
}
void text_template_source::dispatchJSEvent(const QByteArray &json)
{
	PLS_PLUGIN_INFO("update text template info:%s", json.constData());

	if (m_browser) {

		pls_source_dispatch_cef_js(m_browser, "textMotion", json.constData());
	}
}
void text_template_source::updateSize(obs_data_t *browser_settings)
{
	auto settings = obs_source_get_settings(m_source);
	auto index = obs_data_get_int(settings, S_TM_TEMPLATE_LIST);

	obs_video_info ovi;
	obs_get_video_info(&ovi);
	auto groupIndex = obs_data_get_int(settings, S_TM_TEMPLATE_TAB);

	if (groupIndex == 1) {
		if (ovi.base_height > ovi.base_width) {
			width = VC_DEFAULT_WIDTH;
			height = 1280;
		} else {
			width = 1920;
			height = 1080;
		}
		obs_data_set_bool(settings, "isVertical", ovi.base_height > ovi.base_width);
	} else if (index == defaultFirstId()) {
		width = ovi.base_width;
		height = VC_DEFAULT_HEIGHT;
	} else {
		width = VC_DEFAULT_WIDTH;
		height = VC_DEFAULT_HEIGHT;
	}
	obs_data_set_int(settings, "width", width);
	obs_data_set_int(settings, "height", height);
	obs_data_set_int(browser_settings, "width", width);
	obs_data_set_int(browser_settings, "height", height);
	obs_data_release(settings);
}
void text_template_source::updateUrl()
{
	auto browser_settings = obs_source_get_settings(m_browser);
	QString url = QUrl::fromLocalFile(pls_get_app_data_dir("PRISMLiveStudio") + "/textmotion/web/index.html").toString();
	process_url_language(url);
	obs_data_set_string(browser_settings, "url", url.toUtf8().constData());
	obs_data_release(browser_settings);
}
void text_template_source::propertiesEditStart(obs_data_t *settings)
{
	sendNotifyAsync(OBS_SOURCE_TEXT_TEMPLATE_UPDATE_PARAMS, OBS_SOURCE_TEXT_TEMPLATE_UPDATE_PARAMS_SUB_CODE_EDIT_START);
}
void text_template_source::propertiesEditEnd(obs_data_t *settings) {}

//PRISM/Chengbing/20230529/#943/Async notify
void text_template_source::sendNotifyAsync(int type, int subCode)
{
	pls_async_call_mt(this, [this, type, subCode]() { sendNotify(type, subCode); });
}

//PRISM/Chengbing/20230529/#943/Async notify
void text_template_source::updateExternParamsAsync(const calldata_t *extern_params)
{
	const char *cjson = calldata_string(extern_params, "cjson");
	auto cJsonData = QByteArray(cjson);
	int sub_code = calldata_int(extern_params, "sub_code");
	pls_async_call_mt(this, [this, cJsonData, sub_code]() { updateExternParams(cJsonData, sub_code); });
}

void text_template_source::sendNotify(int type, int sub_code)
{

	pls_source_update_extern_params_json(m_source, toJsonStr(true).toStdString().c_str(), sub_code);
}

void text_template_source::updateExternParams(const QByteArray &cjson, int sub_code)
{
	switch (sub_code) {
	case OBS_SOURCE_TEXT_TEMPLATE_UPDATE_PARAMS_SUB_CODE_UPDATE:
	case OBS_SOURCE_TEXT_TEMPLATE_UPDATE_PARAMS_SUB_CODE_EDIT_START:
	case OBS_SOURCE_TEXT_TEMPLATE_UPDATE_PARAMS_SUB_CODE_LOADED:
		dispatchJSEvent(cjson);
		break;
	case OBS_SOURCE_TEXT_TEMPLATE_UPDATE_PARAMS_SUB_CODE_SIZECHANGED:
		updateBoxSize(cjson);
		break;
	default:
		break;
	}
}

void text_template_source::updateBoxSize(const QByteArray &cjson)
{
	QJsonObject obj = QJsonDocument::fromJson(QByteArray(cjson)).object();
	int view_width = obj.value("width").toInt();
	int view_height = obj.value("height").toInt();
	if (baseHeight != view_height || baseWidth != view_width) {
		baseHeight = view_height;
		baseWidth = view_width;

		auto browser_settings = obs_source_get_settings(m_browser);
		auto settings = obs_source_get_settings(m_source);

		if (obs_data_get_int(settings, S_TM_TEMPLATE_TAB) == 1) {
			isVertical = view_height > view_width;
			if (isVertical) {
				obs_data_set_int(browser_settings, "width", VC_DEFAULT_WIDTH);
				obs_data_set_int(browser_settings, "height", 1280);
				obs_data_set_int(settings, "width", VC_DEFAULT_WIDTH);
				obs_data_set_int(settings, "height", 1280);
				width = VC_DEFAULT_WIDTH;
				height = 1280;
			} else {
				obs_data_set_int(browser_settings, "width", 1920);
				obs_data_set_int(browser_settings, "height", 1080);
				obs_data_set_int(settings, "width", 1920);
				obs_data_set_int(settings, "height", 1080);
				width = 1920;
				height = 1080;
			}
			obs_data_set_bool(settings, "isVertical", isVertical);
			obs_source_update(m_source, settings);
			obs_source_update(m_browser, browser_settings);
		}
		obs_data_release(browser_settings);
		obs_data_release(settings);
	}
}

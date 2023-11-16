#include "prism_viewer_count_source.h"

#include <obs.h>
#include <stdio.h>
#include <util/dstr.h>
#include <graphics/image-file.h>
#include <util/platform.h>
#include <sys/stat.h>
#include <filesystem>
#include <string>
#include <codecvt>
#include <util/threading.h>
#include <log.h>
#include <libutils-api.h>
#include <frontend-api.h>
#include <qcolor.h>
#include <qdir.h>
#include <optional>
#include <qurlquery.h>
#include <pls/pls-obs-api.h>
#include <pls/pls-properties.h>
#include <pls/pls-source.h>
#include <libui.h>

#define debug(format, ...) PLS_PLUGIN_DEBUG(format, ##__VA_ARGS__)
#define info(format, ...) PLS_PLUGIN_INFO(format, ##__VA_ARGS__)
#define warn(format, ...) PLS_PLUGIN_WARN(format, ##__VA_ARGS__)
#define error(format, ...) PLS_PLUGIN_ERROR(format, ##__VA_ARGS__)

const auto VC_DEFAULT_WIDTH = 720;
const auto VC_DEFAULT_HEIGHT = 210;
const auto VC_ONE_WIDTH_T1 = 130;
const auto VC_ONE_HEIGHT_T1 = 36;
const auto VC_ONE_WIDTH_T2 = 130;
const auto VC_ONE_HEIGHT_T2 = 38;
const auto VC_ONE_WIDTH_T3 = 106;
const auto VC_ONE_HEIGHT_T3 = 61;
const auto VC_ONE_WIDTH_T4 = 140;
const auto VC_ONE_HEIGHT_T4 = 56;
const auto VC_ONE_WIDTH_T5 = 100;
const auto VC_ONE_HEIGHT_T5 = 100;
const auto VC_ONE_WIDTH_T6 = 89;
const auto VC_ONE_HEIGHT_T6 = 31;
const auto VC_MOTION_WIDTH = 168;
const auto VC_MOTION_HEIGHT = 168;
const auto VC_SPACE = 8;
const auto VC_MARGIN = 10;
const auto VC_FAKE_PLATFORM_COUNT = 3;

const auto S_TAB = "viewercount.tab";
const auto S_TEMPLATE = "viewercount.template";
const auto S_ALL_VIEWER_COUNT = "viewercount.allviewercount";
const auto S_LAYOUT = "viewercount.layout";
const auto S_FONT = "viewercount.font";
const auto S_COLOR = "viewercount.color";
const auto S_MOTION = "viewercount.motion";
const auto S_BACKGROUND_COLOR_T1 = "viewercount.backgroundcolor.t1";
const auto S_BACKGROUND_COLOR_T2 = "viewercount.backgroundcolor.t2";
const auto S_BACKGROUND_COLOR_T3 = "viewercount.backgroundcolor.t3";
const auto S_BACKGROUND_COLOR_T4 = "viewercount.backgroundcolor.t4";
const auto S_BACKGROUND_COLOR_T5 = "viewercount.backgroundcolor.t5";
const auto S_BACKGROUND_COLOR_T6 = "viewercount.backgroundcolor.t6";
const auto S_H_LINE_1 = "viewercount.backgroundcolor.hline1";
const auto S_H_LINE_2 = "viewercount.backgroundcolor.hline2";
const auto S_H_LINE_3 = "viewercount.backgroundcolor.hline3";
const auto S_H_LINE_4 = "viewercount.backgroundcolor.hline4";

#if defined(Q_OS_WIN)
static const char *const s_default_font = "Malgun Gothic";
#elif defined(Q_OS_MACOS)
static const char *const s_default_font = ".AppleSystemUIFont";
#endif

struct viewer_count_source {
	obs_source_t *m_source = nullptr;

	int m_platform_count = 0;
	int m_width = 0;
	int m_height = 0;

	obs_source_t *m_browser = nullptr;
	gs_texture_t *m_source_texture = nullptr;

	QJsonObject m_viewer_count;
	QJsonObject m_live_start;

	void update(const QString &type, const QJsonObject &data = {})
	{
		if (!m_browser) {
			return;
		}

		if (!updateSize()) {
			auto event = pls::JsonDocument<QJsonObject>().add(QStringLiteral("type"), type).add(QStringLiteral("data"), data).toByteArray();
			PLS_PLUGIN_INFO("viewerCountEvent: %s", event.constData());
			pls_source_dispatch_cef_js(m_browser, "viewerCountEvent", event.constData());
		}
	}
	QJsonObject settingsData(obs_data_t *settings, QJsonObject &data) const
	{
		data[QStringLiteral("template")] = (int)obs_data_get_int(settings, S_TEMPLATE);
		data[QStringLiteral("allVisible")] = obs_data_get_int(settings, S_ALL_VIEWER_COUNT) == 0;
		data[QStringLiteral("layout")] = (obs_data_get_int(settings, S_LAYOUT) == 0) ? QStringLiteral("Horizontal") : QStringLiteral("Vertical");
		return data;
	}
	QJsonObject settingsData(obs_data_t *settings, bool bkgenabled, const QString &bkgcolor, QJsonObject &data) const
	{
		data[QStringLiteral("background")] = QJsonObject({{QStringLiteral("enabled"), bkgenabled}, {QStringLiteral("color"), bkgcolor}});
		return settingsData(settings, data);
	}
	QJsonObject settingsData(obs_data_t *settings, int bkgcolor, QJsonObject &data) const
	{
		data[QStringLiteral("background")] = QJsonObject({{QStringLiteral("enabled"), true}, {QStringLiteral("color"), bkgcolor}});
		return settingsData(settings, data);
	}
	QJsonObject settingsData(obs_data_t *settings) const
	{
		if (auto temp = (int)obs_data_get_int(settings, S_TEMPLATE); temp == 0) {
			QJsonObject data;

			obs_data_t *font = obs_data_get_obj(settings, S_FONT);
			QString family = QString::fromUtf8(obs_data_get_string(font, "font-family"));
			QString weight = QString::fromUtf8(obs_data_get_string(font, "font-weight"));
			obs_data_release(font);
			data[QStringLiteral("font")] = QJsonObject({{QStringLiteral("family"), family}, {QStringLiteral("weight"), weight}});

			QColor color = pls_qint64_to_qcolor(obs_data_get_int(settings, S_COLOR));
			data[QStringLiteral("color")] = QString::asprintf("#%02x%02x%02x", color.red(), color.green(), color.blue());

			obs_data_t *background_color = obs_data_get_obj(settings, S_BACKGROUND_COLOR_T1);
			QColor bkgcolor = pls_qint64_to_qcolor(obs_data_get_int(background_color, "color"));
			auto alpha = (int)obs_data_get_int(background_color, "alpha");
			bool enabled = obs_data_get_bool(background_color, "enabled");
			obs_data_release(background_color);

			return settingsData(settings, enabled, QString::asprintf("#%02x%02x%02x%02x", bkgcolor.red(), bkgcolor.green(), bkgcolor.blue(), (int)(255.0 * alpha / 100.0)), data);
		} else if (temp == 1) {
			QJsonObject data;
			return settingsData(settings, (int)obs_data_get_int(settings, S_BACKGROUND_COLOR_T2), data);
		} else if (temp == 2) {
			QJsonObject data;
			return settingsData(settings, (int)obs_data_get_int(settings, S_BACKGROUND_COLOR_T3), data);
		} else if (temp == 3) {
			QJsonObject data;
			return settingsData(settings, (int)obs_data_get_int(settings, S_BACKGROUND_COLOR_T4), data);
		} else if (temp == 4) {
			QJsonObject data;
			return settingsData(settings, (int)obs_data_get_int(settings, S_BACKGROUND_COLOR_T5), data);
		} else if (temp == 5) {
			QJsonObject data;
			data[QStringLiteral("motion")] = obs_data_get_int(settings, S_MOTION) == 0 ? QStringLiteral("Repeat") : QStringLiteral("Off");
			return settingsData(settings, (int)obs_data_get_int(settings, S_BACKGROUND_COLOR_T6), data);
		}
		return QJsonObject();
	}
	void updateSettings(obs_data_t *settings)
	{
		auto data = settingsData(settings);
		update(QStringLiteral("setting"), data);
	}
	void updateViewerCount(const QList<QPair<QString, int>> &viewerCounts)
	{
		QJsonObject data;
		pls_for_each(viewerCounts, [&data](const QPair<QString, int> &viewerCount) { data.insert(viewerCount.first, viewerCount.second); });
		update(QStringLiteral("viewerCount"), data);
	}
	void updateLiveStart() { update(QStringLiteral("liveStart")); }
	void updateLiveEnd() { update(QStringLiteral("liveEnd")); }
	bool updateSize()
	{
		int width = 0;
		int height = 0;
		if (!m_source || !m_browser) {
			width = VC_DEFAULT_WIDTH;
			height = VC_DEFAULT_HEIGHT;
		} else {
			obs_data_t *settings = obs_source_get_settings(m_source);
			auto temp = (int)obs_data_get_int(settings, S_TEMPLATE);
			bool isHorizontal = obs_data_get_int(settings, S_LAYOUT) == 0;
			bool allVisible = obs_data_get_int(settings, S_ALL_VIEWER_COUNT) == 0;
			obs_data_release(settings);

			int platformCount = 0;
			if (m_platform_count <= 0) {
				platformCount = VC_FAKE_PLATFORM_COUNT + (allVisible ? 1 : 0);
			} else {
				platformCount = m_platform_count + ((allVisible && (m_platform_count > 1)) ? 1 : 0);
			}

			calcSize(width, height, temp, isHorizontal, platformCount);
		}

		if (width < VC_DEFAULT_WIDTH) {
			width = VC_DEFAULT_WIDTH;
		}
		if (height < VC_DEFAULT_HEIGHT) {
			height = VC_DEFAULT_HEIGHT;
		}

		if (m_width != width || m_height != height) {
			m_width = width;
			m_height = height;
			PLS_PLUGIN_INFO("update size, width: %d, height: %d", m_width, m_height);
		} else {
			return false;
		}

		PLS_PLUGIN_DEBUG("viewer count size, width: %d, height: %d", width, height);

		// init image source
		obs_data_t *browser_settings = obs_source_get_settings(m_browser);
		obs_data_set_string(browser_settings, "url", getUrl().toUtf8().constData());
		obs_data_set_int(browser_settings, "width", m_width);
		obs_data_set_int(browser_settings, "height", m_height);
		obs_source_update(m_browser, browser_settings);
		obs_data_release(browser_settings);
		return true;
	}
	void calcSize(int &width, int &height, int temp, bool isHorizontal, int platformCount) const
	{
		switch (temp) {
		case 0: // template 1
			if (isHorizontal) {
				width = VC_ONE_WIDTH_T1 * platformCount + VC_SPACE * (platformCount - 1) + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T1 + VC_MARGIN * 2;
			} else {
				width = VC_ONE_WIDTH_T1 + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T1 * platformCount + VC_SPACE * (platformCount - 1) + VC_MARGIN * 2;
			}
			break;
		case 1: // template 2
			if (isHorizontal) {
				width = VC_ONE_WIDTH_T2 * platformCount + VC_SPACE * (platformCount - 1) + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T2 + VC_MARGIN * 2;
			} else {
				width = VC_ONE_WIDTH_T2 + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T2 * platformCount + VC_SPACE * (platformCount - 1) + VC_MARGIN * 2;
			}
			break;
		case 2: // template 3
			if (isHorizontal) {
				width = VC_ONE_WIDTH_T3 * platformCount + VC_SPACE * (platformCount - 1) + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T3 + VC_MARGIN * 2;
			} else {
				width = VC_ONE_WIDTH_T3 + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T3 * platformCount + VC_SPACE * (platformCount - 1) + VC_MARGIN * 2;
			}
			break;
		case 3: // template 4
			if (isHorizontal) {
				width = VC_ONE_WIDTH_T4 * platformCount + VC_SPACE * (platformCount - 1) + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T4 + VC_MARGIN * 2;
			} else {
				width = VC_ONE_WIDTH_T4 + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T4 * platformCount + VC_SPACE * (platformCount - 1) + VC_MARGIN * 2;
			}
			break;
		case 4: // template 5
			if (isHorizontal) {
				width = VC_ONE_WIDTH_T5 * platformCount + VC_SPACE * (platformCount - 1) + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T5 + VC_MARGIN * 2;
			} else {
				width = VC_ONE_WIDTH_T5 + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T5 * platformCount + VC_SPACE * (platformCount - 1) + VC_MARGIN * 2;
			}
			break;
		case 5: // template 6
			calcT6Size(width, height, isHorizontal, platformCount);
			break;
		default:
			break;
		}
	}
	void calcT6Size(int &width, int &height, bool isHorizontal, int platformCount) const
	{
		if (isHorizontal) {
			width = VC_ONE_WIDTH_T6 * platformCount + VC_SPACE * platformCount + VC_MOTION_WIDTH + VC_MARGIN * 2;
			height = qMax(VC_ONE_HEIGHT_T6, VC_MOTION_HEIGHT) + VC_MARGIN * 2;
		} else {
			width = 168;
			if (platformCount == 1) {
				height = 204;
			} else if (platformCount == 2) {
				height = 243;
			} else if (platformCount == 3) {
				height = 282;
			} else if (platformCount == 4) {
				height = 321;
			} else {
				width = qMax(VC_ONE_WIDTH_T6, VC_MOTION_WIDTH) + VC_MARGIN * 2;
				height = VC_ONE_HEIGHT_T6 * platformCount + VC_SPACE * platformCount + VC_MOTION_HEIGHT + VC_MARGIN * 2;
			}
		}
	}
	QString getUrl()
	{
		pls_source_send_notify(m_source, OBS_SOURCE_VIEWER_COUNT_UPDATE_PARAMS, OBS_SOURCE_VIEWER_COUNT_UPDATE_PARAMS_SUB_CODE_UPDATE_URL);

		auto localFileUtf8 = obs_module_file("web/viewerCount.html");
		auto localFile = pls_to_abs_path(QString::fromUtf8(localFileUtf8));
		bfree(localFileUtf8);

		QUrl url = QUrl::fromLocalFile(localFile);

		QUrlQuery query;
		obs_data_t *settings = obs_source_get_settings(m_source);
		auto data = settingsData(settings);
		query.addQueryItem(QStringLiteral("setting"), QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact)));
		obs_data_release(settings);

		if (!m_viewer_count.isEmpty()) {
			query.addQueryItem(QStringLiteral("viewerCount"), QString::fromUtf8(QJsonDocument(m_viewer_count.value(QStringLiteral("data")).toObject()).toJson(QJsonDocument::Compact)));
			m_viewer_count = QJsonObject();
		}

		if (!m_live_start.isEmpty()) {
			query.addQueryItem(QStringLiteral("liveStart"), QString::fromUtf8(QJsonDocument(m_live_start.value(QStringLiteral("data")).toObject()).toJson(QJsonDocument::Compact)));
			m_live_start = QJsonObject();
		}

		url.setQuery(query);

		QString urlstr = url.toString(QUrl::FullyEncoded);
		debug("%s", urlstr.toUtf8().constData());
		return urlstr;
	}
};

static const char *viewer_count_source_get_name(void *param)
{
	pls_unused(param);
	return obs_module_text("ViewerCount.SourceName");
}

static bool tab_changed(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings)
{
	pls_unused(prop);
	auto tab = (int)obs_data_get_int(settings, S_TAB);
	auto tpl = (int)obs_data_get_int(settings, S_TEMPLATE);
	if (tab == 0) { // templates
		obs_property_set_visible(obs_properties_get(props, S_TEMPLATE), true);
		obs_property_set_visible(obs_properties_get(props, S_ALL_VIEWER_COUNT), false);
		obs_property_set_visible(obs_properties_get(props, S_LAYOUT), false);
		obs_property_set_visible(obs_properties_get(props, S_MOTION), false);
		obs_property_set_visible(obs_properties_get(props, S_FONT), false);
		obs_property_set_visible(obs_properties_get(props, S_COLOR), false);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T1), false);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T2), false);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T3), false);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T4), false);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T5), false);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T6), false);
		obs_property_set_visible(obs_properties_get(props, S_H_LINE_1), false);
		obs_property_set_visible(obs_properties_get(props, S_H_LINE_2), false);
		obs_property_set_visible(obs_properties_get(props, S_H_LINE_3), false);
		obs_property_set_visible(obs_properties_get(props, S_H_LINE_4), false);
	} else {
		obs_property_set_visible(obs_properties_get(props, S_TEMPLATE), false);
		obs_property_set_visible(obs_properties_get(props, S_ALL_VIEWER_COUNT), true);
		obs_property_set_visible(obs_properties_get(props, S_LAYOUT), tpl == 0 || tpl == 1 || tpl == 3 || tpl == 4);
		obs_property_set_visible(obs_properties_get(props, S_MOTION), tpl == 5);
		obs_property_set_visible(obs_properties_get(props, S_FONT), tpl == 0);
		obs_property_set_visible(obs_properties_get(props, S_COLOR), tpl == 0);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T1), tpl == 0);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T2), tpl == 1);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T3), tpl == 2);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T4), tpl == 3);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T5), tpl == 4);
		obs_property_set_visible(obs_properties_get(props, S_BACKGROUND_COLOR_T6), tpl == 5);
		obs_property_set_visible(obs_properties_get(props, S_H_LINE_1), true);
		obs_property_set_visible(obs_properties_get(props, S_H_LINE_2), tpl == 0 || tpl == 1 || tpl == 3 || tpl == 4);
		obs_property_set_visible(obs_properties_get(props, S_H_LINE_3), tpl == 5);
		obs_property_set_visible(obs_properties_get(props, S_H_LINE_4), true);
	}
	return true;
}

static bool template_changed(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings)
{
	pls_unused(props, prop);
	if (!obs_data_get_bool(settings, "is_ui_click_cb"))
		return false;

	obs_data_set_bool(settings, "is_ui_click_cb", false);

	auto temp = obs_data_get_int(settings, S_TEMPLATE);

	obs_data_set_int(settings, S_LAYOUT, (temp != 5) ? 0 : 1);
	obs_data_set_int(settings, S_MOTION, 0);

	obs_data_t *font = obs_data_create();
	obs_data_set_string(font, "font-family", s_default_font);
	obs_data_set_string(font, "font-weight", "Bold");
	obs_data_set_obj(settings, S_FONT, font);
	obs_data_release(font);

	obs_data_set_int(settings, S_COLOR, 0xffffff);

	/*
	* color alpha checkbox
	* data obj
	*	color: int
	*	enabled: bool
	*	alpha: int 0-100 (percent)
	*/
	obs_data_t *background_color = obs_data_create();
	obs_data_set_int(background_color, "color", 0x000000);
	obs_data_set_int(background_color, "alpha", 96);
	obs_data_set_bool(background_color, "enabled", false);
	obs_data_set_obj(settings, S_BACKGROUND_COLOR_T1, background_color);
	obs_data_release(background_color);

	obs_data_set_int(settings, S_BACKGROUND_COLOR_T2, 0);
	obs_data_set_int(settings, S_BACKGROUND_COLOR_T3, 1);
	obs_data_set_int(settings, S_BACKGROUND_COLOR_T4, 2);
	obs_data_set_int(settings, S_BACKGROUND_COLOR_T5, 1);
	obs_data_set_int(settings, S_BACKGROUND_COLOR_T6, 1);

	return true;
}

static obs_properties_t *viewer_count_source_properties(void *data)
{
	pls_unused(data);

	obs_properties_t *props = pls_properties_create();

	obs_property_t *tab_prop = pls_properties_tm_add_tab(props, S_TAB);
	obs_property_set_modified_callback(tab_prop, tab_changed);

	obs_property_t *template_prop = pls_properties_add_template_list(props, S_TEMPLATE, obs_module_text("ViewerCount.TemplateTip"));
	obs_property_set_modified_callback(template_prop, template_changed);
	pls_property_set_ignore_callback_when_refresh(template_prop, true);

	obs_property_t *all_viewer_count_prop = pls_properties_add_bool_group(props, S_ALL_VIEWER_COUNT, obs_module_text("ViewerCount.AllViewerCount"));
	obs_property_set_long_description(all_viewer_count_prop, obs_module_text("ViewerCount.AllViewerCount.LongDesc"));
	pls_property_bool_group_add_item(all_viewer_count_prop, nullptr, obs_module_text("ViewerCount.Show"), nullptr, nullptr);
	pls_property_bool_group_add_item(all_viewer_count_prop, nullptr, obs_module_text("ViewerCount.Hide"), nullptr, nullptr);

	pls_properties_add_line(props, S_H_LINE_1, "");

	obs_property_t *layout_prop = pls_properties_add_bool_group(props, S_LAYOUT, obs_module_text("ViewerCount.Layout"));
	pls_property_bool_group_add_item(layout_prop, nullptr, obs_module_text("ViewerCount.Layout.Horizontal"), nullptr, nullptr);
	pls_property_bool_group_add_item(layout_prop, nullptr, obs_module_text("ViewerCount.Layout.Vertical"), nullptr, nullptr);

	pls_properties_add_line(props, S_H_LINE_2, "");

	obs_property_t *motion_prop = pls_properties_add_bool_group(props, S_MOTION, obs_module_text("ViewerCount.Motion"));
	pls_property_bool_group_add_item(motion_prop, nullptr, obs_module_text("ViewerCount.Motion.Repeat"), nullptr, nullptr);
	pls_property_bool_group_add_item(motion_prop, nullptr, obs_module_text("ViewerCount.Motion.Off"), nullptr, nullptr);

	pls_properties_add_line(props, S_H_LINE_3, "");

	pls_properties_add_font_simple(props, S_FONT, obs_module_text("ViewerCount.Font"));

	obs_properties_add_color(props, S_COLOR, obs_module_text("ViewerCount.Color"));

	pls_properties_add_color_alpha_checkbox(props, S_BACKGROUND_COLOR_T1, obs_module_text("ViewerCount.BackgroundColor"));

	obs_property_t *background_color_t2_prop = pls_properties_add_image_group(props, S_BACKGROUND_COLOR_T2, obs_module_text("ViewerCount.BackgroundColor"), 1, 10, PLS_IMAGE_STYLE_BORDER_BUTTON);
	pls_property_image_group_add_item(background_color_t2_prop, "t2_bkgcolor_01", ":/viewer-count/resource/images/bkgcolor-01.png", 0, nullptr);
	pls_property_image_group_add_item(background_color_t2_prop, "t2_bkgcolor_02", ":/viewer-count/resource/images/bkgcolor-03.png", 1, nullptr);
	pls_property_image_group_add_item(background_color_t2_prop, "t2_bkgcolor_03", ":/viewer-count/resource/images/bkgcolor-04.png", 2, nullptr);
	pls_property_image_group_add_item(background_color_t2_prop, "t2_bkgcolor_04", ":/viewer-count/resource/images/bkgcolor-05.png", 3, nullptr);
	pls_property_image_group_add_item(background_color_t2_prop, "t2_bkgcolor_05", ":/viewer-count/resource/images/bkgcolor-06.png", 4, nullptr);
	pls_property_image_group_add_item(background_color_t2_prop, "t2_bkgcolor_06", ":/viewer-count/resource/images/bkgcolor-07.png", 5, nullptr);
	pls_property_image_group_add_item(background_color_t2_prop, "t2_bkgcolor_07", ":/viewer-count/resource/images/bkgcolor-08.png", 6, nullptr);
	pls_property_image_group_add_item(background_color_t2_prop, "t2_bkgcolor_08", ":/viewer-count/resource/images/bkgcolor-09.png", 7, nullptr);
	pls_property_image_group_add_item(background_color_t2_prop, "t2_bkgcolor_09", ":/viewer-count/resource/images/bkgcolor-10.png", 8, nullptr);
	pls_property_image_group_add_item(background_color_t2_prop, "t2_bkgcolor_10", ":/viewer-count/resource/images/bkgcolor-11.png", 9, nullptr);

	obs_property_t *background_color_t3_prop = pls_properties_add_image_group(props, S_BACKGROUND_COLOR_T3, obs_module_text("ViewerCount.BackgroundColor"), 1, 10, PLS_IMAGE_STYLE_BORDER_BUTTON);
	pls_property_image_group_add_item(background_color_t3_prop, "t3_bkgcolor_01", ":/viewer-count/resource/images/bkgcolor-01.png", 0, nullptr);
	pls_property_image_group_add_item(background_color_t3_prop, "t3_bkgcolor_02", ":/viewer-count/resource/images/bkgcolor-03.png", 1, nullptr);
	pls_property_image_group_add_item(background_color_t3_prop, "t3_bkgcolor_03", ":/viewer-count/resource/images/bkgcolor-04.png", 2, nullptr);
	pls_property_image_group_add_item(background_color_t3_prop, "t3_bkgcolor_04", ":/viewer-count/resource/images/bkgcolor-05.png", 3, nullptr);
	pls_property_image_group_add_item(background_color_t3_prop, "t3_bkgcolor_05", ":/viewer-count/resource/images/bkgcolor-06.png", 4, nullptr);
	pls_property_image_group_add_item(background_color_t3_prop, "t3_bkgcolor_06", ":/viewer-count/resource/images/bkgcolor-07.png", 5, nullptr);
	pls_property_image_group_add_item(background_color_t3_prop, "t3_bkgcolor_07", ":/viewer-count/resource/images/bkgcolor-08.png", 6, nullptr);
	pls_property_image_group_add_item(background_color_t3_prop, "t3_bkgcolor_08", ":/viewer-count/resource/images/bkgcolor-09.png", 7, nullptr);
	pls_property_image_group_add_item(background_color_t3_prop, "t3_bkgcolor_09", ":/viewer-count/resource/images/bkgcolor-10.png", 8, nullptr);
	pls_property_image_group_add_item(background_color_t3_prop, "t3_bkgcolor_10", ":/viewer-count/resource/images/bkgcolor-11.png", 9, nullptr);

	obs_property_t *background_color_t4_prop = pls_properties_add_image_group(props, S_BACKGROUND_COLOR_T4, obs_module_text("ViewerCount.BackgroundColor"), 1, 4, PLS_IMAGE_STYLE_BORDER_BUTTON);
	pls_property_image_group_add_item(background_color_t4_prop, "t4_bkgcolor_01", ":/viewer-count/resource/images/bkgcolor-01.png", 0, nullptr);
	pls_property_image_group_add_item(background_color_t4_prop, "t4_bkgcolor_02", ":/viewer-count/resource/images/bkgcolor-02.png", 1, nullptr);
	pls_property_image_group_add_item(background_color_t4_prop, "t4_bkgcolor_03", ":/viewer-count/resource/images/bkgcolor-03.png", 2, nullptr);
	pls_property_image_group_add_item(background_color_t4_prop, "t4_bkgcolor_04", ":/viewer-count/resource/images/bkgcolor-04.png", 3, nullptr);

	obs_property_t *background_color_t5_prop = pls_properties_add_image_group(props, S_BACKGROUND_COLOR_T5, obs_module_text("ViewerCount.BackgroundColor"), 1, 13, PLS_IMAGE_STYLE_BORDER_BUTTON);
	pls_property_image_group_add_item(background_color_t5_prop, "t5_bkgcolor_01", ":/viewer-count/resource/images/bkgcolor-01.png", 0, nullptr);
	pls_property_image_group_add_item(background_color_t5_prop, "t5_bkgcolor_02", ":/viewer-count/resource/images/bkgcolor-02.png", 1, nullptr);
	pls_property_image_group_add_item(background_color_t5_prop, "t5_bkgcolor_03", ":/viewer-count/resource/images/bkgcolor-03.png", 2, nullptr);
	pls_property_image_group_add_item(background_color_t5_prop, "t5_bkgcolor_04", ":/viewer-count/resource/images/bkgcolor-04.png", 3, nullptr);

	obs_property_t *background_color_t6_prop = pls_properties_add_image_group(props, S_BACKGROUND_COLOR_T6, obs_module_text("ViewerCount.BackgroundColor"), 1, 13, PLS_IMAGE_STYLE_BORDER_BUTTON);
	pls_property_image_group_add_item(background_color_t6_prop, "t6_bkgcolor_01", ":/viewer-count/resource/images/bkgcolor-01.png", 0, nullptr);
	pls_property_image_group_add_item(background_color_t6_prop, "t6_bkgcolor_02", ":/viewer-count/resource/images/bkgcolor-05.png", 1, nullptr);

	pls_properties_add_line(props, S_H_LINE_4, "");

	return props;
}

static void init_browser_source(struct viewer_count_source *context)
{
	if (!context || context->m_browser) {
		return;
	}

	// init image source
	obs_data_t *browser_settings = obs_data_create();
	obs_data_set_string(browser_settings, "url", context->getUrl().toUtf8().constData());
	obs_data_set_bool(browser_settings, "is_local_file", false);
	obs_data_set_bool(browser_settings, "reroute_audio", true);
	obs_data_set_bool(browser_settings, "ignore_reload", true);
	obs_data_set_int(browser_settings, "width", context->m_width);
	obs_data_set_int(browser_settings, "height", context->m_height);
	context->m_browser = obs_source_create_private("browser_source", "prism_viewer_count_browser_source", browser_settings);
	obs_data_release(browser_settings);

	obs_source_inc_active(context->m_browser);
	obs_source_inc_showing(context->m_browser);
}

static void viewer_count_source_update(void *data, obs_data_t *settings)
{
	auto context = (viewer_count_source *)(data);
	context->updateSettings(settings);
}

static void viewer_count_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, S_TAB, 0);
	obs_data_set_default_int(settings, S_TEMPLATE, 0);

	obs_data_set_default_int(settings, S_ALL_VIEWER_COUNT, 0);
	obs_data_set_default_int(settings, S_LAYOUT, 0);
	obs_data_set_default_int(settings, S_MOTION, 0);

	obs_data_t *font = obs_data_create();
	obs_data_set_string(font, "font-family", s_default_font);
	obs_data_set_string(font, "font-weight", "Bold");
	obs_data_set_default_obj(settings, S_FONT, font);
	obs_data_release(font);

	obs_data_set_default_int(settings, S_COLOR, 0xffffff);

	/*
	* color alpha checkbox
	* data obj
	*	color: int
	*	enabled: bool
	*	alpha: int 0-100 (percent)
	*/
	obs_data_t *background_color = obs_data_create();
	obs_data_set_int(background_color, "color", 0x000000);
	obs_data_set_int(background_color, "alpha", 96);
	obs_data_set_bool(background_color, "enabled", false);
	obs_data_set_default_obj(settings, S_BACKGROUND_COLOR_T1, background_color);
	obs_data_release(background_color);

	obs_data_set_default_int(settings, S_BACKGROUND_COLOR_T2, 0);
	obs_data_set_default_int(settings, S_BACKGROUND_COLOR_T3, 1);
	obs_data_set_default_int(settings, S_BACKGROUND_COLOR_T4, 2);
	obs_data_set_default_int(settings, S_BACKGROUND_COLOR_T5, 1);
	obs_data_set_default_int(settings, S_BACKGROUND_COLOR_T6, 1);
}

static void source_notified(void *data, calldata_t *calldata)
{
	pls_used(calldata);
	auto context = static_cast<struct viewer_count_source *>(data);
	auto source = (uint64_t)calldata_int(calldata, "source_address");
	if (!source || (source != (uint64_t)context->m_source) || !data)
		return;

	init_browser_source(context);
}

static void viewer_count_source_destroy(void *data);

static void *viewer_count_source_create(obs_data_t *settings, obs_source_t *source)
{
	//obs_source_set_capture_valid(source, true, OBS_SOURCE_ERROR_OK);

	auto context = pls_new_nothrow<viewer_count_source>();
	if (!context) {
		error("viewer count source create failed, because out of memory.");
		//obs_source_set_capture_valid(source, false, OBS_SOURCE_ERROR_UNKNOWN);
		return nullptr;
	}

	signal_handler_connect_ref(obs_get_signal_handler(), "source_create_finished", source_notified, context);

	context->m_source = source;
	context->m_width = VC_DEFAULT_WIDTH;
	context->m_height = VC_DEFAULT_HEIGHT;
	context->updateSettings(settings);
	return context;
}

static void viewer_count_source_destroy(void *data)
{
	auto context = (viewer_count_source *)(data);
	signal_handler_disconnect(obs_get_signal_handler(), "source_create_finished", source_notified, context);

	if (context->m_browser) {
		obs_source_dec_active(context->m_browser);
		obs_source_release(context->m_browser);
	}

	if (context->m_source_texture) {
		obs_enter_graphics();
		gs_texture_destroy(context->m_source_texture);
		context->m_source_texture = nullptr;
		obs_leave_graphics();
	}

	pls_delete(context);
}

static uint32_t viewer_count_source_get_width(void *data)
{
	auto context = (viewer_count_source *)(data);
	return context->m_width;
}

static uint32_t viewer_count_source_get_height(void *data)
{
	auto context = (viewer_count_source *)(data);
	return context->m_height;
}

static void viewer_count_source_clear_texture(gs_texture_t *tex)
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

static void viewer_count_source_video_render(void *data, gs_effect_t *effect)
{
	auto context = (viewer_count_source *)(data);
	gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), context->m_source_texture);
	gs_draw_sprite(context->m_source_texture, 0, 0, 0);
	pls_used(effect);
}

static void viewer_count_source_render(void *data, obs_source_t *source)
{
	auto vc_source = (viewer_count_source *)(data);
	uint32_t source_width = obs_source_get_width(source);
	uint32_t source_height = obs_source_get_height(source);
	if (source_width <= 0 || source_height <= 0) {
		viewer_count_source_clear_texture(vc_source->m_source_texture);
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

static void viewer_count_source_video_tick(void *data, float /*seconds*/)
{
	if (auto context = (viewer_count_source *)(data); context->m_browser) {
		viewer_count_source_render(data, context->m_browser);
	}
}

static void viewer_count_source_activate(void *data)
{
	if (auto context = (viewer_count_source *)(data); context->m_browser) {
		obs_source_inc_active(context->m_browser);
	}
}

static void viewer_count_source_deactivate(void *data)
{
	if (auto context = (viewer_count_source *)(data); context->m_browser) {
		obs_source_dec_active(context->m_browser);
	}
}

static void viewer_count_update_extern_params(void *data, const calldata_t *extern_params)
{
	auto context = (viewer_count_source *)(data);

	const char *json = nullptr;
	long long sub_data = 0;
	if (!calldata_get_string(extern_params, "cjson", &json) || pls_is_empty(json) || !calldata_get_int(extern_params, "sub_code", &sub_data)) {
		return;
	}

	QJsonParseError error;
	QJsonDocument jdoc = QJsonDocument::fromJson(json, &error);
	if (error.error != QJsonParseError::NoError) {
		return;
	}

	bool sizeChanged = false;
	QJsonObject jobj = jdoc.object();
	if (QString type = jobj.value("type").toString(); type == QStringLiteral("viewerCount")) {
		context->m_platform_count = jobj.value("data").toObject().value("platforms").toArray().size();
		if (sub_data == OBS_SOURCE_VIEWER_COUNT_UPDATE_PARAMS_SUB_CODE_UPDATE_URL) {
			context->m_viewer_count = jobj;
		} else if (sub_data == OBS_SOURCE_VIEWER_COUNT_UPDATE_PARAMS_SUB_CODE_UPDATE_PARAMS) {
			sizeChanged = context->updateSize();
		}
	} else if (type == QStringLiteral("liveStart") && sub_data == OBS_SOURCE_VIEWER_COUNT_UPDATE_PARAMS_SUB_CODE_UPDATE_URL) {
		context->m_live_start = jobj;
	}

	if (sub_data == OBS_SOURCE_VIEWER_COUNT_UPDATE_PARAMS_SUB_CODE_UPDATE_PARAMS && !sizeChanged) {
		pls_source_dispatch_cef_js(context->m_browser, "viewerCountEvent", json);
	}
}

static void viewer_count_properties_edit_start(void *data, obs_data_t *settings)
{
	pls_unused(data, settings);
}

static void viewer_count_properties_edit_end(void *data, obs_data_t *settings, bool is_save_click)
{
	pls_unused(data, settings, is_save_click);
}

static void viewer_count_cef_dispatch_js(void *data, const char *event_name, const char *json_data)
{
	if (auto context = (viewer_count_source *)(data); context->m_browser) {
		pls_source_dispatch_cef_js(context->m_browser, event_name, json_data);
	}
}

void register_prism_viewer_count_source()
{
	obs_source_info info;
	memset(&info, 0, sizeof(info));
	info.id = "prism_viewer_count_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = viewer_count_source_get_name;
	info.create = viewer_count_source_create;
	info.destroy = viewer_count_source_destroy;
	info.get_defaults = viewer_count_source_defaults;
	info.get_properties = viewer_count_source_properties;
	info.activate = viewer_count_source_activate;
	info.deactivate = viewer_count_source_deactivate;
	info.video_render = viewer_count_source_video_render;
	info.video_tick = viewer_count_source_video_tick;
	info.get_width = viewer_count_source_get_width;
	info.get_height = viewer_count_source_get_height;
	info.update = viewer_count_source_update;
	info.icon_type = static_cast<obs_icon_type>(PLS_ICON_TYPE_VIEWER_COUNT);

	pls_source_info psi;
	memset(&psi, 0, sizeof(psi));
	psi.update_extern_params = viewer_count_update_extern_params;
	psi.properties_edit_start = viewer_count_properties_edit_start;
	psi.properties_edit_end = viewer_count_properties_edit_end;
	psi.cef_dispatch_js = viewer_count_cef_dispatch_js;
	register_pls_source_info(&info, &psi);

	obs_register_source(&info);
}

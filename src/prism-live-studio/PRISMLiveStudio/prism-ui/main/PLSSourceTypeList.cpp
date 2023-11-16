#include "PLSBasic.h"
#include "window-basic-main.hpp"
#include "pls-common-define.hpp"
#include "utils-api.h"

#define UseFreeMusic
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
using namespace common;
//------------------------ define preset type list ------------------------
struct SourceTypeInfo {
	QString id;
	bool visible = false;
	bool isNew = false;
	int descDisplayType = 0; //0:png, 1:gif
	SourceTypeInfo(QString id_, bool isVisible_ = false, bool _isNew = false, int _descType = 0) : id(id_), visible(isVisible_), isNew(_isNew), descDisplayType(_descType) {}
};

namespace {
struct LocalGlobalVar {
	static std::vector<std::vector<SourceTypeInfo>> presetSourceList;
};
std::vector<std::vector<SourceTypeInfo>> LocalGlobalVar::presetSourceList = {
	{SourceTypeInfo(OBS_DSHOW_SOURCE_ID, false, false),
	 SourceTypeInfo(AUDIO_INPUT_SOURCE_ID, false, false),
	 SourceTypeInfo(AUDIO_OUTPUT_SOURCE_ID),
	 SourceTypeInfo(PRISM_APP_AUDIO_SOURCE_ID),
	 SourceTypeInfo(OBS_APP_AUDIO_CAPTURE_ID),
	 SourceTypeInfo(PRISM_NDI_SOURCE_ID, false, false),
	 SourceTypeInfo(GAME_SOURCE_ID),
	 SourceTypeInfo(PRISM_REGION_SOURCE_ID),
	 SourceTypeInfo(PRISM_MONITOR_SOURCE_ID),
	 SourceTypeInfo(OBS_MACOS_SCREEN_CAPTURE_SOURCE_ID),
	 SourceTypeInfo(WINDOW_SOURCE_ID),
	 SourceTypeInfo(BROWSER_SOURCE_ID),
	 SourceTypeInfo(MEDIA_SOURCE_ID),
	 SourceTypeInfo(IMAGE_SOURCE_ID),
	 SourceTypeInfo(SLIDESHOW_SOURCE_ID),
	 SourceTypeInfo(COLOR_SOURCE_ID),
	 SourceTypeInfo(GDIP_TEXT_SOURCE_ID),
	 SourceTypeInfo(PRISM_INPUT_OVERLAY_SOURCE_ID),
	 SourceTypeInfo(PRISM_INPUT_HISTORY_SOURCE_ID),
	 SourceTypeInfo(DECKLINK_INPUT_SOURCE_ID),
	 SourceTypeInfo(SCENE_SOURCE_ID, true)},

	{
		SourceTypeInfo(PRISM_LENS_SOURCE_ID, false, true, 1),
		SourceTypeInfo(PRISM_LENS_MOBILE_SOURCE_ID, false, true, 1),
		SourceTypeInfo(PRISM_MOBILE_SOURCE_ID),
		SourceTypeInfo(PRISM_TEXT_TEMPLATE_ID, false, false, 1),
		SourceTypeInfo(PRISM_CHAT_SOURCE_ID, false, false, 1),
		SourceTypeInfo(PRISM_VIEWER_COUNT_SOURCE_ID, false, false, 1),
		SourceTypeInfo(PRISM_STICKER_SOURCE_ID, false, false, 1),
		SourceTypeInfo(PRISM_GIPHY_STICKER_SOURCE_ID, false, false, 1),
#ifdef UseFreeMusic
		SourceTypeInfo(BGM_SOURCE_ID, false, false, 0),
#endif // UseFreeMusic
		SourceTypeInfo(PRISM_SPECTRALIZER_SOURCE_ID, false, false, 1),
		SourceTypeInfo(PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, false, false, 1),
		SourceTypeInfo(PRISM_TIMER_SOURCE_ID, false, false, 1),

	},
};
}
bool isNewSource(const QString &id)
{
	for (const auto &sourceTypeInfos : LocalGlobalVar::presetSourceList) {
		for (auto sourceInfo : sourceTypeInfos) {
			if (sourceInfo.id == id) {
				return sourceInfo.isNew;
			}
		}
	}
	return false;
}
int getSourceDisplayType(const QString &id)
{
	for (const auto &sourceTypeInfos : LocalGlobalVar::presetSourceList) {
		for (auto sourceInfo : sourceTypeInfos) {
			if (sourceInfo.id == id) {
				return sourceInfo.descDisplayType;
			}
		}
	}
	return 0;
}

bool checkPluginID(const char *id)
{
	bool isPresetDefined = false;

	size_t count = LocalGlobalVar::presetSourceList.size();
	for (unsigned i = 0; i < (unsigned)count; ++i) {
		if (isPresetDefined)
			break;

		std::vector<SourceTypeInfo> &subList = LocalGlobalVar::presetSourceList[i];
		size_t subCount = subList.size();
		for (unsigned j = 0; j < (unsigned)subCount; ++j) {
			if (isPresetDefined)
				break;

			SourceTypeInfo &info = subList[j];
			if (0 == strcmp(id, info.id.toStdString().c_str())) {
				info.visible = true;
				isPresetDefined = true;
			}
		}
	}

	return isPresetDefined;
}

void PLSBasic::loadSourceTypeList(std::map<QString, bool> &monitors, std::vector<QString> &otherList)
{
	//----------------------- load source type list --------------------------
	size_t idx = 0;
	const char *id;
	const char *unversioned_type;
	while (obs_enum_input_types2(idx, &id, &unversioned_type)) {
		++idx;

		if (uint32_t caps = obs_get_source_output_flags(id); caps & OBS_SOURCE_CAP_DISABLED || caps & OBS_SOURCE_DEPRECATED)
			continue;

		if (!unversioned_type || (*unversioned_type) == 0)
			continue;

		if (monitors.find(unversioned_type) != monitors.end()) {
			monitors[unversioned_type] = true;
			continue;
		}
		bool isPresetDefined = checkPluginID(unversioned_type);
		if (!isPresetDefined) {
			otherList.push_back(unversioned_type);
		}
	}
}
QString PLSBasic::getSupportLanguage() const
{
	QStringList supportLang({"en_US", "ko_KR", "id_ID"});
	QString str(App()->GetLocale());
	return supportLang.contains(str.replace('-', '_')) ? str.toLower() : "en_us";
}

void PLSBasic::GetSourceTypeList(std::vector<std::vector<QString>> &preset, std::vector<QString> &other, std::map<QString, bool> &monitors)
{
	std::vector<std::vector<QString>> presetList;
	std::vector<QString> otherList;

	loadSourceTypeList(monitors, otherList);

	//----------------------- remove unfound source type --------------------------
	std::for_each(LocalGlobalVar::presetSourceList.begin(), LocalGlobalVar::presetSourceList.end(), [&presetList](const auto &subList) {
		std::vector<QString> typeList;

		std::for_each(subList.begin(), subList.end(), [&typeList](const auto &info) {
			if (info.visible)
				typeList.push_back(info.id);
		});

		if (!typeList.empty())
			presetList.push_back(typeList);
	});

	preset = presetList;
	other = otherList;
}

QPushButton *PLSBasic::CreateSourcePopupMenuCustomWidget(const char *id, const QString &display, const QSize &iconSize)
{
	QString qid = QString::fromUtf8(id);
	if (!strcmp(id, PRISM_STICKER_SOURCE_ID) || !strcmp(id, BGM_SOURCE_ID) || !strcmp(id, PRISM_CHAT_SOURCE_ID) || !strcmp(id, PRISM_TEXT_TEMPLATE_ID) ||
	    !strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID) || !strcmp(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID) || !strcmp(id, PRISM_MOBILE_SOURCE_ID) || !strcmp(id, PRISM_TIMER_SOURCE_ID)) {
		QPushButton *widget = pls_new<QPushButton>();
		widget->setObjectName("sourcePopupMenuCustomWidget");
		widget->setProperty("sourceId", qid);
		widget->setProperty("hasBadge", true);
		widget->setMouseTracking(true);
		QLabel *icon = pls_new<QLabel>(widget);
		icon->setObjectName("icon");
		icon->setMouseTracking(true);
		icon->setPixmap(OBSBasic::GetSourceIcon(id).pixmap(iconSize));
		QLabel *text = pls_new<QLabel>(display, widget);
		text->setObjectName("text");
		text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		text->setMouseTracking(true);
		QLabel *badge = pls_new<QLabel>(widget);
		badge->setObjectName("badge");
		badge->setAlignment(Qt::AlignCenter);
		badge->setMouseTracking(true);
		QHBoxLayout *layout = pls_new<QHBoxLayout>(widget);
		layout->setSizeConstraint(QLayout::SetMinimumSize);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->addWidget(icon);
		layout->addWidget(text);
		layout->addWidget(badge);
		layout->addStretch(1);
		return widget;
	} else {
		QPushButton *widget = pls_new<QPushButton>();
		widget->setObjectName("sourcePopupMenuCustomWidget");
		widget->setProperty("sourceId", qid);
		widget->setProperty("hasBadge", false);
		widget->setMouseTracking(true);
		QLabel *icon = pls_new<QLabel>(widget);
		icon->setObjectName("icon");
		icon->setMouseTracking(true);
		icon->setPixmap(OBSBasic::GetSourceIcon(id).pixmap(iconSize));
		QLabel *text = pls_new<QLabel>(display, widget);
		text->setObjectName("text");
		text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		text->setMouseTracking(true);
		QHBoxLayout *layout = pls_new<QHBoxLayout>(widget);
		layout->setSizeConstraint(QLayout::SetMinimumSize);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->addWidget(icon);
		layout->addWidget(text);
		layout->addStretch(1);
		return widget;
	}
}

#include "window-basic-main.hpp"
#include "pls-common-define.hpp"

#define UseFreeMusic
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

//------------------------ define preset type list ------------------------
struct SourceTypeInfo {
	QString id;
	bool visible;
	bool isNew;
	int descDisplayType; //0:png, 1:gif
	SourceTypeInfo(QString id_, bool isVisible_ = false, bool _isNew = false, int _descType = 0) : visible(isVisible_), id(id_), isNew(_isNew), descDisplayType(_descType) {}
};

std::vector<std::vector<SourceTypeInfo>> presetSourceList = {
	{SourceTypeInfo(DSHOW_SOURCE_ID, false, false), SourceTypeInfo(AUDIO_INPUT_SOURCE_ID, false, false), SourceTypeInfo(AUDIO_OUTPUT_SOURCE_ID), SourceTypeInfo(PRISM_NDI_SOURCE_ID, false, false),
	 SourceTypeInfo(GAME_SOURCE_ID), SourceTypeInfo(PRISM_REGION_SOURCE_ID), SourceTypeInfo(PRISM_MONITOR_SOURCE_ID), SourceTypeInfo(WINDOW_SOURCE_ID), SourceTypeInfo(BROWSER_SOURCE_ID),
	 SourceTypeInfo(MEDIA_SOURCE_ID), SourceTypeInfo(IMAGE_SOURCE_ID), SourceTypeInfo(SLIDESHOW_SOURCE_ID), SourceTypeInfo(COLOR_SOURCE_ID), SourceTypeInfo(GDIP_TEXT_SOURCE_ID),
	 SourceTypeInfo(SCENE_SOURCE_ID, true)},

	{
		SourceTypeInfo(PRISM_MOBILE_SOURCE_ID),
		SourceTypeInfo(PRISM_TEXT_MOTION_ID, false, false, 1),
		SourceTypeInfo(PRISM_CHAT_SOURCE_ID, false, false, 1),
		SourceTypeInfo(PRISM_STICKER_SOURCE_ID, false, true, 1),
		SourceTypeInfo(PRISM_GIPHY_STICKER_SOURCE_ID, false, false, 1),
#ifdef UseFreeMusic
		SourceTypeInfo(BGM_SOURCE_ID, false, false, 0),
#endif // UseFreeMusic
		SourceTypeInfo(PRISM_SPECTRALIZER_SOURCE_ID, false, false, 1),
		SourceTypeInfo(PRISM_BACKGROUND_TEMPLATE_SOURCE_ID, false, false, 1),
		SourceTypeInfo(PRISM_TIMER_SOURCE_ID, false, true, 1),

	},
};
bool isNewSource(const QString &id)
{
	for (auto sourceTypeInfos : presetSourceList) {
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
	for (auto sourceTypeInfos : presetSourceList) {
		for (auto sourceInfo : sourceTypeInfos) {
			if (sourceInfo.id == id) {
				return sourceInfo.descDisplayType;
			}
		}
	}
	return 0;
}
void PLSBasic::GetSourceTypeList(std::vector<std::vector<QString>> &preset, std::vector<QString> &other, std::map<QString, bool> &monitors)
{
	std::vector<std::vector<QString>> presetList;
	std::vector<QString> otherList;

	//----------------------- load source type list --------------------------
	size_t idx = 0;
	const char *id;
	while (obs_enum_input_types(idx++, &id)) {
		uint32_t caps = obs_get_source_output_flags(id);
		if (caps & OBS_SOURCE_CAP_DISABLED || caps & OBS_SOURCE_DEPRECATED)
			continue;

		if (!id || (*id) == 0)
			continue;

		if (monitors.find(id) != monitors.end()) {
			monitors[id] = true;
			continue;
		}

		bool isPresetDefined = false;

		for (unsigned i = 0; i < presetSourceList.size() && !isPresetDefined; ++i) {
			std::vector<SourceTypeInfo> &subList = presetSourceList[i];
			for (unsigned j = 0; j < subList.size() && !isPresetDefined; ++j) {
				SourceTypeInfo &info = subList[j];
				if (0 == strcmp(id, info.id.toStdString().c_str())) {
					info.visible = true;
					isPresetDefined = true;
				}
			}
		}

		if (!isPresetDefined)
			otherList.push_back(id);
	}

	//----------------------- remove unfound source type --------------------------
	for (unsigned i = 0; i < presetSourceList.size(); ++i) {
		std::vector<QString> typeList;
		std::vector<SourceTypeInfo> &subList = presetSourceList[i];
		for (unsigned j = 0; j < subList.size(); ++j) {
			SourceTypeInfo &info = subList[j];
			if (info.visible)
				typeList.push_back(info.id);
		}

		if (!typeList.empty())
			presetList.push_back(typeList);
	}

	preset = presetList;
	other = otherList;
}

QPushButton *PLSBasic::CreateSourcePopupMenuCustomWidget(const char *id, const QString &display, const QSize &iconSize)
{
	QString qid = QString::fromUtf8(id);
	if (!strcmp(id, PRISM_STICKER_SOURCE_ID) || !strcmp(id, BGM_SOURCE_ID) || !strcmp(id, PRISM_CHAT_SOURCE_ID) || !strcmp(id, PRISM_TEXT_MOTION_ID) || !strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID) ||
	    !strcmp(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID) || !strcmp(id, PRISM_MOBILE_SOURCE_ID) || !strcmp(id, PRISM_STICKER_SOURCE_ID) || !strcmp(id, PRISM_TIMER_SOURCE_ID)) {
		QPushButton *widget = new QPushButton();
		widget->setObjectName("sourcePopupMenuCustomWidget");
		widget->setProperty("sourceId", qid);
		widget->setProperty("hasBadge", true);
		widget->setMouseTracking(true);
		QLabel *icon = new QLabel(widget);
		icon->setObjectName("icon");
		icon->setMouseTracking(true);
		icon->setPixmap(GetSourceIcon(id).pixmap(iconSize));
		QLabel *text = new QLabel(display, widget);
		text->setObjectName("text");
		text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		text->setMouseTracking(true);
		QLabel *badge = new QLabel(widget);
		badge->setObjectName("badge");
		badge->setAlignment(Qt::AlignCenter);
		badge->setMouseTracking(true);
		QHBoxLayout *layout = new QHBoxLayout(widget);
		layout->setSizeConstraint(QLayout::SetMinimumSize);
		layout->setMargin(0);
		layout->setSpacing(0);
		layout->addWidget(icon);
		layout->addWidget(text);
		layout->addWidget(badge);
		layout->addStretch(1);
		return widget;
	} else {
		QPushButton *widget = new QPushButton();
		widget->setObjectName("sourcePopupMenuCustomWidget");
		widget->setProperty("sourceId", qid);
		widget->setProperty("hasBadge", false);
		widget->setMouseTracking(true);
		QLabel *icon = new QLabel(widget);
		icon->setObjectName("icon");
		icon->setMouseTracking(true);
		icon->setPixmap(GetSourceIcon(id).pixmap(iconSize));
		QLabel *text = new QLabel(display, widget);
		text->setObjectName("text");
		text->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		text->setMouseTracking(true);
		QHBoxLayout *layout = new QHBoxLayout(widget);
		layout->setSizeConstraint(QLayout::SetMinimumSize);
		layout->setMargin(0);
		layout->setSpacing(0);
		layout->addWidget(icon);
		layout->addWidget(text);
		layout->addStretch(1);
		return widget;
	}
}

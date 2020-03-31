#include "window-basic-main.hpp"
#include "pls-common-define.hpp"

void PLSBasic::GetSourceTypeList(std::vector<std::vector<QString>> &preset, std::vector<QString> &other)
{
	static bool typeInited = false;
	static std::vector<std::vector<QString>> presetList;
	static std::vector<QString> otherList;

	if (typeInited) {
		preset = presetList;
		other = otherList;
		return;
	}

	//------------------------ define preset type list ------------------------
	struct SourceTypeInfo {
		QString id;
		bool visible;
		SourceTypeInfo(QString id_, bool isVisible_ = false) : visible(isVisible_), id(id_) {}
	};

	std::vector<std::vector<SourceTypeInfo>> presetSourceList = {
		{
			SourceTypeInfo(DSHOW_SOURCE_ID),
			SourceTypeInfo(AUDIO_INPUT_SOURCE_ID),
		},
		{
			SourceTypeInfo(GAME_SOURCE_ID),
			SourceTypeInfo(PRISM_MONITOR_SOURCE_ID),
			SourceTypeInfo(WINDOW_SOURCE_ID),
		},
		{
			SourceTypeInfo(BROWSER_SOURCE_ID),
		},
		{
			SourceTypeInfo(MEDIA_SOURCE_ID),
			SourceTypeInfo(IMAGE_SOURCE_ID),
			SourceTypeInfo(SLIDESHOW_SOURCE_ID),
			SourceTypeInfo(COLOR_SOURCE_ID),
		},
		{
			SourceTypeInfo(GDIP_TEXT_SOURCE_ID),
			SourceTypeInfo(SCENE_SOURCE_ID, true),
			SourceTypeInfo(AUDIO_OUTPUT_SOURCE_ID),
		},
	};

	//----------------------- load source type list --------------------------
	size_t idx = 0;
	const char *id;
	while (obs_enum_input_types(idx++, &id)) {
		uint32_t caps = obs_get_source_output_flags(id);
		if (caps & OBS_SOURCE_CAP_DISABLED || caps & OBS_SOURCE_DEPRECATED)
			continue;

		if (!id || (*id) == 0)
			continue;

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

	typeInited = true;
	preset = presetList;
	other = otherList;
}

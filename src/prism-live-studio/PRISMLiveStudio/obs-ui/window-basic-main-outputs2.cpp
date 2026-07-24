#include <string>
#include <list>
#include <algorithm>
#include <cinttypes>
#include <QMessageBox>
#include <QThreadPool>
#include <qt-wrappers.hpp>
#include "audio-encoders.hpp"
#include "multitrack-video-error.hpp"
#include "window-basic-main.hpp"
#include "window-basic-main-outputs.hpp"
#include "window-basic-vcam.hpp"
#include "log/module_names.h"
#include "liblog.h"
#include <pls/pls-encoder.h>
#include <utils-api.h>
#include <pls/pls-source.h>
#include <pls/pls-dual-output.h>
#include <pls/pls-base.h>
#include "pls/pls-output.h"
#include "main/pls-log-profile.hpp"
#include "PLSPlatformApi.h"

using namespace std;

//------------------------------------------------------------------------------------------------
size_t pls_generate_hash(const std::string &input)
{
	std::hash<std::string> hasher;
	return hasher(input);
}

void pls_log_stream_platform(const list<PLSPlatformBase *> &platforms)
{
	auto count = platforms.size();
	size_t index = 1;
	for (const auto &item : platforms) {
		auto customRTMP = ChannelData::ChannelDataType::CustomType == item->getChannelType();
		auto name = item->getChannelName().toStdString();
		auto svc = item->getServiceType();

		PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT,
			"[%zu/%zu] log stream detail for platforms \n"
			"\t customRTMP=%d \n"
			"\t name=%s \n"
			"\t svc=%d",
			index, count, customRTMP, name.c_str(), (int)svc);

		++index;
	}
}

list<PLSPlatformBase *> pls_get_platforms(bool dualoutput, bool vertical)
{
	auto platforms = PLS_PLATFORM_ACTIVIED;
	if (dualoutput) {
		list<PLSPlatformBase *> temp;
		temp.swap(platforms);

		for (auto &item : temp) {
			if (item->isVerticalOutput() == vertical)
				platforms.push_back(item);
		}
	}

	return platforms;
}

void pls_fill_platform_info(struct pls_platform_info &info, bool isMultVideoTrack,
			    const std::list<PLSPlatformBase *> &platforms)
{
	if (platforms.empty()) {
		blog(LOG_WARNING, "%s: platform is empty", __FUNCTION__);
		assert(false);
		return;
	}

	auto count = platforms.size();
	auto platform = platforms.front();
	std::string name = platform->getChannelName().toStdString();
	ChannelData::ChannelDataType type = platform->getChannelType();

	if (isMultVideoTrack) {
		if (count != 1) {
			blog(LOG_WARNING, "%s: platform count is not 1 while using mult video track", __FUNCTION__);
			assert(false);
			return;
		}

		snprintf(info.channel_name, sizeof(info.channel_name), "%s", name.c_str());
		snprintf(info.channel_type, sizeof(info.channel_type), "%s", "TwitchMultVideoTrack");
		return;
	}

	if (count > 1) {
		snprintf(info.channel_name, sizeof(info.channel_name), "%s", "prism_mult_platforms");
		snprintf(info.channel_type, sizeof(info.channel_type), "%s", "NaverRelayStream");
		return;
	}

	// only one platform
	snprintf(info.channel_name, sizeof(info.channel_name), "%s", name.c_str());

	switch (type) {
	case ChannelData::ChannelDataType::ChannelType:
		snprintf(info.channel_type, sizeof(info.channel_type), "%s", "ChannelType");
		break;
	case ChannelData::ChannelDataType::CustomType:
		snprintf(info.channel_type, sizeof(info.channel_type), "%s", "CustomType");
		break;
	case ChannelData::ChannelDataType::SRTType:
		snprintf(info.channel_type, sizeof(info.channel_type), "%s", "SRTType");
		break;
	case ChannelData::ChannelDataType::RISTType:
		snprintf(info.channel_type, sizeof(info.channel_type), "%s", "RISTType");
		break;
	default:
		snprintf(info.channel_type, sizeof(info.channel_type), "ChannelType=%d", (int)type);
		break;
	}
}

void BasicOutputHandler::pls_log_output_info(obs_output_t *output, obs_service_t *service, const char *func)
{
	if (!output || !service || !func) {
		assert(false);
		return;
	}

	auto url = obs_service_get_connect_info(service, OBS_SERVICE_CONNECT_INFO_SERVER_URL);
	if (!url) {
		assert(false);
		return;
	}

	auto streamkey = obs_service_get_connect_info(service, OBS_SERVICE_CONNECT_INFO_STREAM_KEY);
	std::string key = streamkey ? streamkey : "";
	size_t hashkey = pls_generate_hash(key);

	auto dualoutput = pls_is_dual_output_on();
	auto platforms = pls_get_platforms(dualoutput, vertical);

	auto type = vertical ? "vertical channel" : "horizon channel";
	auto count = platforms.size();
	assert(count > 0);

	struct pls_platform_info info;
	memset(&info, 0, sizeof(struct pls_platform_info));
	info.filled = true;
	info.hash_streamkey = hashkey;
	snprintf(info.url, sizeof(info.url), "%s", url);
	pls_fill_platform_info(info, !!multitrackVideo, platforms);

	pls_set_platform_info(output, &info);

	PLS_LOG(PLS_LOG_INFO, MAIN_OUTPUT,
		"[%s] log stream detail \n"
		"\t output=%p \n"
		"\t dualoutput=%d \n"
		"\t %s \n"
		"\t multVideo=%d \n"
		"\t platform count=%zu \n"
		"\t url=[%s] \n"
		"\t streamkeyHash=[%zu]", // do not send real streamkey to CN Nelo
		func, output, dualoutput, type, !!multitrackVideo, count, url, hashkey);

	PLS_LOG_KR(PLS_LOG_INFO, MAIN_OUTPUT,
		   "[%s] log stream detail \n"
		   "\t output=%p \n"
		   "\t dualoutput=%d \n"
		   "\t %s \n"
		   "\t multVideo=%d \n"
		   "\t platform count=%zu \n"
		   "\t url=[%s] \n"
		   "\t streamkey=[%s]", // send real streamkey to KR Nelo
		   func, output, dualoutput, type, !!multitrackVideo, count, url, key.c_str());

	pls_log_stream_platform(platforms);
}

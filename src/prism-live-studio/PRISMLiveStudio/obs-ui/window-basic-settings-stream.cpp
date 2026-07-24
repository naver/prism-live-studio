#include <QMessageBox>
#include <QUrl>
#include <QUuid>
#include <qt-wrappers.hpp>

#include "window-basic-settings.hpp"
#include "obs-frontend-api.h"
#include "obs-app.hpp"
#include "window-basic-main.hpp"
#include "url-push-button.hpp"

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#endif

#include "auth-oauth.hpp"

#include "ui-config.h"

#ifdef YOUTUBE_ENABLED
#include "youtube-api-wrappers.hpp"
#endif
#include "PLSSyncServerManager.hpp"
#include "ChannelCommonFunctions.h"
#include "PLSPlatformApi.h"
#include "pls/pls-dual-output.h"

static const QUuid &CustomServerUUID()
{
	static const QUuid uuid = QUuid::fromString(QT_UTF8("{241da255-70f2-4bbb-bef7-509695bf8e65}"));
	return uuid;
}

struct QCef;
struct QCefCookieManager;

extern QCef *cef;
extern QCefCookieManager *panel_cookies;

enum class ListOpt : int {
	ShowAll = 1,
	Custom,
	WHIP,
};

enum class Section : int {
	Connect,
	StreamKey,
};

bool OBSBasicSettings::IsCustomService() const
{
	return outputPage->service->currentData().toInt() == (int)ListOpt::Custom;
}

inline bool OBSBasicSettings::IsWHIP() const
{
	return outputPage->service->currentData().toInt() == (int)ListOpt::WHIP;
}

void OBSBasicSettings::LoadStream1Settings()
{
	if (!outputPage) {
		return;
	}

	//PRISM/WuLongyue/20241104/PRISM_PC-1436
	bool ignoreRecommended = true;

	obs_service_t *service_obj = main->GetService();
	const char *type = obs_service_get_type(service_obj);
	bool is_rtmp_custom = (strcmp(type, "rtmp_custom") == 0);
	bool is_rtmp_common = (strcmp(type, "rtmp_common") == 0);
	bool is_whip = (strcmp(type, "whip_custom") == 0);

	loading = true;

	OBSDataAutoRelease settings = obs_service_get_settings(service_obj);

	const char *service = obs_data_get_string(settings, "service");
	const char *server = obs_data_get_string(settings, "server");

	QByteArray strServer;
	auto bAuto = false;
	auto activePlatforms = PLS_PLATFORM_ACTIVIED;
	if (1 == activePlatforms.size()) {
		if (auto pPlatform = activePlatforms.front();
		    pPlatform->getChannelType() >= ChannelData::ChannelDataType::CustomType &&
		    pPlatform->getChannelName() == TWITCH) {

			bAuto = PLSCHANNELS_API->getValueOfChannel<bool>(pPlatform->getChannelUUID(),
									 ChannelData::g_isTwitchRtmpServerAuto);

			strServer = PLSCHANNELS_API
					    ->getValueOfChannel<QString>(pPlatform->getChannelUUID(),
									 ChannelData::g_channelRtmpUrl)
					    .toUtf8();

			server = strServer;
		}
	}

	const char *key = obs_data_get_string(settings, "key");
	bool use_custom_server = obs_data_get_bool(settings, "using_custom_server");
	protocol = QT_UTF8(obs_service_get_protocol(service_obj));
	const char *bearer_token = obs_data_get_string(settings, "bearer_token");

	if (is_rtmp_custom || is_whip)
		outputPage->customServer->setText(server);

	bool hasTwitch = activePlatforms.size() == 1 && activePlatforms.front()->getPlatFormName() == TWITCH;
	int idx = 0;
	if (is_rtmp_custom) {
		if (hasTwitch) {
			idx = 1;
		}
		lastCustomServer = outputPage->customServer->text();
	} else {
		QString strService = service;
		if (hasTwitch) {
			idx = strService == "WHIP" ? 0 : 1;
		} else {
			idx = outputPage->service->findText(service);
			if (idx == -1 && outputPage->service->count() > 0) {
				idx = 0;
			}
		}
		if (idx >= outputPage->service->count()) {
			idx = 0;
		}
	}
	outputPage->service->setCurrentIndex(idx);
	lastServiceIdx = idx;

	outputPage->enableMultitrackVideo->setChecked(
		config_get_bool(main->Config(), "Stream1", "EnableMultitrackVideo"));

	outputPage->multitrackVideoMaximumAggregateBitrateAuto->setChecked(
		config_get_bool(main->Config(), "Stream1", "MultitrackVideoMaximumAggregateBitrateAuto"));
	if (config_has_user_value(main->Config(), "Stream1", "MultitrackVideoMaximumAggregateBitrate")) {
		outputPage->multitrackVideoMaximumAggregateBitrate->setValue(
			config_get_int(main->Config(), "Stream1", "MultitrackVideoMaximumAggregateBitrate"));
	} else {
		outputPage->multitrackVideoMaximumAggregateBitrate->setValue(0);
	}

	outputPage->multitrackVideoMaximumVideoTracksAuto->setChecked(
		config_get_bool(main->Config(), "Stream1", "MultitrackVideoMaximumVideoTracksAuto"));
	if (config_has_user_value(main->Config(), "Stream1", "MultitrackVideoMaximumVideoTracks"))
		outputPage->multitrackVideoMaximumVideoTracks->setValue(
			config_get_int(main->Config(), "Stream1", "MultitrackVideoMaximumVideoTracks"));
	else
		outputPage->multitrackVideoMaximumVideoTracks->setValue(0);

	outputPage->multitrackVideoStreamDumpEnable->setChecked(
		config_get_bool(main->Config(), "Stream1", "MultitrackVideoStreamDumpEnabled"));

	outputPage->multitrackVideoConfigOverrideEnable->setChecked(
		config_get_bool(main->Config(), "Stream1", "MultitrackVideoConfigOverrideEnabled"));
	if (config_has_user_value(main->Config(), "Stream1", "MultitrackVideoConfigOverride"))
		outputPage->multitrackVideoConfigOverride->setPlainText(
			DeserializeConfigText(
				config_get_string(main->Config(), "Stream1", "MultitrackVideoConfigOverride"))
				.c_str());
	else
		outputPage->multitrackVideoConfigOverride->setPlainText(QString());

	UpdateServerList();

	if (is_rtmp_common) {
		int idx = -1;
		if (use_custom_server) {
			idx = outputPage->server->findData(CustomServerUUID());
		} else {
			if (!bAuto) {
				auto strServer = QString::fromUtf8(server);
				for (int i = 0; i < outputPage->server->count(); ++i) {
					if (outputPage->server->itemData(i).toString() == strServer) {
						idx = i;

						if (outputPage->server->itemText(i) !=
						    QTStr("setting.output.server.auto")) {
							break;
						}
					}
				}
			} else {
				idx = outputPage->server->findText(QTStr("setting.output.server.auto"));
			}
		}

		if (idx == -1) {
			auto syncService = PLSBasic::instance()->getServiceName();
			if (server && *server && !syncService.startsWith("SOOP") && syncService == service) {
				outputPage->server->insertItem(0, server, server);
			}
			if (outputPage->server->count() > 0) {
				idx = 0;
			}
		}
		outputPage->server->setCurrentIndex(idx);
	}

	ServiceChanged(true);

	UpdateVodTrackSetting();
	UpdateMultitrackVideo();

	loading = false;
}

void OBSBasicSettings::SwapMultiTrack(const char *protocol)
{
	if (outputStreamPage) {
		if (PLS_PLATFORM_API->AllowsMultiTrack()) {
			outputStreamPage->advStreamTrackWidget->setCurrentWidget(outputStreamPage->streamMultiTracks);
		} else {
			outputStreamPage->advStreamTrackWidget->setCurrentWidget(outputStreamPage->streamSingleTracks);
		}
	}
}

void OBSBasicSettings::SaveStream1Settings()
{
	if (!outputPage) {
		return;
	}

	bool customServer = IsCustomService();
	bool whip = IsWHIP();
	const char *service_id = "rtmp_common";

	if (customServer) {
		service_id = "rtmp_custom";
	} else if (whip) {
		service_id = "whip_custom";
	}

	obs_service_t *oldService = main->GetService();
	OBSDataAutoRelease hotkeyData = obs_hotkeys_save_service(oldService);

	OBSDataAutoRelease settings = obs_data_create();

	if (!customServer && !whip) {
		obs_data_set_string(settings, "service", QT_TO_UTF8(PLSBasic::instance()->getServiceName()));
		obs_data_set_string(settings, "protocol", QT_TO_UTF8(protocol));
		if (outputPage->server->currentData() == CustomServerUUID()) {
			obs_data_set_bool(settings, "using_custom_server", true);

			obs_data_set_string(settings, "server", QT_TO_UTF8(outputPage->customServer->text()));
		} else {
			obs_data_set_string(settings, "server",
					    QT_TO_UTF8(outputPage->server->currentData().toString()));
		}
	} else {
		obs_data_set_string(settings, "server", QT_TO_UTF8(outputPage->customServer->text().trimmed()));
	}
	obs_data_set_bool(settings, "bwtest", false);

	if (whip) {
		obs_data_set_string(settings, "service", "WHIP");
		obs_data_set_string(settings, "bearer_token", "");
	} else {
		obs_data_set_string(settings, "key", "");
	}

	OBSServiceAutoRelease newService = obs_service_create(service_id, "default_service", settings, hotkeyData);

	if (!newService)
		return;

	main->SetService(newService);
	main->SaveService();
	main->auth = auth;
	if (!!main->auth) {
		main->auth->LoadUI();
		main->SetBroadcastFlowEnabled(main->auth->broadcastFlow());
	} else {
		main->SetBroadcastFlowEnabled(false);
	}

	config_set_bool(main->Config(), "Stream1", "IgnoreRecommended", true);

	auto oldMultitrackVideoSetting = config_get_bool(main->Config(), "Stream1", "EnableMultitrackVideo");

	//fix issue PRISM_PC-4501
	SaveCheckBox(outputPage->enableMultitrackVideo, "Stream1", "EnableMultitrackVideo");

	SaveCheckBox(outputPage->multitrackVideoMaximumAggregateBitrateAuto, "Stream1",
		     "MultitrackVideoMaximumAggregateBitrateAuto");
	SaveSpinBox(outputPage->multitrackVideoMaximumAggregateBitrate, "Stream1",
		    "MultitrackVideoMaximumAggregateBitrate");
	SaveCheckBox(outputPage->multitrackVideoMaximumVideoTracksAuto, "Stream1",
		     "MultitrackVideoMaximumVideoTracksAuto");
	SaveSpinBox(outputPage->multitrackVideoMaximumVideoTracks, "Stream1", "MultitrackVideoMaximumVideoTracks");
	SaveCheckBox(outputPage->multitrackVideoStreamDumpEnable, "Stream1", "MultitrackVideoStreamDumpEnabled");
	SaveCheckBox(outputPage->multitrackVideoConfigOverrideEnable, "Stream1",
		     "MultitrackVideoConfigOverrideEnabled");
	SaveText(outputPage->multitrackVideoConfigOverride, "Stream1", "MultitrackVideoConfigOverride");

	if (oldMultitrackVideoSetting != outputPage->enableMultitrackVideo->isChecked())
		main->ResetOutputs();

	SwapMultiTrack(QT_TO_UTF8(protocol));

	auto activiedPlatforms = PLS_PLATFORM_ACTIVIED;
	if (1 == activiedPlatforms.size()) {
		auto strServer = outputPage->server->currentData().toString();
		if (auto pPlatform = activiedPlatforms.front();
		    pPlatform->getChannelType() >= ChannelData::ChannelDataType::CustomType &&
		    pPlatform->getChannelName() == TWITCH && !strServer.isEmpty()) {
			auto uuid = pPlatform->getChannelUUID();
			auto &keyRtmpUrl = ChannelData::g_channelRtmpUrl;
			auto bAuto = outputPage->server->currentText() == QTStr("setting.output.server.auto");

			auto bServerChanged = PLSCHANNELS_API->getValueOfChannel<QString>(uuid, keyRtmpUrl) !=
					      strServer;
			auto bAutoChanged = PLSCHANNELS_API->getValueOfChannel<bool>(
						    uuid, ChannelData::g_isTwitchRtmpServerAuto) != bAuto;

			//Save to channel if the rtmp server is changed or auto server is changed
			if (bServerChanged || bAutoChanged) {
				PLSCHANNELS_API->setValueOfChannel(uuid, keyRtmpUrl, strServer);

				PLSCHANNELS_API->setValueOfChannel(uuid, ChannelData::g_isTwitchRtmpServerAuto, bAuto);

				PLSCHANNELS_API->backupInfo(uuid);
				PLSCHANNELS_API->sigTryToUpdateChannel(uuid);
			}
		}
	}
}

void OBSBasicSettings::LoadServices(bool showAll)
{
	serviceDualOutput = pls_is_dual_output_on();

	outputPage->service->blockSignals(true);
	outputPage->service->clear();

	QStringList names;

	bool bShowService = prepareStreamServiceData(names);
	outputPage->service->setEnabled(bShowService);
	outputPage->server->setEnabled(bShowService);

	if (showAll)
		names.sort(Qt::CaseInsensitive);

	if (!names.isEmpty() && names.first().toUpper() == "TWITCH") {
		if (obs_is_output_protocol_registered("WHIP")) {
			outputPage->service->addItem("Twitch - WHIP", QVariant((int)ListOpt::WHIP));
		}
		outputPage->service->addItem("Twitch - RTMPS");
	} else {
		for (QString &name : names)
			outputPage->service->addItem(name);
	}

	outputPage->formLayout_5->setRowVisible(outputPage->serverLabel, bShowService);

	if (!lastService.isEmpty()) {
		int idx = outputPage->service->findText(lastService);
		if (idx != -1)
			outputPage->service->setCurrentIndex(idx);
	}

	PLSBasic::instance()->setServiceName(outputPage->service->currentText());

	outputPage->service->blockSignals(false);
}

static inline bool is_auth_service(const std::string &service)
{
	return Auth::AuthType(service) != Auth::Type::None;
}

static inline bool is_external_oauth(const std::string &service)
{
	return Auth::External(service);
}

#ifdef YOUTUBE_ENABLED
static void get_yt_ch_title(Ui::OBSBasicSettings *ui)
{
	const char *name = config_get_string(OBSBasic::Get()->Config(), "YouTube", "ChannelName");
	if (name) {
		ui->connectedAccountText->setText(name);
	} else {
		// if we still not changed the service page
		if (IsYouTubeService(QT_TO_UTF8(ui->service->currentText()))) {
			ui->connectedAccountText->setText(QTStr("Auth.LoadingChannel.Error"));
		}
	}
}
#endif

void OBSBasicSettings::on_service_currentIndexChanged(int idx)
{
	PLSBasic::instance()->setServiceName(outputPage->service->currentText());

	ServiceChanged();

	UpdateServerList();

	UpdateVodTrackSetting();

	protocol = FindProtocol();
	UpdateAdvNetworkGroup();
	UpdateMultitrackVideo();

	if (ServiceSupportsCodecCheck()) {
		lastServiceIdx = idx;
		if (idx == 0)
			lastCustomServer = outputPage->customServer->text();
	}

	if (outputStreamPage) {
		if (!IsCustomService()) {
			outputStreamPage->advStreamTrackWidget->setCurrentWidget(outputStreamPage->streamSingleTracks);
		} else {
			SwapMultiTrack(QT_TO_UTF8(protocol));
		}
	}
}

void OBSBasicSettings::on_customServer_textChanged(const QString &)
{
	protocol = FindProtocol();
	UpdateAdvNetworkGroup();
	UpdateMultitrackVideo();

	if (ServiceSupportsCodecCheck())
		lastCustomServer = outputPage->customServer->text();

	SwapMultiTrack(QT_TO_UTF8(protocol));
}

void OBSBasicSettings::ServiceChanged(bool resetFields)
{
	std::string service = QT_TO_UTF8(outputPage->service->currentText());
	bool custom = IsCustomService();
	bool whip = IsWHIP();

	if (resetFields || lastService != service.c_str()) {
		outputPage->enableMultitrackVideo->setChecked(
			config_get_bool(main->Config(), "Stream1", "EnableMultitrackVideo"));
		UpdateMultitrackVideo();
	}

	if (custom || whip) {
		auto whipServer = PLSSyncServerManager::instance()->getTwitchWhipServer();
		outputPage->stackedWidget_3->setCurrentIndex(1);
		outputPage->stackedWidget_3->setVisible(true);
		outputPage->serverLabel->setVisible(true);
		outputPage->customServer->setText(whipServer);
		outputPage->customServer->setEnabled(false);
	} else {
		outputPage->stackedWidget_3->setCurrentIndex(0);
	}

	auth.reset();

	if (!main->auth) {
		return;
	}
}

QString OBSBasicSettings::FindProtocol()
{
	if (IsCustomService()) {
		if (outputPage->customServer->text().isEmpty())
			return QString("RTMP");

		QString server = outputPage->customServer->text();

		if (obs_is_output_protocol_registered("RTMPS") && server.startsWith("rtmps://"))
			return QString("RTMPS");

		if (server.startsWith("srt://"))
			return QString("SRT");

		if (server.startsWith("rist://"))
			return QString("RIST");

	} else {
		obs_properties_t *props = obs_get_service_properties("rtmp_common");
		obs_property_t *services = obs_properties_get(props, "service");

		OBSDataAutoRelease settings = obs_data_create();

		obs_data_set_string(settings, "service", QT_TO_UTF8(PLSBasic::instance()->getServiceName()));
		obs_property_modified(services, settings);

		obs_properties_destroy(props);

		const char *protocol = obs_data_get_string(settings, "protocol");
		if (protocol && *protocol)
			return QT_UTF8(protocol);
	}

	return QString("RTMP");
}

void OBSBasicSettings::UpdateServerList()
{
	QString serviceName = outputPage->service->currentText();

	lastService = serviceName;

	if (serviceName == "Twitch - RTMPS") {
		outputPage->server->clear();
		auto serverList = initTwitchServer();
		if (serverList.isEmpty()) {
			outputPage->server->addItem(QTStr("setting.output.server.auto"), "auto");
			return;
		}
		for (auto pair : serverList) {
			outputPage->server->addItem(pair.first, pair.second);
		}
		return;
	}

	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	obs_property_t *services = obs_properties_get(props, "service");

	OBSDataAutoRelease settings = obs_data_create();

	obs_data_set_string(settings, "service", QT_TO_UTF8(serviceName));
	obs_property_modified(services, settings);

	obs_property_t *servers = obs_properties_get(props, "server");

	outputPage->server->clear();

	size_t servers_count = obs_property_list_item_count(servers);
	for (size_t i = 0; i < servers_count; i++) {
		const char *name = obs_property_list_item_name(servers, i);
		const char *server = obs_property_list_item_string(servers, i);
		if (strcmp(name, "") != 0 && strcmp(server, "") != 0) {
			outputPage->server->addItem(name, server);
		}
	}
	if (serviceName == "Twitch" || serviceName == "Amazon IVS") {
		outputPage->server->addItem(QTStr("Basic.Settings.Stream.SpecifyCustomServer"), CustomServerUUID());
	}

	QString text = QTStr("setting.output.server.auto");
	if (serviceName != "YouTube - HLS" && serviceName != "Facebook Live" &&
	    text != outputPage->server->itemText(0)) {
		outputPage->server->insertItem(0, text, "auto");
	}
	if (IsWHIP()) {
		outputPage->server->setCurrentIndex(0);
	}

	obs_properties_destroy(props);
}

OBSService OBSBasicSettings::SpawnTempService()
{
	bool custom = IsCustomService();
	bool whip = IsWHIP();
	const char *service_id = "rtmp_common";

	if (custom) {
		service_id = "rtmp_custom";
	} else if (whip) {
		service_id = "whip_custom";
	}

	OBSDataAutoRelease settings = obs_data_create();

	if (!custom && !whip) {
		QString strService = outputPage->service->currentText();
		if (strService == "Twitch - RTMPS") {
			strService = "Twitch";
		}
		obs_data_set_string(settings, "service", QT_TO_UTF8(strService));
		obs_data_set_string(settings, "server", QT_TO_UTF8(outputPage->server->currentData().toString()));
	} else {
		obs_data_set_string(settings, "server", QT_TO_UTF8(outputPage->customServer->text().trimmed()));
	}

	if (whip)
		obs_data_set_string(settings, "bearer_token", "");
	else
		obs_data_set_string(settings, "key", "");

	OBSServiceAutoRelease newService = obs_service_create(service_id, "temp_service", settings, nullptr);
	return newService.Get();
}

void OBSBasicSettings::UpdateVodTrackSetting()
{
	bool enableForCustomServer = config_get_bool(App()->GetUserConfig(), "General", "EnableCustomServerVodTrack");
	bool enableVodTrack = outputPage->service->currentText() == "Twitch - RTMPS";
	bool wasEnabled = !!vodTrackCheckbox;
	bool wasSimpleEnabled = !!simpleVodTrack;

	if (enableForCustomServer && IsCustomService())
		enableVodTrack = true;

	if (enableVodTrack == wasEnabled && enableVodTrack == wasSimpleEnabled)
		return;

	if (!enableVodTrack) {
		delete container;
		delete vodTrackContainer;
		delete simpleVodTrack;
		return;
	}

	if (outputSimplePage && !wasSimpleEnabled) {
		bool simpleAdv = outputSimplePage->simpleOutAdvanced->isChecked();
		bool vodTrackEnabled = config_get_bool(main->Config(), "SimpleOutput", "VodTrackEnabled");

		simpleVodTrack = new PLSCheckBox(this);
		simpleVodTrack->setText(QTStr("Basic.Settings.Output.Simple.TwitchVodTrack"));
		simpleVodTrack->setVisible(simpleAdv);
		simpleVodTrack->setChecked(vodTrackEnabled);

		int pos;
		outputSimplePage->simpleStreamingLayout->getWidgetPosition(outputSimplePage->simpleOutAdvanced, &pos,
									   nullptr);
		outputSimplePage->simpleStreamingLayout->insertRow(pos + 1, nullptr, simpleVodTrack);

		HookWidget(simpleVodTrack.data(), &PLSCheckBox::clicked, &OBSBasicSettings::OutputsChanged);
		connect(outputSimplePage->simpleOutAdvanced, &PLSCheckBox::toggled, simpleVodTrack.data(),
			&QWidget::setVisible);
	}

	if (outputStreamPage && !wasEnabled) {
		QHBoxLayout *vodTrackCheckboxLayout = new QHBoxLayout();
		QLabel *vodTrackCheckboxLable = new QLabel(this);
		vodTrackCheckboxLable->setText(QTStr("Basic.Settings.Output.Adv.TwitchVodTrack"));
		vodTrackCheckboxLable->setProperty("useFor", "FormLabelRole");
		vodTrackCheckbox = new PLSCheckBox(this);
		vodTrackCheckbox->setText(QTStr(""));
		vodTrackCheckbox->setLayoutDirection(Qt::LeftToRight);
		vodTrackCheckboxLayout->addWidget(vodTrackCheckboxLable);
		vodTrackCheckboxLayout->addWidget(vodTrackCheckbox);
		vodTrackCheckboxLayout->setSpacing(10);
		vodTrackCheckboxLayout->setContentsMargins(0, 0, 0, 0);

		container = new QWidget(this);
		container->setLayout(vodTrackCheckboxLayout);

		vodTrackContainer = new QWidget(this);
		QHBoxLayout *vodTrackLayout = new QHBoxLayout();
		for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
			vodTrack[i] = new PLSRadioButton(QString::number(i + 1));
			vodTrackLayout->addWidget(vodTrack[i]);

			HookWidget(vodTrack[i].data(), &PLSRadioButton::clicked, &OBSBasicSettings::OutputsChanged);
		}

		HookWidget(vodTrackCheckbox.data(), &PLSCheckBox::clicked, &OBSBasicSettings::OutputsChanged);

		vodTrackLayout->addStretch();
		vodTrackLayout->setContentsMargins(0, 0, 0, 0);
		vodTrackLayout->setSpacing(10);
		vodTrackContainer->setLayout(vodTrackLayout);

		outputStreamPage->advOutTopLayout->insertRow(2, container, vodTrackContainer);

		auto vodTrackEnabled = config_get_bool(main->Config(), "AdvOut", "VodTrackEnabled");
		vodTrackCheckbox->setChecked(vodTrackEnabled);
		vodTrackContainer->setEnabled(vodTrackEnabled);

		connect(vodTrackCheckbox, &PLSCheckBox::clicked, vodTrackContainer, &QWidget::setEnabled);

		int trackIndex = config_get_int(main->Config(), "AdvOut", "VodTrackIndex");
		for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
			vodTrack[i]->setChecked((i + 1) == trackIndex);
		}
	}
}

OBSService OBSBasicSettings::GetStream1Service()
{
	return stream1Changed ? SpawnTempService() : OBSService(main->GetService());
}

static bool service_supports_codec_copy(const char **codecs, const char *codec)
{
	if (!codecs)
		return true;

	while (*codecs) {
		if (strcmp(*codecs, codec) == 0)
			return true;
		codecs++;
	}

	return false;
}

extern bool EncoderAvailable(const char *encoder);
extern const char *get_simple_output_encoder(const char *name);

static inline bool service_supports_encoder(const char **codecs, const char *encoder)
{
	if (!EncoderAvailable(encoder))
		return false;

	return true;
}

static bool return_first_id(void *data, const char *id)
{
	const char **output = (const char **)data;

	*output = id;
	return false;
}

/* we really need a way to find fallbacks in a less hardcoded way. maybe. */
static QString get_adv_fallback(const QString &enc)
{
	if (enc == "obs_nvenc_hevc_tex" || enc == "obs_nvenc_av1_tex" || enc == "jim_hevc_nvenc" ||
	    enc == "jim_av1_nvenc")
		return "obs_nvenc_h264_tex";
	if (enc == "h265_texture_amf" || enc == "av1_texture_amf")
		return "h264_texture_amf";
	if (enc == "com.apple.videotoolbox.videoencoder.ave.hevc")
		return "com.apple.videotoolbox.videoencoder.ave.avc";
	if (enc == "obs_qsv11_av1")
		return "obs_qsv11";
	return "obs_x264";
}

static QString get_adv_audio_fallback(const QString &enc)
{
	const char *codec = obs_get_encoder_codec(QT_TO_UTF8(enc));

	if (codec && strcmp(codec, "aac") == 0)
		return "ffmpeg_opus";

	QString aac_default = "ffmpeg_aac";
	if (EncoderAvailable("CoreAudio_AAC"))
		aac_default = "CoreAudio_AAC";
	else if (EncoderAvailable("libfdk_aac"))
		aac_default = "libfdk_aac";

	return aac_default;
}

static QString get_simple_fallback(const QString &enc)
{
	if (enc == SIMPLE_ENCODER_NVENC_HEVC || enc == SIMPLE_ENCODER_NVENC_AV1)
		return SIMPLE_ENCODER_NVENC;
	if (enc == SIMPLE_ENCODER_AMD_HEVC || enc == SIMPLE_ENCODER_AMD_AV1)
		return SIMPLE_ENCODER_AMD;
	if (enc == SIMPLE_ENCODER_APPLE_HEVC)
		return SIMPLE_ENCODER_APPLE_H264;
	if (enc == SIMPLE_ENCODER_QSV_AV1)
		return SIMPLE_ENCODER_QSV;
	return SIMPLE_ENCODER_X264;
}

bool OBSBasicSettings::ServiceSupportsCodecCheck()
{
	if (loading)
		return false;

	if (lastServiceIdx != outputPage->service->currentIndex() || IsCustomService()) {
		ResetSimpleEncoders();
		ResetStreamEncoders();
	}

	return true;
}

#define TEXT_USE_STREAM_ENC QTStr("Basic.Settings.Output.Adv.Recording.UseStreamEncoder")

void OBSBasicSettings::ResetSimpleEncoders()
{
	if (!outputSimplePage) {
		return;
	}

	QString lastVideoEnc = outputSimplePage->simpleOutStrEncoder->currentData().toString();
	QString lastAudioEnc = outputSimplePage->simpleOutStrAEncoder->currentData().toString();

	OBSService service = SpawnTempService();
	const char **vcodecs = obs_service_get_supported_video_codecs(service);
	const char **acodecs = obs_service_get_supported_audio_codecs(service);
	const char *type;
	BPtr<char *> output_vcodecs;
	BPtr<char *> output_acodecs;
	size_t idx = 0;

	if (!vcodecs || IsCustomService()) {
		const char *output = nullptr;

		obs_enum_output_types_with_protocol(QT_TO_UTF8(protocol), &output, return_first_id);

		if (nullptr == output) {
			PLS_ERROR(MAIN_OUTPUT, "output is null, protocol is %s", qUtf8Printable(protocol));

			obs_enum_output_types(0, &output);
			if (nullptr == output) {
				PLS_ERROR(MAIN_OUTPUT, "output 0 is still null");

				output = "rtmp_output";
			}
		}

		output_vcodecs = strlist_split(obs_get_output_supported_video_codecs(output), ';', false);
		vcodecs = (const char **)output_vcodecs.Get();
	}

	if (!acodecs || IsCustomService()) {
		const char *output;

		obs_enum_output_types_with_protocol(QT_TO_UTF8(protocol), &output, return_first_id);
		output_acodecs = strlist_split(obs_get_output_supported_audio_codecs(output), ';', false);
		acodecs = (const char **)output_acodecs.Get();
	}

	QSignalBlocker s1(outputSimplePage->simpleOutStrEncoder);
	QSignalBlocker s3(outputSimplePage->simpleOutStrAEncoder);

	outputSimplePage->simpleOutStrEncoder->clear();
	outputSimplePage->simpleOutStrAEncoder->clear();

#define ENCODER_STR(str) QTStr("Basic.Settings.Output.Simple.Encoder." str)

	outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Software"), QString(SIMPLE_ENCODER_X264));
#ifdef _WIN32
	if (service_supports_encoder(vcodecs, "obs_qsv11"))
		outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.QSV.H264"),
							       QString(SIMPLE_ENCODER_QSV));
	if (service_supports_encoder(vcodecs, "obs_qsv11_av1"))
		outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.QSV.AV1"),
							       QString(SIMPLE_ENCODER_QSV_AV1));
#endif
	if (service_supports_encoder(vcodecs, "ffmpeg_nvenc"))
		outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.NVENC.H264"),
							       QString(SIMPLE_ENCODER_NVENC));
	if (service_supports_encoder(vcodecs, "obs_nvenc_av1_tex"))
		outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.NVENC.AV1"),
							       QString(SIMPLE_ENCODER_NVENC_AV1));
#ifdef ENABLE_HEVC
	if (service_supports_encoder(vcodecs, "h265_texture_amf"))
		outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.AMD.HEVC"),
							       QString(SIMPLE_ENCODER_AMD_HEVC));
	if (service_supports_encoder(vcodecs, "ffmpeg_hevc_nvenc"))
		outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.NVENC.HEVC"),
							       QString(SIMPLE_ENCODER_NVENC_HEVC));
#endif
	if (service_supports_encoder(vcodecs, "h264_texture_amf"))
		outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.AMD.H264"),
							       QString(SIMPLE_ENCODER_AMD));
	if (service_supports_encoder(vcodecs, "av1_texture_amf"))
		outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.AMD.AV1"),
							       QString(SIMPLE_ENCODER_AMD_AV1));

#ifdef __APPLE__
	if (service_supports_encoder(vcodecs, "com.apple.videotoolbox.videoencoder.ave.avc")
#ifndef __aarch64__
	    && os_get_emulation_status() == true
#endif
	) {
		if (__builtin_available(macOS 13.0, *)) {
			outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.Apple.H264"),
								       QString(SIMPLE_ENCODER_APPLE_H264));
		}
	}
#ifdef ENABLE_HEVC
	if (service_supports_encoder(vcodecs, "com.apple.videotoolbox.videoencoder.ave.hevc")
#ifndef __aarch64__
	    && os_get_emulation_status() == true
#endif
	) {
		if (__builtin_available(macOS 13.0, *)) {
			outputSimplePage->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.Apple.HEVC"),
								       QString(SIMPLE_ENCODER_APPLE_HEVC));
		}
	}
#endif
#endif

	if (!IsWHIP()) {
		if (service_supports_encoder(acodecs, "CoreAudio_AAC") ||
		    service_supports_encoder(acodecs, "libfdk_aac") ||
		    service_supports_encoder(acodecs, "ffmpeg_aac")) {
			outputSimplePage->simpleOutStrAEncoder->addItem(
				QTStr("Basic.Settings.Output.Simple.Codec.AAC.Default"), "aac");
		}
	} else {
		if (service_supports_encoder(acodecs, "ffmpeg_opus")) {
			outputSimplePage->simpleOutStrAEncoder->addItem(
				QTStr("Basic.Settings.Output.Simple.Codec.Opus"), "opus");
		}
	}
	outputSimplePage->simpleOutStrAEncoder->setCurrentIndex(0);
#undef ENCODER_STR

	if (!lastVideoEnc.isEmpty()) {
		int idx = outputSimplePage->simpleOutStrEncoder->findData(lastVideoEnc);
		if (idx == -1) {
			lastVideoEnc = get_simple_fallback(lastVideoEnc);
			outputSimplePage->simpleOutStrEncoder->setProperty("changed", QVariant(true));
			OutputsChanged();
		}

		idx = outputSimplePage->simpleOutStrEncoder->findData(lastVideoEnc);
		s1.unblock();
		outputSimplePage->simpleOutStrEncoder->setCurrentIndex(idx);
	}

	if (!lastAudioEnc.isEmpty()) {
		int idx = outputSimplePage->simpleOutStrAEncoder->findData(lastAudioEnc);
		if (idx == -1) {
			lastAudioEnc = (lastAudioEnc == "opus") ? "aac" : "opus";
			outputSimplePage->simpleOutStrAEncoder->setProperty("changed", QVariant(true));
			OutputsChanged();
		}

		idx = outputSimplePage->simpleOutStrAEncoder->findData(lastAudioEnc);
		s3.unblock();
		outputSimplePage->simpleOutStrAEncoder->setCurrentIndex(idx);
	}
}

void OBSBasicSettings::ResetStreamEncoders()
{
	if (!outputStreamPage) {
		return;
	}

	QString lastAdvVideoEnc = outputStreamPage->advOutEncoder->currentData().toString();
	QString lastAdvAudioEnc = outputStreamPage->advOutAEncoder->currentData().toString();

	OBSService service = SpawnTempService();
	const char **vcodecs = obs_service_get_supported_video_codecs(service);
	const char **acodecs = obs_service_get_supported_audio_codecs(service);
	const char *type;
	BPtr<char *> output_vcodecs;
	BPtr<char *> output_acodecs;
	size_t idx = 0;

	if (!vcodecs || IsCustomService()) {
		const char *output = nullptr;

		obs_enum_output_types_with_protocol(QT_TO_UTF8(protocol), &output, return_first_id);

		if (nullptr == output) {
			PLS_ERROR(MAIN_OUTPUT, "output is null, protocol is %s", qUtf8Printable(protocol));

			obs_enum_output_types(0, &output);
			if (nullptr == output) {
				PLS_ERROR(MAIN_OUTPUT, "output 0 is still null");

				output = "rtmp_output";
			}
		}

		output_vcodecs = strlist_split(obs_get_output_supported_video_codecs(output), ';', false);
		vcodecs = (const char **)output_vcodecs.Get();
	}

	if (!acodecs || IsCustomService()) {
		const char *output;

		obs_enum_output_types_with_protocol(QT_TO_UTF8(protocol), &output, return_first_id);
		output_acodecs = strlist_split(obs_get_output_supported_audio_codecs(output), ';', false);
		acodecs = (const char **)output_acodecs.Get();
	}

	QSignalBlocker s2(outputStreamPage->advOutEncoder);
	QSignalBlocker s4(outputStreamPage->advOutAEncoder);

	outputStreamPage->advOutEncoder->clear();
	outputStreamPage->advOutAEncoder->clear();

	while (obs_enum_encoder_types(idx++, &type)) {
		const char *name = obs_encoder_get_display_name(type);
		const char *codec = obs_get_encoder_codec(type);
		uint32_t caps = obs_get_encoder_caps(type);

		QString qName = QT_UTF8(name);
		QString qType = QT_UTF8(type);

		if (obs_get_encoder_type(type) == OBS_ENCODER_VIDEO) {
			if ((caps & ENCODER_HIDE_FLAGS) != 0)
				continue;

			outputStreamPage->advOutEncoder->addItem(qName, qType);
		}

		if (obs_get_encoder_type(type) == OBS_ENCODER_AUDIO) {
			if (service_supports_codec_copy(acodecs, codec))
				outputStreamPage->advOutAEncoder->addItem(qName, qType);
		}
	}

	outputStreamPage->advOutEncoder->model()->sort(0);
	outputStreamPage->advOutAEncoder->model()->sort(0);

	if (!lastAdvVideoEnc.isEmpty()) {
		int idx = outputStreamPage->advOutEncoder->findData(lastAdvVideoEnc);
		if (idx == -1) {
			lastAdvVideoEnc = get_adv_fallback(lastAdvVideoEnc);
			outputStreamPage->advOutEncoder->setProperty("changed", QVariant(true));
			OutputsChanged();
		}

		idx = outputStreamPage->advOutEncoder->findData(lastAdvVideoEnc);
		s2.unblock();
		outputStreamPage->advOutEncoder->setCurrentIndex(idx);
	}

	if (!lastAdvAudioEnc.isEmpty()) {
		int idx = outputStreamPage->advOutAEncoder->findData(lastAdvAudioEnc);
		if (idx == -1) {
			lastAdvAudioEnc = get_adv_audio_fallback(lastAdvAudioEnc);
			outputStreamPage->advOutAEncoder->setProperty("changed", QVariant(true));
			OutputsChanged();
		}

		idx = outputStreamPage->advOutAEncoder->findData(lastAdvAudioEnc);
		s4.unblock();
		outputStreamPage->advOutAEncoder->setCurrentIndex(idx);
	}
}

void OBSBasicSettings::ResetRecordEncoders()
{
	if (!outputRecordPage) {
		return;
	}

	OBSService service = SpawnTempService();
	const char **vcodecs = obs_service_get_supported_video_codecs(service);
	const char **acodecs = obs_service_get_supported_audio_codecs(service);
	const char *type;
	BPtr<char *> output_vcodecs;
	BPtr<char *> output_acodecs;
	size_t idx = 0;

	if (!vcodecs || IsCustomService()) {
		const char *output = nullptr;

		obs_enum_output_types_with_protocol(QT_TO_UTF8(protocol), &output, return_first_id);

		if (nullptr == output) {
			PLS_ERROR(MAIN_OUTPUT, "output is null, protocol is %s", qUtf8Printable(protocol));

			obs_enum_output_types(0, &output);
			if (nullptr == output) {
				PLS_ERROR(MAIN_OUTPUT, "output 0 is still null");

				output = "rtmp_output";
			}
		}

		output_vcodecs = strlist_split(obs_get_output_supported_video_codecs(output), ';', false);
		vcodecs = (const char **)output_vcodecs.Get();
	}

	if (!acodecs || IsCustomService()) {
		const char *output;

		obs_enum_output_types_with_protocol(QT_TO_UTF8(protocol), &output, return_first_id);
		output_acodecs = strlist_split(obs_get_output_supported_audio_codecs(output), ';', false);
		acodecs = (const char **)output_acodecs.Get();
	}

	outputRecordPage->advOutRecEncoder->clear();
	outputRecordPage->advOutRecAEncoder->clear();

	while (obs_enum_encoder_types(idx++, &type)) {
		const char *name = obs_encoder_get_display_name(type);
		const char *codec = obs_get_encoder_codec(type);
		uint32_t caps = obs_get_encoder_caps(type);

		QString qName = QT_UTF8(name);
		QString qType = QT_UTF8(type);

		if (obs_get_encoder_type(type) == OBS_ENCODER_VIDEO) {
			if ((caps & ENCODER_HIDE_FLAGS) != 0)
				continue;

			outputRecordPage->advOutRecEncoder->addItem(qName, qType);
		}

		if (obs_get_encoder_type(type) == OBS_ENCODER_AUDIO) {
			outputRecordPage->advOutRecAEncoder->addItem(qName, qType);
		}
	}

	outputRecordPage->advOutRecEncoder->model()->sort(0);
	outputRecordPage->advOutRecEncoder->insertItem(0, TEXT_USE_STREAM_ENC, "none");
	outputRecordPage->advOutRecAEncoder->model()->sort(0);
	outputRecordPage->advOutRecAEncoder->insertItem(0, TEXT_USE_STREAM_ENC, "none");

	if (int index = outputRecordPage->advOutRecAEncoder->currentIndex(); -1 == index) {
		outputRecordPage->advOutRecAEncoder->setCurrentIndex(0);
	}
}

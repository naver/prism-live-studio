#include "PLSBasic.h"
#include "pls-common-define.hpp"
#include "pls-common-language.hpp"
#include <util/profiler.hpp>
#include "frontend-internal.hpp"
#include <qmenu.h>
#include <qmenubar.h>
#include <QMetaEnum>
#include <libutils-api.h>
#include <PLSWatchers.h>
#include <liblog.h>
#include <log/module_names.h>
#include <pls-shared-functions.h>
#include <pls/pls-source.h>
#include <pls/pls-obs-api.h>
#include "obs-app.hpp"
#include "pls/pls-dual-output.h"
#include "qt-wrappers.hpp"
#include <QDesktopServices>
#include "PLSNewIconActionWidget.hpp"
#include "PLSAboutView.hpp"
#include "PLSNoticeView.hpp"
#include "PLSDrawPen/PLSDrawPenMgr.h"
#include "PLSBackgroundMusicView.h"
#include "PLSPlatformPrism.h"
#include "prism-version.h"
#include "PLSChannelsEntrance.h"
#include "PLSLaunchWizardView.h"
#include "PLSMessageBox.h"
#include "ui-validation.hpp"
#include "PLSPlatformApi.h"
#include "pls-shared-values.h"
#include "window-basic-settings.hpp"
#include "login-user-info.hpp"
#include "PLSApp.h"
#include "PLSChatHelper.h"
#include "PLSPrismShareMemory.h"
#include "pls/pls-obs-api.h"
#include "PLSVirtualBgManager.h"
#include "PLSMotionItemView.h"
#include "PLSLiveInfoNaverShoppingLIVEProductItemView.h"
#include "PLSNaverShoppingLIVEProductItemView.h"
#include "PLSNaverShoppingLIVESearchKeywordItemView.h"
#include "PLSAction.h"
#include "platform.hpp"
#include "PLSBasicStatusBar.hpp"
#include "PLSAudioControl.h"
#include "PLSLaboratory.h"
#include "PLSLaboratoryManage.h"
#include "RemoteChat/PLSRemoteChatView.h"
#include "PLSChannelDataAPI.h"
#include "window-basic-main-outputs.hpp"
#include "PLSContactView.hpp"
#include "PLSOpenSourceView.h"

#include "PLSLoginDataHandler.h"
#include "window-dock-browser.hpp"
#include "PLSPreviewTitle.h"
#include "pls-frontend-api.h"
#include "GiphyDownloader.h"
#include "PLSInfoCollector.h"
#include "log/log.h"
#include <QStandardPaths>
#include "PLSSyncServerManager.hpp"
#include "util/windows/win-version.h"
#include "GoLivePannel.h"
#include "PLSChannelSupportVideEncoder.h"
#include "PLSSceneDataMgr.h"
#include "PLSNCB2bBrowserSettings.h"
#include "MutiLanguageTestView.h"
#include "PLSSceneitemMapManager.h"

#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

#define PRISM_STICKER_CENTER_SHOW 1

using namespace common;
extern void printTotalStartTime();
extern PLSLaboratory *g_laboratoryDialog;
extern PLSRemoteChatView *g_remoteChatDialog;
struct LocalVars {
	static PLSBasic *s_basic;
};
constexpr auto CONFIG_BASIC_WINDOW_MODULE = "BasicWindow";
constexpr auto CONFIG_PREVIEW_MODE_MODULE = "PreviewProgramMode";
constexpr auto PRISM_CAM_PRODUCTION_NAME = "PRISMLens";

PLSBasic *LocalVars::s_basic = nullptr;
extern volatile long insideEventLoop;
extern QCef *cef;
extern void loadPixmap(QPixmap &pix, const QString &pixmapPath, const QSize &pixSize);
const char *get_simple_output_encoder(const char *encoder);

#define HEADER_USER_AGENT_KEY QStringLiteral("User-Agent")
#define HEADER_PRISM_LANGUAGE QStringLiteral("Accept-Language")
#define HEADER_PRISM_GCC QStringLiteral("X-prism-cc")
#define HEADER_PRISM_USERCODE QStringLiteral("X-prism-usercode")
#define HEADER_PRISM_OS QStringLiteral("X-prism-os")
#define HEADER_PRISM_IP QStringLiteral("X-prism-ip")
#define HEADER_PRISM_DEVICE QStringLiteral("X-prism-device")
#define HEADER_PRISM_APPVERSION QStringLiteral("X-prism-appversion")

#if defined(Q_OS_WIN)
class HookNativeEvent {
	Q_DISABLE_COPY(HookNativeEvent)

public:
	HookNativeEvent() { m_mouseHook.reset(SetWindowsHookExW(WH_MOUSE, &mouseHookProc, GetModuleHandleW(nullptr), GetCurrentThreadId())); }

protected:
	static LRESULT CALLBACK mouseHookProc(_In_ int code, _In_ WPARAM wParam, _In_ LPARAM lParam)
	{
#if 0
		if (wParam == WM_LBUTTONDOWN) {
			QWidget *widget = QApplication::widgetAt(QCursor::pos());
			if (widget) {
				for (QWidget* p = widget; p; p = p->parentWidget()) {
					qDebug() << p->metaObject()->className() << p->objectName();
				}
			}
		}
#endif

		if ((code < 0) || !LocalVars::s_basic) {
			return CallNextHookEx(nullptr, code, wParam, lParam);
		}

		if ((wParam == WM_LBUTTONDOWN) || (wParam == WM_LBUTTONUP) || (wParam == WM_LBUTTONDBLCLK) || (wParam == WM_RBUTTONDOWN) || (wParam == WM_RBUTTONUP) || (wParam == WM_RBUTTONDBLCLK) ||
		    (wParam == WM_MBUTTONDOWN) || (wParam == WM_MBUTTONUP) || (wParam == WM_MBUTTONDBLCLK) || (wParam == WM_NCLBUTTONDOWN) || (wParam == WM_NCLBUTTONUP) ||
		    (wParam == WM_NCLBUTTONDBLCLK) || (wParam == WM_NCRBUTTONDOWN) || (wParam == WM_NCRBUTTONUP) || (wParam == WM_NCRBUTTONDBLCLK) || (wParam == WM_NCMBUTTONDOWN) ||
		    (wParam == WM_NCMBUTTONUP) || (wParam == WM_NCMBUTTONDBLCLK)) {
			auto mhs = (LPMOUSEHOOKSTRUCT)lParam;
			if (LocalVars::s_basic->m_mainMenuShow && !isInButton(mhs->pt)) {
				LocalVars::s_basic->m_mainMenuShow = false;
			}
		}

		return CallNextHookEx(nullptr, code, wParam, lParam);
	}

	static bool isInButton(const POINT &pt)
	{
		auto menuButton = LocalVars::s_basic->getMainView()->menuButton();
		auto ratio = menuButton->devicePixelRatioF();
		QRect rc(menuButton->mapToGlobal(QPoint(0, 0)) * ratio, menuButton->size() * ratio);
		return rc.contains(pt.x, pt.y);
	}

private:
	struct HHOOKDeleter {
		void operator()(HHOOK &hhook) const { pls_delete(hhook, UnhookWindowsHookEx, nullptr); }
	};
	PLSAutoHandle<HHOOK, HHOOKDeleter> m_mouseHook;
};

void installNativeEventFilter()
{
	static std::unique_ptr<HookNativeEvent> hookNativeEvent;
	if (!hookNativeEvent) {
		hookNativeEvent.reset(pls_new<HookNativeEvent>());
	}
}
#endif

QStringList getActivedChatChannels()
{
	auto activedPlatforms = PLS_PLATFORM_ACTIVIED;
	activedPlatforms.sort([](const PLSPlatformBase *a, const PLSPlatformBase *b) { return a->getChannelOrder() < b->getChannelOrder(); });

	QStringList activedChatChannels;
	for (PLSPlatformBase *platform : PLS_PLATFORM_ACTIVIED) {
		auto index = PLS_CHAT_HELPER->getIndexFromInfo(platform->getInitData());
		if (!PLS_CHAT_HELPER->isCefWidgetIndex(index)) {
			continue;
		} else {
			QString channelName = platform->getPlatFormName();
			if (channelName == NAVER_SHOPPING_LIVE) {
				channelName = QTStr("navershopping.liveinfo.title");
			}
			activedChatChannels.append(channelName);
		}
	}
	return activedChatChannels;
}

int getActivedChatChannelCount()
{
	return static_cast<int>(getActivedChatChannels().length());
}

bool hasActivedChatChannel()
{
	return getActivedChatChannelCount() > 0;
}

bool hasChatSource()
{
	bool found = false;
	obs_enum_sources(
		[](void *param, obs_source_t *source) {
			pls_unused(source);
			auto pfound = (bool *)param;
			const char *id = obs_source_get_id(source);
			if (pls_is_equal(id, PRISM_CHAT_SOURCE_ID) || pls_is_equal(id, PRISM_CHATV2_SOURCE_ID)) {
				*pfound = true;
				return false;
			}
			return true;
		},
		&found);

	return found;
}

static PlatformType getPlatformType(QString &platformName, const QVariantMap &info)
{
	if (auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt(); dataType >= ChannelData::RTMPType) {
		platformName = QStringLiteral("Custom RTMP");
		return PlatformType::CustomRTMP;
	} else if (dataType == ChannelData::ChannelType) {
		static std::map<QString, PlatformType> s_specialType({{BAND, PlatformType::Band}});
		platformName = info.value(ChannelData::g_fixPlatformName, "").toString();
		if (auto it = s_specialType.find(platformName); it != s_specialType.end()) {
			return it->second;
		}
	}
	return PlatformType::Others;
}
PlatformType viewerCountCheckPlatform()
{
	auto activedPlatforms = PLS_PLATFORM_ACTIVIED;
	if (activedPlatforms.empty()) {
		return PlatformType::Empty;
	} else if (activedPlatforms.size() == 1) {
		QString platformName;
		return getPlatformType(platformName, activedPlatforms.front()->getInitData());
	}

	bool hasBand = false;
	bool hasCustomRTMP = false;
	bool hasOthers = false;
	for (auto platform : activedPlatforms) {
		QString platformName;
		if (auto type = getPlatformType(platformName, platform->getInitData()); type == PlatformType::Band) {
			hasBand = true;
		} else if (type == PlatformType::CustomRTMP) {
			hasCustomRTMP = true;
		} else {
			hasOthers = true;
		}
	}

	if (!hasOthers) {
		if (hasBand && hasCustomRTMP) {
			return PlatformType::BandAndCustomRTMP;
		}
		return PlatformType::CustomRTMP;
	} else if (hasBand && hasCustomRTMP) {
		return PlatformType::MpBandAndCustomRTMP;
	} else if (hasBand) {
		return PlatformType::MpBand;
	} else if (hasCustomRTMP) {
		return PlatformType::MpCustomRTMP;
	}
	return PlatformType::MpOthers;
}
QByteArray buildPlatformViewerCount()
{
	bool hasBandOrRTMP = false;
	pls::JsonDocument<QJsonArray> platforms;

	auto maps = PLSCHANNELS_API->getCurrentSelectedChannels();
	for (const auto &info : maps) {
		if (info.empty()) {
			continue;
		}

		QString platformName;
		if (getPlatformType(platformName, info) == PlatformType::Others) {
			auto obj = pls::JsonDocument<QJsonObject>()                                         //
					   .add("platform", platformName)                                   //
					   .add("name", info.value(ChannelData::g_displayLine1).toString()) //
					   .add("viewerCount", info.value(ChannelData::g_viewers).toInt());
			if (platformName == NCB2B) {
				obj.add("icon", PLSLoginDataHandler::instance()->getNCB2BServiceConnfigRes()["serviceLogoWithBackgroundPath"].toString());
			}

			platforms.add(obj.object());
		} else {
			hasBandOrRTMP = true;
		}
	}

	auto platformCount = platforms.array().count();
	return pls::JsonDocument<QJsonObject>()
		.add("type", "viewerCount")                   //
		.add("data", pls::JsonDocument<QJsonObject>() //
				     .add("visiable", (platformCount > 0) || !hasBandOrRTMP)
				     .add("platforms", platforms.array())
				     .object()) //
		.toByteArray();
}
void updatePlatformViewerCount()
{
	pls_async_call_mt(PLS_PLATFORM_API, []() {
		pls_check_app_exiting();
		// notify viewer count source
		QByteArray viewerCountJson = buildPlatformViewerCount();
		PLS_INFO_KR("ViewCount-Source", "viewerCountEvent: %s", viewerCountJson.constData());

		std::vector<OBSSource> sources;
		pls_get_all_source(sources, PRISM_VIEWER_COUNT_SOURCE_ID, nullptr, nullptr);
		for (const auto &source : sources) {
			pls_source_update_extern_params_json(source, viewerCountJson.constData(), OBS_SOURCE_VIEWER_COUNT_UPDATE_PARAMS_SUB_CODE_UPDATE_PARAMS);
		}
	});
}

static QString getStreamStateString(int state)
{
	QString ret = "";
	switch (state) {
	case ChannelData::ReadyState:
		ret = "readyState";
		break;
	case ChannelData::BroadcastGo:
		ret = "broadcastGo";
		break;
	case ChannelData::CanBroadcastState:
		ret = "canBroadcastState";
		break;
	case ChannelData::StreamStarting:
		ret = "streamStarting";
		break;
	case ChannelData::StreamStarted:
		ret = "streamStarted";
		break;
	case ChannelData::StopBroadcastGo:
		ret = "stopBroadcastGo";
		break;
	case ChannelData::CanBroadcastStop:
		ret = "canBroadcastStop";
		break;
	case ChannelData::StreamStopping:
		ret = "streamStopping";
		break;
	case ChannelData::StreamStopped:
		ret = "streamStopped";
		break;
	case ChannelData::StreamEnd:
		ret = "streamEnd";
		break;
	default:
		break;
	}

	return ret;
}

static QString getRecordStateString(int state)
{
	QString ret = "";
	switch (state) {
	case ChannelData::RecordReady:
		ret = "recordReady";
		break;
	case ChannelData::CanRecord:
		ret = "canRecord";
		break;
	case ChannelData::RecordStarting:
		ret = "recordStarting";
		break;
	case ChannelData::RecordStarted:
		ret = "recordStarted";
		break;
	case ChannelData::RecordStopping:
		ret = "recordStopping";
		break;
	case ChannelData::RecordStopGo:
		ret = "recordStopGo";
		break;
	case ChannelData::RecordStopped:
		ret = "recordStopped";
		break;
	default:
		break;
	}

	return ret;
}

GameCaptureResult::GameCaptureResult(const std::string &exe, const std::string &version, const std::string &title, const std::string &source_name, const std::string &source_id)
{
	exeVersion = version;
	gameTitle = title;
	gameExe = exe;
	sourceName = source_name;
	sourceId = source_id;
}

bool GameCaptureResult::foundResult(Result result)
{
	std::lock_guard<std::recursive_mutex> autoLock(m_lockCaptureResult);
	auto it = std::find_if(captureResultList.begin(), captureResultList.end(), [result](const Result &r) { return r.str == result.str; });
	if (it != captureResultList.end()) {
		return true;
	} else {
		return false;
	}
}

bool GameCaptureResult::isCaptured()
{
	std::lock_guard<std::recursive_mutex> autoLock(m_lockCaptureResult);
	auto it = std::find_if(captureResultList.begin(), captureResultList.end(), [](const Result &result) { return result.captured; });
	if (it != captureResultList.end()) {
		return true;
	} else {
		return false;
	}
}

void GameCaptureResult::insertResult(Result result)
{
	std::lock_guard<std::recursive_mutex> autoLock(m_lockCaptureResult);
	if (foundResult(result))
		return;
	captureResultList.push_back(result);
	blog(LOG_INFO,
	     "[game-capture : %s] Event of game capture. \n"
	     "    exe: %s %s\n"
	     "    game_title: %s\n"
	     "    event: %s",
	     sourceName.c_str(), gameExe.c_str(), exeVersion.c_str(), gameTitle.c_str(), result.str.c_str());
}

std::string GameCaptureResult::getLastFailedResult()
{
	std::lock_guard<std::recursive_mutex> autoLock(m_lockCaptureResult);
	std::string ret = "UnDefine error";
	if (!captureResultList.empty()) {
		ret = captureResultList.back().str;
	}
	return ret;
}

CheckCamProcessWorker::~CheckCamProcessWorker()
{
	if (!checkTimer) {
		return;
	}
	checkTimer->stop();
	delete checkTimer;
}

void CheckCamProcessWorker::Check()
{
	int pid = 0;

#if defined(Q_OS_WIN)
	bool processRun = pls_is_process_running(processName.toStdWString().c_str(), pid);
#elif defined(Q_OS_MACOS)
	bool processRun = pls_is_process_running(processName.toUtf8().constData(), pid);
#endif

	if (processIsExisted != processRun) {
		emit checkFinished(processRun);
	}

	processIsExisted = processRun;
}

void CheckCamProcessWorker::CheckCamProcessIsExisted(const QString &name)
{
	processName = name;

	if (checkTimer) {
		return;
	}

	checkTimer = pls_new<QTimer>(this);
	connect(checkTimer.data(), &QTimer::timeout, this, &CheckCamProcessWorker::Check);
	checkTimer->start(2000);
}

QString getOsVersion()
{
#ifdef Q_OS_WIN
	win_version_info wvi;
	get_win_ver(&wvi);
	return QString("Windows %1.%2.%3.%4").arg(wvi.major).arg(wvi.minor).arg(wvi.build).arg(wvi.revis);
#else
	pls_mac_ver_t ver = pls_get_mac_systerm_ver();
	return QString("Mac %1.%2.%3").arg(ver.major).arg(ver.minor).arg(ver.patch);
#endif
}

static QString getUserAgent()
{
#ifdef Q_OS_WIN64
	win_version_info wvi;
	get_win_ver(&wvi);
	LANGID langId = GetUserDefaultUILanguage();

	return QString("PRISM Live Studio/" PLS_VERSION " (Windows %1 Build %2 Architecture x64 Language %3)").arg(wvi.major).arg(wvi.build).arg(langId);
#else
	pls_mac_ver_t wvi = pls_get_mac_systerm_ver();
	QString langId = pls_get_current_system_language_id();
	return QString("PRISM Live Studio/" PLS_VERSION " (MacOS %1 Build %2 Architecture arm Language %3)").arg(wvi.major).arg(wvi.buildNum.c_str()).arg(langId);

#endif
}

void httpRequestHead(QVariantMap &headMap, bool hasGacc)
{
#if defined(Q_OS_WIN)
	headMap[HEADER_PRISM_DEVICE] = QStringLiteral("Windows OS");
#elif defined(Q_OS_MACOS)
	headMap[HEADER_PRISM_DEVICE] = QStringLiteral("Mac OS");
#endif
	if (hasGacc) {
		headMap[HEADER_PRISM_GCC] = GlobalVars::gcc.c_str();
	}
	headMap[HEADER_PRISM_USERCODE] = GlobalVars::logUserID.c_str();
	headMap[HEADER_PRISM_LANGUAGE] = pls_prism_get_locale();
	headMap[HEADER_PRISM_APPVERSION] = PLS_VERSION;
	headMap[HEADER_PRISM_IP] = pls_get_local_ip();
	headMap[HEADER_PRISM_OS] = getOsVersion();
	headMap[HEADER_USER_AGENT_KEY] = getUserAgent();
}

PLSBasic::PLSBasic(PLSMainView *mainView) : OBSBasic(mainView)
{
	LocalVars::s_basic = this;
#if defined(Q_OS_WIN)
	installNativeEventFilter();
#endif

	mainView->installEventFilter(this);

	ui->actionRemoveSource->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	ui->actionRemoveScene->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	ui->scenesFrame->addAction(ui->actionRemoveScene);
	ui->sources->addAction(ui->actionRemoveSource);
	setWindowIcon(QIcon(":/resource/images/logo/PRISMLiveStudio.ico"));

	connect(ui->sources, &SourceTree::SelectItemChanged, this, &OBSBasic::OnSelectItemChanged, Qt::QueuedConnection);
	connect(ui->sources, &SourceTree::VisibleItemChanged, this, &OBSBasic::OnVisibleItemChanged, Qt::QueuedConnection);
	connect(ui->sources, &SourceTree::itemsRemove, this, &OBSBasic::OnSourceItemsRemove, Qt::QueuedConnection);

	connect(
		PLSBasic::instance(), &PLSBasic::outputStateChanged, mainView,
		[this]() {
#if defined(Q_OS_WIN)
			if (logOut) {
				logOut->setEnabled(!pls_is_output_actived());
			}
#elif defined(Q_OS_MACOS)
			if (ui->actionCheckForUpdates) {
				ui->actionCheckForUpdates->setDisabled(pls_is_output_actived());
			}
#endif
			if (m_checkUpdateWidget) {
				m_checkUpdateWidget->setItemDisabled(pls_is_output_actived());
			}
			if (!pls_is_without_third_output_actived()) {
				pls_async_call(this, [this]() {
					checkNoticeAndForceUpdate();
					if (m_noticeTimer.isActive()) {
						m_noticeTimer.stop();
					}
					m_noticeTimer.start();
					PLS_INFO(MAINFRAME_MODULE, "restart notice timer");
				});
			} else {
				if (m_noticeTimer.isActive()) {
					PLS_INFO(MAINFRAME_MODULE, "stop notice timer");
					m_noticeTimer.stop();
				}
			}
		},
		Qt::QueuedConnection);
	connect(
		ui->sources, &SourceTree::itemsReorder, this,
		[this]() {
			PLSSceneitemMapMgrInstance->switchToDualOutputMode();
			ReorderBgmSourceList();
			emit itemsReorderd();
		},
		Qt::QueuedConnection);

	UpdateStudioModeUI(IsPreviewProgramMode());

	updateLiveEndUI();
	updateRecordEndUI();

	connect(mainView, &PLSMainView::studioModeChanged, this, &PLSBasic::TogglePreviewProgramMode);
	connect(mainView, &PLSMainView::isshowSignal, this, &PLSBasic::OnMainViewShow, Qt::QueuedConnection);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::liveStateChanged, this,
		[](int state) {
			auto api = PLSBasic::instance()->getApi();
			if (api) {
				api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_STREAM_STATE_CHANGED, {getStreamStateString(state)});
			}
		},
		Qt::QueuedConnection);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::recordingChanged, this,
		[](int state) {
			auto api = PLSBasic::instance()->getApi();
			if (api) {
				api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_RECORD_STATE_CHANGED, {getRecordStateString(state)});
			}
		},
		Qt::QueuedConnection);

	auto interval = pls_prism_get_qsetting_value("FUTI", 60 * 60).toInt();
	m_noticeTimer.setInterval((interval > 0 ? interval : 600) * 1000);

	connect(&m_noticeTimer, &QTimer::timeout, [this]() {
		PLS_INFO(MAINFRAME_MODULE, "polling time, request update and notice api");
		if (pls_is_without_third_output_actived()) {
			PLS_INFO(MAINFRAME_MODULE, "polling time, output actived, can not request notice and update api");
			return;
		}
		checkNoticeAndForceUpdate();
	});
	obs_frontend_add_event_callback(&frontendEventHandler, this);
	pls_frontend_add_event_callback(&frontendEventHandler, this);
	pls_frontend_add_event_callback(&PLSBasic::LogoutCallback, this);

	connect(
		ui->hMixerScrollArea->verticalScrollBar(), &QScrollBar::rangeChanged, this,
		[this]() {
			pls_check_app_exiting();
			bool showing = ui->hMixerScrollArea->verticalScrollBar()->isVisible();
			if (auto layout = ui->hMixerScrollArea->widget()->layout(); layout) {
				layout->setContentsMargins(3, 1, showing ? 0 : 9, 1);
			}
		},
		Qt::QueuedConnection);
	connect(
		ui->vMixerScrollArea->horizontalScrollBar(), &QScrollBar::rangeChanged, this,
		[this]() {
			pls_check_app_exiting();
			bool showing = ui->vMixerScrollArea->horizontalScrollBar()->isVisible();
			if (auto layout = ui->vMixerScrollArea->widget()->layout(); layout) {
				layout->setContentsMargins(14, 0, 20, showing ? 10 : 20);
			}
		},
		Qt::QueuedConnection);
	connect(ui->vVolumeWidgets, &PLSMixerContent::mixerReorderd, this, &PLSBasic::OnMixerOrderChanged);
	connect(ui->hVolumeWidgets, &PLSMixerContent::mixerReorderd, this, &PLSBasic::OnMixerOrderChanged);

	connect(GiphyDownloader::instance(), &GiphyDownloader::downloadResult, this, &PLSBasic::OnStickerDownloadCallback);
	connect(PLS_PLATFORM_API, &PLSPlatformApi::liveEndedForUi, this, &PLSBasic::checkTwitchAudio, Qt::QueuedConnection);
	connect(PLS_PLATFORM_API, &PLSPlatformApi::platformActiveDone, this, &PLSBasic::checkTwitchAudio, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelAdded, this, &PLSBasic::checkTwitchAudioWithUuid, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelRemovedForCheckVideo, this, &PLSBasic::checkTwitchAudioWithLeader, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelRemovedForChzzk, this, &PLSBasic::removeChzzkSponsorSource, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelRefreshEnd, this, &PLSBasic::setAllChzzkSourceSetting, Qt::QueuedConnection);
	auto goLivePannel = mainView->statusBar()->getGoLivePannel();
	connect(this, &PLSBasic::goLiveCheckTooltip, goLivePannel, &GoLivePannel::setRecTooltip, Qt::QueuedConnection);
#if defined(Q_OS_WIN)
	m_checkUpdateWidgetAction = new QWidgetAction(this);
	m_checkUpdateWidget = pls_new<PLSNewIconActionWidget>(ui->actionCheckForUpdates->text());
	m_checkUpdateWidget->setTextMarginLeft(20);
	m_checkUpdateWidget->setProperty("type", "mainMenu");

	connect(m_checkUpdateWidgetAction, SIGNAL(triggered()), this, SLOT(on_checkUpdate_triggered()));
	m_checkUpdateWidgetAction->setDefaultWidget(m_checkUpdateWidget);
	replaceMenuActionWithWidgetAction(ui->menubar, ui->actionCheckForUpdates, m_checkUpdateWidgetAction);

#endif
}
PLSBasic::~PLSBasic()
{
	pls_delete(backgroundMusicView);
	pls_delete(sceneCollectionView, nullptr);
	if (giphyStickerView)
		pls_delete(giphyStickerView);
	if (prismStickerView)
		pls_delete(prismStickerView);
	if (regionCapture)
		pls_delete(regionCapture);
	if (ncb2bBrowserSettings)
		pls_delete(ncb2bBrowserSettings);
	if (m_sceneTemplate)
		pls_delete(m_sceneTemplate);

	PLSDrawPenMgr::Instance()->Release();
	PLSAudioControl::instance()->ClearObsSignals();
	LocalVars::s_basic = nullptr;

	std::lock_guard<std::recursive_mutex> autoLock(m_lockGameCaptureResultMap);
	for (auto game : m_gameCaptureResultMap) {
		std::string result = game.second->isCaptured() ? "game captured" : game.second->getLastFailedResult();
		PLS_LOGEX(PLS_LOG_WARN, MAINFRAME_MODULE,
			  {{PTS_LOG_TYPE, PTS_TYPE_EVENT},
			   {"source_name", game.second->sourceName.c_str()},
			   {"game_exe", game.second->gameExe.c_str()},
			   {"game_version", game.second->exeVersion.c_str()},
			   {"game_title", game.second->gameTitle.c_str()},
			   {"game_event_type", result.c_str()}},
			  "[game-capture : %s] Shutdown Event of game capture. \n"
			  "    exe: %s %s\n"
			  "    game_title: %s\n"
			  "    event: %s",
			  game.second->sourceName.c_str(), game.second->gameExe.c_str(), game.second->exeVersion.c_str(), game.second->gameTitle.c_str(), result.c_str());

		QVariantMap info = {{"gameTitle", game.second->gameTitle.c_str()}, {"gameExe", game.second->gameExe.c_str()}, {"captureResult", result.c_str()}};
		pls_send_analog(AnalogType::ANALOG_ADD_SOURCE, {{ANALOG_SOURCE_TYPE_KEY, action::GetActionSourceID(game.second->sourceId.c_str())}, {ANALOG_ITEM_KEY, ""}, {ANALOG_DETAIL_KEY, info}});
	}
	m_gameCaptureResultMap.clear();
}

void PLSBasic::getLocalUpdateResult()
{
}

void OBSBasic::UpdateStudioModeUI(bool studioMode)
{
	QMargins margins = ui->horizontalLayout_2->contentsMargins();
	QMargins newMargins = margins;

	if (studioMode) {
		newMargins.setTop(newMargins.top() - 10);
	} else {
		newMargins.setTop(newMargins.top() + 10);
	}
	ui->horizontalLayout_2->setContentsMargins(newMargins);

	ui->scenesFrame->OnStudioModeStatus(studioMode);

	if (previewProgramTitle) {
		previewProgramTitle->OnStudioModeStatus(studioMode);
		previewProgramTitle->CustomResize();
	}

	ui->previewTitle->setVisible(false);
}

PLSBasic *PLSBasic::instance()
{
	return LocalVars::s_basic;
}

PLSMainView *PLSBasic::getMainView()
{
	return mainView;
}

pls_frontend_callbacks *PLSBasic::getApi()
{
	return api;
}
void PLSBasic::removeConfig(const char *config)
{
	std::array<char, 512> cpath;
	if (GetAppConfigPath(cpath.data(), sizeof(cpath), config) <= 0) {
		PLS_INFO(MAINFRAME_MODULE, "remove config[%s] failed, because get config path failed.", config);
		return;
	}

	QString path = QString::fromUtf8(cpath.data());
	if (QFileInfo(path).isDir()) {
		PLS_DEBUG(MAINFRAME_MODULE, "clear config directory.");
		QDir dir(path);
		QFileInfoList fis = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files, QDir::DirsFirst);
		for (const QFileInfo &fi : fis) {
			QString fp = fi.absoluteFilePath();
			if (fi.isDir()) {
				PLS_DEBUG(MAINFRAME_MODULE, "remove config directory.");
				QDir subdir(fp);
				subdir.removeRecursively();
			} else {
				PLS_DEBUG(MAINFRAME_MODULE, "remove config file %s.", pls_get_path_file_name(fp.toUtf8().constData()));
				QFile::remove(fp);
			}
		}
	} else {
		PLS_DEBUG(MAINFRAME_MODULE, "remove config file %s.", pls_get_path_file_name(cpath.data()));
		QFile::remove(path);
	}

	PLS_INFO(MAINFRAME_MODULE, "remove config[%s] successed.", config);
}
void PLSBasic::backupSceneCollection(QString &secneCollectionName, QString &sceneCollectionFile)
{
	QString path = pls_get_app_data_dir() + "/" + QString::fromUtf8("PRISMLiveStudio/global.ini");
	QFile file(path);
	if (!file.exists()) {
		PLS_INFO(LAUNCHER_LOGIN, "Failed to backup scene collection file: 'global.ini' does not exsit.");
		return;
	}

	QSettings settings(path, QSettings::IniFormat);
	auto sc = settings.value("Basic/SceneCollection").toString();
	auto scFile = settings.value("Basic/SceneCollectionFile").toString();

	secneCollectionName = sc;
	sceneCollectionFile = scFile;
}
static void createSceneCollectionConfigFile(const QString &secneCollectionName, const QString &sceneCollectionFile)
{
	if (secneCollectionName.isEmpty() || sceneCollectionFile.isEmpty())
		return;

	QString path = pls_get_app_data_dir() + "/" + QString::fromUtf8("PRISMLiveStudio/global.ini");
	QSettings settings(path, QSettings::IniFormat);
	settings.beginGroup("Basic");
	settings.setValue("SceneCollection", secneCollectionName);
	settings.setValue("SceneCollectionFile", sceneCollectionFile);
	settings.endGroup();
}
void PLSBasic::clearPrismConfigInfo()
{
	PLS_INFO(LAUNCHER_LOGIN, "user info expired, clear user configs");

	removeConfig("PRISMLiveStudio/user/cache");
	removeConfig("PRISMLiveStudio/Cache");
	removeConfig("PRISMLiveStudio/plugin_config");
	removeConfig("PRISMLiveStudio/user/gcc.json");
	removeConfig("PRISMLiveStudio/user/config.ini");
	removeConfig("PRISMLiveStudio/global.ini");
	removeConfig("PRISMLiveStudio/naver_shopping");

	// Do not clear scene collection info when logout
	QFile::rename(pls_get_app_data_dir() + "/" + "PRISMLiveStudio/global.bak", pls_get_app_data_dir() + "/" + "PRISMLiveStudio/global.ini");
}

void OBSBasic::setDocksVisible(bool visible)
{
	QList<PLSDock *> docks;
	docks << ui->scenesDock << ui->sourcesDock << ui->mixerDock << ui->chatDock;

	for (auto dock : docks) {
		if (dock->isFloating()) {
			setDockDisplayAsynchronously(dock, visible);
		}
	}

#define SET_DOCKLIST_VISIBLE(dockList, visible)                        \
	for (auto i = dockList.size() - 1; i >= 0; i--) {              \
		auto dock = static_cast<PLSDock *>(dockList[i].get()); \
		if (!dock || !dock->isFloating()) {                    \
			continue;                                      \
		}                                                      \
		setDockDisplayAsynchronously(dock, visible);           \
	}

	SET_DOCKLIST_VISIBLE(extraDocks, visible);
	SET_DOCKLIST_VISIBLE(extraBrowserDocks, visible);
	SET_DOCKLIST_VISIBLE(ncb2bCustomDocks, visible);
}

void OBSBasic::setDocksVisibleProperty()
{
	QList<PLSDock *> docks;
	docks << ui->scenesDock << ui->sourcesDock << ui->mixerDock << ui->chatDock;

	for (auto dock : docks) {
		if (dock) {
			dock->setProperty("vis", dock->isVisible());
		}
	}
}

void OBSBasic::setDockDisplayAsynchronously(PLSDock *dock, bool visible)
{
	if (!dock) {
		return;
	}
	bool vis = visible && dock->property("vis").toBool();
	if (vis) {
		pls_async_call(this, [dock]() { dock->setVisible(true); });
		return;
	}
	dock->setVisible(false);
}

BrowserDock *OBSBasic::addNcb2bCustomDock(const QString &title, const QString &url, const QString &uuid, bool firstCreate, QByteArray geometry)
{
	BrowserDock *dock = createBrowserDock(title, url, uuid, firstCreate, geometry, true);
	ncb2bCustomDocks.push_back(std::shared_ptr<QDockWidget>(dock));
	ncb2bCustomDockNames.push_back(title);
	ncb2bCustomDockUrls.push_back(url);
	return dock;
}

QList<std::shared_ptr<QDockWidget>> OBSBasic::getNcb2bCustomDocks()
{
	return ncb2bCustomDocks;
}

QList<QString> OBSBasic::getNcb2bCustomDocksUrls()
{
	return ncb2bCustomDockUrls;
}

QList<QString> OBSBasic::getNcb2bCustomDocksNames()
{
	return ncb2bCustomDockNames;
}

std::shared_ptr<QDockWidget> OBSBasic::getNcb2bCustomDock(const QString &title)
{
	if (!ncb2bCustomDockNames.contains(title)) {
		return nullptr;
	}
	int index = ncb2bCustomDockNames.indexOf(title);
	if (index < 0 || index >= ncb2bCustomDocks.count()) {
		return nullptr;
	}
	return ncb2bCustomDocks[index];
}

void OBSBasic::updateNcb2bDockUrl(int index, const QString &url)
{
	if (index < 0 || index >= ncb2bCustomDockUrls.count()) {
		return;
	}
	ncb2bCustomDockUrls[index] = url;
}

void OBSBasic::updateNcb2bDockName(int index, const QString &name)
{
	if (index < 0 || index >= ncb2bCustomDockNames.count()) {
		return;
	}
	ncb2bCustomDockNames[index] = name;
}

bool PLSBasic::willshow()
{
	return true;
}

extern void intializeOutNode();
void PLSBasic::PLSInit()
{
#if defined(Q_OS_WINDOWS)
	LabManage->checkLabDllUpdate();
#endif

	//scene menu
	adjustFileMenu();
	auto sceneCollectionActions = ui->sceneCollectionMenu->actions();
	for (const auto &action : sceneCollectionActions) {
		if (action->isSeparator() || action->objectName() == "actionShowMissingFiles") {
			ui->sceneCollectionMenu->removeAction(action);
		}
	}
	ui->sceneCollectionMenu->insertSeparator(ui->actionSceneCollectionManagement);
	//view menu
	ui->toggleStatusBar->setVisible(false);
	ui->toggleListboxToolbars->setVisible(false);
	//dock
	ui->toggleStats->setVisible(false);
	//profile
	ui->actionDupProfile->setVisible(false);
	ui->actionRenameProfile->setVisible(false);
	ui->actionRemoveProfile->setVisible(false);
	//tools
	ui->autoConfigure->setVisible(false);
	ui->autoConfigure2->setVisible(false);

	initVirtualCamMenu();
	initLabMenu();

#if defined(Q_OS_MACOS)
	ui->menuTools->addAction(virtualCamMenu->menuAction());
	ui->menuTools->addAction(prismLab);
	ui->actionFullscreenInterface->setShortcutContext(Qt::ApplicationShortcut);
	ui->actionFullscreenInterface->setShortcut(QKeySequence(Qt::META | Qt::CTRL | Qt::Key_F));
	ui->actionFullscreenInterface->setVisible(true);
	ui->viewMenu->addAction(ui->actionFullscreenInterface);
#endif

#if defined(Q_OS_WIN)
	//help
	ui->actionCheckForUpdates->setVisible(false);
	ui->menuCrashLogs->menuAction()->setVisible(false);
#endif
	ui->menuLogFiles->menuAction()->setVisible(false);

#if defined(Q_OS_WIN)
	ui->menu_File->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);

	ui->menuBasic_MainMenu_Edit->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);
	ui->orderMenu->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);
	ui->scalingMenu->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);
	ui->transformMenu->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);

	ui->viewMenu->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);

	ui->menuDocks->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);

	ui->profileMenu->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);

	ui->sceneCollectionMenu->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);

	ui->menuTools->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);
	ui->menuBasic_MainMenu_Help->setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);

	adjustedMenu();
	connect(mainView->menuButton(), &QPushButton::clicked, this, &PLSBasic::onShowMainMenu);
#endif

	pls_add_css(this, {"SettingsDialog", "ConnectInfo", "SourceToolBar"});

	// scene display mode
	int displayMethod = GetSceneDisplayMethod();
	SetSceneDisplayMethod(displayMethod);

	initConnect();

	QString udpateUrl(config_get_string(App()->GetUserConfig(), "AppUpdate", "updateUrl"));
	QString updateGcc(config_get_string(App()->GetUserConfig(), "AppUpdate", "updateGcc"));
	//start resource download
	if (udpateUrl.isEmpty()) {
		m_isStartCategoryRequest = true;
	} else {
		PLS_INFO(UPDATE_MODULE, "PLSInit() method enter update program");
		m_updateGcc = updateGcc;
		m_isStartCategoryRequest = false;
	}
#if defined(Q_OS_WINDOWS)
	LabManage->checkLabZipUpdate();
#endif
	PLSLaunchWizardView::instance();
	PLS_PLATFORM_API->initialize();

	initChannelUI();
	intializeOutNode();

	//audio master control
	QList<QAction *> listActions;
	auto *actionAudioMasterCtrl = pls_new<QAction>(ui->hMixerScrollArea);
	actionAudioMasterCtrl->setObjectName("audioMasterControl");
	connect(actionAudioMasterCtrl, &QAction::triggered, this, &PLSBasic::OnActionAudioMasterControlTriggered);
	listActions.push_back(actionAudioMasterCtrl);

	bool muteState = PLSAudioControl::instance()->GetMuteState();
	actionAudioMasterCtrl->setProperty("muteAllAudio", muteState);
	actionAudioMasterCtrl->setToolTip(muteState ? QTStr("Basic.Main.UnmuteAllAudio") : QTStr("Basic.Main.MuteAllAudio"));

	//audio mixer
	auto actionAudioAdvance = pls_new<QAction>(QTStr("Basic.MainMenu.Edit.AdvAudio"), ui->hMixerScrollArea);
	actionAudioAdvance->setObjectName("audioAdvanced");
	connect(actionAudioAdvance, &QAction::triggered, this, &PLSBasic::OnActionAdvAudioPropertiesTriggered, Qt::DirectConnection);
	listActions.push_back(actionAudioAdvance);

	actionSperateMixer = pls_new<QAction>(ui->hMixerScrollArea);
	actionSperateMixer->setObjectName("detachBtn");
	SetAttachWindowBtnText(actionSperateMixer, ui->mixerDock->isFloating());

	connect(actionSperateMixer, &QAction::triggered, this, &PLSBasic::OnMixerDockSeperateBtnClicked);
	connect(ui->mixerDock, &QDockWidget::topLevelChanged, this, &PLSBasic::OnMixerDockLocationChanged);

	bool vertical = config_get_bool(App()->GetUserConfig(), "BasicWindow", "VerticalVolControl");
	actionMixerLayout = pls_new<QAction>(vertical ? QTStr("Basic.MainMenu.Mixer.Horizontal") : QTStr("Basic.MainMenu.Mixer.Vertical"), ui->hMixerScrollArea);
	connect(actionMixerLayout, &QAction::triggered, this, &PLSBasic::OnMixerLayoutTriggerd);

	ui->mixerDock->titleWidget()->setAdvButtonActions({actionSperateMixer, actionMixerLayout});
	ui->mixerDock->titleWidget()->setButtonActions(listActions);

	bool docksLocked = config_get_bool(App()->GetUserConfig(), "BasicWindow", "DocksLocked");
	setDockDetachEnabled(docksLocked);

	setUserIcon();

	ui->actionHelpPortal->disconnect();
	ui->actionWebsite->disconnect();
	ui->actionShowAbout->disconnect();
	ui->actionRepair->disconnect();
	ui->actionHelpOpenSource->disconnect();

#if defined(Q_OS_MACOS)
	ui->actionShowAbout->setMenuRole(QAction::AboutRole);
#endif

	connect(ui->actionHelpPortal, &QAction::triggered, this, &PLSBasic::on_actionHelpPortal_triggered);
	connect(ui->actionHelpOpenSource, &QAction::triggered, this, &PLSBasic::on_actionHelpOpenSource_triggered);
	connect(ui->actionPrismDiscord, &QAction::triggered, this, &PLSBasic::on_actionDiscord_triggered);
	connect(ui->actionWebsite, &QAction::triggered, this, &PLSBasic::on_actionWebsite_triggered);
	connect(ui->actionShowAbout, &QAction::triggered, this, &PLSBasic::on_actionShowAbout_triggered);
	connect(ui->actionContactUs, &QAction::triggered, this, [this]() { on_actionContactUs_triggered(); });
	connect(ui->actionRepair, &QAction::triggered, this, &PLSBasic::on_actionRepair_triggered);
	connect(ui->actionStudioMode, &QAction::triggered, this, &PLSBasic::on_actionStudioMode_triggered);
	connect(ui->stats, &QAction::triggered, this, &PLSBasic::on_stats_triggered);
	connect(ui->actionPrismPolicy, &QAction::triggered, this, &PLSBasic::on_actionPrismPolicy_triggered);

	auto text = ui->actionRepair->text();
	text.replace("PRISM", "OBS");
	ui->actionRepair->setText(text);

#if defined(Q_OS_MACOS)
	ui->menuBasic_MainMenu_Help->addAction(ui->actionCheckForUpdates);
	ui->actionCheckForUpdates->setVisible(true);
	ui->actionCheckForUpdates->setEnabled(true);
	connect(ui->actionCheckForUpdates, SIGNAL(triggered()), this, SLOT(on_checkUpdate_triggered()));
#endif

	CreatePreviewTitle();
	CreateToolArea();
	if (drawPenView)
		drawPenView->UpdateView(GetCurrentScene());

	InitInteractData();
	auto isTest = pls_prism_get_qsetting_value("MultiLanguageTest").toBool();
	if (isTest) {
		ui->menuBasic_MainMenu_Help->addAction("MultiLanguageTest Tool", [this]() {
			MutiLanguageTestView view;
			view.exec();
		});
	}
}

//just once
void PLSBasic::adjustFileMenu()
{
	static bool isAdjusted = false;
	if (isAdjusted) {
		return;
	}
	ui->menu_File->setTitle(QTStr("Basic.MainMenu.File.Settings"));
	ui->menu_File->removeAction(ui->actionShow_Recordings);
	ui->menu_File->removeAction(ui->actionRemux);
	ui->menu_File->removeAction(ui->actionShowProfileFolder);
#if defined(Q_OS_WIN)
	ui->menu_File->removeAction(ui->actionE_xit);
#endif
	auto actions = ui->menu_File->actions();
	for (const auto &action : actions) {
		if (action->isSeparator()) {
			ui->menu_File->removeAction(action);
			action->deleteLater();
		}
	}
	QAction *actionShowVideosFolder = new QAction(QTStr("Basic.MainMenu.File.ShowVideosFolder"), this);
	connect(actionShowVideosFolder, &QAction::triggered, this, &PLSBasic::on_actionShowVideosFolder_triggered);
	ui->menu_File->addAction(actionShowVideosFolder);
	ui->menu_File->addAction(ui->actionRemux);
	ui->menu_File->insertSeparator(ui->actionShowSettingsFolder);
	ui->menu_File->insertSeparator(ui->actionRemux);

	isAdjusted = true;
}

void PLSBasic::initVirtualCamMenu()
{
	if (virtualCamMenu == nullptr) {
		virtualCamMenu = new QMenu(QTStr("Basic.MainMenu.virtualCamera"), this);
		virtualCamMenu->setObjectName("camMenu");
	}

	if (virtualCam == nullptr) {
		virtualCam = new QAction(VirtualCamActive() ? QTStr("Basic.Main.StopVirtualCam") : QTStr("Basic.Main.StartVirtualCam"), this);
		virtualCam->setCheckable(true);
		connect(virtualCam, &QAction::triggered, this, &PLSBasic::VirtualCamActionTriggered);
		auto updateActionState = [this]() {
			virtualCam->setChecked(VirtualCamActive());
			virtualCam->setText(VirtualCamActive() ? QTStr("Basic.Main.StopVirtualCam") : QTStr("Basic.Main.StartVirtualCam"));
		};
		connect(this, &PLSBasic::outputStateChanged, this, updateActionState, Qt::QueuedConnection);
	}
	if (virtualCamSetting == nullptr) {
		virtualCamSetting = new QAction(QTStr("Basic.Main.VirtualCamConfig"), this);
		connect(virtualCamSetting, &QAction::triggered, this, &OBSBasic::OpenVirtualCamConfig);
	}
	virtualCamMenu->addAction(virtualCam);
	virtualCamMenu->addAction(virtualCamSetting);
	virtualCam->setChecked(VirtualCamActive());
	virtualCamMenu->setEnabled(vcamEnabled);
}

void PLSBasic::initLabMenu()
{
#if defined(Q_OS_WIN)
	if (prismLab == nullptr) {
		prismLab = new QAction(QTStr("Basic.MainMenu.File.laboratory"), this);
		this->addAction(prismLab);
		connect(prismLab, &QAction::triggered, this, &PLSBasic::actionLaboratory_triggered);
	}
#endif
}

QList<QAction *> PLSBasic::adjustedMenu()
{
	QList<QAction *> ret;
	ret << ui->menu_File->menuAction() << ui->menuBasic_MainMenu_Help->menuAction() << ui->sceneCollectionMenu->menuAction() << ui->menuBasic_MainMenu_Edit->menuAction()
	    << ui->viewMenu->menuAction() << ui->menuDocks->menuAction() << ui->profileMenu->menuAction() << ui->menuTools->menuAction() << virtualCamMenu->menuAction() << prismLab;

	connect(mainView, &PLSMainView::onGolivePending, ui->action_Settings, &QAction::setDisabled);

	if (logOut == nullptr) {
		logOut = new QAction(QTStr("Basic.MainMenu.File.Logout"), this);
		this->addAction(logOut);
		connect(logOut, &QAction::triggered, this, [this]() {
			if (PLSAlertView::question(this, tr("Confirm"), tr("main.message.logout_alert"), PLSAlertView::Button::Yes | PLSAlertView::Button::No) == PLSAlertView::Button::Yes) {
				PLSApp::plsApp()->backupGolbalConfig();
				pls_prism_logout();
			}
		});
	}
	ret << logOut;

	if (exitApp == nullptr) {
		exitApp = new QAction(QTStr("Basic.MainMenu.File.Exit"), OBSBasic::Get());
		exitApp->setShortcut(QKeySequence(tr("Shift+X")));
		exitApp->setShortcutContext(Qt::ApplicationShortcut);
		connect(exitApp, &QAction::triggered, this, []() { PLSMainView::instance()->close(); });
	}
	ret << exitApp;

	static bool isFirst = true;
	if (isFirst) {
		isFirst = false;
		this->addActions(ret);
	}
	return ret;
}

#if defined(Q_OS_WIN)
void PLSBasic::onShowMainMenu()
{
	pls_check_app_exiting();
	if (!m_mainMenuShow) {
		m_mainMenuShow = true;

		QPushButton *menuButton = dynamic_cast<QPushButton *>(sender());
		auto pos = menuButton->mapToGlobal(menuButton->rect().bottomLeft() + QPoint(0, 5));
		QMenu mainMenu(this);
		pls_push_modal_view(&mainMenu);
		mainMenu.setObjectName("mainMenu");
		mainMenu.setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);
		mainMenu.addActions(adjustedMenu());
		auto bOutputActived = pls_is_output_actived();
		logOut->setEnabled(!bOutputActived);
		ui->profileMenu->setEnabled(!bOutputActived);
		mainMenu.exec(pos);
		pls_pop_modal_view(&mainMenu);
	} else {
		m_mainMenuShow = false;
	}
}
#endif

void PLSBasic::OnActionAudioMasterControlTriggered() const
{
	PLSAudioControl::instance()->TriggerMute();
}

void PLSBasic::OnActionAdvAudioPropertiesTriggered()
{
	on_actionAdvAudioProperties_triggered();
}

void PLSBasic::OnMixerDockSeperateBtnClicked()
{
	changeDockState(ui->mixerDock, actionSperateMixer);
}

void PLSBasic::OnMixerDockLocationChanged()
{
	SetAttachWindowBtnText(actionSperateMixer, ui->mixerDock->isFloating());
}

void PLSBasic::OnMixerLayoutTriggerd()
{
	ToggleVolControlLayout();
	UpdateAudioMixerMenuTxt();
}

void PLSBasic::OnScenesCurrentItemChanged()
{
	if (api) {
		api->on_event(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED);
		api->on_event(OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED);
		//api->on_event(OBS_FRONTEND_EVENT_SCENE_LIST_CURRENT_CHANGED);
	}
	UpdateContextBarDeferred();
}

void PLSBasic::OnStickerClicked()
{
	if (!giphyStickerView) {
		CreateGiphyStickerView();
		giphyStickerView->show();
		giphyStickerView->raise();
	} else {
		bool visible = !giphyStickerView->isVisible();
		giphyStickerView->setVisible(visible);
		if (visible)
			giphyStickerView->raise();
	}
}

void PLSBasic::OnPrismStickerClicked()
{
	if (nullptr == prismStickerView) {
		CreatePrismStickerView();
		prismStickerView->show();
		prismStickerView->raise();
	} else {
		bool visible = !prismStickerView->isVisible();
		prismStickerView->setVisible(visible);
		if (visible)
			prismStickerView->raise();
	}
}

void SetStickerSourceSetting(OBSSource source, const QString &file, const GiphyData &giphyData)
{
	if (file.isEmpty())
		return;
	OBSData oldSettings(obs_data_create());
	obs_data_release(oldSettings);
	OBSData settings = obs_source_get_settings(source);
	obs_data_apply(oldSettings, settings);
	obs_data_release(settings);

	obs_data_set_string(settings, "file", QT_TO_UTF8(file));
	obs_data_set_string(settings, "id", QT_TO_UTF8(giphyData.id));
	obs_data_set_string(settings, "type", QT_TO_UTF8(giphyData.type));
	obs_data_set_string(settings, "title", QT_TO_UTF8(giphyData.title));
	obs_data_set_string(settings, "rating", QT_TO_UTF8(giphyData.rating));

	obs_data_set_int(settings, "preview_width", giphyData.sizePreview.width());
	obs_data_set_int(settings, "preview_height", giphyData.sizePreview.height());
	obs_data_set_int(settings, "original_width", giphyData.sizeOriginal.width());
	obs_data_set_int(settings, "original_height", giphyData.sizeOriginal.height());

	obs_data_set_string(settings, "preview_url", QT_TO_UTF8(giphyData.previewUrl));
	obs_data_set_string(settings, "original_url", QT_TO_UTF8(giphyData.originalUrl));

	obs_source_update(source, settings);
}

void PLSBasic::OnStickerApply(const QString &fileName, const GiphyData &giphyData)
{
	OBSSource newSource;
	AddGiphyStickerSource(fileName, giphyData, newSource);
	if (!newSource)
		return;

	/* register undo/redo */
	const char *id = PRISM_GIPHY_STICKER_SOURCE_ID;
	const char *newName = obs_source_get_name(newSource);
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	std::string scene_name = obs_source_get_name(main->GetCurrentSceneSource());
	auto undo = [scene_name, main](const std::string &data) {
		OBSSourceAutoRelease source = obs_get_source_by_name(data.c_str());
		obs_source_remove(source);

		OBSSourceAutoRelease scene_source = obs_get_source_by_name(scene_name.c_str());
		main->SetCurrentScene(scene_source.Get(), true);
	};
	OBSDataAutoRelease wrapper = obs_data_create();
	obs_data_set_string(wrapper, "id", id);
	OBSSceneItemAutoRelease item = obs_scene_sceneitem_from_source(main->GetCurrentScene(), newSource);
	obs_data_set_int(wrapper, "item_id", obs_sceneitem_get_id(item));
	obs_data_set_string(wrapper, "name", newName);
	obs_data_set_bool(wrapper, "visible", true);

	auto redo = [scene_name, main, fileName, giphyData](const std::string &data) {
		OBSSourceAutoRelease scene_source = obs_get_source_by_name(scene_name.c_str());
		main->SetCurrentScene(scene_source.Get(), true);

		OBSDataAutoRelease dat = obs_data_create_from_json(data.c_str());
		OBSSource source;

		PLSBasic::instance()->AddGiphyStickerSource(fileName, giphyData, source);
		if (source) {
			OBSSceneItemAutoRelease item = obs_scene_sceneitem_from_source(main->GetCurrentScene(), source);
			obs_sceneitem_set_id(item, (int64_t)obs_data_get_int(dat, "item_id"));
		}
	};
	undo_s.add_action(QTStr("Undo.Add").arg(newName), undo, redo, std::string(obs_source_get_name(newSource)), std::string(obs_data_get_json(wrapper)));
}

static void source_show_default(obs_source_t *source, obs_scene_t *scene)
{
	struct AddSourceData {
		obs_source_t *source;
		bool visible;
	};

	AddSourceData data;
	data.source = source;
	data.visible = true;

	auto addStickerSource = [](void *_data, obs_scene_t *scene_ptr) {
		auto add_source_info = (AddSourceData *)_data;
		obs_sceneitem_t *sceneitem;

		sceneitem = obs_scene_add(scene_ptr, add_source_info->source);
		obs_sceneitem_set_visible(sceneitem, add_source_info->visible);
	};

	obs_enter_graphics();
	obs_scene_atomic_update(scene, addStickerSource, &data);
	obs_leave_graphics();
}

void PLSBasic::AddGiphyStickerSource(const QString &file, const GiphyData &giphyData, OBSSource &sourceOut)
{
	OBSScene scene = GetCurrentScene();
	if (!scene)
		return;

	if (file.isEmpty())
		return;

	OBSSource newSource = nullptr;
	const char *id = PRISM_GIPHY_STICKER_SOURCE_ID;
	if (!CreateSource(id, newSource))
		return;

	sourceOut = newSource;

	action::SendActionToNelo(id, action::ACTION_ADD_EVENT, id);
	action::SendActionLog(action::ActionInfo(action::EVENT_MAIN_ADD, action::EVENT_SUB_SOURCE_ADDED, action::EVENT_TYPE_CONFIRMED, action::GetActionSourceID(id)));

	action::SendPropToNelo(id, "stickerId", qUtf8Printable(giphyData.originalUrl));

	SetStickerSourceSetting(newSource, file, giphyData);

#if 0
		// set initialize position to scene center.
		obs_video_info ovi;
		vec3 screenCenter;
		obs_get_video_info(&ovi);
		vec3_set(&screenCenter, float(ovi.base_width), float(ovi.base_height), 0.0f);
		vec3_mulf(&screenCenter, &screenCenter, 0.5f);

		const auto stickerWidth = float(giphyData.sizeOriginal.width());
		const auto stickerHeight = float(giphyData.sizeOriginal.height());

		struct AddSourceData {
			obs_source_t *source;
			bool visible;
			vec2 centerPos;
			vec2 scale;
		};

		AddSourceData data;
		data.source = source;
		data.visible = true;

		float radio = 1.0f;
		data.scale.x = radio;
		data.scale.y = radio;

		data.centerPos.x = screenCenter.x - stickerWidth * radio / 2.0f;
		data.centerPos.y = screenCenter.y - stickerHeight * radio / 2.0f;

		auto addStickerSource = [](void *_data, obs_scene_t *scene_ptr) {
			auto add_source_info = (AddSourceData *)_data;
			obs_sceneitem_t *sceneitem;

			sceneitem = obs_scene_add(scene_ptr, add_source_info->source);
			obs_sceneitem_set_pos(sceneitem, &add_source_info->centerPos);
			obs_sceneitem_set_scale(sceneitem, &add_source_info->scale);
			obs_sceneitem_set_visible(sceneitem, add_source_info->visible);
		};

		obs_enter_graphics();
		obs_scene_atomic_update(scene, addStickerSource, &data);
		obs_leave_graphics();

#else
	source_show_default(newSource, scene);
#endif
	OnSourceCreated(id);

	/* set monitoring if source monitors by default */
	auto flags = obs_source_get_output_flags(newSource);
	if ((flags & OBS_SOURCE_MONITOR_BY_DEFAULT) != 0) {
		obs_source_set_monitoring_type(newSource, OBS_MONITORING_TYPE_MONITOR_ONLY);
	}
}

void PLSBasic::CheckStickerSource() const
{
	PLSStickerDataHandler::CheckStickerSource();
}

void PLSBasic::OnDrawPenClicked() const
{
	if (!drawPenView)
		return;

	// avoid flash #1415
	ui->contextContainer->setVisible(false);

	bool drawPenMode = !drawPenView->isVisible();
	drawPenView->setVisible(drawPenMode);
	if (previewProgramTitle) {
		previewProgramTitle->OnDrawPenModeStatus(drawPenMode, IsPreviewProgramMode());
	}

	if (drawPenMode) {
		ui->preview->SetLocked(drawPenMode);
		if (verticalDisplay) {
			verticalDisplay->SetLocked(drawPenMode);
		}
		ui->actionLockPreview->setChecked(drawPenMode);
	} else {
		bool locked = ui->preview->CacheLocked();
		ui->preview->SetLocked(locked);

		if (verticalDisplay) {
			locked = verticalDisplay->CacheLocked();
			verticalDisplay->SetLocked(locked);
		}
		ui->actionLockPreview->setChecked(locked);
	}

	PLS_UI_STEP(DRAWPEN_MODULE, QString("Drawpen mode switched to ").append(drawPenMode ? "on" : "off").toUtf8().constData(), ACTION_CLICK);
}

void PLSBasic::OnSceneTemplateClicked(ShowType iState)
{
	pls_check_app_exiting();

	if (nullptr == m_sceneTemplate) {
		DialogInfo info;
		info.configId = ConfigId::SceneTemplateConfig;
		info.defaultWidth = 1145;
		info.defaultHeight = 762;
		m_sceneTemplate = pls_new<PLSSceneTemplateContainer>(info);
	}

	switch (iState) {
	case ShowType::ST_Hide:
		m_sceneTemplate->hide();
		break;
	case ShowType::ST_Show:
		m_sceneTemplate->show();
		break;
	default:
		m_sceneTemplate->setVisible(!m_sceneTemplate->isVisible());
		break;
	}

	if (m_sceneTemplate->isVisible()) {
		m_sceneTemplate->activateWindow();
#ifdef __APPLE__
		bringWindowToTop(m_sceneTemplate);
#endif
	}
}

#if defined(Q_OS_MACOS)
static bool isMatchSystemVersion()
{
	pls_mac_ver_t ver = pls_get_mac_systerm_ver();
	int major = ver.major;
	if (major < 12) {
		return false;
	} else if (major == 12) {
		int minor = ver.minor;
		if (minor < 3) {
			return false;
		}
	}
	return true;
}
#endif

void PLSBasic::OnCamStudioClicked(QStringList arguments, QWidget *parent)
{
	bool appInstalled = false;
	QString installLocation;
	QString program;

	appInstalled = CheckCamStudioInstalled(program);
	if (!appInstalled) {
		ShowInstallCamStudioTips(parent, QTStr("Alert.Title"), QTStr("Main.Install.Cam.Studio"), QTStr("Main.cam.install.now"), QTStr("OK"));
		return;
	}

#if defined(Q_OS_MACOS)
	auto startCallback = [](void *inUserData, bool isSucceed, int) {
		if (pls_is_app_exiting()) {
			return;
		}
		auto basicClass = static_cast<PLSBasic *>(inUserData);
		if (!isSucceed) {
			PLS_ERROR(MAIN_CAM_STUDIO, "create cam process failed, error: %d", pls_last_error());
			App()->getMainView()->updateSideBarButtonStyle(ConfigId::CamStudioConfig, false);
			return;
		}
		basicClass->getMainView()->updateSideBarButtonStyle(ConfigId::CamStudioConfig, true);
		QString processName;
		basicClass->CreateCheckCamProcessThread(processName);
		PLS_LOGEX(PLS_LOG_INFO, MAIN_CAM_STUDIO, {{"toolUsage", "CamStudio"}}, "call open cam studio success");
	};
	pls_libutil_api_mac::pls_mac_create_process_with_not_inherit(program, arguments, this, startCallback);
#else
	QString processName;
	// check process running
	camStudioProcess = pls_process_create(program, arguments, "");
	if (!camStudioProcess) {
		PLS_ERROR(MAIN_CAM_STUDIO, "create cam process failed, error: %d", pls_last_error());
		App()->getMainView()->updateSideBarButtonStyle(ConfigId::CamStudioConfig, false);
		return;
	}
	pls_process_destroy(camStudioProcess);
	camStudioProcess = nullptr;
	getMainView()->updateSideBarButtonStyle(ConfigId::CamStudioConfig, true);
	CreateCheckCamProcessThread(processName);
	PLS_LOGEX(PLS_LOG_INFO, MAIN_CAM_STUDIO, {{"toolUsage", "CamStudio"}}, "call open cam studio success");
#endif
}

bool PLSBasic::CheckCamStudioInstalled(QString &program)
{
	bool appInstalled = false;
	QString installLocation;
	QString processName;

	// check install
#if defined(Q_OS_WIN)
	QSettings settings("HKEY_CURRENT_USER\\Software\\NAVER Corporation\\PRISM Lens", QSettings::NativeFormat);
	installLocation = settings.value("InstallDir").toString();
	if (!installLocation.isEmpty()) {
		appInstalled = true;
	}
	processName = QString(PRISM_CAM_PRODUCTION_NAME) + ".exe";
	program = installLocation + QDir::separator() + processName;

#elif defined(Q_OS_MACOS)
	QStringList appList;
	pls_get_install_app_list("com.prismlive.camstudio", appList);
	if (appList.isEmpty()) {
		appInstalled = false;
	} else {
		for (int i = 0; i < appList.size(); i++) {
			QString appPath = appList[i];
			if (appPath.contains(".app/")) {
				continue;
			}
			if (appPath.startsWith("/Applications")) {
				appInstalled = true;
				installLocation = appPath;
				break;
			}
		}
	}

	program = installLocation;
	processName = PRISM_CAM_PRODUCTION_NAME;
#endif

	return appInstalled;
}

void PLSBasic::ShowInstallCamStudioTips(QWidget *parent, QString title, QString content, QString okTip, QString cancelTip)
{
	QMap<PLSAlertView::Button, QString> buttons = {{PLSAlertView::Button::Ok, okTip}, {PLSAlertView::Button::Cancel, cancelTip}};
	auto ret = PLSAlertView::question(parent, title, content, buttons);
	if (ret == PLSAlertView::Button::Ok) {
		QString lang = pls_prism_get_locale();
		QString linkUrl;
		if (0 == lang.compare("ko-KR", Qt::CaseInsensitive)) {
			linkUrl = "https://prismlive.com/ko_kr/lens.html";
		} else {
			linkUrl = "https://prismlive.com/en_us/lens.html";
		}
		QDesktopServices::openUrl(QUrl(linkUrl, QUrl::TolerantMode));
	}
}

void PLSBasic::InitCamStudioSidebarState()
{
	QString processName;

#if defined(Q_OS_WIN)
	processName = QString(PRISM_CAM_PRODUCTION_NAME) + ".exe";
#elif defined(Q_OS_MACOS)
	processName = PRISM_CAM_PRODUCTION_NAME;
#endif
	if (processName.isEmpty()) {
		App()->getMainView()->updateSideBarButtonStyle(ConfigId::CamStudioConfig, false);
		return;
	}
	CreateCheckCamProcessThread(processName);
}

void PLSBasic::CreateCheckCamProcessThread(const QString &processName)
{
	if (!checkCamProcessWorker) {
		checkCamProcessWorker = pls_new<CheckCamProcessWorker>();
		connect(
			checkCamProcessWorker, &CheckCamProcessWorker::checkFinished, this,
			[this](bool isExisted) { App()->getMainView()->updateSideBarButtonStyle(ConfigId::CamStudioConfig, isExisted); }, Qt::QueuedConnection);
		checkCamProcessWorker->moveToThread(&checkCamThread);
		checkCamThread.start();
		QMetaObject::invokeMethod(checkCamProcessWorker, "CheckCamProcessIsExisted", Qt::QueuedConnection, Q_ARG(const QString &, processName));
	}
}

void PLSBasic::CreatePreviewTitle()
{
	if (previewProgramTitle) {
		return;
	}

	bool studioPortraitLayout = config_get_bool(App()->GetUserConfig(), "BasicWindow", "StudioPortraitLayout");
	previewProgramTitle = pls_new<PLSPreviewProgramTitle>(studioPortraitLayout, ui->previewContainer);
	previewProgramTitle->toggleShowHide(false);
	connect(previewProgramTitle->horApplyBtn, &QPushButton::clicked, this, &PLSBasic::TransitionClicked);
	connect(previewProgramTitle->porApplyBtn, &QPushButton::clicked, this, &PLSBasic::TransitionClicked);
}

void PLSBasic::checkNoticeAndForceUpdate()
{
	
}

void PLSBasic::OnPRISMStickerApplied(const StickerHandleResult &stickerData)
{
	QString log("User apply prism sticker: '%1/%2'");
	PLS_UI_STEP(MAIN_PRISM_STICKER, qUtf8Printable(log.arg(stickerData.data.category).arg(stickerData.data.id)), ACTION_CLICK);
	OBSSource newSource = nullptr;
	AddPRISMStickerSource(stickerData, newSource);
	if (!newSource)
		return;

	/* register undo/redo */
	const char *id = PRISM_STICKER_SOURCE_ID;
	const char *newName = obs_source_get_name(newSource);
	OBSBasic *main = reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
	std::string scene_name = obs_source_get_name(main->GetCurrentSceneSource());
	auto undo = [scene_name, main](const std::string &data) {
		OBSSourceAutoRelease source = obs_get_source_by_name(data.c_str());
		obs_source_remove(source);

		OBSSourceAutoRelease scene_source = obs_get_source_by_name(scene_name.c_str());
		main->SetCurrentScene(scene_source.Get(), true);
	};
	OBSDataAutoRelease wrapper = obs_data_create();
	obs_data_set_string(wrapper, "id", id);
	OBSSceneItemAutoRelease item = obs_scene_sceneitem_from_source(main->GetCurrentScene(), newSource);
	obs_data_set_int(wrapper, "item_id", obs_sceneitem_get_id(item));
	obs_data_set_string(wrapper, "name", newName);
	obs_data_set_bool(wrapper, "visible", true);

	auto redo = [scene_name, main, stickerData](const std::string &redoData) {
		OBSSourceAutoRelease scene_source = obs_get_source_by_name(scene_name.c_str());
		main->SetCurrentScene(scene_source.Get(), true);

		OBSDataAutoRelease dat = obs_data_create_from_json(redoData.c_str());
		OBSSource source;
		PLSBasic::instance()->AddPRISMStickerSource(stickerData, source);
		if (source) {
			OBSSceneItemAutoRelease item = obs_scene_sceneitem_from_source(main->GetCurrentScene(), source);
			obs_sceneitem_set_id(item, (int64_t)obs_data_get_int(dat, "item_id"));
		}
	};
	undo_s.add_action(QTStr("Undo.Add").arg(newName), undo, redo, std::string(obs_source_get_name(newSource)), std::string(obs_data_get_json(wrapper)));
}

static GiphyData GetStickerSourceData(const obs_source_t *source)
{
	obs_data_t *settings = obs_source_get_settings(source);
	const char *id = obs_data_get_string(settings, "id");
	const char *type = obs_data_get_string(settings, "type");
	const char *title = obs_data_get_string(settings, "title");
	const char *rating = obs_data_get_string(settings, "rating");
	auto preview_width = (uint32_t)obs_data_get_int(settings, "preview_width");
	auto preview_height = (uint32_t)obs_data_get_int(settings, "preview_height");
	auto original_width = (uint32_t)obs_data_get_int(settings, "original_width");
	auto original_height = (uint32_t)obs_data_get_int(settings, "original_height");
	const char *preview_url = obs_data_get_string(settings, "preview_url");
	const char *original_url = obs_data_get_string(settings, "original_url");

	GiphyData data;
	data.id = id;
	data.type = type;
	data.title = title;
	data.rating = rating;
	data.previewUrl = preview_url;
	data.originalUrl = original_url;
	data.sizePreview = QSize(preview_width, preview_height);
	data.sizeOriginal = QSize(original_width, original_height);
	return data;
}

static void DownloadSticker(const GiphyData &data, uint64_t randomId)
{
	
}

static void SetFileForStickerSource(obs_source_t *source, const QString &file)
{
	if (file.isEmpty())
		return;

	OBSData oldSettings(obs_data_create());
	obs_data_release(oldSettings);
	OBSData settings = obs_source_get_settings(source);
	obs_data_apply(oldSettings, settings);
	obs_data_release(settings);

	obs_data_set_string(settings, "file", QT_TO_UTF8(file));

	obs_source_update(source, settings);
}

void PLSBasic::CreateStickerDownloader(obs_source_t *source)
{
	auto randomId = (uint64_t)source;
	bool findOut = (stickerSourceMap.find(randomId) != stickerSourceMap.end());
	if (!findOut) {
		stickerSourceMap.insert(std::make_pair(randomId, ""));
		auto data = GetStickerSourceData(source);
		DownloadSticker(data, randomId);
	}
}

void PLSBasic::OnStickerDownloadCallback(const TaskResponData &responData)
{
	if (0 == responData.taskData.randomId)
		return;

	auto randomId = responData.taskData.randomId;
	auto iter = stickerSourceMap.find(randomId);
	if (iter != stickerSourceMap.end()) {
		stickerSourceMap.erase(iter);
		auto setFileForStickerSource = [randomId, responData](obs_source_t *source) {
			if (!source)
				return true;
			if (randomId == (uint64_t)source) {
				SetFileForStickerSource(source, responData.fileName);
				return false;
			}
			return true;
		};

		using SetFileForSticker_t = decltype(setFileForStickerSource);
		auto enumSource = [](void *param, obs_source_t *source) {
			auto ref = (SetFileForSticker_t *)(param);
			if (!ref)
				return false;
			return (*ref)(source);
		};
		obs_enum_sources(enumSource, &setFileForStickerSource);
	}
}

void PLSBasic::InitGiphyStickerViewVisible()
{
	bool showSticker = pls_config_get_bool(App()->GetUserConfig(), ConfigId::GiphyStickersConfig, CONFIG_SHOW_MODE);
	if (showSticker) {
		CreateGiphyStickerView();
		giphyStickerView->show();
	}
}

void PLSBasic::InitPrismStickerViewVisible()
{
	bool showSticker = pls_config_get_bool(App()->GetUserConfig(), ConfigId::PrismStickerConfig, CONFIG_SHOW_MODE);
	if (showSticker) {
		CreatePrismStickerView();
		prismStickerView->show();
		prismStickerView->raise();
	}
}

void PLSBasic::InitDualOutputEnabled()
{
	bool bEnabled = pls_config_get_bool(App()->GetUserConfig(), ConfigId::DualOutputConfig, CONFIG_SHOW_MODE);
	if (bEnabled) {
		if (!setDualOutputEnabled(bEnabled, false)) {
			const char *objName = QMetaEnum::fromType<ConfigId>().valueToKey(ConfigId::DualOutputConfig);
			config_set_bool(App()->GetUserConfig(), objName, "showMode", false);
			getMainView()->updateSideBarButtonStyle(ConfigId::DualOutputConfig, false);
		}

		connect(PLSCHANNELS_API, &PLSChannelDataAPI::toDoinitialize, dualOutputTitle.data(), &PLSDualOutputTitle::initPlatformIcon,
			Qt::ConnectionType(Qt::QueuedConnection | Qt::SingleShotConnection));
	}

	connect(PLS_PLATFORM_API, &PLSPlatformApi::liveEnded, this, std::bind(&PLSBasic::changeOutputCount, this, -1));
	connect(this, &PLSBasic::sigOutputActiveChanged, this, [this](bool bValue) {
		getMainView()->setSidebarButtonEnabled(ConfigId::DualOutputConfig, !bValue);
		getMainView()->updateTipsEnableChanged(!bValue);
	});
}

void PLSBasic::OnMixerOrderChanged()
{
	do {
		auto currentPage = ui->stackedMixerArea->currentWidget();
		auto scrollarea = qobject_cast<QScrollArea *>(currentPage);
		if (!scrollarea)
			break;

		if (!scrollarea->widget())
			break;

		QBoxLayout *layout = qobject_cast<QBoxLayout *>(scrollarea->widget()->layout());
		if (!layout)
			break;

		MixerList order;
		for (int i = 0; i < layout->count(); i++) {
			SourceObject obj;
			QLayoutItem *item = layout->itemAt(i);
			if (!item)
				continue;
			auto vol = qobject_cast<VolControl *>(item->widget());
			if (vol) {
				obj.name = obs_source_get_name(vol->GetSource());
				obj.uuid = obs_source_get_uuid(vol->GetSource());
				order.emplace_back(obj);
			}
		}
		mixerOrder.UpdateOrder(obs_source_get_uuid(GetCurrentSceneSource()), std::move(order));
	} while (false);
}

void PLSBasic::initNcb2bBrowserSettingsVisible()
{
	QString userServiceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
	if (userServiceName.isEmpty()) {
		return;
	}

	auto mainView = PLSBasic::instance()->getMainView();
	mainView->setSidebarButtonVisible(ConfigId::Ncb2bBrowserSettings, true);
	mainView->updateSidebarButtonTips(ConfigId::Ncb2bBrowserSettings, QTStr("Ncpb2b.Browser.Settings.Tooltip").arg(userServiceName));

	createNcb2bBrowserSettings();

	bool show = pls_config_get_bool(App()->GetUserConfig(), ConfigId::Ncb2bBrowserSettings, CONFIG_SHOW_MODE);
	if (show) {
		ncb2bBrowserSettings->show();
		ncb2bBrowserSettings->raise();
	}
}

void PLSBasic::on_actionShowAbout_triggered()
{
	PLSAboutView aboutView(this);
	if (aboutView.exec() == PLSAboutView::Accepted) {
		QMetaObject::invokeMethod(this, [this]() { on_checkUpdate_triggered(); }, Qt::QueuedConnection);
	}
}

void PLSBasic::on_actionHelpPortal_triggered()
{
	QDesktopServices::openUrl(QUrl(QString("http://prismlive.com/%1/faq/faq.html?app=pcapp").arg(getSupportLanguage()), QUrl::TolerantMode));
}

void PLSBasic::on_actionHelpOpenSource_triggered()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Main Menu Help Open Source License", ACTION_CLICK);
	PLSOpenSourceView view(this);
	view.exec();
}

void PLSBasic::on_actionDiscord_triggered() const
{
	QDesktopServices::openUrl(QUrl(PLSSyncServerManager::instance()->getDiscordUrl(), QUrl::TolerantMode));
}

void PLSBasic::on_actionWebsite_triggered() const
{
	if (IS_KR()) {
		QDesktopServices::openUrl(QUrl(QString(""), QUrl::TolerantMode));
	} else {
		QDesktopServices::openUrl(QUrl(QString(""), QUrl::TolerantMode));
	}
}

void PLSBasic::on_actionContactUs_triggered(const QString &message, const QString &additionalMessage)
{
	PLSContactView view(message, additionalMessage, this);
	view.exec();
}

void PLSBasic::on_actionRepair_triggered() const
{
	QUrl url = QUrl("", QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
}

void PLSBasic::on_actionUserGuide_triggered() const {}


void PLSBasic::on_stats_triggered()
{
	toggleStatusPanel(-1);
}

void PLSBasic::on_actionStudioMode_triggered()
{
	if (sender() == ui->actionStudioMode) {
		PLS_UI_STEP(MAINMENU_MODULE, "Main Menu View Studio Mode", ACTION_CLICK);
	}

	bool isStudioMode = IsPreviewProgramMode();
	if (isStudioMode) {
		PLS_UI_STEP(MAINFRAME_MODULE, "Exit studio mode", ACTION_CLICK);
	} else {
		PLS_UI_STEP(MAINFRAME_MODULE, "Enter studio mode", ACTION_CLICK);
	}
	SetPreviewProgramMode(!isStudioMode);
}

void PLSBasic::on_actionShowVideosFolder_triggered() const
{
	const char *mode = config_get_string(activeConfiguration, "Output", "Mode");
	const char *type = config_get_string(activeConfiguration, "AdvOut", "RecType");
	const char *adv_path = strcmp(type, "Standard") ? config_get_string(activeConfiguration, "AdvOut", "FFFilePath") : config_get_string(activeConfiguration, "AdvOut", "RecFilePath");
	const char *path = strcmp(mode, "Advanced") ? config_get_string(activeConfiguration, "SimpleOutput", "FilePath") : adv_path;
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void PLSBasic::on_actionPrismPolicy_triggered() const
{
	if (!strcmp(App()->GetLocale(), "ko-KR")) {
		QDesktopServices::openUrl(QUrl("", QUrl::TolerantMode));
	} else {
		QDesktopServices::openUrl(QUrl("", QUrl::TolerantMode));
	}
}

void PLSBasic::on_checkUpdate_triggered()
{
}

void PLSBasic::actionLaboratory_triggered()
{
	if (g_laboratoryDialog) {
		g_laboratoryDialog->show();
		return;
	}

	g_laboratoryDialog = pls_new<PLSLaboratory>(this);
	connect(g_laboratoryDialog, &PLSLaboratory::destroyed, this, []() { g_laboratoryDialog = nullptr; });
	g_laboratoryDialog->setObjectName("PLSLaboratory");
	g_laboratoryDialog->show();
}

void PLSBasic::OnSideBarButtonClicked(int buttonId)
{
	const char *configName = QMetaEnum::fromType<ConfigId>().valueToKey(buttonId);
	if (!configName || !*configName)
		return;

	switch (buttonId) {
	case ConfigId::VirtualCameraConfig:
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar VirtualCamera Button", ACTION_CLICK);
		VirtualCamActionTriggered();
		break;
	case ConfigId::BeautyConfig:
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Beauty Button", ACTION_CLICK);
		//OnBeautyClicked();
		break;
	case ConfigId::GiphyStickersConfig:
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Stickers Button", ACTION_CLICK);
		OnStickerClicked();
		break;
	case ConfigId::BgmConfig:
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView SideBar Music Playlist Button", ACTION_CLICK);
		OnBgmClicked();
		break;
	case ConfigId::LivingMsgView:
		mainView->on_alert_clicked();
		break;
	case ConfigId::ChatConfig:
		mainView->on_chat_clicked();
		break;
	case ConfigId::VirtualbackgroundConfig:
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView sidebar virtualbackground button", ACTION_CLICK);
		//OnVirtualBgClicked();
		break;
	case ConfigId::PrismStickerConfig:
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView sidebar prism sticker button", ACTION_CLICK);
		OnPrismStickerClicked();
		break;
	case ConfigId::DrawPenConfig:
		PLS_LOGEX(PLS_LOG_INFO, DRAWPEN_MODULE, {{"X-DP-USE-COUNT", "DrawPenUse"}}, "Draw Pen Side Bar Clicked");
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView sidebar Drawpen button", ACTION_CLICK);
		OnDrawPenClicked();
		break;
	case ConfigId::RemoteControlConfig:
		PLS_UI_STEP(MAINFRAME_MODULE, "PLSMainView sidebar remote control button", ACTION_CLICK);
		break;
	case ConfigId::CamStudioConfig:
		OnCamStudioClicked({"--display_control=top"}, this);
		break;
	case ConfigId::Ncb2bBrowserSettings:
		onBrowserSettingsClicked();
		break;
	case ConfigId::SceneTemplateConfig:
		OnSceneTemplateClicked(ShowType::ST_Switch);
		break;
	case ConfigId::DualOutputConfig:
		onDualOutputClicked();
		break;
	default:
		break;
	}
}

void PLSBasic::OnTransitionAdded()
{
	OnEvent(OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED);
}

void PLSBasic::onTransitionRemoved(OBSSource source)
{
	OnEvent(OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED);
	this->DeletePropertiesWindow(source);
}

void PLSBasic::OnTransitionRenamed()
{
	OnEvent(OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED);
}

void PLSBasic::OnTransitionSet()
{
	OnEvent(OBS_FRONTEND_EVENT_TRANSITION_CHANGED);
}

void PLSBasic::OnTransitionDurationValueChanged(int value)
{
	if (api)
		api->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_TRANSITION_DURATION_CHANGED, {value});
}

void PLSBasic::addActionPasteMenu()
{
	//#4189 by zengqin Disable the reference paste group option in the same scene
	//OBSSource source = obs_get_source_by_name(copyString.toStdString().c_str());
	//if (source) {
	//	if (obs_source_is_group(source)) {
	//		if (!!obs_scene_get_group(GetCurrentScene(), copyString.toStdString().c_str())) {
	//			ui->actionPasteRef->setEnabled(false);
	//		} else
	//			ui->actionPasteRef->setEnabled(true);
	//	}
	//	obs_source_release(source);
	//} else {
	//	ui->actionPasteRef->setEnabled(false);
	//	ui->actionPasteDup->setEnabled(false);
	//}
}
PLSSceneItemView *PLSBasic::GetCurrentSceneItemView()
{
	return ui->scenesFrame->GetCurrentItem();
}

void PLSBasic::setUserIcon() const
{
	QString iconPath = pls_prism_user_thumbnail_path();
	QPixmap icon;
	if (!QFileInfo::exists(iconPath) || !pls_get_prism_user_thumbnail(icon)) {
		PLS_WARN(PLS_LOGIN_MODULE, "use prism default thumbnail");
		iconPath = ":/resource/images/img-setting-profile-blank.svg";
		icon.load(iconPath);
		loadPixmap(icon, iconPath, QSize(26 * 4, 26 * 4));
	}

	pls_shared_circle_mask_image(icon);

	pls_set_main_view_side_bar_user_button_icon(QIcon(icon));
}

void PLSBasic::initConnect()
{
	// scene action
	auto actionExport = pls_new<QAction>(ui->scenesFrame);
	actionExport->setObjectName(OBJECT_NMAE_EXPORT_BUTTON);

	actionExport->setText(QTStr("Scene.Collection.Export"));
	auto actionAdd = pls_new<QAction>(ui->scenesFrame);

	actionAdd->setObjectName(OBJECT_NMAE_ADD_BUTTON);
	auto actionSwitchEffect = pls_new<QAction>(ui->scenesFrame);
	actionSwitchEffect->setObjectName(OBJECT_NMAE_SWITCH_EFFECT_BUTTON);

	actionSeperateScene = pls_new<QAction>(ui->scenesFrame);
	SetAttachWindowBtnText(actionSeperateScene, ui->scenesDock->isFloating());
	actionSeperateScene->setObjectName("detachBtn");

	connect(actionExport, &QAction::triggered, this, &PLSBasic::OnExportSceneCollectionClicked);
	connect(actionAdd, &QAction::triggered, this, &OBSBasic::on_actionAddScene_triggered);
	connect(actionSwitchEffect, &QAction::triggered, ui->scenesFrame, &PLSSceneListView::OnSceneSwitchEffectBtnClicked);

	ui->scenesDock->titleWidget()->setAdvButtonActions({actionSeperateScene, actionExport});
	auto displayMethodMenu = CreateSceneDisplayMenu();

	ui->scenesDock->titleWidget()->addAdvButtonMenu(displayMethodMenu);
	ui->scenesDock->titleWidget()->setTitleWidgets({sceneCollectionManageTitle});
	ui->scenesDock->titleWidget()->setButtonActions({actionAdd, actionSwitchEffect});
	connect(ui->scenesDock, &QDockWidget::topLevelChanged, this, &PLSBasic::OnSceneDockTopLevelChanged);
	connect(actionSeperateScene, &QAction::triggered, this, &PLSBasic::OnSceneDockSeperatedBtnClicked);

	// source action
	auto actionAddSource = pls_new<QAction>(ui->sources);
	actionAddSource->setObjectName(OBJECT_NMAE_ADD_SOURCE_BUTTON);
	actionSeperateSource = pls_new<QAction>(ui->sources);
	actionSeperateSource->setObjectName("detachBtn");
	SetAttachWindowBtnText(actionSeperateSource, ui->sourcesDock->isFloating());

	connect(actionAddSource, &QAction::triggered, this, &OBSBasic::on_actionAddSource_triggered);
	connect(actionSeperateSource, &QAction::triggered, this, &PLSBasic::OnSourceDockSeperatedBtnClicked);

	ui->sourcesDock->titleWidget()->setAdvButtonActions({actionSeperateSource});
	ui->sourcesDock->titleWidget()->setButtonActions({actionAddSource});
	connect(ui->sourcesDock, &QDockWidget::topLevelChanged, this, &PLSBasic::OnSourceDockTopLevelChanged);

	// chat dock
	actionSeperateChat = pls_new<QAction>(ui->chatDock);
	actionSeperateChat->setObjectName("detachBtn");
	connect(actionSeperateChat, &QAction::triggered, this, &PLSBasic::OnChatDockSeperatedBtnClicked);
	bool isFloating = ui->chatDock->isFloating();
	SetAttachWindowBtnText(actionSeperateChat, isFloating);
	ui->chatDock->titleWidget()->setAdvButtonActions({actionSeperateChat});
	ui->chatDock->titleWidget()->setHasCloseButton(true);
	ui->chatDock->titleWidget()->setCloseButtonVisible(true);
	ui->chatDock->setAttribute(Qt::WA_NativeWindow);
	connect(ui->chatDock, &QDockWidget::topLevelChanged, this, &PLSBasic::OnChatDockTopLevelChanged);
}

void PLSBasic::OnExportSceneCollectionClicked()
{
	const char *name = config_get_string(App()->GetUserConfig(), "Basic", "SceneCollection");
	const char *fileName = config_get_string(App()->GetUserConfig(), "Basic", "SceneCollectionFile");

	on_actionExportSceneCollection_triggered_with_path(name, fileName, this);
}

void PLSBasic::StartUpdateSceneCollectionTimeTimer()
{
	updateSceneCollectionTimeTimer.setInterval(10000);
	connect(&updateSceneCollectionTimeTimer, &QTimer::timeout, this, &PLSBasic::OnUpdateSceneCollectionTimeTimerTriggered);
	updateSceneCollectionTimeTimer.start();
}

void PLSBasic::ShowLoadSceneCollectionError()
{
	if (showLoadSceneCollectionError) {
		PLSAlertView::warning(this, QTStr("Alert.title"), showLoadSceneCollectionErrorStr);
		showLoadSceneCollectionError = false;
		showLoadSceneCollectionErrorStr = "";
	}
}

void OBSBasic::checkSceneDisplayMethod()
{
	auto basic = PLSBasic::instance();
	if (!basic) {
		return;
	}

	//When the number of scenes is greater than 10
	if (PLSSceneDataMgr::Instance()->GetSceneSize() > SCENE_RENDER_NUMBER) {
		if (alreadyShowSceneMethodAlert) {
			return;
		}

		if (basic->GetSceneDisplayMethod() != static_cast<int>(DisplayMethod::DynamicRealtimeView)) {
			return;
		}
		PLS_INFO(MAINFRAME_MODULE, "Scene display mode automatically switches from real-time mode to thumbnail mode.");
		basic->SetSceneDisplayMethod(static_cast<int>(DisplayMethod::ThumbnailView));

		alreadyShowSceneMethodAlert = true;
		if (loadingScene) {
			// alert need to be shown after mainview is visible
			deferShowSceneMethodAlert = true;
		} else {
			PLSAlertView::information(this, QTStr("Alert.Title"), QTStr("Scene.Realtime.Limit.Tips"));
		}

		return;
	}

	//When the number of scenes is less than or equal to 10
	alreadyShowSceneMethodAlert = false;
}

void PLSBasic::showChangeSceneDisplayAlert()
{
	if (deferShowSceneMethodAlert) {
		deferShowSceneMethodAlert = false;
		PLSAlertView::information(this, QTStr("Alert.Title"), QTStr("Scene.Realtime.Limit.Tips"));
	}
}

void PLSBasic::SetSceneDisplayMethod(int method)
{
	ui->scenesFrame->SetSceneDisplayMethod(method);

	auto config = App()->GetUserConfig();
	config_set_int(config, "BasicWindow", "SceneDisplayMethod", method);
	config_save(config);

	auto curMethod = static_cast<DisplayMethod>(method);
	PLS_INFO(MAINSCENE_MODULE, "current scene display method : %s", ui->scenesFrame->getSceneDisplayMethodStr(curMethod));
}

int PLSBasic::GetSceneDisplayMethod() const
{
	if (!config_has_user_value(App()->GetUserConfig(), "BasicWindow", "SceneDisplayMethod")) {
		config_set_int(App()->GetUserConfig(), "BasicWindow", "SceneDisplayMethod", 0);
		return 0;
	}
	int displayMethod = (int)config_get_int(App()->GetUserConfig(), "BasicWindow", "SceneDisplayMethod");
	if (displayMethod < 0 || displayMethod > static_cast<int>(DisplayMethod::TextView)) {
		return 0;
	}
	return displayMethod;
}

QMenu *PLSBasic::CreateSceneDisplayMenu()
{
	auto displayMethodMenu = pls_new<QMenu>(QTStr("Setting.Scene.Display.Method"), ui->scenesFrame);
	displayMethodMenu->setObjectName("displayMethodMenu");
	connect(displayMethodMenu, &QMenu::aboutToShow, this, [this]() {
		int displayMethod = GetSceneDisplayMethod();
		actionRealTime->setChecked(0 == displayMethod);
		actionRealTime->setEnabled(PLSSceneDataMgr::Instance()->GetSceneSize() <= SCENE_RENDER_NUMBER);
		actionThumbnail->setChecked(1 == displayMethod);
		actionText->setChecked(2 == displayMethod);
	});

	actionRealTime = pls_new<QAction>(QTStr("Setting.Scene.Display.Realtime.View"), ui->scenesFrame);
	actionRealTime->setCheckable(true);

	actionThumbnail = pls_new<QAction>(QTStr("Setting.Scene.Display.Thumbnail.View"), ui->scenesFrame);
	actionThumbnail->setCheckable(true);

	actionText = pls_new<QAction>(QTStr("Setting.Scene.Display.Text.View"), ui->scenesFrame);
	actionText->setCheckable(true);

	connect(actionRealTime, &QAction::triggered, this, [this]() { SetSceneDisplayMethod(0); });
	connect(actionThumbnail, &QAction::triggered, this, [this]() { SetSceneDisplayMethod(1); });
	connect(actionText, &QAction::triggered, this, [this]() { SetSceneDisplayMethod(2); });

	displayMethodMenu->addAction(actionRealTime);
	displayMethodMenu->addAction(actionThumbnail);
	displayMethodMenu->addAction(actionText);
	return displayMethodMenu;
}

void PLSBasic::SetDpi(float dpi)
{
	ui->scenesFrame->SetDpi(dpi);
}

QString PLSBasic::getStreamState() const
{
	int streamState;
	if (PLSCHANNELS_API) {
		streamState = PLSCHANNELS_API->currentBroadcastState();
	} else {
		streamState = ChannelData::ReadyState;
	}

	return getStreamStateString(streamState);
}

QString PLSBasic::getRecordState() const
{
	int recordState;
	if (PLSCHANNELS_API) {
		recordState = PLSCHANNELS_API->currentReocrdState();
	} else {
		recordState = ChannelData::RecordReady;
	}

	return getRecordStateString(recordState);
}

int PLSBasic::getRecordDuration() const
{
	return mainView->statusBar()->getRecordDuration();
}

static inline bool SourceMixerHidden(const obs_source_t *source)
{
	PLS_INFO(AUDIO_MIXER, "SourceMixerHidden");

	obs_data_t *priv_settings = obs_source_get_private_settings(const_cast<obs_source *>(source));
	bool hidden = obs_data_get_bool(priv_settings, "mixer_hidden");
	obs_data_release(priv_settings);

	return hidden;
}
static inline bool IncludeHidenAudio()
{
	auto enum_proc = [](void *param, obs_source_t *source) {
		if (param && source && SourceMixerHidden(source)) {
			auto existHiden = static_cast<bool *>(param);
			*existHiden = true;
			return false;
		}
		return true;
	};

	bool existHidenAudio = false;
	obs_enum_sources(enum_proc, &existHidenAudio);
	return existHidenAudio;
}

static inline void includeHidenAudioToast(bool live)
{
	if (live && config_get_bool(App()->GetUserConfig(), "BasicWindow", "RecordWhenStreaming")) {
		return;
	}
	if (IncludeHidenAudio()) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("hide.audio.toast"));
		PLS_INFO(NOTICE_MODULE, "The Audio Mixer has an audio project set to 'Hide'.");
	}
}

static void notifySourceLiveEnd()
{
	// notify chat source
	QJsonObject json;
	json.insert("type", "liveEnd");

	QByteArray jsonString = QJsonDocument(json).toJson(QJsonDocument::Compact);

	PLS_INFO("Chat-Source", "chatEvent: %s", jsonString.constData());
	pls_frontend_call_dispatch_js_event_cb("chatEvent", jsonString.constData());

	// notify viewer count source
	PLS_INFO("ViewCount-Source", "viewerCountEvent: %s", jsonString.constData());
	pls_frontend_call_dispatch_js_event_cb("viewerCountEvent", jsonString.constData());
}

void PLSBasic::frontendEventHandler(obs_frontend_event event, void *private_data)
{
	auto main = (PLSBasic *)private_data;
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING: {
		QStringList activedChatChannels = getActivedChatChannels();
		if (!activedChatChannels.isEmpty()) { // contains chat channel
			activedChatChannels.erase(std::remove_if(activedChatChannels.begin(), activedChatChannels.end(), [](const QString &s) { return s == NCB2B; }), activedChatChannels.end());
			if (!hasChatSource() && !activedChatChannels.isEmpty()) { // no chat source added
				pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, tr("Chat.Toast.NoChatSource.WhenStreaming").arg(activedChatChannels.join(',')));
			}
		} else {                       // not contains chat channel
			if (hasChatSource()) { // chat source added
				pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, tr("Chat.Toast.ChatSource.NoSupportChannel.WhenStreaming"));
			}
		}

		if (pls_frontend_get_dispatch_js_event_cb()) {
			// notify chat source
			int seqHorizontal;
			int seqVertical;
			pls_get_prism_live_seq(seqHorizontal, seqVertical);

			QJsonArray videoSeqs;
			videoSeqs.append(seqHorizontal);
			if (seqVertical > 0) {
				videoSeqs.append(seqVertical);
			}

			QByteArray settingJson = pls::JsonDocument<QJsonObject>()
							 .add("type", "setting")
							 .add("data", pls::JsonDocument<QJsonObject>()
									      .add("singlePlatform", getActivedChatChannelCount() == 1)
									      .add("videoSeq", seqHorizontal)
									      .add("videoSeqs", videoSeqs)
									      .add("NEO_SES", QString::fromUtf8(pls_get_prism_cookie_value()))
									      .object())
							 .toByteArray();
			PLS_INFO("Chat-Source", "chatEvent: %s", settingJson.constData());
			pls_frontend_call_dispatch_js_event_cb("chatEvent", settingJson.constData());

			// notify chat source
			QByteArray liveStartJson = pls::JsonDocument<QJsonObject>().add("type", "liveStart").toByteArray();
			PLS_INFO("Chat-Source", "chatEvent: %s", liveStartJson.constData());
			pls_frontend_call_dispatch_js_event_cb("chatEvent", liveStartJson.constData());

			// notify viewer count source
			PLS_INFO("ViewCount-Source", "viewerCountEvent: %s", liveStartJson.constData());
			pls_frontend_call_dispatch_js_event_cb("viewerCountEvent", liveStartJson.constData());
		}
	} break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		includeHidenAudioToast(false);
		runtime_stats(PLS_RUNTIME_STATS_TYPE_RECORD_START, std::chrono::steady_clock::now());
		if (main->hasSourceMonitor() && !main->isAudioMonitorToastDisplay) {
			main->isAudioMonitorToastDisplay = true;
			pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("living.rec.has.monitor"));
		}
		emit main->outputStateChanged();
		std::thread([]() { PLSInfoCollector::logMsg("Start Recording"); }).detach();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		runtime_stats(PLS_RUNTIME_STATS_TYPE_LIVE_END, std::chrono::steady_clock::now());
		notifySourceLiveEnd();
		emit main->outputStateChanged();
		std::thread([]() { PLSInfoCollector::logMsg("Stop Streaming"); }).detach();
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		main->ui->scenesFrame->RefreshSceneThumbnail();
		break;
	case OBS_FRONTEND_EVENT_FINISHED_LOADING: {
		/*modified by xie-wei #3946*/
		main->CheckStickerSource();
		//auto widget = PLSBasic::Get()->findChild<QDialog *>("SettingsDialog");
		//if (widget) {
		//	widget->setStyleSheet("SettingsDialog QPushButton {min-height: /*hdpi*/ 20px;max-height: /*hdpi*/ 20px;padding: 0;margin: 0;background: red;}");
		//}
		break;
	}
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		main->CheckStickerSource();
		updatePlatformViewerCount();
		std::thread([]() { PLSInfoCollector::logMsg("Scene Collection Changed"); }).detach();
		config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
		PLSSceneitemMapMgrInstance->switchToDualOutputModeForAllScenes();
		PLSSceneitemMapMgrInstance->bindMappedVerticalItemHotkeys();
		main->showChangeSceneDisplayAlert();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		includeHidenAudioToast(true);
		runtime_stats(PLS_RUNTIME_STATS_TYPE_LIVE_START, std::chrono::steady_clock::now());
		emit main->outputStateChanged();
		std::thread([]() { PLSInfoCollector::logMsg("Start Streaming"); }).detach();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		runtime_stats(PLS_RUNTIME_STATS_TYPE_RECORD_END, std::chrono::steady_clock::now());
		emit main->outputStateChanged();
		std::thread([]() { PLSInfoCollector::logMsg("Stop Recording"); }).detach();
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		emit main->outputStateChanged();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		PLSSceneitemMapMgrInstance->switchToDualOutputMode();
		main->updateSourceIcon();
		pls_async_call_mt([main]() { main->ReorderAudioMixer(); });
		break;
	default:
		break;
	}
}
void PLSBasic::frontendEventHandler(pls_frontend_event event, const QVariantList &, void *context)
{
	auto main = (PLSBasic *)context;
	auto basicMain = dynamic_cast<OBSBasic *>(main);
	switch (event) {
	case pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_START:
		if (!basicMain->outputHandler->ReplayBufferActive()) {
			PLS_INFO(HOTKEYS_MODULE, "Auto start replay buffer on stream or record started");
			//TODO reply buffer
			//main->StartReplayBufferWithNoCheck();
		}
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_LIVE_OR_RECORD_END:

		main->isAudioMonitorToastDisplay = false;
		if (!pls_is_living_or_recording() && main->outputHandler->ReplayBufferActive()) {
			PLS_INFO(HOTKEYS_MODULE, "Auto save replay buffer on stream or record stopped");
			main->StopReplayBuffer();
		}
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_STREAMING_START_FAILED:
		notifySourceLiveEnd();
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_REHEARSAL_SWITCH_TO_LIVE:
		main->rehearsalSwitchedToLive();
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_SCENE_COLLECTION_ABOUT_TO_CHANGED:
		main->HideAllInteraction(nullptr);
		break;
	case pls_frontend_event::PLS_FRONTEND_EVENT_DUAL_OUTPUT_ON:
	case pls_frontend_event::PLS_FRONTEND_EVENT_DUAL_OUTPUT_OFF:
		main->updateSourceIcon();
		if (pls_frontend_event::PLS_FRONTEND_EVENT_DUAL_OUTPUT_OFF == event) {
			main->ForceUpdateGroupsSize();
		}
		break;
	default:
		break;
	}
}
void PLSBasic::rehearsalSwitchedToLive()
{
	updateLiveStartUI();

	PLS_PLATFORM_API->doStartGpopMaxTimeLiveTimer();
}

void PLSBasic::showMainViewAfter(QWidget *parentWidget)
{
	pls_check_app_exiting();
	//notice view
	PLS_INFO(MAINFRAME_MODULE, "start request prism notice info.");
	pls_get_new_notice_Info([this, parentWidget](const QVariantMap &noticeInfo) {
		bool updateNow = false;
		pls_async_call(this, [this, noticeInfo]() {
			if (noticeInfo.size()) {
				PLS_INFO(MAINFRAME_MODULE, "show notice");
				PLSNoticeView view(noticeInfo.value(NOTICE_CONTENE).toString(), noticeInfo.value(NOTICE_TITLE).toString(), noticeInfo.value(NOTICE_DETAIL_LINK).toString(), mainView);
#if defined(Q_OS_MACOS)
				view.setWindowTitle(tr("Mac.Title.Notice"));
#endif
				view.exec();
			}
		});

		// if update view never shown

		if (!updateNow) {

			if (PLSLoginDataHandler::instance()->isNeedShowB2BServiceAlert()) {
				pls_async_call_mt([]() { PLSAlertView::warning(nullptr, QObject::tr("Alert.Title"), QObject::tr("Ncb2b.Living.Service.Disable")); });
			}
		}
	});
}

void PLSBasic::LogoutCallback(pls_frontend_event event, const QVariantList &l, void *v)
{
	if (event != pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT)
		return;

	pls_unused(l, v);
	bool defaultPreviewMode = true;
	config_set_bool(App()->GetUserConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_PREVIEW_MODE_MODULE, defaultPreviewMode);
	dynamic_cast<PLSBasic *>(App()->GetMainWindow())->SetPreviewProgramMode(defaultPreviewMode);
}

bool enum_adapter_callback(void *, const char *name, uint32_t id)
{

	if (name && config_get_uint(App()->GetUserConfig(), "Video", "AdapterIdx") == id) {
		GlobalVars::videoAdapter = name;
		return false;
	}
	return true;
}

void PLSBasic::graphicsCardNotice() const
{
	if (mainView->isHidden()) {
		return;
	}

	obs_enter_graphics();
	gs_enum_adapters(enum_adapter_callback, nullptr);
	obs_leave_graphics();

	auto gpuBlacklist = PLSSyncServerManager::instance()->getGPUBlacklist("GpuModels.json");

	bool foundGpu = false;
	auto finder = [](const std::string &name) { return GlobalVars::videoAdapter.compare(name) == 0; };
	if (std::find_if(gpuBlacklist.begin(), gpuBlacklist.end(), finder) != gpuBlacklist.end())
		foundGpu = true;

	OBSBasic *main = OBSBasic::Get();
	if (main && foundGpu) {
		main->SysTrayNotify(QTStr(BLACKLIST_GRAPHICS_CARD_LOWER_REQUIREMENT), QSystemTrayIcon::Warning);
		PLS_LOGEX(PLS_LOG_WARN, NOTICE_MODULE, {{"unsupportedGraphicsCard", GlobalVars::videoAdapter.c_str()}},
			  "Graphics card does not meet the minimum system requirements supported by PRISM.");
	}
}

void PLSBasic::UpdateAudioMixerMenuTxt()
{
	bool vertical = config_get_bool(App()->GetUserConfig(), "BasicWindow", "VerticalVolControl");
	if (actionMixerLayout) {
		actionMixerLayout->setText(vertical ? QTStr("Basic.MainMenu.Mixer.Horizontal") : QTStr("Basic.MainMenu.Mixer.Vertical"));
	}
}

static bool EnumSourceItemForDelScene(obs_scene_t *t, obs_sceneitem_t *item, void *ptr)
{
	pls_unused(t);
	std::vector<obs_source_t *> &sources = *static_cast<std::vector<obs_source_t *> *>(ptr);
	if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, EnumSourceItemForDelScene, ptr);
	} else {
		obs_source_t *source = obs_sceneitem_get_source(item);
		if (source) {
			sources.push_back(source);
		}
	}

	return true;
}

QWidget *OBSBasic::GetPropertiesWindow()
{
	return properties;
}

void OBSBasic::DeletePropertiesWindowInScene(obs_scene_t *scene) const
{
	std::vector<obs_source_t *> sources{};
	obs_scene_enum_items(scene, EnumSourceItemForDelScene, &sources);

	for (auto item : sources) {
		DeletePropertiesWindow(item);
	}
}

void OBSBasic::DeletePropertiesWindow(obs_source_t *source) const
{
	if (properties && source == properties->GetSource()) {
		properties->close();
	}

	//if (virtualBackgroundChromaKey &&
	//    source == virtualBackgroundChromaKey->GetSource()) {
	//	virtualBackgroundChromaKey->OnButtonBoxCancelClicked(source);
	//}
}

void OBSBasic::DeleteFiltersWindowInScene(obs_scene_t *scene) const
{
	std::vector<obs_source_t *> sources{};
	obs_scene_enum_items(scene, EnumSourceItemForDelScene, &sources);

	for (auto item : sources) {
		DeleteFiltersWindow(item);
	}
}

void OBSBasic::DeleteFiltersWindow(const obs_source_t *source) const
{
	if (!filters) {
		return;
	}

	if (source == filters->GetSource()) {
		filters->close();
	}
}

void OBSBasic::loadProfile(const char *savePath, const char *sceneCollection, LoadSceneCollectionWay way)
{
	ProfileScope("OBSBasic::loadProfile");
	if (QString runningPath = App()->getAppRunningPath(); !runningPath.isEmpty()) {
		if (bool fromUserPath = CheckPscFileInPrismUserPath(runningPath); fromUserPath) {
			disableSaving--;
			Load(runningPath.toUtf8().constData());
			disableSaving++;
			return;
		}
		//check existed
		PLSSceneCollectionData data = GetSceneCollectionDataWithUserLocalPath(runningPath);
		if (!data.userLocalPath.isEmpty()) {
			on_actionChangeSceneCollection_triggered(data.fileName, data.filePath, false);
			return;
		}

		// not existed but same content
		QString importName;
		obs_data_t *jsonData = obs_data_create_from_json_file_safe(runningPath.toUtf8().constData(), "bak");
		if (jsonData) {
			importName = obs_data_get_string(jsonData, "name");
			if (CheckSameSceneCollection(importName, runningPath)) {
				AddCollectionUserLocalPath(importName, runningPath);
				on_actionChangeSceneCollection_triggered(importName, GetSceneCollectionPathByName(importName), false);
				return;
			}
		}

		QString path = ImportSceneCollection(this, runningPath, way);
		if (!path.isEmpty()) {
			return;
		}
		if (LoadSceneCollectionWay::RunPscWhenPrismExisted == way) {
			return;
		}
	}

	disableSaving--;
	//std::string strPath(savePath);
	//strPath += sceneCollection;
	//strPath += ".json";
	Load(savePath);
	disableSaving++;
}

void PLSBasic::OnSceneDockTopLevelChanged()
{
	SetAttachWindowBtnText(actionSeperateScene, ui->scenesDock->isFloating());
}

void PLSBasic::OnSourceDockTopLevelChanged()
{
	SetAttachWindowBtnText(actionSeperateSource, ui->sourcesDock->isFloating());
}

void PLSBasic::OnChatDockTopLevelChanged()
{
	bool isFloating = ui->chatDock->isFloating();
	SetAttachWindowBtnText(actionSeperateChat, isFloating);
	ui->chatDock->titleWidget()->setCloseButtonVisible(true);
}

void PLSBasic::OnBrowserDockTopLevelChanged()
{
	BrowserDock *dock = reinterpret_cast<BrowserDock *>(sender());
	if (!dock) {
		return;
	}

	QString uuid = dock->property("uuid").toString();

	QList<QAction *> actions = dock->titleWidget()->GetAdvButtonActions();
	for (int i = 0; i < actions.size(); i++) {
		QAction *action = actions[i];
		if (!action) {
			continue;
		}
		if (0 == action->property("uuid").toString().compare(uuid)) {
			bool isFloating = dock->isFloating();
			SetAttachWindowBtnText(action, isFloating);
			dock->titleWidget()->setCloseButtonVisible(true);
			return;
		}
	}
}

void PLSBasic::CreateAdvancedButtonForBrowserDock(OBSDock *dock, const QString &uuid, bool bDetach)
{
	if (!dock) {
		return;
	}

	bool isBrowerDock = dynamic_cast<BrowserDock *>(dock) != nullptr;

	QAction *actionSeperateSource = pls_new<QAction>(this);
	actionSeperateSource->setObjectName("detachBtn");
	actionSeperateSource->setProperty("uuid", uuid);
	actionSeperateSource->setEnabled(!ui->lockDocks->isChecked());
	PLSBasic::instance()->SetAttachWindowBtnText(actionSeperateSource, dock->isFloating());
	if (bDetach) {
		PLSBasic::instance()->SetAttachWindowBtnText(actionSeperateSource, false);
	}
	connect(actionSeperateSource, &QAction::triggered, PLSBasic::instance(), &PLSBasic::OnBrowserDockSeperatedBtnClicked);
	dock->titleWidget()->setHasCloseButton(true);
	dock->titleWidget()->setCloseButtonVisible(isBrowerDock ? true : dock->isFloating());
	dock->titleWidget()->setAdvButtonActions({actionSeperateSource});
	connect(dock, &QDockWidget::topLevelChanged, PLSBasic::instance(), &PLSBasic::OnBrowserDockTopLevelChanged);
}

void PLSBasic::setDockDetachEnabled(bool dockLocked)
{
	ui->scenesDock->titleWidget()->setAdvButtonActionsEnabledByObjName("detachBtn", !dockLocked);
	ui->sourcesDock->titleWidget()->setAdvButtonActionsEnabledByObjName("detachBtn", !dockLocked);
	ui->mixerDock->titleWidget()->setAdvButtonActionsEnabledByObjName("detachBtn", !dockLocked);
	ui->chatDock->titleWidget()->setAdvButtonActionsEnabledByObjName("detachBtn", !dockLocked);

#define SET_DOCKLIST_DETACH_ENABLED(dockList, enable)                                          \
	for (int i = dockList.size() - 1; i >= 0; i--) {                                       \
		OBSDock *dock = dynamic_cast<OBSDock *>(dockList[i].get());                    \
		if (!dock) {                                                                   \
			continue;                                                              \
		}                                                                              \
		dock->titleWidget()->setAdvButtonActionsEnabledByObjName("detachBtn", enable); \
	}
	SET_DOCKLIST_DETACH_ENABLED(extraBrowserDocks, !dockLocked)
	SET_DOCKLIST_DETACH_ENABLED(extraDocks, !dockLocked)
	SET_DOCKLIST_DETACH_ENABLED(ncb2bCustomDocks, !dockLocked)
}

void PLSBasic::OnSourceDockSeperatedBtnClicked()
{
	changeDockState(ui->sourcesDock, actionSeperateSource);
}

void PLSBasic::OnSceneDockSeperatedBtnClicked()
{
	changeDockState(ui->scenesDock, actionSeperateScene);
}

void PLSBasic::OnChatDockSeperatedBtnClicked()
{
	changeDockState(ui->chatDock, actionSeperateChat);
}

void PLSBasic::OnBrowserDockSeperatedBtnClicked()
{
	QAction *action = reinterpret_cast<QAction *>(sender());
	if (!action) {
		return;
	}

#define CHANGE_DOCKLIST_STATE(dockList, action)                                         \
	for (int i = dockList.size() - 1; i >= 0; i--) {                                \
		BrowserDock *dock = reinterpret_cast<BrowserDock *>(dockList[i].get()); \
		if (!dock) {                                                            \
			continue;                                                       \
		}                                                                       \
		QString title = action->property("uuid").toString();                    \
		if (0 == dock->property("uuid").toString().compare(title)) {            \
			changeDockState(dock, action);                                  \
			dock->titleWidget()->updateTitle();                             \
			return;                                                         \
		}                                                                       \
	}
	CHANGE_DOCKLIST_STATE(extraBrowserDocks, action)
	CHANGE_DOCKLIST_STATE(extraDocks, action)
	CHANGE_DOCKLIST_STATE(ncb2bCustomDocks, action)
}

void PLSBasic::changeDockState(PLSDock *dock, QAction *action) const
{
	if (!dock) {
		return;
	}
	bool isBrowerDock = dynamic_cast<BrowserDock *>(dock) != nullptr;
	bool isFloating = dock->isFloating();
	SetAttachWindowBtnText(action, !isFloating);
	dock->titleWidget()->setCloseButtonVisible(isBrowerDock ? true : !isFloating);

	if (isFloating) {
		dock->setChangeState(true);
		dock->setFloating(!isFloating);
	} else {
		SetDocksMovePolicy(dock);
	}

	if (!dock->isVisible()) {
		dock->setVisible(true);
	}
}

void PLSBasic::SetDocksMovePolicy(PLSDock *dock) const
{
	bool isFloating = dock->isFloating();
	if (!isFloating) {
		QPoint pre = mapToGlobal(QPoint(dock->x(), dock->y()));
		dock->setChangeState(true);
		dock->setFloating(!isFloating);
		docksMovePolicy(dock, pre);
	}
}

void PLSBasic::docksMovePolicy(PLSDock *dock, const QPoint &pre) const
{
	if (qAbs(dock->y() - pre.y()) < DOCK_DEATTACH_MIN_SIZE && qAbs(dock->x() - pre.x()) < DOCK_DEATTACH_MIN_SIZE) {
		dock->move(pre.x() + DOCK_DEATTACH_MIN_SIZE, pre.y() + DOCK_DEATTACH_MIN_SIZE);
	}
}

void OBSBasic::CreateTimerSourcePopupMenu(QMenu *menu, obs_source_t *source) const
{
	OBSData settings = obs_data_create();
	obs_data_set_string(settings, "method", "get_timer_type");
	pls_source_get_private_data(source, settings);
	bool isTimer = obs_data_get_bool(settings, "is_timer");
	bool isStopwatch = obs_data_get_bool(settings, "is_stopwatch");
	if (!isTimer && !isStopwatch) {
		obs_data_release(settings);
		return;
	}

	QMenu *timerMenu = nullptr;
	if (isTimer) {
		timerMenu = menu->addMenu(tr("Basic.MainMenu.Edit.Timer"));
	} else if (isStopwatch) {
		timerMenu = menu->addMenu(tr("Basic.MainMenu.Edit.Stopwatch"));
	}

	auto actionStartTimer = pls_new<QAction>(QTStr("timer.btn.start"), timerMenu);
	auto actionCancelTimer = pls_new<QAction>(QTStr("timer.btn.cancel"), timerMenu);
	timerMenu->addAction(actionStartTimer);
	timerMenu->addAction(actionCancelTimer);

	obs_data_set_string(settings, "method", "get_timer_state");
	pls_source_get_private_data(source, settings);
	const char *startText = obs_data_get_string(settings, "start_text");
	const char *cancelText = obs_data_get_string(settings, "cancel_text");
	bool startSate = obs_data_get_bool(settings, "start_state");
	bool cancelSate = obs_data_get_bool(settings, "cancel_state");
	bool startHighlight = obs_data_get_bool(settings, "start_highlight");
	bool cancelHighlight = obs_data_get_bool(settings, "cancel_highlight");
	obs_data_release(settings);

	actionStartTimer->setText(startText);
	actionStartTimer->setEnabled(startSate);
	actionStartTimer->setCheckable(true);
	actionStartTimer->setChecked(startHighlight);
	actionCancelTimer->setEnabled(cancelSate);
	actionCancelTimer->setText(cancelText);
	actionCancelTimer->setCheckable(true);
	actionCancelTimer->setChecked(cancelHighlight);

	connect(actionStartTimer, &QAction::triggered, this, [source]() {
		obs_data_t *settings_ = obs_data_create();
		obs_data_set_string(settings_, "method", "start_clicked");
		pls_source_set_private_data(source, settings_);
		obs_data_release(settings_);
	});
	connect(actionCancelTimer, &QAction::triggered, this, [source]() {
		obs_data_t *settings_ = obs_data_create();
		obs_data_set_string(settings_, "method", "cancel_clicked");
		pls_source_set_private_data(source, settings_);
		obs_data_release(settings_);
	});
}

void PLSBasic::InitChatDockGeometry()
{

	auto mainView = getMainView();
	if (!mainView)
		return;

#ifdef __APPLE__
	int topOffset = 28;
#else
	int topOffset = 0;
#endif

	QPoint mainTopRight = mainView->mapToGlobal(QPoint(mainView->frameGeometry().width(), 0));
	auto geometryOfNormal = QRect(mainTopRight.x() + 5, mainTopRight.y() - topOffset, 300, 817);
	ui->chatDock->printChatGeometryLog("InitChatDockGeometry before");
	ui->chatDock->setGeometry(geometryOfNormal);
	pls_window_left_right_margin_fit(ui->chatDock);
	ui->chatDock->printChatGeometryLog("InitChatDockGeometry after");
}

void PLSBasic::SetAttachWindowBtnText(QAction *action, bool isFloating) const
{
	if (!action) {
		return;
	}
	if (isFloating) {
		action->setText(QTStr(MAIN_MAINFRAME_TOOLTIP_REATTACH));
	} else {
		action->setText(QTStr(MAIN_MAINFRAME_TOOLTIP_DETACH));
	}
}

void OBSBasic::CreateSceneCollectionView()
{
	if (!sceneCollectionView) {
		sceneCollectionView = pls_new<PLSSceneCollectionView>();
		connect(sceneCollectionView, &PLSSceneCollectionView::currentSceneCollectionChanged, this,
			[this](QString name, QString path) { on_actionChangeSceneCollection_triggered(name, path, false); });
		sceneCollectionView->hide();
	}
}

void OBSBasic::ShowSceneCollectionView()
{
	if (!sceneCollectionView) {
		CreateSceneCollectionView();
	}

	sceneCollectionView->show();
	sceneCollectionView->raise();
}

void PLSBasic::CreateGiphyStickerView()
{
	if (nullptr == giphyStickerView) {
		DialogInfo info;
		info.configId = ConfigId::GiphyStickersConfig;
		info.defaultWidth = 298;
		info.defaultHeight = 817;
		giphyStickerView = new PLSGiphyStickerView(info, nullptr);
		connect(giphyStickerView, &PLSGiphyStickerView::StickerApply, this, &PLSBasic::OnStickerApply);
	}
}

void PLSBasic::CreatePrismStickerView()
{
	if (nullptr == prismStickerView) {
		prismStickerView = new PLSPrismSticker(nullptr);
		connect(prismStickerView, &PLSPrismSticker::StickerApplied, this, &PLSBasic::OnPRISMStickerApplied);
	}
}

static void AddStickerSourceFunc(void *_data, obs_scene_t *scene)
{
	struct AddSourceData {
		obs_source_t *source;
		bool visible;
	};

	auto data_ = (AddSourceData *)_data;
	obs_sceneitem_t *sceneitem;

	sceneitem = obs_scene_add(scene, data_->source);
	obs_sceneitem_set_visible(sceneitem, data_->visible);

	obs_video_info ovi_;
	vec3 center;
	obs_get_video_info(&ovi_);

	if (!ovi_.base_width || !ovi_.base_height)
		return;

	vec3_set(&center, float(ovi_.base_width), float(ovi_.base_height), 0.0f);
	vec3_mulf(&center, &center, 0.5f);

	obs_transform_info itemInfo;
	itemInfo.alignment = OBS_ALIGN_CENTER;
	itemInfo.rot = 0.0f;
	vec2_set(&itemInfo.scale, 1.0f, 1.0f);
	vec2_set(&itemInfo.pos, center.x, center.y);

	vec2 baseSize;
	baseSize.x = static_cast<float>(obs_source_get_width(data_->source));
	baseSize.y = static_cast<float>(obs_source_get_height(data_->source));
	auto temp = 0.000001f;
	if (baseSize.x < temp || baseSize.y < temp)
		return;

	float item_aspect = baseSize.x / baseSize.y;
	float bounds_aspect = (float)ovi_.base_width / (float)ovi_.base_height;
	bool use_width = (bounds_aspect < item_aspect);
	float mul = use_width ? static_cast<float>(ovi_.base_width) / baseSize.x : static_cast<float>(ovi_.base_height) / baseSize.y;
	vec2_set(&itemInfo.scale, mul, mul);

	itemInfo.bounds_type = OBS_BOUNDS_NONE;
	itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
	obs_sceneitem_set_info2(sceneitem, &itemInfo);
}

bool PLSBasic::AddPRISMStickerSource(const StickerHandleResult &data, OBSSource &sourceOut)
{
	OBSScene scene = GetCurrentScene();
	if (!scene)
		return false;
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "loop", true);
	OBSSource newSource = nullptr;
	if (!CreateSource(PRISM_STICKER_SOURCE_ID, newSource, settings))
		return false;

	sourceOut = newSource;
	OBSDataAutoRelease priv_settings = obs_source_get_private_settings(newSource);
	obs_data_set_string(priv_settings, "landscapeVideo", qUtf8Printable(data.landscapeVideoFile));
	obs_data_set_string(priv_settings, "landscapeImage", qUtf8Printable(data.landscapeImage));
	obs_data_set_string(priv_settings, "portraitVideo", qUtf8Printable(data.portraitVideo));
	obs_data_set_string(priv_settings, "portraitImage", qUtf8Printable(data.portraitImage));
	obs_data_set_string(priv_settings, "resourceId", qUtf8Printable(data.data.id));
	obs_data_set_string(priv_settings, "resourceUrl", qUtf8Printable(data.data.resourceUrl));
	obs_data_set_string(priv_settings, "category", qUtf8Printable(data.data.category));
	obs_data_set_int(priv_settings, "version", data.data.version);
#if PRISM_STICKER_CENTER_SHOW
	pls_source_set_private_data(newSource, settings);
#else
	obs_source_update(source, settings);
#endif
	const char *id = PRISM_STICKER_SOURCE_ID;

	action::SendActionToNelo(id, action::ACTION_ADD_EVENT, id);
	QVariantMap stickerUploadVariantMap;
	stickerUploadVariantMap.insert(ANALOG_TOUCH_STICKER_CATEGORY_ID_KEY, data.data.category);
	stickerUploadVariantMap.insert(ANALOG_TOUCH_STICKER_ID_KEY, data.data.id);
	action::SendActionLog(action::ActionInfo(action::EVENT_MAIN_ADD, action::EVENT_SUB_SOURCE_ADDED, action::EVENT_TYPE_CONFIRMED, action::GetActionSourceID(id)));
	pls_send_analog(AnalogType::ANALOG_ADD_SOURCE, {{ANALOG_SOURCE_TYPE_KEY, action::GetActionSourceID(id)}, {ANALOG_DETAIL_KEY, stickerUploadVariantMap}});

	OnSourceCreated(id);
	action::SendPropsToNelo(id, {{"stickerId", qUtf8Printable(data.data.id)}, {"category", qUtf8Printable(data.data.category)}});

#if PRISM_STICKER_CENTER_SHOW
	// set initialize position to scene center.
	obs_video_info ovi;
	vec3 screenCenter;
	obs_get_video_info(&ovi);
	vec3_set(&screenCenter, float(ovi.base_width), float(ovi.base_height), 0.0f);
	vec3_mulf(&screenCenter, &screenCenter, 0.5f);

	struct AddSourceData {
		obs_source_t *source;
		bool visible;
	};

	AddSourceData addSource;
	addSource.source = newSource;
	addSource.visible = true;

	obs_enter_graphics();
	obs_scene_atomic_update(scene, AddStickerSourceFunc, &addSource);
	obs_leave_graphics();
#else
	source_show_default(source, scene);
#endif

	/* set monitoring if source monitors by default */
	uint32_t flags = obs_source_get_output_flags(newSource);
	if ((flags & OBS_SOURCE_MONITOR_BY_DEFAULT) != 0) {
		obs_source_set_monitoring_type(newSource, OBS_MONITORING_TYPE_MONITOR_ONLY);
	}

	action::SendActionLog(action::ActionInfo(action::EVENT_MAIN_EDIT, action::ACT_SRC_STICKER, action::EVENT_TYPE_CONFIRMED, data.data.id));

	return true;
}

bool PLSBasic::CreateSource(const char *id, OBSSource &newSource, obs_data_t *settings)
{
	QString sourceName{QT_UTF8(obs_source_get_display_name(id))};
	QString newName{sourceName};
	int i = 2;
	OBSSourceAutoRelease source_ = nullptr;
	while ((source_ = obs_get_source_by_name(QT_TO_UTF8(newName)))) {
		newName = QString("%1 %2").arg(sourceName).arg(i);
		i++;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(QT_TO_UTF8(newName));
	if (source) {
		OBSMessageBox::information(this, QTStr("Alert.Title"), QTStr("NameExists.Text"));
		return false;
	}

	source = obs_source_create(id, QT_TO_UTF8(newName), settings, nullptr);
	if (source) {
		newSource = source;
		return true;
	}

	return false;
}

static void updateChatExternParams(const OBSSource &source, int code)
{
	QJsonObject setting;
	setting.insert("type", "setting");

	QJsonObject data;
	data.insert("singlePlatform", PLS_PLATFORM_API->isGoLive() ? (pls_get_actived_chat_channel_count() == 1) : false);

	int seqHorizontal;
	int seqVertical;
	pls_get_prism_live_seq(seqHorizontal, seqVertical);

	if (seqHorizontal > 0) {
		data.insert("videoSeq", QString::number(seqHorizontal));
	}

	QJsonArray videoSeqs;
	if (seqHorizontal > 0) {
		videoSeqs.append(seqHorizontal);
	}
	if (seqVertical > 0) {
		videoSeqs.append(seqVertical);
	}
	if (!videoSeqs.isEmpty()) {
		data.insert("videoSeqs", videoSeqs);
	}

	const char *id = obs_source_get_id(source);
	bool isChatTemplateSource = pls_is_equal(id, PRISM_CHATV2_SOURCE_ID);
	if (!pls_is_create_souce_in_loading() && !PLS_PLATFORM_API->isGoLive()) {
		isChatTemplateSource ? data.insert("isShowFake", true) : data.insert("preview", "1");
	} else {
		isChatTemplateSource ? data.insert("isShowFake", false) : data.insert("preview", "0");
	}
	if (isChatTemplateSource && !PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName().isEmpty()) {
		auto logoUrl = PLSLoginDataHandler::instance()->getNCB2BLogoUrl();
		QString platformIcons = "ncp=" + logoUrl;
		data.insert("platformIcons", platformIcons);
	}
	data.insert("NEO_SES", QString::fromUtf8(pls_get_prism_cookie_value()));
	data.insert("userCode", pls_get_prism_usercode());
	setting.insert("data", data);

	QByteArray json = QJsonDocument(setting).toJson(QJsonDocument::Compact);
	pls_source_update_extern_params_json(source, json.constData(), code);
}

static void sourceNotifyAsyncChatUpdateParams(const OBSSource &source, int code)
{
	switch (code) {
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_UPDATE:
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_EDIT_START:
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_LOADED:
		updateChatExternParams(source, code);
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_CHECK_LIVE:
		if (obs_frontend_streaming_active()) {
			// notify chat source
			QJsonObject setting;
			setting.insert("type", "liveStart");
			QByteArray json = QJsonDocument(setting).toJson(QJsonDocument::Compact);
			pls_source_update_extern_params_json(source, json.constData(), code);
		}
		break;
	case OBS_SOURCE_CHAT_UPDATE_PARAMS_SUB_CODE_RESIZE_VIEW: {
		PLSBasic::instance()->resizeCTView();
	} break;
	default:
		break;
	}
}

void PLSBasic::OnSourceNotifyAsync(QString name, int msg, int code)
{
	pls_check_app_exiting();
	OBSSource source = pls_get_source_by_name(name.toStdString().c_str());
	if (!source) {
		return;
	}

	if (pls_get_app_exiting()) {
		PLS_WARN(MAINFRAME_MODULE, "Invalid invoking in %s, source: %p msg: %d code: %d", __FUNCTION__, source.Get(), msg, code);
		assert(false);
		return;
	}
#if 0
	if (msg == OBS_SOURCE_TEXTMOTION_POS_CHANGED) {
		QMetaObject::invokeMethod(
			this, [name]() { centerTextmotionItems(name.toUtf8().constData()); }, Qt::QueuedConnection);
	}

	if (msg == OBS_SOURCE_TEXTMOTION_BOXSIZE_CHANGED) {
		QMetaObject::invokeMethod(
			this, [name]() { textmotionBoxSizeChanged(name.toUtf8().constData()); }, Qt::QueuedConnection);
	}
#endif

	//PRISM/Zhangdewen/20211015/#/Chat Source Event
	if (msg == OBS_SOURCE_CHAT_UPDATE_PARAMS) {
		//todo
		sourceNotifyAsyncChatUpdateParams(source, code);
		return;
	}

	PLS_INFO(SOURCE_MODULE, "Recieved source notify from [%s]. msg:%d code:%d", name.toStdString().c_str(), msg, code);

	const char *pluginID = obs_source_get_id(source);

	if (pls_is_in(msg, OBS_SOURCE_EXCEPTION_BG_FILE_ERROR, OBS_SOURCE_EXCEPTION_BG_FILE_NETWORK_ERROR)) {
		if (pls_is_equal(pluginID, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID)) {
			backgroundTemplateSourceError(msg, code);
		} else {
			/*	if (m_virtualBgView) {
				m_virtualBgView->setBgFileError(name);
			}*/
			QMetaObject::invokeMethod(this, [name]() { PLSVirtualBgManager::checkResourceInvalid({}, name); }, Qt::QueuedConnection);
		}
	} else if (msg == OBS_SOURCE_PROPERTY_ERROR_STATUS) {
		if (properties) {
			if (code == 1) {
				properties->showGuideText(QObject::tr("source.camera.notsupport.hdr"));
			} else
				properties->hideGuideText();
		}
	} else if (msg == OBS_SOURCE_MUSIC_STATE_CHANGED && backgroundMusicView) {
		obs_media_state state = static_cast<obs_media_state>(code);
		backgroundMusicView->OnMediaStateChanged(name, state);
	} else if (msg == OBS_SOURCE_MUSIC_LOOP_STATE_CHANGED && backgroundMusicView) {
		backgroundMusicView->OnLoopStateChanged(name);
	} else if (msg == OBS_SOURCE_MUSIC_MODE_STATE_CHANGED && backgroundMusicView) {
		backgroundMusicView->OnModeStateChanged(name);
	}
}

void PLSBasic::OnMainViewShow(bool isShow)
{

	if (isFirstShow) {
		PLS_INFO(MAINFRAME_MODULE, "start notice timer");
		m_noticeTimer.start();
	}
	if (isShow && isFirstShow) {

		PLS_INFO(UPDATE_MODULE, "enter OnMainViewShow(%d)", isShow);

		getLocalUpdateResult();
		isFirstShow = false;
		PLS_INFO(UPDATE_MODULE, "enter ShowLoadSceneCollectionError");
		ShowLoadSceneCollectionError();
		PLS_INFO(UPDATE_MODULE, "enter showChangeSceneDisplayAlert");
		showChangeSceneDisplayAlert();
		PLS_INFO(UPDATE_MODULE, "enter StartUpdateSceneCollectionTimeTimer");
		StartUpdateSceneCollectionTimeTimer();
		PLS_INFO(UPDATE_MODULE, "enter QApplication::postEvent(ui->scenesDock->titleWidget(), new QEvent(QEvent::Resize))");
		QApplication::postEvent(ui->scenesDock->titleWidget(), new QEvent(QEvent::Resize));
		PLS_PRSIM_SHARE_MEMORY;

		if (ncb2bBrowserSettings) {
			ncb2bBrowserSettings->refreshUI();
		}
		PLSSceneitemMapMgrInstance->bindMappedVerticalItemHotkeys();
		PLSSceneitemMapMgrInstance->switchToDualOutputMode();

		PLSLaunchWizardView *banner = dynamic_cast<PLSLaunchWizardView *>(pls_get_banner_widget());
		if (banner && banner->isNeedShow()) {
			PLS_INFO(UPDATE_MODULE, "pls_get_banner_widget object is existed");
			PLSShowWatcher *watcher = new PLSShowWatcher(banner);
			QObject::connect(
				watcher, &PLSShowWatcher::signalShow, banner,
				[watcher, banner, this]() {
					PLS_INFO(UPDATE_MODULE, "PLSShowWatcher notify banner show event");
					watcher->deleteLater();
					showMainViewAfter(banner);
					banner->activateWindow();
					banner->raise();
				},
				Qt::ConnectionType(Qt::QueuedConnection | Qt::SingleShotConnection));
		} else {
			PLS_INFO(UPDATE_MODULE, "pls_get_banner_widget object is empty");
			showMainViewAfter(getMainView());
		}

		PLSBasicStatusPanel::InitializeValues();
	}

#ifndef _WIN32
	static auto bAutoClose = false;
	if (!isShow) {
		if (m_dialogStatusPanel->isVisible()) {
			bAutoClose = true;
			toggleStatusPanel(0);
		}
	} else {
		if (bAutoClose) {
			bAutoClose = false;
			toggleStatusPanel(1);
		}
	}
#endif
}

void OBSBasic::LoadSourceRecentColorConfig(obs_data_t *obj)
{
	source_recent_color_config = obs_data_get_obj(obj, "source_recent_color_config");
	if (!source_recent_color_config) {
		source_recent_color_config = obs_data_create();
	}
}

void OBSBasic::UpdateSourceRecentColorConfig(QString strColor)
{
	obs_data_t *colorPreset = obs_data_create();
	obs_data_set_string(colorPreset, "color", strColor.toUtf8());

	obs_data_array_t *colorArray = obs_data_get_array(source_recent_color_config, "color-order");
	if (!colorArray) {
		colorArray = obs_data_array_create();
		obs_data_array_push_back(colorArray, colorPreset);
		obs_data_set_array(source_recent_color_config, "color-order", colorArray);
	} else {
		bool existed = false;
		size_t count = obs_data_array_count(colorArray);
		for (int i = 0; i < count; i++) {
			obs_data_t *existedColor = obs_data_array_item(colorArray, i);
			if (const char *color = obs_data_get_string(existedColor, "color"); 0 == strColor.compare(color)) {
				existed = true;
				obs_data_release(existedColor);
				break;
			}
			obs_data_release(existedColor);
		}

		if (!existed) {
			obs_data_array_insert(colorArray, 0, colorPreset);
		}
		count = obs_data_array_count(colorArray);
		if (count > SOURCE_ITEM_MAX_COLOR_COUNT) {
			obs_data_array_erase(colorArray, count - 1);
		}
	}
	obs_data_array_release(colorArray);
	obs_data_release(colorPreset);
}

void OBSBasic::SetScene(OBSScene scene)
{
	obs_source_t *source = obs_scene_get_source(scene);
	if (source) {
		OBSWeakSource temp = OBSGetWeakRef(source);
		bool sceneChenged = false;
		std::lock_guard<std::mutex> locker(mutex_current_scene);
		sceneChenged = m_currentScene != temp;
		if (sceneChenged) {
			m_currentScene = temp;
			PLS_LOG(PLS_LOG_INFO, MAINSCENE_MODULE, "Current scene is changed : '%s' [%p]", obs_source_get_name(source), source);
		}
	}
}

void OBSBasic::SceneItemRemoved(void *data, calldata_t *params)
{
	auto item = (obs_sceneitem_t *)calldata_ptr(params, "item");
	QMetaObject::invokeMethod(static_cast<OBSBasic *>(data), "RemoveSceneItem", Qt::QueuedConnection, Q_ARG(OBSSceneItem, OBSSceneItem(item)));
}

void OBSBasic::RemoveSceneItem(OBSSceneItem item) const
{
	PLSBasic::instance()->OnRemoveSceneItem(item);
}

void OBSBasic::SceneItemSelected(void *data, calldata_t *params)
{
	pls_unused(params);

	auto window = static_cast<OBSBasic *>(data);

	auto scene = (obs_scene_t *)calldata_ptr(params, "scene");
	auto item = (obs_sceneitem_t *)calldata_ptr(params, "item");

	QMetaObject::invokeMethod(window, "SelectSceneItem", Qt::QueuedConnection, Q_ARG(OBSScene, scene), Q_ARG(OBSSceneItem, item), Q_ARG(bool, true));
}

void OBSBasic::SceneItemDeselected(void *data, calldata_t *params)
{
	pls_unused(params);

	auto window = static_cast<OBSBasic *>(data);

	auto scene = (obs_scene_t *)calldata_ptr(params, "scene");
	auto item = (obs_sceneitem_t *)calldata_ptr(params, "item");

	QMetaObject::invokeMethod(window, "SelectSceneItem", Qt::QueuedConnection, Q_ARG(OBSScene, scene), Q_ARG(OBSSceneItem, item), Q_ARG(bool, false));
}

void OBSBasic::OnSelectItemChanged(OBSSceneItem item, bool selected)
{
	//if (mediaController) {
	//	OBSSceneItem items = selected ? item : ui->sources->Get(GetTopSelectedSourceItem());
	//	QMetaObject::invokeMethod(
	//		mediaController, [this, items, selected]() { mediaController->UpdateMediaSource(items, selected); }, Qt::QueuedConnection);
	//}

	PLSBasic::instance()->SetBgmItemSelected(item, selected);

	//reloadToSelectVritualbg();
	//SelectSceneItemForBeauty(GetCurrentScene(), item, selected);
	//const obs_source_t *itemSource = obs_sceneitem_get_source(item);
	//const char *id = obs_source_get_id(itemSource);
	//emit selectItemChanged((qint64)item.get(), (qint64)itemSource, id, selected);
}

void OBSBasic::OnVisibleItemChanged(OBSSceneItem item, bool visible)
{
	PLSAudioControl::instance()->OnSourceItemVisibleChanged(item, visible);
	PLSBasic::instance()->SetBgmItemVisible(item, visible);
}

void OBSBasic::OnSourceItemsRemove(QVector<OBSSceneItem> items)
{
	for (const auto &item : items) {
		//	OnRemoveSourceTreeItem(item);
		PLSAudioControl::instance()->OnSourceItemRemoved(item);
		PLSBasic::instance()->RemoveBgmItem(item);
	}
}

void OBSBasic::OnMultiviewShowTriggered(bool checked)
{
	OBSSource source = GetCurrentSceneSource();
	OBSData data = obs_source_get_private_settings(source);
	obs_data_release(data);

	bool show = obs_data_get_bool(data, "show_in_multiview");
	if (show) {
		return;
	}

	obs_data_set_bool(data, "show_in_multiview", checked);
	OBSProjector::UpdateMultiviewProjectors();
}

void OBSBasic::OnMultiviewHideTriggered(bool checked)
{
	OBSSource source = GetCurrentSceneSource();
	OBSData data = obs_source_get_private_settings(source);
	obs_data_release(data);

	bool show = obs_data_get_bool(data, "show_in_multiview");
	if (!show) {
		return;
	}

	obs_data_set_bool(data, "show_in_multiview", checked);
	OBSProjector::UpdateMultiviewProjectors();
}

void OBSBasic::UpdateSceneSelection(OBSSource source)
{
	if (!source)
		return;
	const obs_scene_t *scene = obs_scene_from_source(source);
	const char *name = obs_source_get_name(source);

	if (!scene)
		return;
	QList<PLSSceneItemView *> items = ui->scenesFrame->FindItems(name);

	if (items.count()) {
		sceneChanging = true;
		ui->scenesFrame->SetCurrentItem(items.first());
		sceneChanging = false;

		const PLSSceneItemView *item = ui->scenesFrame->GetCurrentItem();
		if (item) {
			OBSScene curScene = ui->scenesFrame->GetCurrentItem()->GetData();
			if (api && scene != curScene)
				OnEvent(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED);
		}
	}
}

void PLSBasic::OnUpdateSceneCollectionTimeTimerTriggered()
{
	if (!sceneCollectionView) {
		return;
	}

	sceneCollectionView->UpdateTimeStampLabel();
}

void PLSBasic::replaceMenuActionWithWidgetAction(QMenu *menu, QAction *originAction, QWidgetAction *replaceAction)
{
	foreach(QAction * action, menu->actions())
	{
		if (action->isSeparator()) {
			qDebug("this action is a separator");
		} else if (action->menu()) {
			replaceMenuActionWithWidgetAction(action->menu(), originAction, replaceAction);
		} else {
			if (action == originAction) {
				menu->insertAction(action, replaceAction);
				menu->removeAction(action);
			}
		}
	}
}

void PLSBasic::replaceMenuActionWithWidgetAction(QMenuBar *menuBar, QAction *originAction, QWidgetAction *replaceAction)
{
	foreach(QAction * action, menuBar->actions())
	{
		if (action->isSeparator()) {
			qDebug("this action is a separator");
		} else if (action->menu()) {
			qDebug("action: %s", qUtf8Printable(action->text()));
			replaceMenuActionWithWidgetAction(action->menu(), originAction, replaceAction);
		} else {
			if (action == originAction) {
				menuBar->insertAction(action, replaceAction);
				menuBar->removeAction(action);
			}
		}
	}
}

void PLSBasic::CheckAppUpdate(bool isShowAlert) {
	
}

void PLSBasic::CheckAppUpdateFinished(bool isShowAlert, const PLSErrorHandler::RetData &retData ) {
	
}

int PLSBasic::compareVersion(const QString &v1, const QString &v2) const
{
	return 0;
}

int PLSBasic::getUpdateResult() const
{
	return m_checkUpdateResult;
}

bool PLSBasic::ShowUpdateView(QWidget *parent)
{
	return false;
}

QStringList PLSBasic::getRestartParams(bool isUpdate)
{
	QStringList params;
	if (isUpdate) {
		auto basic = PLSBasic::Get();
		QString updateUrl = m_updateFileUrl;
		QString updateVersion = m_updateVersion;
		QString updateGcc = pls_get_gcc_data();
		QString filePath = m_updateFileUrl;
		params.append(shared_values::k_launcher_command_update_file + updateUrl);
		params.append(shared_values::k_launcher_command_update_version + updateVersion);
		params.append(shared_values::k_launcher_command_update_gcc + updateGcc);
	}
	return params;
}

bool enumStickerSource(void *data, obs_source_t *source)
{
	auto stickerUsed = (std::map<QString, bool> *)data;
	if (stickerUsed) {
		const char *id = obs_source_get_id(source);
		if (id && *id) {
			if (0 != strcmp(id, PRISM_GIPHY_STICKER_SOURCE_ID))
				return true;
			OBSData settings = obs_source_get_settings(source);
			const char *file = obs_data_get_string(settings, "file");
			if (file && *file) {
				QString fileName(file);
				QString filePreview;
				fileName = fileName.mid(fileName.lastIndexOf("/") + 1);
				stickerUsed->insert(std::make_pair(fileName, true));
				filePreview = fileName;
				filePreview.replace(FILE_ORIGINAL, FILE_THUMBNAIL);
				stickerUsed->insert(std::make_pair(filePreview, true));
			}
			obs_data_release(settings);
		}
		return true;
	}
	return false;
}

void PLSBasic::ClearStickerCache() const
{
	std::map<QString, bool> stickerUsed;
	obs_enum_sources(enumStickerSource, &stickerUsed);

	QDir dir(pls_get_user_path(GIPHY_STICKERS_CACHE_PATH).append("/"));
	if (dir.exists()) {
		QFileInfoList infoList = dir.entryInfoList();
		for (int i = 0; i < infoList.size(); i++) {
			QFileInfo fileInfo = infoList.at(i);
			QString name = fileInfo.fileName();
			if (name == "." || name == ".." || fileInfo.isDir() || !name.contains(".gif")) {
				continue;
			}

			if (stickerUsed.end() != stickerUsed.find(name))
				continue;

			// delete thunmbnail and unused oringinal gifs.
			if (!QFile::remove(fileInfo.absoluteFilePath())) {
				PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Delete file %s failed", name.toUtf8().constData());
			}
		}
	}
}

void PLSBasic::InitInteractData() const
{
	auto cx = (int)config_get_int(App()->GetUserConfig(), "InteractionWindow", "cx");
	auto cy = (int)config_get_int(App()->GetUserConfig(), "InteractionWindow", "cy");

	obs_data_t *browserData = obs_data_create();
	obs_data_set_int(browserData, "interaction_cx", cx);
	obs_data_set_int(browserData, "interaction_cy", cy);
	obs_data_set_int(browserData, "prism_hwnd", mainView->window()->winId());
	obs_data_set_string(browserData, "font", this->font().family().toStdString().c_str());
	pls_plugin_set_private_data(BROWSER_SOURCE_ID, browserData);
	obs_data_release(browserData);
}

void PLSBasic::SaveInteractData() const
{
	obs_data_t *browserData = obs_data_create();
	pls_plugin_get_private_data(BROWSER_SOURCE_ID, browserData);
	long long interaction_cx = obs_data_get_int(browserData, "interaction_cx");
	long long interaction_cy = obs_data_get_int(browserData, "interaction_cy");
	obs_data_release(browserData);

	config_set_int(App()->GetUserConfig(), "InteractionWindow", "cx", interaction_cx);
	config_set_int(App()->GetUserConfig(), "InteractionWindow", "cy", interaction_cy);
}

bool PLSBasic::hasSourceMonitor() const
{
	//tranverse all the audio device sources to judge if all the device sources' previous muted status are muted.
	bool isMonitoring = false;
	auto allSourceMute = [&isMonitoring](obs_source_t *source) {
		pls_unused(source);
		if (!source)
			return true;

		uint32_t flags = obs_source_get_output_flags(source);
		if (flags & OBS_SOURCE_AUDIO) {
			if (obs_source_get_monitoring_type(source) != obs_monitoring_type::OBS_MONITORING_TYPE_NONE) {
				isMonitoring = true;
				return false;
			}
		}
		return true;
	};

	using allSourceMute_t = decltype(allSourceMute);

	auto preEnum = [](void *param, obs_source_t *source) /* -- */
	{ return (*(allSourceMute_t *)(param))(source); };

	obs_enum_sources(preEnum, &allSourceMute);

	return isMonitoring;
}

void PLSBasic::restartAppDirect(bool isUpdate)
{
	QStringList params = getRestartParams(isUpdate);
}

void PLSBasic::restartApp(const RestartAppType &restartType, const QStringList &params)
{
	QString filePath = QApplication::applicationFilePath();
	QStringList allParams = params;
	switch (restartType) {
	case RestartAppType::Direct:
		break;
	case RestartAppType::Logout:
		allParams.append(QString::asprintf("%s%d", shared_values::k_launcher_command_type.toUtf8().constData(), RestartAppType::Logout));
		break;
	case RestartAppType::ChangeLang:
		break;
	case RestartAppType::Update:
		allParams.append(QString::asprintf("%s%d", shared_values::k_launcher_command_type.toUtf8().constData(), RestartAppType::Update));
		break;
	default:
		break;
	}
	auto param = allParams.join(' ');
	QObject::connect(qApp, &QApplication::destroyed, [filePath, param, allParams]() {
		PLS_INFO("RestartApp", "restart app param = %s", param.toUtf8().constData());
#if defined(Q_OS_WIN)
		std::wstring filePathW = filePath.toStdWString();
		std::wstring params = param.toStdWString();
		PLS_INFO("App Restart", "Prism Live Studio restarting now...");
		SHELLEXECUTEINFO sei = {};
		sei.cbSize = sizeof(sei);
		sei.lpFile = filePathW.c_str();
		sei.nShow = SW_SHOWNORMAL;
		sei.lpParameters = params.c_str();
		if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
			if (ShellExecuteEx(&sei)) {
				PLS_INFO(APP_MODULE, "successfully to call ShellExecuteEx() for restart PRISMLiveStudio.exe");
			} else {
				PLS_ERROR(APP_MODULE, "failed to call ShellExecuteEx() for restart PRISMLiveStudio.exe ERROR: %lu", GetLastError());
			}
			CoUninitialize();
		} else {
			PLS_ERROR(APP_MODULE, "failed to call CoInitializeEx() for restart PRISMLiveStudio.exe ERROR");
		}
#elif defined(Q_OS_MACOS)
        pls_restart_mac_app(allParams);
#endif
	});
}

void PLSBasic::startDownloading(bool forceDownload)
{
	
}

void PLSBasic::OpenRegionCapture()
{
	if (pls_is_app_exiting())
		return;

	if (regionCapture || properties)
		return;

	regionCapture = pls_new<PLSRegionCapture>(this);
	connect(regionCapture, &PLSRegionCapture::selectedRegion, this, [this](const QRect &selectedRect) {
		if (pls_is_app_exiting())
			return;
		AddSelectRegionSource(PRISM_REGION_SOURCE_ID, selectedRect);
		regionCapture->deleteLater();
	});
	auto max_size = pls_texture_get_max_size();
	regionCapture->StartCapture(max_size, max_size);
}

void PLSBasic::AddSelectRegionSource(const char *id, const QRect &rectSelected)
{
	OBSScene scene = GetCurrentScene();
	if (!scene)
		return;

	if (!rectSelected.isValid())
		return;

	QString sourceName{QT_UTF8(obs_source_get_display_name(id))};
	QString newName{sourceName};
	int i = 2;
	obs_source_t *source_ = nullptr;
	while ((source_ = obs_get_source_by_name(QT_TO_UTF8(newName)))) {
		obs_source_release(source_);
		newName = QString("%1 %2").arg(sourceName).arg(i);
		i += 1;
	}

	obs_source_t *source = obs_source_create(id, QT_TO_UTF8(newName), nullptr, nullptr);
	if (source) {

		OnSourceCreated(id);

		struct AddSourceData {
			obs_source_t *source;
			bool visible;
		};
		AddSourceData data{source, true};

		auto addRegionCaptureSource = [](void *_data, obs_scene_t *scene_ptr) {
			auto add_source_data = (AddSourceData *)_data;
			obs_sceneitem_t *sceneitem;

			sceneitem = obs_scene_add(scene_ptr, add_source_data->source);
			obs_sceneitem_set_visible(sceneitem, add_source_data->visible);
		};

		obs_enter_graphics();
		obs_scene_atomic_update(scene, addRegionCaptureSource, &data);
		obs_leave_graphics();

		/* set monitoring if source monitors by default */
		auto flags = obs_source_get_output_flags(source);
		if ((flags & OBS_SOURCE_MONITOR_BY_DEFAULT) != 0) {
			obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_ONLY);
		}

		OBSSource newSource = source;
		CreatePropertiesWindow(newSource, OPERATION_ADD_SOURCE);

		QTimer::singleShot(0, this, [this]() {
			if (properties) {
				properties->activateWindow();
				properties->setFocus();
			}
		});

		OBSData oldSettings(obs_data_create());
		obs_data_release(oldSettings);
		OBSData settings = obs_source_get_settings(newSource);
		obs_data_apply(oldSettings, settings);
		obs_data_release(settings);

		qInfo() << "user selected a new region=" << rectSelected;
		obs_data_t *region_obj = obs_data_create();
		obs_data_set_int(region_obj, "left", rectSelected.left());
		obs_data_set_int(region_obj, "top", rectSelected.top());
		obs_data_set_int(region_obj, "width", rectSelected.width());
		obs_data_set_int(region_obj, "height", rectSelected.height());
		obs_data_set_obj(settings, "region_select", region_obj);
		obs_data_release(region_obj);
		obs_source_update(newSource, settings);

		obs_source_release(source);
	}
}

void PLSBasic::restartPrismApp(bool isUpdated)
{
	if (mainView) {
		PLS_INFO(MAINFRAME_MODULE, "prism app start restart.");
		mainView->close();
		restartApp();
	} else {
		PLS_INFO(MAINFRAME_MODULE, "mainview pointer is nullptr");
	}
}

bool PLSBasic::checkMainViewClose(QCloseEvent *event)
{
	bool confirmOnExit = config_get_bool(App()->GetUserConfig(), "General", "ConfirmOnExit");

	if (confirmOnExit && (pls_is_living_or_recording() || VirtualCamActive())) {

		//yes to close ,no to cancel
		auto askUser = [this]() {
			SetShowing(true);

			QString txt;
			if (pls_is_living_or_recording()) {
				txt = tr("main.message.exit_broadcasting_alert");
			} else {
				txt = tr("main.message.exit_virtual_camera_on_alert");
			}
			return PLSAlertView::question(mainView, tr("Confirm"), txt, PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel) != PLSAlertView::Button::Ok;
		};
		// close main view in living or recording need show alert
		if (!m_isUpdateLanguage && !m_isSessionExpired && !m_isLogout && askUser()) {
			return false;
		}
	}
	return true;
}

void PLSBasic::closeMainBegin()
{
	pls::http::abortAll(true);
	if (!isFirstShow) {
		config_set_string(App()->GetUserConfig(), "BasicWindow", "DockState", saveState().toBase64().constData());
	}

	HideAllInteraction(nullptr);
	SaveInteractData();

	EnumDialogs();
	SetShowing(false);

	pls_notify_close_modal_views();

	if (properties && properties->isVisible()) {
		properties->close();
		pls_delete(properties); // delete immediately
	}

	PLSFileDownloader::instance()->Stop();

	if (nullptr != checkCamProcessWorker) {
		checkCamProcessWorker->deleteLater();
		checkCamProcessWorker = nullptr;
	}

	ui->chatWidget->quitAppToReleaseCefData();
	checkCamThread.quit();
	checkCamThread.wait();
}

void PLSBasic::closeMainFinished()
{
	if (backgroundMusicView) {
		backgroundMusicView->ClearUrlInfo();
	}

	PLSMotionItemView::cleaupCache();
	PLSLiveInfoNaverShoppingLIVEProductItemView::cleaupCache();
	PLSNaverShoppingLIVEProductItemView::cleaupCache();
	PLSNaverShoppingLIVESearchKeywordItemView::cleaupCache();
}

void PLSBasic::initSideBarWindowVisible()
{
	// beauty
	bool beautyVisible = pls_config_get_bool(App()->GetUserConfig(), ConfigId::BeautyConfig, CONFIG_SHOW_MODE);
	//OnSetBeautyVisible(beautyVisible);

	//giphy
	InitGiphyStickerViewVisible();

	//Prism Sticker
	InitPrismStickerViewVisible();

	// ncb2b
	initNcb2bBrowserSettingsVisible();

	//background music
	bool bgmVisible = pls_config_get_bool(App()->GetUserConfig(), ConfigId::BgmConfig, CONFIG_SHOW_MODE);
	OnSetBgmViewVisible(bgmVisible);

	//toast
	bool toastVisible = pls_config_get_bool(App()->GetUserConfig(), ConfigId::LivingMsgView, CONFIG_SHOW_MODE);
	mainView->setToastMsgViewVisible(toastVisible);
	//chat view
	mainView->showChatView(false, true);

	//virtual background
	//bool virtualVisible = pls_config_get_bool(App()->GetUserConfig(), ConfigId::VirtualbackgroundConfig, CONFIG_SHOW_MODE);
	//if (virtualVisible)
	//OnVirtualBgClicked(true);

	// cam studio
	InitCamStudioSidebarState();

	InitDualOutputEnabled();
}

void PLSBasic::CreateToolArea()
{
	QLayout *layout = ui->centralwidget->layout();
	drawPenView = new PLSDrawPenView(this);
	layout->addWidget(drawPenView);
	if (drawPenView)
		drawPenView->setVisible(false);
}

void PLSBasic::OnToolAreaVisible()
{
	bool checked = ui->toggleContextBar->isChecked();
	if (!checked)
		return;
	bool dp = drawPenView && drawPenView->isVisible();

	if (dp) {
		ui->contextContainer->setVisible(false);
	} else {
		ui->contextContainer->setVisible(true);
		UpdateContextBarDeferred(true);
	}
}

void PLSBasic::UpdateDrawPenView(OBSSource scene, bool collectionChanging) const
{
	if (mainView->isVisible() && drawPenView) {
		obs_scene_t *curscene = obs_scene_from_source(scene);
		drawPenView->UpdateView(curscene, collectionChanging);
	}
}

void PLSBasic::UpdateDrawPenVisible(bool locked)
{
	if (!locked && drawPenView && drawPenView->isVisible()) {
		drawPenView->setVisible(false);
		if (previewProgramTitle) {
			previewProgramTitle->OnDrawPenModeStatus(false, IsPreviewProgramMode());
		}
	}
}

void PLSBasic::ResizeDrawPenCursorPixmap() const
{
	if (IsDrawPenMode())
		PLSDrawPenMgr::Instance()->UpdateCursorPixmap();
}

bool PLSBasic::ShowStickerView(const char *id)
{
	if (pls_is_equal(id, PRISM_GIPHY_STICKER_SOURCE_ID)) {
		CreateGiphyStickerView();
		giphyStickerView->hide();
		giphyStickerView->show();
		giphyStickerView->raise();
		return true;
	}

	if (pls_is_equal(id, PRISM_STICKER_SOURCE_ID)) {
		CreatePrismStickerView();
		prismStickerView->hide();
		prismStickerView->show();
		prismStickerView->raise();
		return true;
	}

	return false;
}

void PLSBasic::OnSourceCreated(const char *id)
{
	if (!id)
		return;
	pls_async_call(this, [this]() {
		PLSSceneitemMapMgrInstance->switchToDualOutputMode();
		updateSourceIcon();
	});
	PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"addSource", id}}, "User add source: %s", id);
}

void PLSBasic::ShowAudioMixerAudioTrackTips(bool bEnabled)
{

	if (!bEnabled) {
		if (m_bubbleTips)
			m_bubbleTips->close();
		return;
	}

	if (m_showMixerAudioTrackTip) {
		return;
	}

	bool hasTipsCount = config_has_user_value(App()->GetUserConfig(), AUDIO_MIXER_CONFIG, AUDIO_MIXER_SHOW_TIPS_COUNT);
	int showTipsCount = 0;
	if (hasTipsCount) {
		showTipsCount = config_get_int(App()->GetUserConfig(), AUDIO_MIXER_CONFIG, AUDIO_MIXER_SHOW_TIPS_COUNT);
	}
	if (showTipsCount >= 3) {
		return;
	}

	auto buddy = ui->mixerDock->titleWidget();
	bubbletips *tips = pls_show_bubble_tips(ui->mixerDock, QPoint(0, 5), tr("Basic.Main.audiomixer.dualOutput.audioTrack.tip"), 17, bubbleInfiniteLoopDuration, BubbleTips::DualOutputAudioTrack);
	if (tips) {
		connect(tips, &bubbletips::clickTextLink, this, [=](const QString &) {
			QMetaObject::invokeMethod(PLSBasic::Get(), "onPopupSettingView", Q_ARG(QString, "Output"), Q_ARG(QString, common::AUDIO_MIXER_DUAL_OUTPUT_ADVANCE_PAGE));
		});
		tips->addListenDockTitleWidget(buddy);
		showTipsCount++;
		m_showMixerAudioTrackTip = true;
		config_set_int(App()->GetUserConfig(), AUDIO_MIXER_CONFIG, AUDIO_MIXER_SHOW_TIPS_COUNT, showTipsCount);
		config_save(App()->GetUserConfig());
		m_bubbleTips = tips;
	}
}

QString PLSBasic::getOutputStreamErrorAlert(int code, const QString &last_error)
{
	if (code == OBS_OUTPUT_SUCCESS) {
		return QString();
	}

	PLSErrorHandler::ExtraData extra_data;
	bool encoder_error = (code == OBS_OUTPUT_ENCODE_ERROR);
	bool use_last_error = pls_is_in(code, OBS_OUTPUT_DISCONNECTED, OBS_OUTPUT_ERROR, OBS_OUTPUT_CONNECT_FAILED);
	if (encoder_error && !last_error.isEmpty()) {
		extra_data.pathValueMap.insert("hasErrorCode", "1");
		extra_data.pathValueMap.insert("arg_lastError", last_error);
	} else if (!last_error.isEmpty() && use_last_error && isVisible()) {
		extra_data.pathValueMap.insert("lastError", last_error);
	}
	extra_data.urlEn = "Output error no url";

	return PLSErrorHandler::getAlertStringByErrCode(QString::number(code), PLSErrApiKey_OutputStream, {}, extra_data).alertMsg;
}

void PLSBasic::ShowOutputStreamErrorAlert(int code, QString last_error, bool vertical, QWidget *parent)
{
	if (code == OBS_OUTPUT_SUCCESS)
		return;

	PLSErrorHandler::ExtraData extra_data;
	bool encoder_error = (code == OBS_OUTPUT_ENCODE_ERROR);
	bool use_last_error = pls_is_in(code, OBS_OUTPUT_DISCONNECTED, OBS_OUTPUT_ERROR, OBS_OUTPUT_CONNECT_FAILED);
	if (encoder_error && !last_error.isEmpty()) {
		extra_data.pathValueMap.insert("hasErrorCode", "1");
		extra_data.pathValueMap.insert("arg_lastError", last_error);
	} else if (!last_error.isEmpty() && use_last_error && isVisible()) {
		// the lastError is used to appended to the alert msg str.
		extra_data.pathValueMap.insert("lastError", last_error);
	}

	// Set output plugin id as the urlEn
	if (outputHandler) {
		if (vertical && outputHandler.voutput && outputHandler.voutput->streamOutput) {
			extra_data.urlEn = obs_output_get_id(outputHandler.voutput->streamOutput);
		} else if (!vertical && outputHandler->streamOutput) {
			extra_data.urlEn = obs_output_get_id(outputHandler->streamOutput);
		}
	}
	extra_data.urlEn = extra_data.urlEn.isEmpty() ? "stream output" : extra_data.urlEn;

	if (encoder_error || (!encoder_error && isVisible())) {
		PLSErrorHandler::showAlertByErrCode(QString::number(code), PLSErrApiKey_OutputStream, {}, extra_data, parent);
	} else {
		auto result = PLSErrorHandler::getAlertStringByErrCode(QString::number(code), PLSErrApiKey_OutputStream, {}, extra_data);
		SysTrayNotify(result.alertMsg, QSystemTrayIcon::Warning);
	}
}

void PLSBasic::ShowOutputRecordErrorAlert(int code, QString last_error, QWidget *parent)
{
	if (code == OBS_OUTPUT_SUCCESS)
		return;

	PLSErrorHandler::ExtraData extra_data;
	bool encoder_error = (code == OBS_OUTPUT_ENCODE_ERROR);
	if (encoder_error && !last_error.isEmpty()) {
		extra_data.pathValueMap.insert("hasErrorCode", "1");
		extra_data.pathValueMap.insert("arg_lastError", last_error);
	} else if (!last_error.isEmpty() && isVisible()) {
		// lastError is used to appended to the alert msg str.
		extra_data.pathValueMap.insert("lastError", last_error);
	}

	// Set output plugin id as the urlEn
	if (outputHandler && outputHandler->fileOutput && !pls_is_empty(obs_output_get_id(outputHandler->fileOutput)))
		extra_data.urlEn = obs_output_get_id(outputHandler->fileOutput);
	else
		extra_data.urlEn = "record output";

	if (isVisible()) {
		PLSErrorHandler::showAlertByErrCode(QString::number(code), PLSErrApiKey_OutputRecord, //
						    PLSErrCustomKey_OutputRecordFailed, extra_data, parent);
	} else {
		auto result = PLSErrorHandler::getAlertStringByErrCode(QString::number(code), PLSErrApiKey_OutputRecord, //
								       PLSErrCustomKey_OutputRecordFailed, extra_data);
		SysTrayNotify(result.alertMsg, QSystemTrayIcon::Warning);
	}
}

void PLSBasic::ShowReplayBufferErrorAlert(int code, QString last_error, QWidget *parent)
{
	Q_UNUSED(last_error)
	if (code == OBS_OUTPUT_SUCCESS)
		return;

	// Set output plugin id as the urlEn
	PLSErrorHandler::ExtraData extra_data{};
	if (outputHandler && outputHandler->replayBuffer && !pls_is_empty(obs_output_get_id(outputHandler->replayBuffer)))
		extra_data.urlEn = obs_output_get_id(outputHandler->replayBuffer);
	else
		extra_data.urlEn = "replay buffer";

	if (isVisible()) {
		PLSErrorHandler::showAlertByErrCode(QString::number(code), PLSErrApiKey_OutputRecord, //
						    PLSErrCustomKey_OutputRecordFailed, extra_data, parent);
	} else {
		auto result = PLSErrorHandler::getAlertStringByErrCode(QString::number(code), PLSErrApiKey_OutputRecord, //
								       PLSErrCustomKey_OutputRecordFailed, extra_data);
		SysTrayNotify(result.alertMsg, QSystemTrayIcon::Warning);
	}
}

void PLSBasic::ReorderAudioMixer()
{
	auto sceneUuid = obs_source_get_uuid(GetCurrentSceneSource());
	if (sceneUuid) {
		PLSBasic::instance()->mixerOrder.Reorder(sceneUuid, volumes);

		bool vertical = config_get_bool(App()->GetUserConfig(), "BasicWindow", "VerticalVolControl");
		for (auto volume : volumes) {
			if (vertical)
				ui->vVolumeWidgets->AddWidget(volume);
			else
				ui->hVolumeWidgets->AddWidget(volume);
		}
	}
}

bool PLSBasic::GetSourceIcon(QIcon &icon, int type) const
{
	switch (static_cast<pls_icon_type>(type)) {
	case PLS_ICON_TYPE_VIEWER_COUNT:
		icon = GetViewerCountIcon();
		return true;
	case PLS_ICON_TYPE_NDI:
		icon = GetNdiIcon();
		return true;
	case PLS_ICON_TYPE_BGM:
		icon = GetBgmIcon();
		return true;
	case PLS_ICON_TYPE_GIPHY:
		icon = GetStickerIcon();
		return true;
	case PLS_ICON_TYPE_TEXT_TEMPLATE:
		icon = GetTextMotionIcon();
		return true;
	case PLS_ICON_TYPE_REGION:
		icon = GetRegionIcon();
		return true;
	case PLS_ICON_TYPE_CHAT:
		icon = GetChatIcon();
		return true;
	case PLS_ICON_TYPE_SPECTRALIZER:
		icon = GetAudiovIcon();
		return true;
	case PLS_ICON_TYPE_VIRTUAL_BACKGROUND:
		icon = GetVirtualBackgroundIcon();
		return true;
	case PLS_ICON_TYPE_PRISM_MOBILE:
		icon = GetPrismMobileIcon();
		return true;
	case PLS_ICON_TYPE_PRISM_STICKER:
		icon = GetPrismStickerIcon();
		return true;
	case PLS_ICON_TYPE_PRISM_TIMER:
		icon = GetTimerIcon();
		return true;
	case PLS_ICON_TYPE_APP_AUDIO:
		icon = GetAppAudioIcon();
		return true;
	case PLS_ICON_TYPE_DECKLINK_INPUT:
		icon = GetDecklinkInputIcon();
		return true;

	default:
		return false;
	}
}

QIcon PLSBasic::GetViewerCountIcon() const
{
	return viewerCountIcon;
}

void PLSBasic::SetViewerCountIcon(const QIcon &icon)
{
	viewerCountIcon = icon;
}

QIcon PLSBasic::GetNdiIcon() const
{
	return ndiIcon;
}

void PLSBasic::SetNdiIcon(const QIcon &icon)
{
	ndiIcon = icon;
}

QIcon PLSBasic::GetBgmIcon() const
{
	return bgmIcon;
}

QIcon PLSBasic::GetStickerIcon() const
{
	return stickerIcon;
}

QIcon PLSBasic::GetTextMotionIcon() const
{
	return textMotionIcon;
}

QIcon PLSBasic::GetRegionIcon() const
{
	return regionIcon;
}

QIcon PLSBasic::GetChatIcon() const
{
	return chatIcon;
}

QIcon PLSBasic::GetPrismMobileIcon() const
{
	return prismMobileIcon;
}

QIcon PLSBasic::GetAudiovIcon() const
{
	return audiovIcon;
}
QIcon PLSBasic::GetVirtualBackgroundIcon() const
{
	return virtualBackgroundIcon;
}

QIcon PLSBasic::GetPrismStickerIcon() const
{
	return prismStickerIcon;
}

QIcon PLSBasic::GetTimerIcon() const
{
	return timerIcon;
}

QIcon PLSBasic::GetAppAudioIcon() const
{
	return appAudioIcon;
}

QIcon PLSBasic::GetDecklinkInputIcon() const
{
	return decklinkInputIcon;
}

void PLSBasic::SetStickerIcon(const QIcon &icon)
{
	stickerIcon = icon;
}

void PLSBasic::SetPrismMobileIcon(const QIcon &icon)
{
	prismMobileIcon = icon;
}

void PLSBasic::SetBgmIcon(const QIcon &icon)
{
	bgmIcon = icon;
}

void PLSBasic::SetTextMotionIcon(const QIcon &icon)
{
	textMotionIcon = icon;
}

void PLSBasic::SetRegionIcon(const QIcon &icon)
{
	regionIcon = icon;
}

void PLSBasic::SetChatIcon(const QIcon &icon)
{
	chatIcon = icon;
}

void PLSBasic::SetAudiovIcon(const QIcon &icon)
{
	audiovIcon = icon;
}
void PLSBasic::SetVirtualBackgroundIcon(const QIcon &icon)
{
	virtualBackgroundIcon = icon;
}

void PLSBasic::SetPrismStickerIcon(const QIcon &icon)
{
	prismStickerIcon = icon;
}

void PLSBasic::SetTimerIcon(const QIcon &icon)
{
	timerIcon = icon;
}

void PLSBasic::SetAppAudioIcon(const QIcon &icon)
{
	appAudioIcon = icon;
}

void PLSBasic::SetDecklinkInputIcon(const QIcon &icon)
{
	decklinkInputIcon = icon;
}

bool PLSBasic::CheckStreamEncoder() const
{ //to do
	return true;
}

int PLSBasic::showSettingView(const QString &tab, const QString &group)
{
	static bool settings_already_executing = false;

	/* Do not load settings window if inside of a temporary event loop
		 * because we could be inside of an Auth::LoadUI call.  Keep trying
		 * once per second until we've exit any known sub-loops. */
	if (os_atomic_load_long(&insideEventLoop) != 0) {
		QTimer::singleShot(1000, this, [tab, group, this]() { onPopupSettingView(tab, group); });
		return -1;
	}

	if (settings_already_executing) {
		return -1;
	}

	settings_already_executing = true;

	OBSBasicSettings settings(this);
	connect(this, &PLSBasic::mainClosing, &settings, &OBSBasicSettings::cancel);
	settings.switchToDualOutputMode(tab, group);
	settings.exec();
	pls_modal_check_app_exiting(-1);
	int result = settings.result();
	SystemTray(false);

	settings_already_executing = false;

	if (QWidget *widget = QApplication::focusWidget(); widget) {
		widget->clearFocus();
	}
	return result;
}

void PLSBasic::setAlertParentWithBanner(setAlertParent cb)
{
	PLSLaunchWizardView *banner = dynamic_cast<PLSLaunchWizardView *>(pls_get_banner_widget());
	if (banner && banner->isNeedShow()) {
		PLS_INFO(UPDATE_MODULE, "pls_get_banner_widget object is existed");
		PLSShowWatcher *watcher = new PLSShowWatcher(banner);
		QObject::connect(
			watcher, &PLSShowWatcher::signalShow, banner,
			[watcher, banner, cb]() {
				PLS_INFO(UPDATE_MODULE, "PLSShowWatcher notify banner show event");
				watcher->deleteLater();
				cb(banner);
				banner->activateWindow();
				banner->raise();
			},
			Qt::ConnectionType(Qt::QueuedConnection | Qt::SingleShotConnection));
	} else {
		PLS_INFO(UPDATE_MODULE, "pls_get_banner_widget object is empty");
		cb(getMainView());
	}
}

void PLSBasic::PrismLogout() const
{
	// do not clean scene collection info
	PLSApp::plsApp()->backupGolbalConfig();

	pls_prism_logout();
}

void PLSBasic::UpdateStudioPortraitLayoutUI(bool studioMode, bool studioPortraitLayout)
{
	if (!studioMode) {
		if (previewProgramTitle) {
			previewProgramTitle->toggleShowHide(false);
		}
		return;
	}

	if (!previewProgramTitle) {
		CreatePreviewTitle();
	}
	previewProgramTitle->SetStudioPortraitLayout(studioPortraitLayout);

	if (studioPortraitLayout) {
		ui->verticalLayout_3->insertWidget(0, previewProgramTitle->GetEditArea());
		ui->verticalLayout_3->addWidget(previewProgramTitle->GetLiveArea());
		ui->verticalLayout_3->addWidget(program);
	} else {
		ui->verticalLayout_3->insertWidget(0, previewProgramTitle->GetTotalArea());
		ui->perviewLayoutHrz->addWidget(program);
	}

	const char *editScene = obs_source_get_name(obs_scene_get_source(GetCurrentScene()));
	previewProgramTitle->setEditTitle(editScene);
	const char *liveScene = obs_source_get_name(GetProgramSource());
	previewProgramTitle->setLiveTitle(liveScene);
}

uint PLSBasic::getStartTimeStamp() const
{
	if (!mainView->statusBar()) {
		return 0;
	}
	return mainView->statusBar()->getStartTime();
}

void PLSBasic::updateMainViewAlwayTop()
{
	if (ui->actionAlwaysOnTop->isChecked()) {
		pls_async_call_mt(ui->actionAlwaysOnTop, [this]() { SetAlwaysOnTop(mainView, true); });
	}
}

void PLSBasic::checkTwitchAudio()
{
	const char *mode = config_get_string(activeConfiguration, "Output", "Mode");
	if (!mode)
		return;
	const char *aencoderid = "";
	bool advOut = astrcmpi(mode, "Advanced") == 0;
	if (!advOut) {
		// simple streaming
		aencoderid = config_get_string(activeConfiguration, "SimpleOutput", "StreamAudioEncoder");
	} else {
		aencoderid = config_get_string(activeConfiguration, "AdvOut", "AudioEncoder");
	}
	PLS_INFO(MAINFRAME_MODULE, "get audioEncoder %s", aencoderid);
	QString strAencoder = aencoderid;
	bool bWHIP = PLSPlatformApi::instance()->isTwitchWHIP();
	if (bWHIP) {
		if (!strAencoder.contains("opus", Qt::CaseInsensitive)) {
			config_set_string(activeConfiguration, "SimpleOutput", "StreamAudioEncoder", "opus");
			config_set_string(activeConfiguration, "AdvOut", "AudioEncoder", "ffmpeg_opus");
			PLS_INFO(MAINFRAME_MODULE, "need modify audioEncoder to opus");
		}
	} else {
		if (!strAencoder.contains("aac", Qt::CaseInsensitive)) {
			config_set_string(activeConfiguration, "SimpleOutput", "StreamAudioEncoder", "aac");
			config_set_string(activeConfiguration, "AdvOut", "AudioEncoder", "ffmpeg_aac");
			PLS_INFO(MAINFRAME_MODULE, "need modify audioEncoder to aac");
		}
	}
	if (!PLS_PLATFORM_API->isGoLive()) {
		ResetOutputs();
		config_save_safe(activeConfiguration, "tmp", nullptr);
	}
}

void PLSBasic::checkTwitchAudioWithUuid(const QString &channelUUID)
{
	auto channelInfo = PLSCHANNELS_API->getChannelInfo(channelUUID);
	if (channelInfo.isEmpty()) {
		return;
	}
	bool isToShow = getInfo(channelInfo, ChannelData::g_displayState, true);
	if (!isToShow) {
		return;
	}
	checkTwitchAudio();
}

void PLSBasic::checkTwitchAudioWithLeader(bool bLeader)
{
	if (bLeader) {
		checkTwitchAudio();
	}
}

bool PLSBasic::checkRecEncoder()
{
	const char *mode = config_get_string(activeConfiguration, "Output", "Mode");
	bool adv = astrcmpi(mode, "Advanced") == 0;
	bool shared = false;

	if (adv) {
		const char *recType = config_get_string(activeConfiguration, "AdvOut", "RecType");

		if (astrcmpi(recType, "FFmpeg") == 0) {
			shared = config_get_bool(activeConfiguration, "AdvOut", "FFOutputToFile");
		} else {
			const char *recordEncoder = config_get_string(activeConfiguration, "AdvOut", "RecEncoder");
			shared = astrcmpi(recordEncoder, "none") == 0;
		}
	} else {
		const char *quality = config_get_string(activeConfiguration, "SimpleOutput", "RecQuality");
		shared = strcmp(quality, "Stream") == 0;
	}
	return shared;
}

void PLSBasic::createNcb2bBrowserSettings()
{
	if (ncb2bBrowserSettings) {
		return;
	}
	DialogInfo info;
	info.configId = ConfigId::Ncb2bBrowserSettings;
	ncb2bBrowserSettings = pls_new<PLSNCB2bBrowserSettings>(info);
	ncb2bBrowserSettings->setVisible(false);
}

void PLSBasic::onBrowserSettingsClicked()
{
	if (!ncb2bBrowserSettings) {
		createNcb2bBrowserSettings();
		ncb2bBrowserSettings->show();
		ncb2bBrowserSettings->raise();
	} else {
		bool visible = !ncb2bBrowserSettings->isVisible();
		ncb2bBrowserSettings->setVisible(visible);
		if (visible)
			ncb2bBrowserSettings->raise();
	}
}

void PLSBasic::removeChzzkSponsorSource()
{
	int CHzzkSourceCount = 0;
	auto cb = [](void *param, obs_source_t *source) {
		auto id = obs_source_get_id(source);
		if (pls_is_equal(id, PRISM_CHZZK_SPONSOR_SOURCE_ID)) {
			obs_source_remove(source);
			++(*reinterpret_cast<int *>(param));
		}
		return true;
	};
	obs_enum_sources(cb, &CHzzkSourceCount);
	if (CHzzkSourceCount > 0) {
		PLS_INFO(MAINFRAME_MODULE, "exist chzzk sponsor source, and delete the undo queue.");
		undo_s.clear();
	}
}

void PLSBasic::setAllChzzkSourceSetting(const QString &platformName)
{
	if (platformName != CHZZK) {
		return;
	}
	auto setChzzkSponsorSourceCb = [this](obs_source_t *source) {
		auto id = obs_source_get_id(source);
		if (pls_is_equal(id, PRISM_CHZZK_SPONSOR_SOURCE_ID)) {
			setChzzkSponsorSourceSetting(source);
		}
		return true;
	};

	using SetSettingForSponsorSource_t = decltype(setChzzkSponsorSourceCb);
	auto cb = [](void *param, obs_source_t *source) {
		auto ref = (SetSettingForSponsorSource_t *)(param);
		if (!ref)
			return false;
		return (*ref)(source);
	};
	obs_enum_sources(cb, &setChzzkSponsorSourceCb);
}

bool PLSBasic::setChzzkSponsorSourceSetting(obs_source_t *source)
{
	if (!source) {
		PLS_WARN(MAINFRAME_MODULE, "Chzzk source is empty");
		return false;
	}
	auto chzzkPlatform = PLS_PLATFORM_API->getExistedPlatformByType(PLSServiceType::ST_CHZZK);
	if (!chzzkPlatform) {
		PLS_WARN(MAINFRAME_MODULE, "Chzzk Platform is empty");
		return false;
	}
	auto data = chzzkPlatform->getInitData();
	auto chzzkExtraData = data.value(ChannelData::g_chzzkExtraData);
	auto channelId = data.value(ChannelData::g_subChannelId).toString();
	OBSData settings = obs_source_get_settings(source);
	obs_data_release(settings);
	obs_data_set_string(settings, "channelId", channelId.toUtf8().constData());
	auto obj = chzzkExtraData.toJsonObject();
	QString chatDonationUrl = obj.value("chatDonationUrl").toString();
	QString videoDonationUrl = obj.value("videoDonationUrl").toString();
	QString missionDonationUrl = obj.value("missionDonationUrl").toString();
	obs_data_set_string(settings, "chatDonationUrl", chatDonationUrl.toUtf8().constData());
	obs_data_set_string(settings, "videoDonationUrl", videoDonationUrl.toUtf8().constData());
	obs_data_set_string(settings, "missionDonationUrl", missionDonationUrl.toUtf8().constData());
	if (chatDonationUrl.isEmpty() || videoDonationUrl.isEmpty() || missionDonationUrl.isEmpty()) {
		PLS_WARN(MAINFRAME_MODULE, "Chzzk Platform DonationUrl is empty");
		return false;
	}
	obs_source_update(source, settings);

	return true;
}

bool PLSBasic::bSuccessGetChzzkSourceUrl(QWidget *parent)
{
	auto chzzkPlatform = PLS_PLATFORM_API->getExistedPlatformByType(PLSServiceType::ST_CHZZK);
	if (!chzzkPlatform) {
		PLS_WARN(MAINFRAME_MODULE, "Chzzk Platform is empty");
		return false;
	}
	auto data = chzzkPlatform->getInitData();
	auto chzzkExtraData = data.value(ChannelData::g_chzzkExtraData);
	auto obj = chzzkExtraData.toJsonObject();
	QString chatDonationUrl = obj.value("chatDonationUrl").toString();
	QString videoDonationUrl = obj.value("videoDonationUrl").toString();
	QString missionDonationUrl = obj.value("missionDonationUrl").toString();
	if (chatDonationUrl.isEmpty() || videoDonationUrl.isEmpty() || missionDonationUrl.isEmpty()) {
		PLS_WARN(MAINFRAME_MODULE, "Chzzk Platform DonationUrl is empty");
		PLSAlertView::Button button = PLSAlertView::warning(
			parent, QTStr("Alert.Title"), QTStr("CHZZKSponsor.GetUrlFail"),
			QMap<PLSAlertView::Button, QString>({{PLSAlertView::Button::Open, QTStr("CHZZKSponsor.GotoVerification")}, {PLSAlertView::Button::Cancel, QTStr("Cancel")}}));
		if (button == PLSAlertView::Button::Open) {
			auto channelId = data.value(ChannelData::g_subChannelId).toString();
			auto url = QString("%1/%2/createProfit").arg(g_plsChzzkStudioHost).arg(channelId);
			QDesktopServices::openUrl(QUrl(url));
		}

		return false;
	}
	return true;
}

void PLSBasic::onPopupSettingView(const QString &tab, const QString &group)
{
	if (!mainView->isSettingEnabled()) {
		return;
	}

	mainView->setSettingIconCheck(true);
	int result = showSettingView(tab, group);
	mainView->setSettingIconCheck(false);
	if (result == static_cast<int>(LoginInfoType::PrismLogoutInfo)) {
		m_isLogout = true;
		PrismLogout();
	} else if (result == static_cast<int>(LoginInfoType::PrismSignoutInfo)) {
		m_isLogout = true;
		PLSApp::plsApp()->backupGolbalConfig();
		pls_prism_signout();
	} else if (result == Qt::UserRole + RESTARTAPP) {
		m_isUpdateLanguage = true;
		restartPrismApp();
	} else if (result == Qt::UserRole + NEED_RESTARTAPP) {
		PLSAlertView::Button button = PLSMessageBox::question(this, QTStr("Confirm"), QTStr("Basic.Settings.NeedRestart"), PLSAlertView::Button::Yes | PLSAlertView::Button::No);
		if (button == PLSAlertView::Button::Yes) {
			restartPrismApp();
		}
	} else {
		goLiveCheckTooltip();
	}
}

void PLSBasic::moveStatusPanel()
{
	if (m_dialogStatusPanel) {
		int leftBottomMargin = 5;
		QPoint mainViewGlobalPos = getMainView()->mapToGlobal(QPoint(0, 0));

		QPoint targetPos;
		targetPos.setX(mainViewGlobalPos.x() + leftBottomMargin);
		targetPos.setY(mainViewGlobalPos.y() + getMainView()->height() - getMainView()->getBottomAreaHeight() - m_dialogStatusPanel->height() - leftBottomMargin);
		m_dialogStatusPanel->move(targetPos);
	}
}

void PLSBasic::singletonWakeup()
{
	PLS_INFO(MAINFRAME_MODULE, "singleton instance wakeup");

	if (pls_get_app_exiting() || m_isAppUpdating) {
		return;
	}

	if (PLSApp::plsApp()->isAppRunning()) {

		RunPrismByPscPath();

		if (pls_is_main_window_closing()) {
			PLS_INFO(MAINFRAME_MODULE, "application is closing, don't wakeup");
			return;
		}

		PLS_INFO(MAINFRAME_MODULE, "wakeup main window");
		SetShowing(true);

		// bring window to top
		bringWindowToTop(mainView);
	} else {
		PLS_INFO(MAINFRAME_MODULE, "application is starting, don't wakeup");
	}
}

void PLSBasic::showEncodingInStatusBar() const
{
	mainView->statusBar()->setEncoding((int)config_get_int(activeConfiguration, "Video", "OutputCX"), (int)config_get_int(activeConfiguration, "Video", "OutputCY"));

	uint32_t fpsNum = 0;
	uint32_t fpsDen = 0;
	GetConfigFPS(fpsNum, fpsDen);
	mainView->statusBar()->setFps(QString("%1fps").arg(fpsNum / fpsDen));
}

bool PLSBasic::toggleStatusPanel(int iSwitch)
{
	if (!m_dialogStatusPanel) {
		m_dialogStatusPanel = pls_new<PLSBasicStatusPanel>(getMainView());
	}

	switch (iSwitch) {
	case -1:
		if (m_dialogStatusPanel->isVisible()) {
			m_dialogStatusPanel->close();
		} else {
			m_dialogStatusPanel->show();
			m_dialogStatusPanel->adjustSize();
		}
		break;

	case 0:
		m_dialogStatusPanel->close();
		break;

	case 1:
		if (m_dialogStatusPanel->isVisible()) {
			m_dialogStatusPanel->close();
		}
		m_dialogStatusPanel->show();
		m_dialogStatusPanel->adjustSize();
		break;

	default:
		break;
	}

	if (m_dialogStatusPanel->isVisible()) {
		moveStatusPanel();
		ui->stats->setChecked(true);
		mainView->statusBar()->setStatsOpen(true);
	} else {
		ui->stats->setChecked(false);
		mainView->statusBar()->setStatsOpen(false);
	}

	return m_dialogStatusPanel->isVisible();
}

void PLSBasic::updateStatusPanel(PLSBasicStatusData &dataStatus) const
{
	if (m_dialogStatusPanel) {
		m_dialogStatusPanel->updateStatusPanel(dataStatus);
	}
}

void PLSBasic::OnUseVideoException(void *data, calldata_t *calldata) {}

void PLSBasic::showEvent(QShowEvent *event)
{
	OBSBasic::showEvent(event);
}

void PLSBasic::resizeEvent(QResizeEvent *event)
{
	if (m_dialogStatusPanel && m_dialogStatusPanel->isVisible()) {
		moveStatusPanel();
		pls_async_call(this, [this]() { moveStatusPanel(); });
	}

	if (previewProgramTitle) {
		previewProgramTitle->CustomResize();
	}

	OBSBasic::resizeEvent(event);
}

bool PLSBasic::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == getMainView() && event->type() == QEvent::Move) {
		if (m_dialogStatusPanel && m_dialogStatusPanel->isVisible()) {
			moveStatusPanel();
			pls_async_call(this, [this]() { moveStatusPanel(); });
		}
	}
	return QWidget::eventFilter(watched, event);
}

bool PLSBasic::CheckHideInteraction(OBSSceneItem item)
{
	if (!interaction_sceneitem_pointer) {
		return true;
	}

	if (item && item == interaction_sceneitem_pointer) {
		OBSSource source = obs_sceneitem_get_source(item);
		if (source) {
			ShowInteractionUI(source, false);
			interaction_sceneitem_pointer = nullptr;
		}

		return true;
	}

	return false;
}

void PLSBasic::HideAllInteraction(obs_source_t *except_source)
{
	auto CloseInteractionCb = [](void *param, obs_source_t *source) {
		auto temp = (obs_source_t *)param;
		if (source && source != temp) {
			auto main = dynamic_cast<PLSBasic *>(App()->GetMainWindow());
			main->ShowInteractionUI(source, false);
		}
		return true;
	};

	obs_enum_sources(CloseInteractionCb, except_source);

	if (!except_source) {
		// all interactions have been closed
		interaction_sceneitem_pointer = nullptr;
	}
}

void PLSBasic::ShowInteractionUI(obs_source_t *source, bool show) const
{
	if (!source) {
		return;
	}

	const char *pluginID = obs_source_get_id(source);
	if (!pluginID || 0 != strcmp(pluginID, BROWSER_SOURCE_ID)) {
		return;
	}

	obs_data_t *data = obs_data_create();
	if (show) {
		obs_data_set_string(data, "method", "ShowInteract");
	} else {
		obs_data_set_string(data, "method", "HideInterct");
	}

	pls_source_set_private_data(source, data);
	obs_data_release(data);
}

void PLSBasic::OnSourceInteractButtonClick(OBSSource source)
{
	OBSSceneItem item = nullptr;

	if (source == nullptr) {
		item = GetCurrentSceneItem();
		source = obs_sceneitem_get_source(item);
	}

	if (source) {
		HideAllInteraction(source);
		ShowInteractionUI(source, true);
		interaction_sceneitem_pointer = item;
	}
}

void PLSBasic::OnRemoveSceneItem(OBSSceneItem item)
{
	CheckHideInteraction(item);
}

void PLSBasic::OnSourceNotify(void *v, calldata_t *params)
{
	auto main = dynamic_cast<PLSBasic *>(App()->GetMainWindow());
	if (!main) {
		return;
	}

	auto source = (obs_source_t *)calldata_ptr(params, "source");
	if (!source) {
		return;
	}

	const char *name = obs_source_get_name(source);
	if (!name) {
		return;
	}

	auto type = (int)calldata_int(params, "message");
	auto sub_code = (int)calldata_int(params, "sub_code");
	QMetaObject::invokeMethod(main, "OnSourceNotifyAsync", Qt::QueuedConnection, Q_ARG(QString, name), Q_ARG(int, type), Q_ARG(int, sub_code));

	//PRISM/Zhangdewen/20220928/#/Viewer Count Source Flicker Optimize
	if (type == OBS_SOURCE_VIEWER_COUNT_UPDATE_PARAMS) {
		// notify viewer count source
		QByteArray viewerCountJson = buildPlatformViewerCount();
		PLS_INFO("ViewCount-Source", "source: %s, viewerCountEvent: %s", obs_source_get_name(source), viewerCountJson.constData());
		pls_source_update_extern_params_json(source, viewerCountJson.constData(), sub_code);

		if (obs_frontend_streaming_active()) {
			// notify viewer count source
			QByteArray liveStartJson = pls::JsonDocument<QJsonObject>().add("type", "liveStart").toByteArray();
			PLS_INFO("ViewCount-Source", "viewerCountEvent: %s", liveStartJson.constData());
			pls_source_update_extern_params_json(source, liveStartJson.constData(), sub_code);
		}
	}
}

void PLSBasic::OnSourceMessage(void *data, calldata_t *calldata)
{
	auto main = dynamic_cast<PLSBasic *>(App()->GetMainWindow());
	if (!main) {
		return;
	}

	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (!source) {
		return;
	}

	auto type = (int)calldata_int(calldata, "event_type");
	auto msg_data = (obs_data_t *)calldata_ptr(calldata, "message");

	if (!msg_data)
		return;

	if (type == PLS_SOURCE_GAME_CAPTURE_FAILED_MSG || type == PLS_SOURCE_GAME_CAPTURE_SUCCESS_MSG) {
		auto exe = obs_data_get_string(msg_data, "game_exe");
		auto exe_version = obs_data_get_string(msg_data, "game_version");
		auto game_title = obs_data_get_string(msg_data, "game_title");
		auto reason = obs_data_get_string(msg_data, "game_event_type");
		std::string source_name = obs_source_get_name(source);
		std::string source_id = obs_source_get_id(source);
		bool wasCaptured = (type == PLS_SOURCE_GAME_CAPTURE_SUCCESS_MSG);

		std::string key = std::string(game_title) + std::string(obs_source_get_uuid(source));
		std::lock_guard<std::recursive_mutex> autoLock(main->m_lockGameCaptureResultMap);
		auto itr = main->m_gameCaptureResultMap.find(key);
		if (itr == main->m_gameCaptureResultMap.end()) {
			auto game = std::make_shared<GameCaptureResult>(exe ? std::string(exe) : "Unknown", exe_version, game_title, source_name, source_id);
			game->insertResult({reason, wasCaptured});
			main->m_gameCaptureResultMap[key] = game;
		} else {
			auto game = main->m_gameCaptureResultMap[key];
			game->insertResult({reason, wasCaptured});
		}
	} else if (type == OBS_SOURCE_VST_CHANGED) { // VST plugin
		auto vstPluginName = obs_data_get_string(msg_data, "vstPlugin");
		if (vstPluginName) {
			PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"vstPlugin", vstPluginName}}, "[VST Plug-in: %p] New vst plugin: '%s' selected.", source, vstPluginName);
		}
	}

	obs_data_release(msg_data);
}

void PLSBasic::RenderVerticalProgram(void *data, uint32_t cx, uint32_t cy) {}

bool PLSBasic::IsSupportEncoder(const QString &encoderId)
{
	bool res = false;
	if (encoderId.isEmpty()) {
		PLS_WARN(MAINFRAME_MODULE, "CheckSupportEncoder param encoderId is empty");
		return res;
	} else {
		PLS_INFO(MAINFRAME_MODULE, "CheckSupportEncoder param encoderId is %s", encoderId.toUtf8().constData());
	}
	ChannelsMap temp = PLSChannelDataAPI::getInstance()->getCurrentSelectedChannels();
	if (temp.size() == 1) {
		auto info = *temp.begin();
		auto channelName = info.value(ChannelData::g_channelName, "").toString();
		auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();
		auto serviceData = PLSBasic::instance()->LoadServiceData();
		OBSDataAutoRelease settings = obs_data_get_obj(serviceData, "settings");
		const char *service = obs_data_get_string(settings, "service");
		QString strService = service;
		PLS_INFO(MAINFRAME_MODULE, "CheckSupportEncoder current channel is %s,LoadServiceData service is %s", channelName.toUtf8().constData(), service);
		if (dataType == ChannelData::ChannelType) {

			if (channelName == YOUTUBE) {
				if (!pls_is_equal(service, YOUTUBE_HLS)) {
					strService = YOUTUBE_RTMP;
				}
			} else if (channelName == TWITCH) {
				if (!strService.contains("Twitch", Qt::CaseInsensitive)) {
					strService = WHIP_SERVICE;
				} else {
					strService = TWITCH_SERVICE;
				}
			} else if (channelName == NAVER_SHOPPING_LIVE || channelName == BAND || channelName == NAVER_TV || channelName == CHZZK) {
				strService = channelName;
			} else if (channelName == FACEBOOK) {
				strService = FACEBOOK_SERVICE;
			} else if (channelName == AFREECATV) {
				strService = AFREECATV_SERVICE;
			} else {
				strService = "DEFALUT";
			}
		} else if (dataType >= ChannelData::CustomType) {
			strService = CUSTOM_RTMP;
		}
		auto iter = channelSupportVideoEncoderMap.find(strService);
		if (iter != channelSupportVideoEncoderMap.end()) {
			QList<QString> supportEncoders = iter.value();
			if (supportEncoders.contains(encoderId, Qt::CaseInsensitive)) {
				res = true;
			}
		}
	} else {
		PLS_INFO(MAINFRAME_MODULE, "CheckSupportEncoder current channel count > 1");
		if (encoderId == "h264") {
			res = true;
		}
	}

	return res;
}

bool PLSBasic::CheckAndModifyAAC()
{
	const char *mode = config_get_string(activeConfiguration, "Output", "Mode");
	if (!mode) {
		PLS_ERROR(MAINFRAME_MODULE, "get mode is null");
		return false;
	}
	const char *aencoderid = "";
	bool advOut = astrcmpi(mode, "Advanced") == 0;
	if (!advOut) {
		// simple streaming
		aencoderid = config_get_string(activeConfiguration, "SimpleOutput", "StreamAudioEncoder");
	} else {
		aencoderid = config_get_string(activeConfiguration, "AdvOut", "AudioEncoder");
	}

	PLS_INFO(MAINFRAME_MODULE, "get audioEncoder %s", aencoderid);
	QString strAencoder = aencoderid;
	bool bModifyOpus = false;
	bool bModifyAAC = false;
	bool bWHIP = PLSPlatformApi::instance()->isTwitchWHIP();
	if (bWHIP) {
		if (strAencoder.contains("opus", Qt::CaseInsensitive)) {
			bModifyOpus = false;
		} else {
			PLS_INFO(MAINFRAME_MODULE, "need modify audioEncoder to opus");
			bModifyOpus = true;
		}
	} else {
		if (strAencoder.contains("aac", Qt::CaseInsensitive)) {
			bModifyAAC = false;
		} else {
			PLS_INFO(MAINFRAME_MODULE, "need modify audioEncoder to aac");
			bModifyAAC = true;
		}
	}

	if (bModifyAAC || bModifyOpus) {
		auto showWarning = [this, bModifyAAC, bModifyOpus]() {
			auto alertResult = PLSAlertView::Button::No;
#define WARNING_VAL(x) QTStr("Basic.Settings.Output.Warn.ServiceCodecCompatibility." x)
			QString msg;
			if (bModifyAAC) {
				msg = WARNING_VAL("Msg").arg("", "Opus", "AAC").replace("\"\" ", "");
			} else {
				msg = WARNING_VAL("Msg").arg("", "AAC", "Opus").replace("\"\" ", "");
			}
			alertResult = PLSAlertView::question(getMainWindow(), tr("Confirm"), msg, PLSAlertView::Button::Yes | PLSAlertView::Button::No);
			if (alertResult == PLSAlertView::Button::Yes) {
				if (bModifyAAC) {
					config_set_string(activeConfiguration, "SimpleOutput", "StreamAudioEncoder", "aac");
					config_set_string(activeConfiguration, "AdvOut", "AudioEncoder", "ffmpeg_aac");
				} else if (bModifyOpus) {
					config_set_string(activeConfiguration, "SimpleOutput", "StreamAudioEncoder", "opus");
					config_set_string(activeConfiguration, "AdvOut", "AudioEncoder", "ffmpeg_opus");
				}
				ResetOutputs();
				config_save_safe(activeConfiguration, "tmp", nullptr);
			}
		};
		pls_async_call_mt(getMainWindow(), showWarning);
		return false;
	}

	return true;
}

bool PLSBasic::CheckSupportEncoder(QString &encoderId)
{
	const char *mode = config_get_string(activeConfiguration, "Output", "Mode");
	if (!mode)
		return false;
	const char *vencoderid = "";
	bool advOut = astrcmpi(mode, "Advanced") == 0;
	if (!advOut) {
		// simple streaming
		const char *vencoder = config_get_string(activeConfiguration, "SimpleOutput", "StreamEncoder");
		if (pls_is_empty(vencoder))
			return false;

		vencoderid = get_simple_output_encoder(vencoder);
	} else {
		vencoderid = config_get_string(activeConfiguration, "AdvOut", "Encoder");
	}

	if (pls_is_empty(vencoderid))
		return false;

	const char *codec = obs_get_encoder_codec(vencoderid);
	encoderId = codec;
	bool isSupport = IsSupportEncoder(encoderId);
	return isSupport;
}

void PLSBasic::showSettingVideo()
{
	onPopupSettingView(QStringLiteral("Output"), QString());
}

bool PLSBasic::isAppUpadting() const
{
	return m_isAppUpdating;
}

void OBSBasic::updateLiveStartUI()
{
	if (updatedLiveStart) {
		return;
	}
	updatedLiveStart = true;
	if (previewProgramTitle) {
		previewProgramTitle->OnLiveStatus(true);
	}
	ui->scenesFrame->OnLiveStatus(true);
	mainView->statusBar()->OnLiveStatus(true);
}

void OBSBasic::updateLiveEndUI()
{
	updatedLiveStart = false;
	if (previewProgramTitle) {
		previewProgramTitle->OnLiveStatus(false);
	}
	ui->scenesFrame->OnLiveStatus(false);
	mainView->statusBar()->OnLiveStatus(false);
}

void OBSBasic::updateRecordStartUI()
{
	if (previewProgramTitle) {
		previewProgramTitle->OnRecordStatus(true);
	}
	ui->scenesFrame->OnRecordStatus(true);
	mainView->statusBar()->OnRecordStatus(true);
}

void OBSBasic::updateRecordEndUI()
{
	if (previewProgramTitle) {
		previewProgramTitle->OnRecordStatus(false);
	}
	mainView->statusBar()->OnRecordStatus(false);
	ui->scenesFrame->OnRecordStatus(false);
}

void PLSBasic::setServiceName(QString strServiceName)
{
	if (strServiceName == "Twitch - WHIP") {
		strServiceName = "WHIP";
	} else if (strServiceName == "Twitch - RTMPS") {
		strServiceName = "Twitch";
	} else if (strServiceName == "YouTube") {
		strServiceName = "YouTube - RTMPS";
	}

	if (m_strServiceName == strServiceName) {
		return;
	}

	m_strServiceName = strServiceName;
}

const QString &PLSBasic::getServiceName() const
{
	return m_strServiceName;
}

bool PLSBasic::IsOutputActivated(obs_output *output)
{
	std::lock_guard locker(m_outputActivateMutex);
	return (m_outputActivating.find(output) != m_outputActivating.end());
}

void PLSBasic::ActivateOutput(obs_output *output)
{
	if (!output)
		return;

	std::lock_guard locker(m_outputActivateMutex);
	if (m_outputActivating.find(output) == m_outputActivating.end()) {
		m_outputActivating.insert(output);
	} else
		assert(false && "ActivateOutput: output is already in the list. ");
}

void PLSBasic::DeactivateOutput(obs_output *output)
{
	if (!output)
		return;

	std::lock_guard locker(m_outputActivateMutex);
	if (auto iter = m_outputActivating.find(output); iter != m_outputActivating.end()) {
		m_outputActivating.erase(iter);
	} else
		assert(false && "DeactivateOutput: output is not in the list. ");
}

void PLSBasic::updateSourceIcon()
{
	ui->sources->updateVisIconVisibleWhenDualoutput();
}

bool PLSBasic::getSourceLoaded()
{
	return loaded;
}

void PLSBasic::ForceUpdateGroupsSize()
{
	pls_enum_all_scenes(
		[](void *param, obs_source_t *source) {
			obs_scene_t *scene = obs_scene_from_source(source);
			pls_scene_enum_items_all(
				scene,
				[](obs_scene_t *, obs_sceneitem_t *item, void *) {
					if (obs_sceneitem_is_group(item)) {
						// force update group size
						obs_sceneitem_defer_group_resize_begin(item);
						obs_sceneitem_defer_group_resize_end(item);
					}
					return true;
				},
				param);

			return true;
		},
		this);
}

bool OBSBasic::queryRemoveSourceItem(OBSSceneItem item)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!source) {
		return false;
	}
	if (!QueryRemoveSource(source, this)) {
		return false;
	}

	/* ----------------------------------------------- */
	/* save undo data                                  */
	OBSScene scene = GetCurrentScene();
	obs_source_t *scene_source = obs_scene_get_source(scene);
	OBSData undo_data = BackupScene(scene_source);

	/* ----------------------------------------------- */
	/* remove items                                    */
	PLSSceneitemMapMgrInstance->removeItem(item);
	obs_sceneitem_remove(item);

	/* ----------------------------------------------- */
	/* save redo data                                  */

	OBSData redo_data = BackupScene(scene_source);

	/* ----------------------------------------------- */
	/* add undo/redo action                            */
	QString str = QTStr("Undo.Delete");
	QString action_name = str.arg(obs_source_get_name(obs_sceneitem_get_source(item)));

	CreateSceneUndoRedoAction(action_name, undo_data, redo_data);

	return true;
}

void PLSBasic::onDualOutputClicked()
{
	bool onOff = !pls_is_dual_output_on();
	setDualOutputEnabled(onOff, true);
}

bool PLSBasic::checkStudioMode()
{
	if (IsPreviewProgramMode()) {
		if (PLSAlertView::Button::No == PLSAlertView::question(pls_get_toplevel_view(this), QTStr("DualOutput.Title"), QTStr("DualOutput.CloseStudioMode"))) {
			return false;
		}

		SetPreviewProgramMode(false);
	}

	return true;
}

bool PLSBasic::setDualOutputEnabled(bool bEnabled, bool bInvokeCore)
{
	//bInvokeCore means whether invoke core function like pls_add_vertical_view
	//It's false means core function have been invoked before load 3rd plugins, and it's only once
	//It's always true if user clicked side bar button

	//if not invoke core function, don't need check video is active or not
	if (bInvokeCore && obs_video_active()) {
		PLSAlertView::warning(this, QTStr("DualOutput.Title"), QTStr("DualOutput.3rd.Plugin.In.Using").arg(pls_get_active_output_name(0)));
		return false;
	}

	if (bInvokeCore && pls_is_dual_output_on() == bEnabled) {
		return true;
	}

	if (bEnabled && !checkStudioMode()) {
		if (!bInvokeCore) {
			pls_set_dual_output_on(false);
		}
		return false;
	}

	if (bInvokeCore) {
		pls_set_dual_output_on(bEnabled);
	}

	sigOpenDualOutput(bEnabled);
	if (api) {
		api->on_event(bEnabled ? pls_frontend_event::PLS_FRONTEND_EVENT_DUAL_OUTPUT_ON : pls_frontend_event::PLS_FRONTEND_EVENT_DUAL_OUTPUT_OFF);
	}

	const char *objName = QMetaEnum::fromType<ConfigId>().valueToKey(ConfigId::DualOutputConfig);
	config_set_bool(App()->GetUserConfig(), objName, "showMode", bEnabled);
	config_save(App()->GetUserConfig());
	getMainView()->updateSideBarButtonStyle(ConfigId::DualOutputConfig, bEnabled);

	getMainView()->setStudioModeDimmed(bEnabled);
	ui->actionStudioMode->setEnabled(!bEnabled);

	if (bInvokeCore) {
		ResetVideo();
	}
	ResetOutputs();

	showDualOutputTitle(bEnabled);

	if (bEnabled) {
		if (ui->preview->isHidden()) {
			ui->preview->show();
		}

		if (ui->previewDisabledWidget->isVisible()) {
			setHorizontalPreviewEnabled(true);
			EnablePreviewDisplay(true);
		}

		showHorizontalDisplay(true);
		showVerticalDisplay(true);

		config_set_bool(Config(), "Stream1", "EnableMultitrackVideo", false);
	} else {
		RemoveVerticalVideo();

		if (ui->preview->isHidden()) {
			ui->preview->show();
		}

		showVerticalDisplay(false);
	}

	ShowAudioMixerAudioTrackTips(bEnabled);

	return true;
}

void PLSBasic::showDualOutputTitle(bool bVisible)
{
	QMargins margins = ui->horizontalLayout_2->contentsMargins();

	if (bVisible) {
		margins.setTop(margins.top() - 10);
	} else {
		margins.setTop(margins.top() + 10);
	}
	ui->horizontalLayout_2->setContentsMargins(margins);

	if (bVisible) {
		dualOutputTitle = new PLSDualOutputTitle(ui->previewContainer);

		ui->verticalLayout_3->insertWidget(0, dualOutputTitle, 0, Qt::AlignTop);
		dualOutputTitle->setVisible(bVisible);
	} else {
		dualOutputTitle->hide();
		dualOutputTitle->deleteLater();
	}

	ui->previewContainer->repaint();
}

void PLSBasic::showVerticalDisplay(bool bVisible)
{
	if (bVisible) {
		CreateVerticalDisplay();

		ui->perviewLayoutHrz->addWidget(verticalDisplay);
	}
	verticalDisplay->setVisible(bVisible);
	verticalPreviewEnabled = bVisible;

	if (!bVisible) {
		ui->previewContainer->repaint();
		pls_async_call(this, [this] { ui->previewContainer->repaint(); });
	}

	if (dualOutputTitle) {
		dualOutputTitle->showVerticalDisplay(bVisible);
	}
}

void PLSBasic::showHorizontalDisplay(bool bVisible)
{
	ui->preview->setVisible(bVisible);

	if (!bVisible) {
		ui->previewContainer->repaint();
		pls_async_call(this, [this] { ui->previewContainer->repaint(); });
	}

	if (dualOutputTitle) {
		dualOutputTitle->showHorizontalDisplay(bVisible);
	}
}

bool OBSBasic::getIsVerticalPreviewFromAction()
{
	auto action = dynamic_cast<QAction *>(sender());
	if (action) {
		return action->property(IS_VERTICAL_PREVIEW).value<bool>();
	}
	auto menu = dynamic_cast<QMenu *>(sender());
	if (menu) {
		return menu->property(IS_VERTICAL_PREVIEW).value<bool>();
	}
	return false;
}

void OBSBasic::ToggleVerticalPreview(bool enable)
{
	PLSBasic::instance()->showVerticalDisplay(enable);
}

void OBSBasic::setHorizontalPreviewEnabled(bool bEnabled)
{
	previewEnabled = bEnabled;
}

bool OBSBasic::getHorizontalPreviewEnabled()
{
	return previewEnabled;
}

bool OBSBasic::getVerticalPreviewEnabled()
{
	return verticalPreviewEnabled;
}

void OBSBasic::setDynamicPropertyForMenuAndAction(QMenu *topMenu, bool isVerticalPreview)
{
	for (auto action : topMenu->actions()) {
		if (action->menu()) {
			action->menu()->setProperty(IS_VERTICAL_PREVIEW, isVerticalPreview);
			setDynamicPropertyForMenuAndAction(action->menu(), isVerticalPreview);
		} else {
			action->setProperty(IS_VERTICAL_PREVIEW, isVerticalPreview);
		}
	}
}

void OBSBasic::addNudgeFunc(OBSBasicPreview *preview)
{
	auto addNudge = [this](const QKeySequence &seq, MoveDir direction, int distance, OBSBasicPreview *preview) {
		if (!preview) {
			return;
		}
		QAction *nudge = new QAction(preview);
		nudge->setShortcut(seq);
		nudge->setShortcutContext(Qt::WidgetShortcut);
		preview->addAction(nudge);
		connect(nudge, &QAction::triggered, [this, distance, direction, preview]() { Nudge(distance, direction, preview); });
	};

	addNudge(Qt::Key_Up, MoveDir::Up, 1, preview);
	addNudge(Qt::Key_Down, MoveDir::Down, 1, preview);
	addNudge(Qt::Key_Left, MoveDir::Left, 1, preview);
	addNudge(Qt::Key_Right, MoveDir::Right, 1, preview);
	addNudge(Qt::SHIFT | Qt::Key_Up, MoveDir::Up, 10, preview);
	addNudge(Qt::SHIFT | Qt::Key_Down, MoveDir::Down, 10, preview);
	addNudge(Qt::SHIFT | Qt::Key_Left, MoveDir::Left, 10, preview);
	addNudge(Qt::SHIFT | Qt::Key_Right, MoveDir::Right, 10, preview);
}

void PLSBasic::changeOutputCount(int iValue)
{
	m_iOutputCount += iValue;

	if (m_iOutputCount > 0) {
		if (!m_bOutputActive) {
			m_bOutputActive = true;
			emit sigOutputActiveChanged(m_bOutputActive);
		}
	} else if (0 == m_iOutputCount) {
		if (m_bOutputActive) {
			m_bOutputActive = false;
			emit sigOutputActiveChanged(m_bOutputActive);
		}
	} else {
		m_iOutputCount = 0;
		assert(false && "iOutputCount is error");
	}
}

void PLSBasic::ResetStatsHotkey() {}

void OBSBasic::selectGroupItem(OBSSceneItem item, bool select)
{
	ui->sources->SelectGroupItem(item, select);
}

static bool reset_group_select(obs_scene_t * /* scene */, obs_sceneitem_t *item, void *param)
{
	if (!obs_sceneitem_is_group(item))
		return true;

	OBSBasic::Get()->selectGroupItem(item, obs_sceneitem_selected(item));
	return true;
}

void OBSBasic::resetAllGroupSelectStatus()
{
	auto callback = [](void *data_ptr, obs_source_t *scene) {
		if (!scene) {
			return true;
		}
		auto source = obs_scene_from_source(scene);
		if (!source) {
			return true;
		}
		pls_scene_enum_items_all(source, reset_group_select, nullptr);
		return true;
	};
	pls_enum_all_scenes(callback, nullptr);
}

void OBSBasic::showsPrismPlusIntroWindow()
{

}

void OBSBasic::showsTipAndPrismPlusIntroWindow(const QString &strContent, const QString &strFeature, QWidget *parent) {

}

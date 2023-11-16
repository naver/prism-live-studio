#include "PLSBasic.h"
#include "pls-common-define.hpp"
#include "pls-common-language.hpp"
#include <util/profiler.hpp>
#include "PLSMotionNetwork.h"
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
#include "PLSGetPropertiesThread.h"
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

#include "PLSResourceManager.h"

#include "window-dock-browser.hpp"
#include "PLSPreviewTitle.h"
#include "pls-frontend-api.h"
#include "GiphyDownloader.h"
#include "PLSInfoCollector.h"
#include "log/log.h"
#include <QStandardPaths>
#include "PLSSyncServerManager.hpp"
#include "util/windows/win-version.h"

//TODO
//#include "PLSBeautyHelper.h"
#if defined(Q_OS_WIN)
#include <Windows.h>
#endif

#define PRISM_STICKER_CENTER_SHOW 1

using namespace common;
extern void printTotalStartTime();
extern PLSLaboratory *g_laboratoryDialog;
extern PLSRemoteChatView *g_remoteChatDialog;
extern void showNetworkAlertOnlyOnce();
struct LocalVars {
	static PLSBasic *s_basic;
};
constexpr auto CONFIG_BASIC_WINDOW_MODULE = "BasicWindow";
constexpr auto CONFIG_PREVIEW_MODE_MODULE = "PreviewProgramMode";
constexpr auto PRISM_CAM_PRODUCTION_NAME = "PRISMLens";

PLSBasic *LocalVars::s_basic = nullptr;
extern volatile long insideEventLoop;
constexpr int RESTARTAPP = 1024;
constexpr int NEED_RESTARTAPP = 1025;
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
		} else if (!((index == ChatPlatformIndex::Twitch) || (index == ChatPlatformIndex::Youtube) || (index == ChatPlatformIndex::Facebook) || (index == ChatPlatformIndex::NaverTV) ||
			     (index == ChatPlatformIndex::AfreecaTV) || (index == ChatPlatformIndex::NaverShopping))) {
			continue;
		} else {
			QString channelName = platform->getChannelName();
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
			if (id && id[0] && !strcmp(id, PRISM_CHAT_SOURCE_ID)) {
				*pfound = true;
				return false;
			}
			return true;
		},
		&found);

	return found;
}

static PlatformType getPlatformType(QString &platformName, PLSPlatformBase *platform)
{
	auto info = platform->getInitData();
	if (auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt(); dataType >= ChannelData::RTMPType) {
		platformName = QStringLiteral("Custom RTMP");
		return PlatformType::CustomRTMP;
	} else if (dataType == ChannelData::ChannelType) {
		if (platformName = info.value(ChannelData::g_platformName, "").toString(); platformName == BAND) {
			return PlatformType::Band;
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
		return getPlatformType(platformName, activedPlatforms.front());
	}

	bool hasBand = false;
	bool hasCustomRTMP = false;
	bool hasOthers = false;
	for (auto platform : activedPlatforms) {
		QString platformName;
		if (auto type = getPlatformType(platformName, platform); type == PlatformType::Band) {
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
	auto _activiedPlatforms = PLS_PLATFORM_ACTIVIED;
	QList<PLSPlatformBase *> activiedPlatforms(_activiedPlatforms.begin(), _activiedPlatforms.end());
	std::sort(activiedPlatforms.begin(), activiedPlatforms.end(),
		  [](auto p1, auto p2) { return p1->getInitData().value(ChannelData::g_displayOrder).toInt() < p2->getInitData().value(ChannelData::g_displayOrder).toInt(); });
	for (auto platform : activiedPlatforms) {
		QString platformName;
		if (getPlatformType(platformName, platform) == PlatformType::Others) {
			platforms.add(pls::JsonDocument<QJsonObject>()                                                            //
					      .add("platform", platformName)                                                      //
					      .add("name", platform->getInitData().value(ChannelData::g_displayLine1).toString()) //
					      .add("viewerCount", platform->getInitData().value(ChannelData::g_viewers).toInt())
					      .object());
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

CheckCamProcessWorker::~CheckCamProcessWorker()
{
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
	ui->actionRemoveSource->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	ui->actionRemoveScene->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	ui->scenesFrame->addAction(ui->actionRemoveScene);
	ui->sources->addAction(ui->actionRemoveSource);
	setWindowIcon(QIcon(":/resource/images/logo/PRISMLiveStudio.ico"));

	connect(ui->sources, &SourceTree::SelectItemChanged, this, &OBSBasic::OnSelectItemChanged, Qt::QueuedConnection);
	connect(ui->sources, &SourceTree::VisibleItemChanged, this, &OBSBasic::OnVisibleItemChanged, Qt::QueuedConnection);
	connect(ui->sources, &SourceTree::itemsRemove, this, &OBSBasic::OnSourceItemsRemove, Qt::QueuedConnection);
	connect(
		this, &PLSBasic::outputStateChanged, mainView,
		[this] {
			if (m_checkUpdateWidget) {
				m_checkUpdateWidget->setItemDisabled(pls_is_output_actived());
				getMainView()->updateTipsEnableChanged();
			}
		},
		Qt::QueuedConnection);
	connect(
		ui->sources, &SourceTree::itemsReorder, this,
		[this]() {
			ReorderBgmSourceList();
			emit itemsReorderd();
		},
		Qt::QueuedConnection);

	UpdateStudioModeUI(IsPreviewProgramMode());

	mainView->statusBar()->OnRecordStatus(false);
	mainView->statusBar()->OnLiveStatus(false);

	ui->scenesFrame->OnLiveStatus(false);
	ui->scenesFrame->OnRecordStatus(false);

	if (previewProgramTitle) {
		previewProgramTitle->OnLiveStatus(false);
		previewProgramTitle->OnRecordStatus(false);
	}

	connect(mainView, &PLSMainView::studioModeChanged, this, &PLSBasic::on_modeSwitch_clicked);
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
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::broadcastGo, this,
		[this]() {
			PLS_INFO(MAINFRAME_MODULE, "stop notice timer");
			m_noticeTimer.stop();
		},
		Qt::QueuedConnection);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::broadCastEnded, this,
		[this]() {
			PLS_INFO(MAINFRAME_MODULE, "restart notice timer");
			if (m_noticeTimer.isActive()) {
				m_noticeTimer.stop();
			}
			m_noticeTimer.start();
		},
		Qt::QueuedConnection);
	m_noticeTimer.setInterval(60 * 60 * 1000);
	m_noticeTimer.start();

	connect(&m_noticeTimer, &QTimer::timeout, [this]() {
		PLS_INIT_INFO(MAINFRAME_MODULE, "start request prism notice info.");
		pls_get_new_notice_Info([this](const QVariantMap &noticeInfo) {
			if (noticeInfo.size() && !pls_is_main_window_closing()) {
				PLSNoticeView view(noticeInfo.value(NOTICE_CONTENE).toString(), noticeInfo.value(NOTICE_TITLE).toString(), noticeInfo.value(NOTICE_DETAIL_LINK).toString(),
						   getMainView());
#if defined(Q_OS_MACOS)
				view.setWindowTitle(tr("Mac.Title.Notice"));
#endif
				PLS_INFO(MAINFRAME_MODULE, "notice view instace create success");
				view.exec();
				PLS_INFO(MAINFRAME_MODULE, "notice view instace delete");
			}
		});
	});
	obs_frontend_add_event_callback(&frontendEventHandler, this);
	pls_frontend_add_event_callback(&frontendEventHandler, this);
	pls_frontend_add_event_callback(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT, &PLSBasic::LogoutCallback, this);

	connect(
		ui->hMixerScrollArea->verticalScrollBar(), &QScrollBar::rangeChanged, this,
		[this]() {
			pls_check_app_exiting();
			bool showing = ui->hMixerScrollArea->verticalScrollBar()->isVisible();
			ui->hVolControlLayout->setContentsMargins(3, 0, showing ? 0 : 9, 0);
		},
		Qt::QueuedConnection);
	connect(
		ui->vMixerScrollArea->horizontalScrollBar(), &QScrollBar::rangeChanged, this,
		[this]() {
			pls_check_app_exiting();
			bool showing = ui->vMixerScrollArea->horizontalScrollBar()->isVisible();
			ui->vVolControlLayout->setContentsMargins(14, 0, 20, showing ? 10 : 20);
		},
		Qt::QueuedConnection);

	connect(GiphyDownloader::instance(), &GiphyDownloader::downloadResult, this, &PLSBasic::OnStickerDownloadCallback);

#if defined(Q_OS_WIN)
	m_checkUpdateWidgetAction = new QWidgetAction(this);
	m_checkUpdateWidget = pls_new<PLSNewIconActionWidget>(ui->actionCheckForUpdates->text());
	m_checkUpdateWidget->setTextMarginLeft(20);
	m_checkUpdateWidget->setProperty("type", "mainMenu");

	connect(m_checkUpdateWidgetAction, SIGNAL(triggered()), this, SLOT(on_checkUpdate_triggered()));
	m_checkUpdateWidgetAction->setDefaultWidget(m_checkUpdateWidget);
	replaceMenuActionWithWidgetAction(ui->menubar, ui->actionCheckForUpdates, m_checkUpdateWidgetAction);

	QWidgetAction *discordWidgetAction = new QWidgetAction(this);
	PLSNewIconActionWidget *discordWidget = pls_new<PLSNewIconActionWidget>(ui->actionPrismDiscord->text());
	discordWidget->setTextMarginLeft(20);
	discordWidget->setBadgeVisible(true);
	discordWidget->setProperty("type", "mainMenu");

	connect(discordWidgetAction, SIGNAL(triggered()), this, SLOT(on_actionDiscord_triggered()));
	discordWidgetAction->setDefaultWidget(discordWidget);
	replaceMenuActionWithWidgetAction(ui->menubar, ui->actionPrismDiscord, discordWidgetAction);

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

	PLSDrawPenMgr::Instance()->Release();
	PLSAudioControl::instance()->ClearObsSignals();
	LocalVars::s_basic = nullptr;
}

void PLSBasic::getLocalUpdateResult() {}

void OBSBasic::UpdateStudioModeUI(bool studioMode)
{
	QMargins margins = ui->horizontalLayout_2->contentsMargins();
	QMargins newMargins = margins;

	if (studioMode) {
		newMargins.setTop(newMargins.left() - 10);
	} else {
		newMargins.setTop(newMargins.left() + 10);
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
	if (GetConfigPath(cpath.data(), sizeof(cpath), config) <= 0) {
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

	for (int i = 0; i < extraDocks.size(); i++) {
		auto dock = static_cast<PLSDock *>(extraDocks[i].get());
		if (!dock || !dock->isFloating()) {
			continue;
		}
		setDockDisplayAsynchronously(dock, visible);
	}
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
		QTimer::singleShot(0, this, [dock]() { dock->setVisible(true); });
		return;
	}
	dock->setVisible(false);
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
	if (pls_prism_get_locale() != "en-US") {
		ui->actionFullscreenInterface->setShortcutContext(Qt::ApplicationShortcut);
		ui->actionFullscreenInterface->setShortcut(QKeySequence("Ctrl+F"));
		ui->actionFullscreenInterface->setVisible(true);
		ui->viewMenu->addAction(ui->actionFullscreenInterface);
	} else {
		ui->actionFullscreenInterface->setVisible(false);
	}
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

	PLSMotionNetwork::instance()->downloadResource(pls_get_app_data_dir(QStringLiteral("PRISMLiveStudio/resources")) + "/category.json");
	QString udpateUrl(config_get_string(App()->GlobalConfig(), "AppUpdate", "updateUrl"));
	QString updateGcc(config_get_string(App()->GlobalConfig(), "AppUpdate", "updateGcc"));
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

	bool vertical = config_get_bool(GetGlobalConfig(), "BasicWindow", "VerticalVolControl");
	actionMixerLayout = pls_new<QAction>(vertical ? QTStr("Basic.MainMenu.Mixer.Horizontal") : QTStr("Basic.MainMenu.Mixer.Vertical"), ui->hMixerScrollArea);
	connect(actionMixerLayout, &QAction::triggered, this, &PLSBasic::OnMixerLayoutTriggerd);

	ui->mixerDock->titleWidget()->setAdvButtonActions({actionSperateMixer, actionMixerLayout});
	ui->mixerDock->titleWidget()->setButtonActions(listActions);

	bool docksLocked = config_get_bool(App()->GlobalConfig(), "BasicWindow", "DocksLocked");
	setDockDetachEnabled(docksLocked);

	setUserIcon();

	ui->actionHelpPortal->disconnect();
	ui->actionWebsite->disconnect();
	ui->actionShowAbout->disconnect();
	ui->actionRepair->disconnect();
	ui->actionHelpOpenSource->disconnect();

	connect(ui->actionHelpPortal, &QAction::triggered, this, &PLSBasic::on_actionHelpPortal_triggered);
	connect(ui->actionHelpOpenSource, &QAction::triggered, this, &PLSBasic::on_actionHelpOpenSource_triggered);
	connect(ui->actionPrismDiscord, &QAction::triggered, this, &PLSBasic::on_actionDiscord_triggered);
	connect(ui->actionWebsite, &QAction::triggered, this, &PLSBasic::on_actionWebsite_triggered);
	connect(ui->actionShowAbout, &QAction::triggered, this, &PLSBasic::on_actionShowAbout_triggered);
	connect(ui->actionContactUs, &QAction::triggered, this, [this]() { on_actionContactUs_triggered(); });
	connect(ui->actionBlog, &QAction::triggered, this, &PLSBasic::on_actionBlog_triggered);
	connect(ui->actionRepair, &QAction::triggered, this, &PLSBasic::on_actionRepair_triggered);
	connect(ui->actionStudioMode, &QAction::triggered, this, &PLSBasic::on_actionStudioMode_triggered);
	connect(ui->stats, &QAction::triggered, this, &PLSBasic::on_stats_triggered);

	auto text = ui->actionRepair->text();
	text.replace("PRISM", "OBS");
	ui->actionRepair->setText(text);

#if defined(Q_OS_MACOS)
	ui->menuBasic_MainMenu_Help->addAction(ui->actionCheckForUpdates);
	ui->actionCheckForUpdates->setVisible(true);
	ui->actionCheckForUpdates->setEnabled(true);
	connect(ui->actionCheckForUpdates, SIGNAL(triggered()), this, SLOT(on_checkUpdate_triggered()));
#endif

	CreateToolArea();
	if (drawPenView)
		drawPenView->UpdateView(GetCurrentScene());

	InitInteractData();
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
		connect(virtualCam, &QAction::triggered, this, &PLSBasic::VCamButtonClicked);
		auto updateActionState = [this]() {
			virtualCam->setChecked(VirtualCamActive());
			virtualCam->setText(VirtualCamActive() ? QTStr("Basic.Main.StopVirtualCam") : QTStr("Basic.Main.StartVirtualCam"));
		};
		connect(this, &PLSBasic::outputStateChanged, this, updateActionState, Qt::QueuedConnection);
	}
	if (virtualCamSetting == nullptr) {
		virtualCamSetting = new QAction(QTStr("Basic.Main.VirtualCamConfig"), this);
		connect(virtualCamSetting, &QAction::triggered, this, &OBSBasic::VCamConfigButtonClicked);
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

	if (logOut == nullptr) {
		logOut = new QAction(QTStr("Basic.MainMenu.File.Logout"), this);
		this->addAction(logOut);
		connect(logOut, &QAction::triggered, this, [this]() {
			if (PLSAlertView::question(this, tr("Confirm"), tr("main.message.logout_alert"), PLSAlertView::Button::Yes | PLSAlertView::Button::No) == PLSAlertView::Button::Yes) {
				PLSApp::plsApp()->backupSceneCollectionConfig();
				pls_prism_logout();
			}
		});
		connect(this, &PLSBasic::outputStateChanged, logOut, [this]() { logOut->setEnabled(!pls_is_output_actived()); });
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
	if (!m_mainMenuShow) {
		m_mainMenuShow = true;

		QPushButton *menuButton = dynamic_cast<QPushButton *>(sender());
		auto pos = menuButton->mapToGlobal(menuButton->rect().bottomLeft() + QPoint(0, 5));
		QMenu mainMenu(this);
		mainMenu.setObjectName("mainMenu");
		mainMenu.setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);
		mainMenu.addActions(adjustedMenu());
		mainMenu.exec(pos);
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
	UpdateContextBar();
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
	AddStickerSource(PRISM_GIPHY_STICKER_SOURCE_ID, fileName, giphyData);
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

void PLSBasic::AddStickerSource(const char *id, const QString &file, const GiphyData &giphyData)
{
	OBSScene scene = GetCurrentScene();
	if (!scene)
		return;

	if (file.isEmpty())
		return;

	obs_source_t *source = nullptr;
	source = CreateSource(id);

	if (source) {
		action::SendActionToNelo(id, action::ACTION_ADD_EVENT, id);
		action::SendActionLog(action::ActionInfo(action::EVENT_MAIN_ADD, action::EVENT_SUB_SOURCE_ADDED, action::EVENT_TYPE_CONFIRMED, action::GetActionSourceID(id)));
		pls_send_analog(AnalogType::ANALOG_GIPHY_STICKER, {{ANALOG_GIPHY_ID_KEY, giphyData.id}});

		OnSourceCreated(id);
		action::SendPropToNelo(id, "stickerId", qUtf8Printable(giphyData.originalUrl));

		SetStickerSourceSetting(source, file, giphyData);

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
		source_show_default(source, scene);
#endif

		/* set monitoring if source monitors by default */
		auto flags = obs_source_get_output_flags(source);
		if ((flags & OBS_SOURCE_MONITOR_BY_DEFAULT) != 0) {
			obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_ONLY);
		}
		obs_source_release(source);
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
		ui->actionLockPreview->setChecked(drawPenMode);
	} else {
		bool locked = ui->preview->CacheLocked();
		ui->preview->SetLocked(locked);
		ui->actionLockPreview->setChecked(locked);
	}

	PLS_UI_STEP(DRAWPEN_MODULE, QString("Drawpen mode switched to ").append(drawPenMode ? "on" : "off").toUtf8().constData(), ACTION_CLICK);
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
	auto startCallback = [](void *inUserData, bool isSucceed) {
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
		QString linkUrl = "https://prismlive.com/en_us/pcapp/";
		if (0 == lang.compare("ko-KR", Qt::CaseInsensitive)) {
			linkUrl = "https://prismlive.com/ko_kr/pcapp/";
		} else if (0 == lang.compare("id-ID", Qt::CaseInsensitive)) {
			linkUrl = "https://prismlive.com/id_id/pcapp/";
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

void PLSBasic::OnPRISMStickerApplied(const StickerHandleResult &stickerData)
{
	QString log("User apply prism sticker: '%1/%2'");
	PLS_UI_STEP(MAIN_PRISM_STICKER, qUtf8Printable(log.arg(stickerData.data.category).arg(stickerData.data.id)), ACTION_CLICK);
	AddPRISMStickerSource(stickerData);
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

static void DownloadSticker(const GiphyData &data, uint64_t randomId) {}

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
	bool showSticker = pls_config_get_bool(App()->GlobalConfig(), ConfigId::GiphyStickersConfig, CONFIG_SHOW_MODE);
	if (showSticker) {
		CreateGiphyStickerView();
		giphyStickerView->show();
	}
}

void PLSBasic::InitPrismStickerViewVisible()
{
	bool showSticker = pls_config_get_bool(App()->GlobalConfig(), ConfigId::PrismStickerConfig, CONFIG_SHOW_MODE);
	if (showSticker) {
		CreatePrismStickerView();
		prismStickerView->show();
		prismStickerView->raise();
	}
}

void PLSBasic::on_actionShowAbout_triggered()
{
	PLSAboutView aboutView(this);
	if (aboutView.exec() == PLSAboutView::Accepted) {
		QMetaObject::invokeMethod(
			this, [this]() { on_checkUpdate_triggered(); }, Qt::QueuedConnection);
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
	if (IS_KR()) {
		QDesktopServices::openUrl(QUrl(QString("https://discord.gg/9j7mFY5g9a"), QUrl::TolerantMode));
	} else {
		QDesktopServices::openUrl(QUrl(QString("https://discord.gg/vxzDZ9V6f9"), QUrl::TolerantMode));
	}
}

void PLSBasic::on_actionWebsite_triggered() const
{
	QDesktopServices::openUrl(QUrl(QString("http://prismlive.com/%1/pcapp/").arg(getSupportLanguage()), QUrl::TolerantMode));
}

void PLSBasic::on_actionContactUs_triggered(const QString &message, const QString &additionalMessage)
{
	PLSContactView view(message, additionalMessage, this);
	view.exec();
}

void PLSBasic::on_actionRepair_triggered() const
{
	QUrl url = QUrl("https://obsproject.com/help", QUrl::TolerantMode);
	QDesktopServices::openUrl(url);
}

void PLSBasic::on_actionBlog_triggered() const
{
	if (!strcmp(App()->GetLocale(), "ko-KR")) {
		QDesktopServices::openUrl(QUrl("https://blog.naver.com/prismlivestudio", QUrl::TolerantMode));
	} else {
		QDesktopServices::openUrl(QUrl("https://medium.com/prismlivestudio", QUrl::TolerantMode));
	}
}

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
	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	const char *type = config_get_string(basicConfig, "AdvOut", "RecType");
	const char *adv_path = strcmp(type, "Standard") ? config_get_string(basicConfig, "AdvOut", "FFFilePath") : config_get_string(basicConfig, "AdvOut", "RecFilePath");
	const char *path = strcmp(mode, "Advanced") ? config_get_string(basicConfig, "SimpleOutput", "FilePath") : adv_path;
	QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void PLSBasic::on_checkUpdate_triggered() {}

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
		VCamButtonClicked();
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
	default:
		break;
	}
}

void PLSBasic::OnTransitionAdded()
{
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED);
}

void PLSBasic::onTransitionRemoved(OBSSource source)
{
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED);
	this->DeletePropertiesWindow(source);
}

void PLSBasic::OnTransitionRenamed()
{
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED);
}

void PLSBasic::OnTransitionSet()
{
	if (api)
		api->on_event(OBS_FRONTEND_EVENT_TRANSITION_CHANGED);
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
	const char *name = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollection");
	const char *fileName = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");

	on_actionExportSceneCollection_triggered_with_path(name, fileName, this);
}

void PLSBasic::StartSaveSceneCollectionTimer()
{
	saveSceneCollectionTimer.setInterval(5000);
	connect(&saveSceneCollectionTimer, &QTimer::timeout, this, &PLSBasic::OnSaveSceneCollectionTimerTriggered);
	saveSceneCollectionTimer.start();
}

void PLSBasic::ShowLoadSceneCollectionError()
{
	if (showLoadSceneCollectionError) {
		PLSAlertView::warning(this, QTStr("Alert.title"), showLoadSceneCollectionErrorStr);
		showLoadSceneCollectionError = false;
		showLoadSceneCollectionErrorStr = "";
	}
}

void PLSBasic::SetSceneDisplayMethod(int method) const
{
	ui->scenesFrame->SetSceneDisplayMethod(method);
}

int PLSBasic::GetSceneDisplayMethod() const
{
	if (!config_has_user_value(App()->GlobalConfig(), "BasicWindow", "SceneDisplayMethod")) {
		config_set_int(App()->GlobalConfig(), "BasicWindow", "SceneDisplayMethod", 0);
		return 0;
	}
	int displayMethod = (int)config_get_int(App()->GlobalConfig(), "BasicWindow", "SceneDisplayMethod");
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
		actionThumbnail->setChecked(1 == displayMethod);
		actionText->setChecked(2 == displayMethod);
	});

	actionRealTime = pls_new<QAction>(QTStr("Setting.Scene.Display.Realtime.View"), ui->scenesFrame);
	actionRealTime->setCheckable(true);

	actionThumbnail = pls_new<QAction>(QTStr("Setting.Scene.Display.Thumbnail.View"), ui->scenesFrame);
	actionThumbnail->setCheckable(true);

	actionText = pls_new<QAction>(QTStr("Setting.Scene.Display.Text.View"), ui->scenesFrame);
	actionText->setCheckable(true);

	connect(actionRealTime, &QAction::triggered, this, [this]() {
		config_set_int(GetGlobalConfig(), "BasicWindow", "SceneDisplayMethod", 0);
		SetSceneDisplayMethod(0);
	});
	connect(actionThumbnail, &QAction::triggered, this, [this]() {
		config_set_int(GetGlobalConfig(), "BasicWindow", "SceneDisplayMethod", 1);
		SetSceneDisplayMethod(1);
	});
	connect(actionText, &QAction::triggered, this, [this]() {
		config_set_int(GetGlobalConfig(), "BasicWindow", "SceneDisplayMethod", 2);
		SetSceneDisplayMethod(2);
	});

	displayMethodMenu->addAction(actionRealTime);
	displayMethodMenu->addAction(actionThumbnail);
	displayMethodMenu->addAction(actionText);
	return displayMethodMenu;
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
	if (live && config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming")) {
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
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		if (QStringList activedChatChannels = getActivedChatChannels(); !activedChatChannels.isEmpty()) { // contains chat channel
			if (!hasChatSource()) {                                                                   // no chat source added
				pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, tr("Chat.Toast.NoChatSource.WhenStreaming").arg(activedChatChannels.join(',')));
			}
		} else {                       // not contains chat channel
			if (hasChatSource()) { // chat source added
				pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, tr("Chat.Toast.ChatSource.NoSupportChannel.WhenStreaming"));
			}
		}

		if (pls_frontend_get_dispatch_js_event_cb()) {
			// notify chat source
			QByteArray settingJson = pls::JsonDocument<QJsonObject>()
							 .add("type", "setting")
							 .add("data", pls::JsonDocument<QJsonObject>()
									      .add("singlePlatform", getActivedChatChannelCount() == 1)
									      .add("videoSeq", pls_get_prism_live_seq())
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

		break;
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
		main->showEncodingInStatusBar();
		main->ui->scenesFrame->RefreshSceneThumbnail();
		break;
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		main->CheckStickerSource();
		updatePlatformViewerCount();
		std::thread([]() { PLSInfoCollector::logMsg("Scene Collection Changed"); }).detach();
		config_save_safe(App()->GlobalConfig(), "tmp", nullptr);
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
	default:
		break;
	}
}
void PLSBasic::rehearsalSwitchedToLive() const
{
	if (previewProgramTitle) {
		previewProgramTitle->OnLiveStatus(true);
	}

	mainView->statusBar()->OnLiveStatus(true);

	PLS_PLATFORM_API->doStartGpopMaxTimeLiveTimer();
}

void PLSBasic::showMainViewAfter(QWidget *parentWidget)
{
	pls_check_app_exiting();
	bool updateNow = false;

	// if update view never shown
	PLS_INFO(UPDATE_MODULE, "enter showMainViewAfter");

	if (!updateNow) {
		//notice view
		PLS_INFO(MAINFRAME_MODULE, "start request prism notice info.");
		pls_get_new_notice_Info([this](const QVariantMap &noticeInfo) {
			if (noticeInfo.size()) {
				PLSNoticeView view(noticeInfo.value(NOTICE_CONTENE).toString(), noticeInfo.value(NOTICE_TITLE).toString(), noticeInfo.value(NOTICE_DETAIL_LINK).toString(), mainView);
#if defined(Q_OS_MACOS)
				view.setWindowTitle(tr("Mac.Title.Notice"));
#endif
				view.exec();
			}
		});
	}
}

void PLSBasic::LogoutCallback(pls_frontend_event, const QVariantList &l, void *v)
{
	pls_unused(l, v);
	bool defaultPreviewMode = true;
	config_set_bool(App()->GlobalConfig(), CONFIG_BASIC_WINDOW_MODULE, CONFIG_PREVIEW_MODE_MODULE, defaultPreviewMode);
	dynamic_cast<PLSBasic *>(App()->GetMainWindow())->SetPreviewProgramMode(defaultPreviewMode);
}

bool enum_adapter_callback(void *, const char *name, uint32_t id)
{

	if (name && config_get_uint(App()->GlobalConfig(), "Video", "AdapterIdx") == id) {
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
	bool vertical = config_get_bool(GetGlobalConfig(), "BasicWindow", "VerticalVolControl");
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

void PLSBasic::CreateAdvancedButtonForBrowserDock(OBSDock *dock, const QString &uuid)
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

	for (int i = 0; i < extraBrowserDocks.count(); i++) {
		OBSDock *dock = static_cast<OBSDock *>(extraBrowserDocks[i].data());
		if (!dock) {
			continue;
		}
		dock->titleWidget()->setAdvButtonActionsEnabledByObjName("detachBtn", !dockLocked);
	}
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

	QString title = action->property("uuid").toString();
	for (int i = 0; i < extraBrowserDocks.size(); i++) {
		BrowserDock *dock = reinterpret_cast<BrowserDock *>(extraBrowserDocks[i].data());
		if (!dock) {
			continue;
		}
		if (0 == dock->property("uuid").toString().compare(title)) {
			changeDockState(dock, action);
			dock->titleWidget()->updateTitle();
			return;
		}
	}
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
	obs_sceneitem_set_info(sceneitem, &itemInfo);
}

void PLSBasic::AddPRISMStickerSource(const StickerHandleResult &data)
{
	OBSScene scene = GetCurrentScene();
	if (!scene)
		return;
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "loop", true);
	obs_source_t *source = CreateSource(PRISM_STICKER_SOURCE_ID, settings);

	OBSDataAutoRelease priv_settings = obs_source_get_private_settings(source);
	obs_data_set_string(priv_settings, "landscapeVideo", qUtf8Printable(data.landscapeVideoFile));
	obs_data_set_string(priv_settings, "landscapeImage", qUtf8Printable(data.landscapeImage));
	obs_data_set_string(priv_settings, "portraitVideo", qUtf8Printable(data.portraitVideo));
	obs_data_set_string(priv_settings, "portraitImage", qUtf8Printable(data.portraitImage));
	obs_data_set_string(priv_settings, "resourceId", qUtf8Printable(data.data.id));
	obs_data_set_string(priv_settings, "resourceUrl", qUtf8Printable(data.data.resourceUrl));
	obs_data_set_string(priv_settings, "category", qUtf8Printable(data.data.category));
	obs_data_set_int(priv_settings, "version", data.data.version);
#if PRISM_STICKER_CENTER_SHOW
	pls_source_set_private_data(source, settings);
#else
	obs_source_update(source, settings);
#endif
	obs_data_release(settings);
	if (source) {
		const char *id = PRISM_STICKER_SOURCE_ID;

		action::SendActionToNelo(id, action::ACTION_ADD_EVENT, id);
		action::SendActionLog(action::ActionInfo(action::EVENT_MAIN_ADD, action::EVENT_SUB_SOURCE_ADDED, action::EVENT_TYPE_CONFIRMED, action::GetActionSourceID(id)));
		pls_send_analog(AnalogType::ANALOG_TOUCH_STICKER, {{ANALOG_TOUCH_STICKER_CATEGORY_ID_KEY, data.data.category}, {ANALOG_TOUCH_STICKER_ID_KEY, data.data.id}});

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
		addSource.source = source;
		addSource.visible = true;

		obs_enter_graphics();
		obs_scene_atomic_update(scene, AddStickerSourceFunc, &addSource);
		obs_leave_graphics();
#else
		source_show_default(source, scene);
#endif

		/* set monitoring if source monitors by default */
		uint32_t flags = obs_source_get_output_flags(source);
		if ((flags & OBS_SOURCE_MONITOR_BY_DEFAULT) != 0) {
			obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_ONLY);
		}
		obs_source_release(source);

		action::SendActionLog(action::ActionInfo(action::EVENT_MAIN_EDIT, action::ACT_SRC_STICKER, action::EVENT_TYPE_CONFIRMED, data.data.id));
	}
}

obs_source_t *PLSBasic::CreateSource(const char *id, obs_data_t *settings)
{
	QString sourceName{QT_UTF8(obs_source_get_display_name(id))};
	QString newName{sourceName};
	int i = 2;
	obs_source_t *source_ = nullptr;
	while ((source_ = obs_get_source_by_name(QT_TO_UTF8(newName)))) {
		obs_source_release(source_);
		newName = QString("%1 %2").arg(sourceName).arg(i);
		i++;
	}

	obs_source_t *source = obs_get_source_by_name(QT_TO_UTF8(newName));
	if (source) {
		OBSMessageBox::information(this, QTStr("Alert.Title"), QTStr("NameExists.Text"));
		obs_source_release(source);
		return nullptr;
	}

	source = obs_source_create(id, QT_TO_UTF8(newName), settings, nullptr);
	return source;
}

static void updateChatExternParams(const OBSSource &source, int code)
{
	QJsonObject setting;
	setting.insert("type", "setting");

	QJsonObject data;
	data.insert("singlePlatform", obs_frontend_streaming_active() ? (pls_get_actived_chat_channel_count() == 1) : false);
	if (int videoSeq = pls_get_prism_live_seq(); videoSeq > 0) {
		data.insert("videoSeq", QString::number(videoSeq));
	}
	if (!pls_is_create_souce_in_loading() && !obs_frontend_streaming_active()) {
		data.insert("preview", "1");
	} else {
		data.insert("preview", "0");
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

	if (msg == OBS_SOURCE_MUSIC_STATE_CHANGED && backgroundMusicView) {
		obs_media_state state = static_cast<obs_media_state>(code);
		backgroundMusicView->OnMediaStateChanged(name, state);
	}
	if (msg == OBS_SOURCE_MUSIC_LOOP_STATE_CHANGED && backgroundMusicView) {
		backgroundMusicView->OnLoopStateChanged(name);
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
			QMetaObject::invokeMethod(
				this, [name]() { PLSVirtualBgManager::checkResourceInvalid({}, name); }, Qt::QueuedConnection);
		}
	} else if (msg == OBS_SOURCE_SENSEAR_ACTION) {
		PLS_INFO(SOURCE_MODULE, "Send sensear action log");
		//todo
		/*action::SendActionLog(
			action::ActionInfo(EVENT_SENSE_AR_INIT, EVENT_SENSE_AR_NAME, EVENT_SENSE_AR_VERSION, QString("%1").arg(hash<string>()(GetHostMacAddress().toStdString()), 16, 16, QChar('0'))));
		PLSApp::uploadAnalogInfo(SENSEAR_API_PATH, {{SENSEAR_VER_KEY, EVENT_SENSE_AR_VERSION}});*/
	} else if (msg == OBS_SOURCE_EXCEPTION_VIDEO_DEVICE) {
		//todo
		//if (!isFirstShow) {
		//	FilterAlertMsg(name, getCameraTips(pluginID));
		//} else {
		//	struct SourceNotifyMsg sourceMsg;
		//	sourceMsg.title = name;
		//	sourceMsg.msg = getCameraTips(pluginID);
		//	SourceNotifyCacheList.push_back(sourceMsg);
		//}
	}
}

void PLSBasic::OnMainViewShow(bool isShow)
{
	//todo
	/*if (isShow && !mainView->property(IS_ALWAYS_ON_TOP_FIRST_SETTED).toBool()) {
		mainView->setProperty(IS_ALWAYS_ON_TOP_FIRST_SETTED, true);

		bool alwaysOnTop = config_get_bool(App()->GlobalConfig(), "BasicWindow", "AlwaysOnTop");
		if (alwaysOnTop || GlobalVars::opt_always_on_top) {
			SetAlwaysOnTop(mainView, MAIN_FRAME, true);
			ui->actionAlwaysOnTop->setChecked(true);
		}
	}

	if (properties) {
		properties->setVisible(isShow);
	}

	if (isShow && isFirstShow) {
		isFirstShow = false;
		for (const auto &iter : SourceNotifyCacheList) {
			FilterAlertMsg(iter.title, iter.msg);
		}
		SourceNotifyCacheList.clear();
		*/

	if (isShow && isFirstShow) {
		PLS_INFO(UPDATE_MODULE, "enter OnMainViewShow(%d)", isShow);
		getLocalUpdateResult();
		isFirstShow = false;
		PLS_INFO(UPDATE_MODULE, "enter ShowLoadSceneCollectionError");
		ShowLoadSceneCollectionError();
		PLS_INFO(UPDATE_MODULE, "enter StartSaveSceneCollectionTimer");
		StartSaveSceneCollectionTimer();
		PLS_INFO(UPDATE_MODULE, "enter QApplication::postEvent(ui->scenesDock->titleWidget(), new QEvent(QEvent::Resize))");
		QApplication::postEvent(ui->scenesDock->titleWidget(), new QEvent(QEvent::Resize));
		PLS_PRSIM_SHARE_MEMORY;
		if (auto banner = pls_get_banner_widget()) {
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
				Qt::QueuedConnection);
		} else {
			PLS_INFO(UPDATE_MODULE, "pls_get_banner_widget object is empty");
			showMainViewAfter(getMainView());
		}
	}
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

	QMetaObject::invokeMethod(window, "SelectSceneItem", Q_ARG(OBSScene, scene), Q_ARG(OBSSceneItem, item), Q_ARG(bool, true));
}

void OBSBasic::SceneItemDeselected(void *data, calldata_t *params)
{
	pls_unused(params);

	auto window = static_cast<OBSBasic *>(data);

	auto scene = (obs_scene_t *)calldata_ptr(params, "scene");
	auto item = (obs_sceneitem_t *)calldata_ptr(params, "item");

	QMetaObject::invokeMethod(window, "SelectSceneItem", Q_ARG(OBSScene, scene), Q_ARG(OBSSceneItem, item), Q_ARG(bool, false));
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
	//todo
	const obs_source_t *itemSource = obs_sceneitem_get_source(item);
	const char *id = obs_source_get_id(itemSource);
	//emit sceneItemVisibleChanged((qint64)item.get(), (qint64)itemSource, id, visible);
	//SceneItemSetVisible(item, visible);
	PLSBasic::instance()->SetBgmItemVisible(item, visible);
	//SceneItemVirtualBgSetVisible(item, visible);
	//uint32_t flags = obs_source_get_output_flags(obs_sceneitem_get_source(item));
	//if (flags & OBS_SOURCE_AUDIO) {
	//	if (!visible) {
	//		auto muteStatus = GetMasterAudioStatusCurrent();
	//		if (muteStatus.audioSourceCount > 0) {
	//			bool allMute = muteStatus.isAllAudioMuted;
	//			UpdateMasterSwitch(allMute);
	//		}
	//	} else {
	//		bool allMute = ConfigAllMuted();
	//		obs_source_t *source = obs_sceneitem_get_source(item);
	//		if (allMute) {
	//			obs_source_set_muted(source, allMute);
	//			SetSourcePreviousMuted(source, allMute);
	//		}
	//		UpdateMixer(source);
	//	}
	//}
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
				api->on_event(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED);
		}
	}
}

void PLSBasic::OnSaveSceneCollectionTimerTriggered()
{
	SaveProjectNow();

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

void PLSBasic::CheckAppUpdate() {}

void PLSBasic::CheckAppUpdateFinished() {}

int PLSBasic::compareVersion(const QString &v1, const QString &v2) const
{
	QStringList v1List = v1.split(".");
	QStringList v2List = v2.split(".");
	auto len1 = v1List.count();
	auto len2 = v2List.count();
	for (int i = 0; i < qMin(len1, len2); i++) {
		if (v1List.at(i).toUInt() > v2List.at(i).toUInt()) {
			return 1;
		} else if (v1List.at(i).toUInt() < v2List.at(i).toUInt()) {
			return -1;
		}
	}
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

	QDir dir(pls_get_user_path(GIPHY_STICKERS_CACHE_PATH));
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
	auto cx = (int)config_get_int(App()->GlobalConfig(), "InteractionWindow", "cx");
	auto cy = (int)config_get_int(App()->GlobalConfig(), "InteractionWindow", "cy");

	obs_data_t *browserData = obs_data_create();
	obs_data_set_int(browserData, "interaction_cx", cx);
	obs_data_set_int(browserData, "interaction_cy", cy);
	obs_data_set_int(browserData, "prism_hwnd", mainView->window()->winId());
	obs_data_set_string(browserData, "font", this->font().family().toStdString().c_str());
	pls_plugin_set_private_data(BROWSER_SOURCE_ID, browserData);
	obs_data_release(browserData);
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

void PLSBasic::restartApp(const RestartAppType &restartType)
{
	QString filePath = QApplication::applicationFilePath();
	QString param;

	switch (restartType) {
	case RestartAppType::Direct:
		break;
	case RestartAppType::Logout:
		param = QString::asprintf("%s%d", shared_values::k_launcher_command_type.toUtf8().constData(), RestartAppType::Logout);
		break;
	case RestartAppType::ChangeLang:
		break;
	case RestartAppType::Update:
		break;
	default:
		break;
	}
	QObject::connect(qApp, &QApplication::destroyed, [filePath, param]() {
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
        PLS_INFO("RestartApp","restart app param = %s", param.toUtf8().constData());
        pls_restart_mac_app(param.toUtf8().constData());
#endif
	});
}

void PLSBasic::startDownloading()
{
	return;
}

void PLSBasic::OpenRegionCapture()
{
	if (regionCapture || properties)
		return;

	regionCapture = pls_new<PLSRegionCapture>(this);
	connect(regionCapture, &PLSRegionCapture::selectedRegion, this, [this](const QRect &selectedRect) {
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
	bool confirmOnExit = config_get_bool(GetGlobalConfig(), "General", "ConfirmOnExit");

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
	config_set_string(App()->GlobalConfig(), "BasicWindow", "DockState", saveState().toBase64().constData());

	HideAllInteraction(nullptr);

	EnumDialogs();
	SetShowing(false);

	pls_notify_close_modal_views();

	if (properties && properties->isVisible()) {
		properties->close();
		pls_delete(properties); // delete immediately
	}

	PLSGetPropertiesThread::Instance()->WaitForFinished();
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
	bool beautyVisible = pls_config_get_bool(App()->GlobalConfig(), ConfigId::BeautyConfig, CONFIG_SHOW_MODE);
	//OnSetBeautyVisible(beautyVisible);

	//giphy
	InitGiphyStickerViewVisible();

	//Prism Sticker
	InitPrismStickerViewVisible();

	//background music
	bool bgmVisible = pls_config_get_bool(App()->GlobalConfig(), ConfigId::BgmConfig, CONFIG_SHOW_MODE);
	OnSetBgmViewVisible(bgmVisible);

	//toast view
	bool toastVisible = pls_config_get_bool(App()->GlobalConfig(), ConfigId::LivingMsgView, CONFIG_SHOW_MODE);
	mainView->initToastMsgView(toastVisible);

	//chat view
	mainView->showChatView(false, false, true);

	//virtual background
	//bool virtualVisible = pls_config_get_bool(App()->GlobalConfig(), ConfigId::VirtualbackgroundConfig, CONFIG_SHOW_MODE);
	//if (virtualVisible)
	//OnVirtualBgClicked(true);

	// cam studio
	InitCamStudioSidebarState();
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
		UpdateContextBar(true);
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

	PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"addSource", id}}, "User add source: %s", id);
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
	settings.switchTo(tab, group);
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

void PLSBasic::PrismLogout() const
{
	// do not clean scene collection info
	PLSApp::plsApp()->backupSceneCollectionConfig();

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
		previewProgramTitle = pls_new<PLSPreviewProgramTitle>(studioPortraitLayout, ui->previewContainer);
		connect(previewProgramTitle->horApplyBtn, &QPushButton::clicked, this, &PLSBasic::TransitionClicked);
		connect(previewProgramTitle->porApplyBtn, &QPushButton::clicked, this, &PLSBasic::TransitionClicked);
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

void PLSBasic::onPopupSettingView(const QString &tab, const QString &group)
{
	int result = showSettingView(tab, group);
	if (result == static_cast<int>(LoginInfoType::PrismLogoutInfo)) {
		m_isLogout = true;
		PrismLogout();
	} else if (result == static_cast<int>(LoginInfoType::PrismSignoutInfo)) {
		m_isLogout = true;
		PLSApp::plsApp()->backupSceneCollectionConfig();
		pls_prism_signout();
	} else if (result == Qt::UserRole + RESTARTAPP) {
		m_isUpdateLanguage = true;
		restartPrismApp();
	} else if (result == Qt::UserRole + NEED_RESTARTAPP) {
		PLSAlertView::Button button = PLSMessageBox::question(this, QTStr("Confirm"), QTStr("Basic.Settings.NeedRestart"), PLSAlertView::Button::Yes | PLSAlertView::Button::No);
		if (button == PLSAlertView::Button::Yes) {
			restartPrismApp();
		}
	}
}

const QSize STATS_POPUP_FIXED_SIZE{1016, 184};

void PLSBasic::moveStatusPanel()
{
	if (m_dialogStatusPanel) {
		int leftBottomMargin = 5;
		m_dialogStatusPanel->move(leftBottomMargin, getMainView()->height() - getMainView()->getBottomAreaHeight() - leftBottomMargin - STATS_POPUP_FIXED_SIZE.height());
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
	mainView->statusBar()->setEncoding((int)config_get_int(basicConfig, "Video", "OutputCX"), (int)config_get_int(basicConfig, "Video", "OutputCY"));

	uint32_t fpsNum = 0;
	uint32_t fpsDen = 0;
	GetConfigFPS(fpsNum, fpsDen);
	mainView->statusBar()->setFps(QString("%1fps").arg(fpsNum / fpsDen));
}

bool PLSBasic::toggleStatusPanel(int iSwitch)
{
	if (!m_dialogStatusPanel) {
		m_dialogStatusPanel = pls_new<PLSBasicStatusPanel>(getMainView());
		m_dialogStatusPanel->setFixedSize(STATS_POPUP_FIXED_SIZE);
	}

	switch (iSwitch) {
	case -1:
		if (m_dialogStatusPanel->isVisible()) {
			m_dialogStatusPanel->hide();
		} else {
			m_dialogStatusPanel->show();
		}
		break;

	case 0:
		m_dialogStatusPanel->hide();
		break;

	case 1:
		m_dialogStatusPanel->show();
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

void PLSBasic::showEvent(QShowEvent *event)
{
	showEncodingInStatusBar();

	OBSBasic::showEvent(event);
}

void PLSBasic::resizeEvent(QResizeEvent *event)
{
	if (m_dialogStatusPanel && m_dialogStatusPanel->isVisible()) {
		moveStatusPanel();
	}

	if (previewProgramTitle) {
		previewProgramTitle->CustomResize();
	}

	OBSBasic::resizeEvent(event);
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

	if (type == PLS_SOURCE_GAME_CAPTURE_FAILED) {
		auto exe = obs_data_get_string(msg_data, "game_exe");
		auto key = exe ? std::string(exe) : "Unknown";
		bool find = main->m_hook_failed_game_list.find(key) != main->m_hook_failed_game_list.end();
		if (!find) {
			auto reason = obs_data_get_string(msg_data, "game_event_type");
			PLS_LOGEX(PLS_LOG_WARN, MAINFRAME_MODULE, {{"game_exe", exe}, {"game_event_type", reason}},
				  "[game-capture : %p] Event of game capture. \n"
				  "    exe: %s\n"
				  "    event: %s",
				  source, exe, reason);
			main->m_hook_failed_game_list.insert(key);
		}
	}

	obs_data_release(msg_data);
}

bool PLSBasic::_isSupportHEVC(const QVariantMap &info) const
{
	if (info.empty()) {
		return false;
	}

	auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();

	if (dataType >= ChannelData::CustomType) {
		return true;
	} else if (dataType == ChannelData::ChannelType) {
		auto name = info.value(ChannelData::g_platformName, "").toString();
		if (name == NAVER_SHOPPING_LIVE) {
			return true;
		} else if (name == YOUTUBE) {
			auto serviceData = PLSBasic::instance()->LoadServiceData();
			OBSDataAutoRelease settings = obs_data_get_obj(serviceData, "settings");
			const char *service = obs_data_get_string(settings, "service");
			if (strcmp(service, "YouTube - HLS") == 0) {
				return true;
			}
		}
	}
	return false;
}

bool PLSBasic::_isSupportHEVC()
{
	ChannelsMap temp = PLSChannelDataAPI::getInstance()->getCurrentSelectedChannels();
	bool result = std::all_of(temp.begin(), temp.end(), [this](const auto &info) { return _isSupportHEVC(info); });
	return result;
}

bool PLSBasic::CheckHEVC()
{
	const char *mode = config_get_string(basicConfig, "Output", "Mode");
	if (!mode)
		return false;
	const char *vencoderid = "";
	bool advOut = astrcmpi(mode, "Advanced") == 0;
	if (!advOut) {
		// simple streaming
		const char *vencoder = config_get_string(basicConfig, "SimpleOutput", "StreamEncoder");
		if (pls_is_empty(vencoder))
			return false;

		vencoderid = get_simple_output_encoder(vencoder);
	} else {
		vencoderid = config_get_string(basicConfig, "AdvOut", "Encoder");
	}

	if (pls_is_empty(vencoderid))
		return false;

	const char *codec = obs_get_encoder_codec(vencoderid);
	if (!pls_is_equal(codec, "hevc")) {
		// never use hevc encoder
		return true;
	}

	if (_isSupportHEVC()) {
		return true;
	}

	auto showWarning = [this]() {
		pls_alert_error_message(getMainWindow(), QTStr("Alert.Title"), QTStr("Hevc.alert.unsupport"));
		if (!pls_is_output_actived()) {
			showSettingVideo();
		}
	};
	QMetaObject::invokeMethod(getMainWindow(), showWarning, Qt::QueuedConnection);

	// Warning : Only VLIVE platform can support hevc
	return false;
}

void PLSBasic::showSettingVideo()
{
	onPopupSettingView(QStringLiteral("Output"), QString());
}

bool PLSBasic::isAppUpadting() const
{
	return m_isAppUpdating;
}

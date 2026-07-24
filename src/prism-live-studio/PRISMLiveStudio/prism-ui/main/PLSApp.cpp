#ifdef _WIN32
#include "windows/PLSSoftStatistic.h"
#include <VersionHelpers.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#else
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#endif // _WIN32
#include "PLSApp.h"
#include <qdir.h>
#include "libhttp-client.h"
#include "frontend-api.h"
#include "liblog.h"
#include "log/module_names.h"
#include "network-state.h"
#include "pls-common-language.hpp"
#include "pls-common-define.hpp"
#include "PLSDialogView.h"
#include "qevent.h"
#include "qt-wrappers.hpp"
#include "PLSBasic.h"
#include "PLSErrorHandler.h"
#include "login-user-info.hpp"
#include "PLSEvents.h"
#include "prism-version.h"
#include "PLSDumpAnalyzer.h"
#ifdef _WIN32
#include <Windows.h>
#include "windows/PLSBlockDump.h"
#elif defined(__APPLE__)
#include "mac/PLSBlockDump.h"
#include "mac/PLSMacNotificationCenter.h"
#endif
#include "libipc.h"

#include "PLSLaunchWizardView.h"
#include "PLSGuideTipsframe.h"
#include "platform.hpp"
#include "PLSMotionDefine.h"

#include <QProxyStyle>
#include <QScreen>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <qradiobutton.h>
#include <PLSComboBox.h>
#include <qtabbar.h>
#include <QLocalSocket>

#include <pls-shared-values.h>
#include "PLSPlatformApi.h"
#include "PLSCheckBox.h"
#include "window-basic-settings.hpp"
#include "PLSPlatformPrism.h"
#include "log/log.h"
#include <libutils-api.h>
#include "PLSLoginDataHandler.h"
#include "PLSPrismShareMemory.h"
#include "PLSSyncServerManager.hpp"
#include "pls/pls-obs-api.h"
#include <libresource.h>
#include "PLSSceneTemplateResourceMgr.h"
#include "test-tools/PLSTestTools.hpp"
#include "pls-performance.h"
#include <libui.h>
#include <pls/pls-base.h>
#ifdef ENABLE_TEST
#include "TestCase.h"
#endif
#include "prism-login/ui/PLSLoginMainView.h"
#include "PLSErrorCodeTransformTool.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "dbghelp.lib")
#define UseFreeMusic

constexpr const char *SIDE_BAR_WINDOW_INITIALLIZED = "sideBarWindowInitialized";
constexpr const char *IS_CHAT_IS_HIDDEN_FIRST_SETTED = "isChatIsHiddenFirstSetted";

constexpr const char *PLS_PROJECT_NAME = "P8e4826_PRISMLiveStudio-ZT";
constexpr const char *PLS_PROJECT_NAME_KR = "P8e4826_PRISMLiveStudio-KR";
constexpr auto DEFAULT_LANG = "en-US";
constexpr auto PRISM_TM_TEMPLATE_WEB_PATH = "PRISMLiveStudio/textmotion/web/index.html";

using namespace std;
using namespace common;

extern bool file_exists(const char *path);
int pls_auto_dump();
extern void httpRequestHead(QVariantMap &headMap, bool hasGacc);
extern QString getOsVersion();

void QtMessageCallback(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
	QByteArray localMsg = msg.toLocal8Bit();

	// Check log which we care about.
	if (type == QtWarningMsg && (msg.contains("QMetaObject::invokeMethod: No such method") || msg.contains("QMetaMethod::invoke: Unable to handle unregistered datatype"))) {
		PLS_WARN(MAINFRAME_MODULE, "%s", localMsg.constData());
		assert(false && "invokeMethod exception");
		return;
	}
	switch (type) {
	case QtDebugMsg:
		pls_debug(false, "QTLog", ctx.file, ctx.line, "[QT::Debug] %s", localMsg.constData());
		break;
	case QtInfoMsg:
		pls_debug(false, "QTLog", ctx.file, ctx.line, "[QT::Info] %s", localMsg.constData());
		break;
	case QtWarningMsg:
		pls_debug(false, "QTLog", ctx.file, ctx.line, "[QT::Warning] %s", localMsg.constData());
		break;
	case QtCriticalMsg:
		pls_info(false, "QTLog", ctx.file, ctx.line, "[QT::Critical] %s", localMsg.constData());
		break;
	case QtFatalMsg:
		pls_info(false, "QTLog", ctx.file, ctx.line, "[QT::Fatal] %s", localMsg.constData());
		break;
	default:
		break;
	}
}

QString getErrorInfoStr(const QString &errorInfo)
{
	QString str = QString("%1\n%2 \n%3").arg(QTStr(PRISM_ALERT_INIT_FAILED)).arg(errorInfo).arg(Str("Contact.Prism"));
	return str;
}
#ifdef _WIN32
void closeHandle(HANDLE &handle)
{
	if (handle) {
		::CloseHandle(handle);
		handle = nullptr;
	}
}
template<typename Cleanup> void closeHandle(HANDLE &handle, Cleanup cleanup)
{
	if (handle) {
		cleanup(handle);
		::CloseHandle(handle);
		handle = nullptr;
	}
}
#endif

void printTotalStartTime()
{
	static bool reportedStartTime = false;
	if (reportedStartTime) {
		return;
	}
	reportedStartTime = true;

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	std::string seconds = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now - GlobalVars::startTime).count());
	PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"launchDuration", seconds.c_str()}}, "PRISMLiveStudio launch duration");
	runtime_stats(PLS_RUNTIME_STATS_TYPE_APP_STARTED, now);
	//PRISM/lizhiyong/20251126/PRISM_PC-4622/add render drop types
	pls_set_launch_render_drop_count(obs_get_lagged_frames());
	PLSBasic::instance()->SetPreviewRenderPerf("MainView just show");
}
static bool filterMouseEvent(QObject *obj, QMouseEvent *event)
{
	if (auto dialogView = dynamic_cast<PLSDialogView *>(obj); dialogView) {
		obj->eventFilter(obj, event);
	}

	switch (event->button()) {
	case Qt::NoButton:
	case Qt::LeftButton:
	case Qt::RightButton:
	case Qt::AllButtons:
	case Qt::MouseButtonMask:
		return false;
	default:
		break;
	}

	if (!App()->HotkeysEnabledInFocus())
		return true;

	obs_key_combination_t hotkey = {0, OBS_KEY_NONE};
	bool pressed = event->type() == QEvent::MouseButtonPress;

	switch (event->button()) {
	case Qt::MiddleButton:
		hotkey.key = OBS_KEY_MOUSE3;
		break;

#define MAP_BUTTON(i, j)                       \
	case Qt::ExtraButton##i:               \
		hotkey.key = OBS_KEY_MOUSE##j; \
		break

		MAP_BUTTON(1, 4);
		MAP_BUTTON(2, 5);
		MAP_BUTTON(3, 6);
		MAP_BUTTON(4, 7);
		MAP_BUTTON(5, 8);
		MAP_BUTTON(6, 9);
		MAP_BUTTON(7, 10);
		MAP_BUTTON(8, 11);
		MAP_BUTTON(9, 12);
		MAP_BUTTON(10, 13);
		MAP_BUTTON(11, 14);
		MAP_BUTTON(12, 15);
		MAP_BUTTON(13, 16);
		MAP_BUTTON(14, 17);
		MAP_BUTTON(15, 18);
		MAP_BUTTON(16, 19);
		MAP_BUTTON(17, 20);
		MAP_BUTTON(18, 21);
		MAP_BUTTON(19, 22);
		MAP_BUTTON(20, 23);
		MAP_BUTTON(21, 24);
		MAP_BUTTON(22, 25);
		MAP_BUTTON(23, 26);
		MAP_BUTTON(24, 27);

#undef MAP_BUTTON

	default:
		break;
	}

	hotkey.modifiers = TranslateQtKeyboardEventModifiers(event->modifiers());
	obs_hotkey_inject_event(hotkey, pressed);
	return true;
}
static bool filterKeyEvent(QObject *obj, const QKeyEvent *event)
{
	if (!App()->HotkeysEnabledInFocus())
		return true;

	auto dialog = qobject_cast<QDialog *>(obj);

	obs_key_combination_t hotkey = {0, OBS_KEY_NONE};
	bool pressed = event->type() == QEvent::KeyPress;

	switch (event->key()) {
	case Qt::Key_Shift:
	case Qt::Key_Control:
	case Qt::Key_Alt:
	case Qt::Key_Meta:
		break;

#ifdef __APPLE__
	case Qt::Key_CapsLock:
		// kVK_CapsLock == 57
		hotkey.key = obs_key_from_virtual_key(57);
		pressed = true;
		break;
#endif

	case Qt::Key_Enter:
	case Qt::Key_Escape:
	case Qt::Key_Return:
		if (dialog && pressed)
			return false;
			/* Falls through. */
#ifdef _WIN32
		__fallthrough;
#endif
	default:
		hotkey.key = obs_key_from_virtual_key(event->nativeVirtualKey());
		break;
	}

	hotkey.modifiers = TranslateQtKeyboardEventModifiers(event->modifiers());

	obs_hotkey_inject_event(hotkey, pressed);
	return true;
}
static QString getExceptionAlertString(init_exception_code code)
{
	QString alertString = "";
	switch (code) {
	case init_exception_code::common:
		alertString = "Failed to initialize, common error.";
		break;
	case init_exception_code::engine_not_support:
		alertString = "Failed to initialize, not support engine driver.";
		break;
	case init_exception_code::engine_param_error:
		alertString = "Failed to initialize, engine param error.";
		break;
	case init_exception_code::engine_not_support_dx_version:
		alertString = "Failed to initialize, not support dx version.";
		break;
	case init_exception_code::failed_create_profile_directory:
		alertString = "Failed to initialize, failed create profile directory.";

		break;
	case init_exception_code::failed_create_required_user_directory:
		alertString = "Failed to initialize, failed create user directory.";

		break;
	case init_exception_code::failed_find_locale_file:
		alertString = "Failed to initialize, not fond locale files.";

		break;
	case init_exception_code::failed_init_application_bunndle:
		alertString = "Failed to initialize, failed init application bunndle.";

		break;
	case init_exception_code::failed_init_global_config:
		alertString = "Failed to initialize, failed init global config.";

		break;
	case init_exception_code::failed_load_locale:
		alertString = "Failed to initialize, failed load locale.";

		break;
	case init_exception_code::failed_load_theme:
		alertString = "Failed to initialize, failed load theme.";

		break;
	case init_exception_code::failed_open_locale_file:
		alertString = "Failed to initialize, failed open locale file.";

		break;
	case init_exception_code::failed_unknow_exception:
		alertString = "Failed to initialize, unknown exception.";
		break;
	case init_exception_code::failed_fasoo_reason:
		alertString = "Failed to initialize, fasoo reason.";
		break;
	case init_exception_code::prism_already_running:
		alertString = "prism is already running.";
		break;
	case init_exception_code::failed_startup_obs:
		alertString = "start obs config failed";
		break;
	default:
		break;
	}
	return alertString;
}

static bool doLogBlacklistSoftware(const std::string &stage)
{
#ifdef _WIN32
	std::vector<SoftInfo> vSoft = pls_installed_software();
	if (!vSoft.empty()) {
		for (const auto &item : vSoft) {
			std::string softDesc = item.softName + " v" + item.softVersion;
			PLS_LOGEX(pls_log_level_t::PLS_LOG_INFO, MAINFRAME_MODULE,
				  {{"stageDesc", stage.c_str()},
				   {"blacklistKey", item.key.c_str()},
				   {"softName", item.softName.c_str()},
				   {"softVersion", item.softVersion.c_str()},
				   {"softDesc", softDesc.c_str()},
				   {"softPublisher", item.softPublisher.c_str()}},
				  "Installed blacklist software detected. \nstageDesc : %s \nblacklistKey : %s \nsoftDesc : %s \nsoftPublisher : %s ", stage.c_str(), item.key.c_str(),
				  softDesc.c_str(), item.softPublisher.c_str());
		}
		return true;
	} else
		PLS_LOGEX(pls_log_level_t::PLS_LOG_INFO, MAINFRAME_MODULE, {{"softName", "No"}}, "No blacklist software installed. \nblacklist softName : No .");
#endif
	// TODO: - mac next >>> check fasoo, may not need in mac
	return false;
}

string CurrentTimeString()
{
	return CurrentDateTimeString();
}

string CurrentDateTimeString()
{
	auto curTime = QDateTime::currentDateTime();
	return curTime.toString("yyyy-MM-dd hh:mm:ss.zzz").toStdString();
}

QString get_windows_version_info()
{
#ifdef _WIN32
	bool server = IsWindowsServer();
	bool x64bit = true;
	if (sizeof(void *) != 8) {
		x64bit = false;
	}
	QString bitStr = x64bit ? "(x64)" : "(x32)";

	uint32_t version_info = GetWindowsVersion();
	uint32_t build_version = GetWindowsBuild();

	QString ver_str("");
	if (version_info >= 0x0A00) {
		if (server) {
			ver_str = "Windows Server 2016 Technical Preview";
		} else {
			ver_str = (build_version >= 21664) ? "Windows 11" : "Windows 10";
		}
	} else if (version_info >= 0x0603) {
		ver_str = server ? "Windows Server 2012 r2" : "Windows 8.1";
	} else if (version_info >= 0x0602) {
		ver_str = server ? "Windows Server 2012" : "Windows 8";
	} else if (version_info >= 0x0601) {
		ver_str = server ? "Windows Server 2008 r2" : "Windows 7";
	} else if (version_info >= 0x0600) {
		ver_str = server ? "Windows Server 2008" : "Windows Vista";
	} else {
		ver_str = "Windows Before Vista";
	}

	QString output_ver_info = ver_str + bitStr;
	return output_ver_info;
#else

	std::string osVersion = pls_libutil_api_mac::pls_get_system_version();
	std::string hardWare = pls_libutil_api_mac::pls_get_machine_hardware_name();

	return QString("(%1) (%2)").arg(osVersion.c_str(), hardWare.c_str());
#endif
}

static void do_log(int log_level, const char *format, va_list args, void *param)
{
	pls_unused(log_level, format, args, param);
}

static bool openConfig(ConfigFile &config, const char *fileName)
{
	std::array<char, 512> path;
	if (GetAppConfigPath(path.data(), sizeof(path), fileName) <= 0) {
		return false;
	} else if (int errorcode = config.Open(path.data(), CONFIG_OPEN_ALWAYS); errorcode != CONFIG_SUCCESS) {
		PLS_ERROR(MAINFRAME_MODULE, "Failed to open %s, errorcode = %d", fileName, errorcode);
		return false;
	}
	return true;
}
static void removeConfig(const char *config)
{
	PLS_PERFORMANCE_FUNCTION();

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

static QString parsePscPath(const QString &arg)
{
	QString pscPath = arg;
	pscPath.replace("\\", "/");
	PLS_INIT_INFO(MAINFRAME_MODULE, "set psc path: %s", pls_get_path_file_name(pscPath).toUtf8().constData());
	return pscPath;
}

PLSApp::PLSApp(int &argc, char **argv, profiler_name_store_t *store) : OBSApp(argc, argv, store)
{
	// force modify current work directory
	QDir::setCurrent(QApplication::applicationDirPath());

	pls_add_global_css({"ThirdPlugins"});

	pls::HotKeyLocker::setHotKeyCb([this]() { UpdateHotkeyFocusSetting(true); }, [this]() { DisableHotkeys(); });

	pls::http::setDefaultRequestHeadersFactory([]() {
		QVariantMap headMap;
		httpRequestHead(headMap, true);
		return headMap;
	});

#ifdef _WIN32
	std::ignore = CoInitialize(nullptr);
#endif
	qInstallMessageHandler(QtMessageCallback);
	installEventFilter(this);

	setWindowIcon(QIcon::fromTheme("obs", QIcon(":/res/images/PRISMLiveStudio.ico")));

	parseOpenFilePath([](const QStringList &cmdlines) {
		for (const auto &arg : cmdlines) {
			if (arg.startsWith(QStringLiteral("--running-path=")))
				return parsePscPath(arg.mid(15));
			else if (arg.endsWith(".psc", Qt::CaseInsensitive))
				return parsePscPath(arg);
		}
		return QString();
	});

	if (pls_is_dev_server()) {
		pls_load_dev_server();
	}
}

PLSApp::~PLSApp()
{
	pls_set_app_exiting(true);
	pls_set_obs_exiting(true);

#ifdef _WIN32
	CoUninitialize();
#endif

	if (!pls_is_debugger_present()) {
		auto threadExit = std::thread([] {
			this_thread::sleep_for(10s);

			PLS_LOGEX(PLS_LOG_ERROR, MAINFRAME_MODULE, {{"exitTimeout", to_string(static_cast<int>(init_exception_code::timeout_by_shutdown)).data()}}, "PRISM exit timeout");

			pls_capture_force_exit_dump("timeout_by_shutdown");
			pls_log_cleanup();

#if defined(Q_OS_MACOS)
			pid_t pid = getpid();
			kill(pid, SIGKILL);
#endif

#if defined(Q_OS_WINDOWS)
			TerminateProcess(GetCurrentProcess(), 0);
#endif
		});
		threadExit.detach();
	}
}

void PLSApp::AppInit()
{
	PLS_PERFORMANCE_FUNCTION();
	OBSApp::AppInit();
	//create the navershopping global init
	if (!openConfig(naverShoppingConfig, "PRISMLiveStudio/naver_shopping/naver_shopping.ini")) {
		PLS_ERROR(MAINFRAME_MODULE, "navershopppingcofig  init error");
	}

	//open the cookie init file
	if (!openConfig(cookieConfig, "PRISMLiveStudio/Cache/cookies.ini")) {
		PLS_ERROR(MAINFRAME_MODULE, "cookies  init error");
	}

	PLS_SYNC_SERVER_MANAGE;
	PLS_PLATFORM_API;
	PLS_PLATFORM_PRSIM; // zhangdewen singleton init

	// set scene template desgin mode flag for libobs
	bool supportExportTemplates = pls_get_qsetting_value("SupportExportTemplates", false).toBool();
	pls_set_design_mode(supportExportTemplates);

	//add scene templates custom font database
	QString path = QString(SCENE_TEMPLATE_DIR).append("custom_font/");
	auto customFontPath = pls_get_user_path(path);
	pls_add_custom_font(customFontPath);

	//add custom font database
	auto fontPath = pls_get_app_user_data_dir_path_pn(QStringLiteral("/textmotion/web/static/fonts"), false);
	pls_add_custom_font(fontPath);
	//add custom chatv2 font database
#if defined(Q_OS_WIN)
	QString chatV2FontPath = pls_get_app_data_path(QStringLiteral("/prism-plugins/prism-chatv2-source/fonts"));
#elif defined(Q_OS_MACOS)
	QString chatV2FontPath = pls_get_dll_dir("prism-chatv2-source") + "/fonts";
#endif
	pls_add_custom_font(chatV2FontPath);
}

bool PLSApp::PLSInit()
{
	PLS_PERFORMANCE_FUNCTION();
	PLSLoginDataHandler::instance()->initCustomChannelObj();
	if (!OBSApp::OBSInit()) {
		return false;
	}
	connect(getMainView(), &PLSMainView::popupSettingView, PLSBasic::instance(), &PLSBasic::onPopupSettingView);

	// Unified interface to init side bar window visible
	initSideBarWindowVisible();

	//add login event
	PLSBasic::instance()->getApi()->on_event(pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGIN);
	//PLS_INIT_INFO(MAINFRAME_MODULE, "start channels update all rtmps .");
	//PLSBasic::instance()->CreateHotkeys();
	//PRISM/WangChuanjing/20200825/#3423/for main view load delay
	//obs_set_system_initialized(true);

	QString verStr;
#if defined(Q_OS_WIN)
	pls_win_ver_t ver = pls_get_win_ver();
	verStr = QString("%1.%2.%3").arg(ver.minor).arg(ver.build).arg(ver.revis);
#elif defined(Q_OS_MACOS)
	pls_mac_ver_t ver = pls_get_mac_systerm_ver();
	verStr = QString("%1.%2.%3").arg(ver.minor).arg(ver.patch).arg(ver.buildNum.c_str());
#endif

	PLS_LOGEX(PLS_LOG_INFO, MAINFRAME_MODULE, {{"startState", "finished"}, {"OSMinorVersion", verStr.toUtf8().constData()}}, "System has been initialized.");
	return true;
}
void PLSApp::clearNaverShoppingConfig()
{
	removeConfig("PRISMLiveStudio/naver_shopping/naver_shopping.ini");
	//create the navershopping global init
	pls::chars<512> path;
	int len = GetAppConfigPath(path, sizeof(path), "PRISMLiveStudio/naver_shopping/naver_shopping.ini");
	if (len <= 0) {
		return;
	}
	//open the update init file
	int errorcode = naverShoppingConfig.Open(path, CONFIG_OPEN_ALWAYS);
	if (errorcode != CONFIG_SUCCESS) {
		return;
	}
}
void PLSApp::InitCrashConfigDefaults() const
{
#if defined(Q_OS_MACOS)
#define MAX_PATH PATH_MAX
#endif

	std::array<char, MAX_PATH> path;
	int len = os_get_config_path(path.data(), MAX_PATH, PRISM_CRASH_CONFIG_PATH);
	if (len <= 0) {
		return;
	}

	obs_data *crashData = obs_data_create_from_json_file_safe(path.data(), "bak");
	if (!crashData)
		crashData = obs_data_create();

	obs_data *currentObj = obs_data_create();
	obs_data_set_string(currentObj, PRISM_SESSION, GlobalVars::prismSession.c_str());
	obs_data_set_array(currentObj, MODULES, nullptr);
	obs_data_set_obj(crashData, CURRENT_PRISM, currentObj);

	obs_data_save_json_safe(crashData, path.data(), "tmp", "bak");

	obs_data_release(currentObj);
	obs_data_release(crashData);
}

void PLSApp::initSideBarWindowVisible() const
{
	auto mainView = PLSBasic::instance()->getMainView();
	if (!mainView)
		return;
	if (mainView->isVisible()) {
		PLSBasic::instance()->initSideBarWindowVisible();
	} else {
		// app minimum to the system tray so here we show side bar window delay
		connect(
			mainView, &PLSMainView::isshowSignal, mainView,
			[mainView](bool isShow) {
				if (isShow && !mainView->property(SIDE_BAR_WINDOW_INITIALLIZED).toBool()) {
					mainView->setProperty(SIDE_BAR_WINDOW_INITIALLIZED, true);
					PLSBasic::instance()->initSideBarWindowVisible();
				}
			},
			Qt::QueuedConnection);
	}
}
void PLSApp::createLoadingApp()
{
	if (m_loadingAppPid > 0) {
		m_loadingAppPro = pls_process_create(m_loadingAppPid);
	} else {
		m_loadingAppPro = pls_create_loading_app(GlobalVars::prismSession.c_str(), GlobalVars::prismSubSession.c_str());
	}
}
void PLSApp::destoryLoadingApp()
{
	pls_destory_loading_app(m_loadingAppPro);
	m_loadingAppPro = nullptr;
	m_loadingAppPid = 0;
}
void PLSApp::setAnalogBaseInfo(QJsonObject &obj, bool isUploadHardwareInfo)
{

	obj.insert("hashMac", QString("%1").arg(hash<string>()(pls_get_local_mac().toStdString()), 16, 16, QChar('0')));
	obj.insert("hashUserID", PLSLoginUserInfo::getInstance()->getUserCodeWithEncode());
	obj.insert("gcc", pls_get_gcc());
	obj.insert("ver", PLS_VERSION);
	obj.insert("unixTime", QDateTime::currentMSecsSinceEpoch());
	obj.insert("envOsVer", getOsVersion());

	if (isUploadHardwareInfo) {
		obj.insert("envCpu", pls_get_cpu_name().c_str());
		obj.insert("envGpu", GlobalVars::videoAdapter.c_str());
		obj.insert("envMem", QString("%1MB").arg(os_get_sys_total_size() / 1024 / 1024));
	}
}

void PLSApp::backupGolbalConfig() const
{
	ConfigFile backupGlobalConfig;
	if (!openConfig(backupGlobalConfig, "PRISMLiveStudio/global.bak")) {
		PLS_WARN(MAINFRAME_MODULE, "open global.bak config failed.");
		return;
	}

	const char *sceneCollectionName = config_get_string(App()->GetUserConfig(), "Basic", "SceneCollection");
	const char *sceneCollectionFile = config_get_string(App()->GetUserConfig(), "Basic", "SceneCollectionFile");

	auto lastHideDate = config_get_string(App()->GetUserConfig(), common::LAUNCHER_CONFIG, common::CONFIG_LASTHIDEDATE);
	config_set_string(backupGlobalConfig, common::LAUNCHER_CONFIG, common::CONFIG_LASTHIDEDATE, lastHideDate);

	auto displayVer = config_get_string(App()->GetUserConfig(), common::NEWFUNCTIONTIP_CONFIG, common::CONFIG_DISPLAYVERISON);
	config_set_string(backupGlobalConfig, common::NEWFUNCTIONTIP_CONFIG, common::CONFIG_DISPLAYVERISON, displayVer);

	auto chatNewBadge = config_get_bool(App()->GlobalConfig(), common::CHAT_WIDGET_CONFIG, common::CHAT_WIDGET_NEW_BADGE);
	config_set_bool(backupGlobalConfig, common::CHAT_WIDGET_CONFIG, common::CHAT_WIDGET_NEW_BADGE, chatNewBadge);
	config_set_string(backupGlobalConfig, "Basic", "SceneCollection", sceneCollectionName);

	config_set_string(backupGlobalConfig, "Basic", "SceneCollectionFile", sceneCollectionFile);
	config_save(backupGlobalConfig);
}

const char *PLSApp::getProjectName() const
{
	return PLS_PROJECT_NAME;
}

const char *PLSApp::getProjectName_kr() const
{
	return PLS_PROJECT_NAME_KR;
}

bool PLSApp::notify(QObject *obj, QEvent *evt)
{
	if (pls_object_is_valid(obj)) {
		if (QThread::currentThread() == qApp->thread()) {
#if defined(_WIN32)
			PLSBlockDump::Instance()->UpdateNotifyEvent(obj, evt);
#elif defined(__APPLE__)
			PLSBlockDump::instance()->updateNotifyEvent(obj, evt);
#endif
		}

		return OBSApp::notify(obj, evt);
	}
	return false;
}

bool PLSApp::event(QEvent *event)
{
	if (PLSBasic ::Get() && (event->type() == QEvent::TabletEnterProximity || event->type() == QEvent::TabletLeaveProximity)) {
		bool active = event->type() == QEvent::TabletEnterProximity ? true : false;
		return true;
	}
#if defined(Q_OS_MACOS)
	if (event->type() == QEvent::FileOpen) {
		QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
		if (auto file = openEvent->file(); file.endsWith(".psc", Qt::CaseInsensitive)) {
			emit wakeUp({openEvent->file()});
		}
		const auto url = openEvent->url();
		if (url.scheme() == g_plsAppleIDCustomScheme) {
			PLS_INFO("apple auth callback url = %s", qPrintable(url.toString()));
			emit appleIDAuthCallbackUrl(url.toString());
		} else if (url.scheme() == g_plsFacebookCustomScheme) {
			PLS_INFO("facebook auth callback url = %s", qPrintable(url.toString()));
			emit facebookAuthCallbackUrl(url.toString());
			emit PLS_EVENTS->facebookAuthCallbackUrl(url.toString());
		}
	}
#endif
	return PLSUiApp::event(event);
}

void PLSApp::sessionExpiredhandler() const
{
	PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_SESSION_EXPIRED, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("PLSApp::sessionExpiredhandler"),
					      PLSBasic::instance()->getMainView());
	pls_prism_change_over_login_view();
}

bool PLSApp::eventFilter(QObject *obj, QEvent *event)
{

#ifdef Q_OS_MACOS
	if (event->type() == QEvent::Quit) {
		pls_set_app_exiting(true);
		pls_set_obs_exiting(true);
	}
#endif

	if (!obj->isWidgetType()) {
		return QApplication::eventFilter(obj, event);
	}

#if defined(Q_OS_WIN)
	if (event->type() == QEvent::CursorChange) {
		auto w = static_cast<QWidget *>(obj);
		switch (w->cursor().shape()) {
		case Qt::SplitVCursor:
			w->setCursor(Qt::SizeVerCursor);
			return true;
		case Qt::SplitHCursor:
			w->setCursor(Qt::SizeHorCursor);
			return true;
		default:
			break;
		}
	}
#endif

#ifdef DEBUG
	if (event->type() == QMouseEvent::MouseButtonPress) {
		for (auto o = obj; o; o = o->parent())
			qDebug() << "obj name = " << o->objectName() << "class name = " << o->metaObject()->className();
	}
#endif // DEBUG

	//if (obj->property("notShowHandCursor").toBool()) {
	//	return QApplication::eventFilter(obj, event);
	//}
	//
	//auto mo = obj->metaObject();
	//if (mo->inherits(&QComboBox::staticMetaObject) || mo->inherits(&QPushButton::staticMetaObject) || mo->inherits(&QToolButton::staticMetaObject) || mo->inherits(&QCheckBox::staticMetaObject) ||
	//    mo->inherits(&PLSCheckBox::staticMetaObject) || mo->inherits(&QRadioButton::staticMetaObject) || mo->inherits(&QSlider::staticMetaObject) || mo->inherits(&QListWidget::staticMetaObject) ||
	//    mo->inherits(&PLSComboBox::staticMetaObject) || mo->inherits(&QMenu::staticMetaObject) || mo->inherits(&PLSComboBoxListView::staticMetaObject) ||
	//    mo->inherits(&QSpinBox::staticMetaObject) || mo->inherits(&SilentUpdateCheckBox::staticMetaObject) || mo->inherits(&PLSElideCheckBox::staticMetaObject) ||
	//    mo->inherits(&QTabBar::staticMetaObject) || mo->inherits(&PLSRadioButton::staticMetaObject) || obj->property("showHandCursor").toBool()) {
	//	//#1795, when click close button to close dialog, when window deacitve, maybe trigger QEvent::Enter after deactive window.
	//	if (static_cast<QWidget *>(obj)->isVisible() && event->type() == QEvent::Enter) {
	//		static_cast<QWidget *>(obj)->setCursor(Qt::PointingHandCursor);
	//	} else if (event->type() == QEvent::Leave) {
	//		if (mo->inherits(&QMenu::staticMetaObject) && static_cast<QWidget *>(obj)->isVisible()) {
	//			return QApplication::eventFilter(obj, event);
	//		}
	//		static_cast<QWidget *>(obj)->unsetCursor();
	//	}
	//}

	return QApplication::eventFilter(obj, event);
}

void PLSApp::onIpcConnected(pls_ipc_t ipc) {}
void PLSApp::onIpcDisconnected(pls_ipc_t ipc) {}
void PLSApp::onIpcMessage(pls_ipc_t ipc, int type, const QJsonValue &data) {}

int PLSApp::runProgram(PLSApp &program, int argc, char *argv[], ScopeProfiler &prof)
{
	PLS_INFO("PLSAPP", "get current app runing path %s", qPrintable(pls_get_app_dir()));
	PLS_PERFORMANCE_FUNCTION();

	PLS_PERFORMANCE_START(request_initial_api);
	PLSInitApiFlow::instance()->startInitApisFlow();
	PLS_PERFORMANCE_END(request_initial_api);

	try {
		auto pid = pls_cmdline_get_arg(pls_cmdline_args(), shared_values::k_launcher_command_loading_app_pid);
		if (pid.has_value()) {
			program.m_loadingAppPid = pid.value().toInt();
			PLS_INFO("PLSAPP", "get loading pid  = %d", program.m_loadingAppPid);
		}
		auto restartType = pls_cmdline_get_uint32_arg(pls_cmdline_args(), shared_values::k_launcher_command_type);
		PLS_INFO("PLSAPP", "restartType = %d", restartType);
		if (restartType == static_cast<int>(RestartAppType::Logout)) {
			PLSBasic::clearPrismConfigInfo();
		}
#if defined(Q_OS_WIN)
		auto authCallbackUrl = pls_cmdline_get_arg(pls_cmdline_args(), shared_values::k_auth2_apple_id_callback);
		if (authCallbackUrl.has_value()) {
			PLS_INFO("PLSAPP", "authCallbackUrl = %s", authCallbackUrl.value().toUtf8().constData());
			QLocalSocket socket;
			socket.connectToServer("PRISMLiveStudio", QIODevice::WriteOnly);
			if (socket.waitForConnected(500)) {
				socket.write(authCallbackUrl.value().toUtf8());
				socket.flush();
				if (!socket.waitForBytesWritten(500)) {
					PLS_WARN("PLSAPP", "Timeout waiting for bytes to be written, error = %s", socket.errorString().toUtf8().constData());
				}
			}
			socket.close();
		} else {
			PLS_INFO("PLSAPP", "authCallbackUrl is nullptr");
		}
#endif
		program.AppInit();

		OBSApp::deleteOldestFile(false, "PRISMLiveStudio/profiler_data");

		if (!pls_singleton_app_instance([&program](const QStringList &cmdlines) { pls_async_call(&program, [&program, cmdlines]() { emit program.wakeUp(cmdlines); }); })) {
			throw init_exception_code::prism_already_running;
		}

#if defined(Q_OS_MACOS)
		const auto &obsVersion = pls_get_installed_obs_version();
		PLS_INIT_INFO(MAINFRAME_MODULE, obsVersion.isEmpty() ? "The installed obs studio app not founded%s" : "The installed obs studio app version is %s", obsVersion.toUtf8().constData());
#endif

		program.initPeerApp();
		program.initIpc();
		QObject::connect(&program, &PLSApp::wakeUp, &program, [&program](const QStringList &args) {
			PLS_DEBUG(MAINFRAME_MODULE, "wake up triggered.");
			if (pls_get_app_exiting()) {
				return;
			}

			program.parseOpenFilePath(args);
			if (auto loginView = PLSLoginMainView::get(); loginView) {
				PLS_DEBUG(MAINFRAME_MODULE, "wake up login view.");
				pls_bring_window_to_front(loginView->winId());
			} else if (auto modalWidget = QApplication::activeModalWidget(); modalWidget) {
				PLS_DEBUG(MAINFRAME_MODULE, "wake up modal widget.");
				pls_bring_window_to_front(modalWidget->winId());
			} else if (!program.isAppRunning()) {
				return;
			} else if (auto basic = PLSBasic::instance(); basic) {
				PLS_DEBUG(MAINFRAME_MODULE, "wake up main view.");
				basic->singletonWakeup();
			}
		});

		if (restartType != static_cast<int>(RestartAppType::Update)) {
			program.createLoadingApp();
		}

		removeConfig(QString("PRISMLiveStudio/user/%1.png").arg(PLSLoginUserInfo::getInstance()->getAuthType()).toUtf8().constData());

		PLS_INIT_INFO(MAINFRAME_MODULE, "app configuration information initialization completed.");

		// Check registry flag to skip resource download for debugging
		// Set "SkipResourceDownload" to true in registry to skip resource download
		// Windows: HKEY_CURRENT_USER\Software\NAVER Corporation\Prism Live Studio\SkipResourceDownload
		if (bool skipResourceDownload = pls_get_qsetting_value("SkipResourceDownload", false).toBool(); !skipResourceDownload) {
			pls::rsm::downloadAll();
		} else {
			PLS_INIT_INFO(MAINFRAME_MODULE, "SkipResourceDownload flag is set, skipping resource download for debugging.");
		}

		GuideRegisterManager::instance()->load();

		PLS_INIT_INFO(MAINFRAME_MODULE, "init program");
		if (!program.PLSInit()) {
			PLS_ERROR(MAINFRAME_MODULE, "init mainView failed");
			QMetaObject::invokeMethod(
				&program,
				[]() {
					auto mainView = App()->getMainView();
					mainView->setProperty("forceClose", true);
					mainView->close();
				},
				Qt::QueuedConnection);
			return PLSApp::exec();
		}
		prof.Stop();

		program.setAppRunning(true);

		GuideRegisterManager::instance()->beginShow();

		program.destoryLoadingApp();

		doLogBlacklistSoftware("PrismStarted");

		pls_async_call_mt([&program, argc, argv]() {
			pls_add_tools_menu_seperator();
			pls_init_test_tools(PLSBasic::instance()->ui->menuTools, [](const std::function<void(const char *name, const std::function<void()> &show)> &add_tool) {
				add_tool("Error Code Transform Tool", []() {
					if (auto view = PLSErrorCodeTransformTool::instance(); view)
						view->show();
				});
			});

#ifdef ENABLE_TEST
			tests::genFileName();
			if (GlobalVars::unitTest) {
				GlobalVars::unitTestExitCode = tests::run(argc, argv);
				program.getMainView()->close();
			} else {
				tests::show(argc, argv);
			}
#endif
		});

		return PLSApp::exec();

	} catch (init_exception_code code) {
		if (code == init_exception_code::engine_not_support && doLogBlacklistSoftware("PrismInitFailed")) {
			code = init_exception_code::failed_fasoo_reason;
		}
		QString codeStr = getExceptionAlertString(code);
		PLS_LOGEX(PLS_LOG_ERROR, MAINFRAME_MODULE, {{"launchFailed", QString("0x%1%2").arg(static_cast<int>(code), 0, 16).arg(pls_get_init_exit_code_str(code)).toUtf8().constData()}},
			  "failed to initialize, %s", codeStr.toUtf8().constData());
		pls_set_app_exiting(true);
		pls_set_obs_exiting(true);

		return static_cast<int>(code);
	} catch (...) {
		auto code = init_exception_code::failed_unknow_exception;
		QString codeStr = getExceptionAlertString(code);
		PLS_LOGEX(PLS_LOG_ERROR, MAINFRAME_MODULE, {{"launchFailed", QString("0x%1%2").arg(static_cast<int>(code), 0, 16).arg(pls_get_init_exit_code_str(code)).toUtf8().constData()}},
			  "failed to initialize, unknown exception catched");
		pls_set_app_exiting(true);
		pls_set_obs_exiting(true);

		doLogBlacklistSoftware("PrismInitFailed");
		pls_process_terminate(static_cast<int>(code));
		return static_cast<int>(code);
	}
}

void PLSApp::generatePrismSessionAndSubSession(int argc, char *argv[])
{
	for (int i = 1; i < argc; i++) {
		QString argument = argv[i];
		if (argument.startsWith(shared_values::k_launcher_command_log_prism_session)) {
			GlobalVars::prismSession = argument.remove(0, QString(shared_values::k_launcher_command_log_prism_session).size()).toStdString();
		} else if (argument.startsWith(shared_values::k_launcher_command_log_sub_prism_session)) {
			GlobalVars::prismSubSession = argument.remove(0, QString(shared_values::k_launcher_command_log_sub_prism_session).size()).toStdString();
		}
	}
	if (GlobalVars::prismSession.empty()) {
		GlobalVars::prismSession = QUuid::createUuid().toString().toStdString();
	}
	if (GlobalVars::prismSubSession.empty()) {
		GlobalVars::prismSubSession = QUuid::createUuid().toString().toStdString();
	}
}

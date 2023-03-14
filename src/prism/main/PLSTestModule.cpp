#include "PLSTestModule.h"
#include "ui_PLSTestModule.h"
#include <windows.h>
#include "log/log.h"
#include <util/platform.h>
#include <util/util.hpp>
#include "qt-wrappers.hpp"
#include "pls-app.hpp"
#include "log.h"
#include "action.h"
#include "log/module_names.h"
#include "pls-app.hpp"
#include "window-basic-main.hpp"
#include "pls-common-language.hpp"
#include "PLSDialogSavePCM.h"
#include <thread>

enum init_exception_type {
	init_exception_type_engine_crash = 0,
	init_exception_type_engine_failed,
	init_exception_type_engine_invalid_param,
	init_exception_type_engine_invalid_dx_version,
	init_exception_type_prism_failed,
	init_exception_type_engine_check_timeout,
};

enum runtime_exception_type {
	runtime_exception_texture_outofmemory = 0,
	runtime_exception_graphics_not_support_dx11,
	runtime_exception_device_rebuild_failed,
	runtime_exception_camera_process_crash,
	runtime_exception_camera_process_disappear,
	runtime_exception_camera_process_empty_path,
	runtime_exception_ui_block_for_while,
	runtime_save_pcm,
	runtime_exception_enable_ref_alert,
	runtime_exception_disable_ref_alert,
	runtime_exception_subprocess_devicerebuild,
	runtime_exception_main_engine_rebuild_failed,
	runtime_exception_prism_alert_popup,
};

enum crash_exception_type {
	crash_exception_vst_plugin = 0,
	crash_exception_system_outofmemery,
	crash_exception_grahpics_driver,
	crash_exception_video_device,
	crash_exception_audio_device,
	crash_exception_others,
};

struct test_info {
	QString strName;
	int index;
};

test_info init_exception_list[] = {{"Initiation crashed, graphics driver error", init_exception_type_engine_crash},
				   {"Failed to initialize, update graphics driver", init_exception_type_engine_failed},
				   {"Failed to initialize, contact us", init_exception_type_engine_invalid_param},
				   {"Unsupported DirectX version", init_exception_type_engine_invalid_dx_version},
				   {"Other common alert", init_exception_type_prism_failed},
				   {"Initiation EnvCheck timeout", init_exception_type_engine_check_timeout}};

test_info runtime_exception_list[] = {{"Insufficient video memory", runtime_exception_texture_outofmemory},
				      {"Graphics not support DX11", runtime_exception_graphics_not_support_dx11},
				      {"Device rebuild failed", runtime_exception_device_rebuild_failed},
				      {"Camera process crash", runtime_exception_camera_process_crash},
				      {"Camera process disappear", runtime_exception_camera_process_disappear},
				      {"Camera process empty path crash", runtime_exception_camera_process_empty_path},
				      {"UI block for 20s", runtime_exception_ui_block_for_while},
				      {"Enable alert for reference count", runtime_exception_enable_ref_alert},
				      {"Disable alert for reference count", runtime_exception_disable_ref_alert},
				      {"Subprocess device rebuild", runtime_exception_subprocess_devicerebuild},
				      {"Main engine rebuild failed", runtime_exception_main_engine_rebuild_failed},
				      {"Disable alert for reference count", runtime_exception_disable_ref_alert},
				      {"Save audio into PCM", runtime_save_pcm},
				      {"Popup Prism alert messagebox every 3s", runtime_exception_prism_alert_popup}};

test_info crash_exception_list[] = {{"Vst plugin crash", crash_exception_vst_plugin},
				    {"System out of memory", crash_exception_system_outofmemery},
				    /*  {"Graphics driver crash", crash_exception_grahpics_driver},
				    {"Video device crash", crash_exception_video_device},  {"Audio device crash", crash_exception_audio_device},*/
				    {"Other type crash", crash_exception_others}};

PLSTestModule::PLSTestModule(QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView(parent, dpiHelper), ui(new Ui::PLSTestModule)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSLoadingBtn, PLSCssIndex::PLSContactView});
	dpiHelper.setInitSize(this, {480, 400});
	ui->setupUi(this->content());
	QMetaObject::connectSlotsByName(this);

	int count = sizeof(init_exception_list) / sizeof(test_info);
	for (int i = 0; i < count; i++) {
		ui->initExceptionComboBox->addItem(init_exception_list[i].strName);
	}

	count = sizeof(runtime_exception_list) / sizeof(test_info);
	for (int i = 0; i < count; i++) {
		ui->runtimeExceptionComboBox->addItem(runtime_exception_list[i].strName);
	}

	count = sizeof(crash_exception_list) / sizeof(test_info);
	for (int i = 0; i < count; i++) {
		ui->crashExceptionComboBox->addItem(crash_exception_list[i].strName);
	}

	connect(ui->pushButtonQuit, &QPushButton::clicked, qApp, &QCoreApplication::quit);
	connect(ui->pushButtonExit, &QPushButton::clicked, [] { exit(0); });
	connect(ui->pushButtonAbort, &QPushButton::clicked, abort);
	connect(ui->pushButtonExitProcess, &QPushButton::clicked, [] { ExitProcess(0); });
}

PLSTestModule::~PLSTestModule()
{
	if (m_pTimerForAlert)
		m_pTimerForAlert->stop();
	delete ui;
}

std::wstring get_process_path_for_test()
{
	const std::wstring process_name = L"EnvCheck.exe";
	WCHAR szFilePath[MAX_PATH] = {};
	GetModuleFileNameW(NULL, szFilePath, MAX_PATH);

	int nLen = (int)wcslen(szFilePath);
	for (int i = nLen - 1; i >= 0; --i) {
		if (szFilePath[i] == '\\') {
			szFilePath[i + 1] = 0;
			break;
		}
	}

	return std::wstring(szFilePath) + process_name;
}

static QString getInitExceptionAlertString(init_exception_code code)
{
	QString alertString = "";
	switch (code) {
	case init_exception_code_common:
		alertString = QTStr(PRISM_ALERT_INIT_FAILED);
		break;
	case init_exception_code_engine_not_support:
		alertString = QTStr(ENGINE_ALERT_INIT_FAILED);
		break;
	case init_exception_code_engine_param_error:
		alertString = QTStr(ENGINE_ALERT_INIT_PARAM_ERROR);
		break;
	case init_exception_code_engine_not_support_dx_version:
		alertString = QTStr(ENGINE_ALERT_INIT_DX_VERSION);
		break;
	default:
		break;
	}
	return alertString;
}

int init_engine_test(struct obs_video_info *ovi, gs_engine_test_type test_type)
{
	int ret = obs_engine_check_for_test(ovi, test_type);
	switch (ret) {
	case OBS_VIDEO_MODULE_NOT_FOUND: {
		PLS_ERROR("PLSTestModule", "Failed to initialize video:  Graphics module not found");
		throw init_exception_code_common;
	}
	case OBS_VIDEO_NOT_SUPPORTED:
		throw init_exception_code_engine_not_support; //init failed
	case OBS_VIDEO_INVALID_PARAM:
		throw init_exception_code_engine_param_error; //invalid param
	case OBS_VIDEO_NOT_SUPPORTED_ENGINE_VERSION:
		throw init_exception_code_engine_not_support_dx_version; //dx version
	default:
		if (ret != OBS_VIDEO_SUCCESS) {
			throw init_exception_code_common;
		}
	}
	return ret;
}

int initPrismForTest()
{
	const char *sceneCollection = config_get_string(App()->GlobalConfig(), "Basic", "SceneCollectionFile");
	char savePath[512];
	char fileName[512];
	int ret;

	if (!sceneCollection) {
		PLS_ERROR(MAIN_TEST_MODE, "Failed to get scene collection name");
		throw init_exception_code_common;
	}

	ret = snprintf(fileName, 512, "PRISMLiveStudio/basic/scenes/%s.json", sceneCollection);
	if (ret <= 0) {
		PLS_ERROR(MAIN_TEST_MODE, "Failed to create scene collection file name");
		throw init_exception_code_common;
	}

	ret = GetConfigPath(savePath, sizeof(savePath), fileName);
	if (ret <= 0) {
		PLS_ERROR(MAIN_TEST_MODE, "Failed to get scene collection json file path");
		throw init_exception_code_common;
	}

	PLS_INFO(MAIN_TEST_MODE, "Init success, return common exception for auto test");
	throw init_exception_code_common;
}

int getInitExceptionId(QString name)
{
	int count = sizeof(init_exception_list) / sizeof(test_info);
	for (int i = 0; i < count; i++) {
		if (name == init_exception_list[i].strName) {
			return init_exception_list[i].index;
		}
	}
	return -1;
}

void PLSTestModule::on_initExceptionBtn_clicked()
{
	QString sel_text = ui->initExceptionComboBox->currentText();
	int index = getInitExceptionId(sel_text);
	switch (index) {
	case init_exception_type_engine_crash:
		initEngineCrash();
		break;
	case init_exception_type_engine_failed:
		initEngineFailed();
		break;
	case init_exception_type_engine_invalid_param:
		initEngineInvalidParam();
		break;
	case init_exception_type_engine_invalid_dx_version:
		invalidDxVersion();
		break;
	case init_exception_type_prism_failed:
		prismInitFailed();
		break;
	case init_exception_type_engine_check_timeout:
		initEngineCheckTimeout();
		break;
	default:
		PLSMessageBox::information(NULL, QTStr("Alert"), QTStr("Commond error"));
		break;
	}
}

void PLSTestModule::initEngineCrash()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Init Engine Crash Test", ACTION_CLICK);

	HKEY hKey;
	LPCWSTR name = L"EnvCheckCrash";

	std::wstring strvalue = TEXT("true");
	int iLen = (strvalue.size() + 1) * sizeof(wchar_t);

	LPCWSTR strPath = L"SOFTWARE\\NAVER Corporation\\Prism Live Studio";
	if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_CURRENT_USER, strPath, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey)) {
		LONG lRtn = RegSetValueEx(hKey, name, 0, REG_SZ, (const BYTE *)strvalue.c_str(), iLen);
		if (lRtn == ERROR_SUCCESS) {
			PLS_INFO(MAIN_TEST_MODE, "Set EnvCheck crash success");
		} else {
			PLS_INFO(MAIN_TEST_MODE, "Set EnvCheck crash failed");
		}
		::RegCloseKey(hKey);
	}
	SystemParametersInfo(SPI_SETCURSORS, 0, NULL, SPIF_SENDCHANGE);
}

void PLSTestModule::initEngineFailed()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Init Engine Failed Test", ACTION_CLICK);
	try {
		struct obs_video_info ovi = {0};
		ovi.graphics_module = "libobs-d3d11.dll"; //needed only
		ovi.base_width = 1920;
		ovi.base_height = 1080;
		ovi.output_width = 1280;
		ovi.output_height = 720;
		ovi.output_format = VIDEO_FORMAT_NV12;
		ovi.colorspace = VIDEO_CS_601;
		ovi.range = VIDEO_RANGE_FULL;
		ovi.adapter = 0;
		ovi.gpu_conversion = true;
		ovi.scale_type = OBS_SCALE_BILINEAR;

		init_engine_test(&ovi, GS_E_TEST_INIT_FAILED);
	} catch (const char * /*error*/) {
	} catch (init_exception_code code) {
		QString codeStr = getInitExceptionAlertString(code);
		if (!codeStr.isEmpty()) {
			PLSErrorBox(nullptr, "%s", codeStr.toStdString().c_str());
		}
	}
}

void PLSTestModule::initEngineInvalidParam()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Init Engine Invalid Param Test", ACTION_CLICK);
	try {
		struct obs_video_info ovi = {0};
		ovi.graphics_module = "libobs-d3d11.dll"; //needed only
		ovi.base_width = 1920;
		ovi.base_height = 1080;
		ovi.output_width = 1280;
		ovi.output_height = 720;
		ovi.output_format = VIDEO_FORMAT_NV12;
		ovi.colorspace = VIDEO_CS_601;
		ovi.range = VIDEO_RANGE_FULL;
		ovi.adapter = 0;
		ovi.gpu_conversion = true;
		ovi.scale_type = OBS_SCALE_BILINEAR;

		init_engine_test(&ovi, GS_E_TEST_INIT_INVALID_PARAM);
	} catch (const char * /*error*/) {
	} catch (init_exception_code code) {
		QString codeStr = getInitExceptionAlertString(code);
		if (!codeStr.isEmpty()) {
			PLSErrorBox(nullptr, "%s", codeStr.toStdString().c_str());
		}
	}
}

void PLSTestModule::invalidDxVersion()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Init Engine Invalid DX Version Test", ACTION_CLICK);

	try {
		struct obs_video_info ovi = {0};
		ovi.graphics_module = "libobs-d3d11.dll"; //needed only
		ovi.base_width = 1920;
		ovi.base_height = 1080;
		ovi.output_width = 1280;
		ovi.output_height = 720;
		ovi.output_format = VIDEO_FORMAT_NV12;
		ovi.colorspace = VIDEO_CS_601;
		ovi.range = VIDEO_RANGE_FULL;
		ovi.adapter = 0;
		ovi.gpu_conversion = true;
		ovi.scale_type = OBS_SCALE_BILINEAR;

		init_engine_test(&ovi, GS_E_TEST_NOT_SUPPORT_VERSION);
	} catch (const char * /*error*/) {
	} catch (init_exception_code code) {
		QString codeStr = getInitExceptionAlertString(code);
		if (!codeStr.isEmpty()) {
			PLSErrorBox(nullptr, "%s", codeStr.toStdString().c_str());
		}
	}
}

void PLSTestModule::prismInitFailed()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Init PRISM Failed Test", ACTION_CLICK);

	try {
		initPrismForTest();
	} catch (const char * /*error*/) {
	} catch (init_exception_code code) {
		QString codeStr = getInitExceptionAlertString(code);
		if (!codeStr.isEmpty()) {
			PLSErrorBox(nullptr, "%s", codeStr.toStdString().c_str());
		}
	}
}

void PLSTestModule::initEngineCheckTimeout()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Init Engine Crash Test", ACTION_CLICK);

	HKEY hKey;
	LPCWSTR name = L"EnvCheckTimeout";

	std::wstring strvalue = TEXT("true");
	int iLen = (strvalue.size() + 1) * sizeof(wchar_t);

	LPCWSTR strPath = L"SOFTWARE\\NAVER Corporation\\Prism Live Studio";
	if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_CURRENT_USER, strPath, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey)) {
		LONG lRtn = RegSetValueEx(hKey, name, 0, REG_SZ, (const BYTE *)strvalue.c_str(), iLen);
		if (lRtn == ERROR_SUCCESS) {
			PLS_INFO(MAIN_TEST_MODE, "Set EnvCheck timeout success");
		} else {
			PLS_INFO(MAIN_TEST_MODE, "Set EnvCheck timeout failed");
		}
		::RegCloseKey(hKey);
	}
	SystemParametersInfo(SPI_SETCURSORS, 0, NULL, SPIF_SENDCHANGE);
}

void PLSTestModule::on_runtimeExceptionComboBox_currentIndexChanged(int index)
{
	ui->runtimeExceptionBtn->setText("Go");
	if (m_pTimerForAlert) {
		m_pTimerForAlert->stop();
	}
}

void PLSTestModule::textureOutofmemory()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Create Texture Out Of Memory Test", ACTION_CLICK);
	const int tex_width = 7680;
	const int tex_height = 4320;
	std::vector<gs_texture_t *> vecTexture;
	obs_enter_graphics();
	while (true) {
		gs_texture_t *tex = gs_texture_create(tex_width, tex_height, GS_RGBA, 1, NULL, GS_DYNAMIC);
		if (!tex) {
			PLS_INFO(MAINMENU_MODULE, "Create texture failed");
			break;
		}
		vecTexture.push_back(tex);
	}

	for (int i = 0; i < (int)vecTexture.size(); i++) {
		gs_texture_destroy(vecTexture[i]);
	}
	obs_leave_graphics();

	vecTexture.clear();
}

void PLSTestModule::showNotSupportDX11Notice()
{
	PLSBasic::Get()->SysTrayNotify(QTStr(BLACKLIST_GRAPHICS_CARD_LOWER_DX11), QSystemTrayIcon::Warning);
	PLS_WARN(MAIN_TEST_MODE, "Graphics card does not support DX11.");
}

void PLSTestModule::deviceRebuildFailed()
{
	struct obs_video_info ovi = {0};
	ovi.graphics_module = "libobs-d3d11.dll"; //needed only
	ovi.base_width = 1920;
	ovi.base_height = 1080;
	ovi.output_width = 1280;
	ovi.output_height = 720;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.colorspace = VIDEO_CS_601;
	ovi.range = VIDEO_RANGE_FULL;
	ovi.adapter = 0;
	ovi.gpu_conversion = true;
	ovi.scale_type = OBS_SCALE_BILINEAR;

	obs_engine_check_for_test(&ovi, GS_E_TEST_REBUILD_FAILED);
}

void PLSTestModule::cameraProcessCrash(CamTestModeType type)
{
	HKEY hKey;
	LPCWSTR name = L"CamCrashTest";

	LPCWSTR strPath = L"SOFTWARE\\NAVER Corporation\\Prism Live Studio";
	if (ERROR_SUCCESS == ::RegOpenKeyEx(HKEY_CURRENT_USER, strPath, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &hKey)) {
		LONG lRtn = RegSetValueEx(hKey, name, 0, REG_DWORD, (LPBYTE)&type, sizeof(CamTestModeType));
		if (lRtn == ERROR_SUCCESS) {
			PLS_INFO(MAIN_TEST_MODE, "Set session crash success");
		} else {
			PLS_INFO(MAIN_TEST_MODE, "Set session crash failed");
		}
		::RegCloseKey(hKey);
	}
	SystemParametersInfo(SPI_SETCURSORS, 0, NULL, SPIF_SENDCHANGE);
}

void PLSTestModule::subProcessDeviceRebuild()
{
	obs_data_t *beautyData = obs_data_create();
	obs_data_set_bool(beautyData, "SubProcessRebuild", true);
	obs_plugin_set_private_data("dshow_input", beautyData);
	obs_data_release(beautyData);
}

void PLSTestModule::mainEngineRebuildFailed()
{
	obs_enter_graphics();
	gs_set_device_rebuild_status(false);
	obs_leave_graphics();
}

void PLSTestModule::saveAudioIntoPCM()
{
	PLSDialogSavePCM dlg(this);
	dlg.exec();
}

void PLSTestModule::popupPrismAlertMessagebox()
{
	if (nullptr == m_pTimerForAlert) {
		m_pTimerForAlert = new QTimer(this);
		connect(m_pTimerForAlert, &QTimer::timeout, []() {
			QString title = "Test Alert";
			QString content;
			content.sprintf("Now it is \n%s\n Have a good day!", qUtf8Printable(QDateTime::currentDateTime().toString()));
			PLSBasic::Get()->FilterAlertMsg(title, content);
		});
	}

	if (m_pTimerForAlert->isActive()) {
		m_pTimerForAlert->stop();
		ui->runtimeExceptionBtn->setText("Go");
	} else {
		m_pTimerForAlert->setInterval(3000);
		m_pTimerForAlert->start();
		ui->runtimeExceptionBtn->setText("Stop");
	}
}

int getRuntimeExceptionIndex(QString name)
{
	int count = sizeof(runtime_exception_list) / sizeof(test_info);
	for (int i = 0; i < count; i++) {
		if (name == runtime_exception_list[i].strName) {
			return runtime_exception_list[i].index;
		}
	}
	return -1;
}

void PLSTestModule::on_runtimeExceptionBtn_clicked()
{
	QString sel_text = ui->runtimeExceptionComboBox->currentText();
	int index = getRuntimeExceptionIndex(sel_text);
	switch (index) {
	case runtime_exception_texture_outofmemory:
		textureOutofmemory();
		break;
	case runtime_exception_graphics_not_support_dx11:
		showNotSupportDX11Notice();
		break;
	case runtime_exception_device_rebuild_failed:
		deviceRebuildFailed();
		break;
	case runtime_exception_camera_process_crash:
		cameraProcessCrash(CamTestModeType::TMT_CRASH);
		break;
	case runtime_exception_camera_process_disappear:
		cameraProcessCrash(CamTestModeType::TMT_DISAPPEAR);
		break;
	case runtime_exception_camera_process_empty_path:
		cameraProcessCrash(CamTestModeType::TMT_EMPTY_PATH);
		break;
	case runtime_exception_ui_block_for_while:
		Sleep(20000);
		break;
	case runtime_save_pcm:
		saveAudioIntoPCM();
		break;
	case runtime_exception_enable_ref_alert:
		enable_popup_messagebox(true);
		break;
	case runtime_exception_disable_ref_alert:
		enable_popup_messagebox(false);
		break;
	case runtime_exception_subprocess_devicerebuild:
		subProcessDeviceRebuild();
		break;
	case runtime_exception_main_engine_rebuild_failed:
		mainEngineRebuildFailed();
		break;
	case runtime_exception_prism_alert_popup:
		popupPrismAlertMessagebox();
		break;
	default:
		PLSMessageBox::information(NULL, QTStr("Alert"), QTStr("Commond error"));
		break;
	}
}

int getCrashExceptionIndex(QString name)
{
	int count = sizeof(crash_exception_list) / sizeof(test_info);
	for (int i = 0; i < count; i++) {
		if (name == crash_exception_list[i].strName) {
			return crash_exception_list[i].index;
		}
	}
	return -1;
}

void PLSTestModule::on_crashExceptionBtn_clicked()
{
	QString sel_text = ui->crashExceptionComboBox->currentText();
	int index = getCrashExceptionIndex(sel_text);
	switch (index) {
	case crash_exception_vst_plugin:
		vstPluginCrash();
		break;
	case crash_exception_system_outofmemery:
		systemOutOfMemeryCrash();
		break;
	case crash_exception_grahpics_driver:
		graphicsDriverCrash();
		break;
	case crash_exception_video_device:
		videoDeviceCrash();
		break;
	case crash_exception_audio_device:
		audioDeviceCrash();
		break;
	case crash_exception_others:
		othersTypeCrash();
		break;
	default:
		PLSMessageBox::information(NULL, QTStr("Alert"), QTStr("Commond error"));
		break;
	}
}

void PLSTestModule::vstPluginCrash()
{
	PLS_UI_STEP(MAINMENU_MODULE, "VST Plugin Crash Test", ACTION_CLICK);
	obs_source_t *vstSource = obs_source_create("vst_filter", "vst crash test", nullptr, nullptr);
	if (vstSource) {
		obs_source_custom_test(vstSource);
		obs_source_release(vstSource);
	}
}

void PLSTestModule::systemOutOfMemeryCrash()
{
	PLS_UI_STEP(MAINMENU_MODULE, "System Out Of Memery Crash Test", ACTION_CLICK);
	DebugBreak();
}

void PLSTestModule::graphicsDriverCrash()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Graphics Driver Crash Test", ACTION_CLICK);
}

void PLSTestModule::videoDeviceCrash()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Video Device Crash Test", ACTION_CLICK);
}

void PLSTestModule::audioDeviceCrash()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Audio Device Crash Test", ACTION_CLICK);
}

void PLSTestModule::othersTypeCrash()
{
	PLS_UI_STEP(MAINMENU_MODULE, "Other Type Crash Test", ACTION_CLICK);
	QWidget *w = new QWidget();
	w->parent()->setParent(nullptr);
}

void PLSTestModule::on_pushButtonLookupString_clicked()
{
	auto key = ui->lineEditLookupString->text();
	if (key.isEmpty()) {
		return;
	}

	PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr(key.toUtf8().constData()));
}

void PLSTestModule::on_pushButton_clicked()
{
	auto str = pls_masking_person_info(ui->lineEdit->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_2_clicked()
{
	auto str = pls_masking_user_id_info(ui->lineEdit_2->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_3_clicked()
{
	auto str = pls_masking_email_info(ui->lineEdit_3->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_4_clicked()
{
	auto str = pls_masking_address_info(ui->lineEdit_4->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_5_clicked()
{
	auto str = pls_masking_ip_info(ui->lineEdit_5->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_6_clicked()
{
	auto str = pls_masking_name_info(ui->lineEdit_6->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_7_clicked()
{
	auto str = pls_masking_bank_card_info(ui->lineEdit_7->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_8_clicked()
{
	auto str = pls_masking_Region_info(ui->lineEdit_8->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_9_clicked()
{
	auto str = pls_masking_passport_info(ui->lineEdit_9->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_10_clicked()
{
	auto str = pls_masking_datetime_info(ui->lineEdit_10->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_11_clicked()
{
	auto str = pls_masking_identify_info(ui->lineEdit_11->text());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_12_clicked()
{
	auto str = pls_masking_int_info(ui->lineEdit_12->text().toInt());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}
void PLSTestModule::on_pushButton_13_clicked()
{
	auto str = pls_masking_double_info(ui->lineEdit_13->text().toDouble());
	PLSAlertView::warning(this, QTStr("Alert.Title"), str);
}

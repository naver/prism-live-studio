#include "PLSOnBoardingDlg.h"
#include "ui_PLSOnBoardingDlg.h"
#include "PLSAlertView.h"
#include "PLSBasic.h"
#include "PLSErrorHandler.h"
#include "libutils-api.h"
#include "liblog.h"
#include "PLSUIApp.h"
#include <QDesktopServices>

static constexpr auto k_lens_startByPrism = "startByPrism";
static constexpr auto k_lens_closeByPrism = "closeByPrism";
static constexpr auto k_prism_isDisplayedLensQuit = "isDisplayedLensQuit";
static constexpr auto k_prism_isDisplayedOnBoard = "isDisplayedOnBoard";

static constexpr auto k_prism_lens_start_close_min_version = "2.0.1";

PLSOnBoardingDlg::PLSOnBoardingDlg(QWidget *parent) : PLSDialogView(parent), ui(new Ui::PLSOnBoardingDlg)
{
	setupUi(ui);
	pls_add_css(this, {"PLSOnBoardingDlg"});

	setWindowTitle(tr("main.onBoarding.title"));

	setFixedSize(970, pls_is_os_sys_macos() ? (676 - PLS_TITLE_BAR_HEIGHT) : 676);
	setResizeEnabled(false);

	ui->pushButton_link->setLabelText(tr("main.onBoarding.extra.link.button"), true, false);
	ui->pushButton_link->modifyContentCenter();
	pls_flush_style(ui->label_setting_bg, "OS", pls_is_os_sys_macos() ? "mac" : "win");
}

PLSOnBoardingDlg::~PLSOnBoardingDlg()
{
	delete ui;
}

void PLSOnBoardingDlg::on_pushButton_link_clicked()
{
	pls_async_invoke([]() { QDesktopServices::openUrl(QUrl(g_plsOnBoardingUrl)); });
}
void PLSOnBoardingDlg::on_pushButton_start_clicked()
{
	pls::lens::startLens();
}
namespace pls::lens {

bool isNotCloseLensType()
{
	auto type = PLSBasic::instance()->restartAppType();
	PLS_INFO(MAIN_CAM_STUDIO, "prism lens together to get restart type:%i.", static_cast<int>(type));
	switch (type) {
	case RestartAppType::ChangeLang:
	case RestartAppType::Logout:
	case RestartAppType::Update:
		return true;
	default:
		break;
	}
	return false;
}

QString getLensVersion()
{
	QString lensVersion;
#if defined(Q_OS_WIN)
	QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PRISM Lens", QSettings::NativeFormat);
	lensVersion = settings.value("DisplayVersion", "").toString();
#elif defined(Q_OS_MACOS)
	lensVersion = pls_libutil_api_mac::pls_get_app_version_by_identifier("com.prismlive.camstudio");
#endif
	return lensVersion;
}
bool isLensVersionLessThanStartCloseVersion()
{
	auto lensVersion = getLensVersion();
	int ret = pls_compare_version(lensVersion, k_prism_lens_start_close_min_version);
	if (ret == -1) {
		return true;
	}
	return false;
}

bool isNeedShowQuitAlert()
{
	if (pls_get_qsetting_value(k_prism_isDisplayedLensQuit, false).toBool() || //
	    isLensVersionLessThanStartCloseVersion() ||                            //
	    isNotCloseLensType() ||                                                //
	    !getIsLensRunning()) {
		return false;
	}
	QVariant closeByPrismVariant = pls_get_qsetting_value(pls_product_type_t::Lens, k_lens_closeByPrism, true);
	// null or true ---> is mean need close by prism.
	return closeByPrismVariant.toBool();
}

bool isNeedStartLensWhenStartPrism()
{

	if (isLensVersionLessThanStartCloseVersion()) {
		return false;
	}
	QVariant startByPrismVariant = pls_get_qsetting_value(pls_product_type_t::Lens, k_lens_startByPrism, true);
	// null or true ---> is mean need start by prism.
	return startByPrismVariant.toBool();
}

bool isNeedShowOnBoardingDialog()
{
	if (pls_get_qsetting_value(k_prism_isDisplayedOnBoard, false).toBool()) {
		return false;
	}
	return isNeedStartLensWhenStartPrism();
}

void showLensQuitAlert(QWidget *parent)
{
	if (!isNeedShowQuitAlert()) {
		return;
	}
	PLS_INFO(MAIN_CAM_STUDIO, "prism lens together to show quit alert.");
	pls_set_qsetting_value(k_prism_isDisplayedLensQuit, true);
	auto alert = PLSAlertView(parent, PLSAlertView::Icon::Information, QObject::tr("Alert.Title"), QObject::tr("main.lens.quit.together.alert"), "", {QDialogButtonBox::StandardButton::Ok},
				  QDialogButtonBox::StandardButton::Ok, {{"disableExtralLink", true}});
	QObject::connect(&alert, &PLSAlertView::contentlinkActivated, pls_get_main_view(), [](const QString &link) { pls_request_lens_show_settings(); });
	alert.exec();
}
void startLensIfNeed()
{
	printBaseLog();
	if (!isNeedStartLensWhenStartPrism()) {
		return;
	}
	PLS_INFO(MAIN_CAM_STUDIO, "prism lens together to start lens.");
	pls_async_invoke([]() { startLens(); });
}
void startLens()
{
	PLSUiApp::instance()->openApp({"--display_control=top"}, PLSBasic::instance(), [](pls_app_state_t state) {
		switch (state) {
		case pls_app_state_t::AppNotInstalled:
			PLS_INFO(MAIN_CAM_STUDIO, "prism lens together to start lens falid, not install lens.");
			return false;
		case pls_app_state_t::OpenProcessOk:
			return true;
		case pls_app_state_t::OpenProcessFailed:
			PLSBasic::instance()->setAlertParentWithBanner([](QWidget *parent) {
				PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_MAIN_START_LENS_FAILED, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("PLSOnBoardingDlg::startLens"),
								      parent);
			});
			PLS_ERROR(MAIN_CAM_STUDIO, "auto start create cam process failed.");
			return false;
		case pls_app_state_t::ProcessStarted:
			PLS_INFO(MAIN_CAM_STUDIO, "auto start call open cam studio success.");
			return false;
		default:
			return false;
		}
	});
}

void showOnBoardingDialogIfNeed(QWidget *parent)
{
	if (!getThisShowOnBoardingDialog()) {
		return;
	}
	PLS_INFO(MAIN_CAM_STUDIO, "prism lens together to show onboarding dialog.");
	pls_set_qsetting_value(k_prism_isDisplayedOnBoard, true);
	PLSOnBoardingDlg *dlg = new PLSOnBoardingDlg(parent);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->show();
}

void closeLensIfNeed()
{
	auto CheckFunc = [](QByteArray &reason) -> bool {
		if (!pls_get_qsetting_value(pls_product_type_t::Lens, k_lens_closeByPrism, true).toBool()) {
			reason = "closeByPrism";
			return false;
		}
		if (isLensVersionLessThanStartCloseVersion()) {
			reason = "version check";
			return false;
		}
		if (!getIsLensRunning()) {
			reason = "lens running";
			return false;
		}
		if (isNotCloseLensType()) {
			reason = "stop type";
			return false;
		}
		return true;
	};
	QByteArray faildReason;
	if (!CheckFunc(faildReason)) {
		PLS_INFO(MAIN_CAM_STUDIO, "prism lens together to request lens exit failed. reason:%s.", faildReason.constData());
		return;
	}

	PLS_INFO(MAIN_CAM_STUDIO, "prism lens together to request lens exit.");
	pls_request_lens_exit();
}
void printBaseLog()
{
	auto closeByPrism = pls_get_qsetting_value(pls_product_type_t::Lens, k_lens_closeByPrism, true).toBool();
	auto startByPrism = pls_get_qsetting_value(pls_product_type_t::Lens, k_lens_startByPrism, true).toBool();
	auto isDisplayedLensQuit = pls_get_qsetting_value(k_prism_isDisplayedLensQuit, false).toBool();
	auto isDisplayedOnBoard = pls_get_qsetting_value(k_prism_isDisplayedOnBoard, false).toBool();

	QString log = QString("prism lens together to print lens auto start base log.");
	log = log.append("\nlensVersion: ").append(getLensVersion());
	log = log.append("\ncloseByPrism: ").append(pls_bool_2_string(closeByPrism));
	log = log.append("\nstartByPrism: ").append(pls_bool_2_string(startByPrism));
	log = log.append("\nisDisplayedLensQuit: ").append(pls_bool_2_string(isDisplayedLensQuit));
	log = log.append("\nisDisplayedOnBoard: ").append(pls_bool_2_string(isDisplayedOnBoard));

	PLS_INFO(MAIN_CAM_STUDIO, log.toUtf8().constData());
}

bool getThisShowOnBoardingDialog()
{
	static std::optional<bool> s_isThisShowOnBoarding = std::nullopt;
	if (!s_isThisShowOnBoarding.has_value()) {
		s_isThisShowOnBoarding = isNeedShowOnBoardingDialog();
	}
	return s_isThisShowOnBoarding.value();
}

bool getIsLensRunning()
{
	int pid = 0;
	bool processRun = false;
#if defined(Q_OS_WIN)
	processRun = pls_is_process_running(L"PRISMLens.exe", pid);
#elif defined(Q_OS_MACOS)
	processRun = pls_is_process_running("PRISMLens", pid);
#endif
	PLS_INFO(MAIN_CAM_STUDIO, "prism lens together to get lens is running:%s", pls_bool_2_string(processRun));
	return processRun;
}

}

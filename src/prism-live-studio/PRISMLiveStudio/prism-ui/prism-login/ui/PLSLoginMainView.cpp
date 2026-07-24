#include "qglobal.h"
#if defined(Q_OS_WIN)
#include <Windows.h>
#elif defined(Q_OS_MACOS)
#include <libutils-api-mac.h>
#endif
#include "PLSLoginMainView.h"
#include "ui_PLSLoginMainView.h"
#include "libutils-api.h"
#include "PLSCommonFunc.h"
#include "PLSLoginDataHandler.h"
#include "PLSIPCHandler.h"
#include "prism-version.h"
#include <qthread.h>
#include <QVariantHash>
#include "PLSAlertView.h"
#include "liblog.h"
#include <PLSComboBox.h>
#include <QSpinBox>
#include <QRadioButton>
#include <PLSCheckBox.h>
#include <qevent.h>
#include <qlistwidget.h>
#include "pls-common-define.hpp"
#include "PLSApp.h"
#include "pls-shared-values.h"
#include "frontend-api.h"
#include "PLSNoticePopupDialog.hpp"
#include "PLSNoticeUpdateCoordinator.hpp"
#include "PLSNoticeUpdateRepository.hpp"
#include "PLSNoticeUpdateTypes.hpp"
#include "login-user-info.hpp"
#include "pls-common-define.hpp"
#include <PLSBasic.h>
#include <QDialog>

QPointer<PLSLoginMainView> PLSLoginMainView::g_launcerMainView = nullptr;
int PLSLoginMainView::closeResultValue = -1;

PLSLoginMainView *PLSLoginMainView::instance()
{
	if (nullptr == g_launcerMainView) {
		g_launcerMainView = pls_new<PLSLoginMainView>();
	}
	return g_launcerMainView;
}

PLSLoginMainView::PLSLoginMainView(QWidget *parent) : PLSDialogView(parent)
{
	pls_add_css(this, {"PLSLoginMainView", "PrismLoginView", "PLSEidt"});
	ui = pls_new<Ui::PLSLoginMainView>();
	setWindowState(Qt::WindowActive);
	setupUi(ui);
	setWindowTitle(QString());
	setResizeEnabled(false);
#if defined(Q_OS_MACOS)
	setHasMinButton(false);
#elif defined(Q_OS_WIN)
	setHasMinButton(true);
#endif
	setWindowIcon(QIcon(":/resource/images/logo/PRISMLiveStudio.ico"));
	setAttribute(Qt::WA_DeleteOnClose);
	addMacTopMargin(34);

	auto children = findChildren<QLineEdit *>();
	for (auto edit : children) {
		pls_uistep_v2_enable(edit, false);
	}
}

PLSLoginMainView::~PLSLoginMainView()
{
	pls_notify_close_modal_views_with_parent(this);

	PLS_INFO(LAUNCHER_INIT, "PLSLoginMainView delete");
	pls_delete(ui, nullptr);
}

QWidget *PLSLoginMainView::changeView(pls_window_type windowType)
{
	m_currentType = windowType;
	QWidget *currentWidget = nullptr;
	switch (windowType) {
	case pls_window_type::PLS_UPDATING_VIEW:
		setWindowTitle(QStringLiteral("PRISMLiveStudioUpdate"));
		ui->progressView->updateView(QString(tr("launcher.update.download")), windowType);
		currentWidget = ui->progressView;
		break;
	case pls_window_type::PLS_LOGIN_VIEW:
		PLSLoginDataHandler::instance()->resetLoginResultCommitState();
		currentWidget = ui->loginView;
		setWindowTitle(QTStr("login.login_cap"));
		break;
	default:
		break;
	}
	if (currentWidget) {
		ui->stackedWidget->setCurrentWidget(currentWidget);
	}
	pls_async_call_mt(this, [this]() { activateWindow(); });

	return currentWidget;
}
bool installUpdate(const QString &filePath)
{
	PLS_INFO(UPDATE_MODULE, "start install new app");

	std::wstring filePathW = filePath.toStdWString();
#if defined(Q_OS_WIN)
	SHELLEXECUTEINFO sei = {};
	sei.cbSize = sizeof(sei);
	sei.lpFile = filePathW.c_str();
	sei.nShow = SW_SHOWNORMAL;
	bool isSuccess = false;
	if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
		if (ShellExecuteEx(&sei)) {
			PLS_INFO(UPDATE_MODULE, "successfully to call ShellExecuteEx() for new packet");
			isSuccess = true;
		} else {
			PLS_ERROR(UPDATE_MODULE, "failed to call ShellExecuteEx() for new packet ERROR: %lu", GetLastError());
		}
		CoUninitialize();
	} else {
		PLS_ERROR(APP_MODULE, "failed to call CoInitializeEx() for new packet ERROR");
	}
	return isSuccess;
#elif defined(Q_OS_MACOS)
	QString prismBundlePath = pls_get_bundle_dir();
	return pls_install_mac_package(filePath, prismBundlePath, GlobalVars::prismSession, pls_get_gcc().toStdString(), PLS_VERSION);
#endif
}

void installUpdateAndCloseLauncher()
{
#if defined(Q_OS_WIN)
	if (!installUpdate(PLSLoginDataHandler::instance()->getInstallPackagePath())) {
		PLS_INFO(LAUNCHER_CHECK, "intall new app failed");
		PLSLoginMainView::instance()->done(QDialog::Rejected);
	} else {
		PLS_INFO(LAUNCHER_CHECK, "intall new app success");
		PLSLoginMainView::instance()->done(pls_launcher_const::APP_INSTALL_ACCEPT);
	}
#elif defined(Q_OS_MACOS)
	PLSLoginMainView::instance()->done(pls_launcher_const::APP_INSTALL_ACCEPT);
	QObject::connect(qApp, &QApplication::destroyed, []() {
		bool result = installUpdate(PLSLoginDataHandler::instance()->getInstallPackagePath());
		PLS_INFO("Process", "mac update status: intall new app result is %s", result ? "true" : "false");
	});
#endif
}

void PLSLoginMainView::startdownloadNewInstallPackage(const QString &fileUrl, const QString &gcc)
{
	PLSLoginDataHandler::instance()->startDownloadNewPackage(
		[this](qint64 currentValue, qint64 totalValue, PLSUpdateDownloadState updateDownloadState) {
			pls_async_call_mt(this, [updateDownloadState, this, currentValue, totalValue] {
				pls_check_app_exiting();
				ui->progressView->updateProgress((int)currentValue, (int)totalValue);
				if (updateDownloadState == PLSUpdateDownloadState::PLSUpdateDownloadSuccess) {
					QMetaObject::invokeMethod(
						qApp,
						[this]() {
							pls_check_app_exiting();
							installUpdateAndCloseLauncher();
						},
						Qt::QueuedConnection);

				} else if (updateDownloadState == PLSUpdateDownloadState::PLSUpdateDownloadFailed) {
					pls_check_app_exiting();
					prismUpdateFailedHandler();
				}
			});
		},
		fileUrl, gcc);
}

bool PLSLoginMainView::updateTipHandler()
{
	if (!g_launcerMainView) {
		return true;
	}
	AppUpdateResult updateResult = PLSLoginDataHandler::instance()->getUpdateResult();
	if (updateResult == AppUpdateResult::AppHasUpdate) {
		bool isForceUpdate = PLSLoginDataHandler::instance()->isForcePrismAppUpdate();
		QString updateVersion = PLSLoginDataHandler::instance()->getUpdateVersion();
		QString updateInfoUrl = PLSLoginDataHandler::instance()->getUpdateInfoUrl();
		QString installFileUrl = PLSLoginDataHandler::instance()->getInstallFileUrl();
		PLSNoticePopupDialog updateTipView(updateInfoUrl, true, isForceUpdate, QString());
		const int dlg_result = updateTipView.exec();
		if (dlg_result == QDialog::Accepted) {
			changeView(pls_window_type::PLS_UPDATING_VIEW);
			startdownloadNewInstallPackage(installFileUrl, pls_get_gcc());
			return true;
		}
		if (isForceUpdate) {
			return false;
		}
		PLSLoginFunc::saveUpdateInfo({{common::UPDATE_NEXT_VERSION_INFO, updateVersion}});
		return true;
	}
	changeView(pls_window_type::PLS_LOGIN_VIEW);
	return true;
}

void PLSLoginMainView::showNoticeView()
{
	auto list = pls_get_new_notice_from_cache();
	if (!list.isEmpty()) {
		PLSNoticeUpdateCoordinator::instance()->showPopupQueueDirect(list, this, nullptr);
	} else {
		changeView(pls_window_type::PLS_LOGIN_VIEW);
	}
}

void PLSLoginMainView::prismProgressChanged(const QString &uiStr, int progress)
{
	PLS_INFO(LAUNCHER_STARTUP, "prismProgressChanged entered: %i", progress);
	if (m_currentType != pls_window_type::PLS_APP_RUNNING) {
		return;
	}

	m_appRunProgress = progress;
	ui->progressView->updateProgressAndText(uiStr, progress, 100);

	if (progress == 100) {
		//close alert view
		pls_notify_close_modal_views();

		this->hide();
		this->close();
		this->deleteLater();
	}
}

void PLSLoginMainView::prismUpdateFailedHandler()
{
	auto restartType = pls_cmdline_get_uint32_arg(pls_cmdline_args(), shared_values::k_launcher_command_type);
	if (PLSLoginDataHandler::instance()->isNeedLogin() && restartType != static_cast<int>(RestartAppType::Update)) {
		changeView(pls_window_type::PLS_LOGIN_VIEW);
	} else {
		done(QDialog::Rejected);
	}
}

void PLSLoginMainView::closeEvent(QCloseEvent *event)
{
	pls_notify_close_modal_views_with_parent(this);
	PLSDialogView::closeEvent(event);

#ifdef Q_OS_MACOS
	PLSLoginMainView::closeResultValue = pls_libutil_api_mac::pls_get_is_app_quitting_by_dock() ? pls_launcher_const::CLOSE_APP_FROM_LOGIN : -1;
#endif
}

bool PLSMouseEnterEventFilter::eventFilter(QObject *obj, QEvent *event)
{
	if (!obj->isWidgetType() || obj->property("notShowHandCursor").toBool()) {
		return QObject::eventFilter(obj, event);
	}
	if (event->type() == QEvent::MouseButtonPress) {
		qDebug() << obj->metaObject()->className();
	}

	auto widget = static_cast<QWidget *>(obj);
	if (obj->metaObject()->inherits(&QComboBox::staticMetaObject) || obj->metaObject()->inherits(&QPushButton::staticMetaObject) || obj->metaObject()->inherits(&QToolButton::staticMetaObject) ||
	    obj->metaObject()->inherits(&QCheckBox::staticMetaObject) || obj->metaObject()->inherits(&QRadioButton::staticMetaObject) || obj->metaObject()->inherits(&QSlider::staticMetaObject) ||
	    obj->metaObject()->inherits(&QMenu::staticMetaObject) || obj->metaObject()->inherits(&QListWidget::staticMetaObject) || obj->metaObject()->inherits(&PLSComboBox::staticMetaObject) ||
	    obj->metaObject()->inherits(&PLSComboBoxListView::staticMetaObject) || obj->metaObject()->inherits(&QSpinBox::staticMetaObject) || obj->property("showHandCursor").toBool()) {
		if (event->type() == QEvent::Enter) {
			widget->setCursor(Qt::PointingHandCursor);
		}
	}
	return QObject::eventFilter(obj, event);
}

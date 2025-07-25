#include "PLSRemoteControlConfigView.h"
#include "./ui_PLSRemoteControlConfigView.h"

#include "pls-shared-functions.h"

#include <libutils-api.h>

#include <QResizeEvent>
#include <qhostaddress.h>
#include <qpointer.h>
#include <qdesktopservices.h>
#include <obs-ui/obs-app.hpp>

#include "PLSBasic.h"

static void remote_control_socket_initialized(void *data, calldata_t *calldata)
{
	auto port = (uint64_t)calldata_int(calldata, "port");
	if (port == 0 || !data)
		return;

	auto context = static_cast<const PLSRemoteControlConfigView *>(data);
	QPointer<const PLSRemoteControlConfigView> qContext(context);
	pls_async_call_mt([qContext]() {
		if (qContext == nullptr || qContext.isNull())
			return;

		qContext->load();
	});
}

PLSRemoteControlConfigView::PLSRemoteControlConfigView(QWidget *parent) : PLSDialogView(DialogInfo{550, 600, 0, ConfigId::RemoteControlConfig}, parent)
{
	ui = pls_new<Ui::PLSRemoteControlConfigView>();
	ui->setupUi(content());

	// history view
	pls_add_css(this, {"PLSRemoteControlConfigView"});
	m_historyView = pls_new<PLSRemoteControlHistoryView>(content());

	setHasMaxResButton(true);
#if defined(Q_OS_MACOS)
	setWindowTitle(tr("sidebar.remotecontrol.mac.title"));
#else
	setWindowTitle("");
#endif
	setTitleCustomWidth(400);

	auto tl = pls_new<QHBoxLayout>(titleLabel());
	tl->setContentsMargins(QMargins(0, 0, 0, 0));
	tl->setSpacing(4);

	QLabel *titleText = pls_new<QLabel>(tr("sidebar.remotecontrol.tooltip"));
	titleText->setObjectName("titleText");
	tl->addWidget(titleText);

	auto titleIconLayout = pls_new<QVBoxLayout>(titleLabel());
	titleIconLayout->setSpacing(0);
	titleIconLayout->setContentsMargins(0, 1, 0, 0);
	titleIconLayout->setObjectName(QString::fromUtf8("titleIconLayout"));
	QLabel *titleIcon = pls_new<QLabel>();
	titleIcon->setObjectName("titleIcon");
	titleIconLayout->addWidget(titleIcon);

	tl->addLayout(titleIconLayout);
	tl->addStretch(1);

	ui->titleLabel->setText(tr("remotecontrol.connect.guide.title"));
	ui->hintLabel->setText(tr("remotecontrol.connect.guide.text"));
	ui->historyDescLabel->setText(tr("remotecontrol.history.guide.text"));
	ui->historyBtn->setText(tr("remotecontrol.history.button.text"));

	connect(ui->googleBtn, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QString("https://play.google.com/store/apps/details?id=com.prism.live")); });
	connect(ui->appleBtn, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QString("https://apps.apple.com/app/id1319056339")); });
	connect(ui->historyBtn, &QPushButton::clicked, this, [] {
		QString logfile;
		pls_get_remote_control_log_file(logfile);
		pls_show_in_graphical_shell(logfile);
	});

	QMetaObject::invokeMethod(qApp, [this]() { this->load(); }, Qt::QueuedConnection);
}

PLSRemoteControlConfigView::~PLSRemoteControlConfigView() noexcept(true)
{
	if (_registered) {
		signal_handler_disconnect(obs_get_signal_handler(), "remote_control_socket_initialized", remote_control_socket_initialized, this);
		pls_frontend_remove_event_callback(PLSRemoteControlConfigView::pls_frontend_event_received, this);
	}
}

void PLSRemoteControlConfigView::resizeEvent(QResizeEvent *event)
{
	PLSDialogView::resizeEvent(event);

	m_historyView->setGeometry(ui->contentFrame->geometry());
}

void PLSRemoteControlConfigView::showEvent(QShowEvent *event)
{
	QString peerName;
	bool connected = false;
	pls_get_remote_control_client_info(peerName, connected);
	m_historyView->setName(peerName);
	m_historyView->setVisible(connected);
	m_historyView->setGeometry(ui->contentFrame->geometry());

	if (!_registered) {
		signal_handler_connect(obs_get_signal_handler(), "remote_control_socket_initialized", remote_control_socket_initialized, this);
		pls_frontend_add_event_callback(PLSRemoteControlConfigView::pls_frontend_event_received, this);
		_registered = true;
	}

	PLSBasic::instance()->getMainView()->updateSideBarButtonStyle(ConfigId::RemoteControlConfig, true);
	pls_window_right_margin_fit(this);
	PLSDialogView::showEvent(event);
	load();

	if (!_registeredNetworkState) {
		_registeredNetworkState = true;
		QPointer<PLSRemoteControlConfigView> view = this;
		pls_network_state_monitor([view](bool) {
			pls_async_call_mt([view]() {
				if (view && view->isVisible())
					view->load();
			});
		});
	}
}

void PLSRemoteControlConfigView::hideEvent(QHideEvent *event)
{
	PLSBasic::instance()->getMainView()->updateSideBarButtonStyle(ConfigId::RemoteControlConfig, false);
	PLSDialogView::hideEvent(event);
}

void PLSRemoteControlConfigView::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}

void PLSRemoteControlConfigView::load() const
{
	quint16 port = 0;
	pls_get_remote_control_server_info(port);
	if (port == 0)
		return;

	QJsonObject info;
	QJsonObject ip_port;
	QJsonArray array;
	auto serviceIPs = pls_get_valid_hosts();
	for (auto addr : serviceIPs) {
		ip_port["host"] = addr.toString();
		ip_port["port"] = port;
		array.append(ip_port);
	}
	info["endInfos"] = array;
	info["version"] = 1;
	info["type"] = "RemoteControl";
	const int n = 3;
	QPixmap logoPix(pls_shared_paint_svg(QStringLiteral(":/resource/images/remote-control/img-logo.svg"), QSize(36 * n, 36 * n)));
	auto image = pls_generate_qr_image(info, 190 * n, 8 * n, logoPix);
	if (image.isNull()) {
		assert(false);
		return;
	}

	auto pix = QPixmap::fromImage(image);
	ui->qrImageLabel->setPixmap(pix);
}

void PLSRemoteControlConfigView::pls_frontend_event_received(pls_frontend_event event, const QVariantList &params, void *context)
{
	auto view = (PLSRemoteControlConfigView *)context;

	if (view && event == pls_frontend_event::PLS_FRONTEND_EVENT_REMOTE_CONTROL_CONNECTION_CHANGED && params.size() > 1) {
		auto name = params.at(0).toString();
		auto connected = params.at(1).toBool();
		QPointer<PLSRemoteControlConfigView> pView(view);
		QMetaObject::invokeMethod(
			qApp,
			[name, connected, pView]() {
				if (!pView)
					return;

				pView->m_historyView->setName(name);
				pView->m_historyView->setVisible(connected);
				pView->m_historyView->setGeometry(pView->ui->contentFrame->geometry());
			},
			Qt::QueuedConnection);
	}
}

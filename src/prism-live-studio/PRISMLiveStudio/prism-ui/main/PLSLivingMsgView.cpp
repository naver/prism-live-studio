#ifdef Q_OS_WIN
#include <Windows.h>
#else
#endif
#include "PLSLivingMsgView.hpp"
#include "ui_PLSLivingMsgView.h"
#include <QDateTime>
#include "PLSLivingMsgItem.hpp"
#include <obs-app.hpp>
#include "PLSMainView.hpp"
#include "PLSToastButton.hpp"
#include <QResizeEvent>
#include "libutils-api.h"
#include "PLSBasic.h"

constexpr auto TIMEOFFSET = 1000;

PLSLivingMsgView::PLSLivingMsgView(DialogInfo info, QWidget *parent) : PLSSideBarDialogView(info, parent)
{
	pls_set_css(this, {"PLSLivingMsgView"});
	ui = pls_new<Ui::PLSLivingMsgView>();

	setupUi(ui);
	setHasMinButton(true);
	setHasMaxResButton(true);
	setCaptionButtonMargin(9);
	ui->stackedWidget->setCurrentIndex(0);
	ui->livingMsgMainLayout->setAlignment(Qt::AlignTop);
	setWindowTitle(tr("Alert.Title"));
	connect(&m_t, &QTimer::timeout, [this]() {
		initializeView();
		m_currentTime = QDateTime::currentMSecsSinceEpoch();
		foreach(auto var, m_msgItems)
		{
			var->updateTimeView(m_currentTime);
		}
	});
	m_t.start(TIMEOFFSET);
#if defined(Q_OS_MACOS)
	setMinimumSize(300, 360);
#elif defined(Q_OS_WIN)
	setMinimumSize(300, 400);
#endif
}

PLSLivingMsgView::~PLSLivingMsgView()
{
	m_t.stop();
	pls_delete(ui);
}

void PLSLivingMsgView::initializeView()
{
	if (m_msgItems.isEmpty()) {
		ui->stackedWidget->setCurrentIndex(0);
	} else {
		ui->stackedWidget->setCurrentIndex(1);
	}
}

void PLSLivingMsgView::clearMsgView()
{
	for (auto item : m_msgItems) {
		item->setParent(nullptr);
		pls_delete(item);
	}
	m_msgItems.clear();
	initializeView();
	update();
}

void PLSLivingMsgView::setShow(bool isVisable)
{
	setVisible(isVisable);

	if (config_get_bool(App()->GlobalConfig(), "LivingMsgView", "MaximizedState")) {
		showMaximized();
	}
}

void PLSLivingMsgView::showEvent(QShowEvent *event)
{
	PLSBasic::instance()->getMainView()->updateSideBarButtonStyle(ConfigId::LivingMsgView, true);
	pls_window_right_margin_fit(this);
	PLSSideBarDialogView::showEvent(event);
}

void PLSLivingMsgView::hideEvent(QHideEvent *event)
{
	PLSBasic::instance()->getMainView()->updateSideBarButtonStyle(ConfigId::LivingMsgView, false);
	emit hideSignal();
	PLSSideBarDialogView::hideEvent(event);
}

void PLSLivingMsgView::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}

void PLSLivingMsgView::addMsgItem(const QString &msgInfo, const long long time, pls_toast_info_type type)
{
	PLSLivingMsgItem *item = pls_new<PLSLivingMsgItem>(msgInfo, time, type);
	item->setMsgInfo(msgInfo);
	m_msgItems.push_back(item);
	ui->livingMsgMainLayout->insertWidget(0, item);
}

QString PLSLivingMsgView::getInfoWithUrl(const QString &str, const QString &url, const QString &replaceStr) const
{
	QString templateStr = QString("<a href=\"%1\"><span style=\"color:#effc35;\">%2</span></a>").arg(url).arg(replaceStr);
	QString info(str);
	return info.replace(url, templateStr, Qt::CaseInsensitive).replace('\n', "<br/>");
}

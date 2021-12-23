#include <Windows.h>
#include "PLSLivingMsgView.hpp"
#include "ui_PLSLivingMsgView.h"
#include <QDateTime>
#include "PLSLivingMsgItem.hpp"
#include <pls-app.hpp>
#include "main-view.hpp"
#include "PLSToastButton.hpp"
#include <QResizeEvent>

#define TIMEOFFSET 1000

PLSLivingMsgView::PLSLivingMsgView(DialogInfo info, QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView(info, parent, dpiHelper), ui(new Ui::PLSLivingMsgView), m_currentTime(0)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSLivingMsgView});
	notifyFirstShow([=]() { this->InitGeometry(true); });

	ui->setupUi(this->content());
	setHasMaxResButton(true);
	setCaptionButtonMargin(9);
	ui->stackedWidget->setCurrentIndex(0);
	ui->livingMsgMainLayout->setAlignment(Qt::AlignTop);
	setWindowTitle(tr("Alert.Title"));
	setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

	connect(&m_t, &QTimer::timeout, [&]() {
		initializeView();
		m_currentTime = QDateTime::currentMSecsSinceEpoch();
		foreach(auto var, m_msgItems) { var->updateTimeView(m_currentTime); }
	});
	m_t.start(TIMEOFFSET);
}

PLSLivingMsgView::~PLSLivingMsgView()
{
	if (!isMaxState) {
		config_set_string(App()->GlobalConfig(), "LivingMsgView", "geometry", saveGeometry().toBase64().constData());
	}
	m_t.stop();
	delete ui;
}

void PLSLivingMsgView::initializeView()
{
	if (0 == m_msgItems.size()) {
		ui->stackedWidget->setCurrentIndex(0);
	} else {
		ui->stackedWidget->setCurrentIndex(1);
	}
}

void PLSLivingMsgView::setGeometryOfNormal(const QRect &geometry)
{
	this->geometryOfNormal = geometry;
}

void PLSLivingMsgView::clearMsgView()
{
	for (auto item : m_msgItems) {
		item->setParent(nullptr);
		delete item;
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
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::LivingMsgView, true);
	pls_window_right_margin_fit(this);
	PLSDialogView::showEvent(event);
}

void PLSLivingMsgView::hideEvent(QHideEvent *event)
{
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::LivingMsgView, false);
	emit hideSignal();
	PLSDialogView::hideEvent(event);
}

void PLSLivingMsgView::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}

void PLSLivingMsgView::resizeEvent(QResizeEvent *event)
{
	PLSDialogView::resizeEvent(event);
}

void PLSLivingMsgView::addMsgItem(const QString &msgInfo, const long long time, pls_toast_info_type type)
{
	PLSLivingMsgItem *item = new PLSLivingMsgItem(msgInfo, time, type);
	item->setMsgInfo(msgInfo);
	m_msgItems.push_back(item);
	ui->livingMsgMainLayout->insertWidget(0, item);
}

QString PLSLivingMsgView::getInfoWithUrl(const QString &str, const QString &url, const QString &replaceStr)
{
	QString templateStr = QString("<a href=\"%1\"><span style=\"color:#effc35;\">%2</span></a>").arg(url).arg(replaceStr);
	QString info(str);
	return info.replace(url, templateStr, Qt::CaseInsensitive).replace('\n', "<br/>");
}

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

PLSLivingMsgView::PLSLivingMsgView(QWidget *parent) : PLSDialogView(parent), ui(new Ui::PLSLivingMsgView), m_currentTime(0)
{
	ui->setupUi(this->content());
	setHasMaxResButton(true);
	setCaptionButtonMargin(9);
	ui->stackedWidget->setCurrentIndex(0);
	ui->livingMsgMainLayout->setAlignment(Qt::AlignTop);
	setWindowTitle(tr("Live.toast.title"));
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
	config_set_string(App()->GlobalConfig(), "LivingMsgView", "geometry", saveGeometry().toBase64().constData());
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

void PLSLivingMsgView::showEvent(QShowEvent *event)
{
	qobject_cast<PLSMainView *>(pls_get_main_view())->getToastButton()->updateIconStyle(true);
	config_set_bool(App()->GlobalConfig(), "LivingMsgView", "showMode", isVisible());
	config_save(App()->GlobalConfig());
	PLSDialogView::showEvent(event);
}

void PLSLivingMsgView::hideEvent(QHideEvent *event)
{
	qobject_cast<PLSMainView *>(pls_get_main_view())->getToastButton()->updateIconStyle(false);

	config_set_string(App()->GlobalConfig(), "LivingMsgView", "geometry", saveGeometry().toBase64().constData());
	config_set_bool(App()->GlobalConfig(), "LivingMsgView", "showMode", isVisible());
	config_save(App()->GlobalConfig());
	emit hideSignal();
	PLSDialogView::hideEvent(event);
}

void PLSLivingMsgView::closeEvent(QCloseEvent *event)
{
	hide();
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

#include "PLSProgressView.h"
#include "ui_PLSProgressView.h"
#include "libutils-api.h"
#include "PLSLoginMainView.h"
#include "../PLSLoginDataHandler.h"
#include "liblog.h"
#include "login-view.hpp"
#include "prism-version.h"

constexpr auto TIME_INTERVAL = 100;

PLSProgressView::PLSProgressView(QWidget *parent) : QWidget(parent)
{
	ui = pls_new<Ui::PLSProgressView>();
	ui->setupUi(this);
	m_winType = pls_window_type::PLS_CHECK_UPDATE_VIEW;
	m_timer.setInterval(TIME_INTERVAL);
	connect(&m_timer, &QTimer::timeout, this, &PLSProgressView::updateProgressView);
	ui->progressBar->setTextVisible(false);
	ui->logoLabel->setMovie(&m_logoMovie);
	m_logoMovie.setFileName(":/resource/images/prism-login/login-loop/2_login_loop.png");
	m_logoMovie.setFormat("APNG");
	m_logoMovie.setScaledSize({255, 230});
	m_logoMovie.start();

	ui->versionLabel->setText("Current Version  " PLS_VERSION);
}

PLSProgressView::~PLSProgressView()
{
	disconnect(&m_timer);
	pls_delete(ui);
}

void PLSProgressView::updateProgressView()
{
	ui->progressBar->setValue(m_currentValue);
	auto value = static_cast<int>(100 * (m_currentValue * 1.0 / m_maxValue));
	ui->progressNumLabel->setText(QString("%1%").arg(value));
}

void PLSProgressView::resetProgressValues()
{
	m_minValue = 0;
	m_maxValue = 100;
	m_currentValue = 0;
	ui->progressNumLabel->setText(QString("%1%").arg(0));
	ui->progressBar->setValue(m_minValue);
}

void PLSProgressView::updateView(const QString &tipText, pls_window_type type)
{
	m_winType = type;

	ui->progressContent->setText(tipText);
	resetProgressValues();
	m_timer.start();
}

void PLSProgressView::updateProgressAndText(const QString &text, const int &currentValue, const int &maxValue)
{
	ui->progressContent->setText(text);
	updateProgress(currentValue, maxValue);
}

void PLSProgressView::hideEvent(QHideEvent *event)
{
	m_timer.stop();
	QWidget::hideEvent(event);
}

void PLSProgressView::updateProgress(const int &currentValue, const int &maxValue)
{
	if (currentValue <= 0 || maxValue <= 0) {
		return;
	}
	ui->progressBar->setMaximum(maxValue);
	m_currentValue = currentValue;
	m_maxValue = maxValue;
	updateProgressView();
	if (currentValue == maxValue) {
		m_timer.stop();
		finshHandle(m_winType);
	}
}

void PLSProgressView::finshHandle(pls_window_type winType) const
{
	switch (winType) {
	case pls_window_type::PLS_CHECK_ENV_RES_VIEW:
		break;

	case pls_window_type::PLS_UPDATING_VIEW:
		break;
	case pls_window_type::PLS_RES_DOWNLOADING_VIEW:
		break;
	default:
		break;
	}
}

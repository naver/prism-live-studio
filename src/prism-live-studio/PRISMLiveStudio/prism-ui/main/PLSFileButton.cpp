#include "PLSFileButton.hpp"
#include "ui_PLSFileButton.h"
#include "login-common-helper.hpp"
#include "pls-common-define.hpp"
#include "utils-api.h"
#include <QFileDialog>
#include <QMouseEvent>
#include <QCursor>
using namespace common;
PLSFileButton::PLSFileButton(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSFileButton>();
	ui->setupUi(this);
	setCursor(Qt::PointingHandCursor);
}

PLSFileButton::~PLSFileButton()
{
	pls_delete(ui);
}

void PLSFileButton::setFileButtonEnabled(bool enabled)
{
	m_enabled = enabled;
	if (m_enabled) {
		this->setProperty(STATUS, STATUS_NORMAL);
		setCursor(Qt::PointingHandCursor);
	} else {
		this->setProperty(STATUS, STATUS_DISABLE);
		setCursor(Qt::ArrowCursor);
	}
	LoginCommonHelpers::refreshStyle(ui->fileTitleLabel);
	LoginCommonHelpers::refreshStyle(ui->fileIconLabel);
}

void PLSFileButton::enterEvent(QEnterEvent *event)
{
	if (!m_enabled) {
		QFrame::enterEvent(event);
		return;
	}
	this->setProperty(STATUS, STATUS_HOVER);
	LoginCommonHelpers::refreshStyle(ui->fileTitleLabel);
	LoginCommonHelpers::refreshStyle(ui->fileIconLabel);
	QFrame::enterEvent(event);
}

void PLSFileButton::leaveEvent(QEvent *event)
{
	if (!m_enabled) {
		QFrame::leaveEvent(event);
		return;
	}
	this->setProperty(STATUS, STATUS_NORMAL);
	LoginCommonHelpers::refreshStyle(ui->fileTitleLabel);
	LoginCommonHelpers::refreshStyle(ui->fileIconLabel);
	QFrame::leaveEvent(event);
}

void PLSFileButton::mousePressEvent(QMouseEvent *event)
{
	if (!m_enabled) {
		QFrame::mousePressEvent(event);
		return;
	}
	this->setProperty(STATUS, STATUS_CLICKED);
	LoginCommonHelpers::refreshStyle(ui->fileTitleLabel);
	LoginCommonHelpers::refreshStyle(ui->fileIconLabel);
	QFrame::mousePressEvent(event);
}

void PLSFileButton::mouseReleaseEvent(QMouseEvent *event)
{
	if (!m_enabled) {
		QFrame::mouseReleaseEvent(event);
		return;
	}
	this->setProperty(STATUS, STATUS_NORMAL);
	LoginCommonHelpers::refreshStyle(ui->fileTitleLabel);
	LoginCommonHelpers::refreshStyle(ui->fileIconLabel);
	emit fileSelected();
	QFrame::mouseReleaseEvent(event);
}

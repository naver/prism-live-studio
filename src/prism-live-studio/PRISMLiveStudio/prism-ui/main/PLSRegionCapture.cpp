#include "PLSRegionCapture.h"
#include "libutils-api.h"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "libui.h"

PLSRegionCapture::PLSRegionCapture(QObject *parent) : QObject(parent)
{
	m_process = pls_new<QProcess>(this);
	connect(m_process, &QProcess::finished, this, &PLSRegionCapture::onCaptureFinished);
}

PLSRegionCapture::~PLSRegionCapture()
{
	if (m_process->state() == QProcess::Running) {
		m_process->terminate();
	}
}

QRect PLSRegionCapture::GetSelectedRect() const
{
	return m_rectSelected;
}

void PLSRegionCapture::StartCapture(uint64_t maxWidth, uint64_t maxHeight)
{
	QStringList params;

	m_rectSelected = QRect();

	params << QString("--max-width=%1").arg(maxWidth) << QString("--max-height=%1").arg(maxHeight);
	m_process->start("region-capture.exe", params);
}

void PLSRegionCapture::onCaptureFinished()
{
	QString param = m_process->readAllStandardOutput();

	qInfo() << "region capture process output: " << param;

	auto list = param.split("|");
	if (list.size() == 4) {
		m_rectSelected.setX(list.at(0).toInt());
		m_rectSelected.setY(list.at(1).toInt());
		m_rectSelected.setWidth(list.at(2).toInt());
		m_rectSelected.setHeight(list.at(3).toInt());
	}
	selectedRegion(m_rectSelected);
}

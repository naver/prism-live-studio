#include "TextMotionTemplateButton.h"
#include <QMovie>
#include "utils-api.h"
#include <QVBoxLayout>
#include <qlabel.h>
#include "log/log.h"

TextMotionTemplateButton::TextMotionTemplateButton(QWidget *parent) : PLSTemplateButton(parent, UseForTextMotion{})
{
	m_paidIcon = pls_new<QLabel>(this);
	m_paidIcon->setObjectName("PAID");
	m_paidIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_paidIcon->setScaledContents(true);
}

void TextMotionTemplateButton::setTemplateData(const TextMotionTemplateData &data)
{
	m_templateData = data;
}

void TextMotionTemplateButton::showEvent(QShowEvent *event)
{
	PLSTemplateButton::showEvent(event);
	if (m_paidIcon) {
		m_paidIcon->move(6, 6);
		bool isShowPaidIcon = isPaid() && isMovieValid();
		m_paidIcon->setVisible(isShowPaidIcon);
	}
}
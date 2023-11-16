#include "TextMotionTemplateButton.h"
#include <QMovie>
#include "utils-api.h"

TextMotionTemplateButton::TextMotionTemplateButton(QWidget *parent) : PLSTemplateButton(parent, UseForTextMotion{}) {}

void TextMotionTemplateButton::setTemplateData(const TextMotionTemplateData &data)
{
	m_templateData = data;
}

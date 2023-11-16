#ifndef TEXTMOTIONTEMPLATEBUTTON_H
#define TEXTMOTIONTEMPLATEBUTTON_H

#include <QPushButton>
#include <QMovie>
#include <QResizeEvent>

#include "TextMotionTemplateDataHelper.h"
#include "PLSTemplateButton.h"

class TextMotionTemplateButton : public PLSTemplateButton {
	Q_OBJECT

public:
	explicit TextMotionTemplateButton(QWidget *parent = nullptr);
	~TextMotionTemplateButton() override = default;

	void setTemplateData(const TextMotionTemplateData &data);

private:
	TextMotionTemplateData m_templateData;
};

#endif // TEXTMOTIONTEMPLATEBUTTON_H

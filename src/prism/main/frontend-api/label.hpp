#pragma once

#include <QLabel>

#include "frontend-api-global.h"

class FRONTEND_API PLSElideLabel : public QLabel {
	Q_OBJECT

public:
	explicit PLSElideLabel(QWidget *parent = nullptr);
	explicit PLSElideLabel(const QString &text, QWidget *parent = nullptr);
	~PLSElideLabel();

public:
	void setText(const QString &text);
	QString text() const;

public:
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

private:
	void updateText();

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	QString originText;
};

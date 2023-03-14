#pragma once

#include <QCheckBox>

#include "frontend-api-global.h"

class FRONTEND_API PLSElideCheckBox : public QCheckBox {
	Q_OBJECT

public:
	explicit PLSElideCheckBox(QWidget *parent = nullptr);
	explicit PLSElideCheckBox(const QString &text, QWidget *parent = nullptr);
	~PLSElideCheckBox();

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

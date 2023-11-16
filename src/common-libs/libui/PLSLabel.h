#ifndef _PRISM_COMMON_LIBHDPI_LABEL_H
#define _PRISM_COMMON_LIBHDPI_LABEL_H

#include "libui-globals.h"

#include <QLabel>

class LIBUI_API PLSElideLabel : public QLabel {
	Q_OBJECT

public:
	explicit PLSElideLabel(QWidget *parent = nullptr);
	explicit PLSElideLabel(const QString &text, QWidget *parent = nullptr);
	~PLSElideLabel() override = default;

	void setText(const QString &text);
	QString text() const;

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

private:
	void updateText();

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	QString originText;
};

class LIBUI_API PLSLabel : public QLabel {
	Q_OBJECT
public:
	explicit PLSLabel(QWidget *parent = nullptr, bool cutText = true);
	explicit PLSLabel(const QString &text, QWidget *parent = nullptr, bool cutText = true);

	void SetText(const QString &text);
	QString Text() const;

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	QString GetNameElideString() const;

	QString realText{};
	bool cutTextIfNeed = true;
};

class LIBUI_API PLSCombinedLabel : public QLabel {
	Q_OBJECT

public:
	explicit PLSCombinedLabel(QWidget *parent = nullptr);

	void SetShowText(const QString &strText);
	void SetAdditionalText(const QString &strText);
	void SetSpacing(int spacing);
	void SetAdditionalVisible(bool visible);
	void SetMiddleVisible(bool visible);

public slots:
	void UpdataUi();

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	int GetTextSpacing();

	QLabel *additionalLabel = nullptr;
	QLabel *middleLabel = nullptr;
	int spacing = 5;
	QString strText;
	QString strAdditional;
};

class LIBUI_API PLSApngLabel : public QLabel {
	Q_OBJECT

public:
	explicit PLSApngLabel(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QLabel(parent, f) {}
	~PLSApngLabel() override = default;

protected:
	void paintEvent(QPaintEvent *) override;
};

#endif // _PRISM_COMMON_LIBHDPI_LABEL_H

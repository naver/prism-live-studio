#ifndef PLSTEXTLOADINGVIEW_H
#define PLSTEXTLOADINGVIEW_H

#include "libui-globals.h"

#include <QShowEvent>
#include <QString>
#include <QWidget>

class QLabel;
class PLSLoadingView;

class LIBUI_API PLSTextLoadingView : public QWidget {
public:
	explicit PLSTextLoadingView(const QString &text, QWidget *parent = nullptr, const QString &pathImage = QString());

	void setLoadingText(const QString &text);
	QString loadingText() const;

protected:
	void showEvent(QShowEvent *event) override;

private:
	QWidget *m_panel = nullptr;
	PLSLoadingView *m_loadingView = nullptr;
	QLabel *m_textLabel = nullptr;

	static constexpr int SPINNER_SIZE = 24;
	static constexpr int SPACING = 5;
};

#endif

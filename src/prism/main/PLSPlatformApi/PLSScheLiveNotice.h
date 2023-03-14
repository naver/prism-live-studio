#ifndef PLSSCHELIVENOTICE_H
#define PLSSCHELIVENOTICE_H

#include "dialog-view.hpp"

namespace Ui {
class PLSScheLiveNotice;
}

class PLSPlatformBase;

class PLSScheLiveNotice : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSScheLiveNotice(PLSPlatformBase *platform, const QString &title, const QString &startTime, QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSScheLiveNotice();

public:
	void setMessageText(const QString &messageText);

protected:
	virtual bool eventFilter(QObject *watched, QEvent *event);

private slots:
	void on_okButton_clicked();

private:
	Ui::PLSScheLiveNotice *ui;
	QString messageText;
	QString title;
};

#endif // PLSSCHELIVENOTICE_H

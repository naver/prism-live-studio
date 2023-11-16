#ifndef PLSSCHELIVENOTICE_H
#define PLSSCHELIVENOTICE_H

#include "PLSDialogView.h"

namespace Ui {
class PLSScheLiveNotice;
}

class PLSPlatformBase;

class PLSScheLiveNotice : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSScheLiveNotice(const PLSPlatformBase *platform, const QString &title, const QString &startTime, QWidget *parent = nullptr);
	~PLSScheLiveNotice() override;

	void setMessageText(const QString &messageText);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void on_okButton_clicked();

private:
	Ui::PLSScheLiveNotice *ui = nullptr;
	QString messageText;
	QString title;
};

#endif // PLSSCHELIVENOTICE_H

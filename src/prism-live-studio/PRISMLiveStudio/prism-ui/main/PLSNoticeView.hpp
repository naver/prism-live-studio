#ifndef PLSNOTICEVIEW_H
#define PLSNOTICEVIEW_H

#include "PLSDialogView.h"
#include "libbrowser.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSNoticeView;
}
QT_END_NAMESPACE

class PLSNoticeView : public PLSDialogView {
	Q_OBJECT

public:
	PLSNoticeView(const QString &content, const QString &title, const QString &detailURL, QWidget *parent = nullptr);
	~PLSNoticeView() override;

private slots:
	void on_confirmButton_clicked();
	void on_learnMoreButton_clicked();

private:
	void initUI();

	Ui::PLSNoticeView *ui;
	QString m_content;
	QString m_detailURL;
	QString m_title;
	pls::browser::BrowserWidget *m_browserWidget;
};

#endif // PLSNOTICEVIEW_H

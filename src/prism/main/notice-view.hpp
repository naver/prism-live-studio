#ifndef PLSNOTICEVIEW_H
#define PLSNOTICEVIEW_H

#include <dialog-view.hpp>
#include <browser-panel.hpp>

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSNoticeView;
}
QT_END_NAMESPACE

class PLSNoticeView : public PLSDialogView {
	Q_OBJECT

public:
	PLSNoticeView(const QString &content, const QString &title, const QString &detailURL, QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSNoticeView();

private slots:
	void on_confirmButton_clicked();
	void on_learnMoreButton_clicked();
	void onTimeOut();

protected:
	bool isInCustomControl(QWidget *child) const;

private:
	void initUI(PLSDpiHelper dpiHelper);
	void initConnect();

private:
	Ui::PLSNoticeView *ui;
	QString m_content;
	QString m_detailURL;
	QString m_title;
	QCefWidget *m_cefWidget;
	QPoint m_mouseStartPoint;
	QPoint m_windowTopLeftPoint;
	bool m_pressed;
	QTimer *timer;
};

#endif // PLSNOTICEVIEW_H

#ifndef PLSUPDATEVIEW_HPP
#define PLSUPDATEVIEW_HPP

#include <dialog-view.hpp>
#include <browser-panel.hpp>
#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSUpdateView;
}
QT_END_NAMESPACE

class PLSUpdateView : public PLSDialogView {
	Q_OBJECT

public:
	PLSUpdateView(bool manualUpdate, bool isForceUpdate, const QString &version, const QString &fileUrl, const QString &updateInfoUrl, QWidget *parent = nullptr);
	~PLSUpdateView();

private:
	void initUI();
	void initConnect();

private slots:
	void on_nextUpdateBtn_clicked();
	void on_nowUpdateBtn_clicked();
	void isShowMainView(bool isShow);
	void onTimeOut();

protected:
	void showEvent(QShowEvent *event);
	bool isInCustomControl(QWidget *child) const;

private:
	Ui::PLSUpdateView *ui;
	bool m_manualUpdate;
	bool m_isForceUpdate;
	QString m_version;
	QString m_fileUrl;
	QString m_updateInfoUrl;
	QCefWidget *m_cefWidget;
	QPoint m_mouseStartPoint;
	QPoint m_windowTopLeftPoint;
	bool m_pressed;
	QTimer *timer;
};
#endif // PLSUPDATEVIEW_HPP

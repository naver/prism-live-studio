#ifndef PLSUPDATEVIEW_HPP
#define PLSUPDATEVIEW_HPP

#include "PLSDialogView.h"
#include <QObject>
#include <QString>
#include "libbrowser.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class PLSUpdateView;
}
QT_END_NAMESPACE

class PLSUpdateView : public PLSDialogView {
	Q_OBJECT

public:
	PLSUpdateView(bool manualUpdate, bool isForceUpdate, const QString &version, const QString &fileUrl, const QString &updateInfoUrl, QWidget *parent = nullptr);
	~PLSUpdateView() override;

private:
	void initUI();
	void initConnect() const;

private slots:
	void on_nextUpdateBtn_clicked();
	void on_nowUpdateBtn_clicked();
	void isShowMainView(bool isShow);

protected:
	void showEvent(QShowEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	Ui::PLSUpdateView *ui;
	bool m_manualUpdate;
	bool m_isForceUpdate;
	QString m_version;
	QString m_fileUrl;
	QString m_updateInfoUrl;
	pls::browser::BrowserWidget *m_browserWidget;
	QPoint m_mouseStartPoint;
	QPoint m_windowTopLeftPoint;
	bool m_pressed;
};
#endif // PLSUPDATEVIEW_HPP
